#ifndef AppName
  #error AppName must be supplied from branding.json
#endif
#ifndef AppPrefix
  #error AppPrefix must be supplied from branding.json
#endif
#ifndef AppVersion
  #error AppVersion must be supplied from VERSION
#endif
#ifndef AppRepository
  #error AppRepository must be supplied from branding.json
#endif
#ifndef AppPublisher
  #error AppPublisher must be supplied from branding.json
#endif
#ifndef AppNameZh
  #define AppNameZh AppName
#endif
#ifndef SourceDir
  #error SourceDir must contain the complete release payload
#endif
#ifndef OutputDir
  #error OutputDir must be supplied
#endif
#ifndef TargetArch
  #error TargetArch must be x64 or arm64
#endif
#ifndef ApplicationId
  #define ApplicationId "{{BAE3DE00-B354-46A9-BD28-99AC2A89FAE0}"
#endif
#ifndef AppExeName
  #define AppExeName AppPrefix + "_pp.exe"
#endif
#ifndef OutputName
  #define OutputName AppPrefix + "_setup-release-" + AppVersion + "-" + TargetArch
#endif
#ifndef StartMenuDir
  #define StartMenuDir "{autoprograms}\" + AppName
#endif
#ifndef DesktopDir
  #define DesktopDir "{autodesktop}"
#endif
#ifndef StartupValue
  #define StartupValue AppName
#endif
#ifndef AdminTaskName
  #define AdminTaskName AppName + " RunAsAdmin ({username})"
#endif

[Setup]
ChangesAssociations=yes
AppId={#ApplicationId}
AppName={cm:ProductName}
AppVersion={#AppVersion}
AppPublisher={#AppPublisher}
AppPublisherURL=https://github.com/{#AppRepository}
AppSupportURL=https://github.com/{#AppRepository}/issues
AppUpdatesURL=https://github.com/{#AppRepository}/releases
DefaultDirName={localappdata}\Programs\{#AppName}
UsePreviousAppDir=yes
DisableDirPage=no
DisableProgramGroupPage=yes
PrivilegesRequired=lowest
#if TargetArch == "arm64"
ArchitecturesAllowed=arm64
ArchitecturesInstallIn64BitMode=arm64
#elif TargetArch == "x64"
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
#else
  #error Unsupported architecture
#endif
MinVersion=10.0.17763
OutputDir={#OutputDir}
OutputBaseFilename={#OutputName}
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
DisableWelcomePage=no
SetupIconFile=..\app.ico
UninstallDisplayIcon={app}\{#AppExeName}
VersionInfoProductName={#AppName}
VersionInfoDescription={#AppName} Setup
VersionInfoVersion={#AppVersion}
VersionInfoOriginalFileName={#OutputName}.exe
VersionInfoCompany={#AppPublisher}
LicenseFile=..\LICENSE
; Only close the exact installed executable, after interactive consent.
CloseApplications=no
RestartApplications=no
SetupLogging=yes

[Languages]
Name: "chinesesimp"; MessagesFile: "languages\ChineseSimplified.isl"
Name: "english"; MessagesFile: "compiler:Default.isl"

[CustomMessages]
english.ProductName={#AppName}
chinesesimp.ProductName={#AppNameZh}
english.PluginPackage={#AppName} Plugin Package
chinesesimp.PluginPackage={#AppNameZh}插件包
english.AppRunning=%1 is still running. Close it before continuing.
chinesesimp.AppRunning=%1 仍在运行，请关闭后继续。
english.CloseApp=Detected %1 running from:%n%2%n%nClose it and continue? Save your work before proceeding.
chinesesimp.CloseApp=检测到 %1 正在运行：%n%2%n%n是否退出程序并继续？请先保存正在处理的内容。
english.ForceCloseApp=%1 has not exited or does not support an exit request.%n%nEnd its process and continue? Unsaved captures, recordings or exports may be lost. Choose No to return and close it yourself.
chinesesimp.ForceCloseApp=%1 尚未退出，或当前版本不支持退出请求。%n%n是否结束进程并继续？未保存的截图、录制或导出可能丢失。选择“否”返回后可自行关闭程序。
english.CloseAppFailed=Unable to close %1. It may be running with higher privileges. Close it manually and retry.
chinesesimp.CloseAppFailed=无法关闭 %1，程序可能以更高权限运行。请手动退出后重试。
english.ProcessCheckFailed=Unable to check whether %1 is running. Close it and retry, or check Windows Management Instrumentation.
chinesesimp.ProcessCheckFailed=无法确认 %1 是否正在运行。请退出程序并重试，或检查 Windows 管理规范服务。
english.StartupTaskRemovalFailed=The elevated startup task could not be removed. Remove it in Windows Task Scheduler if it still points to this installation.
chinesesimp.StartupTaskRemovalFailed=无法删除提权开机启动任务。如果它仍指向此安装目录，请在 Windows 任务计划程序中删除。

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"; Flags: unchecked

[Files]
Source: "{#SourceDir}\{#AppExeName}"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#SourceDir}\LICENSE"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#SourceDir}\THIRD-PARTY-NOTICES.md"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#SourceDir}\licenses\*"; DestDir: "{app}\licenses"; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "languages\ChineseSimplified-LICENSE.txt"; DestDir: "{app}\licenses"; Flags: ignoreversion
; Models can be replaced by users or shared with a portable installation.
Source: "{#SourceDir}\models\PP-OCRv6_det_small.onnx"; DestDir: "{app}\models"; Flags: ignoreversion onlyifdoesntexist uninsneveruninstall
Source: "{#SourceDir}\models\PP-OCRv6_rec_small.onnx"; DestDir: "{app}\models"; Flags: ignoreversion onlyifdoesntexist uninsneveruninstall

[Icons]
Name: "{#StartMenuDir}\{cm:ProductName}"; Filename: "{app}\{#AppExeName}"; WorkingDir: "{app}"
Name: "{#StartMenuDir}\{cm:UninstallProgram,{cm:ProductName}}"; Filename: "{uninstallexe}"
Name: "{#DesktopDir}\{cm:ProductName}"; Filename: "{app}\{#AppExeName}"; WorkingDir: "{app}"; Tasks: desktopicon

[Run]
Filename: "{app}\{#AppExeName}"; Description: "{cm:LaunchProgram,{cm:ProductName}}"; WorkingDir: "{app}"; Flags: nowait postinstall skipifsilent

[Registry]
Root: HKA; Subkey: "Software\Classes\.jtbplugin"; ValueType: string; ValueData: "{#AppPrefix}.Plugin"
Root: HKA; Subkey: "Software\Classes\.jtbplugin\OpenWithProgids"; ValueType: string; ValueName: "{#AppPrefix}.Plugin"; ValueData: ""; Flags: uninsdeletevalue
Root: HKA; Subkey: "Software\Classes\{#AppPrefix}.Plugin"; ValueType: string; ValueData: "{cm:PluginPackage}"; Flags: uninsdeletekey
Root: HKA; Subkey: "Software\Classes\{#AppPrefix}.Plugin\DefaultIcon"; ValueType: string; ValueData: """%1"""
Root: HKA; Subkey: "Software\Classes\{#AppPrefix}.Plugin\shell\open\command"; ValueType: string; ValueData: """{app}\{#AppExeName}"" --install-plugins ""%1"""

[Code]
const
  ProcessQueryLimitedInformation = $1000;
  ProcessTerminate = $0001;
  SynchronizeAccess = $00100000;
  EventModifyState = $0002;
  WaitTimeout = $00000102;

function OpenProcess(Access: LongWord; InheritHandle: Boolean; ProcessId: LongWord): THandle;
  external 'OpenProcess@kernel32.dll stdcall';
function QueryFullProcessImageName(Process: THandle; Flags: LongWord; ImageName: String; var Size: LongWord): Boolean;
  external 'QueryFullProcessImageNameW@kernel32.dll stdcall';
function WaitForSingleObject(Handle: THandle; Milliseconds: LongWord): LongWord;
  external 'WaitForSingleObject@kernel32.dll stdcall';
function TerminateProcess(Process: THandle; ExitCode: LongWord): Boolean;
  external 'TerminateProcess@kernel32.dll stdcall';
function OpenEvent(Access: LongWord; InheritHandle: Boolean; Name: String): THandle;
  external 'OpenEventW@kernel32.dll stdcall';
function SetEvent(Handle: THandle): Boolean;
  external 'SetEvent@kernel32.dll stdcall';
function CloseHandle(Handle: THandle): Boolean;
  external 'CloseHandle@kernel32.dll stdcall';

function InstalledProcesses: Variant;
var
  Locator, Service: Variant;
  Path: String;
begin
  Path := ExpandConstant('{app}\{#AppExeName}');
  Log('Checking installed application: ' + Path);
  StringChangeEx(Path, '\', '\\', True);
  StringChangeEx(Path, '"', '\"', True);
  Locator := CreateOleObject('WbemScripting.SWbemLocator');
  Service := Locator.ConnectServer('', 'root\CIMV2', '', '');
  Result := Service.ExecQuery('SELECT ProcessId FROM Win32_Process WHERE ExecutablePath = "' + Path + '"');
end;

function InstalledExecutableIsRunning: Boolean;
var
  Processes: Variant;
begin
  Processes := InstalledProcesses;
  Result := Processes.Count > 0;
end;

procedure CloseInstalledProcesses(Force: Boolean);
var
  Processes, Process: Variant;
  Index: Integer;
  Pid, Access, Size: LongWord;
  Handle, ExitEvent: THandle;
  Image: String;
begin
  Processes := InstalledProcesses;
  for Index := 0 to Processes.Count - 1 do begin
    Process := Processes.ItemIndex(Index);
    Pid := Process.ProcessId;
    Access := ProcessQueryLimitedInformation or SynchronizeAccess;
    if Force then Access := Access or ProcessTerminate;
    Handle := OpenProcess(Access, False, Pid);
    if Handle <> 0 then begin
      try
        Size := 32768;
        SetLength(Image, Size);
        if QueryFullProcessImageName(Handle, 0, Image, Size) then begin
          SetLength(Image, Size);
          { Check the pinned process handle, not just a name or a possibly reused PID. }
          if (CompareText(Image, ExpandConstant('{app}\{#AppExeName}')) = 0) and
             (WaitForSingleObject(Handle, 0) = WaitTimeout) then begin
            if Force then begin
              Log(Format('Ending installed application PID %d: %s', [Pid, Image]));
              if TerminateProcess(Handle, 0) then WaitForSingleObject(Handle, 5000);
            end else begin
              ExitEvent := OpenEvent(EventModifyState, False, 'Local\{#AppPrefix}.InstallerExit.' + IntToStr(Pid));
              if ExitEvent <> 0 then begin
                try
                  Log(Format('Requesting application exit, PID %d: %s', [Pid, Image]));
                  if SetEvent(ExitEvent) then WaitForSingleObject(Handle, 10000);
                finally
                  CloseHandle(ExitEvent);
                end;
              end;
            end;
          end;
        end;
      finally
        CloseHandle(Handle);
      end;
    end;
  end;
end;

function EnsureInstalledApplicationClosed(Interactive: Boolean): String;
begin
  Result := '';
  try
    if not InstalledExecutableIsRunning then Exit;
    Result := FmtMessage(CustomMessage('AppRunning'), [CustomMessage('ProductName')]);
    { Silent installs never implicitly terminate an application. }
    if not Interactive then Exit;
    if MsgBox(FmtMessage(CustomMessage('CloseApp'), [CustomMessage('ProductName'),
        ExpandConstant('{app}\{#AppExeName}')]), mbConfirmation, MB_YESNO or MB_DEFBUTTON2) <> IDYES then Exit;
    CloseInstalledProcesses(False);
    if InstalledExecutableIsRunning then begin
      if MsgBox(FmtMessage(CustomMessage('ForceCloseApp'), [CustomMessage('ProductName')]),
          mbConfirmation, MB_YESNO or MB_DEFBUTTON2) <> IDYES then Exit;
      CloseInstalledProcesses(True);
    end;
    if InstalledExecutableIsRunning then
      Result := FmtMessage(CustomMessage('CloseAppFailed'), [CustomMessage('ProductName')])
    else Result := '';
  except
    Log('Could not close the installed executable: ' + GetExceptionMessage);
    Result := FmtMessage(CustomMessage('ProcessCheckFailed'), [CustomMessage('ProductName')]);
  end;
end;

function NextButtonClick(CurPageID: Integer): Boolean;
var
  Error: String;
begin
  Result := True;
  if CurPageID = wpReady then begin
    Error := EnsureInstalledApplicationClosed(not WizardSilent);
    Result := Error = '';
    if not Result then begin
      Log(Error);
      if (not WizardSilent) and
         (Error <> FmtMessage(CustomMessage('AppRunning'), [CustomMessage('ProductName')])) then
        MsgBox(Error, mbError, MB_OK);
    end;
  end;
end;

function PrepareToInstall(var NeedsRestart: Boolean): String;
begin
  { Recheck in case an instance started after the Ready page. }
  Result := EnsureInstalledApplicationClosed(not WizardSilent);
end;

function InitializeUninstall: Boolean;
var
  Error: String;
begin
  Error := EnsureInstalledApplicationClosed(not UninstallSilent);
  Result := Error = '';
  if not Result then begin
    Log(Error);
    if not UninstallSilent then MsgBox(Error, mbError, MB_OK);
  end;
end;

procedure RemoveOwnedStartupEntries;
var
  Value, Target: String;
  Scheduler, Folder, Task, Action: Variant;
  TaskExists: Boolean;
begin
  Target := ExpandConstant('{app}\{#AppExeName}');
  if RegQueryStringValue(HKCU, 'Software\Microsoft\Windows\CurrentVersion\Run', '{#StartupValue}', Value) then
    if CompareText(RemoveQuotes(Trim(Value)), Target) = 0 then
      RegDeleteValue(HKCU, 'Software\Microsoft\Windows\CurrentVersion\Run', '{#StartupValue}');
  TaskExists := False;
  try
    Scheduler := CreateOleObject('Schedule.Service');
    Scheduler.Connect;
    Folder := Scheduler.GetFolder('\');
    Task := Folder.GetTask(ExpandConstant('{#AdminTaskName}'));
    TaskExists := True;
    Action := Task.Definition.Actions.Item(1);
    if CompareText(RemoveQuotes(Trim(String(Action.Path))), Target) = 0 then begin
      Folder.DeleteTask(ExpandConstant('{#AdminTaskName}'), 0);
      TaskExists := False;
    end else
      TaskExists := False;
  except
    if TaskExists then begin
      Log('Could not remove the matching elevated startup task: ' + GetExceptionMessage);
      if not UninstallSilent then MsgBox(CustomMessage('StartupTaskRemovalFailed'), mbInformation, MB_OK);
    end;
  end;
end;

procedure CurUninstallStepChanged(CurUninstallStep: TUninstallStep);
var
  Root: Integer;
  Owner: String;
begin
  if CurUninstallStep = usUninstall then begin
    RemoveOwnedStartupEntries;
    if IsAdminInstallMode then Root := HKLM else Root := HKCU;
    if RegQueryStringValue(Root, 'Software\Classes\.jtbplugin', '', Owner) then
      if CompareText(Owner, '{#AppPrefix}.Plugin') = 0 then
        RegDeleteValue(Root, 'Software\Classes\.jtbplugin', '');
  end;
end;
