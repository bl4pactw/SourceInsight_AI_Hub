#ifndef SIHUB_TARGET_H
#define SIHUB_TARGET_H

#include "hub.h"

BOOL target_send(HWND owner_window, const wchar_t *prompt, int *shell_code_out);

#endif
