/* This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */
#include "config.h"
#include "libs/Strings.h"

#include <assert.h>
#include <ctype.h>
#include <locale.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static unsigned long cases, length_calls;
static bool baseline;

/* This standalone policy driver owns allocation without the UI library. */
void *fxmalloc(size_t length)
{
	void *result = malloc(length);

	assert(result != NULL && length > 0);
	return result;
}

size_t __real_strlen(const char *text);
size_t __wrap_strlen(const char *text)
{
	length_calls++;
	return __real_strlen(text);
}

/* Independent streaming oracle: accumulate bytes, remembering the last
 * non-whitespace output position. No backward pointer or trim loop. */
static void expected(char *output, const char *input, bool line, bool quoted)
{
	size_t used = 0, kept = 0;
	bool leading = true, remove_quote = false;

	if (quoted)
	{
		while (*input == ' ')
			input++;
		if (*input == '"')
		{
			remove_quote = true;
			input++;
		}
	}
	for (; *input && (!line || *input != '\n'); input++)
	{
		bool space = isspace((unsigned char)*input) != 0;

		if (leading && space)
			continue;
		leading = false;
		output[used++] = *input;
		if (!space)
			kept = used;
	}
	if (remove_quote && kept && output[kept - 1] == '"')
		kept--;
	output[kept] = '\0';
}

static void check(const char *input)
{
	char reference[8192], *result;

	assert(__real_strlen(input) < sizeof(reference));
	expected(reference, input, true, false);
	CopyString(&result, input);
	assert(result != input && strcmp(result, reference) == 0);
	free(result);
	expected(reference, input, false, false);
	result = stripcpy(input);
	assert(result != input && strcmp(result, reference) == 0);
	free(result);
	expected(reference, input, true, true);
	length_calls = 0;
	CopyStringWithQuotes(&result, input);
	unsigned long calls = length_calls;

	const char *opening = input;
	while (*opening == ' ')
		opening++;
	assert(calls == (baseline && *opening == '"' ? 1UL : 0UL));
	assert(result != input && strcmp(result, reference) == 0);
	free(result);
	cases++;
}

static void literals(void)
{
	const char *inputs[] = {"", "\n", " \t\nsecond", "\t\r\v\f",
		" value \t\r\nsecond", "\"value\"", " \"value \" \t",
		"\t\"value\"", "\"\"", "\"", "\" \nvalue",
		" \"value\nrest\"", " 'value' ", "a\nb", "a\r\nb"};
	char *result = (char *)1;

	CopyString(&result, NULL);
	assert(result == NULL);
	result = (char *)1;
	CopyStringWithQuotes(&result, NULL);
	assert(result == NULL && stripcpy(NULL) == NULL);
	for (size_t i = 0; i < sizeof(inputs) / sizeof(*inputs); i++)
		check(inputs[i]);
	for (unsigned value = 1; value < 256; value++)
	{
		char bytes[] = {' ', (char)value, 'x', (char)value, ' ', 0};
		check(bytes);
	}
	char long_input[8192];
	memset(long_input, 'x', sizeof(long_input) - 1);
	long_input[0] = '"';
	long_input[sizeof(long_input) - 2] = '"';
	long_input[sizeof(long_input) - 1] = 0;
	check(long_input);
	char mutable[] = " owned ";
	CopyString(&result, mutable);
	memset(mutable, 'z', sizeof(mutable) - 1);
	assert(strcmp(result, "owned") == 0);
	free(result);
}

static void combinations(void)
{
	const char alphabet[] = {' ', '\t', '\n', '"', 'a', (char)0xe9};
	char text[6];
	unsigned limit = 1;

	for (unsigned length = 0; length < sizeof(text); length++)
	{
		for (unsigned code = 0; code < limit; code++)
		{
			unsigned remainder = code;
			for (unsigned i = 0; i < length; i++)
			{
				text[i] =
					alphabet[remainder % sizeof(alphabet)];
				remainder /= sizeof(alphabet);
			}
			text[length] = 0;
			check(text);
		}
		limit *= sizeof(alphabet);
	}
}

int main(int argc, char **argv)
{
	baseline = argc == 2 && strcmp(argv[1], "--baseline") == 0;
	assert(argc == 1 || baseline);
	assert(setlocale(LC_CTYPE, "C") != NULL);
	literals();
	combinations();
	printf("C locale: %lu string cases, three operations each\n", cases);
	unsigned long first = cases;
	const char *locale = setlocale(LC_CTYPE, "");
	if (locale && strcmp(locale, "C") != 0)
	{
		literals();
		combinations();
		printf("%s locale: %lu additional cases\n",
			locale, cases - first);
	}
	return 0;
}
