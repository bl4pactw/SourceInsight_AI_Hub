# SI AI Hub — v0.8

Source Insight 4 熱鍵 → 游標處彈出選單 → 選 3 個範本之一 → VS Code（Remote）裡的 Claude Code 輸入框出現預填 prompt。全程無 console 視窗。

```
SI4 macro ──RunCmdLine──> sihub.exe (GUI, 無 console)
                            ├─ 第一個實例：常駐 server（隱藏視窗）
                            └─ 之後的實例：WM_COPYDATA 轉給 server 後立即結束
server ─ 選單 @ SI 文字游標 ─> 範本 ─> Z:\ → Linux 路徑 ─> prompt
       ─> （剪貼簿）─> ShellExecute("vscode://anthropic.claude-code/open?prompt=…")
```

## 檔案

| 檔案 | 說明 |
|---|---|
| `dist/` | **部署用**：`sihub.exe`（x64、MinGW、static、GUI subsystem）、`WebView2Loader.dll`、範本、設定範例、`remote/`；整個資料夾內容複製到 `C:\Tools\SiHub\` 即可 |
| `src/` | 原始碼（模組化 Win32 C，純 ASCII，見下方「程式結構」） |
| `CLAUDE.md` | C 程式碼規範（Claude Code / Cowork 共用） |
| `res/` | 圖示、對話框、manifest、回覆檢視器頁面（`viewer.html`，編進 exe） |
| `remote/` | 部署到 Linux 的回覆 hook（`stop_hook.sh`、`last_reply.py`、`install.sh`），由 hub 經 SSH 上傳；**請和 sihub.exe 放在同一資料夾** |
| `third_party/libssh2/` | libssh2 1.11.1 原始碼（BSD-3-Clause，見其 `COPYING`），以 WinCNG 編譯，不需 OpenSSL |
| `third_party/cmark-gfm/` | cmark-gfm 0.29.0.gfm.13（BSD-2-Clause 等，見 `COPYING`），Markdown → HTML |
| `third_party/webview2/` | `WebView2Loader.dll`（x64/x86/arm64，Microsoft 簽章）與自產的最小 C 標頭 `webview2_c.h`；來源與 SHA256 見其 `README.txt` |
| `tools/gen_webview2_header.py` | 產生 `webview2_c.h` 的腳本 |
| `macros/SiHub.em` | SI4 巨集：`SiHub_Ask`、`SiHub_Quit` |
| `sihub.ini.example` | 設定範例（路徑對應、目標 URI、範本檔位置） |
| `sihub_templates.txt` | 選單範本（UTF-8，可增減，最多 9 個） |
| `CMakeLists.txt` / `cmake/` | CMake 建置（`build.bat`＝MSVC、`build_mingw.sh`＝MinGW 交叉編譯，兩者都輸出到 `dist/`） |

## 安裝（約 3 分鐘）

1. 建 `C:\Tools\SiHub\`，把 `dist\` 裡的所有檔案（`sihub.exe`、`WebView2Loader.dll`、`sihub_templates.txt`、`remote\`…）複製進去；複製 `sihub.ini.example` 為 `sihub.ini`，改 `[paths]`：
   ```ini
   win_prefix=Z:\
   linux_prefix=/home/bl4/<NFS 匯出的 Linux 目錄>
   ```
2. `macros/SiHub.em` 中的 `SiHub_ExePath()` 確認指向上述 exe；把 `.em` 加進 SI 專案（或 Base 專案）。
3. SI：Options → Key Assignments → `SiHub_Ask` → 綁 `Ctrl+Alt+A`。

## 驗收步驟

1. **先驗證 URI handler**（最關鍵的外部依賴）：VS Code 開著 Remote 視窗，`Win+R` 貼上
   `vscode://anthropic.claude-code/open?prompt=hello`
   → Claude Code 面板應開啟並預填 `hello`。若沒有，先更新 Claude Code 擴充套件。
2. **驗證設定（不開 VS Code）**：`sihub.ini` 設 `log=1`、`dry_run=1`，在 SI 按熱鍵選一項，看 `sihub.log` 的 `prompt=` 與 `uri=` 路徑是否正確。
3. **端到端**：`dry_run=0`，SI 選取幾行 → 熱鍵 → 選單出現在游標下方 → 按 `1`/`2`/`3` → VS Code 前景、Claude 輸入框已預填 → 確認後送出。Claude 會先回報第 N 行內容，用來確認原始碼樹一致。
4. 連按多次熱鍵：工作管理員中只應有 **一個** 常駐 `sihub.exe`。
5. 系統匣出現藍色 **SI** 圖示（Windows 11 可能收在 `^` 溢位區，可拖到工作列常駐）。
6. 結束常駐程式：系統匣圖示右鍵 → **Exit**，或執行 SI 巨集 `SiHub_Quit`（換新版 exe 前使用）。

## 範本（`sihub_templates.txt`）

範本與程式碼分離，存檔即生效（每次觸發重讀），不需重編、不需重啟 hub。

```text
[解釋這段程式碼]
請解釋 {symbol}（{path} 第 {line1}-{line2} 行）……

[追蹤定義 / 來源]
……
```

- `[標題]` 須從行首開始，依序對應選單 `&1`~`&9`；內容多行、原樣送出
- 內容最後不要以 `@路徑` 結尾：Claude 輸入框會開啟 `@` 檔案選單（顯示「No files found」）並攔截 Enter / Ctrl+Enter，導致無法直接送出
- `#` 註解只在第一個標題之前有效（範本內的 `#define` 不受影響）；內容行以 `[` 開頭請寫 `\[`
- 佔位符：`{path}` `{winpath}` `{file}` `{symbol}` `{line1}` `{line2}` `{line}`
- 預設 3 個範本都要求 Claude 先回報起始行內容，用來確認 Windows/Linux 兩端是同一份 code
- 範本檔讀不到或沒有任何 `[標題]` 時，使用 `sihub.c` 內建的英文備援範本，選單頂端會顯示灰色提示

## 送出目標：VS Code 或 tmux

| 目標 | 行為 | 設定 |
|---|---|---|
| `vscode` | 開 `vscode://` 預填到 VS Code Claude，需自己按送出 | `[target] uri` |
| `tmux` | 經 SSH 連線把 prompt 貼進遠端 tmux 裡的 Claude CLI，並按 Enter 送出 | `[tmux]` 區段 |

- 預設目標：`sihub.ini` 的 `[target] default=vscode|tmux`；範本可用 `[標題] !tmux` 個別指定。選單每一項右側會顯示目標
- tmux 使用前，在遠端（任何 bl4 的終端機）執行：`tmux new -A -s sihub claude`
- `[tmux] connection=bl4@192.168.4.70`：該連線不在清單會自動加入；未連線會自動連線，prompt 排隊等連上後送出
- **安全檢查**：送出前確認 tmux 視窗裡執行的是 `allowed_commands`（預設 `claude,node`）之一；若是 bash 等其他程式，拒絕送出，避免 prompt 被當成指令執行
- prompt 以 stdin 傳送（tmux `load-buffer` → `paste-buffer -p` bracketed paste），引號、`$`、反引號都不會被 shell 解讀；多行 prompt 不會提前送出
- `submit=0` 只貼上不送出；`submit_delay_ms` 調整貼上與 Enter 之間的等待
- 送出成功顯示系統匣通知；失敗（找不到 session、視窗不是 Claude、連線失敗）跳出原因

## 讀回 Claude 的回覆（tmux 目標）

送到 tmux 的 prompt，hub 會等 Claude 回覆完成後取回 Markdown 原文，存成 exe 旁的 `last_reply.md`，自動開啟[回覆檢視器](#回覆檢視器viewer)，並跳出通知「Claude replied」（點通知帶到前景；系統匣也有 **Open last reply**）。

**一次性安裝**：系統匣 → Connections → 連線 → **Install reply hook**，然後在遠端重新啟動 tmux 裡的 Claude（`exit` 後 `tmux new -A -s sihub claude`）。

運作方式：

1. Linux 端 Claude Code 的 **Stop hook**（`~/.sihub/stop_hook.sh`）在每次回覆完成時執行；只有在 tmux 內（有 `$TMUX_PANE`）才記錄，並依 tmux session 分檔寫 `~/.sihub/stop/<session>.json`，所以 VS Code 等其他 Claude 對話不會干擾
2. hub 送出前記下該檔時間戳，之後每 `reply_poll_ms` 輪詢；時間戳改變即代表回覆完成
3. `~/.sihub/last_reply.py` 從對話紀錄取出「最後一則使用者提問之後的所有回覆文字」（排除工具呼叫、工具結果、thinking）

- 安裝程式會備份 `~/.claude/settings.json`（`.sihub-backup-時間`），只新增 hook、不動其他設定；重複執行不會重複新增
- 超過 `reply_timeout_s` 沒等到會停止並通知；回覆仍可在 tmux 中查看
- 未安裝 hook 時照常送出，通知會提醒安裝
- 需要遠端有 `python3`

## 回覆檢視器（Viewer）

收到回覆時自動開啟檢視器視窗（不搶焦點，可設 `[viewer] auto_open=0` 關閉）；點通知或系統匣 **Open last reply** 會把它帶到前景。內容一律來自 `last_reply.md`，檔案照常保留。

| 操作 | 動作 |
|---|---|
| **檢視** / **原文** 切換（`Ctrl+M`） | 渲染後的 Markdown ↔ 帶行號的原始 Markdown；切換時保持捲動位置 |
| 檢視模式選取 → `Ctrl+C` | 複製**所選區塊對應的原始 Markdown 行**（表格只選幾列會自動補表頭）；提示「已複製第 X–Y 行」 |
| 原文模式選取 → `Ctrl+C` | 逐字複製所選原文 |
| **全部複製**（`Ctrl+Shift+C`） | 剪貼簿同時放 Markdown（純文字）與 HTML；貼到 Word/Outlook 有格式，貼到編輯器是 Markdown |
| 程式碼區塊右上角 **複製** | 只複製該區塊的程式碼 |
| 連結 | 以預設瀏覽器開啟（只允許 http/https） |
| `Esc` / 關閉鈕 | 隱藏視窗（下次開啟立即顯示）；位置與大小記在 `sihub.ini` 的 `[viewer] placement` |

- 需要 **Microsoft Edge WebView2 Runtime**（Windows 11 內建）與 exe 旁的 `WebView2Loader.dll`。缺少時退回用預設程式開啟 `last_reply.md`（原因寫在 log）。
- WebView2 使用者資料放在 `%LOCALAPPDATA%\SiHub\WebView2`。
- 安全：Markdown 以 cmark-gfm 安全模式轉換（不輸出原始 HTML、移除 `javascript:` 連結），頁面不載入任何外部資源。
- 自訂頁面：把修改過的 `viewer.html` 放在 exe 旁即優先使用（除錯可設 `[viewer] devtools=1` 開 F12）。
- 第一次使用前建議在檔案總管檢查 `WebView2Loader.dll` → 內容 → 數位簽章為 Microsoft Corporation。

## 建置

```
build.bat          :: MSVC（VS 2019+ 的 x64 Native Tools 命令提示字元，需 CMake）
sh build_mingw.sh  # Linux 上以 mingw-w64 交叉編譯（需 cmake、ninja）
```

兩者都把可部署檔案安裝到 `dist/`。

## SSH 連線（Connections）

系統匣右鍵 → **Connections** 管理常駐 SSH 連線（hub 內建 libssh2，不經 `ssh.exe`，不會閃視窗）。

| 操作 | 說明 |
|---|---|
| Connect to... | 輸入 `user@host` 或 `user@host:port`，加入清單並連線 |
| ●／○ 清單 | ● 已連線、◔ 連線中、○ 未連線或失敗；各項子選單有 Connect / Disconnect / Run test command / Remove |
| 密碼 | 需要時跳出密碼視窗（最多 3 次）；**密碼不存檔**，只在登入當下使用 |
| 主機金鑰 | 第一次連線顯示 SHA256 指紋請你確認，記在 exe 旁的 `known_hosts`（OpenSSH 格式）；之後金鑰變了會拒絕連線並警告 |
| 金鑰登入 | 自動嘗試 `%USERPROFILE%\.ssh\id_ecdsa`、`id_rsa`（無密語）；**Ed25519 金鑰不支援**（WinCNG 限制），會改用密碼 |
| Run test command | 在遠端執行 `uname -a`、顯示 shell 與 tmux 版本，確認連線可用 |

- 連線清單存在 `sihub.ini` 的 `[connections]`（`conn1=bl4@192.168.4.70`），hub 重啟後清單還在，但需重新連線
- 連線保持到 Disconnect 或 hub 結束；每 30 秒送 keepalive，斷線時跳出提示
- 需要 Windows 10 以上（WinCNG 的 ECDSA / ECDH）

## 系統匣圖示

| 操作 | 動作 |
|---|---|
| 雙擊 | 開啟範本檔 |
| 右鍵 → Edit templates | 開啟 `sihub_templates.txt` |
| 右鍵 → Edit settings | 開啟 `sihub.ini` |
| 右鍵 → Open log | 開啟 `sihub.log`（`log=1` 時才可用） |
| 右鍵 → Open folder | 開啟 exe 所在資料夾 |
| 右鍵 → Open last reply | 開啟回覆檢視器 |
| 右鍵 → Connections | SSH 連線管理（見上節） |
| 右鍵 → Exit | 結束常駐 hub（會先中斷所有 SSH 連線） |

Explorer 重新啟動後圖示會自動補回；不想顯示可設 `[general] tray_icon=0`（改設定後需重啟 hub）。

## 程式結構

依賴只往下，沒有循環；每個模組只透過 `xxx.h` 公開函式，狀態為模組內 `static g_*`。

```
main.c ── 命令列解析、決定 client / server
  ├─ ipc.c        單一實例 mutex、WM_COPYDATA 送/收（client ↔ server 契約）
  └─ wingui.c     隱藏視窗、message loop、游標處範本選單、請求流程協調
       ├─ tray.c       系統匣圖示與右鍵選單
       ├─ preprompt.c  範本檔解析、Z:\ → Linux 路徑、佔位符替換 → prompt（不碰 UI）
       ├─ target.c     prompt → 剪貼簿，再送到 VS Code（URI）或 tmux（經 sshconn）
       │    └─ tmuxcmd.c   產生遠端 tmux 送出 / 回覆檢查腳本並驗證、解析結果（不含 Win32 型別，可在 Linux 測試）
       ├─ reply.c      回覆等待：計時輪詢、逾時、寫 last_reply.md
       │    └─ viewer.c    WebView2 檢視器視窗（COM 回呼以 C 實作、動態載入 loader、頁面 ↔ C 以 web message 溝通）
       │         └─ mdrender.c  cmark-gfm 包裝：Markdown → 帶 data-sourcepos 的 HTML
       └─ winutil.c    MessageBox、ShellExecute、剪貼簿
       ├─ sshui.c      連線清單選單、密碼 / 主機金鑰對話框、結果顯示
       │    └─ sshconn.c   連線管理：每條連線一個 worker thread、工作佇列、清單存 ini
       │         └─ sshclient.c  libssh2 同步封裝：連線、known_hosts、登入、執行指令（不含 Win32 型別，可在 Linux 測試）
共用：config.c（sihub.ini）、hub_log.c、wstr.c（wide_buf / UTF-8 / 字串工具）、hub.h（struct hub_request、版本）
```

| 要改的東西 | 動哪個模組 |
|---|---|
| 新增 / 調整 prompt 前處理（例如帶入選取文字、SDK 資訊） | `preprompt.c` |
| 新的送出目標（其他 AI app、MCP） | `target.c` |
| 選單、系統匣、視窗行為 | `wingui.c` / `tray.c` |
| 新設定項 | `config.h` / `config.c` |
| 遠端操作（例如 tmux 送 prompt） | 透過 `sshconn_exec_async()`，不直接碰 libssh2 |

## 設計重點

- **不閃視窗**：exe 為 `/SUBSYSTEM:WINDOWS`，SI 用 `RunCmdLine(..., fWait=0)` 啟動；開 VS Code 走 `ShellExecuteW` 開 URI，不經 `cmd`/`code.cmd`。
- **單一實例**：named mutex + `FindWindow`；client 以 `AllowSetForegroundWindow` 把前景權交給 server，選單才能正常取得焦點。
- **選單位置**：`GetGUIThreadInfo` 取 SI 的文字游標（caret），取不到則用滑鼠位置。按 Esc／點外面取消後焦點回到 SI。
- **剪貼簿備援**：`copy_to_clipboard=1` 時 prompt 同時進剪貼簿，URI 預填失效時可直接 Ctrl+V。
- **設定熱更新**：每次觸發重讀 `sihub.ini` 與範本檔。
- **原始碼純 ASCII**：中文只存在範本檔（UTF-8，程式以 `MultiByteToWideChar(CP_UTF8)` 讀入），避免編譯器 / code page 編碼問題。

## 已知限制 / 下一步

- URI handler 為 Claude Code 擴充套件的功能，未在本機驗證，以驗收步驟 1 為準；URI 一律開在「最後作用中」的 VS Code 視窗。
- 僅支援單一路徑對應；多個 SDK 樹需擴充 `[paths]`。
- URI 長度約 1 KB（中文經 UTF-8 編碼後膨脹），範本勿過長；未來可改用 MCP/本機通道傳遞選取內容。
- 尚無範本編輯 UI、其他 AI 目標。
