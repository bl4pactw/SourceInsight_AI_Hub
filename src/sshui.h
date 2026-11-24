#ifndef SIHUB_SSHUI_H
#define SIHUB_SSHUI_H

#include "hub.h"
#include "sshconn.h"

#define SSHUI_NOTICE_TITLE_MAX  64

struct sshui_notice
{
	BOOL show;
	wchar_t title[SSHUI_NOTICE_TITLE_MAX];
	wchar_t text[SSHCONN_NAME_MAX];
};

BOOL sshui_prompt_active(void);
HMENU sshui_create_connections_menu(void);
BOOL sshui_run_command(HWND owner, int command_id);
void sshui_handle_prompt(HWND owner, struct sshconn_prompt *prompt);
void sshui_handle_event(struct sshconn_event *event, struct sshui_notice *notice_out);

#endif
