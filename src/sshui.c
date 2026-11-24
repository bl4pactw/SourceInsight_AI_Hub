#include <stdlib.h>
#include "sshui.h"
#include "winutil.h"
#include "wstr.h"

#define SSHUI_TAG_TEST              1
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

enum connection_action
{
	CONNECTION_ACTION_CONNECT,
	CONNECTION_ACTION_DISCONNECT,
	CONNECTION_ACTION_TEST,
	CONNECTION_ACTION_REMOVE,
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

static void run_connection_command(HWND owner, int slot, enum connection_action action)
{
	struct sshconn_info info;

	if (!sshconn_get_info(slot, &info))
		return;

	switch (action) {
	case CONNECTION_ACTION_CONNECT: sshconn_connect(slot); break;
	case CONNECTION_ACTION_DISCONNECT: sshconn_disconnect(slot); break;
	case CONNECTION_ACTION_TEST:
		if (!sshconn_exec_async(slot, SSHUI_TEST_COMMAND, NULL, 0, SSHUI_TAG_TEST))
			winutil_show_error(L"%ls is not connected.", info.name);

		break;

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
	wstr_copy(notice_out->title, SSHUI_NOTICE_TITLE_MAX, title);
	wstr_copy(notice_out->text, SSHCONN_NAME_MAX, text);
}

void sshui_handle_event(struct sshconn_event *event, struct sshui_notice *notice_out)
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
	else if (event->type == SSHCONN_EVENT_EXEC && event->tag == SSHUI_TAG_TEST) {
		show_exec_result(event);
	}

	sshconn_free_event(event);
}
