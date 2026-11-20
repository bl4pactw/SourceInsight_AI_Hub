#ifndef SIHUB_IPC_H
#define SIHUB_IPC_H

#include "hub.h"

#define IPC_WINDOW_CLASS    L"SiHub.Server.v1"

enum ipc_status
{
	IPC_OK = 0,
	IPC_ERR_NO_SERVER = 2,
	IPC_ERR_SEND = 3,
};

BOOL ipc_become_server(void);
void ipc_release_server(void);
enum ipc_status ipc_send_request(const struct hub_request *request);
enum ipc_status ipc_send_quit(void);
BOOL ipc_receive_request(const COPYDATASTRUCT *copy_data, struct hub_request *request_out);

#endif
