#include <string.h>
#include "ipc.h"
#include "hub_log.h"

#define IPC_MUTEX_NAME          L"Local\\SiHub.Server.v1"
#define IPC_COPYDATA_MAGIC      0x53494842u
#define SERVER_FIND_RETRIES     40
#define SERVER_FIND_DELAY_MS    50
#define COPYDATA_TIMEOUT_MS     3000

static HANDLE g_server_mutex;

BOOL ipc_become_server(void)
{
	BOOL already_running;

	g_server_mutex = CreateMutexW(NULL, TRUE, IPC_MUTEX_NAME);
	already_running = (GetLastError() == ERROR_ALREADY_EXISTS);

	if (already_running && g_server_mutex) {
		CloseHandle(g_server_mutex);
		g_server_mutex = NULL;
	}

	return !already_running;
}

void ipc_release_server(void)
{

	if (!g_server_mutex)
		return;

	ReleaseMutex(g_server_mutex);
	CloseHandle(g_server_mutex);
	g_server_mutex = NULL;
}

static HWND find_server_window(void)
{
	HWND server_window = NULL;
	int attempt;

	for (attempt = 0; attempt < SERVER_FIND_RETRIES && !server_window; attempt++) {
		server_window = FindWindowW(IPC_WINDOW_CLASS, NULL);
		if (!server_window)
			Sleep(SERVER_FIND_DELAY_MS);
	}

	if (!server_window)
		hub_log(L"ERROR: server window not found");

	return server_window;
}

enum ipc_status ipc_send_quit(void)
{
	HWND server_window = find_server_window();

	if (!server_window)
		return IPC_ERR_NO_SERVER;

	PostMessageW(server_window, WM_CLOSE, 0, 0);

	return IPC_OK;
}

enum ipc_status ipc_send_request(const struct hub_request *request)
{
	COPYDATASTRUCT copy_data;
	HWND server_window;
	DWORD server_process_id = 0;
	DWORD_PTR message_result = 0;

	server_window = find_server_window();
	if (!server_window)
		return IPC_ERR_NO_SERVER;

	GetWindowThreadProcessId(server_window, &server_process_id);
	AllowSetForegroundWindow(server_process_id);

	copy_data.dwData = IPC_COPYDATA_MAGIC;
	copy_data.cbData = sizeof(*request);
	copy_data.lpData = (PVOID)request;

	if (!SendMessageTimeoutW(server_window, WM_COPYDATA, 0, (LPARAM)&copy_data, SMTO_ABORTIFHUNG, COPYDATA_TIMEOUT_MS, &message_result) || !message_result) {
		hub_log(L"ERROR: WM_COPYDATA failed (%lu)", GetLastError());
		return IPC_ERR_SEND;
	}

	return IPC_OK;
}

BOOL ipc_receive_request(const COPYDATASTRUCT *copy_data, struct hub_request *request_out)
{

	if (!copy_data || copy_data->dwData != IPC_COPYDATA_MAGIC || copy_data->cbData != sizeof(*request_out))
		return FALSE;

	memcpy(request_out, copy_data->lpData, sizeof(*request_out));
	request_out->file[REQ_FILE_MAX - 1] = 0;
	request_out->symbol[REQ_SYMBOL_MAX - 1] = 0;

	return TRUE;
}
