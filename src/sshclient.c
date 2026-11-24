#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <netdb.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <libssh2.h>
#include "sshclient.h"

#ifdef _WIN32
typedef SOCKET socket_handle;
#define INVALID_SOCKET_HANDLE   INVALID_SOCKET
#define close_socket            closesocket
#define SOCKET_SHUTDOWN_BOTH    SD_BOTH
#else
typedef int socket_handle;
#define INVALID_SOCKET_HANDLE   (-1)
#define close_socket            close
#define SOCKET_SHUTDOWN_BOTH    SHUT_RDWR
#endif

#define SSH_DEFAULT_PORT        22
#define SSH_CONNECT_TIMEOUT_MS  10000
#define SSH_IO_TIMEOUT_MS       15000
#define SSH_KEEPALIVE_SECONDS   30
#define SSH_PASSWORD_ATTEMPTS   3
#define SSH_OUTPUT_MAX          (1024 * 1024)
#define SSH_READ_CHUNK          4096
#define SSH_HOST_ENTRY_MAX      300
#define SSH_KEY_PATH_MAX        1024
#define SHA256_DIGEST_LENGTH    32
#define FINGERPRINT_PREFIX      "SHA256:"
#define PRIVATE_KEY_PATH_FMT    "%s/%s"
#define PUBLIC_KEY_PATH_FMT     "%s/%s.pub"
#define HOST_ENTRY_PORT_FMT     "[%s]:%d"

static const char *const private_key_name_tbl[] =
{
	"id_ecdsa",
	"id_rsa",
	"id_ed25519",
};

static const char base64_digit_tbl[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

struct ssh_client
{
	socket_handle socket;
	LIBSSH2_SESSION *session;
	char password[SSH_PASSWORD_MAX];
	char error[SSH_ERROR_MAX];
};

static void secure_clear(void *buffer, size_t size)
{
	volatile unsigned char *bytes = buffer;

	while (size--)
		*bytes++ = 0;
}

static void set_error(struct ssh_client *client, const char *format, ...)
{
	va_list args;

	va_start(args, format);
	vsnprintf(client->error, SSH_ERROR_MAX, format, args);
	va_end(args);
}

static void set_session_error(struct ssh_client *client, const char *action)
{
	char *message = NULL;

	if (client->session)
		libssh2_session_last_error(client->session, &message, NULL, 0);

	set_error(client, "%s: %s", action, message ? message : "unknown error");
}

bool sshclient_global_init(void)
{
#ifdef _WIN32
	WSADATA wsa_data;

	if (WSAStartup(MAKEWORD(2, 2), &wsa_data) != 0)
		return false;
#endif

	return libssh2_init(0) == 0;
}

void sshclient_global_cleanup(void)
{
	libssh2_exit();
#ifdef _WIN32
	WSACleanup();
#endif
}

struct ssh_client *sshclient_create(void)
{
	struct ssh_client *client = calloc(1, sizeof(*client));

	if (client)
		client->socket = INVALID_SOCKET_HANDLE;

	return client;
}

void sshclient_destroy(struct ssh_client *client)
{

	if (!client)
		return;

	sshclient_disconnect(client);
	secure_clear(client, sizeof(*client));
	free(client);
}

const char *sshclient_last_error(const struct ssh_client *client)
{
	return client->error;
}

int sshclient_keepalive_interval_ms(void)
{
	return SSH_KEEPALIVE_SECONDS * 1000;
}

static void set_socket_blocking(socket_handle sock, bool blocking)
{
#ifdef _WIN32
	u_long non_blocking = blocking ? 0 : 1;

	ioctlsocket(sock, FIONBIO, &non_blocking);
#else
	int flags = fcntl(sock, F_GETFL, 0);

	fcntl(sock, F_SETFL, blocking ? (flags & ~O_NONBLOCK) : (flags | O_NONBLOCK));
#endif
}

static bool connect_in_progress(void)
{
#ifdef _WIN32
	return WSAGetLastError() == WSAEWOULDBLOCK;
#else
	return errno == EINPROGRESS;
#endif
}

static bool wait_connected(socket_handle sock)
{
	struct timeval timeout;
	fd_set write_set;
	fd_set error_set;
	int socket_error = 0;
	socklen_t error_length = sizeof(socket_error);

	timeout.tv_sec = SSH_CONNECT_TIMEOUT_MS / 1000;
	timeout.tv_usec = (SSH_CONNECT_TIMEOUT_MS % 1000) * 1000;
	FD_ZERO(&write_set);
	FD_ZERO(&error_set);
	FD_SET(sock, &write_set);
	FD_SET(sock, &error_set);

	if (select((int)sock + 1, NULL, &write_set, &error_set, &timeout) <= 0)
		return false;

	if (getsockopt(sock, SOL_SOCKET, SO_ERROR, (char *)&socket_error, &error_length) != 0)
		return false;

	return socket_error == 0;
}

static socket_handle open_socket(struct ssh_client *client, const char *host, int port)
{
	struct addrinfo hints;
	struct addrinfo *address_list = NULL;
	struct addrinfo *address;
	socket_handle sock = INVALID_SOCKET_HANDLE;
	char port_text[16];

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	snprintf(port_text, sizeof(port_text), "%d", port);

	if (getaddrinfo(host, port_text, &hints, &address_list) != 0) {
		set_error(client, "Cannot resolve host '%s'", host);
		return INVALID_SOCKET_HANDLE;
	}

	for (address = address_list; address; address = address->ai_next) {
		sock = socket(address->ai_family, address->ai_socktype, address->ai_protocol);
		if (sock == INVALID_SOCKET_HANDLE)
			continue;

		set_socket_blocking(sock, false);

		if (connect(sock, address->ai_addr, (int)address->ai_addrlen) == 0 || (connect_in_progress() && wait_connected(sock))) {
			set_socket_blocking(sock, true);
			break;
		}

		close_socket(sock);
		sock = INVALID_SOCKET_HANDLE;
	}

	freeaddrinfo(address_list);

	if (sock == INVALID_SOCKET_HANDLE)
		set_error(client, "Cannot connect to %s:%d", host, port);

	return sock;
}

static void base64_encode(const unsigned char *data, size_t length, char *text_out)
{
	unsigned int group;
	size_t index;

	for (index = 0; index + 2 < length; index += 3) {
		group = (data[index] << 16) | (data[index + 1] << 8) | data[index + 2];
		*text_out++ = base64_digit_tbl[(group >> 18) & 0x3F];
		*text_out++ = base64_digit_tbl[(group >> 12) & 0x3F];
		*text_out++ = base64_digit_tbl[(group >> 6) & 0x3F];
		*text_out++ = base64_digit_tbl[group & 0x3F];
	}

	if (length - index == 1) {
		group = data[index] << 16;
		*text_out++ = base64_digit_tbl[(group >> 18) & 0x3F];
		*text_out++ = base64_digit_tbl[(group >> 12) & 0x3F];
	}
	else if (length - index == 2) {
		group = (data[index] << 16) | (data[index + 1] << 8);
		*text_out++ = base64_digit_tbl[(group >> 18) & 0x3F];
		*text_out++ = base64_digit_tbl[(group >> 12) & 0x3F];
		*text_out++ = base64_digit_tbl[(group >> 6) & 0x3F];
	}

	*text_out = 0;
}

static bool get_fingerprint(struct ssh_client *client, char *fingerprint_out)
{
	const unsigned char *digest;
	size_t prefix_length = strlen(FINGERPRINT_PREFIX);

	digest = (const unsigned char *)libssh2_hostkey_hash(client->session, LIBSSH2_HOSTKEY_HASH_SHA256);
	if (!digest)
		return false;

	memcpy(fingerprint_out, FINGERPRINT_PREFIX, prefix_length);
	base64_encode(digest, SHA256_DIGEST_LENGTH, fingerprint_out + prefix_length);

	return true;
}

static int knownhost_key_type(int hostkey_type)
{

	switch (hostkey_type) {
	case LIBSSH2_HOSTKEY_TYPE_RSA: return LIBSSH2_KNOWNHOST_KEY_SSHRSA;
	case LIBSSH2_HOSTKEY_TYPE_DSS: return LIBSSH2_KNOWNHOST_KEY_SSHDSS;
	case LIBSSH2_HOSTKEY_TYPE_ECDSA_256: return LIBSSH2_KNOWNHOST_KEY_ECDSA_256;
	case LIBSSH2_HOSTKEY_TYPE_ECDSA_384: return LIBSSH2_KNOWNHOST_KEY_ECDSA_384;
	case LIBSSH2_HOSTKEY_TYPE_ECDSA_521: return LIBSSH2_KNOWNHOST_KEY_ECDSA_521;
	case LIBSSH2_HOSTKEY_TYPE_ED25519: return LIBSSH2_KNOWNHOST_KEY_ED25519;
	default: return LIBSSH2_KNOWNHOST_KEY_UNKNOWN;
	}
}

static bool trust_new_hostkey(struct ssh_client *client, LIBSSH2_KNOWNHOSTS *known_hosts, const char *known_hosts_path, const char *host_entry, const char *key, size_t key_length, int key_type, const struct ssh_prompt_ops *ops)
{
	char fingerprint[SSH_FINGERPRINT_MAX];
	int type_mask = LIBSSH2_KNOWNHOST_TYPE_PLAIN | LIBSSH2_KNOWNHOST_KEYENC_RAW | knownhost_key_type(key_type);

	if (!get_fingerprint(client, fingerprint)) {
		set_error(client, "Cannot compute host key fingerprint");
		return false;
	}

	if (!ops->confirm_hostkey(ops->context, host_entry, fingerprint)) {
		set_error(client, "Host key for %s was not accepted", host_entry);
		return false;
	}

	if (libssh2_knownhost_addc(known_hosts, host_entry, NULL, key, key_length, NULL, 0, type_mask, NULL) != 0 || libssh2_knownhost_writefile(known_hosts, known_hosts_path, LIBSSH2_KNOWNHOST_FILE_OPENSSH) != 0)
		set_error(client, "Host key accepted but could not be saved to %s", known_hosts_path);

	return true;
}

static bool verify_hostkey(struct ssh_client *client, const char *host, int port, const char *known_hosts_path, const struct ssh_prompt_ops *ops)
{
	LIBSSH2_KNOWNHOSTS *known_hosts;
	struct libssh2_knownhost *found = NULL;
	char host_entry[SSH_HOST_ENTRY_MAX];
	const char *key;
	size_t key_length = 0;
	int key_type = 0;
	int check;
	bool trusted = false;

	if (port == SSH_DEFAULT_PORT)
		snprintf(host_entry, sizeof(host_entry), "%s", host);
	else
		snprintf(host_entry, sizeof(host_entry), HOST_ENTRY_PORT_FMT, host, port);

	key = libssh2_session_hostkey(client->session, &key_length, &key_type);

	if (!key) {
		set_error(client, "Cannot read server host key");
		return false;
	}

	known_hosts = libssh2_knownhost_init(client->session);

	if (!known_hosts) {
		set_error(client, "Cannot initialize known hosts");
		return false;
	}

	libssh2_knownhost_readfile(known_hosts, known_hosts_path, LIBSSH2_KNOWNHOST_FILE_OPENSSH);
	check = libssh2_knownhost_checkp(known_hosts, host, port, key, key_length, LIBSSH2_KNOWNHOST_TYPE_PLAIN | LIBSSH2_KNOWNHOST_KEYENC_RAW, &found);

	switch (check) {
	case LIBSSH2_KNOWNHOST_CHECK_MATCH: trusted = true; break;
	case LIBSSH2_KNOWNHOST_CHECK_NOTFOUND: trusted = trust_new_hostkey(client, known_hosts, known_hosts_path, host_entry, key, key_length, key_type, ops); break;
	case LIBSSH2_KNOWNHOST_CHECK_MISMATCH: set_error(client, "HOST KEY MISMATCH for %s - possible man-in-the-middle attack. Remove the old entry from %s only if the server key was changed on purpose.", host_entry, known_hosts_path); break;
	default: set_error(client, "Host key check failed for %s", host_entry); break;
	}

	libssh2_knownhost_free(known_hosts);

	return trusted;
}

static bool file_exists(const char *path)
{
	FILE *file = fopen(path, "rb");

	if (!file)
		return false;

	fclose(file);

	return true;
}

static bool try_key_files(struct ssh_client *client, const char *user, const char *key_dir)
{
	char private_path[SSH_KEY_PATH_MAX];
	char public_path[SSH_KEY_PATH_MAX];
	const char *public_arg;
	size_t index;

	if (!key_dir || !key_dir[0])
		return false;

	for (index = 0; index < sizeof(private_key_name_tbl) / sizeof(private_key_name_tbl[0]); index++) {
		snprintf(private_path, sizeof(private_path), PRIVATE_KEY_PATH_FMT, key_dir, private_key_name_tbl[index]);
		snprintf(public_path, sizeof(public_path), PUBLIC_KEY_PATH_FMT, key_dir, private_key_name_tbl[index]);
		if (!file_exists(private_path))
			continue;

		public_arg = file_exists(public_path) ? public_path : NULL;
		if (libssh2_userauth_publickey_fromfile_ex(client->session, user, (unsigned int)strlen(user), public_arg, private_path, NULL) == 0)
			return true;
	}

	return false;
}

static void keyboard_interactive_callback(const char *name, int name_length, const char *instruction, int instruction_length, int prompt_count, const LIBSSH2_USERAUTH_KBDINT_PROMPT *prompts, LIBSSH2_USERAUTH_KBDINT_RESPONSE *responses, void **abstract)
{
	struct ssh_client *client = *abstract;
	int index;

	(void)name;
	(void)name_length;
	(void)instruction;
	(void)instruction_length;
	(void)prompts;

	for (index = 0; index < prompt_count; index++) {
		responses[index].text = strdup(client->password);
		responses[index].length = responses[index].text ? (unsigned int)strlen(client->password) : 0;
	}
}

static bool try_password(struct ssh_client *client, const char *user, const char *host, const char *methods, const struct ssh_prompt_ops *ops)
{
	bool use_password = strstr(methods, "password") != NULL;
	bool use_keyboard = strstr(methods, "keyboard-interactive") != NULL;
	unsigned int user_length = (unsigned int)strlen(user);
	int attempt;
	int result;

	if (!use_password && !use_keyboard)
		return false;

	for (attempt = 1; attempt <= SSH_PASSWORD_ATTEMPTS; attempt++) {

		if (!ops->ask_password(ops->context, user, host, attempt, client->password, SSH_PASSWORD_MAX)) {
			set_error(client, "Login cancelled");
			break;
		}

		if (use_password)
			result = libssh2_userauth_password_ex(client->session, user, user_length, client->password, (unsigned int)strlen(client->password), NULL);
		else
			result = libssh2_userauth_keyboard_interactive_ex(client->session, user, user_length, keyboard_interactive_callback);

		secure_clear(client->password, SSH_PASSWORD_MAX);
		if (result == 0)
			return true;

		set_session_error(client, "Login failed");
		if (libssh2_session_last_errno(client->session) != LIBSSH2_ERROR_AUTHENTICATION_FAILED)
			break;
	}

	return false;
}

static bool authenticate(struct ssh_client *client, const char *user, const char *host, const char *key_dir, const struct ssh_prompt_ops *ops)
{
	const char *methods;

	methods = libssh2_userauth_list(client->session, user, (unsigned int)strlen(user));
	if (!methods)
		return libssh2_userauth_authenticated(client->session) == 1;

	if (strstr(methods, "publickey") && try_key_files(client, user, key_dir))
		return true;

	if (try_password(client, user, host, methods, ops))
		return true;

	if (!client->error[0])
		set_error(client, "No supported login method (server offers: %s)", methods);

	return false;
}

bool sshclient_connect(struct ssh_client *client, const char *user, const char *host, int port, const char *known_hosts_path, const char *key_dir, const struct ssh_prompt_ops *ops)
{

	client->error[0] = 0;
	client->socket = open_socket(client, host, port);
	if (client->socket == INVALID_SOCKET_HANDLE)
		return false;

	client->session = libssh2_session_init_ex(NULL, NULL, NULL, client);

	if (!client->session) {
		set_error(client, "Cannot create SSH session");
		sshclient_disconnect(client);
		return false;
	}

	libssh2_session_set_blocking(client->session, 1);
	libssh2_session_set_timeout(client->session, SSH_IO_TIMEOUT_MS);

	if (libssh2_session_handshake(client->session, client->socket) != 0) {
		set_session_error(client, "SSH handshake failed");
		sshclient_disconnect(client);
		return false;
	}

	if (!verify_hostkey(client, host, port, known_hosts_path, ops) || !authenticate(client, user, host, key_dir, ops)) {
		sshclient_disconnect(client);
		return false;
	}

	libssh2_keepalive_config(client->session, 1, SSH_KEEPALIVE_SECONDS);
	client->error[0] = 0;

	return true;
}

static bool write_all(LIBSSH2_CHANNEL *channel, const char *data, size_t length)
{
	ssize_t written;

	while (length > 0) {
		written = libssh2_channel_write(channel, data, length);
		if (written < 0)
			return false;

		data += written;
		length -= (size_t)written;
	}

	return true;
}

static bool read_all(LIBSSH2_CHANNEL *channel, struct ssh_exec_result *result)
{
	char chunk[SSH_READ_CHUNK];
	char *grown;
	ssize_t read_count;

	for (;;) {
		read_count = libssh2_channel_read(channel, chunk, sizeof(chunk));
		if (read_count == 0)
			return true;

		if (read_count < 0)
			return false;

		if (result->output_length + (size_t)read_count > SSH_OUTPUT_MAX)
			continue;

		grown = realloc(result->output, result->output_length + (size_t)read_count + 1);
		if (!grown)
			return false;

		result->output = grown;
		memcpy(result->output + result->output_length, chunk, (size_t)read_count);
		result->output_length += (size_t)read_count;
		result->output[result->output_length] = 0;
	}
}

bool sshclient_exec(struct ssh_client *client, const char *command, const char *stdin_data, size_t stdin_length, struct ssh_exec_result *result_out)
{
	LIBSSH2_CHANNEL *channel;
	bool ok = false;

	memset(result_out, 0, sizeof(*result_out));
	result_out->exit_code = -1;

	if (!client->session) {
		set_error(client, "Not connected");
		return false;
	}

	channel = libssh2_channel_open_session(client->session);

	if (!channel) {
		set_session_error(client, "Cannot open channel");
		return false;
	}

	libssh2_channel_handle_extended_data2(channel, LIBSSH2_CHANNEL_EXTENDED_DATA_MERGE);

	if (libssh2_channel_exec(channel, command) != 0)
		set_session_error(client, "Cannot run command");
	else if (stdin_length && !write_all(channel, stdin_data, stdin_length))
		set_session_error(client, "Cannot send input");
	else if (libssh2_channel_send_eof(channel) != 0 || !read_all(channel, result_out))
		set_session_error(client, "Cannot read output");
	else
		ok = true;

	libssh2_channel_close(channel);
	libssh2_channel_wait_closed(channel);
	if (ok)
		result_out->exit_code = libssh2_channel_get_exit_status(channel);

	libssh2_channel_free(channel);

	return ok;
}

void sshclient_free_result(struct ssh_exec_result *result)
{
	free(result->output);
	memset(result, 0, sizeof(*result));
}

bool sshclient_keepalive(struct ssh_client *client)
{
	int next_seconds = 0;

	if (!client->session)
		return false;

	if (libssh2_keepalive_send(client->session, &next_seconds) != 0) {
		set_session_error(client, "Connection lost");
		return false;
	}

	return true;
}

void sshclient_disconnect(struct ssh_client *client)
{

	if (client->session) {
		libssh2_session_disconnect(client->session, "Bye");
		libssh2_session_free(client->session);
		client->session = NULL;
	}

	if (client->socket != INVALID_SOCKET_HANDLE) {
		close_socket(client->socket);
		client->socket = INVALID_SOCKET_HANDLE;
	}
}

void sshclient_interrupt(struct ssh_client *client)
{

	if (client->socket != INVALID_SOCKET_HANDLE)
		shutdown(client->socket, SOCKET_SHUTDOWN_BOTH);
}
