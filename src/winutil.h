#ifndef SIHUB_WINUTIL_H
#define SIHUB_WINUTIL_H

#include "hub.h"

#define SHELL_EXECUTE_ERROR_MAX     32

int winutil_shell_execute(const wchar_t *target);
void winutil_open_path(const wchar_t *path);
void winutil_show_error(const wchar_t *format, ...);
BOOL winutil_set_clipboard_text(HWND owner_window, const wchar_t *text);

#endif
