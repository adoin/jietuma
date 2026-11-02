<div align="center">

<img src="assets/jietuma-logo.png" width="128" alt="截圖媽 logo" />

# 截圖媽

**Windows 截圖、螢幕繪製、OCR、釘圖、翻譯與剪貼簿工具**

[![build](https://img.shields.io/github/actions/workflow/status/adoin/jietuma/build.yml?label=release&style=flat-square)](https://github.com/adoin/jietuma/actions/workflows/build.yml) [![license](https://img.shields.io/github/license/adoin/jietuma?style=flat-square)](LICENSE) [![release](https://img.shields.io/github/v/release/adoin/jietuma?style=flat-square)](https://github.com/adoin/jietuma/releases/latest) ![platform](https://img.shields.io/badge/Windows-x64%20%7C%20ARM64-0078D4?style=flat-square)

[下載 Windows 版](https://github.com/adoin/jietuma/releases/latest) · [從原始碼建置](#source-setup) · [開發與測試](#development)

[简体中文](README_zh-CN.md) | **繁體中文** | [English](README.md) | [日本語](README_JA.md) | [한국어](README_KO.md)

</div>

**截圖媽（Jietuma）**是基於上游 [jietuba](https://github.com/1003129155/jietuba) 的獨立發行版。名稱及發行存放庫統一設定於 [branding.json](branding.json)，詳見[品牌與發行說明](docs/branding.md)。

## 外掛生態

外掛可擴充截圖生命週期、工具列位置及串接流程，以及剪貼簿預覽、類型、中繼資料和貼上動作。外掛透過 `.jtbplugin` 套件獨立安裝；批次安裝記住目錄，按外掛 ID 比較版本，僅啟用最新版本。安裝包與管理清單均顯示外掛圖示。詳見[外掛開發與安裝](docs/plugins.md)及[外掛專案](https://github.com/adoin/jietuma-plugins)。

## 專案簡介

截圖媽是一款免費開源的 Windows 截圖與效率工具，集截圖標註、螢幕繪製、OCR、翻譯、萬物釘圖、螢幕錄影與剪貼簿歷史於一體。打通截圖、釘圖與剪貼簿，截下來的內容隨時找回、隨手再利用。

## 截圖 ⇄ 釘圖 ⇄ 剪貼簿，三向互通

截完存入歷史，從歷史一鍵釘回螢幕；釘圖繼續標註、取字，再複製回剪貼簿，三者聯動，一氣呵成。

- **豐富截圖工具，絲滑操作**：智慧吸附視窗與控制項，十餘種標註工具隨手可用；四向長截圖、自動滾動、多螢幕與混合 DPI、HDR 截圖。
- **萬物釘圖**：一鍵釘住你需要的內容，文字、圖片、檔案，邊看邊用；縮放、標註、取字、翻譯，隨手完成。
- **剪貼簿記錄一切**：不只是自己的截圖，任何來源複製即記錄，群組收藏、搜尋篩選、多選拖出，重要內容隨時找回；任何內容都可以直接拖曳到想要的視窗，或者釘在桌面。
- **本機執行、彈性部署**：採用 C++/Rust，記憶體用量依啟用的功能與外掛而異。提供安裝程式與可攜式 ZIP 套件。

由 C++（Qt6）實作，影像處理、剪貼簿、PP-OCR 引擎等核心由 Rust 實作，支援 Windows x86_64 與 ARM64。早期的 Python（PySide6）實作已封存到 [`archive/python`](https://github.com/1003129155/jietuba/tree/archive/python) 分支，不再更新。

## 功能一覽

<table>
<tr>
<td width="50%" valign="top">
<img width="100%" alt="截圖與標註" src="https://github.com/user-attachments/assets/397d6a3a-51a6-4860-9d05-0aed2c7c0429" />
<p><b>截圖與標註</b><br>智慧辨識視窗和控制項，畫筆、箭頭、序號、馬賽克、聚光燈、浮水印等十餘種標註，隨時復原重做。</p>
</td>
<td width="50%" valign="top">
<img width="100%" alt="剪貼簿歷史" src="https://github.com/user-attachments/assets/d34ff233-fe7f-49dd-89ae-c5978fa25b5a" />
<p><b>剪貼簿歷史</b><br>記錄一切，萬物釘圖。群組收藏、搜尋篩選、多選拖出，複製過的內容隨時找回。</p>
</td>
</tr>
<tr>
<td width="50%" valign="top">
<img width="100%" alt="滾動長截圖" src="https://github.com/user-attachments/assets/a179b3e7-d201-4208-b518-81cecc4e9661" />
<p><b>滾動長截圖</b><br>上下左右四個方向自動拼接，可自動滾動；往回滾只定位不截短，一鍵裁掉多餘部分。</p>
</td>
<td width="50%" valign="top">
<img width="100%" alt="OCR 文字辨識" src="https://github.com/user-attachments/assets/60a16100-5edc-4543-9a35-daf05b1e244e" />
<p><b>OCR 文字辨識</b><br>Windows 11 剪取工具 OCR 與 PP-OCR 雙引擎，離線辨識，自動合併換行、整理標點。</p>
</td>
</tr>
<tr>
<td width="50%" valign="top">
<img width="100%" alt="釘圖" src="https://github.com/user-attachments/assets/d03e74e7-576b-46eb-8aea-21cc96d6a020" />
<p><b>萬物釘圖</b><br>一鍵釘在螢幕上，邊看邊用。縮放、標註、取字、翻譯，隨手完成。</p>
</td>
<td width="50%" valign="top">
<img width="100%" alt="GIF 與 MP4 錄製" src="https://github.com/user-attachments/assets/284f09b1-24b2-4569-9bc7-278c3bcd34da" />
<p><b>GIF / MP4 錄製</b><br>錄製中可標註，GIF 可回放剪輯；MP4 可調影格率與位元速率，可錄系統聲音和滑鼠游標，不錄麥克風。</p>
</td>
</tr>
<tr>
<td width="50%" valign="top">
<p><b>翻譯</b><br>選中文字，一鍵翻譯。支援 DeepL、Google、Azure、Amazon、百度、火山引擎、騰訊雲、有道、DeepSeek 與 OpenAI 相容介面，結果可朗讀。</p>
</td>
<td width="50%" valign="top">
<p><b>螢幕繪製</b><br>直接在螢幕上書寫、標註，邊操作邊講解；白板和游標高亮，讓簡報、教學更直觀。</p>
</td>
</tr>
</table>

還有：

- **掃碼**：辨識選區裡的 QR Code 和條碼，一次多個。
- **全域滑鼠快速鍵**：按住輔助按鍵拖曳，直接截圖、釘圖、辨識文字、翻譯或掃碼（QR Code / 條碼）。
- **其他**：PDF 匯出、截圖浮水印、延遲截圖、五種介面語言（簡中、繁中、英、日、韓）。

---

## 下載與使用

1. 開啟 [Releases](https://github.com/adoin/jietuma/releases/latest)，下載 `jietuma_setup-release-<版本>-x64.exe` 或 ARM64 安裝程式。
2. 執行安裝程式，支援升級與解除安裝；也可選擇下表中的可攜式 ZIP 套件。

|  | 完整版 `jietuma_pp-…zip` | 輕量版 `jietuma_lite-…zip` |
|---|---|---|
| 套件內容 | `jietuma_pp.exe` 和同層的 `models/` 目錄（都要保留） | 單一 `jietuma_lite.exe` |
| OCR 引擎 | 剪取工具 OCR + 內建 PP-OCR，不裝系統剪取工具也能辨識 | 預設使用 Windows 11 剪取工具的 OCR，可在設定裡下載 [PP-OCR 元件包](#ppocr-pack) |
| 適合 | Windows 10，或想要 OCR 開箱即用 | Windows 11，想要體積小 |

兩個版本預設自動選擇可用引擎：優先使用 Windows 11 剪取工具 OCR，否則嘗試 PP-OCR；可在設定的「OCR 設定」中手動切換。辨識語種取決於引擎和模型，完整版內建的 PP-OCRv6 small 支援中、英、日。剪取工具版本低於 11.2308（沒有「文字操作」）或未安裝剪取工具時，輕量版需要可用的剪取工具 OCR 或 PP-OCR 元件包才能辨識文字。

> [!NOTE]
> 透過瀏覽器下載後，Windows 可能顯示執行警告。出現提示時，按「更多資訊」，再選擇「仍要執行」即可啟動。

> [!WARNING]
> 兩個版本共用設定和剪貼簿歷史，可以互相換用，但不要同時執行，預設快速鍵會衝突。

> [!TIP]
> 設定預設保存在 `%APPDATA%\Jietuma\settings.ini`。在 exe 旁放一個 `settings.ini`（空檔案也可以），程式就改用它，複製程式資料夾即可帶走設定。截圖儲存資料夾、剪貼簿歷史位置、開機自動啟動這類只對本機有意義的項目仍留在登錄檔；剪貼簿歷史預設在 `%APPDATA%\Jietuma\clipboard`，可在設定的「剪貼簿」頁更換位置。

### 更新

在「關於 → 檢查更新 → 更新並重新啟動」手動更新。「關於」頁提供可選的「自動更新」開關：啟用後檢查本存放庫的 GitHub Releases，待程式視窗、釘圖、錄製和匯出等工作不再佔用時，以對應安裝程式升級。設定、剪貼簿歷史及外掛均會保留。

<a id="ppocr-pack"></a>

### PP-OCR 元件包（輕量版）

輕量版帶 PP-OCR 的前後處理，但不帶 ONNX Runtime 和模型。可在設定的「管理 PP-OCR 模型」中下載：模型從 ModelScope 取得，ONNX Runtime 從微軟發行的 `onnxruntime` wheel 中提取，下載來源包括 PyPI 和鏡像站；下載後依內建清單驗證檔案大小與雜湊。支援 PP-OCRv6、PP-OCRv5 和多種語種辨識模型，也可以新增本地 ONNX 檔案。下載、新增、切換都不用重新啟動。

也可以手動放進 exe 旁的 `models/` 目錄：`PP-OCRv6_det_small.onnx`、`PP-OCRv6_rec_small.onnx`（存放庫的 [models/](models/) 裡就有）和 ONNX Runtime 1.24 及以上的純 CPU 版 `onnxruntime.dll`。

## 程式碼簽章政策

Windows 發行套件中的 `jietuma_pp.exe` 和 `jietuma_lite.exe` 均由 GitHub Actions 從本存放庫 [cpp/](cpp/) 的 C++ 原始碼建置。發行流程支援透過 SignPath 進行程式碼簽章；是否簽章取決於該次建置組態，請以具體發行套件的 EXE 數位簽章和發行說明為準。


- 提交者、審查者、核准者：[@adoin](https://github.com/adoin)
- 外部貢獻透過 Pull Request 提交，經維護者審核後才會合併。

<a id="privacy"></a>

### 隱私說明

截圖、OCR、標註、錄影及剪貼簿歷史均在本機處理與保存。選用的網路功能會連線至設定的服務；啟用自動更新後會存取本存放庫的 GitHub Releases。已安裝外掛以目前使用者權限執行，可存取截圖、剪貼簿內容及檔案。

- 日常截圖、OCR 辨識和剪貼簿記錄不需要上傳內容；使用 PP-OCR 前需準備好本地模型和執行階段程式庫。
- 翻譯時，待翻譯文字會傳送到設定中選定的服務，並使用你設定的憑證；該服務的隱私權政策適用。自訂 OpenAI 相容介面也可以指向本機服務，此時文字傳送到該本地端點。
- 在**關於 → 檢查更新**中檢查或安裝更新時，程式會存取 GitHub 取得版本資訊並下載發行套件。
- 在「管理 PP-OCR 模型」中下載模型或執行階段程式庫時，會存取 ModelScope、PyPI 或內建清單中的鏡像來源；這些請求用於下載元件，不上傳待辨識圖片。
- 在翻譯設定中測試連線或取得模型清單時，會請求你設定的服務端點。連線測試傳送固定範例文字；取得模型清單不傳送剪貼簿內容。

---

<a id="source-setup"></a>

## 從原始碼建置

需要 Visual Studio 的「使用 C++ 的桌面開發」（含 CMake）、Rust MSVC 工具鏈和 Qt 6.11 的 MSVC SDK。用 PowerShell 執行：

```powershell
git clone https://github.com/adoin/jietuma.git
cd jietuma/cpp
python -m venv .tools
.\.tools\Scripts\python.exe -m pip install py7zr==1.1.4
.\.tools\Scripts\python.exe scripts\install_qt.py   # 下載 Qt SDK 到 .sdk/
.\build.cmd                                         # 編譯、測試並部署到 dist/Release
```

已有 Qt6 MSVC x64 SDK 時，用 `.\scripts\build.ps1 -QtRoot C:\Qt\6.11.0\msvc2022_64`；`-SkipTests` 跳過測試，`-Configuration Debug` 建置 Debug 版，建置完執行 `run.cmd` 即可。

發行用的單一檔案版靜態連結 Qt 和 MSVC 執行階段程式庫，無需另行安裝這兩項；OCR 所需模型與 ONNX Runtime 見上方元件包說明。先用 `.\scripts\build_static_qt.ps1` 從 Qt 官方原始碼建置一份靜態 SDK，再建置：

```powershell
.\build.cmd -Static          # 輕量版：dist/Release-static/jietuma_lite.exe
.\build.cmd -Static -Full    # 完整版：dist/Release-static-full/jietuma_pp.exe，內嵌 ONNX Runtime，附預設 PP-OCR 模型
```

ARM64 版在 x64 上交叉編譯：先建置好 x64 靜態 Qt，再執行 `.\scripts\build_static_qt.ps1 -Arch arm64` 和 `.\build.cmd -Static -Arch arm64`，需要 Visual Studio 的 MSVC ARM64 建置工具和 `rustup target add aarch64-pc-windows-msvc`。版本號取自存放庫根目錄的 `VERSION` 檔案。

PP-OCR 模型放在存放庫的 [models/](models/) 目錄中，包括 `PP-OCRv6_det_small.onnx` 和 `PP-OCRv6_rec_small.onnx`。剪取工具 OCR 所需檔案取自本機的 Windows 11 剪取工具，首次使用時自動複製到 `%LOCALAPPDATA%\Jietuma\oneocr`，不需要另外下載。

---

<a id="development"></a>

## 開發與測試

`.\build.cmd` 會編譯並執行全部測試，其中名稱以 `_desktop` 結尾的測試會在真實桌面上注入鍵鼠、註冊全域快速鍵、截圖，執行期間不要操作滑鼠和鍵盤。只執行不需要桌面的測試：

```powershell
$env:PATH = "$PWD\.sdk\6.11.0\msvc2022_64\bin;$env:PATH"
ctest --test-dir build\msvc -C Release --output-on-failure -E "_desktop$|_windows_parity$"
```

測試程式都在臨時沙盒裡執行，不會讀寫本機的設定、剪貼簿歷史和日誌。另有分析建置（`scripts\check.ps1`：AddressSanitizer、程式碼分析、洩漏偵測）和涵蓋率（`scripts\coverage.ps1`）。

[發行流程](.github/workflows/build.yml) 僅由 `release-*` 標籤觸發。Rust 測試與授權檢查、x64/ARM64 建置、安裝／升級／解除安裝煙霧測試集中在同一次執行中；一般提交與 PR 不觸發 Actions。完整 C++ 回歸、涵蓋率與舊 Python／介面對齊測試仍可在本機執行，不列為本次發行驗證。Actions 歷史保留最近 3 次。

發行由 [build.yml](.github/workflows/build.yml) 完成：標籤必須是 `release-<VERSION 檔案裡的版本>`；[lite.yml](.github/workflows/lite.yml) 建置輕量版和完整版（x64 與 ARM64），再由發行流程按組態簽章、封裝，分別產生 `jietuma_lite-<tag>-<arch>.zip` 和 `jietuma_pp-<tag>-<arch>.zip`。

---

## 目錄結構總覽

```text
# 專案根目錄
├── README.md / README_zh-CN.md / README_zh-TW.md / README_JA.md / README_KO.md   # 英文、簡中、繁中、日文、韓文說明文件
├── VERSION                  # 版本號，CMake 與發行流程都讀它
├── app.ico                  # 程式與更新器圖示
├── licenses/                # 發行套件附帶的第三方授權
│
├── cpp/                     # C++（Qt6）實作
│   ├── src/                 # 原始碼，按 <套件>/<模組> 組織
│   │   ├── annotation/      # 螢幕即時標註 — 每屏一層、穿透切換、跨屏復原
│   │   ├── barcode/         # QR Code/條碼辨識（zxing-cpp）
│   │   ├── canvas/          # 畫布 — 圖形編輯核心、復原、圖層
│   │   ├── capture/         # 截圖擷取 — 螢幕截圖、視窗與元素辨識
│   │   ├── clipboard/       # 剪貼簿管理 — 歷史、群組/快速啟動、匯入匯出、搜尋
│   │   ├── core/            # 核心 — 啟動引導、日誌、資源、主題、國際化、快速鍵
│   │   ├── gif/             # GIF/MP4 錄製、GIF 編輯、回放、匯出
│   │   ├── ocr/             # 剪取工具 OCR 與 PP-OCR
│   │   ├── pin/             # 釘圖 — 置頂、編輯、OCR、翻譯
│   │   ├── settings/        # 統一設定管理
│   │   ├── stitch/          # 長截圖拼接
│   │   ├── text_recognition/ # OCR 結果排版
│   │   ├── tools/           # 繪圖工具 — 筆、矩形、箭頭、文字、馬賽克等
│   │   ├── translation/     # 多提供商翻譯與朗讀
│   │   ├── ui/              # 介面元件、截圖視窗、工具列、設定視窗
│   │   └── video/           # MP4 錄製子處理程序
│   ├── native/              # rust_libs 的 C 介面，靜態連結
│   ├── resources/           # 預設設定、介面翻譯（XML 與 .qm）、日誌文案
│   ├── scripts/             # 建置、Qt SDK 準備、翻譯編譯
│   └── tests/               # QtTest 測試與對照資料
│
├── rust_libs/               # Rust 程式庫原始碼，由 C++ 透過 cpp/native 靜態連結
│   ├── gifrecorder/         # GIF 影格儲存、回放與合成編碼
│   ├── longstitch/          # 長截圖拼接演算法
│   ├── pyclipboard/         # 剪貼簿底層操作
│   ├── ppocr_rust/          # PP-OCR (PaddleOCR) ONNX 辨識引擎
│   ├── hdrcapture/          # HDR 桌面截圖
│   ├── updater/             # 只替換主 EXE 的獨立更新器
│   └── video_recorder/      # Media Foundation 錄製與 WASAPI 系統聲音採集
│
├── models/                  # PP-OCR ONNX 模型（完整版隨套件發行）
│   ├── PP-OCRv6_det_small.onnx   # 文字偵測模型 (DBNet)
│   └── PP-OCRv6_rec_small.onnx   # 文字辨識模型 (CRNN/CTC)
│
└── svg/                     # SVG 圖示資源
```

## 模組詳細說明

各模組的詳細說明見 [docs/MODULES.md](docs/MODULES.md)。

---

## 授權

版權和 MIT 授權見 [LICENSE](LICENSE)；Qt 與其他第三方元件的說明見 [THIRD-PARTY-NOTICES.md](cpp/THIRD-PARTY-NOTICES.md)。
