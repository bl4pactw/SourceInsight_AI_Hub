#ifndef SIHUB_TARGET_H
#define SIHUB_TARGET_H

#include "hub.h"

#define TARGET_ERROR_MAX    512

enum hub_target target_resolve(enum hub_target requested);
const wchar_t *target_name(enum hub_target target);
BOOL target_send(HWND owner_window, enum hub_target target, const wchar_t *prompt, wchar_t *error_out, size_t error_chars);

#endif
