#include "winutil.h"
#include <stdarg.h>
#include <string.h>
#include <shellapi.h>
#include "hub_log.h"
#include "wstr.h"
#include "../res/resource.h"

#define MESSAGE_TEXT_MAX          10240
#define PASSWORD_MASK_CHAR      L'\x25CF'
#define CLIPBOARD_RETRIES       5
#define CLIPBOARD_DELAY_MS      20

int winutil_shell_execute(const wchar_t *target)
{
	return (int)(INT_PTR)ShellExecuteW(NULL, L"open", target, NULL, NULL, SW_SHOWNORMAL);
}

static int show_message(HWND owner, UINT type, const wchar_t *format, va_list args)
{
	wchar_t message_text[MESSAGE_TEXT_MAX];

	wstr_vformat(message_text, MESSAGE_TEXT_MAX, format, args);

	return MessageBoxW(owner, message_text, HUB_TITLE, type | MB_SETFOREGROUND | MB_TOPMOST);
}

void winutil_show_error(const wchar_t *format, ...)
{
	va_list args;

	va_start(args, format);
	show_message(NULL, MB_ICONWARNING | MB_OK, format, args);
	va_end(args);
}

void winutil_show_info(const wchar_t *format, ...)
{
	va_list args;

	va_start(args, format);
	show_message(NULL, MB_ICONINFORMATION | MB_OK, format, args);
	va_end(args);
}

BOOL winutil_confirm(HWND owner, const wchar_t *format, ...)
{
	va_list args;
	int answer;

	va_start(args, format);
	answer = show_message(owner, MB_ICONQUESTION | MB_YESNO | MB_DEFBUTTON2, format, args);
	va_end(args);

	return answer == IDYES;
}

void winutil_open_path(const wchar_t *path)
{
	int shell_code = winutil_shell_execute(path);

	if (shell_code <= SHELL_EXECUTE_ERROR_MAX)
		winutil_show_error(L"Cannot open:\n%ls\n\n(ShellExecute=%d)", path, shell_code);
}

struct prompt_dialog
{
	const wchar_t *title;
	const wchar_t *message;
	BOOL is_password;
	wchar_t *text;
	size_t text_chars;
};

static void init_prompt_dialog(HWND dialog, struct prompt_dialog *state)
{
	SetWindowLongPtrW(dialog, DWLP_USER, (LONG_PTR)state);
	SetWindowTextW(dialog, state->title);
	SetDlgItemTextW(dialog, IDC_PROMPT_MESSAGE, state->message);
	SetDlgItemTextW(dialog, IDC_PROMPT_EDIT, state->text);
	if (state->is_password)
		SendDlgItemMessageW(dialog, IDC_PROMPT_EDIT, EM_SETPASSWORDCHAR, PASSWORD_MASK_CHAR, 0);

	SetWindowPos(dialog, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
	SetForegroundWindow(dialog);
	SetFocus(GetDlgItem(dialog, IDC_PROMPT_EDIT));
}

static INT_PTR CALLBACK prompt_dialog_proc(HWND dialog, UINT message, WPARAM w_param, LPARAM l_param)
{
	struct prompt_dialog *state = (struct prompt_dialog *)GetWindowLongPtrW(dialog, DWLP_USER);

	switch (message) {
	case WM_INITDIALOG:
		init_prompt_dialog(dialog, (struct prompt_dialog *)l_param);
		return FALSE;

	case WM_COMMAND:
		if (LOWORD(w_param) != IDOK && LOWORD(w_param) != IDCANCEL)
			break;

		if (LOWORD(w_param) == IDOK)
			GetDlgItemTextW(dialog, IDC_PROMPT_EDIT, state->text, (int)state->text_chars);

		SetDlgItemTextW(dialog, IDC_PROMPT_EDIT, L"");
		EndDialog(dialog, LOWORD(w_param));
		return TRUE;

	default:
		break;
	}

	return FALSE;
}

BOOL winutil_prompt_text(HWND owner, const wchar_t *title, const wchar_t *message, BOOL is_password, wchar_t *text_inout, size_t text_chars)
{
	struct prompt_dialog state;

	state.title = title;
	state.message = message;
	state.is_password = is_password;
	state.text = text_inout;
	state.text_chars = text_chars;

	return DialogBoxParamW(GetModuleHandleW(NULL), MAKEINTRESOURCEW(IDD_PROMPT), owner, prompt_dialog_proc, (LPARAM)&state) == IDOK;
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
