#include <stdlib.h>
#include "preprompt.h"
#include "config.h"
#include "hub_log.h"

#define TEMPLATE_FILE_MAX       (256 * 1024)
#define TEMPLATE_ERROR_MAX      256
#define TEMPLATE_TRIM_CHARS     L" \t\n"
#define LINE_NUMBER_TEXT_MAX    16
#define LINUX_PATH_MAX          REQ_FILE_MAX
#define TARGET_NAME_MAX         16
#define TARGET_SUFFIX_MARK      L'!'

struct prompt_template
{
	wchar_t label[TEMPLATE_LABEL_MAX];
	wchar_t *body;
	enum hub_target target;
};

static const struct
{
	const wchar_t *label;
	const wchar_t *body;
} fallback_template_tbl[] =
{
	{
		.label = L"Explain this code",
		.body = L"Explain {symbol} ({path}, lines {line1}-{line2}): purpose, inputs/outputs, key flow and caveats.\n"
				L"First quote line {line1} as you see it, to confirm we are looking at the same source tree.",
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

static const wchar_t missing_symbol_text[] = L"(current position)";

static struct prompt_template g_templates[TEMPLATE_MAX];
static int g_template_count;
static wchar_t g_load_error[TEMPLATE_ERROR_MAX];

void preprompt_free_templates(void)
{
	int index;

	for (index = 0; index < g_template_count; index++) {
		free(g_templates[index].body);
		g_templates[index].body = NULL;
	}

	g_template_count = 0;
}

static void load_fallback_templates(void)
{
	int fallback_count = (int)ARRAYSIZE(fallback_template_tbl);
	int index;

	for (index = 0; index < fallback_count && index < TEMPLATE_MAX; index++) {
		wstr_copy(g_templates[index].label, TEMPLATE_LABEL_MAX, fallback_template_tbl[index].label);
		g_templates[index].body = _wcsdup(fallback_template_tbl[index].body);
		g_templates[index].target = HUB_TARGET_DEFAULT;
		g_template_count = index + 1;
	}
}

static BOOL is_blank(wchar_t character)
{
	return character == L' ' || character == L'\t';
}

static BOOL parse_target_suffix(const wchar_t *suffix, size_t suffix_length, enum hub_target *target_out)
{
	wchar_t name[TARGET_NAME_MAX];

	while (suffix_length && is_blank(*suffix)) {
		suffix++;
		suffix_length--;
	}

	if (suffix_length < 2 || suffix[0] != TARGET_SUFFIX_MARK || suffix_length - 1 >= TARGET_NAME_MAX)
		return FALSE;

	wmemcpy(name, suffix + 1, suffix_length - 1);
	name[suffix_length - 1] = 0;
	*target_out = config_parse_target(name);

	return *target_out != HUB_TARGET_DEFAULT;
}

static BOOL parse_template_header(const wchar_t *line, size_t line_length, wchar_t *label_out, enum hub_target *target_out)
{
	size_t close_index;
	size_t label_length;

	*target_out = HUB_TARGET_DEFAULT;
	while (line_length && is_blank(line[line_length - 1]))
		line_length--;

	if (line_length < 3 || line[0] != L'[')
		return FALSE;

	close_index = line_length - 1;
	while (close_index > 0 && line[close_index] != L']')
		close_index--;

	if (close_index < 2)
		return FALSE;

	if (close_index + 1 < line_length && !parse_target_suffix(line + close_index + 1, line_length - close_index - 1, target_out))
		return FALSE;

	label_length = close_index - 1;
	if (label_length >= TEMPLATE_LABEL_MAX)
		label_length = TEMPLATE_LABEL_MAX - 1;

	wmemcpy(label_out, line + 1, label_length);
	label_out[label_length] = 0;

	return TRUE;
}

static void begin_template(const wchar_t *label, enum hub_target target)
{
	wstr_copy(g_templates[g_template_count].label, TEMPLATE_LABEL_MAX, label);
	g_templates[g_template_count].body = NULL;
	g_templates[g_template_count].target = target;
	g_template_count++;
}

static void finish_template(struct wide_buf *body_buf)
{
	wchar_t *body;

	if (g_template_count <= 0) {
		wide_buf_free(body_buf);
		return;
	}

	wide_buf_trim_end(body_buf, TEMPLATE_TRIM_CHARS);
	body = wide_buf_detach(body_buf);
	g_templates[g_template_count - 1].body = body ? body : _wcsdup(L"");
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

static void parse_template_text(const wchar_t *file_text)
{
	struct wide_buf body_buf;
	wchar_t label[TEMPLATE_LABEL_MAX];
	enum hub_target target;
	const wchar_t *line_start = file_text;
	const wchar_t *line_end;
	size_t line_length;

	wide_buf_init(&body_buf);

	while (*line_start) {
		line_end = wcschr(line_start, L'\n');
		line_length = line_end ? (size_t)(line_end - line_start) : wcslen(line_start);
		if (line_length && line_start[line_length - 1] == L'\r')
			line_length--;

		if (parse_template_header(line_start, line_length, label, &target)) {
			finish_template(&body_buf);

			if (g_template_count >= TEMPLATE_MAX) {
				hub_log(L"WARN: more than %d templates, rest ignored", TEMPLATE_MAX);
				break;
			}

			begin_template(label, target);
		}
		else if (g_template_count > 0) {
			append_template_line(&body_buf, line_start, line_length);
		}

		if (!line_end)
			break;

		line_start = line_end + 1;
	}

	finish_template(&body_buf);
}

void preprompt_load_templates(void)
{
	const wchar_t *template_path = config_get()->template_path;
	wchar_t *file_text;
	BOOL file_read;

	preprompt_free_templates();
	g_load_error[0] = 0;

	file_text = wstr_read_utf8_file(template_path, TEMPLATE_FILE_MAX);
	file_read = (file_text != NULL);

	if (file_read) {
		parse_template_text(file_text);
		free(file_text);
	}

	if (!file_read)
		wstr_format(g_load_error, TEMPLATE_ERROR_MAX, L"(Cannot read %ls - using built-in templates)", template_path);
	else if (g_template_count == 0)
		wstr_format(g_load_error, TEMPLATE_ERROR_MAX, L"(No [label] section in %ls - using built-in templates)", template_path);

	if (g_load_error[0]) {
		hub_log(L"WARN: %ls", g_load_error);
		load_fallback_templates();
	}

	hub_log(L"loaded %d template(s) from '%ls'", g_template_count, template_path);
}

int preprompt_template_count(void)
{
	return g_template_count;
}

const wchar_t *preprompt_template_label(int index)
{

	if (index < 0 || index >= g_template_count)
		return L"";

	return g_templates[index].label;
}

enum hub_target preprompt_template_target(int index)
{

	if (index < 0 || index >= g_template_count)
		return HUB_TARGET_DEFAULT;

	return g_templates[index].target;
}

const wchar_t *preprompt_load_error(void)
{
	return g_load_error;
}

static void map_to_linux_path(const wchar_t *windows_path, wchar_t *linux_path_out, size_t out_chars)
{
	const struct hub_config *config = config_get();
	wchar_t normalized_path[REQ_FILE_MAX];
	wchar_t normalized_prefix[MAX_PATH];
	const wchar_t *relative_part;
	size_t prefix_length;
	size_t linux_prefix_length;
	BOOL need_separator;

	wstr_copy(normalized_path, REQ_FILE_MAX, windows_path);
	wstr_copy(normalized_prefix, MAX_PATH, config->win_prefix);
	wstr_replace_char(normalized_path, L'/', L'\\');
	wstr_replace_char(normalized_prefix, L'/', L'\\');
	prefix_length = wcslen(normalized_prefix);

	if (prefix_length && config->linux_prefix[0] && _wcsnicmp(normalized_path, normalized_prefix, prefix_length) == 0) {
		relative_part = normalized_path + prefix_length;
		while (*relative_part == L'\\')
			relative_part++;

		linux_prefix_length = wcslen(config->linux_prefix);
		need_separator = linux_prefix_length && config->linux_prefix[linux_prefix_length - 1] != L'/';
		wstr_format(linux_path_out, out_chars, L"%ls%ls%ls", config->linux_prefix, need_separator ? L"/" : L"", relative_part);
		wstr_replace_char(linux_path_out, L'\\', L'/');
		return;
	}

	hub_log(L"WARN: no path mapping for '%ls' (win_prefix='%ls', linux_prefix='%ls')", windows_path, config->win_prefix, config->linux_prefix);
	wstr_copy(linux_path_out, out_chars, normalized_path);
	wstr_replace_char(linux_path_out, L'\\', L'/');
}

BOOL preprompt_build(int index, const struct hub_request *request, struct wide_buf *prompt_out)
{
	wchar_t linux_path[LINUX_PATH_MAX];
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
			.value = wstr_basename(request->file),
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
	size_t subst_index;
	BOOL replaced;

	if (index < 0 || index >= g_template_count)
		return FALSE;

	map_to_linux_path(request->file, linux_path, LINUX_PATH_MAX);
	wstr_format(first_line_text, LINE_NUMBER_TEXT_MAX, L"%d", request->first_line);
	wstr_format(last_line_text, LINE_NUMBER_TEXT_MAX, L"%d", request->last_line);

	cursor = g_templates[index].body;
	while (*cursor) {
		replaced = FALSE;

		if (*cursor == L'{') {
			for (subst_index = 0; subst_index < ARRAYSIZE(_substitutions); subst_index++) {
				placeholder_length = wcslen(_substitutions[subst_index].placeholder);

				if (wcsncmp(cursor, _substitutions[subst_index].placeholder, placeholder_length) == 0) {
					wide_buf_append(prompt_out, _substitutions[subst_index].value);
					cursor += placeholder_length;
					replaced = TRUE;
					break;
				}
			}
		}

		if (!replaced)
			wide_buf_append_chars(prompt_out, cursor++, 1);
	}

	if (!prompt_out->chars)
		hub_log(L"WARN: template %d '%ls' is empty", index + 1, g_templates[index].label);

	return prompt_out->chars != NULL;
}
