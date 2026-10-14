/* This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or any later version.
 */
#include "config.h"
#include "libs/module_monitor.h"
#include "libs/Module.h"
#include "libs/Parse.h"
#include <assert.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>

static void check_round_trip(const char *name)
{
	int values[MODULE_MONITOR_VALUES] = {INT_MIN, INT_MAX, 1, -1, 0, 80,
		60, -2, 800, 600, 10, 20, 30, 40};
	int parsed[MODULE_MONITOR_VALUES];
	char output[(FvwmPacketBodyMaxSize - 3) * sizeof(unsigned long)];
	char *token = NULL;

	assert(module_monitor_format(output, sizeof(output), name, values));
	char *next = GetNextToken(output, &token);
	assert(strcmp(token, "Monitor") == 0);
	free(token);
	next = GetNextToken(next, &token);
	assert(strcmp(token, name) == 0);
	free(token);
	assert(module_parse_decimal_integers(next, parsed,
		MODULE_MONITOR_VALUES));
	assert(memcmp(values, parsed, sizeof(values)) == 0);
	const size_t needed = strlen(output) + 1;
	assert(module_monitor_format(output, needed, name, values));
	assert(!module_monitor_format(output, needed - 1, name, values));
	assert(output[0] == '\0');
}

static void test_names_and_capacity(void)
{
	const char *names[] = {"DP-1", "two monitors", "quote'and\"tick`",
		"back\\slash", " leading trailing ", "tab\tnewline\nname"};
	for (size_t i = 0; i < sizeof(names) / sizeof(names[0]); i++)
	{
		check_round_trip(names[i]);
	}
	char name[MAX_TOKEN_LENGTH + 1];
	memset(name, 'a', sizeof(name));
	name[MAX_TOKEN_LENGTH - 1] = '\0';
	check_round_trip(name);
	name[MAX_TOKEN_LENGTH - 1] = 'a';
	name[MAX_TOKEN_LENGTH] = '\0';
	char output[2048];
	int values[MODULE_MONITOR_VALUES] = {0};
	assert(!module_monitor_format(output, sizeof(output), name, values));
	assert(output[0] == '\0');
	assert(!module_monitor_format(output, sizeof(output), NULL, values));
	assert(!module_monitor_format(output, sizeof(output), "", values));
	assert(!module_monitor_format(output, sizeof(output), "DP-1", NULL));
	assert(!module_monitor_format(NULL, sizeof(output), "DP-1", values));
	assert(!module_monitor_format(output, 0, "DP-1", values));
	assert(module_monitor_format(output, sizeof(output), "DP-1", values));
	assert(strcmp(output, "Monitor DP-1 0 0 0 0 0 0 0 0 0 0 0 0 0 0") == 0);
}

static void test_work_area(void)
{
	int values[MODULE_MONITOR_VALUES] = {0};
	module_monitor_area initial = {-800, -600, 800, 600}, result;
	values[10] = 10;
	values[11] = 20;
	values[12] = 30;
	values[13] = 40;
	assert(module_monitor_prepare_area(values, &initial, false, &result));
	assert(result.x == -790 && result.y == -570);
	assert(result.width == 770 && result.height == 530);
	assert(module_monitor_prepare_area(values, &initial, true, &result));
	assert(memcmp(&initial, &result, sizeof(result)) == 0);
	assert(module_monitor_prepare_area(values, &initial, false, &initial));
	assert(initial.x == -790 && initial.width == 770);
	initial = (module_monitor_area){INT_MAX, 0, 1, 1};
	assert(!module_monitor_prepare_area(values, &initial, true, &result));
	assert(result.width == 0 && result.x == 0);
	initial = (module_monitor_area){INT_MIN, 0, 1, 1};
	assert(module_monitor_prepare_area(values, &initial, true, &result));
	values[10] = -1;
	assert(!module_monitor_prepare_area(values, &initial, false, &result));
	initial = (module_monitor_area){0, 0, 800, 600};
	values[10] = INT_MIN;
	values[11] = INT_MIN;
	assert(!module_monitor_prepare_area(values, &initial, false, &result));
	values[10] = 800;
	values[11] = 0;
	assert(!module_monitor_prepare_area(values, &initial, false, &result));
	assert(module_monitor_prepare_area(values, &initial, true, &result));
	assert(!module_monitor_prepare_area(NULL, &initial, false, &result));
	assert(result.width == 0);
	assert(!module_monitor_prepare_area(values, NULL, false, &result));
	assert(!module_monitor_prepare_area(values, &initial, false, NULL));
}

int main(void)
{
	test_names_and_capacity();
	test_work_area();
	return 0;
}
