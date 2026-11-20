#include <stdlib.h>
#include "target.h"
#include "config.h"
#include "hub_log.h"
#include "winutil.h"
#include "wstr.h"

static const wchar_t hex_digit_tbl[] = L"0123456789ABCDEF";

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

BOOL target_send(HWND owner_window, const wchar_t *prompt, int *shell_code_out)
{
	const struct hub_config *config = config_get();
	struct wide_buf uri;
	BOOL sent = TRUE;

	wide_buf_init(&uri);
	*shell_code_out = 0;
	if (config->copy_to_clipboard)
		winutil_set_clipboard_text(owner_window, prompt);

	build_uri(config->uri_template, prompt, &uri);
	hub_log(L"prompt=%ls", prompt);
	hub_log(L"uri=%ls (length %u)", uri.chars, (unsigned)uri.length);

	if (!config->dry_run) {
		*shell_code_out = winutil_shell_execute(uri.chars);
		sent = *shell_code_out > SHELL_EXECUTE_ERROR_MAX;
		if (!sent)
			hub_log(L"ERROR: ShellExecute failed code=%d", *shell_code_out);
	}

	wide_buf_free(&uri);

	return sent;
}
