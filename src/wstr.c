#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "wstr.h"

#define WIDE_BUF_INIT_CAPACITY  256
#define UTF8_BOM                "\xEF\xBB\xBF"
#define UTF8_BOM_LENGTH         3

void wide_buf_init(struct wide_buf *buf)
{
	buf->chars = NULL;
	buf->length = 0;
	buf->capacity = 0;
}

static BOOL wide_buf_reserve(struct wide_buf *buf, size_t extra_chars)
{
	size_t needed = buf->length + extra_chars + 1;
	size_t new_capacity;
	wchar_t *new_chars;

	if (needed <= buf->capacity)
		return TRUE;

	new_capacity = buf->capacity ? buf->capacity : WIDE_BUF_INIT_CAPACITY;
	while (new_capacity < needed)
		new_capacity *= 2;

	new_chars = realloc(buf->chars, new_capacity * sizeof(wchar_t));
	if (!new_chars)
		return FALSE;

	buf->chars = new_chars;
	buf->capacity = new_capacity;

	return TRUE;
}

BOOL wide_buf_append_chars(struct wide_buf *buf, const wchar_t *source, size_t char_count)
{

	if (!wide_buf_reserve(buf, char_count))
		return FALSE;

	wmemcpy(buf->chars + buf->length, source, char_count);
	buf->length += char_count;
	buf->chars[buf->length] = 0;

	return TRUE;
}

BOOL wide_buf_append(struct wide_buf *buf, const wchar_t *source)
{
	return wide_buf_append_chars(buf, source, wcslen(source));
}

void wide_buf_trim_end(struct wide_buf *buf, const wchar_t *trim_chars)
{

	while (buf->length && wcschr(trim_chars, buf->chars[buf->length - 1]))
		buf->chars[--buf->length] = 0;
}

wchar_t *wide_buf_detach(struct wide_buf *buf)
{
	wchar_t *chars = buf->chars;

	wide_buf_init(buf);

	return chars;
}

void wide_buf_free(struct wide_buf *buf)
{
	free(buf->chars);
	wide_buf_init(buf);
}

void wstr_vformat(wchar_t *dest, size_t dest_chars, const wchar_t *format, va_list args)
{
	_vsnwprintf(dest, dest_chars - 1, format, args);
	dest[dest_chars - 1] = 0;
}

void wstr_format(wchar_t *dest, size_t dest_chars, const wchar_t *format, ...)
{
	va_list args;

	va_start(args, format);
	wstr_vformat(dest, dest_chars, format, args);
	va_end(args);
}

void wstr_copy(wchar_t *dest, size_t dest_chars, const wchar_t *source)
{
	wcsncpy(dest, source, dest_chars - 1);
	dest[dest_chars - 1] = 0;
}

void wstr_replace_char(wchar_t *text, wchar_t from_char, wchar_t to_char)
{

	for (; *text; text++) {
		if (*text == from_char)
			*text = to_char;
	}
}

const wchar_t *wstr_basename(const wchar_t *path)
{
	const wchar_t *last_backslash = wcsrchr(path, L'\\');
	const wchar_t *last_slash = wcsrchr(path, L'/');
	const wchar_t *last_separator = last_slash > last_backslash ? last_slash : last_backslash;

	return last_separator ? last_separator + 1 : path;
}

char *wstr_to_utf8(const wchar_t *text)
{
	char *utf8_text;
	int utf8_size;

	utf8_size = WideCharToMultiByte(CP_UTF8, 0, text, -1, NULL, 0, NULL, NULL);
	if (utf8_size <= 0)
		return NULL;

	utf8_text = malloc(utf8_size);
	if (utf8_text)
		WideCharToMultiByte(CP_UTF8, 0, text, -1, utf8_text, utf8_size, NULL, NULL);

	return utf8_text;
}

void wstr_from_utf8(const char *text, wchar_t *dest, size_t dest_chars)
{

	if (!MultiByteToWideChar(CP_UTF8, 0, text, -1, dest, (int)dest_chars))
		dest[0] = 0;

	dest[dest_chars - 1] = 0;
}

static wchar_t *utf8_to_wstr(const char *utf8_text)
{
	wchar_t *wide_text;
	int wide_chars;

	wide_chars = MultiByteToWideChar(CP_UTF8, 0, utf8_text, -1, NULL, 0);
	if (wide_chars <= 0)
		return NULL;

	wide_text = malloc(wide_chars * sizeof(wchar_t));
	if (wide_text)
		MultiByteToWideChar(CP_UTF8, 0, utf8_text, -1, wide_text, wide_chars);

	return wide_text;
}

static char *read_file_bytes(const wchar_t *path, size_t max_bytes, size_t *size_out)
{
	HANDLE file_handle;
	DWORD file_size;
	DWORD bytes_read = 0;
	char *raw_bytes = NULL;

	file_handle = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	if (file_handle == INVALID_HANDLE_VALUE)
		return NULL;

	file_size = GetFileSize(file_handle, NULL);
	if (file_size != INVALID_FILE_SIZE && file_size <= max_bytes)
		raw_bytes = malloc(file_size + 1);

	if (raw_bytes && !ReadFile(file_handle, raw_bytes, file_size, &bytes_read, NULL)) {
		free(raw_bytes);
		raw_bytes = NULL;
	}

	CloseHandle(file_handle);

	if (raw_bytes) {
		raw_bytes[bytes_read] = 0;
		*size_out = bytes_read;
	}

	return raw_bytes;
}

wchar_t *wstr_read_utf8_file(const wchar_t *path, size_t max_bytes)
{
	const char *utf8_text;
	char *raw_bytes;
	wchar_t *wide_text;
	size_t byte_count = 0;

	raw_bytes = read_file_bytes(path, max_bytes, &byte_count);
	if (!raw_bytes)
		return NULL;

	utf8_text = raw_bytes;
	if (byte_count >= UTF8_BOM_LENGTH && memcmp(raw_bytes, UTF8_BOM, UTF8_BOM_LENGTH) == 0)
		utf8_text += UTF8_BOM_LENGTH;

	wide_text = utf8_to_wstr(utf8_text);
	free(raw_bytes);

	return wide_text;
}
