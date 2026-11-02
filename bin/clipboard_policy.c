/* This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */
#include "config.h"
#include "bin/clipboard_policy.h"
#include <string.h>

bool clipboard_numbers(const char *text, size_t length, uint64_t *values,
	size_t count)
{
	size_t offset = 0;

	if (!text || !values || !count)
		return false;
	for (size_t i = 0; i < count; i++)
	{
		uint64_t value = 0;
		size_t start = offset;

		while (offset < length && text[offset] >= '0' &&
			text[offset] <= '9')
		{
			unsigned digit = text[offset++] - '0';

			if (value > (UINT64_MAX - digit) / 10)
				return false;
			value = value * 10 + digit;
		}
		if (offset == start || (offset - start > 1 && text[start] == '0'))
			return false;
		values[i] = value;
		if (i + 1 < count && (offset >= length || text[offset++] != ' '))
			return false;
	}
	return offset == length;
}

bool clipboard_configure(const char *const *arguments,
	struct clipboard_config *output)
{
	uint64_t values[7] = {0};

	if (!output)
		return false;
	*output = (struct clipboard_config){0};
	if (!arguments)
		return false;
	for (size_t i = 0; i < 7; i++)
		if (!arguments[i] || !clipboard_numbers(arguments[i],
			strlen(arguments[i]), &values[i], 1) || !values[i])
			return false;
	if (values[1] > CONTENT_ITEMS_MAX || values[2] > 32768 ||
		values[3] < values[2] || values[3] > values[1] * values[2] ||
		values[4] > SIZE_MAX || values[5] > values[4])
		return false;
	*output = (struct clipboard_config){.epoch = values[0],
		.items = (size_t)values[1], .bytes = (size_t)values[2],
		.total = (size_t)values[3], .mapped = (size_t)values[4],
		.locked = (size_t)values[5], .lifetime_ms = values[6]};
	return true;
}

bool clipboard_request_parse(const char *text, size_t length, uint64_t epoch,
	uint64_t previous, struct clipboard_request *output)
{
	uint64_t values[CLIPBOARD_FIELDS] = {0};
	bool reference;

	if (!output)
		return false;
	*output = (struct clipboard_request){0};
	if (!clipboard_numbers(text, length, values, CLIPBOARD_FIELDS) ||
		values[0] != 1 || !values[1] || values[1] <= previous ||
		values[2] < CLIPBOARD_STATUS || values[2] > CLIPBOARD_QUIT ||
		values[3] != epoch)
		return false;
	reference = values[2] == CLIPBOARD_PIN || values[2] == CLIPBOARD_REMOVE;
	if (reference ? (!values[4] || !values[5]) : (values[4] || values[5]))
		return false;
	if ((values[2] == CLIPBOARD_PIN && values[6] > 1) ||
		(values[2] == CLIPBOARD_STATE && values[6] > CLIPBOARD_LOCKED) ||
		(values[2] != CLIPBOARD_PIN && values[2] != CLIPBOARD_STATE &&
		 values[2] != CLIPBOARD_LIST && values[6]))
		return false;
	*output = (struct clipboard_request){.sequence = values[1],
		.verb = (enum clipboard_verb)values[2],
		.ref = {values[3], values[4], values[5]}, .value = values[6]};
	return true;
}

enum content_hint clipboard_targets(const uint32_t *atoms, size_t count,
	uint32_t text, uint32_t sensitive, bool *supported)
{
	bool hint = false;

	if (!supported)
		return CONTENT_HINT_MALFORMED;
	*supported = false;
	if ((!atoms && count) || count > 256 || !text || !sensitive)
		return CONTENT_HINT_MALFORMED;
	for (size_t i = 0; i < count; i++)
	{
		if (!atoms[i])
		{
			*supported = false;
			return CONTENT_HINT_MALFORMED;
		}
		hint |= atoms[i] == sensitive;
		*supported |= atoms[i] == text;
	}
	if (hint)
	{
		*supported = false;
		return CONTENT_HINT_SENSITIVE;
	}
	return CONTENT_HINT_ABSENT;
}
