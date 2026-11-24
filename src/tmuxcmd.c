#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include "tmuxcmd.h"

#define TMUX_BUFFER_NAME        "sihub"
#define COMMAND_LIST_MAX        256
#define SUBMIT_DELAY_MAX_MS     5000
#define SUBMIT_TEXT_MAX         96
#define STAMP_NONE              "none"
#define MARK_PREFIX             "SIHUB_MARK "
#define NO_HOOK_TOKEN           "SIHUB_NOHOOK"
#define REPLY_PENDING_TOKEN     "SIHUB_PENDING"
#define REPLY_READY_TOKEN       "SIHUB_READY"
#define MARKER_PATH_FMT         "$HOME/.sihub/stop/$s.json"
#define HOOK_PATH               "$HOME/.sihub/stop_hook.sh"
#define REPLY_SCRIPT_PATH       "$HOME/.sihub/last_reply.py"

#define SCRIPT_CHECK_FMT \
	"s='%s'\n" \
	"tmux has-session -t \"$s\" 2>/dev/null || { echo \"tmux session '$s' not found. Start it with: tmux new -A -s $s claude\"; exit %d; }\n" \
	"cmd=$(tmux display-message -p -t \"$s\" '#{pane_current_command}')\n" \
	"case ' %s ' in *\" $cmd \"*) ;; *) echo \"tmux session '$s' is running '$cmd', not Claude. Prompt was not sent.\"; exit %d;; esac\n"

#define SCRIPT_MARK \
	"[ -x \"" HOOK_PATH "\" ] || echo '" NO_HOOK_TOKEN "'\n" \
	"echo \"" MARK_PREFIX "$(stat -c %y \"" MARKER_PATH_FMT "\" 2>/dev/null || echo " STAMP_NONE ")\"\n"

#define SCRIPT_PASTE \
	"tmux load-buffer -b " TMUX_BUFFER_NAME " - && tmux paste-buffer -p -d -b " TMUX_BUFFER_NAME " -t \"$s\""

#define SCRIPT_SUBMIT_FMT       " && sleep %d.%03d && tmux send-keys -t \"$s\" Enter"
#define SCRIPT_END              "\n"

#define SCRIPT_REPLY_CHECK_FMT \
	"s='%s'\n" \
	"cur=$(stat -c %%y \"" MARKER_PATH_FMT "\" 2>/dev/null || echo " STAMP_NONE ")\n" \
	"[ \"$cur\" = '%s' ] && { echo " REPLY_PENDING_TOKEN "; exit 0; }\n" \
	"echo " REPLY_READY_TOKEN "\n" \
	"exec python3 \"" REPLY_SCRIPT_PATH "\" \"" MARKER_PATH_FMT "\"\n"

static bool is_session_char(char character)
{
	return isalnum((unsigned char)character) || character == '_' || character == '-' || character == '.' || character == ':';
}

static bool is_command_char(char character)
{
	return isalnum((unsigned char)character) || character == '_' || character == '-' || character == '.';
}

static bool is_stamp_char(char character)
{
	return isdigit((unsigned char)character) || character == ' ' || character == '-' || character == ':' || character == '.' || character == '+';
}

bool tmuxcmd_is_valid_session(const char *session)
{
	const char *cursor;

	if (!session || !session[0])
		return false;

	for (cursor = session; *cursor; cursor++) {
		if (!is_session_char(*cursor))
			return false;
	}

	return true;
}

bool tmuxcmd_is_valid_command_list(const char *command_list)
{
	const char *cursor;

	if (!command_list || !command_list[0] || strlen(command_list) >= COMMAND_LIST_MAX)
		return false;

	for (cursor = command_list; *cursor; cursor++) {
		if (!is_command_char(*cursor) && *cursor != ',' && *cursor != ' ')
			return false;
	}

	return true;
}

static bool is_valid_stamp(const char *stamp)
{
	const char *cursor;

	if (!strcmp(stamp, STAMP_NONE))
		return true;

	if (!stamp[0])
		return false;

	for (cursor = stamp; *cursor; cursor++) {
		if (!is_stamp_char(*cursor))
			return false;
	}

	return true;
}

static void commas_to_spaces(const char *command_list, char *list_out)
{

	for (; *command_list; command_list++)
		*list_out++ = *command_list == ',' ? ' ' : *command_list;

	*list_out = 0;
}

static bool append_text(char *script_out, size_t script_size, size_t *used, const char *text)
{
	size_t length = strlen(text);

	if (*used + length >= script_size)
		return false;

	memcpy(script_out + *used, text, length + 1);
	*used += length;

	return true;
}

bool tmuxcmd_build_send(const char *session, const char *allowed_commands, bool submit, int submit_delay_ms, bool want_mark, char *script_out, size_t script_size)
{
	char space_list[COMMAND_LIST_MAX];
	char submit_text[SUBMIT_TEXT_MAX];
	size_t used;
	int written;

	if (!tmuxcmd_is_valid_session(session) || !tmuxcmd_is_valid_command_list(allowed_commands))
		return false;

	if (submit_delay_ms < 0)
		submit_delay_ms = 0;

	if (submit_delay_ms > SUBMIT_DELAY_MAX_MS)
		submit_delay_ms = SUBMIT_DELAY_MAX_MS;

	commas_to_spaces(allowed_commands, space_list);
	written = snprintf(script_out, script_size, SCRIPT_CHECK_FMT, session, TMUXCMD_EXIT_NO_SESSION, space_list, TMUXCMD_EXIT_NOT_CLAUDE);
	if (written < 0 || (size_t)written >= script_size)
		return false;

	used = (size_t)written;
	snprintf(submit_text, sizeof(submit_text), SCRIPT_SUBMIT_FMT, submit_delay_ms / 1000, submit_delay_ms % 1000);

	if (want_mark && !append_text(script_out, script_size, &used, SCRIPT_MARK))
		return false;

	if (!append_text(script_out, script_size, &used, SCRIPT_PASTE))
		return false;

	if (submit && !append_text(script_out, script_size, &used, submit_text))
		return false;

	return append_text(script_out, script_size, &used, SCRIPT_END);
}

bool tmuxcmd_parse_mark(const char *output, char *stamp_out, size_t stamp_size, bool *hook_missing_out)
{
	const char *mark;
	size_t length;

	*hook_missing_out = output && strstr(output, NO_HOOK_TOKEN) != NULL;
	stamp_out[0] = 0;
	if (!output)
		return false;

	mark = strstr(output, MARK_PREFIX);
	if (!mark)
		return false;

	mark += strlen(MARK_PREFIX);
	length = strcspn(mark, "\r\n");
	if (length == 0 || length >= stamp_size)
		return false;

	memcpy(stamp_out, mark, length);
	stamp_out[length] = 0;

	return is_valid_stamp(stamp_out);
}

bool tmuxcmd_build_reply_check(const char *session, const char *stamp, char *script_out, size_t script_size)
{
	int written;

	if (!tmuxcmd_is_valid_session(session) || !is_valid_stamp(stamp))
		return false;

	written = snprintf(script_out, script_size, SCRIPT_REPLY_CHECK_FMT, session, stamp);

	return written > 0 && (size_t)written < script_size;
}

const char *tmuxcmd_parse_reply(const char *output, bool *ready_out)
{
	size_t ready_length = strlen(REPLY_READY_TOKEN);

	*ready_out = false;
	if (!output || strncmp(output, REPLY_READY_TOKEN, ready_length) != 0)
		return NULL;

	*ready_out = true;
	output += ready_length;
	if (*output == '\r')
		output++;

	if (*output == '\n')
		output++;

	return output;
}
