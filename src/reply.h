#ifndef SIHUB_REPLY_H
#define SIHUB_REPLY_H

#include "hub.h"
#include "sshconn.h"

#define REPLY_TIMER_ID          1

enum reply_status
{
	REPLY_IDLE,
	REPLY_DISABLED,
	REPLY_NO_HOOK,
	REPLY_WAITING,
	REPLY_RECEIVED,
	REPLY_TIMED_OUT,
	REPLY_FAILED,
};

enum reply_status reply_begin(HWND window, int slot, const char *send_output);
enum reply_status reply_on_timer(HWND window);
enum reply_status reply_handle_check(HWND window, const struct sshconn_event *event);
BOOL reply_available(void);
void reply_show_last(BOOL activate);

#endif
