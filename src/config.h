#ifndef SIHUB_CONFIG_H
#define SIHUB_CONFIG_H

#include "hub.h"

#define URI_TEMPLATE_MAX        512
#define URI_PROMPT_PLACEHOLDER  L"{prompt}"

struct hub_config
{
	wchar_t exe_dir[MAX_PATH];
	wchar_t ini_path[MAX_PATH];
	wchar_t log_path[MAX_PATH];
	wchar_t template_path[MAX_PATH];
	wchar_t known_hosts_path[MAX_PATH];
	wchar_t ssh_key_dir[MAX_PATH];
	wchar_t win_prefix[MAX_PATH];
	wchar_t linux_prefix[MAX_PATH];
	wchar_t uri_template[URI_TEMPLATE_MAX];
	BOOL log_enabled;
	BOOL tray_icon_enabled;
	BOOL copy_to_clipboard;
	BOOL dry_run;
};

void config_init(void);
void config_load(void);
const struct hub_config *config_get(void);

#endif
