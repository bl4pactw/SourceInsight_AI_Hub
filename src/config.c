#include "config.h"
#include "hub_log.h"
#include "wstr.h"

#define INI_PATH_FMT            L"%ls\\sihub.ini"
#define LOG_PATH_FMT            L"%ls\\sihub.log"
#define KNOWN_HOSTS_PATH_FMT    L"%ls\\known_hosts"
#define REPLY_PATH_FMT          L"%ls\\last_reply.md"
#define REMOTE_DIR_PATH_FMT     L"%ls\\remote"
#define SSH_KEY_DIR_PATH_FMT    L"%ls\\.ssh"
#define USER_PROFILE_ENV        L"USERPROFILE"
#define EXE_DIR_FILE_PATH_FMT   L"%ls\\%ls"
#define DEFAULT_TEMPLATE_PATH   L"sihub_templates.txt"

#define INI_SECTION_GENERAL     L"general"
#define INI_SECTION_PATHS       L"paths"
#define INI_SECTION_TARGET      L"target"
#define INI_SECTION_TMUX        L"tmux"
#define INI_SECTION_VIEWER      L"viewer"
#define TARGET_NAME_MAX         16
#define INI_KEY_LOG             L"log"
#define INI_KEY_TEMPLATES       L"templates"
#define INI_KEY_TRAY_ICON       L"tray_icon"
#define INI_KEY_WIN_PREFIX      L"win_prefix"
#define INI_KEY_LINUX_PREFIX    L"linux_prefix"
#define INI_KEY_URI             L"uri"
#define INI_KEY_CLIPBOARD       L"copy_to_clipboard"
#define INI_KEY_DRY_RUN         L"dry_run"
#define INI_KEY_DEFAULT_TARGET  L"default"
#define INI_KEY_TMUX_CONNECTION L"connection"
#define INI_KEY_TMUX_SESSION    L"session"
#define INI_KEY_TMUX_SUBMIT     L"submit"
#define INI_KEY_TMUX_DELAY      L"submit_delay_ms"
#define INI_KEY_TMUX_COMMANDS   L"allowed_commands"
#define INI_KEY_TMUX_READ_REPLY L"read_reply"
#define INI_KEY_REPLY_TIMEOUT   L"reply_timeout_s"
#define INI_KEY_REPLY_POLL      L"reply_poll_ms"
#define INI_KEY_VIEWER_AUTO     L"auto_open"
#define INI_KEY_VIEWER_DEVTOOLS L"devtools"

static const wchar_t default_win_prefix[] = L"Z:\\";
static const wchar_t default_linux_prefix[] = L"";
static const wchar_t default_uri_template[] = L"vscode://anthropic.claude-code/open?prompt=" URI_PROMPT_PLACEHOLDER;
static const wchar_t default_template_path[] = DEFAULT_TEMPLATE_PATH;
static const int default_log_enabled = 0;
static const int default_tray_icon = 1;
static const int default_copy_to_clipboard = 1;
static const int default_dry_run = 0;
static const wchar_t default_target_name[] = L"vscode";
static const wchar_t default_tmux_connection[] = L"";
static const wchar_t default_tmux_session[] = L"sihub";
static const wchar_t default_tmux_allowed_commands[] = L"claude,node";
static const int default_tmux_submit = 1;
static const int default_tmux_submit_delay_ms = 300;
static const int default_tmux_read_reply = 1;
static const int default_reply_timeout_s = 900;
static const int default_reply_poll_ms = 2000;
static const int default_viewer_auto_open = 1;
static const int default_viewer_devtools = 0;

static const struct
{
	const wchar_t *name;
	enum hub_target target;
} target_name_tbl[] =
{
	{
		.name = L"vscode",
		.target = HUB_TARGET_VSCODE,
	},
	{
		.name = L"tmux",
		.target = HUB_TARGET_TMUX,
	},
};

static struct hub_config g_config;

static BOOL is_absolute_path(const wchar_t *path)
{
	return (path[0] && path[1] == L':') || (path[0] == L'\\' && path[1] == L'\\');
}

static void resolve_template_path(const wchar_t *template_setting)
{

	if (is_absolute_path(template_setting))
		wstr_copy(g_config.template_path, MAX_PATH, template_setting);
	else
		wstr_format(g_config.template_path, MAX_PATH, EXE_DIR_FILE_PATH_FMT, g_config.exe_dir, template_setting);
}

void config_init(void)
{
	wchar_t user_profile[MAX_PATH];
	wchar_t *last_backslash;

	GetModuleFileNameW(NULL, g_config.exe_dir, MAX_PATH);
	last_backslash = wcsrchr(g_config.exe_dir, L'\\');
	if (last_backslash)
		*last_backslash = 0;

	wstr_format(g_config.ini_path, MAX_PATH, INI_PATH_FMT, g_config.exe_dir);
	wstr_format(g_config.log_path, MAX_PATH, LOG_PATH_FMT, g_config.exe_dir);
	wstr_format(g_config.known_hosts_path, MAX_PATH, KNOWN_HOSTS_PATH_FMT, g_config.exe_dir);
	wstr_format(g_config.reply_path, MAX_PATH, REPLY_PATH_FMT, g_config.exe_dir);
	wstr_format(g_config.remote_dir, MAX_PATH, REMOTE_DIR_PATH_FMT, g_config.exe_dir);
	if (GetEnvironmentVariableW(USER_PROFILE_ENV, user_profile, MAX_PATH))
		wstr_format(g_config.ssh_key_dir, MAX_PATH, SSH_KEY_DIR_PATH_FMT, user_profile);
}

void config_load(void)
{
	const wchar_t *ini_path = g_config.ini_path;
	wchar_t template_setting[MAX_PATH];
	wchar_t target_name[TARGET_NAME_MAX];

	g_config.log_enabled = GetPrivateProfileIntW(INI_SECTION_GENERAL, INI_KEY_LOG, default_log_enabled, ini_path);
	g_config.tray_icon_enabled = GetPrivateProfileIntW(INI_SECTION_GENERAL, INI_KEY_TRAY_ICON, default_tray_icon, ini_path);
	g_config.copy_to_clipboard = GetPrivateProfileIntW(INI_SECTION_TARGET, INI_KEY_CLIPBOARD, default_copy_to_clipboard, ini_path);
	g_config.dry_run = GetPrivateProfileIntW(INI_SECTION_TARGET, INI_KEY_DRY_RUN, default_dry_run, ini_path);
	GetPrivateProfileStringW(INI_SECTION_PATHS, INI_KEY_WIN_PREFIX, default_win_prefix, g_config.win_prefix, MAX_PATH, ini_path);
	GetPrivateProfileStringW(INI_SECTION_PATHS, INI_KEY_LINUX_PREFIX, default_linux_prefix, g_config.linux_prefix, MAX_PATH, ini_path);
	GetPrivateProfileStringW(INI_SECTION_TARGET, INI_KEY_URI, default_uri_template, g_config.uri_template, URI_TEMPLATE_MAX, ini_path);
	GetPrivateProfileStringW(INI_SECTION_GENERAL, INI_KEY_TEMPLATES, default_template_path, template_setting, MAX_PATH, ini_path);

	GetPrivateProfileStringW(INI_SECTION_TARGET, INI_KEY_DEFAULT_TARGET, default_target_name, target_name, TARGET_NAME_MAX, ini_path);
	GetPrivateProfileStringW(INI_SECTION_TMUX, INI_KEY_TMUX_CONNECTION, default_tmux_connection, g_config.tmux_connection, TMUX_CONNECTION_MAX, ini_path);
	GetPrivateProfileStringW(INI_SECTION_TMUX, INI_KEY_TMUX_SESSION, default_tmux_session, g_config.tmux_session, TMUX_SESSION_MAX, ini_path);
	GetPrivateProfileStringW(INI_SECTION_TMUX, INI_KEY_TMUX_COMMANDS, default_tmux_allowed_commands, g_config.tmux_allowed_commands, TMUX_COMMANDS_MAX, ini_path);
	g_config.tmux_submit = GetPrivateProfileIntW(INI_SECTION_TMUX, INI_KEY_TMUX_SUBMIT, default_tmux_submit, ini_path);
	g_config.tmux_submit_delay_ms = GetPrivateProfileIntW(INI_SECTION_TMUX, INI_KEY_TMUX_DELAY, default_tmux_submit_delay_ms, ini_path);
	g_config.tmux_read_reply = GetPrivateProfileIntW(INI_SECTION_TMUX, INI_KEY_TMUX_READ_REPLY, default_tmux_read_reply, ini_path);
	g_config.reply_timeout_s = GetPrivateProfileIntW(INI_SECTION_TMUX, INI_KEY_REPLY_TIMEOUT, default_reply_timeout_s, ini_path);
	g_config.reply_poll_ms = GetPrivateProfileIntW(INI_SECTION_TMUX, INI_KEY_REPLY_POLL, default_reply_poll_ms, ini_path);
	g_config.viewer_auto_open = GetPrivateProfileIntW(INI_SECTION_VIEWER, INI_KEY_VIEWER_AUTO, default_viewer_auto_open, ini_path);
	g_config.viewer_devtools = GetPrivateProfileIntW(INI_SECTION_VIEWER, INI_KEY_VIEWER_DEVTOOLS, default_viewer_devtools, ini_path);
	g_config.default_target = config_parse_target(target_name);
	if (g_config.default_target == HUB_TARGET_DEFAULT)
		g_config.default_target = HUB_TARGET_VSCODE;

	resolve_template_path(template_setting);
	hub_log_configure(g_config.log_path, g_config.log_enabled);
}

const struct hub_config *config_get(void)
{
	return &g_config;
}

enum hub_target config_parse_target(const wchar_t *name)
{
	size_t index;

	for (index = 0; index < ARRAYSIZE(target_name_tbl); index++) {
		if (!_wcsicmp(name, target_name_tbl[index].name))
			return target_name_tbl[index].target;
	}

	return HUB_TARGET_DEFAULT;
}
