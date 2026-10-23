/* This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include "config.h"
#include "modules/FvwmForm/FvwmForm.h"
#include "libs/fvwmsignal.h"
#include "modules/FvwmForm/form_input.h"
#include <assert.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>

Item *root_item_ptr;

static void expect(const char *command, const char *expected)
{
	int length = -1;
	char *end = NULL;
	char *result = ParseCommand(0, (char *)command, '\0', &length, &end);

	assert(result != NULL && strcmp(result, expected) == 0);
	assert(length == (int)strlen(expected));
	assert(end == command + strlen(command) + 1);
}

static void test_existing_substitution(void)
{
	Item input = {.input = {.head = {.type = I_INPUT, .name = "input"},
		.value = "abc"}};
	Item empty = {.input = {.head = {.type = I_INPUT, .name = "empty"},
		.value = ""}};
	Item on = {.choice = {.head = {.type = I_CHOICE, .name = "on"},
		.value = "yes", .on = 1}};
	Item off = {.choice = {.head = {.type = I_CHOICE, .name = "off"},
		.value = "no", .on = 0}};
	Item escaped = {.input = {.head = {.type = I_INPUT,
		.name = "escaped\\ name"}, .value = "kept"}};
	Item *choices[] = {&on, &off};
	Item selected = {.selection = {.head = {.type = I_SELECT, .name = "sel"},
		.n = 2, .choices = choices}};
	Item *items[] = {&input, &empty, &on, &off, &escaped, &selected};

	for (size_t i = 0; i + 1 < sizeof(items) / sizeof(items[0]); i++)
	{
		items[i]->header.next = items[i + 1];
	}
	root_item_ptr = &input;
	expect("prefix $(input) suffix", "prefix abc suffix");
	expect("$( i n p u t )", "abc");
	expect("$(escaped\\ name)", "kept");
	expect("$(input?yes)/$(empty?no)/$(empty!yes)", "yes//yes");
	expect("$(on?yes)/$(off?no)/$(off!yes)", "yes//yes");
	expect("$(input?$(on?yes)$(off?no))", "yes");
	expect("$(sel)", "yes");
	expect("$(sel?unused)", "yes");
	expect("$(input?right\\)side)", "right)side");
	expect("$literal/$(missing)", "$literal/");
	/* Preserve these existing byte results; their manual semantics are separate. */
	expect("$(off)", "no");
	expect("$(missing?text)", "text)");
	root_item_ptr = NULL;
}

static void test_long_identifiers_and_output(void)
{
	size_t name_size = 1024, output_size = 262144;
	char *name = malloc(name_size + 1);
	char *command = malloc(name_size + 4);
	char *literal = malloc(output_size + 1);

	assert(name != NULL && command != NULL && literal != NULL);
	memset(name, 'n', name_size);
	name[name_size] = '\0';
	memcpy(command, "$(", 2);
	memcpy(command + 2, name, name_size);
	memcpy(command + name_size + 2, ")", 2);
	Item input = {.input = {.head = {.type = I_INPUT, .name = name},
		.value = "whole-name"}};
	root_item_ptr = &input;
	expect(command, "whole-name");
	memset(literal, 'x', output_size);
	literal[output_size] = '\0';
	expect(literal, literal);
	expect("short", "short");
	root_item_ptr = NULL;
	free(name);
	free(command);
	free(literal);
}

static void test_refusal_and_retry(void)
{
	Item input = {.input = {.head = {.type = I_INPUT, .name = "input"},
		.value = "yes"}};
	const char *commands[] = {"$(", "$(input", "$(input\\",
		"$(input?text", "$(input?$(input)"};
	root_item_ptr = &input;

	for (size_t i = 0; i < sizeof(commands) / sizeof(commands[0]); i++)
	{
		int length = -1;
		char *end = NULL;
		assert(ParseCommand(0, (char *)commands[i], '\0', &length,
			&end) == NULL);
		assert(length == 0 && end >= commands[i] &&
			end <= commands[i] + strlen(commands[i]));
		expect("$(input)", "yes");
	}
	int length = -1;
	char *end = NULL;
	assert(ParseCommand(INT_MAX, "x", '\0', &length, &end) == NULL);
	assert(length == 0);
	assert(ParseCommand(-1, "x", '\0', &length, &end) == NULL);
	root_item_ptr = NULL;
}

static void test_cancelled_command(void)
{
	char command[] = "literal";
	int length = -1;
	char *end = NULL;

	isTerminated = 1;
	assert(ParseCommand(0, command, '\0', &length, &end) == NULL);
	assert(length == 0 && end == command);
	isTerminated = 0;
	expect(command, command);
}

static void test_encoded_field(void)
{
	Item input = {.input = {.head = {.type = I_INPUT, .name = "Title"},
		.value = "a'b\"$;\n"}};
	Item literal = {.input = {.head = {.type = I_INPUT,
		.name = "Title:Base64"}, .value = "existing"}};
	root_item_ptr = &input;
	expect("$(Title:Base64)", "YSdiIiQ7Cg");
	input.input.value = "";
	expect("$(Title:Base64)", "");
	input.header.next = &literal;
	expect("$(Title:Base64)", "existing");
	root_item_ptr = NULL;
	assert(form_command_begin());
	expect("$(FormJob:Token)", "0000000000000001");
	assert(form_command_begin());
	expect("$(FormJob:Token)", "0000000000000002");
}

static void test_list_packet(void)
{
	form_list_state state = {.rows = 12};
	Item list = {.list = {.head = {.type = I_LIST, .name = "picks"},
		.state = &state}};

	root_item_ptr = &list;
	assert(form_list_add(list.list.state, true));
	expect("$(picks:Base64)", "AAAB");
	list.list.state->refused = true;
	int length;
	char *end;
	assert(ParseCommand(0, "$(picks:Base64)", '\0', &length, &end) == NULL);
	root_item_ptr = NULL;
}

int main(void)
{
	test_list_packet();
	test_existing_substitution();
	test_long_identifiers_and_output();
	test_refusal_and_retry();
	test_cancelled_command();
	test_encoded_field();
	return 0;
}
