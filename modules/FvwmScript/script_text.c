/* This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include "script_text.h"
#include <limits.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

size_t
script_text_next(
    const char *text, size_t remaining, script_text_step_fn step, void *context)
{
	int stride;

	if (remaining == 0) {
		return 0;
	}
	stride = step == NULL ? 1 : step(text, context);
	if (stride < 1) {
		stride = 1;
	}
	return (size_t)stride > remaining ? remaining : (size_t)stride;
}

int
script_text_byte_offset(const char *text, size_t length, int character,
    script_text_step_fn step, void *context)
{
	size_t byte  = 0;
	int    count = 0;

	if (length > INT_MAX) {
		length = INT_MAX;
	}
	while (count < character && byte < length) {
		byte +=
		    script_text_next(text + byte, length - byte, step, context);
		count++;
	}
	return (int)byte;
}

int
script_text_char_offset(const char *text, size_t length, int byte,
    script_text_step_fn step, void *context)
{
	size_t offset = 0;
	size_t end    = byte < 0 ? 0 : (size_t)byte;
	int    count  = 0;

	if (end > length) {
		end = length;
	}
	while (offset < end && count < INT_MAX) {
		offset += script_text_next(
		    text + offset, length - offset, step, context);
		count++;
	}
	return count;
}

bool
script_text_replace(char **text, size_t first, size_t last, const char *insert,
    size_t insert_length, size_t maximum)
{
	size_t length, retained, new_length;
	char  *replacement;

	if (text == NULL || *text == NULL
	    || (insert == NULL && insert_length != 0)) {
		return false;
	}
	length = strlen(*text);
	if (first > last || last > length) {
		return false;
	}
	retained = length - (last - first);
	if (retained > maximum || insert_length > maximum - retained) {
		return false;
	}
	new_length = retained + insert_length;
	if (new_length == SIZE_MAX) {
		return false;
	}
	replacement = malloc(new_length + 1);
	if (replacement == NULL) {
		return false;
	}
	memcpy(replacement, *text, first);
	if (insert_length != 0) {
		memcpy(replacement + first, insert, insert_length);
	}
	memcpy(replacement + first + insert_length, *text + last,
	    length - last + 1);
	free(*text);
	*text = replacement;
	return true;
}

bool
script_text_edit(char **text, size_t first, size_t last, const char *insert,
    size_t insert_length, size_t maximum)
{
	size_t length;

	if (text == NULL || *text == NULL) {
		return false;
	}
	length = strlen(*text);
	if (length > maximum) {
		maximum = length - 1;
	}
	if (maximum > INT_MAX) {
		maximum = INT_MAX;
	}
	return script_text_replace(
	    text, first, last, insert, insert_length, maximum);
}

bool
script_text_delete(char *text, size_t first, size_t last)
{
	size_t length;

	if (text == NULL) {
		return false;
	}
	length = strlen(text);

	if (first > last || last > length) {
		return false;
	}
	memmove(text + first, text + last, length - last + 1);
	return true;
}

const char *
script_text_option(const char *text, int index, size_t *length)
{
	while (index > 1 && *text != '\0') {
		text += strcspn(text, "|");
		if (*text == '|') {
			text++;
		}
		index--;
	}
	*length = strcspn(text, "|");
	return text;
}
