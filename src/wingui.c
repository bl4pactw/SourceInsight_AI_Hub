#include "wingui.h"
#include "config.h"
#include "hub_log.h"
#include "ipc.h"
#include "preprompt.h"
#include "sshconn.h"
#include "sshui.h"
#include "target.h"
#include "tray.h"
#include "winutil.h"
#include "wstr.h"
#include "../res/resource.h"

#define MENU_HEADER_MAX         400
#define MENU_ITEM_MAX           (TEMPLATE_LABEL_MAX + 8)
#define MENU_ID_HEADER          1
#define MENU_ID_ERROR           2
#define MENU_ID_TEMPLATE_BASE   100
#define MENU_CANCELLED          (-1)

static struct hub_request g_pending_request;
static BOOL g_has_pending_request;

static POINT get_menu_anchor(HWND foreground_window)
{
	GUITHREADINFO thread_info;
	POINT anchor;
	POINT caret_bottom_left;
	DWORD thread_id;

	GetCursorPos(&anchor);
	if (!foreground_window)
		return anchor;

	ZeroMemory(&thread_info, sizeof(thread_info));
	thread_info.cbSize = sizeof(thread_info);
	thread_id = GetWindowThreadProcessId(foreground_window, NULL);

	if (GetGUIThreadInfo(thread_id, &thread_info) && thread_info.hwndCaret) {
		caret_bottom_left.x = thread_info.rcCaret.left;
		caret_bottom_left.y = thread_info.rcCaret.bottom;
		if (ClientToScreen(thread_info.hwndCaret, &caret_bottom_left))
			anchor = caret_bottom_left;
	}
	else {
		hub_log(L"no system caret on foreground window, using mouse position");
	}

	return anchor;
}

static HMENU create_template_menu(const struct hub_request *request)
{
	wchar_t header_text[MENU_HEADER_MAX];
	wchar_t item_text[MENU_ITEM_MAX];
	const wchar_t *load_error = preprompt_load_error();
	HMENU menu;
	int index;

	menu = CreatePopupMenu();
	if (!menu)
		return NULL;

	wstr_format(header_text, MENU_HEADER_MAX, L"%ls:%d  %ls", wstr_basename(request->file), request->first_line, request->symbol);
	AppendMenuW(menu, MF_STRING | MF_GRAYED, MENU_ID_HEADER, header_text);
	AppendMenuW(menu, MF_SEPARATOR, 0, NULL);
	if (load_error[0])
		AppendMenuW(menu, MF_STRING | MF_GRAYED, MENU_ID_ERROR, load_error);

	for (index = 0; index < preprompt_template_count(); index++) {
		wstr_format(item_text, MENU_ITEM_MAX, L"&%d  %ls", index + 1, preprompt_template_label(index));
		AppendMenuW(menu, MF_STRING, MENU_ID_TEMPLATE_BASE + index, item_text);
	}

	return menu;
}

static int pick_template(HWND hub_window, const struct hub_request *request, HWND previous_foreground)
{
	POINT anchor;
	HMENU menu;
	int selected_id;
	int template_index;

	if (request->auto_pick >= 1 && request->auto_pick <= preprompt_template_count())
		return request->auto_pick - 1;

	anchor = get_menu_anchor(previous_foreground);
	menu = create_template_menu(request);
	if (!menu)
		return MENU_CANCELLED;

	SetForegroundWindow(hub_window);
	selected_id = (int)TrackPopupMenuEx(menu, TPM_RETURNCMD | TPM_NONOTIFY | TPM_LEFTALIGN | TPM_TOPALIGN | TPM_RIGHTBUTTON, anchor.x, anchor.y, hub_window, NULL);
	PostMessageW(hub_window, WM_NULL, 0, 0);
	DestroyMenu(menu);

	template_index = selected_id - MENU_ID_TEMPLATE_BASE;
	if (template_index >= 0 && template_index < preprompt_template_count())
		return template_index;

	hub_log(L"menu cancelled");
	if (previous_foreground && IsWindow(previous_foreground))
		SetForegroundWindow(previous_foreground);

	return MENU_CANCELLED;
}

static void handle_request(HWND hub_window, const struct hub_request *request)
{
	struct wide_buf prompt;
	HWND previous_foreground;
	int template_index;
	int shell_code;

	previous_foreground = GetForegroundWindow();
	config_load();
	preprompt_load_templates();
	hub_log(L"request file='%ls' line=%d-%d symbol='%ls'", request->file, request->first_line, request->last_line, request->symbol);

	template_index = pick_template(hub_window, request, previous_foreground);
	if (template_index == MENU_CANCELLED)
		return;

	hub_log(L"template=%d '%ls'", template_index + 1, preprompt_template_label(template_index));
	wide_buf_init(&prompt);

	if (preprompt_build(template_index, request, &prompt) && !target_send(hub_window, prompt.chars, &shell_code)) {
		winutil_show_error(L"Cannot open target URI (ShellExecute=%d).\n\n"
						   L"Check that VS Code is installed and the vscode:// protocol is registered.\n"
						   L"Prompt was %lscopied to the clipboard.",
						   shell_code, config_get()->copy_to_clipboard ? L"" : L"NOT ");
	}

	wide_buf_free(&prompt);
}

static void queue_request(HWND hub_window, const struct hub_request *request)
{
	g_pending_request = *request;
	g_has_pending_request = TRUE;
	PostMessageW(hub_window, WM_HUB_SHOWMENU, 0, 0);
}

static void run_pending_request(HWND hub_window)
{
	struct hub_request request;

	if (!g_has_pending_request)
		return;

	request = g_pending_request;
	g_has_pending_request = FALSE;
	handle_request(hub_window, &request);
}

static LRESULT CALLBACK hub_window_proc(HWND window, UINT message, WPARAM w_param, LPARAM l_param)
{
	struct sshui_notice notice;
	struct hub_request request;

	if (tray_handle_message(window, message, l_param))
		return 0;

	switch (message) {
	case WM_COPYDATA:
		if (!ipc_receive_request((const COPYDATASTRUCT *)l_param, &request))
			return FALSE;

		queue_request(window, &request);
		return TRUE;

	case WM_HUB_SHOWMENU:
		run_pending_request(window);
		return 0;

	case WM_HUB_SSH_PROMPT:
		sshui_handle_prompt(window, (struct sshconn_prompt *)l_param);
		return 0;

	case WM_HUB_SSH_EVENT:
		sshui_handle_event((struct sshconn_event *)l_param, &notice);
		if (notice.show)
			tray_show_balloon(window, notice.title, notice.text);

		return 0;

	case WM_CLOSE:
		if (sshui_prompt_active())
			return 0;

		hub_log(L"server quit");
		sshconn_shutdown();
		DestroyWindow(window);
		return 0;

	case WM_DESTROY:
		tray_remove(window);
		PostQuitMessage(0);
		return 0;

	default:
		break;
	}

	return DefWindowProcW(window, message, w_param, l_param);
}

static HWND create_hub_window(HINSTANCE instance)
{
	WNDCLASSEXW window_class;

	ZeroMemory(&window_class, sizeof(window_class));
	window_class.cbSize = sizeof(window_class);
	window_class.lpfnWndProc = hub_window_proc;
	window_class.hInstance = instance;
	window_class.lpszClassName = IPC_WINDOW_CLASS;
	window_class.hIcon = LoadIconW(instance, MAKEINTRESOURCEW(IDI_SIHUB));
	if (!RegisterClassExW(&window_class))
		return NULL;

	return CreateWindowExW(WS_EX_TOOLWINDOW, IPC_WINDOW_CLASS, HUB_TITLE, WS_POPUP, 0, 0, 0, 0, NULL, NULL, instance, NULL);
}

int wingui_run_server(HINSTANCE instance, const struct hub_request *initial_request)
{
	HWND hub_window;
	MSG message;

	hub_window = create_hub_window(instance);
	if (!hub_window)
		return 1;

	hub_log(L"server start v%ls", HUB_VERSION);
	tray_init(hub_window);
	sshconn_init(hub_window);
	if (initial_request)
		queue_request(hub_window, initial_request);

	while (GetMessageW(&message, NULL, 0, 0) > 0) {
		TranslateMessage(&message);
		DispatchMessageW(&message);
	}

	preprompt_free_templates();

	return 0;
}
