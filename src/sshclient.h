#ifndef SIHUB_SSHCLIENT_H
#define SIHUB_SSHCLIENT_H

#include <stdbool.h>
#include <stddef.h>

#define SSH_FINGERPRINT_MAX     96
#define SSH_ERROR_MAX           256
#define SSH_PASSWORD_MAX        256

struct ssh_client;

struct ssh_prompt_ops
{
	bool (*confirm_hostkey)(void *context, const char *host_entry, const char *fingerprint);
	bool (*ask_password)(void *context, const char *user, const char *host, int attempt, char *password_out, size_t password_size);
	void *context;
};

struct ssh_exec_result
{
	char *output;
	size_t output_length;
	int exit_code;
};

bool sshclient_global_init(void);
void sshclient_global_cleanup(void);

struct ssh_client *sshclient_create(void);
void sshclient_destroy(struct ssh_client *client);
bool sshclient_connect(struct ssh_client *client, const char *user, const char *host, int port, const char *known_hosts_path, const char *key_dir, const struct ssh_prompt_ops *ops);
bool sshclient_exec(struct ssh_client *client, const char *command, const char *stdin_data, size_t stdin_length, struct ssh_exec_result *result_out);
void sshclient_free_result(struct ssh_exec_result *result);
bool sshclient_keepalive(struct ssh_client *client);
int sshclient_keepalive_interval_ms(void);
void sshclient_interrupt(struct ssh_client *client);
void sshclient_disconnect(struct ssh_client *client);
const char *sshclient_last_error(const struct ssh_client *client);

#endif
