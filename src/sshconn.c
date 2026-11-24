#include <stdlib.h>
#include <string.h>
#include "sshconn.h"
#include "sshclient.h"
#include "config.h"
#include "hub_log.h"
#include "wstr.h"

#define SSH_USER_MAX            64
#define SSH_HOST_MAX            256
#define SSH_DEFAULT_PORT        22
#define SSH_PORT_MIN            1
#define SSH_PORT_MAX            65535
#define SSH_PATH_ACP_MAX        (MAX_PATH * 2)
#define CONNECTION_NAME_FMT     L"%ls@%ls"
#define CONNECTION_PORT_FMT     L"%ls@%ls:%d"
#define INI_SECTION_CONNECTIONS L"connections"
#define INI_KEY_CONNECTION_FMT  L"conn%d"
#define INI_KEY_MAX             16
#define DISCONNECTED_ERROR      "Disconnected"

struct ssh_job
{
	struct ssh_job *next;
	char *command;
	char *stdin_data;
	size_t stdin_length;
	UINT tag;
};

struct ssh_connection
{
	BOOL in_use;
	char user[SSH_USER_MAX];
	char host[SSH_HOST_MAX];
	int port;
	wchar_t name[SSHCONN_NAME_MAX];
	enum sshconn_state state;
	wchar_t error[SSHCONN_ERROR_MAX];
	HANDLE thread;
	HANDLE wake_event;
	volatile LONG stop_requested;
	struct ssh_client *client;
	struct ssh_job *job_head;
	struct ssh_job *job_tail;
};

static struct ssh_connection g_connections[SSHCONN_MAX];
static CRITICAL_SECTION g_lock;
static HWND g_notify_window;
static volatile LONG g_shutting_down;
static BOOL g_initialized;

static void utf8_to_wide(const char *text, wchar_t *dest, size_t dest_chars)
{

	if (!MultiByteToWideChar(CP_UTF8, 0, text, -1, dest, (int)dest_chars))
		dest[0] = 0;

	dest[dest_chars - 1] = 0;
}

static BOOL wide_to_codepage(UINT codepage, const wchar_t *text, char *dest, size_t dest_size)
{
	int written = WideCharToMultiByte(codepage, 0, text, -1, dest, (int)dest_size, NULL, NULL);

	if (written <= 0)
		dest[0] = 0;

	return written > 0;
}

static BOOL parse_spec(const wchar_t *spec, struct ssh_connection *connection)
{
	wchar_t buffer[SSHCONN_NAME_MAX];
	wchar_t *at_sign;
	wchar_t *colon;
	int port = SSH_DEFAULT_PORT;

	wstr_copy(buffer, SSHCONN_NAME_MAX, spec);
	at_sign = wcschr(buffer, L'@');
	if (!at_sign || at_sign == buffer || !at_sign[1])
		return FALSE;

	*at_sign = 0;
	colon = wcsrchr(at_sign + 1, L':');

	if (colon) {
		*colon = 0;
		port = _wtoi(colon + 1);
	}

	if (port < SSH_PORT_MIN || port > SSH_PORT_MAX || !at_sign[1])
		return FALSE;

	if (!wide_to_codepage(CP_UTF8, buffer, connection->user, SSH_USER_MAX) || !wide_to_codepage(CP_UTF8, at_sign + 1, connection->host, SSH_HOST_MAX))
		return FALSE;

	connection->port = port;
	if (port == SSH_DEFAULT_PORT)
		wstr_format(connection->name, SSHCONN_NAME_MAX, CONNECTION_NAME_FMT, buffer, at_sign + 1);
	else
		wstr_format(connection->name, SSHCONN_NAME_MAX, CONNECTION_PORT_FMT, buffer, at_sign + 1, port);

	return TRUE;
}

static void save_connections(void)
{
	const wchar_t *ini_path = config_get()->ini_path;
	wchar_t key[INI_KEY_MAX];
	int slot;

	for (slot = 0; slot < SSHCONN_MAX; slot++) {
		wstr_format(key, INI_KEY_MAX, INI_KEY_CONNECTION_FMT, slot + 1);
		WritePrivateProfileStringW(INI_SECTION_CONNECTIONS, key, g_connections[slot].in_use ? g_connections[slot].name : NULL, ini_path);
	}
}

static void load_connections(void)
{
	const wchar_t *ini_path = config_get()->ini_path;
	wchar_t key[INI_KEY_MAX];
	wchar_t spec[SSHCONN_NAME_MAX];
	int slot;

	for (slot = 0; slot < SSHCONN_MAX; slot++) {
		wstr_format(key, INI_KEY_MAX, INI_KEY_CONNECTION_FMT, slot + 1);
		GetPrivateProfileStringW(INI_SECTION_CONNECTIONS, key, L"", spec, SSHCONN_NAME_MAX, ini_path);
		if (spec[0] && parse_spec(spec, &g_connections[slot]))
			g_connections[slot].in_use = TRUE;
	}
}

void sshconn_init(HWND notify_window)
{

	if (g_initialized)
		return;

	InitializeCriticalSection(&g_lock);
	g_notify_window = notify_window;
	g_initialized = TRUE;

	if (!sshclient_global_init())
		hub_log(L"ERROR: SSH library initialization failed");

	load_connections();
}

BOOL sshconn_is_shutting_down(void)
{
	return g_shutting_down != 0;
}

static BOOL is_valid_slot(int slot)
{
	return slot >= 0 && slot < SSHCONN_MAX && g_connections[slot].in_use;
}

BOOL sshconn_get_info(int slot, struct sshconn_info *info_out)
{
	struct ssh_connection *connection;

	ZeroMemory(info_out, sizeof(*info_out));
	if (slot < 0 || slot >= SSHCONN_MAX)
		return FALSE;

	connection = &g_connections[slot];
	EnterCriticalSection(&g_lock);
	info_out->in_use = connection->in_use;
	info_out->state = connection->state;
	wstr_copy(info_out->name, SSHCONN_NAME_MAX, connection->name);
	wstr_copy(info_out->error, SSHCONN_ERROR_MAX, connection->error);
	LeaveCriticalSection(&g_lock);

	return info_out->in_use;
}

static void post_event(struct sshconn_event *event)
{

	if (!g_notify_window || !PostMessageW(g_notify_window, WM_HUB_SSH_EVENT, 0, (LPARAM)event))
		sshconn_free_event(event);
}

static struct sshconn_event *create_event(int slot, enum sshconn_event_type type)
{
	struct sshconn_event *event = calloc(1, sizeof(*event));

	if (!event)
		return NULL;

	event->slot = slot;
	event->type = type;
	EnterCriticalSection(&g_lock);
	event->state = g_connections[slot].state;
	wstr_copy(event->name, SSHCONN_NAME_MAX, g_connections[slot].name);
	wstr_copy(event->error, SSHCONN_ERROR_MAX, g_connections[slot].error);
	LeaveCriticalSection(&g_lock);

	return event;
}

void sshconn_free_event(struct sshconn_event *event)
{

	if (!event)
		return;

	free(event->output);
	free(event);
}

static void set_state(int slot, enum sshconn_state state, const char *error)
{
	struct ssh_connection *connection = &g_connections[slot];
	struct sshconn_event *event;

	EnterCriticalSection(&g_lock);
	connection->state = state;
	if (error)
		utf8_to_wide(error, connection->error, SSHCONN_ERROR_MAX);
	else
		connection->error[0] = 0;

	LeaveCriticalSection(&g_lock);

	hub_log(L"ssh %ls: state=%d %ls", connection->name, (int)state, connection->error);
	event = create_event(slot, SSHCONN_EVENT_STATE);
	if (event)
		post_event(event);
}

static BOOL send_prompt(struct sshconn_prompt *prompt)
{

	if (sshconn_is_shutting_down() || !g_notify_window)
		return FALSE;

	SendMessageW(g_notify_window, WM_HUB_SSH_PROMPT, 0, (LPARAM)prompt);

	return prompt->accepted;
}

static bool prompt_hostkey(void *context, const char *host_entry, const char *fingerprint)
{
	struct sshconn_prompt prompt;

	(void)context;
	ZeroMemory(&prompt, sizeof(prompt));
	prompt.type = SSHCONN_PROMPT_HOSTKEY;
	prompt.host_entry = host_entry;
	prompt.fingerprint = fingerprint;

	return send_prompt(&prompt) != FALSE;
}

static bool prompt_password(void *context, const char *user, const char *host, int attempt, char *password_out, size_t password_size)
{
	struct sshconn_prompt prompt;

	(void)context;
	ZeroMemory(&prompt, sizeof(prompt));
	prompt.type = SSHCONN_PROMPT_PASSWORD;
	prompt.user = user;
	prompt.host = host;
	prompt.attempt = attempt;
	prompt.password_out = password_out;
	prompt.password_size = password_size;

	return send_prompt(&prompt) != FALSE;
}

static struct ssh_job *pop_job(struct ssh_connection *connection)
{
	struct ssh_job *job;

	EnterCriticalSection(&g_lock);
	job = connection->job_head;

	if (job) {
		connection->job_head = job->next;
		if (!connection->job_head)
			connection->job_tail = NULL;
	}

	LeaveCriticalSection(&g_lock);

	return job;
}

static void free_job(struct ssh_job *job)
{
	free(job->command);
	free(job->stdin_data);
	free(job);
}

static void run_job(int slot, struct ssh_client *client, struct ssh_job *job)
{
	struct ssh_exec_result result;
	struct sshconn_event *event;
	BOOL exec_ok;

	exec_ok = sshclient_exec(client, job->command, job->stdin_data, job->stdin_length, &result);
	event = create_event(slot, SSHCONN_EVENT_EXEC);

	if (!event) {
		sshclient_free_result(&result);
		return;
	}

	event->tag = job->tag;
	event->exec_ok = exec_ok;
	event->exit_code = result.exit_code;
	event->output = result.output;
	if (!exec_ok)
		utf8_to_wide(sshclient_last_error(client), event->error, SSHCONN_ERROR_MAX);

	post_event(event);
}

static void fail_pending_jobs(int slot)
{
	struct ssh_connection *connection = &g_connections[slot];
	struct sshconn_event *event;
	struct ssh_job *job;

	while ((job = pop_job(connection)) != NULL) {
		event = create_event(slot, SSHCONN_EVENT_EXEC);

		if (event) {
			event->tag = job->tag;
			event->exit_code = -1;
			utf8_to_wide(DISCONNECTED_ERROR, event->error, SSHCONN_ERROR_MAX);
			post_event(event);
		}

		free_job(job);
	}
}

static BOOL connect_client(struct ssh_connection *connection, struct ssh_client *client)
{
	const struct hub_config *config = config_get();
	char known_hosts_path[SSH_PATH_ACP_MAX];
	char key_dir[SSH_PATH_ACP_MAX];
	struct ssh_prompt_ops ops;

	wide_to_codepage(CP_ACP, config->known_hosts_path, known_hosts_path, SSH_PATH_ACP_MAX);
	wide_to_codepage(CP_ACP, config->ssh_key_dir, key_dir, SSH_PATH_ACP_MAX);
	ops.confirm_hostkey = prompt_hostkey;
	ops.ask_password = prompt_password;
	ops.context = connection;

	return sshclient_connect(client, connection->user, connection->host, connection->port, known_hosts_path, key_dir, &ops);
}

static void serve_connection(int slot, struct ssh_client *client)
{
	struct ssh_connection *connection = &g_connections[slot];
	struct ssh_job *job;

	while (!connection->stop_requested) {
		WaitForSingleObject(connection->wake_event, (DWORD)sshclient_keepalive_interval_ms());

		while (!connection->stop_requested && (job = pop_job(connection)) != NULL) {
			run_job(slot, client, job);
			free_job(job);
		}

		if (!connection->stop_requested && !sshclient_keepalive(client)) {
			set_state(slot, SSHCONN_FAILED, sshclient_last_error(client));
			return;
		}
	}

	set_state(slot, SSHCONN_DISCONNECTED, NULL);
}

static DWORD WINAPI connection_worker(LPVOID parameter)
{
	int slot = (int)(INT_PTR)parameter;
	struct ssh_connection *connection = &g_connections[slot];
	struct ssh_client *client = sshclient_create();

	if (!client) {
		set_state(slot, SSHCONN_FAILED, "Out of memory");
		return 0;
	}

	EnterCriticalSection(&g_lock);
	connection->client = client;
	LeaveCriticalSection(&g_lock);

	if (!connect_client(connection, client))
		set_state(slot, connection->stop_requested ? SSHCONN_DISCONNECTED : SSHCONN_FAILED, connection->stop_requested ? NULL : sshclient_last_error(client));
	else if (connection->stop_requested)
		set_state(slot, SSHCONN_DISCONNECTED, NULL);
	else {
		set_state(slot, SSHCONN_CONNECTED, NULL);
		serve_connection(slot, client);
	}

	EnterCriticalSection(&g_lock);
	connection->client = NULL;
	LeaveCriticalSection(&g_lock);

	sshclient_destroy(client);
	fail_pending_jobs(slot);

	return 0;
}

static void close_finished_thread(struct ssh_connection *connection)
{

	if (!connection->thread)
		return;

	WaitForSingleObject(connection->thread, INFINITE);
	CloseHandle(connection->thread);
	connection->thread = NULL;
}

BOOL sshconn_connect(int slot)
{
	struct ssh_connection *connection;
	BOOL busy;

	if (!is_valid_slot(slot) || sshconn_is_shutting_down())
		return FALSE;

	connection = &g_connections[slot];
	EnterCriticalSection(&g_lock);
	busy = connection->state == SSHCONN_CONNECTING || connection->state == SSHCONN_CONNECTED;
	LeaveCriticalSection(&g_lock);

	if (busy)
		return FALSE;

	close_finished_thread(connection);
	if (!connection->wake_event)
		connection->wake_event = CreateEventW(NULL, FALSE, FALSE, NULL);

	InterlockedExchange(&connection->stop_requested, 0);
	set_state(slot, SSHCONN_CONNECTING, NULL);
	connection->thread = CreateThread(NULL, 0, connection_worker, (LPVOID)(INT_PTR)slot, 0, NULL);

	if (!connection->thread) {
		set_state(slot, SSHCONN_FAILED, "Cannot start connection thread");
		return FALSE;
	}

	return TRUE;
}

void sshconn_disconnect(int slot)
{
	struct ssh_connection *connection;

	if (!is_valid_slot(slot))
		return;

	connection = &g_connections[slot];
	InterlockedExchange(&connection->stop_requested, 1);
	if (connection->wake_event)
		SetEvent(connection->wake_event);

	EnterCriticalSection(&g_lock);
	if (connection->client && connection->state == SSHCONN_CONNECTING)
		sshclient_interrupt(connection->client);

	LeaveCriticalSection(&g_lock);
}

int sshconn_add(const wchar_t *spec)
{
	struct ssh_connection parsed;
	int free_slot = -1;
	int slot;

	ZeroMemory(&parsed, sizeof(parsed));
	if (!parse_spec(spec, &parsed))
		return -1;

	for (slot = 0; slot < SSHCONN_MAX; slot++) {
		if (g_connections[slot].in_use && !wcscmp(g_connections[slot].name, parsed.name))
			return slot;

		if (!g_connections[slot].in_use && free_slot < 0)
			free_slot = slot;
	}

	if (free_slot < 0)
		return -1;

	close_finished_thread(&g_connections[free_slot]);
	EnterCriticalSection(&g_lock);
	strcpy(g_connections[free_slot].user, parsed.user);
	strcpy(g_connections[free_slot].host, parsed.host);
	g_connections[free_slot].port = parsed.port;
	wstr_copy(g_connections[free_slot].name, SSHCONN_NAME_MAX, parsed.name);
	g_connections[free_slot].state = SSHCONN_DISCONNECTED;
	g_connections[free_slot].error[0] = 0;
	g_connections[free_slot].in_use = TRUE;
	LeaveCriticalSection(&g_lock);

	save_connections();

	return free_slot;
}

BOOL sshconn_remove(int slot)
{
	struct sshconn_info info;

	if (!sshconn_get_info(slot, &info) || info.state == SSHCONN_CONNECTING || info.state == SSHCONN_CONNECTED)
		return FALSE;

	close_finished_thread(&g_connections[slot]);
	EnterCriticalSection(&g_lock);
	g_connections[slot].in_use = FALSE;
	LeaveCriticalSection(&g_lock);

	save_connections();

	return TRUE;
}

BOOL sshconn_exec_async(int slot, const char *command, const char *stdin_data, size_t stdin_length, UINT tag)
{
	struct ssh_connection *connection;
	struct ssh_job *job;
	BOOL queued = FALSE;

	if (!is_valid_slot(slot))
		return FALSE;

	job = calloc(1, sizeof(*job));
	if (!job)
		return FALSE;

	job->command = _strdup(command);
	job->stdin_data = stdin_length ? malloc(stdin_length) : NULL;
	job->stdin_length = job->stdin_data ? stdin_length : 0;
	job->tag = tag;
	if (job->stdin_data)
		memcpy(job->stdin_data, stdin_data, stdin_length);

	connection = &g_connections[slot];
	EnterCriticalSection(&g_lock);

	if (job->command && connection->state == SSHCONN_CONNECTED) {
		if (connection->job_tail)
			connection->job_tail->next = job;
		else
			connection->job_head = job;

		connection->job_tail = job;
		queued = TRUE;
	}

	LeaveCriticalSection(&g_lock);

	if (!queued) {
		free_job(job);
		return FALSE;
	}

	SetEvent(connection->wake_event);

	return TRUE;
}

static void wait_thread_pumping(HANDLE thread)
{
	MSG message;

	while (MsgWaitForMultipleObjects(1, &thread, FALSE, INFINITE, QS_SENDMESSAGE) == WAIT_OBJECT_0 + 1)
		PeekMessageW(&message, NULL, 0, 0, PM_NOREMOVE);
}

void sshconn_shutdown(void)
{
	struct ssh_connection *connection;
	int slot;

	if (!g_initialized)
		return;

	InterlockedExchange(&g_shutting_down, 1);

	for (slot = 0; slot < SSHCONN_MAX; slot++) {
		connection = &g_connections[slot];
		InterlockedExchange(&connection->stop_requested, 1);
		if (connection->wake_event)
			SetEvent(connection->wake_event);

		EnterCriticalSection(&g_lock);
		if (connection->client)
			sshclient_interrupt(connection->client);

		LeaveCriticalSection(&g_lock);
	}

	for (slot = 0; slot < SSHCONN_MAX; slot++) {
		connection = &g_connections[slot];

		if (connection->thread) {
			wait_thread_pumping(connection->thread);
			CloseHandle(connection->thread);
			connection->thread = NULL;
		}

		if (connection->wake_event) {
			CloseHandle(connection->wake_event);
			connection->wake_event = NULL;
		}
	}

	g_notify_window = NULL;
	sshclient_global_cleanup();
	DeleteCriticalSection(&g_lock);
	g_initialized = FALSE;
}
