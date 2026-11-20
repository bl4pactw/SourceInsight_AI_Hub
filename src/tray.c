#include "tray.h"
#include <shellapi.h>
#include "config.h"
#include "hub_log.h"
#include "winutil.h"
#include "wstr.h"
#include "../res/resource.h"

#define WM_HUB_TRAY             (WM_APP + 2)
#define TRAY_ICON_ID            1
#define TRAY_TIP_MAX            128
#define TASKBAR_CREATED_MSG     L"TaskbarCreated"

#define TRAY_ID_TITLE           200
#define TRAY_ID_EDIT_TEMPLATES  201
#define TRAY_ID_EDIT_SETTINGS   202
#define TRAY_ID_OPEN_LOG        203
#define TRAY_ID_OPEN_FOLDER     204
#define TRAY_ID_EXIT            205

static UINT g_taskbar_created_msg;
static BOOL g_tray_icon_added;

static void fill_tray_data(HWND hub_window, NOTIFYICONDATAW *tray_data)
{
	ZeroMemory(tray_data, sizeof(*tray_data));
	tray_data->cbSize = sizeof(*tray_data);
	tray_data->hWnd = hub_window;
	tray_data->uID = TRAY_ICON_ID;
}

static HICON load_tray_icon(void)
{
	HICON icon;

	icon = (HICON)LoadImageW(GetModuleHandleW(NULL), MAKEINTRESOURCEW(IDI_SIHUB), IMAGE_ICON, GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON), LR_DEFAULTCOLOR);
	if (!icon)
		icon = LoadIconW(NULL, (LPCWSTR)IDI_APPLICATION);

	return icon;
}

static void add_tray_icon(HWND hub_window)
{
	NOTIFYICONDATAW tray_data;

	if (!config_get()->tray_icon_enabled || g_tray_icon_added)
		return;

	fill_tray_data(hub_window, &tray_data);
	tray_data.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
	tray_data.uCallbackMessage = WM_HUB_TRAY;
	tray_data.hIcon = load_tray_icon();
	wstr_format(tray_data.szTip, TRAY_TIP_MAX, L"%ls v%ls", HUB_TITLE, HUB_VERSION);

	g_tray_icon_added = Shell_NotifyIconW(NIM_ADD, &tray_data);
	if (!g_tray_icon_added)
		hub_log(L"WARN: Shell_NotifyIcon(NIM_ADD) failed");
}

void tray_init(HWND hub_window)
{
	g_taskbar_created_msg = RegisterWindowMessageW(TASKBAR_CREATED_MSG);
	add_tray_icon(hub_window);
}

void tray_remove(HWND hub_window)
{
	NOTIFYICONDATAW tray_data;

	if (!g_tray_icon_added)
		return;

	fill_tray_data(hub_window, &tray_data);
	Shell_NotifyIconW(NIM_DELETE, &tray_data);
	g_tray_icon_added = FALSE;
}

static HMENU create_tray_menu(void)
{
	const struct hub_config *config = config_get();
	wchar_t title_text[TRAY_TIP_MAX];
	HMENU menu;

	menu = CreatePopupMenu();
	if (!menu)
		return NULL;

	wstr_format(title_text, TRAY_TIP_MAX, L"%ls v%ls", HUB_TITLE, HUB_VERSION);
	AppendMenuW(menu, MF_STRING | MF_GRAYED, TRAY_ID_TITLE, title_text);
	AppendMenuW(menu, MF_SEPARATOR, 0, NULL);
	AppendMenuW(menu, MF_STRING, TRAY_ID_EDIT_TEMPLATES, L"Edit &templates");
	AppendMenuW(menu, MF_STRING, TRAY_ID_EDIT_SETTINGS, L"Edit &settings");
	AppendMenuW(menu, MF_STRING | (config->log_enabled ? 0 : MF_GRAYED), TRAY_ID_OPEN_LOG, L"Open &log");
	AppendMenuW(menu, MF_STRING, TRAY_ID_OPEN_FOLDER, L"Open &folder");
	AppendMenuW(menu, MF_SEPARATOR, 0, NULL);
	AppendMenuW(menu, MF_STRING, TRAY_ID_EXIT, L"E&xit");
	SetMenuDefaultItem(menu, TRAY_ID_EDIT_TEMPLATES, FALSE);

	return menu;
}

static void run_tray_command(HWND hub_window, int command_id)
{
	const struct hub_config *config = config_get();

	switch (command_id) {
	case TRAY_ID_EDIT_TEMPLATES: winutil_open_path(config->template_path); break;
	case TRAY_ID_EDIT_SETTINGS: winutil_open_path(config->ini_path); break;
	case TRAY_ID_OPEN_LOG: winutil_open_path(config->log_path); break;
	case TRAY_ID_OPEN_FOLDER: winutil_open_path(config->exe_dir); break;
	case TRAY_ID_EXIT: PostMessageW(hub_window, WM_CLOSE, 0, 0); break;
	default: break;
	}
}

static void show_tray_menu(HWND hub_window)
{
	POINT mouse_position;
	HMENU menu;
	int command_id;

	config_load();
	menu = create_tray_menu();
	if (!menu)
		return;

	GetCursorPos(&mouse_position);
	SetForegroundWindow(hub_window);
	command_id = (int)TrackPopupMenuEx(menu, TPM_RETURNCMD | TPM_NONOTIFY | TPM_RIGHTBUTTON | TPM_BOTTOMALIGN, mouse_position.x, mouse_position.y, hub_window, NULL);
	PostMessageW(hub_window, WM_NULL, 0, 0);
	DestroyMenu(menu);

	run_tray_command(hub_window, command_id);
}

BOOL tray_handle_message(HWND hub_window, UINT message, LPARAM l_param)
{

	if (message == WM_HUB_TRAY) {
		if (LOWORD(l_param) == WM_RBUTTONUP || LOWORD(l_param) == WM_CONTEXTMENU)
			show_tray_menu(hub_window);
		else if (LOWORD(l_param) == WM_LBUTTONDBLCLK)
			run_tray_command(hub_window, TRAY_ID_EDIT_TEMPLATES);

		return TRUE;
	}

	if (g_taskbar_created_msg && message == g_taskbar_created_msg) {
		g_tray_icon_added = FALSE;
		add_tray_icon(hub_window);
		return TRUE;
	}

	return FALSE;
}
