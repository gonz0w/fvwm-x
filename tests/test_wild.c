/* This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */
#include "config.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "libs/wild.h"

static unsigned long reference_calls, comparisons;

/* Frozen recursive implementation, independent of the production cursors. */
static int reference_match(const char *pattern, const char *string)
{
	reference_calls++;
	if(string == NULL)
	{
		if(pattern == NULL)
			return 1;
		else if(strcmp(pattern,"*")==0)
			return 1;
		else
			return 0;
	}
	if(pattern == NULL)
		return 1;

	while (*string && *pattern)
	{
		if (*pattern == '?')
		{
			/* match any character */
			pattern += 1;
			string += 1;
		}
		else if (*pattern == '*')
		{
			/* see if the rest of the pattern matches any trailing
			 * substring of the string. */
			pattern += 1;
			if (*pattern == 0)
			{
				return 1; /* trailing * must match rest */
			}
			while (*string)
			{
				if (reference_match(pattern,string))
				{
					return 1;
				}
				string++;
			}
			return 0;
		}
		else
		{
			if (*pattern == '\\')
			{
				/* has strange, but harmless effects if the
				 * last character is a '\\' */
				pattern ++;
			}
			if  (*pattern++ != *string++)
			{
				return 0;
			}
		}
	}
	if((*pattern == 0)&&(*string == 0))
		return 1;
	if((*string == 0)&&(strcmp(pattern,"*")==0))
		return 1;

	return 0;
}


static void compare(const char *pattern, const char *value)
{
	int expected = reference_match(pattern, value);
	int actual = matchWildcards(pattern, value);

	if (expected != actual)
	{
		fprintf(stderr, "pattern=[%s] value=[%s]: %d != %d\n",
			pattern ? pattern : "NULL", value ? value : "NULL",
			actual, expected);
		abort();
	}
	comparisons++;
}

static unsigned word(char *out, unsigned code, const char *alphabet)
{
	unsigned length = 0;

	while (code)
	{
		out[length++] = alphabet[(code - 1) % 5];
		code = (code - 1) / 5;
	}
	out[length] = 0;
	return length;
}

static unsigned random_value(void)
{
	static unsigned state = 0x519bf31U;

	state ^= state << 13;
	state ^= state >> 17;
	state ^= state << 5;
	return state;
}

static void differential(void)
{
	char pattern[32], value[32];
	const char alphabet[] = "ab*?\\";
	const char *special[] = {NULL, "", "*", "**", "***", "\\",
		"\\*", "\\?", "*\\", "*\\*", "a**", "*a**", "a\\",
		"*?*", "*a", "*a?", "*a*b", "*\\a*b", "*a\\*b"};

	for (unsigned p = 0; p < sizeof(special) / sizeof(*special); p++)
		for (unsigned v = 0; v < sizeof(special) / sizeof(*special); v++)
			compare(special[p], special[v]);
	for (unsigned p = 0; p < 3906; p++)
	{
		word(pattern, p, alphabet);
		compare(pattern, NULL);
		for (unsigned v = 0; v < 781; v++)
		{
			word(value, v, alphabet);
			compare(pattern, value);
		}
	}
	for (unsigned n = 0; n < 200000; n++)
	{
		unsigned plen = random_value() % 24;
		unsigned vlen = random_value() % 24;

		for (unsigned i = 0; i < plen; i++)
			pattern[i] = alphabet[random_value() % 5];
		for (unsigned i = 0; i < vlen; i++)
			value[i] = alphabet[random_value() % 5];
		pattern[plen] = value[vlen] = 0;
		compare(pattern, value);
	}
	/* Bytes are compared without locale or signed-char classification. */
	compare("*\200?\\\377", "abc\200z\377");
	compare("*\200\377", "abc\200\377");
	printf("shared wildcard independent legacy comparisons: %lu passed\n",
		comparisons);
}

static void bounded_stack(void)
{
	size_t count = 65536;
	char *pattern = malloc(count + 5);

	assert(pattern);
	memset(pattern, '*', count);
	strcpy(pattern + count, "?*");
	assert(matchWildcards(pattern, "a"));
	assert(!matchWildcards(pattern, ""));
	strcpy(pattern + count, "\\a*");
	assert(matchWildcards(pattern, "a"));
	assert(!matchWildcards(pattern, "b"));
	free(pattern);
	puts("65536 plain/escaped stars retain constant cursor storage: passed");
}

static void measured_cases(void)
{
	char value[4098], suffix[2050];
	const char *patterns[] = {"*a*a*a*a*a*a*b", "*\\a*\\a*\\a*b"};

	for (unsigned i = 0; i < 2; i++)
	{
		reference_calls = 0;
		compare(patterns[i], "aaaaaaaaaaaa");
		printf("shared wildcard family %u: legacy entries=%lu\n",
			i, reference_calls);
	}
	memset(value, 'a', sizeof(value) - 2);
	value[sizeof(value) - 2] = 'b';
	value[sizeof(value) - 1] = 0;
	suffix[0] = '*';
	memset(suffix + 1, 'a', sizeof(suffix) - 3);
	suffix[sizeof(suffix) - 2] = 'b';
	suffix[sizeof(suffix) - 1] = 0;
	reference_calls = 0;
	compare(suffix, value);
	printf("anchored literal suffix: legacy entries=%lu\n", reference_calls);
	suffix[sizeof(suffix) - 2] = 'c';
	compare(suffix, value);
	compare("*abcd", "abc");
	compare("*abcd", "abcd");
	compare("*abcd", "");
}

int main(void)
{
	differential();
	bounded_stack();
	measured_cases();
	return 0;
}
