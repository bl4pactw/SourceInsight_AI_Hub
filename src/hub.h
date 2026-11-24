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

#define HUB_VERSION         L"0.7.0"
#define HUB_TITLE           L"SI AI Hub"

#define WM_HUB_SHOWMENU     (WM_APP + 1)
#define WM_HUB_TRAY         (WM_APP + 2)
#define WM_HUB_SSH_EVENT    (WM_APP + 3)
#define WM_HUB_SSH_PROMPT   (WM_APP + 4)

#define HUB_JOB_TEST        1
#define HUB_JOB_TMUX_SEND   2
#define HUB_JOB_REPLY_CHECK 3
#define HUB_JOB_INSTALL_STEP 4
#define HUB_JOB_INSTALL_DONE 5
#define HUB_JOB_KIND_MASK   0xFF
#define HUB_JOB_SERIAL_SHIFT 8

#define REQ_FILE_MAX        1024
#define REQ_SYMBOL_MAX      256

enum hub_target
{
	HUB_TARGET_DEFAULT,
	HUB_TARGET_VSCODE,
	HUB_TARGET_TMUX,
};

struct hub_request
{
	wchar_t file[REQ_FILE_MAX];
	wchar_t symbol[REQ_SYMBOL_MAX];
	int first_line;
	int last_line;
	int auto_pick;
};

#endif
