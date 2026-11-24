#include <stdlib.h>
#include <string.h>
#include "sshui.h"
#include "config.h"
#include "reply.h"
#include "winutil.h"
#include "wstr.h"

#define SSHUI_TEST_COMMAND          "uname -a; echo \"shell: $SHELL\"; command -v tmux >/dev/null && tmux -V || echo 'tmux: not installed'"

#define MENU_ID_CONNECT_NEW         300
#define MENU_ID_CONNECTION_BASE     400
#define MENU_CONNECTION_STRIDE      10
#define MENU_ID_CONNECTION_LAST     (MENU_ID_CONNECTION_BASE + SSHCONN_MAX * MENU_CONNECTION_STRIDE - 1)

#define CONNECTION_LABEL_MAX        (SSHCONN_NAME_MAX + 32)
#define HOST_TEXT_MAX               300
#define FINGERPRINT_TEXT_MAX        100
#define USER_TEXT_MAX               64
#define MESSAGE_TEXT_MAX            1024
#define PASSWORD_INPUT_MAX          256
#define OUTPUT_TEXT_MAX             8192
#define REMOTE_FILE_PATH_FMT        L"%ls\\%ls"
#define REMOTE_FILE_MAX             (256 * 1024)
#define INSTALL_RUN_COMMAND         "sh \"$HOME/.sihub/install.sh\""
#define NOTICE_TEXT_FMT             L"%ls\n%ls"

enum connection_action
{
	CONNECTION_ACTION_CONNECT,
	CONNECTION_ACTION_DISCONNECT,
	CONNECTION_ACTION_TEST,
	CONNECTION_ACTION_REMOVE,
	CONNECTION_ACTION_INSTALL_HOOK,
};

static const struct
{
	const wchar_t *file_name;
	const char *upload_command;
} remote_file_tbl[] =
{
	{
		.file_name = L"stop_hook.sh",
		.upload_command = "mkdir -p \"$HOME/.sihub\" && cat > \"$HOME/.sihub/stop_hook.sh\"",
	},
	{
		.file_name = L"last_reply.py",
		.upload_command = "mkdir -p \"$HOME/.sihub\" && cat > \"$HOME/.sihub/last_reply.py\"",
	},
	{
		.file_name = L"install.sh",
		.upload_command = "mkdir -p \"$HOME/.sihub\" && cat > \"$HOME/.sihub/install.sh\"",
	},
};

static const wchar_t *const connection_label_fmt_tbl[] =
{
	[SSHCONN_DISCONNECTED] = L"\x25CB  %ls",
	[SSHCONN_CONNECTING] = L"\x25D4  %ls  (connecting...)",
	[SSHCONN_CONNECTED] = L"\x25CF  %ls",
	[SSHCONN_FAILED] = L"\x25CB  %ls  (failed)",
};

static BOOL g_prompt_active;

BOOL sshui_prompt_active(void)
{
	return g_prompt_active;
}

static UINT connection_command(int slot, enum connection_action action)
{
	return MENU_ID_CONNECTION_BASE + slot * MENU_CONNECTION_STRIDE + action;
}

static HMENU create_connection_menu(int slot, const struct sshconn_info *info)
{
	BOOL active = info->state == SSHCONN_CONNECTING || info->state == SSHCONN_CONNECTED;
	HMENU menu;

	menu = CreatePopupMenu();
	if (!menu)
		return NULL;

	if (active)
		AppendMenuW(menu, MF_STRING, connection_command(slot, CONNECTION_ACTION_DISCONNECT), L"&Disconnect");
	else
		AppendMenuW(menu, MF_STRING, connection_command(slot, CONNECTION_ACTION_CONNECT), L"&Connect");

	AppendMenuW(menu, MF_STRING | (info->state == SSHCONN_CONNECTED ? 0 : MF_GRAYED), connection_command(slot, CONNECTION_ACTION_TEST), L"Run &test command");
	AppendMenuW(menu, MF_STRING | (info->state == SSHCONN_CONNECTED ? 0 : MF_GRAYED), connection_command(slot, CONNECTION_ACTION_INSTALL_HOOK), L"Install reply &hook");
	AppendMenuW(menu, MF_SEPARATOR, 0, NULL);
	AppendMenuW(menu, MF_STRING | (active ? MF_GRAYED : 0), connection_command(slot, CONNECTION_ACTION_REMOVE), L"&Remove");

	return menu;
}

HMENU sshui_create_connections_menu(void)
{
	struct sshconn_info info;
	wchar_t label[CONNECTION_LABEL_MAX];
	HMENU menu;
	HMENU connection_menu;
	int listed = 0;
	int slot;

	menu = CreatePopupMenu();
	if (!menu)
		return NULL;

	for (slot = 0; slot < SSHCONN_MAX; slot++) {
		if (!sshconn_get_info(slot, &info))
			continue;

		connection_menu = create_connection_menu(slot, &info);
		if (!connection_menu)
			continue;

		wstr_format(label, CONNECTION_LABEL_MAX, connection_label_fmt_tbl[info.state], info.name);
		AppendMenuW(menu, MF_POPUP | MF_STRING, (UINT_PTR)connection_menu, label);
		listed++;
	}

	if (listed)
		AppendMenuW(menu, MF_SEPARATOR, 0, NULL);

	AppendMenuW(menu, MF_STRING, MENU_ID_CONNECT_NEW, L"Connect &to...");

	return menu;
}

static void connect_new(HWND owner)
{
	wchar_t spec[SSHCONN_NAME_MAX];
	BOOL entered;
	int slot;

	spec[0] = 0;
	g_prompt_active = TRUE;
	entered = winutil_prompt_text(owner, L"Connect", L"SSH target (user@host or user@host:port):", FALSE, spec, SSHCONN_NAME_MAX);
	g_prompt_active = FALSE;
	if (!entered || !spec[0])
		return;

	slot = sshconn_add(spec);

	if (slot < 0) {
		winutil_show_error(L"Invalid target, or the connection list is full:\n%ls\n\nUse user@host or user@host:port.", spec);
		return;
	}

	sshconn_connect(slot);
}

static char *load_remote_file(const wchar_t *file_name)
{
	wchar_t path[MAX_PATH];
	wchar_t *wide_text;
	char *utf8_text;
	char *read_cursor;
	char *write_cursor;

	wstr_format(path, MAX_PATH, REMOTE_FILE_PATH_FMT, config_get()->remote_dir, file_name);
	wide_text = wstr_read_utf8_file(path, REMOTE_FILE_MAX);
	if (!wide_text)
		return NULL;

	utf8_text = wstr_to_utf8(wide_text);
	free(wide_text);
	if (!utf8_text)
		return NULL;

	for (read_cursor = utf8_text, write_cursor = utf8_text; *read_cursor; read_cursor++) {
		if (*read_cursor != '\r')
			*write_cursor++ = *read_cursor;
	}

	*write_cursor = 0;

	return utf8_text;
}

static void install_reply_hook(int slot, const struct sshconn_info *info)
{
	char *file_text;
	size_t index;
	BOOL queued;

	if (!winutil_confirm(NULL, L"Install the reply hook on %ls?\n\nUploads stop_hook.sh, last_reply.py and install.sh to ~/.sihub and adds a Stop hook to ~/.claude/settings.json (a backup is made).", info->name))
		return;

	for (index = 0; index < ARRAYSIZE(remote_file_tbl); index++) {
		file_text = load_remote_file(remote_file_tbl[index].file_name);
		if (!file_text) {
			winutil_show_error(L"Cannot read %ls\\%ls.\n\nCopy the remote folder next to sihub.exe.", config_get()->remote_dir, remote_file_tbl[index].file_name);
			return;
		}

		queued = sshconn_exec_async(slot, remote_file_tbl[index].upload_command, file_text, strlen(file_text), HUB_JOB_INSTALL_STEP);
		free(file_text);
		if (!queued) {
			winutil_show_error(L"%ls is not connected.", info->name);
			return;
		}
	}

	sshconn_exec_async(slot, INSTALL_RUN_COMMAND, NULL, 0, HUB_JOB_INSTALL_DONE);
}

static void run_connection_command(HWND owner, int slot, enum connection_action action)
{
	struct sshconn_info info;

	if (!sshconn_get_info(slot, &info))
		return;

	switch (action) {
	case CONNECTION_ACTION_CONNECT: sshconn_connect(slot); break;
	case CONNECTION_ACTION_DISCONNECT: sshconn_disconnect(slot); break;
	case CONNECTION_ACTION_TEST:
		if (!sshconn_exec_async(slot, SSHUI_TEST_COMMAND, NULL, 0, HUB_JOB_TEST))
			winutil_show_error(L"%ls is not connected.", info.name);

		break;

	case CONNECTION_ACTION_INSTALL_HOOK: install_reply_hook(slot, &info); break;
	case CONNECTION_ACTION_REMOVE:
		if (winutil_confirm(owner, L"Remove %ls from the connection list?", info.name))
			sshconn_remove(slot);

		break;

	default: break;
	}
}

BOOL sshui_run_command(HWND owner, int command_id)
{
	int offset;

	if (command_id == MENU_ID_CONNECT_NEW) {
		connect_new(owner);
		return TRUE;
	}

	if (command_id < MENU_ID_CONNECTION_BASE || command_id > MENU_ID_CONNECTION_LAST)
		return FALSE;

	offset = command_id - MENU_ID_CONNECTION_BASE;
	run_connection_command(owner, offset / MENU_CONNECTION_STRIDE, (enum connection_action)(offset % MENU_CONNECTION_STRIDE));

	return TRUE;
}

static void answer_hostkey(HWND owner, struct sshconn_prompt *prompt)
{
	wchar_t host_entry[HOST_TEXT_MAX];
	wchar_t fingerprint[FINGERPRINT_TEXT_MAX];

	wstr_from_utf8(prompt->host_entry, host_entry, HOST_TEXT_MAX);
	wstr_from_utf8(prompt->fingerprint, fingerprint, FINGERPRINT_TEXT_MAX);
	prompt->accepted = winutil_confirm(owner, L"The authenticity of host '%ls' can't be established.\n\nServer key fingerprint:\n%ls\n\nTrust this host and remember its key?", host_entry, fingerprint);
}

static void answer_password(HWND owner, struct sshconn_prompt *prompt)
{
	wchar_t user[USER_TEXT_MAX];
	wchar_t host[HOST_TEXT_MAX];
	wchar_t message[MESSAGE_TEXT_MAX];
	wchar_t password[PASSWORD_INPUT_MAX];

	wstr_from_utf8(prompt->user, user, USER_TEXT_MAX);
	wstr_from_utf8(prompt->host, host, HOST_TEXT_MAX);
	wstr_format(message, MESSAGE_TEXT_MAX, prompt->attempt > 1 ? L"Login failed, please try again.\n\nPassword for %ls@%ls:" : L"Password for %ls@%ls:", user, host);
	password[0] = 0;

	if (winutil_prompt_text(owner, L"SSH login", message, TRUE, password, PASSWORD_INPUT_MAX))
		prompt->accepted = WideCharToMultiByte(CP_UTF8, 0, password, -1, prompt->password_out, (int)prompt->password_size, NULL, NULL) > 0;

	SecureZeroMemory(password, sizeof(password));
}

void sshui_handle_prompt(HWND owner, struct sshconn_prompt *prompt)
{
	prompt->accepted = FALSE;
	if (sshconn_is_shutting_down())
		return;

	g_prompt_active = TRUE;
	if (prompt->type == SSHCONN_PROMPT_HOSTKEY)
		answer_hostkey(owner, prompt);
	else
		answer_password(owner, prompt);

	g_prompt_active = FALSE;
}

static void show_exec_result(const struct sshconn_event *event)
{
	wchar_t *output_text;

	if (!event->exec_ok) {
		winutil_show_error(L"Command failed on %ls:\n\n%ls", event->name, event->error);
		return;
	}

	output_text = malloc(OUTPUT_TEXT_MAX * sizeof(wchar_t));
	if (!output_text)
		return;

	wstr_from_utf8(event->output ? event->output : "", output_text, OUTPUT_TEXT_MAX);
	winutil_show_info(L"%ls  (exit code %d)\n\n%ls", event->name, event->exit_code, output_text);
	free(output_text);
}

static void set_notice(struct sshui_notice *notice_out, const wchar_t *title, const wchar_t *text)
{
	notice_out->show = TRUE;
	notice_out->opens_reply = FALSE;
	wstr_copy(notice_out->title, SSHUI_NOTICE_TITLE_MAX, title);
	wstr_copy(notice_out->text, SSHCONN_NAME_MAX, text);
}

static void set_notice_detail(struct sshui_notice *notice_out, const wchar_t *title, const wchar_t *name, const wchar_t *detail)
{
	wchar_t text[SSHCONN_NAME_MAX];

	wstr_format(text, SSHCONN_NAME_MAX, NOTICE_TEXT_FMT, name, detail);
	set_notice(notice_out, title, text);
}

void sshui_notice_for_timer(enum reply_status status, struct sshui_notice *notice_out)
{
	ZeroMemory(notice_out, sizeof(*notice_out));
	if (status == REPLY_TIMED_OUT)
		set_notice(notice_out, L"No reply from Claude", L"Stopped waiting. The reply can still be read in tmux.");
	else if (status == REPLY_FAILED)
		set_notice(notice_out, L"Reply check failed", L"See sihub.log (set [general] log=1).");
}

static void report_install(const struct sshconn_event *event)
{
	wchar_t *output_text;

	if (!event->exec_ok) {
		winutil_show_error(L"Reply hook install failed on %ls:\n\n%ls", event->name, event->error);
		return;
	}

	output_text = malloc(OUTPUT_TEXT_MAX * sizeof(wchar_t));
	if (!output_text)
		return;

	wstr_from_utf8(event->output ? event->output : "", output_text, OUTPUT_TEXT_MAX);
	if (event->exit_code == 0)
		winutil_show_info(L"%ls\n\n%ls", event->name, output_text);
	else
		winutil_show_error(L"Reply hook install failed on %ls (exit code %d):\n\n%ls", event->name, event->exit_code, output_text);

	free(output_text);
}

static void report_reply_check(HWND owner, const struct sshconn_event *event, struct sshui_notice *notice_out)
{
	enum reply_status status = reply_handle_check(owner, event);

	if (status == REPLY_RECEIVED) {
		set_notice_detail(notice_out, L"Claude replied", event->name, L"Click to open last_reply.md");
		notice_out->opens_reply = TRUE;
	}
	else {
		sshui_notice_for_timer(status, notice_out);
	}
}

static void report_tmux_send(HWND owner, const struct sshconn_event *event, struct sshui_notice *notice_out)
{
	wchar_t *output_text;

	if (!event->exec_ok) {
		if (event->state != SSHCONN_FAILED)
			winutil_show_error(L"Prompt was not sent to tmux on %ls:\n\n%ls\n\nThe prompt is on the clipboard if copy_to_clipboard=1.", event->name, event->error);

		return;
	}

	if (event->exit_code == 0) {
		switch (reply_begin(owner, event->slot, event->output)) {
		case REPLY_WAITING: set_notice_detail(notice_out, L"Sent to Claude (tmux)", event->name, L"Waiting for the reply..."); break;
		case REPLY_NO_HOOK: set_notice_detail(notice_out, L"Sent to Claude (tmux)", event->name, L"Reply hook not installed: Connections > Install reply hook"); break;
		default: set_notice(notice_out, L"Sent to Claude (tmux)", event->name); break;
		}

		return;
	}

	output_text = malloc(OUTPUT_TEXT_MAX * sizeof(wchar_t));
	if (!output_text)
		return;

	wstr_from_utf8(event->output ? event->output : "", output_text, OUTPUT_TEXT_MAX);
	winutil_show_error(L"Prompt was not sent to tmux on %ls (exit code %d):\n\n%ls", event->name, event->exit_code, output_text);
	free(output_text);
}

void sshui_handle_event(HWND owner, struct sshconn_event *event, struct sshui_notice *notice_out)
{
	ZeroMemory(notice_out, sizeof(*notice_out));
	if (!event)
		return;

	if (event->type == SSHCONN_EVENT_STATE) {
		if (event->state == SSHCONN_CONNECTED)
			set_notice(notice_out, L"Connected", event->name);
		else if (event->state == SSHCONN_DISCONNECTED)
			set_notice(notice_out, L"Disconnected", event->name);
		else if (event->state == SSHCONN_FAILED)
			winutil_show_error(L"SSH connection to %ls failed:\n\n%ls", event->name, event->error);
	}
	else if (event->type == SSHCONN_EVENT_EXEC && event->tag == HUB_JOB_TEST) {
		show_exec_result(event);
	}
	else if (event->type == SSHCONN_EVENT_EXEC && event->tag == HUB_JOB_TMUX_SEND) {
		report_tmux_send(owner, event, notice_out);
	}
	else if (event->type == SSHCONN_EVENT_EXEC && (event->tag & HUB_JOB_KIND_MASK) == HUB_JOB_REPLY_CHECK) {
		report_reply_check(owner, event, notice_out);
	}
	else if (event->type == SSHCONN_EVENT_EXEC && event->tag == HUB_JOB_INSTALL_STEP && (!event->exec_ok || event->exit_code != 0)) {
		report_install(event);
	}
	else if (event->type == SSHCONN_EVENT_EXEC && event->tag == HUB_JOB_INSTALL_DONE) {
		report_install(event);
	}

	sshconn_free_event(event);
}
