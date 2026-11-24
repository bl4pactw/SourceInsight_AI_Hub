#ifndef SIHUB_CONFIG_H
#define SIHUB_CONFIG_H

#include "hub.h"

#define URI_TEMPLATE_MAX        512
#define URI_PROMPT_PLACEHOLDER  L"{prompt}"
#define TMUX_CONNECTION_MAX     320
#define TMUX_SESSION_MAX        64
#define TMUX_COMMANDS_MAX       128

struct hub_config
{
	wchar_t exe_dir[MAX_PATH];
	wchar_t ini_path[MAX_PATH];
	wchar_t log_path[MAX_PATH];
	wchar_t template_path[MAX_PATH];
	wchar_t known_hosts_path[MAX_PATH];
	wchar_t reply_path[MAX_PATH];
	wchar_t remote_dir[MAX_PATH];
	wchar_t ssh_key_dir[MAX_PATH];
	wchar_t win_prefix[MAX_PATH];
	wchar_t linux_prefix[MAX_PATH];
	wchar_t uri_template[URI_TEMPLATE_MAX];
	BOOL log_enabled;
	BOOL tray_icon_enabled;
	BOOL copy_to_clipboard;
	BOOL dry_run;
	enum hub_target default_target;
	wchar_t tmux_connection[TMUX_CONNECTION_MAX];
	wchar_t tmux_session[TMUX_SESSION_MAX];
	wchar_t tmux_allowed_commands[TMUX_COMMANDS_MAX];
	BOOL tmux_submit;
	int tmux_submit_delay_ms;
	BOOL tmux_read_reply;
	int reply_timeout_s;
	int reply_poll_ms;
};

void config_init(void);
void config_load(void);
const struct hub_config *config_get(void);
enum hub_target config_parse_target(const wchar_t *name);

#endif
