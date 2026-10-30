/* This program is free software; you can redistribute it and/or modify
 * it under the GNU General Public License, version 2 or any later version. */
#include "config.h"
#include "modules/FvwmButtons/buttons_startup.h"
#include <assert.h>
#include <string.h>

int main(void)
{
	buttons_startup_state state = {0};
	buttons_startup_state unset = {0};
	char maximum[MAX_MODULE_INPUT_TEXT_LEN + 1];
	char oversized[MAX_MODULE_INPUT_TEXT_LEN + 2];
	char framed[MAX_MODULE_INPUT_TEXT_LEN + 3];
	char oversized_frame[MAX_MODULE_INPUT_TEXT_LEN + 4];

	assert(!buttons_startup_take(&unset));
	assert(unset.consumed && !buttons_startup_take(&unset));
	assert(!buttons_startup_set(&unset, "Echo too late"));
	assert(!buttons_startup_set(NULL, "Echo absent"));
	assert(!buttons_startup_take(NULL));
	assert(!buttons_startup_set(&state, NULL));
	assert(!buttons_startup_set(&state, ""));
	assert(!buttons_startup_set(&state, " \t "));
	assert(!buttons_startup_set(&state, "\n"));
	assert(!buttons_startup_set(&state, "\r\n"));
	assert(buttons_startup_set(&state, "  Echo first \t"));
	assert(!strcmp(state.command, "Echo first"));
	assert(buttons_startup_set(&state, "Echo physical LF\n"));
	assert(!strcmp(state.command, "Echo physical LF"));
	assert(buttons_startup_set(&state, "Echo physical CRLF\r\n"));
	assert(!strcmp(state.command, "Echo physical CRLF"));
	assert(buttons_startup_set(&state, "Echo last valid"));
	buttons_startup_state before = state;
	assert(!buttons_startup_set(&state, "\nEcho bad"));
	assert(!buttons_startup_set(&state, "Echo bad\r"));
	assert(!buttons_startup_set(&state, "Echo one\nEcho two"));
	assert(!buttons_startup_set(&state, "Echo bad\n\n"));
	assert(!buttons_startup_set(&state, "Echo bad\r\r\n"));
	assert(!buttons_startup_set(&state, "\t"));
	assert(!memcmp(&state, &before, sizeof(state)));
	memset(oversized, 'x', sizeof(oversized) - 1);
	oversized[sizeof(oversized) - 1] = 0;
	assert(!buttons_startup_set(&state, oversized));
	assert(!memcmp(&state, &before, sizeof(state)));
	memset(maximum, 'x', sizeof(maximum) - 1);
	maximum[sizeof(maximum) - 1] = 0;
	assert(buttons_startup_set(&state, maximum));
	assert(strlen(state.command) == MAX_MODULE_INPUT_TEXT_LEN);
	memcpy(framed, maximum, MAX_MODULE_INPUT_TEXT_LEN);
	framed[MAX_MODULE_INPUT_TEXT_LEN] = '\n';
	framed[MAX_MODULE_INPUT_TEXT_LEN + 1] = 0;
	assert(buttons_startup_set(&state, framed));
	assert(!strcmp(state.command, maximum));
	framed[MAX_MODULE_INPUT_TEXT_LEN] = '\r';
	framed[MAX_MODULE_INPUT_TEXT_LEN + 1] = '\n';
	framed[MAX_MODULE_INPUT_TEXT_LEN + 2] = 0;
	assert(buttons_startup_set(&state, framed));
	assert(!strcmp(state.command, maximum));
	framed[MAX_MODULE_INPUT_TEXT_LEN] = 'x';
	assert(!buttons_startup_set(&state, framed));
	assert(!strcmp(state.command, maximum));
	memset(oversized_frame, 'x', MAX_MODULE_INPUT_TEXT_LEN + 1);
	oversized_frame[MAX_MODULE_INPUT_TEXT_LEN + 1] = '\r';
	oversized_frame[MAX_MODULE_INPUT_TEXT_LEN + 2] = '\n';
	oversized_frame[MAX_MODULE_INPUT_TEXT_LEN + 3] = 0;
	assert(!buttons_startup_set(&state, oversized_frame));
	assert(!strcmp(state.command, maximum));
	const char *command = buttons_startup_take(&state);
	assert(command && !strcmp(command, maximum) && state.consumed);
	assert(!buttons_startup_take(&state));
	assert(!buttons_startup_set(&state, "Echo cannot replay"));
	assert(!strcmp(command, maximum));
	return 0;
}
