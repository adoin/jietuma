# 插件

截图吧通过 `.jtbplugin` ZIP 包安装插件。系统托盘右键菜单或剪贴板窗口右下角菜单中的“安装插件”会检查清单和包内路径、限制解压大小，并将插件安装到数据目录的 `plugins/<id>/`。同 ID 的安装包会更新现有插件，并保留启用状态；“管理插件”可以启用、停用或卸载插件。插件是本机可执行程序，具有当前用户权限，安装前请确认来源可信。

插件项目和示例位于 [jietuma-plugins](https://github.com/adoin/jietuma-plugins)。发布页提供已构建的 `.jtbplugin` 文件。插件单独发布，不依赖截图吧的 Qt 版本。

## 清单与执行

ZIP 根目录包含 `plugin.json`，其中 `id` 由小写字母、数字、点、下划线或短横线组成；`api_version` 为 `2`，`type` 为 `extension`。插件在 `contributes` 下组合声明命令、钩子、工具栏和剪贴板能力。`command` 是包内相对路径的 Windows `.exe`；仅提供声明式内容的插件可省略。进程插件从标准输入读取 UTF-8 JSON，从标准输出写回一个 JSON 对象，不得在标准输出写日志。旧协议插件包不受支持，需要使用新协议重新打包。

## 程序级扩展：事件、命令与工具栏

`type: "extension"` 使用 `contributes` 声明命令、事件钩子、工具栏栏位及剪贴板能力；各项可以在同一个包中组合。当前宿主接入截图与剪贴板域，其他域可使用相同协议扩展。

```json
{
  "id": "my-extension", "name": "Example", "version": "1.0.0",
  "api_version": 2, "type": "extension", "command": "my-extension.exe",
  "contributes": {
    "commands": ["convert"],
    "hooks": [{"event": "screenshot.before_export", "priority": 10}],
    "toolbar": [{
      "id": "convert", "location": "screenshot", "name": "Convert and copy",
      "name_i18n": {"zh": "转换并复制"},
      "pipeline": [{"plugin": "my-extension", "command": "convert"}]
    }]
  }
}
```

截图工具栏栏位默认位于“…”中，可在工具栏的调整界面显示、隐藏或排序。`icon` 可选，填写包内 PNG 或 SVG 相对路径；未提供时使用通用图标。`pipeline` 按数组顺序调用不同插件声明的 `commands`，前一步的结果就是下一步的输入；缺少或停用依赖、执行失败、响应无效和超时都会中止，保留截图窗口供重试。末尾结果为图片或文本时复制到剪贴板。工具栏栏位在下一次截图时重新加载；钩子和命令以执行时的启用状态为准。

当前截图事件依次包括 `screenshot.started`（开始截图会话）、`screenshot.selection_confirmed`（选区确认，可多次触发）、`screenshot.before_export`（复制、保存、快速保存、钉图或插件栏位导出前）以及 `screenshot.exported`（生成最终输出）和 `screenshot.ended`（会话结束）。没有导出就结束时还发送 `screenshot.cancelled`。`exported` 表示输出已生成，并不保证随后异步复制或保存成功；OCR、扫码、长截图和录屏暂不经过导出链。钉图插件改变图片时使用合成结果，标注不再可单独编辑。

同一截图会话的事件和命令串行执行；同一事件的钩子按 `priority` 从大到小排序，相同优先级按插件 ID 排序。`before_export` 钩子接收 PNG 图片并可返回修改后的图片；失败时取消该次导出，不会静默绕过处理。其他事件用于通知，返回值不会改变后续的原始选区。事件和命令在后台进程执行，启动超时 2 秒、运行超时 5 秒；关闭截图窗口后不再执行与该窗口关联的完成回调。

请求是 `{"api_version":2,"type":"extension","event":"...","command":"...","payload":{...},"context":{...}}`。事件的 `command` 为空，工具栏命令的 `event` 为 `command`。`context` 包含 `session_id`、`action`、选区坐标 `selection`；截图内容为敏感信息，只有声明了相应事件或命令的已启用插件才会收到。插件返回 `{"api_version":2,"ok":true,"payload":{...}}`；省略 `payload` 表示保持输入，`ok:false` 表示失败。截图链的数据类型为 `{"kind":"image","data":"<PNG Base64>"}`、`{"kind":"text","data":"..."}` 或 `{"kind":"none"}`；总 JSON 上限 8 MiB，图片不超过 6400 万像素。导出前图片超过限制时中止导出；导出通知若图片超过限制则只携带 `none`。

## 剪贴板能力

在 `contributes.clipboard` 中可组合声明 `preview`、`action`、`metadata` 和 `backup`。前三者声明 `content_types`，命令型能力的 `command` 必须在 `contributes.commands` 注册。预览命令接收 `payload.kind: "clipboard"`，`data` 中包含 `content_type`、`sub_kind`、`content`、`language`、`paths` 和可选 `html`。清单中的 `formats` 最多声明 8 种原始格式，符合限制的数据通过 `formats_base64` 提供。预览返回 `payload: {"kind":"preview","data":{"kind":"text|html|markdown|image","content":"..."}}`；不适用时返回 `payload: {"kind":"none"}`。预览失败、无效或超时后回退到内置预览。

文本操作命令接收 `payload: {"kind":"text","data":"..."}`，返回相同类型；`context.source_app` 提供来源。这个通用文本命令也可以接在截图插件的文本输出之后。格式检查使用预览能力的 `modes: ["formats"]` 和 `formats: ["*"]`；选择单项时提供该格式的数据。

采集过滤器在 `contributes.hooks` 中声明 `clipboard.before_capture` 和 `content_types`。事件的 `payload.kind` 为 `clipboard`，返回 `payload: {"kind":"decision","data":false}` 可阻止入史。过滤器出错、超时或输入过大时匹配内容不会保存，并提示用户修复或停用插件；安装、更新和停用与采集互斥。

清单中的 `settings` 可声明最多 8 个 `multiline` 设置项，字段包括 `key`、`type`、`label` 和可选的 `label_i18n`、`default`。用户在“管理插件 → 插件设置”编辑；插件通过环境变量 `JIETUBA_PLUGIN_CONFIG` 读取 JSON 文件。配置独立于插件程序保存，更新或卸载插件时保留。

“敏感内容保护”过滤器识别部分高置信度私钥、服务令牌和 JWT；它不能识别所有秘密，且可能跳过文档中的样例。“忽略指定应用”过滤器按设置中的程序文件名精确匹配来源，系统无法确定来源时无法匹配。

安装具有 `clipboard.backup` 能力的扩展后，剪贴板设置菜单提供加密备份及恢复。备份命令接收 `payload.kind: "backup"`，`data` 包含操作、路径和本次密码。备份使用 SQLite 一致快照与图片文件，恢复先验证到新目录，再可选择切换；现有数据不被覆盖。密码只在本次任务中通过标准输入传给插件，不保存在插件设置中。

声明式 `clipboard.metadata` 能力使用 `fields` 描述条目级 `line` 或 `multiline` 字段。用户通过条目右键菜单编辑，内容存入历史数据库并参与搜索；删除条目时元数据随之删除，也包含在完整备份中。插件被停用或卸载时，已保存的条目元数据保留。

## 原始格式

截图吧保存常见文本、HTML、RTF、图片、文件及额外注册格式，格式视图可按名称逐项查看。额外格式最多采集 16 项、单项 4 MiB、合计 16 MiB。虚拟文件、OLE 句柄、隐私标记和可能触发昂贵延迟渲染的表格格式不作为通用格式采集；已有普通文本、图片及文件行为保持不变。来源应用要求不记录的内容仍会跳过。插件只能读取已经保存且自己在清单中声明的格式。


## 可叠加类型、选择字段和多格式动作

`contributes.clipboard.types` 是数组，每项包含 `id`、`name`、`name_i18n`、`metadata_field`、`exclude_values`。宿主依据本插件的条目元数据字段计算保存在 `types` 数组中的类型，并以 `plugin:<插件 id>:<类型 id>` 查询历史。原始 `content_type` 保持不变，因此同一条目可以同时显示在文本和插件类型中。`types` 是该能力的保留元数据字段。

`clipboard.metadata.fields` 支持 `choice`，通过 `choices: [{value, name, name_i18n}]` 声明下拉选项。文本行右侧 `</>` 按钮提供这些选项；元数据编辑窗口也使用同一选项。插件的全局 `settings` 还支持 `boolean`、`number`（整数，`minimum`、`maximum`）、`choice`，原有 `multiline` 保持不变。设置保存在独立的 `.data/<id>.json` 中。

`clipboard.action.entries` 可声明多个 `{id, command, name, name_i18n}` 动作；所有 command 必须同时列在 `contributes.commands`。可选的 `when_formats_any` 和 `when_text_contains` 以 OR 关系控制适用范围。

动作仍可返回 `payload.kind=text` 表示复制文本，也可返回 `original` 表示使用该历史条目的原格式粘贴，或返回 `clipboard`：

```json
{"api_version":2,"ok":true,"payload":{"kind":"clipboard","data":{"text":"A\tB","formats_base64":{"XML Spreadsheet":"BASE64_BYTES"}}}}
```

多格式动作由宿主写入剪贴板后粘贴到打开面板前的目标应用。最多 8 个格式、解码后总计 4 MiB；仅接收自包含的注册格式字节，拒绝已知的 OLE/虚拟文件句柄、BIFF 和保留的 CF_ 格式。普通文本通过 `text` 字段提供。`original` 不经插件转码，避免大型原始格式在协议中往返。插件不会直接模拟系统键鼠。

预览请求新增本插件的 `metadata` 及 `appearance`（light/dark）。悬停预览异步执行，展示 HTML/Markdown 时由宿主阻止本地文件及网络资源加载。


`contributes.clipboard.action.default_for_paste: true` 将该能力的主 `command` 接入单条历史的带格式粘贴流程。带 entries 时仅同名 command 对应的条目作为默认处理器；多个匹配处理器按插件 ID 顺序取首个。插件返回 `clipboard` 时写入其格式并继续粘贴，`original` 或 `none` 则回退宿主原格式流程，错误不会发送粘贴。纯文本粘贴跳过此处理器。异步请求固定原目标窗口，新的单条粘贴请求会使旧处理结果失效。插件输出的注册格式先于纯文本后备格式写入 Windows 剪贴板。

动作请求的 `context.target_window` 是本次粘贴目标窗口句柄的十进制字符串，供已安装插件自行校验目标并实现应用适配；宿主不包含 Office 业务逻辑。`action.timeout_ms` 可在 1000—30000 毫秒内设置执行上限，默认 5000 毫秒。


### 插件安装、版本和图标

插件必须提供有效 SemVer `version`，插件身份以 `id` 为准。批量安装同 ID 仅选择最高版本，再与已安装版本比较；相同或更低版本跳过，稳定版高于同版本预发布，构建元数据不影响优先级。目录始终为 `plugins/<id>`，升级保留配置和禁用状态。文件选择器支持多选并记住目录；文件关联打开支持单独安装窗口及向运行中实例转交。

顶层 `icon` 指定包内 PNG、ICO 或 SVG，用于安装确认和插件管理列表。显示前限制图像大小，缺失图标使用应用图标。官方打包器把最大 1 MiB 的多尺寸 ICO（16—256 像素）作为文件头，后面连接标准 ZIP 数据。Windows 的 `DefaultIcon="%1"` 原生读取该图标；截图妈验证 ICO 目录和边界，再从 ZIP 起始位置读取清单与内容。普通 ZIP 包仍可安装。图标头和 ZIP 都不执行插件代码，安装器注册文件关联并在卸载时只移除本程序拥有的关联。
