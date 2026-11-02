<div align="center">

<img src="assets/jietuma-logo.png" width="128" alt="jietuma logo" />

# jietuma

**Windows 向けスクリーンショット・画面描画・OCR・ピン留め・翻訳・クリップボードツール**

[![build](https://img.shields.io/github/actions/workflow/status/adoin/jietuma/cpp.yml?branch=main&label=build&style=flat-square)](https://github.com/adoin/jietuma/actions/workflows/cpp.yml) [![license](https://img.shields.io/github/license/adoin/jietuma?style=flat-square)](LICENSE) [![release](https://img.shields.io/github/v/release/adoin/jietuma?style=flat-square)](https://github.com/adoin/jietuma/releases/latest) ![platform](https://img.shields.io/badge/Windows-x64%20%7C%20ARM64-0078D4?style=flat-square)

[Windows 版をダウンロード](https://github.com/adoin/jietuma/releases/latest) · [ソースからビルド](#source-setup) · [開発とテスト](#development)

[簡体中文](README_zh-CN.md) | [繁體中文](README_zh-TW.md) | [English](README.md) | **日本語** | [한국어](README_KO.md)

</div>

**Jietuma（截图妈）** は [jietuba](https://github.com/1003129155/jietuba) を基にした独立配布版です。名称とリリース先は [branding.json](branding.json) で管理します。[ブランドとリリース](docs/branding.md)も参照してください。

## プラグイン

スクリーンショットのライフサイクル、ツールバー、パイプライン、クリップボードのプレビュー・種類・メタデータ・貼り付け操作を拡張できます。`.jtbplugin` は個別にインストールします。一括インストールはフォルダーを記憶し、プラグイン ID ごとに最新版だけを適用します。パッケージと管理画面にアイコンを表示します。[開発とインストール](docs/plugins.md)・[プラグインリポジトリ](https://github.com/adoin/jietuba-plugins)。

## 概要

jietuma は Windows 向けの無料・オープンソースのスクリーンショット・作業支援ツールです。スクリーンショットへの注釈、画面描画、OCR、翻訳、何でもピン留め、録画、クリップボード履歴をひとつに統合。キャプチャ・ピン留め・クリップボードがつながり、撮ったものはいつでも見つけ出して再利用できます。

## スクリーンショット ⇄ ピン留め ⇄ クリップボード、滑らかにつながる

キャプチャを履歴に保存し、ワンクリックで画面にピン留め。注釈を加えたり文字を取り出したりして、再びクリップボードへ。3 つの機能が連携し、ひと続きの操作で完結します。

- **豊富なキャプチャツール、滑らかな操作感**：ウィンドウやコントロールに吸着し、十数種類の注釈ツールをすぐに使用。上下左右のスクロールキャプチャ、自動スクロール、複数モニター、異なる DPI、HDR キャプチャに対応。
- **何でもピン留め**：テキストも画像もファイルも、必要なものをワンクリックで画面に固定して、見ながら作業。拡大縮小、注釈、文字の取り出し、翻訳もその場で。
- **クリップボードですべて記録**：自分で撮ったスクリーンショットだけでなく、どこからコピーしたものでも履歴に保存。グループやお気に入りで整理し、検索・絞り込み・複数項目のドラッグで、大切な情報をいつでも取り出せます。内容を使いたいウィンドウに直接ドラッグしたり、画面にピン留めしたりできます。
- **ローカル実行と柔軟な配布**：C++/Rust で実装し、メモリ使用量は有効な機能やプラグインによって変わります。インストーラーとポータブル ZIP を提供します。

C++（Qt6）で実装し、画像処理・クリップボード・PP-OCR エンジンなどのコアは Rust で実装しています。Windows x86_64 および ARM64 に対応。以前の Python（PySide6）実装は [`archive/python`](https://github.com/1003129155/jietuba/tree/archive/python) ブランチにアーカイブ済みで、今後は更新しません。

## 機能一覧

<table>
<tr>
<td width="50%" valign="top">
<img width="100%" alt="キャプチャと注釈" src="https://github.com/user-attachments/assets/397d6a3a-51a6-4860-9d05-0aed2c7c0429" />
<p><b>キャプチャと注釈</b><br>ウィンドウやコントロールを自動認識。ペン、矢印、番号、モザイク、スポットライト、透かしなど十数種の注釈をいつでも元に戻せます。</p>
</td>
<td width="50%" valign="top">
<img width="100%" alt="クリップボード履歴" src="https://github.com/user-attachments/assets/d34ff233-fe7f-49dd-89ae-c5978fa25b5a" />
<p><b>クリップボード履歴</b><br>すべて記録、何でもピン留め。グループ分け、お気に入り、検索、絞り込み、複数項目のドラッグで、コピーした情報がいつでも見つかります。</p>
</td>
</tr>
<tr>
<td width="50%" valign="top">
<img width="100%" alt="スクロールキャプチャ" src="https://github.com/user-attachments/assets/a179b3e7-d201-4208-b518-81cecc4e9661" />
<p><b>スクロールキャプチャ</b><br>上下左右の 4 方向を自動結合し、自動スクロールにも対応。戻しても位置合わせだけで短くならず、余分な部分はワンクリックで切り取れます。</p>
</td>
<td width="50%" valign="top">
<img width="100%" alt="OCR 文字認識" src="https://github.com/user-attachments/assets/60a16100-5edc-4543-9a35-daf05b1e244e" />
<p><b>OCR 文字認識</b><br>Windows 11 Snipping Tool の OCR と PP-OCR の 2 エンジン、オフライン認識。折り返しの結合や句読点も自動で整えます。</p>
</td>
</tr>
<tr>
<td width="50%" valign="top">
<img width="100%" alt="ピン留め" src="https://github.com/user-attachments/assets/d03e74e7-576b-46eb-8aea-21cc96d6a020" />
<p><b>何でもピン留め</b><br>ワンクリックで画面に固定し、見ながら作業。拡大縮小、注釈、文字の取り出し、翻訳もその場で。</p>
</td>
<td width="50%" valign="top">
<img width="100%" alt="GIF と MP4 の録画" src="https://github.com/user-attachments/assets/284f09b1-24b2-4569-9bc7-278c3bcd34da" />
<p><b>GIF / MP4 録画</b><br>録画中に注釈でき、GIF は再生しながら編集。MP4 はフレームレートとビットレートを指定でき、システム音声とマウスポインターを録画（マイクは非対応）。</p>
</td>
</tr>
<tr>
<td width="50%" valign="top">
<p><b>翻訳</b><br>選択した文字をワンキーで翻訳。DeepL、Google、Azure、Amazon、Baidu、Volcengine、Tencent Cloud、Youdao、DeepSeek、OpenAI 互換 API に対応し、結果の読み上げもできます。</p>
</td>
<td width="50%" valign="top">
<p><b>画面に直接描画</b><br>画面に直接書き込み、操作しながら説明。ホワイトボードとカーソルハイライトで、デモや授業をわかりやすく。</p>
</td>
</tr>
</table>

ほかにも：

- **コード読み取り**：選択範囲内の QR コードとバーコードを一度に複数読み取ります。
- **グローバルマウスショートカット**：修飾キーを押したままドラッグして、すぐにキャプチャ・ピン留め・文字認識・翻訳・QRコード／バーコードの読み取り。
- **その他**：PDF エクスポート、透かし、遅延キャプチャ、5 つの表示言語（簡体字中国語、繁体字中国語、英語、日本語、韓国語）。


---

## ダウンロードと起動

1. [Releases](https://github.com/adoin/jietuma/releases/latest) から `jietuma_setup-release-<version>-x64.exe` または ARM64 インストーラーをダウンロードします。
2. インストーラーは更新とアンインストールに対応します。下記のポータブル ZIP も利用できます。

|  | 完全版 `jietuma_pp-…zip` | 軽量版 `jietuma_lite-…zip` |
|---|---|---|
| 中身 | `jietuma_pp.exe` と同じ階層の `models/` フォルダー（両方必要） | 単一の `jietuma_lite.exe` |
| OCR エンジン | Snipping Tool OCR + 同梱の PP-OCR。システムの Snipping Tool がなくても認識可能 | 既定では Windows 11 Snipping Tool OCR。必要に応じて設定から [PP-OCR コンポーネント](#ppocr-pack)をダウンロード |
| おすすめ | Windows 10、またはすぐに OCR を使いたい場合 | Windows 11 で、サイズを小さくしたい場合 |

どちらも利用可能なエンジンを自動選択し、Windows 11 Snipping Tool OCR を優先、それがなければ PP-OCR を試みます。設定の「OCR設定」で手動選択もできます。対応言語はエンジンとモデルによって異なり、同梱の PP-OCRv6 small は中国語・英語・日本語に対応します。Snipping Tool がない場合や、11.2308 より古く「テキスト アクション」がない場合、軽量版で文字を認識するには利用可能な Snipping Tool OCR または PP-OCR コンポーネントが必要です。

> [!NOTE]
> ブラウザーからダウンロードすると Windows の警告が表示される場合があります。表示されたら「詳細情報」をクリックし、「実行」を選択すると起動できます。

> [!WARNING]
> 2 つの版は設定とクリップボード履歴を共有するので切り替えて使えますが、同時に起動しないでください。既定のホットキーが競合します。

> [!TIP]
> 設定は既定で `%APPDATA%\Jietuma\settings.ini` に保存されます。exe の隣に `settings.ini`（空のファイルでも可）を置くとそちらを使うので、プログラムのフォルダーをコピーすれば設定も持ち運べます。スクリーンショットの保存先、クリップボード履歴の場所、自動起動など、このパソコンでしか意味のない項目は引き続きレジストリに残ります。クリップボード履歴は既定で `%APPDATA%\Jietuma\clipboard` にあり、設定の「クリップボード」ページで場所を変えられます。

### 更新

「バージョン情報 → 更新の確認 → 更新して再起動」で手動更新できます。同じページの自動更新を有効にすると、このリポジトリの GitHub Releases を確認し、ウィンドウ・ピン・録画・書き出しなどが終了してから対応するインストーラーで更新します。設定、履歴、プラグインは保持されます。

<a id="ppocr-pack"></a>

### PP-OCR コンポーネント（軽量版）

軽量版には PP-OCR の前処理・後処理は含まれますが、ONNX Runtime とモデルは含まれません。設定の「PP-OCR モデルの管理」から取得できます。モデルは ModelScope から、ONNX Runtime は PyPI またはミラーで配布される Microsoft の `onnxruntime` wheel から取得し、内蔵カタログのサイズとハッシュで検証します。PP-OCRv6、PP-OCRv5、各種言語別のモデルを選ぶほか、ローカルの ONNX ファイルも追加できます。ダウンロード、追加、切り替えに再起動は不要です。

手動で exe と同じ階層の `models/` フォルダーに置くこともできます。`PP-OCRv6_det_small.onnx` と `PP-OCRv6_rec_small.onnx`（リポジトリの [models/](models/) にあります）、および ONNX Runtime 1.24 以上の CPU 専用 `onnxruntime.dll` です。

## コード署名ポリシー

Windows 版の `jietuma_pp.exe` と `jietuma_lite.exe` は、GitHub Actions が本リポジトリの [cpp/](cpp/) の C++ ソースからビルドします。リリース手順は SignPath によるコード署名に対応していますが、署名の有無はビルド設定によって異なります。各リリースの説明と EXE のデジタル署名をご確認ください。


- コミッター・レビュアー・承認者：[@adoin](https://github.com/adoin)
- 外部からの貢献は Pull Request で受け付け、メンテナーのレビュー後にのみマージします。

<a id="privacy"></a>

### プライバシー

キャプチャ、OCR、描画、録画、クリップボード履歴はローカルで処理・保存します。任意のネットワーク機能は設定したサービスに接続します。自動更新が有効な場合はこのリポジトリの GitHub Releases に自動接続します。プラグインは現在のユーザー権限で実行され、キャプチャ、クリップボード、ファイルにアクセスできます。

- 通常のキャプチャ、OCR 認識、クリップボード記録に内容のアップロードは不要です。PP-OCR の認識にはローカルのモデルと ONNX Runtime が必要です。
- 翻訳対象のテキストは、設定で選んだサービスに設定済みの認証情報を使って送信され、そのサービスのプライバシーポリシーが適用されます。独自の OpenAI 互換エンドポイントをこの PC 上のサービスに設定した場合は、そのローカルエンドポイントに送信します。
- **バージョン情報 → 更新を確認**で更新を確認・インストールすると、リリース情報の取得とパッケージのダウンロードのために GitHub に接続します。
- 「PP-OCR モデルの管理」でモデルや ONNX Runtime を取得する際は、ModelScope、PyPI、内蔵カタログのミラーに接続します。コンポーネントのダウンロード用であり、認識対象の画像は送信しません。
- 翻訳設定で接続テストやモデル一覧の取得を行うと、設定したサービスのエンドポイントに接続します。接続テストは固定のサンプル文を送り、モデル一覧の取得ではクリップボードの内容を送りません。

---

<a id="source-setup"></a>

## ソースからビルド

Visual Studio の「C++ によるデスクトップ開発」（CMake を含む）、Rust MSVC ツールチェーン、Qt 6.11 の MSVC SDK が必要です。PowerShell から実行します。

```powershell
git clone https://github.com/adoin/jietuma.git
cd jietuma/cpp
python -m venv .tools
.\.tools\Scripts\python.exe -m pip install py7zr==1.1.4
.\.tools\Scripts\python.exe scripts\install_qt.py   # Qt SDK を .sdk/ にダウンロード
.\build.cmd                                         # ビルド・テストし dist/Release に配置
```

Qt 6 の MSVC x64 SDK を持っている場合は `.\scripts\build.ps1 -QtRoot C:\Qt\6.11.0\msvc2022_64` を使います。`-SkipTests` でテストを省略、`-Configuration Debug` で Debug ビルドです。ビルド後は `run.cmd` で起動できます。

リリース用の単一ファイル版は Qt と MSVC ランタイムを静的リンクするため、この 2 つを別途インストールする必要はありません。OCR のモデルと ONNX Runtime は上記のコンポーネント説明をご参照ください。まず `.\scripts\build_static_qt.ps1` で Qt の公式ソースから静的 SDK をビルドし、続けて次を実行します。

```powershell
.\build.cmd -Static          # 軽量版：dist/Release-static/jietuma_lite.exe
.\build.cmd -Static -Full    # フル版：dist/Release-static-full/jietuma_pp.exe（ONNX Runtime を内蔵し、既定の PP-OCR モデルを同梱）
```

ARM64 版は x64 上でクロスコンパイルします。先に x64 の静的 Qt をビルドし、`.\scripts\build_static_qt.ps1 -Arch arm64` と `.\build.cmd -Static -Arch arm64` を実行します。Visual Studio の MSVC ARM64 ビルドツールと `rustup target add aarch64-pc-windows-msvc` が必要です。バージョン番号はリポジトリ直下の `VERSION` ファイルから取得します。

PP-OCR モデルはリポジトリの [models/](models/) にあります（`PP-OCRv6_det_small.onnx` と `PP-OCRv6_rec_small.onnx`）。Snipping Tool OCR はローカルの Windows 11 Snipping Tool のファイルを使い、初回使用時に `%LOCALAPPDATA%\Jietuma\oneocr` へ自動コピーするため、追加のダウンロードは不要です。

---

<a id="development"></a>

## 開発とテスト

`.\build.cmd` はビルドしてすべてのテストを実行します。名前が `_desktop` で終わるテストは実際のデスクトップでキーボードとマウスの入力を注入し、グローバルホットキーを登録し、画面をキャプチャするため、実行中はマウスとキーボードに触れないでください。デスクトップを必要としないテストだけを実行するには次のようにします。

```powershell
$env:PATH = "$PWD\.sdk\6.11.0\msvc2022_64\bin;$env:PATH"
ctest --test-dir build\msvc -C Release --output-on-failure -E "_desktop$|_windows_parity$"
```

テストプログラムは一時的なサンドボックスで動作し、お使いの設定、クリップボード履歴、ログを読み書きしません。ほかに分析ビルド（`scripts\check.ps1`：AddressSanitizer、コード分析、リーク検出）とカバレッジ（`scripts\coverage.ps1`）があります。

[C++ CI](.github/workflows/cpp.yml) はドキュメントのみのプルリクエストではビルドを省略し、それ以外ではビルド、オフスクリーンテストと行カバレッジの確認を行います。デスクトップテストは別に実行し、その失敗はこのチェックをブロックしません。`rust_libs/` の変更時は [Rust CI](.github/workflows/rust.yml) が Rust の単体テストとサードパーティライセンスの再確認を行います。

リリースは [build.yml](.github/workflows/build.yml) で行い、タグは `release-<VERSION ファイルのバージョン>` とします。[lite.yml](.github/workflows/lite.yml) が x64 と ARM64 の軽量版・完全版をビルドし、リリース処理が設定に応じて署名・パッケージ化して、`jietuma_lite-<tag>-<arch>.zip` と `jietuma_pp-<tag>-<arch>.zip` を作成します。

---

## ディレクトリ構造

```text
# プロジェクトルート
├── README.md / README_zh-CN.md / README_zh-TW.md / README_JA.md / README_KO.md   # 英語・簡体字・繁体字・日本語・韓国語のドキュメント
├── VERSION                  # バージョン番号（CMake とリリースワークフローが参照）
├── app.ico                  # アプリと更新機能のアイコン
├── licenses/                # 配布パッケージに同梱するサードパーティライセンス
│
├── cpp/                     # C++（Qt6）実装
│   ├── src/                 # ソース（<パッケージ>/<モジュール> 構成）
│   │   ├── annotation/      # 画面注釈 — 画面ごとのレイヤー、透過の切り替え、画面をまたぐ元に戻す
│   │   ├── barcode/         # QR コード・バーコード認識（zxing-cpp）
│   │   ├── canvas/          # キャンバス — 編集の中核、元に戻す、レイヤー
│   │   ├── capture/         # キャプチャ — スクリーンショット、ウィンドウ・要素の検出
│   │   ├── clipboard/       # クリップボード管理 — 履歴、グループ/クイック起動、インポート/エクスポート、検索
│   │   ├── core/            # コア — 起動、ログ、リソース、テーマ、国際化、ホットキー
│   │   ├── gif/             # GIF/MP4 録画、GIF 編集、再生、書き出し
│   │   ├── ocr/             # Snipping Tool OCR と PP-OCR
│   │   ├── pin/             # ピン留め — 最前面表示、編集、OCR、翻訳
│   │   ├── settings/        # 設定の一元管理
│   │   ├── stitch/          # 長いスクリーンショットの結合
│   │   ├── text_recognition/ # OCR 結果のレイアウト
│   │   ├── tools/           # 描画ツール — ペン、矩形、矢印、テキスト、モザイクなど
│   │   ├── translation/     # 複数プロバイダーの翻訳と読み上げ
│   │   ├── ui/              # UI コンポーネント、キャプチャ画面、ツールバー、設定画面
│   │   └── video/           # MP4 録画子プロセス
│   ├── native/              # rust_libs の C インターフェース（静的リンク）
│   ├── resources/           # 既定設定、UI 翻訳（XML と .qm）、ログ文言
│   ├── scripts/             # ビルド、Qt SDK の準備、翻訳のコンパイル
│   └── tests/               # QtTest のテストと照合データ
│
├── rust_libs/               # Rust ライブラリ（cpp/native 経由で C++ に静的リンク）
│   ├── gifrecorder/         # GIF フレームの保存・再生・合成エンコード
│   ├── longstitch/          # 長いスクリーンショット結合アルゴリズム
│   ├── pyclipboard/         # クリップボード操作
│   ├── ppocr_rust/          # PP-OCR (PaddleOCR) ONNX 認識エンジン
│   ├── hdrcapture/          # HDR デスクトップキャプチャ
│   ├── updater/             # メイン EXE だけを置き換える独立した更新機能
│   └── video_recorder/      # Media Foundation 録画と WASAPI システム音声の取得
│
├── models/                  # PP-OCR ONNX モデル（フル版に同梱）
│   ├── PP-OCRv6_det_small.onnx   # 文字検出モデル (DBNet)
│   └── PP-OCRv6_rec_small.onnx   # 文字認識モデル (CRNN/CTC)
│
└── svg/                     # SVG アイコン
```

## モジュール詳細

各モジュールの詳細は [docs/MODULES.md](docs/MODULES.md) をご覧ください。

---

## ライセンス

著作権と MIT ライセンスは [LICENSE](LICENSE) に、Qt などのサードパーティコンポーネントについては [THIRD-PARTY-NOTICES.md](cpp/THIRD-PARTY-NOTICES.md) に記載しています。
