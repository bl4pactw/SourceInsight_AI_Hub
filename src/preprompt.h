#ifndef SIHUB_PREPROMPT_H
#define SIHUB_PREPROMPT_H

#include "hub.h"
#include "wstr.h"

#define TEMPLATE_MAX            9
#define TEMPLATE_LABEL_MAX      64

void preprompt_load_templates(void);
void preprompt_free_templates(void);
int preprompt_template_count(void);
const wchar_t *preprompt_template_label(int index);
enum hub_target preprompt_template_target(int index);
const wchar_t *preprompt_load_error(void);
BOOL preprompt_build(int index, const struct hub_request *request, struct wide_buf *prompt_out);

#endif
