#ifndef SIHUB_WINUTIL_H
#define SIHUB_WINUTIL_H

#include "hub.h"

#define SHELL_EXECUTE_ERROR_MAX     32

int winutil_shell_execute(const wchar_t *target);
void winutil_open_path(const wchar_t *path);
void winutil_show_error(const wchar_t *format, ...);
void winutil_show_info(const wchar_t *format, ...);
BOOL winutil_confirm(HWND owner, const wchar_t *format, ...);
BOOL winutil_prompt_text(HWND owner, const wchar_t *title, const wchar_t *message, BOOL is_password, wchar_t *text_inout, size_t text_chars);
BOOL winutil_set_clipboard_text(HWND owner_window, const wchar_t *text);

#endif
