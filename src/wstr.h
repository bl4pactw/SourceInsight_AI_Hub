#ifndef SIHUB_WSTR_H
#define SIHUB_WSTR_H

#include <stdarg.h>
#include "hub.h"

struct wide_buf
{
	wchar_t *chars;
	size_t length;
	size_t capacity;
};

void wide_buf_init(struct wide_buf *buf);
BOOL wide_buf_append_chars(struct wide_buf *buf, const wchar_t *source, size_t char_count);
BOOL wide_buf_append(struct wide_buf *buf, const wchar_t *source);
void wide_buf_trim_end(struct wide_buf *buf, const wchar_t *trim_chars);
wchar_t *wide_buf_detach(struct wide_buf *buf);
void wide_buf_free(struct wide_buf *buf);

void wstr_format(wchar_t *dest, size_t dest_chars, const wchar_t *format, ...);
void wstr_vformat(wchar_t *dest, size_t dest_chars, const wchar_t *format, va_list args);
void wstr_copy(wchar_t *dest, size_t dest_chars, const wchar_t *source);
void wstr_replace_char(wchar_t *text, wchar_t from_char, wchar_t to_char);
const wchar_t *wstr_basename(const wchar_t *path);
char *wstr_to_utf8(const wchar_t *text);
void wstr_from_utf8(const char *text, wchar_t *dest, size_t dest_chars);
wchar_t *wstr_read_utf8_file(const wchar_t *path, size_t max_bytes);

#endif
