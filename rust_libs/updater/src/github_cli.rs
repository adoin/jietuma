//! Use an authenticated GitHub CLI without extracting or logging its credentials.
use crate::{fail, Error, Result};
use std::{
    io::Read,
    path::{Path, PathBuf},
    process::{Child, Command, ExitStatus, Stdio},
    sync::atomic::{AtomicBool, Ordering},
    time::{Duration, Instant},
};

const JSON_LIMIT: usize = 1024 * 1024;
struct Output {
    status: ExitStatus,
    stdout: Vec<u8>,
    stderr: Vec<u8>,
}

fn find_gh() -> Option<PathBuf> {
    let filename = if cfg!(windows) { "gh.exe" } else { "gh" };
    let mut candidates: Vec<_> = std::env::var_os("PATH")
        .into_iter()
        .flat_map(|paths| std::env::split_paths(&paths).collect::<Vec<_>>())
        .filter(|path| path.is_absolute())
        .map(|path| path.join(filename))
        .collect();
    #[cfg(windows)]
    if let Some(path) = std::env::var_os("ProgramFiles") {
        candidates.push(PathBuf::from(path).join("GitHub CLI/gh.exe"));
    }
    candidates.into_iter().find(|path| path.is_file())
}
fn cli_error() -> Error {
    Error {
        code: "github_cli",
        message: "GitHub CLI request failed; check its authentication and network connection"
            .into(),
    }
}

#[cfg(windows)]
struct Job(windows_sys::Win32::Foundation::HANDLE);
#[cfg(windows)]
impl Job {
    fn new(child: &Child) -> Result<Self> {
        use std::os::windows::io::AsRawHandle;
        use windows_sys::Win32::System::JobObjects::*;
        let handle = unsafe { CreateJobObjectW(std::ptr::null(), std::ptr::null()) };
        if handle.is_null() {
            return Err(cli_error());
        }
        let job = Self(handle);
        let mut limits: JOBOBJECT_EXTENDED_LIMIT_INFORMATION = unsafe { std::mem::zeroed() };
        limits.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
        if unsafe {
            SetInformationJobObject(
                handle,
                JobObjectExtendedLimitInformation,
                &limits as *const _ as *const _,
                std::mem::size_of_val(&limits) as u32,
            )
        } == 0
            || unsafe { AssignProcessToJobObject(handle, child.as_raw_handle()) } == 0
        {
            return Err(cli_error());
        }
        Ok(job)
    }
}
#[cfg(windows)]
impl Drop for Job {
    fn drop(&mut self) {
        unsafe {
            windows_sys::Win32::Foundation::CloseHandle(self.0);
        }
    }
}
struct Process {
    child: Child,
    #[cfg(windows)]
    job: Option<Job>,
}
impl Drop for Process {
    fn drop(&mut self) {
        let _ = self.child.kill();
        let _ = self.child.wait();
    }
}

async fn run(
    mut command: Command,
    flag: &AtomicBool,
    limit: usize,
    timeout: Duration,
) -> Result<Output> {
    if flag.load(Ordering::Acquire) {
        return fail("cancelled", "检查已取消");
    }
    command
        .stdin(Stdio::null())
        .stdout(Stdio::piped())
        .stderr(Stdio::piped());
    #[cfg(windows)]
    {
        use std::os::windows::process::CommandExt;
        command.creation_flags(windows_sys::Win32::System::Threading::CREATE_NO_WINDOW);
    }
    let mut process = Process {
        child: command.spawn().map_err(|_| cli_error())?,
        #[cfg(windows)]
        job: None,
    };
    #[cfg(windows)]
    {
        process.job = Some(Job::new(&process.child)?);
    }
    let out = process.child.stdout.take().ok_or_else(cli_error)?;
    let err = process.child.stderr.take().ok_or_else(cli_error)?;
    let stdout = std::thread::spawn(move || {
        let mut bytes = Vec::new();
        out.take((limit + 1) as u64)
            .read_to_end(&mut bytes)
            .map(|_| bytes)
    });
    let stderr = std::thread::spawn(move || {
        let mut bytes = Vec::new();
        err.take(16385).read_to_end(&mut bytes).map(|_| bytes)
    });
    let start = Instant::now();
    let status = loop {
        if flag.load(Ordering::Acquire) {
            break fail("cancelled", "检查已取消");
        }
        if start.elapsed() >= timeout {
            break fail("github_cli_timeout", "GitHub CLI request timed out");
        }
        match process.child.try_wait() {
            Ok(Some(status)) => break Ok(status),
            Ok(None) => tokio::time::sleep(Duration::from_millis(20)).await,
            Err(_) => break Err(cli_error()),
        }
    };
    // Closing the job also kills descendants if the parent updater is terminated by the UI.
    drop(process);
    let out = stdout
        .join()
        .map_err(|_| cli_error())?
        .map_err(|_| cli_error())?;
    let err = stderr
        .join()
        .map_err(|_| cli_error())?
        .map_err(|_| cli_error())?;
    let status = status?;
    if out.len() > limit || err.len() > 16384 {
        return fail("github_cli", "GitHub CLI response exceeded its size limit");
    }
    Ok(Output {
        status,
        stdout: out,
        stderr: err,
    })
}
fn command(executable: &Path, arguments: &[&str]) -> Command {
    let mut command = Command::new(executable);
    command
        .args(arguments)
        .env("GH_PROMPT_DISABLED", "1")
        .env("GH_NO_UPDATE_NOTIFIER", "1")
        .env("GH_NO_EXTENSION_UPDATE_NOTIFIER", "1")
        .env("NO_COLOR", "1")
        .env_remove("GH_FORCE_TTY");
    command
}
pub(crate) async fn latest(flag: &AtomicBool) -> Result<Option<Vec<u8>>> {
    latest_from(find_gh(), flag).await
}
async fn latest_from(executable: Option<PathBuf>, flag: &AtomicBool) -> Result<Option<Vec<u8>>> {
    let Some(executable) = executable else {
        return Ok(None);
    };
    let auth = run(
        command(
            &executable,
            &["auth", "status", "--active", "--hostname", "github.com"],
        ),
        flag,
        65536,
        Duration::from_secs(5),
    )
    .await?;
    if !auth.status.success() {
        if auth_missing(&auth.stdout, &auth.stderr) {
            return Ok(None);
        }
        return Err(classify_failure(&auth.stderr));
    }
    let endpoint = concat!("repos/", env!("JTM_REPOSITORY"), "/releases/latest");
    let output = run(
        command(
            &executable,
            &[
                "api",
                "--hostname",
                "github.com",
                endpoint,
                "-H",
                "Accept: application/vnd.github+json",
            ],
        ),
        flag,
        JSON_LIMIT,
        Duration::from_secs(20),
    )
    .await?;
    if !output.status.success() {
        // Never turn an authenticated request failure into an anonymous retry.
        return Err(classify_failure(&output.stderr));
    }
    Ok(Some(output.stdout))
}

fn auth_missing(stdout: &[u8], stderr: &[u8]) -> bool {
    let message = format!(
        "{} {}",
        String::from_utf8_lossy(stdout),
        String::from_utf8_lossy(stderr)
    )
    .to_ascii_lowercase();
    message.contains("not logged in")
        || message.contains("not logged into")
        || message.contains("http 401")
        || (message.contains("token") && message.contains("invalid"))
}
fn classify_failure(stderr: &[u8]) -> Error {
    let message = String::from_utf8_lossy(stderr).to_ascii_lowercase();
    if message.contains("rate limit") || message.contains("http 429") {
        Error {
            code: "github_rate_limit",
            message: "Authenticated GitHub API rate limit exceeded".into(),
        }
    } else if message.contains("http 401") || message.contains("bad credentials") {
        Error {
            code: "github_auth",
            message: "GitHub CLI authentication failed (HTTP 401)".into(),
        }
    } else if message.contains("http 403") || message.contains("http 404") {
        Error {
            code: "github_access",
            message: "GitHub repository or release is inaccessible (HTTP 403/404)".into(),
        }
    } else if [
        "timeout",
        "timed out",
        "dial tcp",
        "tls",
        "proxy",
        "connection",
        "no such host",
    ]
    .iter()
    .any(|value| message.contains(value))
    {
        Error {
            code: "github_network",
            message: "GitHub CLI network connection failed".into(),
        }
    } else {
        cli_error()
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    fn runtime() -> tokio::runtime::Runtime {
        tokio::runtime::Builder::new_current_thread()
            .enable_all()
            .build()
            .unwrap()
    }
    fn fixture(mode: &str) -> Command {
        let mut command = Command::new(std::env::current_exe().unwrap());
        command
            .args([
                "--ignored",
                "--exact",
                "github_cli::tests::process_fixture",
                "--nocapture",
            ])
            .env("JIETUMA_CLI_FIXTURE", mode);
        command
    }
    #[test]
    #[ignore = "child process fixture"]
    fn process_fixture() {
        match std::env::var("JIETUMA_CLI_FIXTURE").as_deref() {
            Ok("sleep") => std::thread::sleep(Duration::from_secs(10)),
            Ok("large") => {
                use std::io::Write;
                let _ = std::io::stdout().write_all(&vec![b'x'; 65536]);
            }
            _ => println!("fixture response"),
        }
    }
    #[test]
    fn missing_or_unauthenticated_cli_allows_public_fallback() {
        let flag = AtomicBool::new(false);
        runtime().block_on(async {
            assert!(latest_from(None, &flag).await.unwrap().is_none());
            // An unexpected authentication-command failure must not trigger anonymous fallback.
            assert_eq!(
                latest_from(Some(std::env::current_exe().unwrap()), &flag)
                    .await
                    .err()
                    .unwrap()
                    .code,
                "github_cli"
            );
        });
    }
    #[test]
    fn only_missing_login_allows_anonymous_fallback() {
        assert!(auth_missing(
            b"",
            b"You are not logged into any GitHub hosts."
        ));
        assert!(auth_missing(b"", b"The token in default is invalid."));
        assert!(!auth_missing(b"", b"HTTP 403: API rate limit exceeded"));
        assert!(!auth_missing(b"", b"dial tcp: connection refused"));
        assert_eq!(
            classify_failure(b"HTTP 401: Bad credentials").code,
            "github_auth"
        );
        assert_eq!(
            classify_failure(b"dial tcp: connection refused").code,
            "github_network"
        );
    }
    #[test]
    fn cli_failures_are_classified_without_exposing_output() {
        assert_eq!(
            classify_failure(b"HTTP 403: API rate limit exceeded").code,
            "github_rate_limit"
        );
        assert_eq!(classify_failure(b"HTTP 429").code, "github_rate_limit");
        let failure = classify_failure(b"HTTP 403: Forbidden; token_private_value");
        assert_eq!(failure.code, "github_access");
        assert!(!failure.message.contains("token_private_value"));
    }
    #[test]
    #[ignore = "requires an authenticated local GitHub CLI"]
    fn live_authenticated_release_check() {
        let data = runtime()
            .block_on(latest(&AtomicBool::new(false)))
            .unwrap()
            .expect("authenticated gh not available");
        let value: serde_json::Value = serde_json::from_slice(&data).unwrap();
        let release = crate::download::parse_release(
            &value,
            crate::contract::Arch::X64,
            crate::contract::AppVariant::Full,
        )
        .unwrap();
        println!(
            "Authenticated gh selected {} / {}",
            release.tag_name, release.asset_name
        );
    }
    #[test]
    fn process_output_is_bounded_and_timeouts_kill_children() {
        let flag = AtomicBool::new(false);
        runtime().block_on(async {
            let result = run(fixture("ok"), &flag, 65536, Duration::from_secs(5))
                .await
                .unwrap();
            assert!(result.status.success());
            assert!(String::from_utf8_lossy(&result.stdout).contains("fixture response"));
            assert_eq!(
                run(fixture("large"), &flag, 1024, Duration::from_secs(5))
                    .await
                    .err()
                    .unwrap()
                    .code,
                "github_cli"
            );
            assert_eq!(
                run(fixture("sleep"), &flag, 65536, Duration::from_millis(100))
                    .await
                    .err()
                    .unwrap()
                    .code,
                "github_cli_timeout"
            );
            flag.store(true, Ordering::Release);
            assert_eq!(
                run(fixture("sleep"), &flag, 65536, Duration::from_secs(5))
                    .await
                    .err()
                    .unwrap()
                    .code,
                "cancelled"
            );
        });
    }
}
