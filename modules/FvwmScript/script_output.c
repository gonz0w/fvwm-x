/* This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */
#include "config.h"
#include "script_output.h"
#include "libs/safemalloc.h"
#include <ctype.h>
#include <errno.h>
#include <string.h>

char *script_output_extract_limited(const char *text, size_t length, int line,
	int word, size_t limit)
{
	size_t begin = 0, end, position = 1;
	char *result;

	if (length > SCRIPT_OUTPUT_MAX_BYTES || memchr(text, '\0', length))
		return fxstrdup("");
	while (line > 1 && position < (size_t)line && begin < length)
	{
		while (begin < length && text[begin] != '\n')
			begin++;
		if (begin < length)
			begin++;
		position++;
	}
	end = begin;
	while (end < length && text[end] != '\n')
		end++;
	if (word != -1)
	{
		position = 1;
		while (begin < end)
		{
			while (begin < end &&
			       isspace((unsigned char)text[begin]))
				begin++;
			if (word <= 1 || position == (size_t)word)
				break;
			while (begin < end &&
			       !isspace((unsigned char)text[begin]))
				begin++;
			position++;
		}
		end = begin;
		while (end < length && text[end] != '\n' &&
		       !isspace((unsigned char)text[end]))
			end++;
	}
	if (end - begin > limit)
	{
		errno = EFBIG;
		return NULL;
	}
	result = fxmalloc(end - begin + 1);
	memcpy(result, text + begin, end - begin);
	result[end - begin] = '\0';
	return result;
}

char *script_output_extract(const char *text, size_t length, int line, int word)
{
	return script_output_extract_limited(text, length, line, word,
		SCRIPT_OUTPUT_MAX_BYTES);
}
