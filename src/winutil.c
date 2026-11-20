#include "winutil.h"
#include <stdarg.h>
#include <string.h>
#include <shellapi.h>
#include "hub_log.h"
#include "wstr.h"

#define ERROR_TEXT_MAX          600
#define CLIPBOARD_RETRIES       5
#define CLIPBOARD_DELAY_MS      20

int winutil_shell_execute(const wchar_t *target)
{
	return (int)(INT_PTR)ShellExecuteW(NULL, L"open", target, NULL, NULL, SW_SHOWNORMAL);
}

void winutil_show_error(const wchar_t *format, ...)
{
	wchar_t error_text[ERROR_TEXT_MAX];
	va_list args;

	va_start(args, format);
	wstr_vformat(error_text, ERROR_TEXT_MAX, format, args);
	va_end(args);

	MessageBoxW(NULL, error_text, HUB_TITLE, MB_ICONWARNING | MB_SETFOREGROUND);
}

void winutil_open_path(const wchar_t *path)
{
	int shell_code = winutil_shell_execute(path);

	if (shell_code <= SHELL_EXECUTE_ERROR_MAX)
		winutil_show_error(L"Cannot open:\n%ls\n\n(ShellExecute=%d)", path, shell_code);
}

static BOOL write_clipboard(const wchar_t *text)
{
	size_t byte_count = (wcslen(text) + 1) * sizeof(wchar_t);
	HGLOBAL memory_handle;

	memory_handle = GlobalAlloc(GMEM_MOVEABLE, byte_count);
	if (!memory_handle)
		return FALSE;

	memcpy(GlobalLock(memory_handle), text, byte_count);
	GlobalUnlock(memory_handle);

	if (!SetClipboardData(CF_UNICODETEXT, memory_handle)) {
		GlobalFree(memory_handle);
		return FALSE;
	}

	return TRUE;
}

BOOL winutil_set_clipboard_text(HWND owner_window, const wchar_t *text)
{
	BOOL written;
	int attempt;

	for (attempt = 0; attempt < CLIPBOARD_RETRIES; attempt++) {
		if (OpenClipboard(owner_window)) {
			EmptyClipboard();
			written = write_clipboard(text);
			CloseClipboard();
			return written;
		}

		Sleep(CLIPBOARD_DELAY_MS);
	}

	hub_log(L"WARN: OpenClipboard failed (%lu)", GetLastError());

	return FALSE;
}
