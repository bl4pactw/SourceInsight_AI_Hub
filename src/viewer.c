#include <stdlib.h>
#include <string.h>
#include <shlobj.h>
#include "viewer.h"
#include "webview2_c.h"
#include "config.h"
#include "hub_log.h"
#include "mdrender.h"
#include "winutil.h"
#include "wstr.h"
#include "../res/resource.h"

#define VIEWER_WINDOW_CLASS         L"SiHubViewer"
#define VIEWER_CAPTION_FMT          L"%ls - " HUB_TITLE
#define VIEWER_CAPTION_MAX          (MAX_PATH + 32)
#define VIEWER_LOADER_PATH_FMT      L"%ls\\WebView2Loader.dll"
#define VIEWER_PAGE_PATH_FMT        L"%ls\\viewer.html"
#define VIEWER_USER_DATA_PATH_FMT   L"%ls\\SiHub\\WebView2"
#define LOCAL_APP_DATA_ENV          L"LOCALAPPDATA"
#define VIEWER_CREATE_ENV_PROC      "CreateCoreWebView2EnvironmentWithOptions"

#define VIEWER_INI_SECTION          L"viewer"
#define VIEWER_INI_KEY_PLACEMENT    L"placement"
#define VIEWER_PLACEMENT_FMT        L"%d,%d,%d,%d,%d"
#define VIEWER_PLACEMENT_FIELDS     5
#define VIEWER_PLACEMENT_MAX        64

#define VIEWER_DEFAULT_WIDTH        900
#define VIEWER_DEFAULT_HEIGHT       760
#define VIEWER_MIN_SIZE             240
#define VIEWER_BASE_DPI             96

#define VIEWER_FILE_MAX             (8 * 1024 * 1024)
#define VIEWER_PAGE_MAX             (1024 * 1024)
#define VIEWER_META_MAX             96
#define VIEWER_META_FMT             L"%04u-%02u-%02u %02u:%02u:%02u  \x00B7  %.1f KB"
#define VIEWER_BYTES_PER_KB         1024.0
#define VIEWER_JSON_ESCAPE_FMT      L"\\u%04x"
#define VIEWER_JSON_ESCAPE_MAX      8

#define VIEWER_MESSAGE_READY        L"ready"
#define VIEWER_MESSAGE_CLOSE        L"close"
#define VIEWER_MESSAGE_OPEN_URL     L"open-url:"

enum viewer_state
{
	VIEWER_STATE_IDLE,
	VIEWER_STATE_STARTING,
	VIEWER_STATE_READY,
	VIEWER_STATE_UNAVAILABLE,
};

static const GUID iid_unknown = { 0x00000000, 0x0000, 0x0000, { 0xc0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x46 } };

static const wchar_t *const allowed_url_prefix_tbl[] =
{
	L"http://",
	L"https://",
};

static enum viewer_state g_state;
static BOOL g_class_registered;
static HWND g_window;
static HMODULE g_loader_module;
static ICoreWebView2Environment *g_environment;
static ICoreWebView2Controller *g_controller;
static ICoreWebView2 *g_webview;
static BOOL g_page_ready;
static wchar_t *g_content_json;
static wchar_t g_fallback_path[MAX_PATH];
static BOOL g_fallback_wanted;

static BOOL is_iid(REFIID riid, const GUID *iid)
{
	return !memcmp(riid, iid, sizeof(GUID));
}

static HRESULT query_handler(void *self, REFIID riid, const GUID *own_iid, void **object)
{

	if (is_iid(riid, own_iid) || is_iid(riid, &iid_unknown)) {
		*object = self;
		return S_OK;
	}

	*object = NULL;

	return E_NOINTERFACE;
}

static void release_webview(void)
{

	if (g_controller) {
		g_controller->lpVtbl->Close(g_controller);
		g_controller->lpVtbl->Release(g_controller);
		g_controller = NULL;
	}

	if (g_webview) {
		g_webview->lpVtbl->Release(g_webview);
		g_webview = NULL;
	}

	if (g_environment) {
		g_environment->lpVtbl->Release(g_environment);
		g_environment = NULL;
	}

	g_page_ready = FALSE;
}

static void save_placement(void)
{
	wchar_t placement_text[VIEWER_PLACEMENT_MAX];
	WINDOWPLACEMENT placement;
	RECT *normal;

	placement.length = sizeof(placement);
	if (!g_window || !IsWindowVisible(g_window) || !GetWindowPlacement(g_window, &placement))
		return;

	normal = &placement.rcNormalPosition;
	wstr_format(placement_text, VIEWER_PLACEMENT_MAX, VIEWER_PLACEMENT_FMT, (int)normal->left, (int)normal->top, (int)(normal->right - normal->left), (int)(normal->bottom - normal->top), IsZoomed(g_window) ? 1 : 0);
	WritePrivateProfileStringW(VIEWER_INI_SECTION, VIEWER_INI_KEY_PLACEMENT, placement_text, config_get()->ini_path);
}

static BOOL load_placement(RECT *normal_out, BOOL *maximized_out)
{
	wchar_t placement_text[VIEWER_PLACEMENT_MAX];
	int left;
	int top;
	int width;
	int height;
	int maximized;

	GetPrivateProfileStringW(VIEWER_INI_SECTION, VIEWER_INI_KEY_PLACEMENT, L"", placement_text, VIEWER_PLACEMENT_MAX, config_get()->ini_path);
	if (swscanf(placement_text, VIEWER_PLACEMENT_FMT, &left, &top, &width, &height, &maximized) != VIEWER_PLACEMENT_FIELDS)
		return FALSE;

	if (width < VIEWER_MIN_SIZE || height < VIEWER_MIN_SIZE)
		return FALSE;

	SetRect(normal_out, left, top, left + width, top + height);
	if (!MonitorFromRect(normal_out, MONITOR_DEFAULTTONULL))
		return FALSE;

	*maximized_out = maximized != 0;

	return TRUE;
}

static void hide_viewer(void)
{

	if (!g_window)
		return;

	save_placement();
	if (g_controller)
		g_controller->lpVtbl->SetIsVisible(g_controller, FALSE);

	ShowWindow(g_window, SW_HIDE);
}

static void fail_viewer(const wchar_t *stage, HRESULT error_code)
{
	hub_log(L"viewer: %ls failed (0x%08lx), using the file instead", stage, (unsigned long)error_code);
	g_state = VIEWER_STATE_UNAVAILABLE;
	release_webview();

	if (g_window) {
		DestroyWindow(g_window);
		g_window = NULL;
	}

	if (g_fallback_wanted)
		winutil_open_path(g_fallback_path);
}

static void post_content(void)
{

	if (g_webview && g_page_ready && g_content_json)
		g_webview->lpVtbl->PostWebMessageAsJson(g_webview, g_content_json);
}

static void open_url(const wchar_t *url)
{
	size_t index;

	for (index = 0; index < ARRAYSIZE(allowed_url_prefix_tbl); index++) {
		if (!_wcsnicmp(url, allowed_url_prefix_tbl[index], wcslen(allowed_url_prefix_tbl[index]))) {
			winutil_shell_execute(url);
			return;
		}
	}

	hub_log(L"viewer: blocked url '%ls'", url);
}

static void handle_web_message(const wchar_t *message)
{
	size_t open_url_length = wcslen(VIEWER_MESSAGE_OPEN_URL);

	if (!wcscmp(message, VIEWER_MESSAGE_READY)) {
		g_page_ready = TRUE;
		post_content();
	}
	else if (!wcscmp(message, VIEWER_MESSAGE_CLOSE)) {
		hide_viewer();
	}
	else if (!wcsncmp(message, VIEWER_MESSAGE_OPEN_URL, open_url_length)) {
		open_url(message + open_url_length);
	}
}

static HRESULT STDMETHODCALLTYPE message_handler_query(ICoreWebView2WebMessageReceivedEventHandler *self, REFIID riid, void **object)
{
	return query_handler(self, riid, &IID_ICoreWebView2WebMessageReceivedEventHandler, object);
}

static ULONG STDMETHODCALLTYPE message_handler_add_ref(ICoreWebView2WebMessageReceivedEventHandler *self)
{
	(void)self;

	return 1;
}

static ULONG STDMETHODCALLTYPE message_handler_release(ICoreWebView2WebMessageReceivedEventHandler *self)
{
	(void)self;

	return 1;
}

static HRESULT STDMETHODCALLTYPE message_handler_invoke(ICoreWebView2WebMessageReceivedEventHandler *self, ICoreWebView2 *sender, ICoreWebView2WebMessageReceivedEventArgs *args)
{
	LPWSTR message = NULL;

	(void)self;
	(void)sender;

	if (SUCCEEDED(args->lpVtbl->TryGetWebMessageAsString(args, &message)) && message)
		handle_web_message(message);

	CoTaskMemFree(message);

	return S_OK;
}

static const struct ICoreWebView2WebMessageReceivedEventHandler_vtbl message_handler_vtbl =
{
	.QueryInterface = message_handler_query,
	.AddRef = message_handler_add_ref,
	.Release = message_handler_release,
	.Invoke = message_handler_invoke,
};

static ICoreWebView2WebMessageReceivedEventHandler g_message_handler =
{
	.lpVtbl = &message_handler_vtbl,
};

static wchar_t *load_page_html(void)
{
	wchar_t page_path[MAX_PATH];
	const char *page_bytes = NULL;
	HGLOBAL resource_data = NULL;
	wchar_t *page_html;
	HRSRC resource;
	DWORD page_size;
	int wide_chars;

	wstr_format(page_path, MAX_PATH, VIEWER_PAGE_PATH_FMT, config_get()->exe_dir);
	page_html = wstr_read_utf8_file(page_path, VIEWER_PAGE_MAX);

	if (page_html) {
		hub_log(L"viewer: page override %ls", page_path);
		return page_html;
	}

	resource = FindResourceW(NULL, MAKEINTRESOURCEW(IDR_VIEWER_HTML), RT_RCDATA);
	if (resource)
		resource_data = LoadResource(NULL, resource);

	if (resource_data)
		page_bytes = LockResource(resource_data);

	if (!page_bytes)
		return NULL;

	page_size = SizeofResource(NULL, resource);
	wide_chars = MultiByteToWideChar(CP_UTF8, 0, page_bytes, (int)page_size, NULL, 0);
	if (wide_chars <= 0)
		return NULL;

	page_html = malloc(((size_t)wide_chars + 1) * sizeof(wchar_t));
	if (!page_html)
		return NULL;

	MultiByteToWideChar(CP_UTF8, 0, page_bytes, (int)page_size, page_html, wide_chars);
	page_html[wide_chars] = 0;

	return page_html;
}

static void configure_settings(ICoreWebView2 *webview)
{
	ICoreWebView2Settings *settings = NULL;

	if (FAILED(webview->lpVtbl->Settings(webview, &settings)) || !settings)
		return;

	settings->lpVtbl->SetAreDevToolsEnabled(settings, config_get()->viewer_devtools);
	settings->lpVtbl->SetIsStatusBarEnabled(settings, FALSE);
	settings->lpVtbl->SetAreDefaultScriptDialogsEnabled(settings, FALSE);
	settings->lpVtbl->SetIsZoomControlEnabled(settings, TRUE);
	settings->lpVtbl->Release(settings);
}

static void resize_webview(void)
{
	RECT bounds;

	if (!g_controller || !GetClientRect(g_window, &bounds))
		return;

	g_controller->lpVtbl->SetBounds(g_controller, bounds);
}

static void move_focus(void)
{

	if (g_controller)
		g_controller->lpVtbl->MoveFocus(g_controller, WEBVIEW2_MOVE_FOCUS_REASON_PROGRAMMATIC);
}

static HRESULT STDMETHODCALLTYPE controller_handler_query(ICoreWebView2CreateCoreWebView2ControllerCompletedHandler *self, REFIID riid, void **object)
{
	return query_handler(self, riid, &IID_ICoreWebView2CreateCoreWebView2ControllerCompletedHandler, object);
}

static ULONG STDMETHODCALLTYPE controller_handler_add_ref(ICoreWebView2CreateCoreWebView2ControllerCompletedHandler *self)
{
	(void)self;

	return 1;
}

static ULONG STDMETHODCALLTYPE controller_handler_release(ICoreWebView2CreateCoreWebView2ControllerCompletedHandler *self)
{
	(void)self;

	return 1;
}

static HRESULT STDMETHODCALLTYPE controller_handler_invoke(ICoreWebView2CreateCoreWebView2ControllerCompletedHandler *self, HRESULT error_code, ICoreWebView2Controller *controller)
{
	ICoreWebView2 *webview = NULL;
	wchar_t *page_html;
	INT64 token;

	(void)self;

	if (g_state != VIEWER_STATE_STARTING || !g_window)
		return S_OK;

	if (FAILED(error_code) || !controller) {
		fail_viewer(L"controller", error_code);
		return S_OK;
	}

	controller->lpVtbl->AddRef(controller);
	g_controller = controller;

	if (FAILED(controller->lpVtbl->CoreWebView2(controller, &webview)) || !webview) {
		fail_viewer(L"webview", E_NOINTERFACE);
		return S_OK;
	}

	g_webview = webview;
	configure_settings(webview);
	webview->lpVtbl->add_WebMessageReceived(webview, &g_message_handler, &token);
	resize_webview();
	controller->lpVtbl->SetIsVisible(controller, IsWindowVisible(g_window) && !IsIconic(g_window));
	page_html = load_page_html();

	if (!page_html) {
		fail_viewer(L"page", E_FAIL);
		return S_OK;
	}

	g_page_ready = FALSE;
	webview->lpVtbl->NavigateToString(webview, page_html);
	free(page_html);
	g_state = VIEWER_STATE_READY;
	if (GetForegroundWindow() == g_window)
		move_focus();

	hub_log(L"viewer: ready");

	return S_OK;
}

static const struct ICoreWebView2CreateCoreWebView2ControllerCompletedHandler_vtbl controller_handler_vtbl =
{
	.QueryInterface = controller_handler_query,
	.AddRef = controller_handler_add_ref,
	.Release = controller_handler_release,
	.Invoke = controller_handler_invoke,
};

static ICoreWebView2CreateCoreWebView2ControllerCompletedHandler g_controller_handler =
{
	.lpVtbl = &controller_handler_vtbl,
};

static HRESULT STDMETHODCALLTYPE environment_handler_query(ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler *self, REFIID riid, void **object)
{
	return query_handler(self, riid, &IID_ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler, object);
}

static ULONG STDMETHODCALLTYPE environment_handler_add_ref(ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler *self)
{
	(void)self;

	return 1;
}

static ULONG STDMETHODCALLTYPE environment_handler_release(ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler *self)
{
	(void)self;

	return 1;
}

static HRESULT STDMETHODCALLTYPE environment_handler_invoke(ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler *self, HRESULT error_code, ICoreWebView2Environment *environment)
{
	HRESULT result;

	(void)self;

	if (g_state != VIEWER_STATE_STARTING || !g_window)
		return S_OK;

	if (FAILED(error_code) || !environment) {
		fail_viewer(L"environment", error_code);
		return S_OK;
	}

	environment->lpVtbl->AddRef(environment);
	g_environment = environment;
	result = environment->lpVtbl->CreateCoreWebView2Controller(environment, g_window, &g_controller_handler);
	if (FAILED(result))
		fail_viewer(L"controller", result);

	return S_OK;
}

static const struct ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler_vtbl environment_handler_vtbl =
{
	.QueryInterface = environment_handler_query,
	.AddRef = environment_handler_add_ref,
	.Release = environment_handler_release,
	.Invoke = environment_handler_invoke,
};

static ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler g_environment_handler =
{
	.lpVtbl = &environment_handler_vtbl,
};

static void start_webview(void)
{
	CreateCoreWebView2EnvironmentWithOptionsFunc create_environment;
	wchar_t loader_path[MAX_PATH];
	wchar_t local_app_data[MAX_PATH];
	wchar_t user_data_path[MAX_PATH];
	HRESULT result;

	g_state = VIEWER_STATE_STARTING;
	wstr_format(loader_path, MAX_PATH, VIEWER_LOADER_PATH_FMT, config_get()->exe_dir);
	if (!g_loader_module)
		g_loader_module = LoadLibraryExW(loader_path, NULL, LOAD_WITH_ALTERED_SEARCH_PATH);

	if (!g_loader_module) {
		fail_viewer(L"loading WebView2Loader.dll", HRESULT_FROM_WIN32(GetLastError()));
		return;
	}

	create_environment = (CreateCoreWebView2EnvironmentWithOptionsFunc)(void (*)(void))GetProcAddress(g_loader_module, VIEWER_CREATE_ENV_PROC);
	if (!create_environment) {
		fail_viewer(L"finding the environment entry point", HRESULT_FROM_WIN32(GetLastError()));
		return;
	}

	if (!GetEnvironmentVariableW(LOCAL_APP_DATA_ENV, local_app_data, MAX_PATH)) {
		fail_viewer(L"reading " LOCAL_APP_DATA_ENV, HRESULT_FROM_WIN32(GetLastError()));
		return;
	}

	wstr_format(user_data_path, MAX_PATH, VIEWER_USER_DATA_PATH_FMT, local_app_data);
	SHCreateDirectoryExW(NULL, user_data_path, NULL);
	result = create_environment(NULL, user_data_path, NULL, &g_environment_handler);
	if (FAILED(result))
		fail_viewer(L"environment", result);
}

static LRESULT CALLBACK viewer_window_proc(HWND window, UINT message, WPARAM w_param, LPARAM l_param)
{
	MINMAXINFO *size_limits;

	switch (message) {
	case WM_SIZE:
		if (g_controller) {
			g_controller->lpVtbl->SetIsVisible(g_controller, w_param != SIZE_MINIMIZED);
			resize_webview();
		}

		return 0;

	case WM_SETFOCUS:
		move_focus();
		return 0;

	case WM_GETMINMAXINFO:
		size_limits = (MINMAXINFO *)l_param;
		size_limits->ptMinTrackSize.x = VIEWER_MIN_SIZE;
		size_limits->ptMinTrackSize.y = VIEWER_MIN_SIZE;
		return 0;

	case WM_CLOSE:
		hide_viewer();
		return 0;

	default:
		break;
	}

	return DefWindowProcW(window, message, w_param, l_param);
}

static BOOL register_viewer_class(HINSTANCE instance)
{
	WNDCLASSEXW window_class;

	if (g_class_registered)
		return TRUE;

	ZeroMemory(&window_class, sizeof(window_class));
	window_class.cbSize = sizeof(window_class);
	window_class.lpfnWndProc = viewer_window_proc;
	window_class.hInstance = instance;
	window_class.lpszClassName = VIEWER_WINDOW_CLASS;
	window_class.hIcon = LoadIconW(instance, MAKEINTRESOURCEW(IDI_SIHUB));
	window_class.hCursor = LoadCursorW(NULL, IDC_ARROW);
	window_class.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
	g_class_registered = RegisterClassExW(&window_class) != 0;

	return g_class_registered;
}

static int scale_for_dpi(int size)
{
	HDC screen = GetDC(NULL);
	int dpi = screen ? GetDeviceCaps(screen, LOGPIXELSX) : VIEWER_BASE_DPI;

	if (screen)
		ReleaseDC(NULL, screen);

	return MulDiv(size, dpi, VIEWER_BASE_DPI);
}

static BOOL create_viewer_window(BOOL activate)
{
	HINSTANCE instance = GetModuleHandleW(NULL);
	WINDOWPLACEMENT placement;
	BOOL maximized = FALSE;
	HWND window;

	if (!register_viewer_class(instance))
		return FALSE;

	window = CreateWindowExW(0, VIEWER_WINDOW_CLASS, HUB_TITLE, WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, scale_for_dpi(VIEWER_DEFAULT_WIDTH), scale_for_dpi(VIEWER_DEFAULT_HEIGHT), NULL, NULL, instance, NULL);
	if (!window)
		return FALSE;

	g_window = window;
	placement.length = sizeof(placement);
	GetWindowPlacement(window, &placement);
	load_placement(&placement.rcNormalPosition, &maximized);

	if (maximized)
		placement.showCmd = SW_SHOWMAXIMIZED;
	else
		placement.showCmd = activate ? SW_SHOWNORMAL : SW_SHOWNOACTIVATE;

	SetWindowPlacement(window, &placement);
	if (activate)
		SetForegroundWindow(window);

	return TRUE;
}

static void show_existing_window(BOOL activate)
{

	if (!IsWindowVisible(g_window))
		ShowWindow(g_window, activate ? SW_SHOW : SW_SHOWNOACTIVATE);

	if (activate) {
		if (IsIconic(g_window))
			ShowWindow(g_window, SW_RESTORE);

		SetForegroundWindow(g_window);
	}

	if (g_controller)
		g_controller->lpVtbl->SetIsVisible(g_controller, !IsIconic(g_window));
}

static void format_meta(const wchar_t *path, wchar_t *meta, size_t meta_chars)
{
	WIN32_FILE_ATTRIBUTE_DATA attributes;
	FILETIME local_time;
	SYSTEMTIME time;

	meta[0] = 0;
	if (!GetFileAttributesExW(path, GetFileExInfoStandard, &attributes) || !FileTimeToLocalFileTime(&attributes.ftLastWriteTime, &local_time) || !FileTimeToSystemTime(&local_time, &time))
		return;

	wstr_format(meta, meta_chars, VIEWER_META_FMT, time.wYear, time.wMonth, time.wDay, time.wHour, time.wMinute, time.wSecond, attributes.nFileSizeLow / VIEWER_BYTES_PER_KB);
}

static BOOL json_append_string(struct wide_buf *buf, const wchar_t *text)
{
	wchar_t escape_text[VIEWER_JSON_ESCAPE_MAX];
	const wchar_t *run_start = text;
	const wchar_t *cursor;
	BOOL ok;

	ok = wide_buf_append(buf, L"\"");
	for (cursor = text; ok && *cursor; cursor++) {
		if (*cursor >= L' ' && *cursor != L'"' && *cursor != L'\\')
			continue;

		wstr_format(escape_text, VIEWER_JSON_ESCAPE_MAX, VIEWER_JSON_ESCAPE_FMT, (unsigned)*cursor);
		ok = wide_buf_append_chars(buf, run_start, (size_t)(cursor - run_start)) && wide_buf_append(buf, escape_text);
		run_start = cursor + 1;
	}

	return ok && wide_buf_append_chars(buf, run_start, (size_t)(cursor - run_start)) && wide_buf_append(buf, L"\"");
}

static BOOL json_append_field(struct wide_buf *buf, const wchar_t *name, const wchar_t *value, BOOL is_last)
{
	return wide_buf_append(buf, L"\"") && wide_buf_append(buf, name) && wide_buf_append(buf, L"\":") && json_append_string(buf, value) && wide_buf_append(buf, is_last ? L"}" : L",");
}

static wchar_t *build_content_json(const wchar_t *path)
{
	wchar_t meta[VIEWER_META_MAX];
	struct wide_buf json;
	char *markdown_utf8 = NULL;
	char *html_utf8 = NULL;
	wchar_t *html = NULL;
	wchar_t *markdown;
	BOOL ok;

	markdown = wstr_read_utf8_file(path, VIEWER_FILE_MAX);
	if (!markdown)
		return NULL;

	markdown_utf8 = wstr_to_utf8(markdown);
	if (markdown_utf8)
		html_utf8 = mdrender_to_html(markdown_utf8, strlen(markdown_utf8));

	if (html_utf8)
		html = wstr_utf8_to_wide(html_utf8);

	format_meta(path, meta, VIEWER_META_MAX);
	wide_buf_init(&json);
	ok = html && wide_buf_append(&json, L"{") && json_append_field(&json, L"markdown", markdown, FALSE) && json_append_field(&json, L"html", html, FALSE) && json_append_field(&json, L"title", wstr_basename(path), FALSE) && json_append_field(&json, L"meta", meta, TRUE);
	free(markdown);
	free(markdown_utf8);
	mdrender_free(html_utf8);
	free(html);

	if (!ok) {
		wide_buf_free(&json);
		return NULL;
	}

	return wide_buf_detach(&json);
}

void viewer_show_file(const wchar_t *path, BOOL activate)
{
	wchar_t caption[VIEWER_CAPTION_MAX];
	wchar_t *content_json;

	wstr_copy(g_fallback_path, MAX_PATH, path);
	g_fallback_wanted = activate;

	if (g_state == VIEWER_STATE_UNAVAILABLE) {
		if (activate)
			winutil_open_path(path);

		return;
	}

	content_json = build_content_json(path);
	if (!content_json) {
		hub_log(L"viewer: cannot render %ls", path);
		if (activate)
			winutil_open_path(path);

		return;
	}

	free(g_content_json);
	g_content_json = content_json;
	wstr_format(caption, VIEWER_CAPTION_MAX, VIEWER_CAPTION_FMT, wstr_basename(path));

	if (g_window) {
		SetWindowTextW(g_window, caption);
		show_existing_window(activate);
		post_content();
		return;
	}

	if (!create_viewer_window(activate)) {
		fail_viewer(L"creating the window", HRESULT_FROM_WIN32(GetLastError()));
		return;
	}

	SetWindowTextW(g_window, caption);
	start_webview();
}

void viewer_shutdown(void)
{
	save_placement();
	release_webview();

	if (g_window) {
		DestroyWindow(g_window);
		g_window = NULL;
	}

	free(g_content_json);
	g_content_json = NULL;
	g_state = VIEWER_STATE_IDLE;
}
