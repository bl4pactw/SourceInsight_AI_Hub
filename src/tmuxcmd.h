#ifndef SIHUB_TMUXCMD_H
#define SIHUB_TMUXCMD_H

#include <stdbool.h>
#include <stddef.h>

#define TMUXCMD_SCRIPT_MAX      2048
#define TMUXCMD_EXIT_NO_SESSION 3
#define TMUXCMD_EXIT_NOT_CLAUDE 4
#define TMUXCMD_STAMP_MAX       64

bool tmuxcmd_is_valid_session(const char *session);
bool tmuxcmd_is_valid_command_list(const char *command_list);
bool tmuxcmd_build_send(const char *session, const char *allowed_commands, bool submit, int submit_delay_ms, bool want_mark, char *script_out, size_t script_size);
bool tmuxcmd_parse_mark(const char *output, char *stamp_out, size_t stamp_size, bool *hook_missing_out);
bool tmuxcmd_build_reply_check(const char *session, const char *stamp, char *script_out, size_t script_size);
const char *tmuxcmd_parse_reply(const char *output, bool *ready_out);

#endif
