#include "config.h"
#include "hub_log.h"
#include "wstr.h"

#define INI_PATH_FMT            L"%ls\\sihub.ini"
#define LOG_PATH_FMT            L"%ls\\sihub.log"
#define EXE_DIR_FILE_PATH_FMT   L"%ls\\%ls"
#define DEFAULT_TEMPLATE_PATH   L"sihub_templates.txt"

#define INI_SECTION_GENERAL     L"general"
#define INI_SECTION_PATHS       L"paths"
#define INI_SECTION_TARGET      L"target"
#define INI_KEY_LOG             L"log"
#define INI_KEY_TEMPLATES       L"templates"
#define INI_KEY_TRAY_ICON       L"tray_icon"
#define INI_KEY_WIN_PREFIX      L"win_prefix"
#define INI_KEY_LINUX_PREFIX    L"linux_prefix"
#define INI_KEY_URI             L"uri"
#define INI_KEY_CLIPBOARD       L"copy_to_clipboard"
#define INI_KEY_DRY_RUN         L"dry_run"

static const wchar_t default_win_prefix[] = L"Z:\\";
static const wchar_t default_linux_prefix[] = L"";
static const wchar_t default_uri_template[] = L"vscode://anthropic.claude-code/open?prompt=" URI_PROMPT_PLACEHOLDER;
static const wchar_t default_template_path[] = DEFAULT_TEMPLATE_PATH;
static const int default_log_enabled = 0;
static const int default_tray_icon = 1;
static const int default_copy_to_clipboard = 1;
static const int default_dry_run = 0;

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
	wchar_t *last_backslash;

	GetModuleFileNameW(NULL, g_config.exe_dir, MAX_PATH);
	last_backslash = wcsrchr(g_config.exe_dir, L'\\');
	if (last_backslash)
		*last_backslash = 0;

	wstr_format(g_config.ini_path, MAX_PATH, INI_PATH_FMT, g_config.exe_dir);
	wstr_format(g_config.log_path, MAX_PATH, LOG_PATH_FMT, g_config.exe_dir);
}

void config_load(void)
{
	const wchar_t *ini_path = g_config.ini_path;
	wchar_t template_setting[MAX_PATH];

	g_config.log_enabled = GetPrivateProfileIntW(INI_SECTION_GENERAL, INI_KEY_LOG, default_log_enabled, ini_path);
	g_config.tray_icon_enabled = GetPrivateProfileIntW(INI_SECTION_GENERAL, INI_KEY_TRAY_ICON, default_tray_icon, ini_path);
	g_config.copy_to_clipboard = GetPrivateProfileIntW(INI_SECTION_TARGET, INI_KEY_CLIPBOARD, default_copy_to_clipboard, ini_path);
	g_config.dry_run = GetPrivateProfileIntW(INI_SECTION_TARGET, INI_KEY_DRY_RUN, default_dry_run, ini_path);
	GetPrivateProfileStringW(INI_SECTION_PATHS, INI_KEY_WIN_PREFIX, default_win_prefix, g_config.win_prefix, MAX_PATH, ini_path);
	GetPrivateProfileStringW(INI_SECTION_PATHS, INI_KEY_LINUX_PREFIX, default_linux_prefix, g_config.linux_prefix, MAX_PATH, ini_path);
	GetPrivateProfileStringW(INI_SECTION_TARGET, INI_KEY_URI, default_uri_template, g_config.uri_template, URI_TEMPLATE_MAX, ini_path);
	GetPrivateProfileStringW(INI_SECTION_GENERAL, INI_KEY_TEMPLATES, default_template_path, template_setting, MAX_PATH, ini_path);

	resolve_template_path(template_setting);
	hub_log_configure(g_config.log_path, g_config.log_enabled);
}

const struct hub_config *config_get(void)
{
	return &g_config;
}
