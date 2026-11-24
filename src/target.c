#include <stdlib.h>
#include <string.h>
#include "target.h"
#include "config.h"
#include "hub_log.h"
#include "sshconn.h"
#include "tmuxcmd.h"
#include "winutil.h"
#include "wstr.h"

static const wchar_t hex_digit_tbl[] = L"0123456789ABCDEF";

static const wchar_t *const target_name_tbl[] =
{
	[HUB_TARGET_DEFAULT] = L"default",
	[HUB_TARGET_VSCODE] = L"VS Code",
	[HUB_TARGET_TMUX] = L"tmux",
};

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

	utf8_text = wstr_to_utf8(text);
	if (!utf8_text)
		return;

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

static void build_uri(const wchar_t *uri_template, const wchar_t *prompt, struct wide_buf *uri_out)
{
	const wchar_t *placeholder = wcsstr(uri_template, URI_PROMPT_PLACEHOLDER);

	if (!placeholder) {
		wide_buf_append(uri_out, uri_template);
		return;
	}

	wide_buf_append_chars(uri_out, uri_template, (size_t)(placeholder - uri_template));
	append_url_encoded(uri_out, prompt);
	wide_buf_append(uri_out, placeholder + wcslen(URI_PROMPT_PLACEHOLDER));
}

static BOOL wide_to_utf8_buf(const wchar_t *text, char *dest, size_t dest_size)
{
	return WideCharToMultiByte(CP_UTF8, 0, text, -1, dest, (int)dest_size, NULL, NULL) > 0;
}

static BOOL send_to_vscode(const wchar_t *prompt, wchar_t *error_out, size_t error_chars)
{
	const struct hub_config *config = config_get();
	struct wide_buf uri;
	int shell_code = 0;
	BOOL sent = TRUE;

	wide_buf_init(&uri);
	build_uri(config->uri_template, prompt, &uri);
	hub_log(L"uri=%ls (length %u)", uri.chars, (unsigned)uri.length);

	if (!config->dry_run) {
		shell_code = winutil_shell_execute(uri.chars);
		sent = shell_code > SHELL_EXECUTE_ERROR_MAX;
	}

	if (!sent)
		wstr_format(error_out, error_chars, L"Cannot open target URI (ShellExecute=%d).\n\nCheck that VS Code is installed and the vscode:// protocol is registered.", shell_code);

	wide_buf_free(&uri);

	return sent;
}

static BOOL build_tmux_script(const struct hub_config *config, char *script_out, size_t script_size)
{
	char session[TMUX_SESSION_MAX];
	char allowed_commands[TMUX_COMMANDS_MAX];

	if (!wide_to_utf8_buf(config->tmux_session, session, TMUX_SESSION_MAX) || !wide_to_utf8_buf(config->tmux_allowed_commands, allowed_commands, TMUX_COMMANDS_MAX))
		return FALSE;

	return tmuxcmd_build_send(session, allowed_commands, config->tmux_submit != FALSE, config->tmux_submit_delay_ms, config->tmux_read_reply != FALSE, script_out, script_size);
}

static int prepare_connection(const wchar_t *connection_name)
{
	struct sshconn_info info;
	int slot;

	slot = sshconn_add(connection_name);
	if (slot < 0 || !sshconn_get_info(slot, &info))
		return -1;

	if (info.state == SSHCONN_DISCONNECTED || info.state == SSHCONN_FAILED)
		sshconn_connect(slot);

	return slot;
}

static BOOL send_to_tmux(const wchar_t *prompt, wchar_t *error_out, size_t error_chars)
{
	const struct hub_config *config = config_get();
	char script[TMUXCMD_SCRIPT_MAX];
	char *prompt_utf8;
	BOOL queued;
	int slot;

	if (!config->tmux_connection[0]) {
		wstr_format(error_out, error_chars, L"No tmux connection configured.\n\nSet [tmux] connection=user@host in sihub.ini.");
		return FALSE;
	}

	if (!build_tmux_script(config, script, sizeof(script))) {
		wstr_format(error_out, error_chars, L"Invalid [tmux] session '%ls' or allowed_commands '%ls' in sihub.ini.\n\nAllowed characters: letters, digits, _ - . (and : in session).", config->tmux_session, config->tmux_allowed_commands);
		return FALSE;
	}

	if (config->dry_run) {
		hub_log(L"dry_run: tmux script not sent");
		return TRUE;
	}

	slot = prepare_connection(config->tmux_connection);
	if (slot < 0) {
		wstr_format(error_out, error_chars, L"Invalid [tmux] connection '%ls' in sihub.ini.\n\nUse user@host or user@host:port.", config->tmux_connection);
		return FALSE;
	}

	prompt_utf8 = wstr_to_utf8(prompt);
	queued = prompt_utf8 && sshconn_exec_async(slot, script, prompt_utf8, strlen(prompt_utf8), HUB_JOB_TMUX_SEND);
	free(prompt_utf8);

	if (!queued)
		wstr_format(error_out, error_chars, L"Cannot queue the prompt for %ls.", config->tmux_connection);

	return queued;
}

enum hub_target target_resolve(enum hub_target requested)
{
	return requested == HUB_TARGET_DEFAULT ? config_get()->default_target : requested;
}

const wchar_t *target_name(enum hub_target target)
{
	return target_name_tbl[target_resolve(target)];
}

BOOL target_send(HWND owner_window, enum hub_target target, const wchar_t *prompt, wchar_t *error_out, size_t error_chars)
{
	const struct hub_config *config = config_get();

	error_out[0] = 0;
	if (config->copy_to_clipboard)
		winutil_set_clipboard_text(owner_window, prompt);

	hub_log(L"target=%ls prompt=%ls", target_name(target), prompt);

	if (target_resolve(target) == HUB_TARGET_TMUX)
		return send_to_tmux(prompt, error_out, error_chars);

	return send_to_vscode(prompt, error_out, error_chars);
}
