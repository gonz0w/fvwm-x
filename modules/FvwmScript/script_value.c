/* This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include "config.h"
#include "script_value.h"
#include "libs/safemalloc.h"
#include <errno.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

_Static_assert(INT_MAX <= INT32_MAX, "Script integers need at most 32 bits");

#define SCRIPT_VALUE_INTEGER_BASE 2147683648LL
_Static_assert(LONG_MAX >= 4295167295LL,
	"Full widget integer tags require the supported 64-bit long model");

long script_value_encode_integer(int value)
{
	return (long)(SCRIPT_VALUE_INTEGER_BASE + value);
}

bool script_value_decode_integer(long encoded, int *value)
{
	*value = 0;
	if (encoded < SCRIPT_VALUE_INTEGER_BASE + INT_MIN ||
	    encoded > SCRIPT_VALUE_INTEGER_BASE + INT_MAX)
	{
		errno = ERANGE;
		return false;
	}
	*value = (int)(encoded - SCRIPT_VALUE_INTEGER_BASE);
	return true;
}

bool script_value_integer(const char *text, int *value)
{
	long number;
	char *end;

	*value = 0;
	errno = 0;
	number = strtol(text, &end, 10);
	/* BSD may report EINVAL for no conversion; atoi historically returns
	 * zero here. Distinguish that case from actual numeric overflow. */
	if (end == text)
	{
		errno = 0;
		return true;
	}
	if (errno != 0 || number < INT_MIN || number > INT_MAX)
	{
		errno = ERANGE;
		return false;
	}
	*value = (int)number;
	return true;
}

bool script_value_calculate(script_value_operation operation, int left,
	int right, int *value)
{
	int64_t result;

	*value = 0;
	switch (operation)
	{
	case SCRIPT_VALUE_ADD:
		result = (int64_t)left + right;
		break;
	case SCRIPT_VALUE_MULTIPLY:
		result = (int64_t)left * right;
		break;
	case SCRIPT_VALUE_DIVIDE:
	case SCRIPT_VALUE_REMAINDER:
		if (right == 0 || (left == INT_MIN && right == -1))
		{
			errno = EDOM;
			return false;
		}
		result = operation == SCRIPT_VALUE_DIVIDE ?
			left / right : left % right;
		break;
	default:
		errno = EINVAL;
		return false;
	}
	if (result < INT_MIN || result > INT_MAX)
	{
		errno = ERANGE;
		return false;
	}
	*value = (int)result;
	return true;
}

bool script_value_hex_number(const char *text, int *value)
{
	long long number;
	int64_t signed_number;
	char *end;

	*value = 0;
	if (text[0] == '#')
		text++;
	errno = 0;
	number = strtoll(text, &end, 16);
	if (end == text)
	{
		errno = 0;
		return true;
	}
	if (errno != 0 || number < -(int64_t)UINT32_MAX ||
	    number > UINT32_MAX)
	{
		errno = ERANGE;
		return false;
	}
	/* Preserve the existing signed interpretation of 32-bit hex patterns
	 * without relying on an out-of-range unsigned-to-signed conversion. */
	uint32_t bits = (uint32_t)number;
	signed_number = bits <= INT32_MAX ? bits :
		(int64_t)bits - ((int64_t)UINT32_MAX + 1);
	if (signed_number < INT_MIN || signed_number > INT_MAX)
	{
		errno = ERANGE;
		return false;
	}
	*value = (int)signed_number;
	return true;
}

char *script_value_hex(int value, int width)
{
	char natural[sizeof(unsigned int) * 2 + 1];

	if (width > SCRIPT_VALUE_HEX_MAX_WIDTH)
	{
		errno = EOVERFLOW;
		return NULL;
	}
	int count = snprintf(natural, sizeof(natural), "%X",
		(unsigned int)value);
	size_t length = width > count ? (size_t)width : (size_t)count;
	char *text = fxmalloc(length + 1);
	size_t padding = length - count;
	memset(text, '0', padding);
	memcpy(text + padding, natural, count + 1);
	return text;
}

char *script_value_copy(const char *text, int first, int last)
{
	size_t length = strlen(text);

	if (first < 1)
		first = 1;
	if (last < 1)
		last = 1;
	if (first > last || (size_t)first > length)
		return fxstrdup("");
	size_t end = (size_t)last < length ? (size_t)last : length;
	size_t count = end - (size_t)first + 1;
	char *result = fxmalloc(count + 1);
	memcpy(result, text + first - 1, count);
	result[count] = '\0';
	return result;
}

char *script_value_record(const char *text, int index)
{
	size_t length = strlen(text), position = 0;
	size_t selected = 0, selected_length = 0, record = 1;

	while (position < length)
	{
		size_t count = 0;
		if (length - position < 4)
			goto malformed;
		for (size_t i = 0; i < 4; i++)
		{
			unsigned char digit = text[position + i];
			if (digit < '0' || digit > '9')
				goto malformed;
			count = count * 10 + digit - '0';
		}
		position += 4;
		if (count > length - position)
			goto malformed;
		if (index > 0 && record == (size_t)index)
		{
			selected = position;
			selected_length = count;
		}
		position += count;
		record++;
	}
	char *result = fxmalloc(selected_length + 1);
	memcpy(result, text + selected, selected_length);
	result[selected_length] = '\0';
	return result;
malformed:
	errno = EINVAL;
	return NULL;
}
