/* This program is free software; you can redistribute it and/or modify
 * it under the GNU General Public License, version 2 or any later version. */
#include "config.h"
#include "buttons_startup.h"
#include <stddef.h>
#include <string.h>

bool buttons_startup_set(buttons_startup_state *state, const char *text)
{
	size_t length = 0, first = 0;

	if (!state || state->consumed || !text)
		return false;
	/* Include at most one physical CRLF terminator in the bounded scan. */
	while (length <= MAX_MODULE_INPUT_TEXT_LEN + 2 && text[length])
		length++;
	if (length > MAX_MODULE_INPUT_TEXT_LEN + 2)
		return false;
	if (length && text[length - 1] == '\n')
	{
		length--;
		if (length && text[length - 1] == '\r')
			length--;
	}
	if (length > MAX_MODULE_INPUT_TEXT_LEN)
		return false;
	for (size_t i = 0; i < length; i++)
		if (text[i] == '\r' || text[i] == '\n')
			return false;
	while (first < length && (text[first] == ' ' || text[first] == '\t'))
		first++;
	while (length > first &&
	    (text[length - 1] == ' ' || text[length - 1] == '\t'))
		length--;
	if (length == first)
		return false;
	memmove(state->command, text + first, length - first);
	state->command[length - first] = 0;
	return true;
}

const char *buttons_startup_take(buttons_startup_state *state)
{
	if (!state || state->consumed)
		return NULL;
	state->consumed = true;
	return state->command[0] ? state->command : NULL;
}
