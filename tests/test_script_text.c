/* This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include "modules/FvwmScript/script_text.h"
#include <assert.h>
#include <limits.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

static int
byte_stride(const char *text, void *context)
{
	int *calls = context;
	(*calls)++;
	return (unsigned char)*text < 128 ? 1 : 3;
}

static void
test_offsets(void)
{
	const char text[] = "a\xe2\x82\xac"
			    "b";
	int	   calls  = 0;

	assert(script_text_byte_offset(text, 5, -1, byte_stride, &calls) == 0);
	assert(script_text_byte_offset(text, 5, 2, byte_stride, &calls) == 4);
	assert(script_text_byte_offset(text, 5, INT_MAX, byte_stride, &calls)
	       == 5);
	assert(script_text_char_offset(text, 5, -1, byte_stride, &calls) == 0);
	assert(script_text_char_offset(text, 5, 4, byte_stride, &calls) == 2);
	assert(script_text_char_offset(text, 5, INT_MAX, byte_stride, &calls)
	       == 3);
	/* The last byte is a truncated legacy multibyte character. */
	assert(script_text_byte_offset("a\xe2", 2, INT_MAX, byte_stride, &calls)
	       == 2);
	assert(script_text_char_offset("a\xe2", 2, INT_MAX, byte_stride, &calls)
	       == 2);
	calls = 0;
	assert(script_text_byte_offset(text, 5, INT_MAX, byte_stride, &calls)
	       == 5);
	assert(calls == 3);
	assert(script_text_next(text, 0, byte_stride, &calls) == 0);
	assert(calls == 3);
}

static char *
owned(const char *text)
{
	char *copy = malloc(strlen(text) + 1);
	assert(copy != NULL);
	strcpy(copy, text);
	return copy;
}

static void
test_edits(void)
{
	char *text     = owned("abcdef");
	char *original = text;

	assert(!script_text_replace(&text, 7, 7, "x", 1, INT_MAX));
	assert(!script_text_replace(&text, 4, 2, "x", 1, INT_MAX));
	assert(!script_text_replace(&text, 0, 0, "x", SIZE_MAX, INT_MAX));
	assert(!script_text_replace(&text, 0, 0, "x", 1, 6));
	assert(text == original && strcmp(text, "abcdef") == 0);
	assert(script_text_replace(&text, 1, 4, text + 2, 3, 6));
	assert(strcmp(text, "acdeef") == 0);
	assert(script_text_delete(text, 1, 4));
	assert(strcmp(text, "aef") == 0);
	assert(!script_text_delete(text, 4, 5));
	assert(script_text_replace(&text, 0, 3, NULL, 0, 0));
	assert(strcmp(text, "") == 0);
	assert(script_text_replace(&text, 0, 0, "z", 1, 1));
	assert(strcmp(text, "z") == 0);
	free(text);
}

static void
test_interactive_limit(void)
{
	char *text = owned("abcd");
	char *original;

	assert(script_text_edit(&text, 4, 4, "e", 1, 5));
	original = text;
	assert(!script_text_edit(&text, 5, 5, "f", 1, 5));
	assert(text == original && strcmp(text, "abcde") == 0);
	assert(!script_text_edit(&text, 1, 3, "xyz", 3, 5));
	assert(text == original && strcmp(text, "abcde") == 0);
	assert(script_text_edit(&text, 1, 3, text + 2, 2, 5));
	assert(strcmp(text, "acdde") == 0);
	/* Oversized old titles may shrink, but cannot stay the same size. */
	original = text;
	assert(!script_text_edit(&text, 0, 1, "x", 1, 2));
	assert(text == original && strcmp(text, "acdde") == 0);
	assert(script_text_edit(&text, 0, 2, "x", 1, 2));
	assert(strcmp(text, "xdde") == 0);
	assert(script_text_delete(text, 0, 2));
	assert(strcmp(text, "de") == 0);
	assert(script_text_edit(&text, 0, 2, "z", 1, 2));
	assert(!script_text_edit(&text, 1, 1, "x", SIZE_MAX, SIZE_MAX));
	assert(strcmp(text, "z") == 0);
	free(text);
}

static void
test_default_edit_limit(void)
{
	const size_t limit = 1024U * 1024U;
	char	    *text  = malloc(limit);
	char	    *original;

	assert(text != NULL);
	memset(text, 'x', limit - 1);
	text[limit - 1] = '\0';
	assert(script_text_edit(&text, limit - 1, limit - 1, "z", 1, limit));
	assert(strlen(text) == limit && text[limit - 1] == 'z');
	original = text;
	assert(!script_text_edit(&text, limit, limit, "y", 1, limit));
	assert(text == original && strlen(text) == limit);
	free(text);
}

static void
test_options(void)
{
	size_t	    length;
	const char *text = "first||last|";
	const char *option;

	option = script_text_option(text, 0, &length);
	assert(length == 5 && memcmp(option, "first", length) == 0);
	option = script_text_option(text, 2, &length);
	assert(length == 0 && *option == '|');
	option = script_text_option(text, 3, &length);
	assert(length == 4 && memcmp(option, "last", length) == 0);
	option = script_text_option(text, INT_MAX, &length);
	assert(length == 0 && *option == '\0');
	assert(!script_text_delete(NULL, 0, 0));
}

int
main(void)
{
	test_offsets();
	test_edits();
	test_options();
	test_interactive_limit();
	test_default_edit_limit();
	return 0;
}
