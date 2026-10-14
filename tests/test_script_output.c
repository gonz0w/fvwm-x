/* This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */
#include "config.h"
#include "modules/FvwmScript/script_output.h"
#include <assert.h>
#include <limits.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>

static void check(const char *text, int line, int word, const char *expected)
{
	char *result = script_output_extract(text, strlen(text), line, word);
	assert(result && strcmp(result, expected) == 0);
	free(result);
}

static void test_positions(void)
{
	check("one two\nthree four\n", 0, 0, "one");
	check("one two\nthree four\n", 1, 1, "one");
	check("one two\nthree four\n", 2, 2, "four");
	check("one two\nthree four\n", 2, -1, "three four");
	check("one two\nthree four\n", 3, 1, "");
	check("last", 2, -1, "");
	check("last", INT_MAX, INT_MAX, "");
	check("last", INT_MIN, INT_MIN, "last");
	check("", 1, 1, "");
	check("\nsecond\n", 1, 1, "");
	check("\nsecond\n", 2, 1, "second");
	check("  one\t two   three ", 1, 2, "two");
	check("  one\t two   three ", 1, 3, "three");
	check("  one\t two   three ", 1, 4, "");
}

static void test_explicit_lengths(void)
{
	char *input = malloc(SCRIPT_OUTPUT_MAX_BYTES + 1);
	assert(input);
	memset(input, 'x', SCRIPT_OUTPUT_MAX_BYTES + 1);
	char *result = script_output_extract(input,
		SCRIPT_OUTPUT_MAX_BYTES, 0, 0);
	assert(result && strlen(result) == SCRIPT_OUTPUT_MAX_BYTES);
	assert(memcmp(result, input, SCRIPT_OUTPUT_MAX_BYTES) == 0);
	free(result);
	result = script_output_extract(input,
		SCRIPT_OUTPUT_MAX_BYTES + 1, 0, 0);
	assert(result && result[0] == '\0');
	free(result);
	result = script_output_extract("x\0y", 3, 1, -1);
	assert(result && result[0] == '\0');
	free(result);
	free(input);
}

static void test_result_limit(void)
{
	char *result = script_output_extract_limited("abc def", 7, 1, 1, 3);
	assert(result && strcmp(result, "abc") == 0);
	free(result);
	errno = 0;
	assert(script_output_extract_limited("abc def", 7, 1, 1, 2) == NULL);
	assert(errno == EFBIG);
	result = script_output_extract_limited("", 0, 1, 1, 0);
	assert(result && result[0] == '\0');
	free(result);
}

int main(void)
{
	test_result_limit();
	test_positions();
	test_explicit_lengths();
	return 0;
}
