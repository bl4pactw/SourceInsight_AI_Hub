# C 程式碼規範

> 適用：在任何專案生成或修改 C 程式碼時。
> 來源：使用者手動修改 ipa_mon.c 後歸納並逐條確認（2026-11-13 ~ 11-16）。
> 原因：使用者會逐行閱讀生成的程式碼，這套排版與命名是長期習慣，直接影響閱讀速度與舒適度。

## 基底

Linux kernel 風格：tab 縮排、`snake_case`、函式的 `{` 獨立一行、`if`/`for`/`while`/`switch` 的 `{` 與條件同一行、只有一行的 if 不加大括號。以下為與 kernel 風格不同或額外的規則。

## 排版

1. **struct / enum 定義與初始化的 `{` 獨立一行**，包含函式內的匿名 struct。

   ```c
   struct iface
   {
   	char name[IFNAMSIZ];
   };

   static const char *const teth_state_str_tbl[] =
   {
   	[TETH_OFF] = "off",
   };

   } g_mon_ctx =
   {
   	.fast_hz = 10,
   	.cpu = -1,
   };
   ```

2. **enum 成員、designated initializer 一行一個**，不要寫成 `.a = 1, .b = 2,`。
3. **`else` 不接在 `}` 後面**：`}` 換行後寫 `else if (...) {` 或 `else {`。
4. **空行節奏**：
   - 區域變數宣告之後空一行。
   - `if`/`for`/`while`/`do` 區塊前後如果還有其他敘述，各空一行。只有一行的 if（例如 `if (...) continue;`）後面也空一行再接下一句。
   - 前面有其他敘述時，最後的 `return` 前空一行。
   - `{` 後的第一個東西是另一個控制敘述或註解時，`{` 後先空一行。
5. **簡短的 `case` 可以寫成一行**：`case 'f': x = atoi(optarg); break;`。多個敘述的 case 內部照第 4 點空行。
6. **一行只宣告一個變數**：不寫 `int a = 0, b, i;` 或 `char *p = buf, *end = buf + n;`。
7. **條件式不拆行**：`if (a && b)` 再長也寫在同一行。
8. **不輸出行尾空白**。
9. **不要重新對齊使用者原本寫好的行**（例如既有 `#define` 的欄位）；新加的 `#define` 群組可以用空行隔開，自成一組對齊。

## 路徑

10. **路徑一律用 `#define`，放在檔案開頭**，名字以 `_PATH` 結尾，程式裡不直接寫路徑字串。
11. **路徑中有執行時才決定的部分時**，定義完整的格式字串，名字以 `_PATH_FMT` 結尾，不要拆成基底目錄再用 `snprintf` 拼接。

    ```c
    #define IOSS_STATS_PATH_FMT    "/sys/class/net/%s/ioss/statistics"

    snprintf(path, sizeof(path), IOSS_STATS_PATH_FMT, ifname);
    ```

## 命名

12. **不用看不出意思的縮寫**：不用 `g`、`jb`、`jb_*` 這類一兩個字母的名字或前綴。
13. **檔案層級（全域 / `static`）變數用 `g_` 加上有意義的名字**，例如 `g_stop`、`g_mon_ctx`、`g_json_out`、`g_read_buf`。函式名稱不加 `g_`。
14. **唯讀的 `static const` 對照表不加 `g_`，名字以 `_tbl` 結尾**，例如 `teth_state_str_tbl`。陣列不用 `_lst`、`_list`，那會讓人以為是 linked list。
15. **函式內匿名 struct 的變數名加底線前綴**，例如 `struct { ... } _req;`。

## 註解

16. **生成 C 程式碼時不加任何註解**（這是使用者明確的 "no comments" 指示）：包括檔頭說明、函式說明、行尾註解。
17. **使用者寫的註解是和 Claude 溝通程式碼的管道**：要把它當成需求或說明來讀。
    - 註解裡的需求或說明，在程式碼已經做到、或雙方已經達成共識之後，就可以移除該註解；移除時要在回覆裡列出移除了哪些。
    - 還沒做到、還在討論中的註解，不要刪除、改寫或「整理」。
