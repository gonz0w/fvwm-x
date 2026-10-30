/* This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or any later version.
 */
#include "config.h"
#include "session_clients_policy.h"
#include "palette_policy.h"
#include <stdio.h>
#include <string.h>

static bool hex_token(const char *text, size_t length)
{
	return strlen(text) == length &&
		strspn(text, "0123456789abcdef") == length;
}

bool session_clients_parse(const char *text,
	struct session_clients_request *request)
{
	struct session_clients_request parsed = {0};
	char operation[16], extra;

	memset(request, 0, sizeof(*request));
	if (!text || strnlen(text, 1001) > 1000 || sscanf(text,
	    "%64s %16s %15s %48s %48s %c", parsed.cookie,
	    parsed.serial, operation, parsed.target,
	    parsed.generation, &extra) != 5 ||
	    strspn(parsed.cookie, "0123456789abcdef-") !=
		strlen(parsed.cookie) || !hex_token(parsed.serial, 16))
		return false;
	if (strcmp(operation, "delete") == 0)
	{
		if (!hex_token(parsed.target, 48) ||
		    strcmp(parsed.generation, "-") != 0)
			return false;
		parsed.operation = SESSION_CLIENTS_DELETE;
	}
	else if (strcmp(operation, "quit-if-empty") == 0)
	{
		if (strcmp(parsed.target, "-") != 0 ||
		    !hex_token(parsed.generation, 48))
			return false;
		parsed.operation = SESSION_CLIENTS_QUIT_IF_EMPTY;
	}
	else if (strcmp(operation, "delegate-xsmp") == 0)
	{
		if (strcmp(parsed.target, "-") != 0 ||
		    strcmp(parsed.generation, "-") != 0)
			return false;
		parsed.operation = SESSION_CLIENTS_DELEGATE_XSMP;
	}
	else
		return false;
	*request = parsed;
	return true;
}

bool session_clients_membership_update(struct session_clients_membership *state,
	const uint64_t *identities, size_t count)
{
	if (!state->revision || count > SESSION_CLIENTS_MAX ||
	    (count && !identities))
		return false;
	for (size_t i = 0; i < count; i++)
		if (!identities[i])
			return false;
	if (state->count == count && (!count ||
	    memcmp(state->identities, identities,
		count * sizeof(*identities)) == 0))
		return true;
	if (++state->revision == 0)
		return false;
	if (count)
		memcpy(state->identities, identities,
			count * sizeof(*identities));
	state->count = count;
	return true;
}

void session_clients_title(char output[SESSION_CLIENTS_TITLE_MAX + 1],
	const char *input)
{
	char sanitized[PALETTE_TEXT_MAX + 1];
	size_t length, encoded = 0;

	palette_text(sanitized, input, "Unnamed application");
	length = strlen(sanitized);
	for (size_t i = 0; i < length; i++)
		encoded += sanitized[i] == '"' || sanitized[i] == '\\' ? 2 : 1;
	if (encoded > SESSION_CLIENTS_TITLE_MAX)
	{
		encoded = 0;
		for (length = 0; sanitized[length]; length++)
		{
			size_t cost = sanitized[length] == '"' ||
				sanitized[length] == '\\' ? 2 : 1;

			if (encoded + cost > SESSION_CLIENTS_TITLE_MAX - 3)
				break;
			encoded += cost;
		}
		while (length && (sanitized[length] & 0xc0) == 0x80)
			length--;
		memcpy(sanitized + length, "...", 3);
		length += 3;
	}
	memcpy(output, sanitized, length);
	output[length] = '\0';
}
