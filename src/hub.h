#ifndef SIHUB_HUB_H
#define SIHUB_HUB_H

#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <wchar.h>

#define HUB_VERSION         L"0.5.0"
#define HUB_TITLE           L"SI AI Hub"

#define WM_HUB_SHOWMENU     (WM_APP + 1)
#define WM_HUB_TRAY         (WM_APP + 2)
#define WM_HUB_SSH_EVENT    (WM_APP + 3)
#define WM_HUB_SSH_PROMPT   (WM_APP + 4)

#define REQ_FILE_MAX        1024
#define REQ_SYMBOL_MAX      256

struct hub_request
{
	wchar_t file[REQ_FILE_MAX];
	wchar_t symbol[REQ_SYMBOL_MAX];
	int first_line;
	int last_line;
	int auto_pick;
};

#endif
