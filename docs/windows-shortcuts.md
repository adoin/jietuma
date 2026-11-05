# 钉图移动与系统 Win+V

点击钉图后，方向键每次移动 1 个屏幕坐标像素；Shift 加方向键每次移动 10 像素。编辑标注、文字输入及锁定移动时不会移动整张钉图，Ctrl/Alt/Win 加方向键沿用原有行为。

剪贴板启用且任一剪贴板全局快捷键为 Win+V 时，程序询问是否禁用 Windows 自带的 Win+V。同一会话只询问一次；关闭剪贴板或移除 Win+V 后再次启用会重新检查。已有禁用设置时不重复询问。

用户同意后，将当前用户注册表 `HKEY_CURRENT_USER\Software\Microsoft\Windows\CurrentVersion\Explorer\Advanced` 的 `DisabledHotkeys`（REG_SZ）原内容追加 `V`，保留已有字母；类型异常或无写入权限会报错。拒绝确认不写注册表。不修改系统剪贴板历史开关。通常需要注销并重新登录 Windows 后生效，程序不会自动注销或重启资源管理器。

恢复系统 Win+V：从同一注册表值中移除 `V`（保留其他字母），然后重新登录 Windows。

参考：[剪贴板替代程序的 Win+V 设置说明](https://github.com/Nucs/BetterClipboard)。
