/* This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */
#include "config.h"
#include "modules/FvwmScript/script_command.h"
#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

static void test_append_limits(void)
{
	char *text = malloc(1);
	size_t length = 0;
	char value[SCRIPT_COMMAND_MAX_BYTES + 2];

	assert(text);
	text[0] = '\0';
	memset(value, 'x', sizeof(value));
	value[SCRIPT_COMMAND_MAX_BYTES] = '\0';
	assert(script_text_append(&text, &length, value,
		SCRIPT_COMMAND_MAX_BYTES));
	assert(length == SCRIPT_COMMAND_MAX_BYTES && strcmp(text, value) == 0);
	char *original = text;
	assert(!script_text_append(&text, &length, "x",
		SCRIPT_COMMAND_MAX_BYTES));
	assert(text == original && length == SCRIPT_COMMAND_MAX_BYTES);
	assert(script_text_append(&text, &length, "",
		SCRIPT_COMMAND_MAX_BYTES));
	length = SIZE_MAX;
	assert(!script_text_append(&text, &length, "", SIZE_MAX));
	assert(!script_text_append(&text, &length, "x", SIZE_MAX));
	assert(text && length == SIZE_MAX);
	free(text);
}

static void test_launch_assembly(void)
{
	char *command = script_launch_command("FvwmScript", "demo", "id10");
	assert(command && strcmp(command, "FvwmScript demo id10 ") == 0);
	free(command);
	command = script_launch_command("FvwmScript", " \tdemo  arg", "id10");
	assert(command && strcmp(command, "FvwmScript demo id10   arg") == 0);
	free(command);
	assert(script_launch_command("FvwmScript", " \t", "id10") == NULL);
	char filename[SCRIPT_COMMAND_MAX_BYTES + 2];
	memset(filename, 'a', sizeof(filename));
	filename[200] = '\0';
	command = script_launch_command("FvwmScript", filename, "id10");
	assert(command && strlen(command) == 200 + 10 + 4 + 3);
	free(command);
	filename[200] = 'a';
	filename[SCRIPT_COMMAND_MAX_BYTES] = '\0';
	/* Filename fits by itself, but the whole framed command must fit. */
	assert(script_launch_command("FvwmScript", filename, "id10") == NULL);
	filename[SCRIPT_COMMAND_MAX_BYTES] = 'a';
	filename[SCRIPT_COMMAND_MAX_BYTES + 1] = '\0';
	assert(script_launch_command("FvwmScript", filename, "id10") == NULL);
}

int main(void)
{
	test_append_limits();
	test_launch_assembly();
	return 0;
}
