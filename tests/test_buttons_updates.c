/* This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or any later version.
 */
#include "config.h"
#include "modules/FvwmButtons/buttons_updates.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

int main(void)
{
	buttons_update_plan plan;
	const char *error;
	const char *invalid[] = {
		NULL, "", "alpha Title ready", "()", "(alpha)",
		"(alpha Title)", "(alpha Title ready,)", "(alpha Title ready",
		"(alpha Title ready))", "(alpha Title ready) junk",
		"(alpha Title ready) (beta Enabled 2)",
		"(alpha Title ready) (beta Bogus 1)",
		"(alpha Title ready, title again)",
		"(alpha Title \"unterminated)", "(alpha Title 'unterminated)",
		"(alpha Title \"x\"junk)", "(alpha Title x'junk)",
		"(alpha Title x\\", "(alpha Title x y)",
		"(alpha Enabled 01)", "(alpha Selected -1)",
		"(1junk Enabled 1)", "(2147483648 Enabled 1)",
		"(+0+0junk Enabled 1)", "(+0 Enabled 1)",
		"(+0-1 Enabled 1)", "(+2147483648+0 Enabled 1)",
		"('' Enabled 1)"
	};
	char input[BUTTONS_UPDATE_BYTES + 2];
	size_t i, used;

	for (i = 0; i < sizeof(invalid) / sizeof(invalid[0]); i++)
	{
		assert(!buttons_parse_updates(invalid[i], &plan, &error));
		assert(error);
	}
	assert(buttons_parse_updates(
		" (alpha Title \"Ready, (now)\", Enabled 1, Selected 0) "
		"(+2+1 ActiveTitle 'hello world', PressTitle \"\") "
		"(0 Icon clock.xpm) ", &plan, &error));
	assert(!error && plan.group_count == 3 && plan.change_count == 6);
	assert(plan.groups[1].first == 3 && plan.groups[1].count == 2);
	assert(!strcmp(plan.changes[0].value, "Ready, (now)"));
	assert(!strcmp(plan.changes[3].value, "hello world"));
	assert(!*plan.changes[4].value);
	assert(buttons_parse_updates("(a Title a\\ b\\,c\\(d\\))",
		&plan, &error));
	assert(!strcmp(plan.changes[0].value, "a b,c(d)"));
	used = 0;
	for (i = 0; i < BUTTONS_UPDATE_GROUPS; i++)
	{
		used += (size_t)snprintf(input + used, sizeof(input) - used, "(%zu Enabled 1)", i);
	}
	assert(buttons_parse_updates(input, &plan, &error));
	assert(plan.group_count == BUTTONS_UPDATE_GROUPS);
	strcpy(input + used, "(32 Enabled 1)");
	assert(!buttons_parse_updates(input, &plan, &error));
	assert(!strcmp(error, "request exceeds 32 buttons"));
	used = 0;
	for (i = 0; i < 16; i++)
	{
		used += (size_t)snprintf(input + used, sizeof(input) - used,
			"(%zu Title a, Icon b, ActiveTitle c, ActiveIcon d, "
			"PressTitle e, PressIcon f, Enabled 1, Selected 0)", i);
	}
	assert(buttons_parse_updates(input, &plan, &error));
	assert(plan.change_count == BUTTONS_UPDATE_CHANGES);
	strcpy(input + used, "(16 Enabled 1)");
	assert(!buttons_parse_updates(input, &plan, &error));
	assert(!strcmp(error, "request exceeds 128 changes"));
	strcpy(input, "(a Title ");
	memset(input + 9, 'a', BUTTONS_UPDATE_BYTES - 10);
	input[BUTTONS_UPDATE_BYTES - 1] = ')';
	input[BUTTONS_UPDATE_BYTES] = '\0';
	assert(buttons_parse_updates(input, &plan, &error));
	assert(strlen(plan.changes[0].value) == BUTTONS_UPDATE_BYTES - 10);
	input[BUTTONS_UPDATE_BYTES] = ' ';
	input[BUTTONS_UPDATE_BYTES + 1] = '\0';
	assert(!buttons_parse_updates(input, &plan, &error));
	assert(!strcmp(error, "request exceeds 8192 bytes"));
	assert(buttons_parse_updates("(a Title recovered)", &plan, &error));
	assert(plan.group_count == 1 && plan.change_count == 1);
	return 0;
}
