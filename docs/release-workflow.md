# 发布与上游同步

“关于 → 自动更新”默认关闭。开启后，启动约 15 秒进行首次检查，此后每 6 小时检查本仓库的最新正式 Release；失败也等待下一轮，不弹出错误窗口。检测到新版后，在所有应用窗口和钉图关闭、截图/录制/导出等任务结束时自动下载、安装并重启。下载期间重新开始操作会延后安装。关闭开关或取消更新会停止尚未交接的自动更新，手动检查不受影响。

更新沿用已内置的更新器，按当前架构和完整版/轻量版选择 Release ZIP，原地替换主程序并保留配置、插件、剪贴板历史和模型；无需重新运行安装向导。Release 中仍同时提供安装包。旧版本只有手动更新入口，需要先手动安装含此开关的版本才能使用自动更新。

`origin` 是自己的仓库 `adoin/jietuma`，`upstream` 是官方仓库 `1003129155/jietuba`。

- `main` 是默认分支，保留截图妈的品牌、插件生态和业务功能；日常开发与 PR 以它为目标。
- `upstream-sync` 是官方代码的同步分支，保持与 `upstream/master2` 一致，不提交截图妈的改动。先更新它，再通过合并将官方改动吸收到 `main`。

本地 `main` 跟踪 `origin/main`；`upstream-sync` 跟踪 `upstream/master2`，推送目标设为 `origin`。发布工作流从标签指向的提交读取，不依赖分支名称，因此要对包含截图妈功能和发布配置的提交打标签。官方同步分支不会自动变成截图妈发行版。

新克隆中，添加官方远程并配置同步分支：

```powershell
git remote add upstream https://github.com/1003129155/jietuba.git
git config remote.upstream.tagOpt --no-tags
git fetch upstream
git switch -c upstream-sync --track upstream/master2
git config branch.upstream-sync.pushRemote origin
git switch main
```

先提交或妥善保存当前未提交的工作，再同步官方代码：

```powershell
git fetch upstream --no-tags
git switch upstream-sync
git merge --ff-only upstream/master2
git push origin upstream-sync
git switch main
git merge upstream-sync
git push origin main
```

合并到 `main` 时若发生冲突，保留截图妈所需的品牌和插件功能，完成验证后再提交、推送；也可从同步分支发起目标为 `main` 的 PR 进行审查。`--ff-only` 不会覆盖或重写历史；若同步分支快进失败，先检查是否混入了业务提交，不要强推。官方标签不自动取回，也不使用 `git push --tags` 批量推送，避免意外触发发布。

主程序使用 `release-<VERSION>`，其中版本必须与仓库的 `VERSION` 文件一致。发布前先将 `VERSION` 改为未使用的新版本并提交；例如改为 `2.2.4` 后：

```powershell
git tag release-2.2.4
git push origin release-2.2.4
```

推送该标签后，工作流构建并发布对应提交的版本，生成 x64 和 ARM64 完整版安装包、轻量版和完整版便携 ZIP、校验值及 WinGet 清单；普通分支推送不发布。安装包在各自架构的运行器上完成安装、升级和卸载检查后才会发布。默认按当前用户安装，可选择安装路径；卸载保留设置、模型和其他用户文件。交互式升级或卸载会检查安装目录内的程序并询问是否退出；优先请求正常退出，10 秒内未退出或旧版不支持退出请求时，再单独确认是否强制结束。拒绝结束会停留在安装前；静默操作检测到运行实例时直接失败，不擅自关闭。其他目录内的同名程序不受影响。

安装包与程序共用 `branding.json` 和 `VERSION`。如果启用 SignPath 签名，还需配置 `SIGNPATH_INSTALLER_ARTIFACT_CONFIGURATION` 指向安装包对应的签名配置；不配置签名策略时生成的安装包未签名。

不要覆盖、移动或强推已有标签。所有云端入口仅由版本标签触发，普通分支提交、PR、定时任务和手动分支构建不触发。`lite.yml` 与 `rust.yml` 仅供主发布工作流复用，不单独产生 runs；上游的独立 C++ 覆盖率工作流已移除，完整回归与旧 Python 对齐测试保留为本地诊断。

主项目标签为 `release-<VERSION>`；插件仓库支持批量 `v<批次版本>` 和独立插件 `<插件ID>/v<版本>`，详见插件仓库文档。失败后在原 tag run 上使用 GitHub 的 Re-run jobs，或 `gh run rerun <run-id> --failed`，不用创建分支构建。

每个仓库分别保留最近 3 条成功和 3 条失败的 Actions 运行记录。取消的记录不占用失败名额。清理是发布工作流中的收尾 job，即使构建失败也执行；不会删除当前运行、尚未结束的运行、Git 标签、Release 或其发布附件。运行中的保护可能短暂保留额外记录，后续完成时继续清理。

后续吸收官方更新时，保留本分支仅标签触发的策略，不恢复上游的分支 push 或 PR 检查入口。`scripts/test_actions_retention.py` 同时检查运行保留规则与单一 tag 入口。

## 发布构建复用

历史工件只接受本仓库整体完成且结论为 success 的发布 run；失败、取消、未完成的 run 均不读取。首次 SDK 种子来自已获授权且成功完成的一次维护导出，临时入口和开发分支已删除。日常仍仅版本标签触发。

x64 和 ARM64 静态 Qt SDK 按构建配方分别校验、复用并上传到当前 run；当前 run 的下游任务通过成功的 SDK job 依赖取得工件。SDK 校验包含架构、摘要、所需文件和最低 MSVC 版本；历史缓存不兼容或不存在时从源码构建。

每个架构在同一构建目录依次生成轻量版和完整版，共享 C++、Rust 和依赖输出。版本号隔离到单独源文件；版本差异宏只作用于更新器实现。跨标签保存整棵构建树，以及 Rust 测试产物、Cargo 下载和 cargo-about；缓存键包含工具链、构建配置和工作区路径。只为内容摘要一致的源码恢复时间戳，变更源码仍重新编译，Cargo 自行检查增量指纹。Git 凭据不进入缓存。

所有发行、架构烟测和安装器检查继续保留。应用增量缓存首次需要填充，实际云端总耗时应以下次标签构建为准。

安装器进程关闭逻辑可在隔离目录验证：`pwsh -File scripts/test_installer_process.ps1 -ScratchRoot <已有临时目录>`。脚本编译生产 Pascal 关闭逻辑，只替换确认对话框的返回值；覆盖拒绝、静默、正常退出、强制结束、超时和同名异目录进程保护，不操作实际安装实例。
