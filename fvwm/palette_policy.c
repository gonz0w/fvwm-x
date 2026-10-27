/* This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or any later version.
 */
#include "config.h"
#include "palette_policy.h"
#include <inttypes.h>
#include <stdio.h>
#include <string.h>

_Static_assert(PALETTE_OWNER_MAX * (sizeof(struct palette_slot) +
	PALETTE_ROW_MAX * sizeof(struct palette_row)) <= PALETTE_BUDGET_MAX,
	"palette snapshot storage must stay within its global budget");

static bool hex_token(const char *text, size_t length)
{
	return strlen(text) == length &&
		strspn(text, "0123456789abcdef") == length;
}

bool palette_request_parse(const char *text, struct palette_request *request)
{
	struct palette_request parsed = {0};
	char index[11], extra;
	unsigned int n = 0;

	memset(request, 0, sizeof(*request));
	if (!text || strnlen(text, 1001) > 1000 || sscanf(text,
	    "%64s %16s %48s %10s %c", parsed.cookie, parsed.serial,
	    parsed.generation, index, &extra) != 4 ||
	    strspn(parsed.cookie, "0123456789abcdef-") !=
		strlen(parsed.cookie) || !hex_token(parsed.serial, 16))
		return false;
	if (strcmp(parsed.generation, "-") == 0 && strcmp(index, "-") == 0)
		parsed.release = true;
	else
	{
		if (!hex_token(parsed.generation, 48) || !*index)
			return false;
		for (const char *p = index; *p; p++)
		{
			if (*p < '0' || *p > '9' ||
			    n > (PALETTE_ROW_MAX - 1) / 10 ||
			    n * 10 + (unsigned int)(*p - '0') >=
				PALETTE_ROW_MAX)
				return false;
			n = n * 10 + (unsigned int)(*p - '0');
		}
		parsed.index = n;
	}
	*request = parsed;
	return true;
}

bool palette_next_identity(uint64_t *next, uint64_t *identity)
{
	*identity = 0;
	if (!*next)
		return false;
	*identity = *next;
	/* Zero permanently refuses allocation after exhaustion; never reuse. */
	*next = *next == UINT64_MAX ? 0 : *next + 1;
	return true;
}

int palette_slot_select(const struct palette_slot *slots, uintptr_t owner)
{
	int vacant = -1;

	if (!owner)
		return -1;
	for (int i = 0; i < PALETTE_OWNER_MAX; i++)
	{
		if (slots[i].owner == owner)
			return i;
		if (!slots[i].owner && vacant == -1)
			vacant = i;
	}
	return vacant;
}

bool palette_slot_matches(const struct palette_slot *slot, uintptr_t owner,
	const char *cookie, const char *generation)
{
	return owner && slot->owner == owner &&
		strcmp(slot->cookie, cookie) == 0 &&
		(!generation || strcmp(slot->generation, generation) == 0);
}

bool palette_page_valid(size_t offset, size_t limit, size_t count)
{
	return count <= PALETTE_ROW_MAX && offset <= count &&
		limit > 0 && limit <= 16;
}

/* Validate one scalar without reading beyond the bounded source prefix. */
static size_t utf8_scalar(const unsigned char *s, size_t remaining)
{
	size_t bytes;
	uint32_t value;

	if (s[0] < 0x80)
		return 1;
	if (s[0] >= 0xc2 && s[0] <= 0xdf)
		bytes = 2, value = s[0] & 0x1f;
	else if (s[0] >= 0xe0 && s[0] <= 0xef)
		bytes = 3, value = s[0] & 0x0f;
	else if (s[0] >= 0xf0 && s[0] <= 0xf4)
		bytes = 4, value = s[0] & 7;
	else
		return 0;
	if (bytes > remaining)
		return 0;
	for (size_t i = 1; i < bytes; i++)
	{
		if ((s[i] & 0xc0) != 0x80)
			return 0;
		value = (value << 6) | (s[i] & 0x3f);
	}
	if ((bytes == 3 && value < 0x800) ||
	    (bytes == 4 && value < 0x10000) || value > 0x10ffff ||
	    (value >= 0xd800 && value <= 0xdfff))
		return 0;
	return bytes;
}

bool palette_text(char output[PALETTE_TEXT_MAX + 1], const char *input,
	const char *fallback)
{
	size_t length, used = 0, i = 0;
	bool exact = true;

	if (!input || !*input)
		input = fallback;
	length = strnlen(input, PALETTE_TEXT_MAX + 1);
	while (i < length && used < PALETTE_TEXT_MAX)
	{
		const unsigned char *s = (const unsigned char *)input + i;
		size_t bytes = utf8_scalar(s, length - i);

		if (!bytes || s[0] < 0x20 || s[0] == 0x7f ||
		    (bytes == 2 && s[0] == 0xc2 && s[1] <= 0x9f))
		{
			output[used++] = '?';
			i += bytes ? bytes : 1;
			exact = false;
			continue;
		}
		if (bytes > PALETTE_TEXT_MAX - used)
			break;
		memcpy(output + used, input + i, bytes);
		used += bytes;
		i += bytes;
	}
	if (i < length)
	{
		/* ASCII dots leave a valid scalar boundary when truncating. */
		while (used > PALETTE_TEXT_MAX - 3)
		{
			used--;
			while (used && (output[used] & 0xc0) == 0x80)
				used--;
		}
		memcpy(output + used, "...", 3);
		used += 3;
		exact = false;
	}
	output[used] = '\0';
	return exact;
}

void palette_row_id(char output[PALETTE_ID_MAX + 1],
	const char *incarnation, const struct palette_row *row)
{
	if (row->kind == PALETTE_WINDOW)
		snprintf(output, PALETTE_ID_MAX + 1, "%.32s-w-%016" PRIx64,
			incarnation, row->identity);
	else
		snprintf(output, PALETTE_ID_MAX + 1,
			"%.32s-d-%016" PRIx64 "-%016" PRIx64 "-%08" PRIx32,
			incarnation, row->output, row->role,
			(uint32_t)row->desk);
}
