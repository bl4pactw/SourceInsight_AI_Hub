#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shellapi.h>
#include <wchar.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include "../res/resource.h"

#define HUB_VERSION              L"0.3.1"
#define HUB_TITLE                L"SI AI Hub"
#define HUB_WINDOW_CLASS         L"SiHub.Server.v1"
#define HUB_MUTEX_NAME           L"Local\\SiHub.Server.v1"
#define HUB_COPYDATA_MAGIC       0x53494842u
#define WM_HUB_SHOWMENU          (WM_APP + 1)
#define WM_HUB_TRAY              (WM_APP + 2)
#define TRAY_ICON_ID             1

#define INI_PATH_FMT             L"%ls\\sihub.ini"
#define LOG_PATH_FMT             L"%ls\\sihub.log"
#define EXE_DIR_FILE_PATH_FMT    L"%ls\\%ls"
#define DEFAULT_TEMPLATE_PATH    L"sihub_templates.txt"

#define INI_SECTION_GENERAL      L"general"
#define INI_SECTION_PATHS        L"paths"
#define INI_SECTION_TARGET       L"target"
#define INI_KEY_LOG              L"log"
#define INI_KEY_TEMPLATES        L"templates"
#define INI_KEY_TRAY_ICON        L"tray_icon"
#define INI_KEY_WIN_PREFIX       L"win_prefix"
#define INI_KEY_LINUX_PREFIX     L"linux_prefix"
#define INI_KEY_URI              L"uri"
#define INI_KEY_CLIPBOARD        L"copy_to_clipboard"
#define INI_KEY_DRY_RUN          L"dry_run"

#define PROMPT_PLACEHOLDER       L"{prompt}"

#define TEMPLATE_MAX             9
#define TEMPLATE_LABEL_MAX       64
#define TEMPLATE_FILE_MAX        (256 * 1024)
#define TEMPLATE_ERROR_MAX       256
#define REQ_FILE_MAX             1024
#define REQ_SYMBOL_MAX           256
#define URI_TEMPLATE_MAX         512
#define LINE_NUMBER_TEXT_MAX     16
#define MENU_HEADER_MAX          400
#define MENU_ITEM_MAX            (TEMPLATE_LABEL_MAX + 8)
#define ERROR_TEXT_MAX           600
#define TRAY_TIP_MAX             128
#define LOG_MESSAGE_MAX          4096
#define LOG_UTF8_MAX             (LOG_MESSAGE_MAX * 3)
#define WIDE_BUF_INIT_CAPACITY   256

#define SERVER_FIND_RETRIES      40
#define SERVER_FIND_DELAY_MS     50
#define COPYDATA_TIMEOUT_MS      3000
#define CLIPBOARD_RETRIES        5
#define CLIPBOARD_DELAY_MS       20

#define MENU_ID_HEADER           1
#define MENU_ID_ERROR            2
#define MENU_ID_TEMPLATE_BASE    100

#define TRAY_ID_TITLE            200
#define TRAY_ID_EDIT_TEMPLATES   201
#define TRAY_ID_EDIT_SETTINGS    202
#define TRAY_ID_OPEN_LOG         203
#define TRAY_ID_OPEN_FOLDER      204
#define TRAY_ID_EXIT             205

struct hub_request
{
	wchar_t file[REQ_FILE_MAX];
	wchar_t symbol[REQ_SYMBOL_MAX];
	int first_line;
	int last_line;
	int auto_pick;
};

struct prompt_template
{
	wchar_t label[TEMPLATE_LABEL_MAX];
	wchar_t *body;
};

struct hub_config
{
	wchar_t win_prefix[MAX_PATH];
	wchar_t linux_prefix[MAX_PATH];
	wchar_t uri_template[URI_TEMPLATE_MAX];
	BOOL copy_to_clipboard;
	BOOL log_enabled;
	BOOL dry_run;
	BOOL tray_icon_enabled;
	wchar_t template_path[MAX_PATH];
	wchar_t template_error[TEMPLATE_ERROR_MAX];
	int template_count;
	struct prompt_template templates[TEMPLATE_MAX];
};

struct wide_buf
{
	wchar_t *chars;
	size_t length;
	size_t capacity;
};

struct cmd_args
{
	BOOL quit_requested;
	BOOL has_request;
	struct hub_request request;
};

static const wchar_t default_win_prefix[] = L"Z:\\";
static const wchar_t default_linux_prefix[] = L"";
static const wchar_t default_uri_template[] = L"vscode://anthropic.claude-code/open?prompt=" PROMPT_PLACEHOLDER;
static const wchar_t default_template_path[] = DEFAULT_TEMPLATE_PATH;
static const int default_log_enabled = 0;
static const int default_copy_to_clipboard = 1;
static const int default_dry_run = 0;
static const int default_tray_icon = 1;
static const wchar_t missing_symbol_text[] = L"(current position)";

static const wchar_t hex_digit_tbl[] = L"0123456789ABCDEF";

static const struct
{
	const wchar_t *label;
	const wchar_t *body;
} fallback_template_tbl[] =
{
	{
		.label = L"Explain this code",
		.body = L"Explain {symbol} ({path}, lines {line1}-{line2}): purpose, inputs/outputs, key flow and caveats.\n"
				L"First quote line {line1} as you see it, to confirm we are looking at the same source tree.\n\n"
				L"@{path}",
	},
	{
		.label = L"Trace definition / origin",
		.body = L"Find where {symbol} is defined and implemented (macro expansion, conditional-compile branches, the variant actually built). "
				L"List file paths and line numbers. Start from {path}:{line1}.\n"
				L"First quote line {line1} to confirm the source tree matches.",
	},
	{
		.label = L"Impact analysis",
		.body = L"If {symbol} at {path} lines {line1}-{line2} is changed, list all callers/references and affected modules, "
				L"assess the risk, and point out what else must change with it.\n"
				L"First quote line {line1} to confirm the source tree matches.",
	},
};

static struct hub_config g_config;
static struct hub_request g_pending_request;
static BOOL g_has_pending_request;
static wchar_t g_exe_dir[MAX_PATH];
static UINT g_taskbar_created_msg;
static BOOL g_tray_icon_added;

static void format_wstr(wchar_t *dest, size_t dest_chars, const wchar_t *format, ...)
{
	va_list args;

	va_start(args, format);
	_vsnwprintf(dest, dest_chars - 1, format, args);
	va_end(args);
	dest[dest_chars - 1] = 0;
}

static void copy_wstr(wchar_t *dest, size_t dest_chars, const wchar_t *source)
{
	wcsncpy(dest, source, dest_chars - 1);
	dest[dest_chars - 1] = 0;
}

static BOOL wide_buf_reserve(struct wide_buf *buf, size_t extra_chars)
{
	size_t needed = buf->length + extra_chars + 1;
	size_t new_capacity;
	wchar_t *new_chars;

	if (needed <= buf->capacity)
		return TRUE;

	new_capacity = buf->capacity ? buf->capacity : WIDE_BUF_INIT_CAPACITY;
	while (new_capacity < needed)
		new_capacity *= 2;

	new_chars = realloc(buf->chars, new_capacity * sizeof(wchar_t));
	if (!new_chars)
		return FALSE;

	buf->chars = new_chars;
	buf->capacity = new_capacity;

	return TRUE;
}

static void wide_buf_append_chars(struct wide_buf *buf, const wchar_t *source, size_t char_count)
{

	if (!wide_buf_reserve(buf, char_count))
		return;

	wmemcpy(buf->chars + buf->length, source, char_count);
	buf->length += char_count;
	buf->chars[buf->length] = 0;
}

static void wide_buf_append(struct wide_buf *buf, const wchar_t *source)
{
	wide_buf_append_chars(buf, source, wcslen(source));
}

static void wide_buf_free(struct wide_buf *buf)
{
	free(buf->chars);
	buf->chars = NULL;
	buf->length = 0;
	buf->capacity = 0;
}

static wchar_t *wide_buf_detach(struct wide_buf *buf)
{
	wchar_t *chars = buf->chars;

	buf->chars = NULL;
	buf->length = 0;
	buf->capacity = 0;

	return chars;
}

static void hub_log(const wchar_t *format, ...)
{
	wchar_t message[LOG_MESSAGE_MAX];
	wchar_t log_path[MAX_PATH];
	char message_utf8[LOG_UTF8_MAX];
	SYSTEMTIME now;
	va_list args;
	FILE *log_file;
	int utf8_bytes;

	if (!g_config.log_enabled)
		return;

	va_start(args, format);
	_vsnwprintf(message, LOG_MESSAGE_MAX - 1, format, args);
	va_end(args);
	message[LOG_MESSAGE_MAX - 1] = 0;

	format_wstr(log_path, MAX_PATH, LOG_PATH_FMT, g_exe_dir);
	log_file = _wfopen(log_path, L"ab");
	if (!log_file)
		return;

	GetLocalTime(&now);
	utf8_bytes = WideCharToMultiByte(CP_UTF8, 0, message, -1, message_utf8, sizeof(message_utf8), NULL, NULL);
	fprintf(log_file, "%04d-%02d-%02d %02d:%02d:%02d.%03d [%lu] %s\r\n",
			now.wYear, now.wMonth, now.wDay, now.wHour, now.wMinute, now.wSecond, now.wMilliseconds,
			GetCurrentProcessId(), utf8_bytes > 0 ? message_utf8 : "?");
	fclose(log_file);
}

static void free_templates(void)
{
	int index;

	for (index = 0; index < g_config.template_count; index++) {
		free(g_config.templates[index].body);
		g_config.templates[index].body = NULL;
	}

	g_config.template_count = 0;
}

static void load_fallback_templates(void)
{
	int fallback_count = (int)ARRAYSIZE(fallback_template_tbl);
	int index;

	for (index = 0; index < fallback_count && index < TEMPLATE_MAX; index++) {
		struct prompt_template *target = &g_config.templates[index];

		copy_wstr(target->label, TEMPLATE_LABEL_MAX, fallback_template_tbl[index].label);
		target->body = _wcsdup(fallback_template_tbl[index].body);
		g_config.template_count = index + 1;
	}
}

static wchar_t *read_utf8_file(const wchar_t *path)
{
	HANDLE file_handle;
	DWORD file_size;
	DWORD bytes_read = 0;
	BOOL read_ok;
	char *raw_bytes;
	const char *utf8_text;
	wchar_t *wide_text = NULL;
	int wide_chars;

	file_handle = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	if (file_handle == INVALID_HANDLE_VALUE)
		return NULL;

	file_size = GetFileSize(file_handle, NULL);

	if (file_size == INVALID_FILE_SIZE || file_size > TEMPLATE_FILE_MAX) {
		CloseHandle(file_handle);
		return NULL;
	}

	raw_bytes = malloc(file_size + 1);

	if (!raw_bytes) {
		CloseHandle(file_handle);
		return NULL;
	}

	read_ok = ReadFile(file_handle, raw_bytes, file_size, &bytes_read, NULL);
	CloseHandle(file_handle);

	if (!read_ok) {
		free(raw_bytes);
		return NULL;
	}

	raw_bytes[bytes_read] = 0;
	utf8_text = raw_bytes;
	if (bytes_read >= 3 && (unsigned char)utf8_text[0] == 0xEF && (unsigned char)utf8_text[1] == 0xBB && (unsigned char)utf8_text[2] == 0xBF)
		utf8_text += 3;

	wide_chars = MultiByteToWideChar(CP_UTF8, 0, utf8_text, -1, NULL, 0);
	if (wide_chars > 0)
		wide_text = malloc(wide_chars * sizeof(wchar_t));

	if (wide_text)
		MultiByteToWideChar(CP_UTF8, 0, utf8_text, -1, wide_text, wide_chars);

	free(raw_bytes);

	return wide_text;
}

static BOOL parse_template_header(const wchar_t *line, size_t line_length, wchar_t *label_out)
{
	size_t label_length;

	while (line_length && (line[line_length - 1] == L' ' || line[line_length - 1] == L'\t'))
		line_length--;

	if (line_length < 3 || line[0] != L'[' || line[line_length - 1] != L']')
		return FALSE;

	label_length = line_length - 2;
	if (label_length >= TEMPLATE_LABEL_MAX)
		label_length = TEMPLATE_LABEL_MAX - 1;

	wmemcpy(label_out, line + 1, label_length);
	label_out[label_length] = 0;

	return TRUE;
}

static void finish_current_template(struct wide_buf *body_buf)
{
	wchar_t *body;

	if (g_config.template_count <= 0) {
		wide_buf_free(body_buf);
		return;
	}

	while (body_buf->length && wcschr(L" \t\n", body_buf->chars[body_buf->length - 1]))
		body_buf->chars[--body_buf->length] = 0;

	body = wide_buf_detach(body_buf);
	g_config.templates[g_config.template_count - 1].body = body ? body : _wcsdup(L"");
}

static void append_template_line(struct wide_buf *body_buf, const wchar_t *content, size_t content_length)
{

	if (content_length >= 2 && content[0] == L'\\' && content[1] == L'[') {
		content++;
		content_length--;
	}

	if (!body_buf->length && !content_length)
		return;

	wide_buf_append_chars(body_buf, content, content_length);
	wide_buf_append(body_buf, L"\n");
}

static void load_templates(void)
{
	struct wide_buf body_buf;
	wchar_t label[TEMPLATE_LABEL_MAX];
	wchar_t *file_text;
	const wchar_t *line_start;
	const wchar_t *line_end;
	size_t line_length;

	ZeroMemory(&body_buf, sizeof(body_buf));
	free_templates();
	g_config.template_error[0] = 0;

	file_text = read_utf8_file(g_config.template_path);

	if (!file_text) {
		format_wstr(g_config.template_error, TEMPLATE_ERROR_MAX, L"(Cannot read %ls - using built-in templates)", g_config.template_path);
		hub_log(L"ERROR: cannot read templates '%ls' (%lu)", g_config.template_path, GetLastError());
		load_fallback_templates();
		return;
	}

	line_start = file_text;
	while (*line_start) {
		line_end = wcschr(line_start, L'\n');
		line_length = line_end ? (size_t)(line_end - line_start) : wcslen(line_start);
		if (line_length && line_start[line_length - 1] == L'\r')
			line_length--;

		if (parse_template_header(line_start, line_length, label)) {
			finish_current_template(&body_buf);

			if (g_config.template_count >= TEMPLATE_MAX) {
				hub_log(L"WARN: more than %d templates, rest ignored", TEMPLATE_MAX);
				break;
			}

			copy_wstr(g_config.templates[g_config.template_count].label, TEMPLATE_LABEL_MAX, label);
			g_config.templates[g_config.template_count].body = NULL;
			g_config.template_count++;
		}
		else if (g_config.template_count > 0) {
			append_template_line(&body_buf, line_start, line_length);
		}

		if (!line_end)
			break;

		line_start = line_end + 1;
	}

	finish_current_template(&body_buf);
	free(file_text);

	if (g_config.template_count == 0) {
		format_wstr(g_config.template_error, TEMPLATE_ERROR_MAX, L"(No [label] section in %ls - using built-in templates)", g_config.template_path);
		load_fallback_templates();
	}

	hub_log(L"loaded %d template(s) from '%ls'", g_config.template_count, g_config.template_path);
}

static BOOL is_absolute_path(const wchar_t *path)
{
	return (path[0] && path[1] == L':') || (path[0] == L'\\' && path[1] == L'\\');
}

static void load_config(void)
{
	wchar_t ini_path[MAX_PATH];
	wchar_t template_setting[MAX_PATH];

	format_wstr(ini_path, MAX_PATH, INI_PATH_FMT, g_exe_dir);

	g_config.log_enabled = GetPrivateProfileIntW(INI_SECTION_GENERAL, INI_KEY_LOG, default_log_enabled, ini_path);
	g_config.tray_icon_enabled = GetPrivateProfileIntW(INI_SECTION_GENERAL, INI_KEY_TRAY_ICON, default_tray_icon, ini_path);
	g_config.copy_to_clipboard = GetPrivateProfileIntW(INI_SECTION_TARGET, INI_KEY_CLIPBOARD, default_copy_to_clipboard, ini_path);
	g_config.dry_run = GetPrivateProfileIntW(INI_SECTION_TARGET, INI_KEY_DRY_RUN, default_dry_run, ini_path);
	GetPrivateProfileStringW(INI_SECTION_PATHS, INI_KEY_WIN_PREFIX, default_win_prefix, g_config.win_prefix, MAX_PATH, ini_path);
	GetPrivateProfileStringW(INI_SECTION_PATHS, INI_KEY_LINUX_PREFIX, default_linux_prefix, g_config.linux_prefix, MAX_PATH, ini_path);
	GetPrivateProfileStringW(INI_SECTION_TARGET, INI_KEY_URI, default_uri_template, g_config.uri_template, URI_TEMPLATE_MAX, ini_path);
	GetPrivateProfileStringW(INI_SECTION_GENERAL, INI_KEY_TEMPLATES, default_template_path, template_setting, MAX_PATH, ini_path);

	if (is_absolute_path(template_setting))
		copy_wstr(g_config.template_path, MAX_PATH, template_setting);
	else
		format_wstr(g_config.template_path, MAX_PATH, EXE_DIR_FILE_PATH_FMT, g_exe_dir, template_setting);

	load_templates();
}

static void replace_char(wchar_t *text, wchar_t from_char, wchar_t to_char)
{

	for (; *text; text++) {
		if (*text == from_char)
			*text = to_char;
	}
}

static void map_to_linux_path(const wchar_t *windows_path, wchar_t *linux_path_out, size_t out_chars)
{
	wchar_t normalized_path[REQ_FILE_MAX];
	wchar_t normalized_prefix[MAX_PATH];
	const wchar_t *relative_part;
	size_t prefix_length;
	size_t linux_prefix_length;
	BOOL need_separator;

	copy_wstr(normalized_path, REQ_FILE_MAX, windows_path);
	copy_wstr(normalized_prefix, MAX_PATH, g_config.win_prefix);
	replace_char(normalized_path, L'/', L'\\');
	replace_char(normalized_prefix, L'/', L'\\');
	prefix_length = wcslen(normalized_prefix);

	if (prefix_length && g_config.linux_prefix[0] && _wcsnicmp(normalized_path, normalized_prefix, prefix_length) == 0) {
		relative_part = normalized_path + prefix_length;
		while (*relative_part == L'\\')
			relative_part++;

		linux_prefix_length = wcslen(g_config.linux_prefix);
		need_separator = linux_prefix_length && g_config.linux_prefix[linux_prefix_length - 1] != L'/';
		format_wstr(linux_path_out, out_chars, L"%ls%ls%ls", g_config.linux_prefix, need_separator ? L"/" : L"", relative_part);
		replace_char(linux_path_out, L'\\', L'/');
		return;
	}

	hub_log(L"WARN: no path mapping for '%ls' (win_prefix='%ls', linux_prefix='%ls')", windows_path, g_config.win_prefix, g_config.linux_prefix);
	copy_wstr(linux_path_out, out_chars, normalized_path);
	replace_char(linux_path_out, L'\\', L'/');
}

static const wchar_t *path_basename(const wchar_t *path)
{
	const wchar_t *last_backslash = wcsrchr(path, L'\\');
	const wchar_t *last_slash = wcsrchr(path, L'/');
	const wchar_t *last_separator = last_slash > last_backslash ? last_slash : last_backslash;

	return last_separator ? last_separator + 1 : path;
}

static void build_prompt(const struct prompt_template *prompt_template, const struct hub_request *request, struct wide_buf *prompt_out)
{
	wchar_t linux_path[REQ_FILE_MAX];
	wchar_t first_line_text[LINE_NUMBER_TEXT_MAX];
	wchar_t last_line_text[LINE_NUMBER_TEXT_MAX];
	const wchar_t *symbol_text = request->symbol[0] ? request->symbol : missing_symbol_text;
	struct
	{
		const wchar_t *placeholder;
		const wchar_t *value;
	} _substitutions[] =
	{
		{
			.placeholder = L"{path}",
			.value = linux_path,
		},
		{
			.placeholder = L"{winpath}",
			.value = request->file,
		},
		{
			.placeholder = L"{file}",
			.value = path_basename(request->file),
		},
		{
			.placeholder = L"{symbol}",
			.value = symbol_text,
		},
		{
			.placeholder = L"{line1}",
			.value = first_line_text,
		},
		{
			.placeholder = L"{line2}",
			.value = last_line_text,
		},
		{
			.placeholder = L"{line}",
			.value = first_line_text,
		},
	};
	const wchar_t *cursor;
	size_t placeholder_length;
	size_t index;
	BOOL replaced;

	map_to_linux_path(request->file, linux_path, REQ_FILE_MAX);
	format_wstr(first_line_text, LINE_NUMBER_TEXT_MAX, L"%d", request->first_line);
	format_wstr(last_line_text, LINE_NUMBER_TEXT_MAX, L"%d", request->last_line);

	cursor = prompt_template->body;
	while (*cursor) {
		replaced = FALSE;

		if (*cursor == L'{') {
			for (index = 0; index < ARRAYSIZE(_substitutions); index++) {
				placeholder_length = wcslen(_substitutions[index].placeholder);

				if (wcsncmp(cursor, _substitutions[index].placeholder, placeholder_length) == 0) {
					wide_buf_append(prompt_out, _substitutions[index].value);
					cursor += placeholder_length;
					replaced = TRUE;
					break;
				}
			}
		}

		if (!replaced)
			wide_buf_append_chars(prompt_out, cursor++, 1);
	}
}

static BOOL is_url_unreserved(unsigned char byte)
{
	return (byte >= 'A' && byte <= 'Z') || (byte >= 'a' && byte <= 'z') || (byte >= '0' && byte <= '9') || byte == '-' || byte == '_' || byte == '.' || byte == '~';
}

static void append_url_encoded(struct wide_buf *output, const wchar_t *text)
{
	const unsigned char *byte;
	wchar_t plain_char;
	wchar_t escaped[3];
	char *utf8_text;
	int utf8_size;

	utf8_size = WideCharToMultiByte(CP_UTF8, 0, text, -1, NULL, 0, NULL, NULL);
	if (utf8_size <= 0)
		return;

	utf8_text = malloc(utf8_size);
	if (!utf8_text)
		return;

	WideCharToMultiByte(CP_UTF8, 0, text, -1, utf8_text, utf8_size, NULL, NULL);

	for (byte = (const unsigned char *)utf8_text; *byte; byte++) {
		if (is_url_unreserved(*byte)) {
			plain_char = (wchar_t)*byte;
			wide_buf_append_chars(output, &plain_char, 1);
		}
		else {
			escaped[0] = L'%';
			escaped[1] = hex_digit_tbl[*byte >> 4];
			escaped[2] = hex_digit_tbl[*byte & 0x0F];
			wide_buf_append_chars(output, escaped, 3);
		}
	}

	free(utf8_text);
}

static void build_uri(const wchar_t *prompt, struct wide_buf *uri_out)
{
	const wchar_t *placeholder = wcsstr(g_config.uri_template, PROMPT_PLACEHOLDER);

	if (!placeholder) {
		wide_buf_append(uri_out, g_config.uri_template);
		return;
	}

	wide_buf_append_chars(uri_out, g_config.uri_template, (size_t)(placeholder - g_config.uri_template));
	append_url_encoded(uri_out, prompt);
	wide_buf_append(uri_out, placeholder + wcslen(PROMPT_PLACEHOLDER));
}

static void set_clipboard_text(HWND owner_window, const wchar_t *text)
{
	size_t byte_count = (wcslen(text) + 1) * sizeof(wchar_t);
	HGLOBAL memory_handle;
	int attempt;

	for (attempt = 0; attempt < CLIPBOARD_RETRIES; attempt++) {
		if (!OpenClipboard(owner_window)) {
			Sleep(CLIPBOARD_DELAY_MS);
			continue;
		}

		EmptyClipboard();
		memory_handle = GlobalAlloc(GMEM_MOVEABLE, byte_count);

		if (memory_handle) {
			memcpy(GlobalLock(memory_handle), text, byte_count);
			GlobalUnlock(memory_handle);
			if (!SetClipboardData(CF_UNICODETEXT, memory_handle))
				GlobalFree(memory_handle);
		}

		CloseClipboard();
		return;
	}

	hub_log(L"WARN: OpenClipboard failed (%lu)", GetLastError());
}

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

static int pick_template(HWND hub_window, const struct hub_request *request, POINT anchor, HWND previous_foreground)
{
	wchar_t header_text[MENU_HEADER_MAX];
	wchar_t item_text[MENU_ITEM_MAX];
	HMENU menu;
	int selected_id;
	int template_index;
	int index;

	if (request->auto_pick >= 1 && request->auto_pick <= g_config.template_count)
		return request->auto_pick - 1;

	menu = CreatePopupMenu();
	if (!menu)
		return -1;

	format_wstr(header_text, MENU_HEADER_MAX, L"%ls:%d  %ls", path_basename(request->file), request->first_line, request->symbol);
	AppendMenuW(menu, MF_STRING | MF_GRAYED, MENU_ID_HEADER, header_text);
	AppendMenuW(menu, MF_SEPARATOR, 0, NULL);
	if (g_config.template_error[0])
		AppendMenuW(menu, MF_STRING | MF_GRAYED, MENU_ID_ERROR, g_config.template_error);

	for (index = 0; index < g_config.template_count; index++) {
		format_wstr(item_text, MENU_ITEM_MAX, L"&%d  %ls", index + 1, g_config.templates[index].label);
		AppendMenuW(menu, MF_STRING, MENU_ID_TEMPLATE_BASE + index, item_text);
	}

	SetForegroundWindow(hub_window);
	selected_id = (int)TrackPopupMenuEx(menu, TPM_RETURNCMD | TPM_NONOTIFY | TPM_LEFTALIGN | TPM_TOPALIGN | TPM_RIGHTBUTTON, anchor.x, anchor.y, hub_window, NULL);
	PostMessageW(hub_window, WM_NULL, 0, 0);
	DestroyMenu(menu);

	template_index = selected_id - MENU_ID_TEMPLATE_BASE;

	if (template_index < 0 || template_index >= g_config.template_count) {
		hub_log(L"menu cancelled");
		if (previous_foreground && IsWindow(previous_foreground))
			SetForegroundWindow(previous_foreground);

		return -1;
	}

	return template_index;
}

static void show_error_box(const wchar_t *format, ...)
{
	wchar_t error_text[ERROR_TEXT_MAX];
	va_list args;

	va_start(args, format);
	_vsnwprintf(error_text, ERROR_TEXT_MAX - 1, format, args);
	va_end(args);
	error_text[ERROR_TEXT_MAX - 1] = 0;

	MessageBoxW(NULL, error_text, HUB_TITLE, MB_ICONWARNING | MB_SETFOREGROUND);
}

static void handle_request(HWND hub_window, const struct hub_request *request)
{
	struct wide_buf prompt;
	struct wide_buf uri;
	const struct prompt_template *chosen;
	HWND previous_foreground;
	POINT anchor;
	int template_index;
	int shell_code;

	ZeroMemory(&prompt, sizeof(prompt));
	ZeroMemory(&uri, sizeof(uri));
	load_config();
	previous_foreground = GetForegroundWindow();
	anchor = get_menu_anchor(previous_foreground);
	hub_log(L"request file='%ls' line=%d-%d symbol='%ls'", request->file, request->first_line, request->last_line, request->symbol);

	template_index = pick_template(hub_window, request, anchor, previous_foreground);
	if (template_index < 0)
		return;

	chosen = &g_config.templates[template_index];
	build_prompt(chosen, request, &prompt);

	if (!prompt.chars) {
		hub_log(L"WARN: template %d '%ls' is empty", template_index + 1, chosen->label);
		return;
	}

	if (g_config.copy_to_clipboard)
		set_clipboard_text(hub_window, prompt.chars);

	build_uri(prompt.chars, &uri);
	hub_log(L"template=%d '%ls' uri_length=%u", template_index + 1, chosen->label, (unsigned)uri.length);
	hub_log(L"prompt=%ls", prompt.chars);
	hub_log(L"uri=%ls", uri.chars);

	if (!g_config.dry_run) {
		shell_code = (int)(INT_PTR)ShellExecuteW(NULL, L"open", uri.chars, NULL, NULL, SW_SHOWNORMAL);

		if (shell_code <= 32) {
			hub_log(L"ERROR: ShellExecute failed code=%d", shell_code);
			show_error_box(L"Cannot open target URI (ShellExecute=%d).\n\n"
						   L"Check that VS Code is installed and the vscode:// protocol is registered.\n"
						   L"Prompt was %lscopied to the clipboard.",
						   shell_code, g_config.copy_to_clipboard ? L"" : L"NOT ");
		}
	}

	wide_buf_free(&prompt);
	wide_buf_free(&uri);
}

static void fill_tray_data(HWND hub_window, NOTIFYICONDATAW *tray_data)
{
	ZeroMemory(tray_data, sizeof(*tray_data));
	tray_data->cbSize = sizeof(*tray_data);
	tray_data->hWnd = hub_window;
	tray_data->uID = TRAY_ICON_ID;
}

static void add_tray_icon(HWND hub_window)
{
	NOTIFYICONDATAW tray_data;

	if (!g_config.tray_icon_enabled || g_tray_icon_added)
		return;

	fill_tray_data(hub_window, &tray_data);
	tray_data.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
	tray_data.uCallbackMessage = WM_HUB_TRAY;
	tray_data.hIcon = (HICON)LoadImageW(GetModuleHandleW(NULL), MAKEINTRESOURCEW(IDI_SIHUB), IMAGE_ICON,
										GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON), LR_DEFAULTCOLOR);
	if (!tray_data.hIcon)
		tray_data.hIcon = LoadIconW(NULL, (LPCWSTR)IDI_APPLICATION);

	format_wstr(tray_data.szTip, TRAY_TIP_MAX, L"%ls v%ls", HUB_TITLE, HUB_VERSION);
	g_tray_icon_added = Shell_NotifyIconW(NIM_ADD, &tray_data);
	if (!g_tray_icon_added)
		hub_log(L"WARN: Shell_NotifyIcon(NIM_ADD) failed");
}

static void remove_tray_icon(HWND hub_window)
{
	NOTIFYICONDATAW tray_data;

	if (!g_tray_icon_added)
		return;

	fill_tray_data(hub_window, &tray_data);
	Shell_NotifyIconW(NIM_DELETE, &tray_data);
	g_tray_icon_added = FALSE;
}

static void open_with_shell(const wchar_t *path)
{
	int shell_code = (int)(INT_PTR)ShellExecuteW(NULL, L"open", path, NULL, NULL, SW_SHOWNORMAL);

	if (shell_code <= 32)
		show_error_box(L"Cannot open:\n%ls\n\n(ShellExecute=%d)", path, shell_code);
}

static void open_exe_dir_path(const wchar_t *path_fmt)
{
	wchar_t full_path[MAX_PATH];

	format_wstr(full_path, MAX_PATH, path_fmt, g_exe_dir);
	open_with_shell(full_path);
}

static void show_tray_menu(HWND hub_window)
{
	wchar_t title_text[TRAY_TIP_MAX];
	POINT mouse_position;
	HMENU menu;
	int selected_id;

	load_config();
	menu = CreatePopupMenu();
	if (!menu)
		return;

	format_wstr(title_text, TRAY_TIP_MAX, L"%ls v%ls", HUB_TITLE, HUB_VERSION);
	AppendMenuW(menu, MF_STRING | MF_GRAYED, TRAY_ID_TITLE, title_text);
	AppendMenuW(menu, MF_SEPARATOR, 0, NULL);
	AppendMenuW(menu, MF_STRING, TRAY_ID_EDIT_TEMPLATES, L"Edit &templates");
	AppendMenuW(menu, MF_STRING, TRAY_ID_EDIT_SETTINGS, L"Edit &settings");
	AppendMenuW(menu, MF_STRING | (g_config.log_enabled ? 0 : MF_GRAYED), TRAY_ID_OPEN_LOG, L"Open &log");
	AppendMenuW(menu, MF_STRING, TRAY_ID_OPEN_FOLDER, L"Open &folder");
	AppendMenuW(menu, MF_SEPARATOR, 0, NULL);
	AppendMenuW(menu, MF_STRING, TRAY_ID_EXIT, L"E&xit");
	SetMenuDefaultItem(menu, TRAY_ID_EDIT_TEMPLATES, FALSE);

	GetCursorPos(&mouse_position);
	SetForegroundWindow(hub_window);
	selected_id = (int)TrackPopupMenuEx(menu, TPM_RETURNCMD | TPM_NONOTIFY | TPM_RIGHTBUTTON | TPM_BOTTOMALIGN, mouse_position.x, mouse_position.y, hub_window, NULL);
	PostMessageW(hub_window, WM_NULL, 0, 0);
	DestroyMenu(menu);

	switch (selected_id) {
	case TRAY_ID_EDIT_TEMPLATES: open_with_shell(g_config.template_path); break;
	case TRAY_ID_EDIT_SETTINGS: open_exe_dir_path(INI_PATH_FMT); break;
	case TRAY_ID_OPEN_LOG: open_exe_dir_path(LOG_PATH_FMT); break;
	case TRAY_ID_OPEN_FOLDER: open_with_shell(g_exe_dir); break;
	case TRAY_ID_EXIT: PostMessageW(hub_window, WM_CLOSE, 0, 0); break;
	default: break;
	}
}

static BOOL accept_copy_data(HWND window, const COPYDATASTRUCT *copy_data)
{

	if (!copy_data || copy_data->dwData != HUB_COPYDATA_MAGIC || copy_data->cbData != sizeof(struct hub_request))
		return FALSE;

	memcpy(&g_pending_request, copy_data->lpData, sizeof(struct hub_request));
	g_pending_request.file[REQ_FILE_MAX - 1] = 0;
	g_pending_request.symbol[REQ_SYMBOL_MAX - 1] = 0;
	g_has_pending_request = TRUE;
	PostMessageW(window, WM_HUB_SHOWMENU, 0, 0);

	return TRUE;
}

static LRESULT CALLBACK hub_window_proc(HWND window, UINT message, WPARAM w_param, LPARAM l_param)
{
	struct hub_request request;

	switch (message) {
	case WM_COPYDATA:
		return accept_copy_data(window, (const COPYDATASTRUCT *)l_param);

	case WM_HUB_SHOWMENU:
		if (!g_has_pending_request)
			return 0;

		request = g_pending_request;
		g_has_pending_request = FALSE;
		handle_request(window, &request);

		return 0;

	case WM_HUB_TRAY:
		if (LOWORD(l_param) == WM_RBUTTONUP || LOWORD(l_param) == WM_CONTEXTMENU)
			show_tray_menu(window);
		else if (LOWORD(l_param) == WM_LBUTTONDBLCLK)
			open_with_shell(g_config.template_path);

		return 0;

	case WM_CLOSE:
		hub_log(L"server quit");
		DestroyWindow(window);

		return 0;

	case WM_DESTROY:
		remove_tray_icon(window);
		PostQuitMessage(0);

		return 0;

	default:
		break;
	}

	if (g_taskbar_created_msg && message == g_taskbar_created_msg) {
		g_tray_icon_added = FALSE;
		add_tray_icon(window);
		return 0;
	}

	return DefWindowProcW(window, message, w_param, l_param);
}

static void parse_command_line(struct cmd_args *args)
{
	LPWSTR *arg_list;
	const wchar_t *option;
	const wchar_t *value;
	int arg_count = 0;
	int index;

	ZeroMemory(args, sizeof(*args));
	arg_list = CommandLineToArgvW(GetCommandLineW(), &arg_count);
	if (!arg_list)
		return;

	for (index = 1; index < arg_count; index++) {
		option = arg_list[index];
		value = (index + 1 < arg_count) ? arg_list[index + 1] : NULL;

		if (!wcscmp(option, L"--quit")) {
			args->quit_requested = TRUE;
			continue;
		}

		if (!value)
			break;

		if (!wcscmp(option, L"--file")) {
			copy_wstr(args->request.file, REQ_FILE_MAX, value);
			args->has_request = TRUE;
		}
		else if (!wcscmp(option, L"--symbol")) {
			copy_wstr(args->request.symbol, REQ_SYMBOL_MAX, value);
		}
		else if (!wcscmp(option, L"--line")) {
			args->request.first_line = _wtoi(value);
		}
		else if (!wcscmp(option, L"--line2")) {
			args->request.last_line = _wtoi(value);
		}
		else if (!wcscmp(option, L"--pick")) {
			args->request.auto_pick = _wtoi(value);
		}
		else {
			continue;
		}

		index++;
	}

	if (args->request.first_line < 1)
		args->request.first_line = 1;

	if (args->request.last_line < args->request.first_line)
		args->request.last_line = args->request.first_line;

	LocalFree(arg_list);
}

static int run_client(const struct cmd_args *args)
{
	COPYDATASTRUCT copy_data;
	HWND server_window = NULL;
	DWORD server_process_id = 0;
	DWORD_PTR message_result = 0;
	int attempt;

	for (attempt = 0; attempt < SERVER_FIND_RETRIES && !server_window; attempt++) {
		server_window = FindWindowW(HUB_WINDOW_CLASS, NULL);
		if (!server_window)
			Sleep(SERVER_FIND_DELAY_MS);
	}

	if (!server_window) {
		hub_log(L"ERROR: server window not found");
		return 2;
	}

	if (args->quit_requested) {
		PostMessageW(server_window, WM_CLOSE, 0, 0);
		return 0;
	}

	if (!args->has_request)
		return 0;

	GetWindowThreadProcessId(server_window, &server_process_id);
	AllowSetForegroundWindow(server_process_id);

	copy_data.dwData = HUB_COPYDATA_MAGIC;
	copy_data.cbData = sizeof(struct hub_request);
	copy_data.lpData = (PVOID)&args->request;

	if (!SendMessageTimeoutW(server_window, WM_COPYDATA, 0, (LPARAM)&copy_data, SMTO_ABORTIFHUNG, COPYDATA_TIMEOUT_MS, &message_result) || !message_result) {
		hub_log(L"ERROR: WM_COPYDATA failed (%lu)", GetLastError());
		return 3;
	}

	return 0;
}

static int run_server(HINSTANCE instance, const struct cmd_args *args)
{
	WNDCLASSEXW window_class;
	HWND hub_window;
	MSG message;

	ZeroMemory(&window_class, sizeof(window_class));
	window_class.cbSize = sizeof(window_class);
	window_class.lpfnWndProc = hub_window_proc;
	window_class.hInstance = instance;
	window_class.lpszClassName = HUB_WINDOW_CLASS;
	window_class.hIcon = LoadIconW(instance, MAKEINTRESOURCEW(IDI_SIHUB));
	if (!RegisterClassExW(&window_class))
		return 1;

	g_taskbar_created_msg = RegisterWindowMessageW(L"TaskbarCreated");
	hub_window = CreateWindowExW(WS_EX_TOOLWINDOW, HUB_WINDOW_CLASS, HUB_TITLE, WS_POPUP, 0, 0, 0, 0, NULL, NULL, instance, NULL);
	if (!hub_window)
		return 1;

	hub_log(L"server start v%ls", HUB_VERSION);
	add_tray_icon(hub_window);

	if (args->has_request) {
		g_pending_request = args->request;
		g_has_pending_request = TRUE;
		PostMessageW(hub_window, WM_HUB_SHOWMENU, 0, 0);
	}

	while (GetMessageW(&message, NULL, 0, 0) > 0) {
		TranslateMessage(&message);
		DispatchMessageW(&message);
	}

	return 0;
}

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE prev_instance, LPWSTR cmd_line, int show_cmd)
{
	struct cmd_args args;
	HANDLE single_instance_mutex;
	wchar_t *last_backslash;
	BOOL server_already_running;
	int exit_code = 0;

	(void)prev_instance;
	(void)cmd_line;
	(void)show_cmd;

	SetProcessDPIAware();
	GetModuleFileNameW(NULL, g_exe_dir, MAX_PATH);
	last_backslash = wcsrchr(g_exe_dir, L'\\');
	if (last_backslash)
		*last_backslash = 0;

	load_config();
	parse_command_line(&args);

	single_instance_mutex = CreateMutexW(NULL, TRUE, HUB_MUTEX_NAME);
	server_already_running = (GetLastError() == ERROR_ALREADY_EXISTS);

	if (server_already_running)
		exit_code = run_client(&args);
	else if (!args.quit_requested)
		exit_code = run_server(instance, &args);

	if (single_instance_mutex) {
		if (!server_already_running)
			ReleaseMutex(single_instance_mutex);

		CloseHandle(single_instance_mutex);
	}

	return exit_code;
}
