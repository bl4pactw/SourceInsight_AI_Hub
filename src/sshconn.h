#ifndef SIHUB_SSHCONN_H
#define SIHUB_SSHCONN_H

#include "hub.h"

#define SSHCONN_MAX             8
#define SSHCONN_NAME_MAX        320
#define SSHCONN_ERROR_MAX       256

enum sshconn_state
{
	SSHCONN_DISCONNECTED,
	SSHCONN_CONNECTING,
	SSHCONN_CONNECTED,
	SSHCONN_FAILED,
};

enum sshconn_event_type
{
	SSHCONN_EVENT_STATE,
	SSHCONN_EVENT_EXEC,
};

enum sshconn_prompt_type
{
	SSHCONN_PROMPT_HOSTKEY,
	SSHCONN_PROMPT_PASSWORD,
};

struct sshconn_info
{
	BOOL in_use;
	enum sshconn_state state;
	wchar_t name[SSHCONN_NAME_MAX];
	wchar_t error[SSHCONN_ERROR_MAX];
};

struct sshconn_event
{
	int slot;
	enum sshconn_event_type type;
	enum sshconn_state state;
	UINT tag;
	BOOL exec_ok;
	int exit_code;
	char *output;
	wchar_t name[SSHCONN_NAME_MAX];
	wchar_t error[SSHCONN_ERROR_MAX];
};

struct sshconn_prompt
{
	enum sshconn_prompt_type type;
	const char *user;
	const char *host;
	const char *host_entry;
	const char *fingerprint;
	int attempt;
	char *password_out;
	size_t password_size;
	BOOL accepted;
};

void sshconn_init(HWND notify_window);
void sshconn_shutdown(void);
BOOL sshconn_is_shutting_down(void);
BOOL sshconn_get_info(int slot, struct sshconn_info *info_out);
int sshconn_add(const wchar_t *spec);
BOOL sshconn_remove(int slot);
BOOL sshconn_connect(int slot);
void sshconn_disconnect(int slot);
BOOL sshconn_exec_async(int slot, const char *command, const char *stdin_data, size_t stdin_length, UINT tag);
void sshconn_free_event(struct sshconn_event *event);

#endif
