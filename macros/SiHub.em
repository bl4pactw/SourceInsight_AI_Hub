/*
 * SiHub.em - Source Insight 4.x macros for SI AI Hub (MVP)
 *
 * 安裝：
 *   1. 修改下方 SiHub_ExePath() 回傳的路徑
 *   2. Project > Add and Remove Project Files 加入本檔（或放進 Base 專案）
 *   3. Options > Key Assignments 搜尋 "SiHub_Ask"，綁定熱鍵（建議 Ctrl+Alt+A）
 *
 * sihub.exe 是 GUI 程式（無 console），RunCmdLine 不會閃出任何視窗。
 */

/* sihub.exe 的完整路徑（反斜線要寫兩次） */
macro SiHub_ExePath()
{
    return "C:\\Tools\\SiHub\\sihub.exe"
}

/* 把字串包上雙引號 */
macro SiHub_Quote(s)
{
    q = CharFromAscii(34)
    return Cat(Cat(q, s), q)
}

/* 主要入口：游標處彈出選單 -> 選範本 -> VS Code Claude 預填 prompt */
macro SiHub_Ask()
{
    hwnd = GetCurrentWnd()
    if (hwnd == hNil)
        stop

    hbuf = GetWndBuf(hwnd)
    sFile = GetBufName(hbuf)            /* e.g. Z:\sdk\drivers\foo.c */

    /* SI 行號從 0 起算，轉成 1 起算 */
    ln1 = GetWndSelLnFirst(hwnd) + 1
    ln2 = GetWndSelLnLast(hwnd) + 1

    sym = GetCurSymbol()
    if (sym == Nil)
        sym = ""

    sArgs = Cat(" --file ", SiHub_Quote(sFile))
    sArgs = Cat(sArgs, " --line @ln1@ --line2 @ln2@")
    sArgs = Cat(sArgs, Cat(" --symbol ", SiHub_Quote(sym)))

    sCmd = Cat(SiHub_Quote(SiHub_ExePath()), sArgs)

    /* fWait = 0：不等待，SI 立即返回 */
    RunCmdLine(sCmd, Nil, 0)
}

/* 結束常駐的 hub（改版 / 換 exe 時用） */
macro SiHub_Quit()
{
    sCmd = Cat(SiHub_Quote(SiHub_ExePath()), " --quit")
    RunCmdLine(sCmd, Nil, 0)
}
