#ifndef SIHUB_MDRENDER_H
#define SIHUB_MDRENDER_H

#include <stddef.h>

char *mdrender_to_html(const char *markdown, size_t markdown_length);
void mdrender_free(char *html);

#endif
