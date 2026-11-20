#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include "hub_log.h"
#include "wstr.h"

#define LOG_MESSAGE_MAX     4096

static wchar_t g_log_path[MAX_PATH];
static BOOL g_log_enabled;

void hub_log_configure(const wchar_t *log_path, BOOL enabled)
{
	wstr_copy(g_log_path, MAX_PATH, log_path);
	g_log_enabled = enabled;
}

void hub_log(const wchar_t *format, ...)
{
	wchar_t message[LOG_MESSAGE_MAX];
	SYSTEMTIME now;
	va_list args;
	FILE *log_file;
	char *message_utf8;

	if (!g_log_enabled || !g_log_path[0])
		return;

	va_start(args, format);
	wstr_vformat(message, LOG_MESSAGE_MAX, format, args);
	va_end(args);

	log_file = _wfopen(g_log_path, L"ab");
	if (!log_file)
		return;

	GetLocalTime(&now);
	message_utf8 = wstr_to_utf8(message);
	fprintf(log_file, "%04d-%02d-%02d %02d:%02d:%02d.%03d [%lu] %s\r\n",
			now.wYear, now.wMonth, now.wDay, now.wHour, now.wMinute, now.wSecond, now.wMilliseconds,
			GetCurrentProcessId(), message_utf8 ? message_utf8 : "?");
	free(message_utf8);
	fclose(log_file);
}
