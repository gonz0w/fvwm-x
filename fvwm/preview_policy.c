/* This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or any later version.
 */
#include "config.h"
#include "preview_policy.h"
#include <stdio.h>
#include <string.h>

static bool token(const char *text, size_t length)
{
	return strlen(text) == length &&
		strspn(text, "0123456789abcdef") == length;
}

static bool number(const char *text, unsigned int maximum, unsigned int *value)
{
	unsigned int n = 0;

	if (!*text)
		return false;
	for (; *text; text++)
	{
		if (*text < '0' || *text > '9' ||
		    n > (maximum - (unsigned int)(*text - '0')) / 10)
			return false;
		n = n * 10 + (unsigned int)(*text - '0');
	}
	*value = n;
	return true;
}

bool preview_request_parse(const char *text, struct preview_request *request)
{
	struct preview_request parsed = {0};
	char operation[9], opacity[11], timeout[11], target[PREVIEW_PLAN_TOKEN_MAX + 1], extra;

	memset(request, 0, sizeof(*request));
	if (!text || strnlen(text, 1001) > 1000 || sscanf(text,
	    "%64s %16s %16s %8s %48s %48s %10s %10s %701s %c",
	    parsed.cookie, parsed.serial, parsed.view, operation,
	    parsed.generation, parsed.lease, opacity, timeout,
	    target, &extra) != 9 ||
	    strspn(parsed.cookie, "0123456789abcdef-") !=
		strlen(parsed.cookie) ||
	    !token(parsed.serial, 16) || !token(parsed.view, 16) ||
	    !number(opacity, 100, &parsed.opacity) ||
	    !number(timeout, PREVIEW_TIMEOUT_MAX, &parsed.timeout_ms))
		return false;
	if (strcmp(operation, "prepare") == 0)
	{
		parsed.operation = PREVIEW_PREPARE;
		if (!token(parsed.generation, MODULE_QUERY_GENERATION_LEN) ||
		    strcmp(parsed.lease, "-") != 0 || parsed.opacity ||
		    parsed.timeout_ms < PREVIEW_TIMEOUT_MIN ||
		    (target[0] != '1' && target[0] != '2' && target[0] != '3') ||
		    target[1] != ':')
			return false;
		parsed.resource_kind = (enum preview_resource_kind)(target[0] - '0');
		if (parsed.resource_kind == PREVIEW_RESOURCE_COMPLETE)
		{
			char roles[PREVIEW_PLAN_HEX_LENGTH + 1];

			if (strlen(target) < PREVIEW_PLAN_HEX_LENGTH + 3 ||
			    target[PREVIEW_PLAN_HEX_LENGTH + 2] != ':' ||
			    !background_plan_parse(target + PREVIEW_PLAN_HEX_LENGTH + 3,
				&parsed.plan))
				return false;
			memcpy(roles, target + 2, PREVIEW_PLAN_HEX_LENGTH);
			roles[PREVIEW_PLAN_HEX_LENGTH] = '\0';
			if (!preview_plan_parse(roles, parsed.colors))
				return false;
		}
		else if (!preview_plan_parse(target + 2, parsed.colors))
			return false;
	}
	else if (strcmp(operation, "start") == 0)
	{
		size_t length = strlen(target);

		parsed.operation = PREVIEW_START;
		if (!token(parsed.generation,
				MODULE_QUERY_GENERATION_LEN) ||
		    strcmp(parsed.lease, "-") != 0 ||
		    parsed.timeout_ms < PREVIEW_TIMEOUT_MIN ||
		    !length || length % 2 || length > 2 * PREVIEW_NAME_MAX ||
		    strspn(target, "0123456789abcdef") != length)
			return false;
		for (size_t i = 0; i < length; i += 2)
		{
			unsigned int byte;

			if (sscanf(target + i, "%2x", &byte) != 1 || !byte)
				return false;
			parsed.target[i / 2] = (char)byte;
		}
	}
	else
	{
		if (parsed.opacity || parsed.timeout_ms ||
		    strcmp(target, "-") != 0)
			return false;
		if (strcmp(operation, "query") == 0 ||
		    strcmp(operation, "abandon") == 0)
		{
			parsed.operation = strcmp(operation, "query") == 0 ?
				PREVIEW_QUERY : PREVIEW_ABANDON;
			if (strcmp(parsed.generation, "-") != 0 ||
			    strcmp(parsed.lease, "-") != 0)
				return false;
		}
		else
		{
			if (strcmp(operation, "confirm") == 0)
				parsed.operation = PREVIEW_CONFIRM;
			else if (strcmp(operation, "revert") == 0)
				parsed.operation = PREVIEW_REVERT;
			else if (strcmp(operation, "apply") == 0)
				parsed.operation = PREVIEW_APPLY;
			else
				return false;
			if (!token(parsed.generation,
				MODULE_QUERY_GENERATION_LEN) ||
			    !token(parsed.lease, MODULE_QUERY_GENERATION_LEN))
				return false;
		}
	}
	*request = parsed;
	return true;
}

bool preview_plan_parse(const char *text, uint32_t colors[14][4])
{
	uint32_t parsed[14][4] = {{0}};

	memset(colors, 0, sizeof(parsed));
	if (!text || !token(text, PREVIEW_PLAN_HEX_LENGTH))
		return false;
	for (unsigned int i = 0; i < 14; i++)
	{
		for (unsigned int j = 0; j < 4; j++)
		{
			for (unsigned int k = 0; k < 6; k++)
			{
				char c = *text++;

				parsed[i][j] = (parsed[i][j] << 4) |
					(unsigned int)(c <= '9' ? c - '0' : c - 'a' + 10);
			}
		}
	}
	memcpy(colors, parsed, sizeof(parsed));
	return true;
}

enum preview_publication_result preview_publication_queue_check(size_t queued)
{
	return queued > PREVIEW_PUBLICATION_QUEUE_MAX ?
		PREVIEW_PUBLICATION_RETRY : PREVIEW_PUBLICATION_READY;
}

bool preview_root_notice_matches(uint64_t serial, uint64_t ordinal,
	uint64_t expected_serial, uint64_t expected_ordinal,
	bool synthetic, bool new_value)
{
	return !synthetic && new_value && ordinal &&
		serial == expected_serial && ordinal == expected_ordinal;
}

bool preview_metadata_add(size_t *used, size_t bytes)
{
	if (*used > PREVIEW_METADATA_MAX ||
	    bytes > PREVIEW_METADATA_MAX - *used)
		return false;
	*used += bytes;
	return true;
}

bool preview_generation_room(const char *generation)
{
	/* Complete publication advances root, colorset and menu revisions.
	 * Reserve the same three for recovery; legacy plans use fewer. */
	return generation && token(generation, MODULE_QUERY_GENERATION_LEN) &&
		strcmp(generation + MODULE_QUERY_GENERATION_LEN - 16,
			"fffffffffffffff9") <= 0;
}

enum preview_resource_guard preview_resource_guard_check(uint64_t now_ms,
	uint64_t deadline_ms, bool generation_matches, bool owner_alive)
{
	if (!generation_matches)
		return PREVIEW_RESOURCE_CONFLICT;
	if (!owner_alive)
		return PREVIEW_RESOURCE_OWNER_GONE;
	return now_ms >= deadline_ms ? PREVIEW_RESOURCE_EXPIRED :
		PREVIEW_RESOURCE_VALID;
}

bool preview_resource_colors_supported(int fg_tint, int bg_tint,
	unsigned int fg_alpha)
{
	return fg_tint == 0 && bg_tint == 0 && fg_alpha == 100;
}

bool preview_resource_opacity(enum preview_resource_kind kind, bool restore,
	int captured, int *opacity)
{
	if (!opacity)
		return false;
	*opacity = 0;
	if ((kind != PREVIEW_RESOURCE_HIGH_CONTRAST &&
	     kind != PREVIEW_RESOURCE_OPAQUE_COLORS) ||
	    captured < 0 || captured > 100)
		return false;
	*opacity = restore || kind == PREVIEW_RESOURCE_OPAQUE_COLORS ?
		captured : 100;
	return true;
}

uint64_t preview_deadline(uint64_t now_ms, unsigned int timeout_ms)
{
	return now_ms > UINT64_MAX - timeout_ms ?
		UINT64_MAX : now_ms + timeout_ms;
}

int preview_remaining(uint64_t now_ms, uint64_t deadline_ms)
{
	uint64_t left = deadline_ms > now_ms ? deadline_ms - now_ms : 0;

	return left > PREVIEW_TIMEOUT_MAX ? PREVIEW_TIMEOUT_MAX : (int)left;
}

enum preview_recovery preview_recovery_check(uint64_t now_ms,
	uint64_t deadline_ms, const char *expected, const char *current,
	bool force)
{
	if (!current || !*current || strcmp(expected, current) != 0)
		return PREVIEW_CONFLICT;
	return force || now_ms >= deadline_ms ? PREVIEW_RESTORE : PREVIEW_WAIT;
}
