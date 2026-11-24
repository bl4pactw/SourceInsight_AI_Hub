#ifndef SIHUB_TRAY_H
#define SIHUB_TRAY_H

#include "hub.h"

void tray_init(HWND hub_window);
void tray_remove(HWND hub_window);
BOOL tray_handle_message(HWND hub_window, UINT message, LPARAM l_param);
void tray_show_balloon(HWND hub_window, const wchar_t *title, const wchar_t *text, BOOL opens_reply);

#endif
