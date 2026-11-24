#include <limits.h>
#include <string.h>
#include "reply.h"
#include "config.h"
#include "hub_log.h"
#include "tmuxcmd.h"
#include "winutil.h"

#define REPLY_POLL_MIN_MS       500

struct reply_wait
{
	BOOL active;
	BOOL check_in_flight;
	int slot;
	UINT serial;
	char stamp[TMUXCMD_STAMP_MAX];
	ULONGLONG started_ms;
};

static struct reply_wait g_wait;

static void stop_wait(HWND window)
{
	g_wait.active = FALSE;
	g_wait.check_in_flight = FALSE;
	KillTimer(window, REPLY_TIMER_ID);
}

enum reply_status reply_begin(HWND window, int slot, const char *send_output)
{
	const struct hub_config *config = config_get();
	char stamp[TMUXCMD_STAMP_MAX];
	bool hook_missing;
	UINT poll_ms;

	if (!config->tmux_read_reply)
		return REPLY_DISABLED;

	if (!tmuxcmd_parse_mark(send_output, stamp, sizeof(stamp), &hook_missing) || hook_missing)
		return REPLY_NO_HOOK;

	stop_wait(window);
	g_wait.active = TRUE;
	g_wait.slot = slot;
	g_wait.serial++;
	strcpy(g_wait.stamp, stamp);
	g_wait.started_ms = GetTickCount64();
	poll_ms = config->reply_poll_ms < REPLY_POLL_MIN_MS ? REPLY_POLL_MIN_MS : (UINT)config->reply_poll_ms;
	SetTimer(window, REPLY_TIMER_ID, poll_ms, NULL);
	hub_log(L"reply: waiting (stamp '%hs')", stamp);

	return REPLY_WAITING;
}

static BOOL queue_check(void)
{
	const struct hub_config *config = config_get();
	char session[TMUX_SESSION_MAX];
	char script[TMUXCMD_SCRIPT_MAX];

	if (!WideCharToMultiByte(CP_UTF8, 0, config->tmux_session, -1, session, TMUX_SESSION_MAX, NULL, NULL))
		return FALSE;

	if (!tmuxcmd_build_reply_check(session, g_wait.stamp, script, sizeof(script)))
		return FALSE;

	return sshconn_exec_async(g_wait.slot, script, NULL, 0, HUB_JOB_REPLY_CHECK | (g_wait.serial << HUB_JOB_SERIAL_SHIFT));
}

enum reply_status reply_on_timer(HWND window)
{
	ULONGLONG timeout_ms = (ULONGLONG)config_get()->reply_timeout_s * 1000;

	if (!g_wait.active) {
		KillTimer(window, REPLY_TIMER_ID);
		return REPLY_IDLE;
	}

	if (GetTickCount64() - g_wait.started_ms > timeout_ms) {
		hub_log(L"reply: timed out");
		stop_wait(window);
		return REPLY_TIMED_OUT;
	}

	if (g_wait.check_in_flight)
		return REPLY_WAITING;

	g_wait.check_in_flight = queue_check();

	if (!g_wait.check_in_flight) {
		hub_log(L"reply: cannot queue check");
		stop_wait(window);
		return REPLY_FAILED;
	}

	return REPLY_WAITING;
}

static BOOL write_reply_file(const wchar_t *path, const char *text)
{
	HANDLE file_handle;
	DWORD length = (DWORD)strlen(text);
	DWORD written = 0;
	BOOL ok;

	file_handle = CreateFileW(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
	if (file_handle == INVALID_HANDLE_VALUE)
		return FALSE;

	ok = WriteFile(file_handle, text, length, &written, NULL) && written == length;
	CloseHandle(file_handle);

	return ok;
}

enum reply_status reply_handle_check(HWND window, const struct sshconn_event *event)
{
	const wchar_t *reply_path = config_get()->reply_path;
	const char *reply_text;
	bool ready;

	if (!g_wait.active || event->slot != g_wait.slot || (event->tag >> HUB_JOB_SERIAL_SHIFT) != (g_wait.serial & (UINT_MAX >> HUB_JOB_SERIAL_SHIFT)))
		return REPLY_IDLE;

	g_wait.check_in_flight = FALSE;

	if (!event->exec_ok || event->exit_code != 0) {
		hub_log(L"reply: check failed (exit %d) %ls", event->exit_code, event->error);
		stop_wait(window);
		return REPLY_FAILED;
	}

	reply_text = tmuxcmd_parse_reply(event->output, &ready);
	if (!ready)
		return REPLY_WAITING;

	stop_wait(window);

	if (!write_reply_file(reply_path, reply_text)) {
		hub_log(L"reply: cannot write %ls", reply_path);
		return REPLY_FAILED;
	}

	hub_log(L"reply: received %u bytes -> %ls", (unsigned)strlen(reply_text), reply_path);

	return REPLY_RECEIVED;
}

BOOL reply_available(void)
{
	return GetFileAttributesW(config_get()->reply_path) != INVALID_FILE_ATTRIBUTES;
}

void reply_open_last(void)
{
	winutil_open_path(config_get()->reply_path);
}
