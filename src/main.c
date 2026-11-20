#include "hub.h"
#include <shellapi.h>
#include "config.h"
#include "ipc.h"
#include "wingui.h"
#include "wstr.h"

struct cmd_args
{
	BOOL quit_requested;
	BOOL has_request;
	struct hub_request request;
};

static BOOL parse_option(struct cmd_args *args, const wchar_t *option, const wchar_t *value)
{

	if (!wcscmp(option, L"--file")) {
		wstr_copy(args->request.file, REQ_FILE_MAX, value);
		args->has_request = TRUE;
	}
	else if (!wcscmp(option, L"--symbol")) {
		wstr_copy(args->request.symbol, REQ_SYMBOL_MAX, value);
	}
	else if (!wcscmp(option, L"--line")) {
		args->request.first_line = _wtoi(value);
	}
	else if (!wcscmp(option, L"--line2")) {
		args->request.last_line = _wtoi(value);
	}
	else if (!wcscmp(option, L"--pick")) {
		args->request.auto_pick = _wtoi(value);
	}
	else {
		return FALSE;
	}

	return TRUE;
}

static void parse_command_line(struct cmd_args *args)
{
	LPWSTR *arg_list;
	const wchar_t *value;
	int arg_count = 0;
	int index;

	ZeroMemory(args, sizeof(*args));
	arg_list = CommandLineToArgvW(GetCommandLineW(), &arg_count);
	if (!arg_list)
		return;

	for (index = 1; index < arg_count; index++) {
		if (!wcscmp(arg_list[index], L"--quit")) {
			args->quit_requested = TRUE;
			continue;
		}

		value = (index + 1 < arg_count) ? arg_list[index + 1] : NULL;
		if (value && parse_option(args, arg_list[index], value))
			index++;
	}

	if (args->request.first_line < 1)
		args->request.first_line = 1;

	if (args->request.last_line < args->request.first_line)
		args->request.last_line = args->request.first_line;

	LocalFree(arg_list);
}

static int run_client(const struct cmd_args *args)
{

	if (args->quit_requested)
		return ipc_send_quit();

	if (!args->has_request)
		return IPC_OK;

	return ipc_send_request(&args->request);
}

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE prev_instance, LPWSTR cmd_line, int show_cmd)
{
	struct cmd_args args;
	int exit_code;

	(void)prev_instance;
	(void)cmd_line;
	(void)show_cmd;

	SetProcessDPIAware();
	config_init();
	config_load();
	parse_command_line(&args);

	if (!ipc_become_server())
		return run_client(&args);

	if (args.quit_requested) {
		ipc_release_server();
		return 0;
	}

	exit_code = wingui_run_server(instance, args.has_request ? &args.request : NULL);
	ipc_release_server();

	return exit_code;
}
