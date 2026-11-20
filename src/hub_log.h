#ifndef SIHUB_HUB_LOG_H
#define SIHUB_HUB_LOG_H

#include "hub.h"

void hub_log_configure(const wchar_t *log_path, BOOL enabled);
void hub_log(const wchar_t *format, ...);

#endif
