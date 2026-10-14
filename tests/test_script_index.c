/* This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include "modules/FvwmScript/script_index.h"
#include <assert.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>

static void test_integer_bounds(void)
{
	int value;
	const char *bad[] = {NULL, "", "x", "3x", "3 ", "1.5",
		"9999999999999999999999999999999999999999999999"};
	char text[64];

	for (size_t i = 0; i < sizeof(bad) / sizeof(bad[0]); i++)
	{
		value = 123;
		assert(!script_index_parse(bad[i], INT_MIN, INT_MAX, &value));
		assert(value == 0);
	}
	assert(script_index_parse("+3", 0, 19, &value) && value == 3);
	assert(script_index_parse(" 03", 0, 19, &value) && value == 3);
	assert(script_index_parse("19", 0, 19, &value) && value == 19);
	assert(!script_index_parse("20", 0, 19, &value) && value == 0);
	assert(!script_index_parse("-1", 0, 19, &value));
	assert(!script_index_parse("1", 2, 1, &value));
	snprintf(text, sizeof(text), "%d", INT_MIN);
	assert(script_index_parse(text, INT_MIN, INT_MAX, &value));
	assert(value == INT_MIN);
	snprintf(text, sizeof(text), "%d", INT_MAX);
	assert(script_index_parse(text, INT_MIN, INT_MAX, &value));
	assert(value == INT_MAX);
}

static void test_widget_mapping(void)
{
	int ids[1001];
	const char *bad[] = {"0", "-1", "1001", "2", "", "1x",
		"9999999999999999999999999999999999999999999999"};

	for (size_t i = 0; i < 1001; i++)
		ids[i] = -1;
	ids[1] = 0;
	ids[1000] = 1;
	assert(script_widget_index("1", ids, 1001, 2) == 0);
	assert(script_widget_index("1000", ids, 1001, 2) == 1);
	for (size_t i = 0; i < sizeof(bad) / sizeof(bad[0]); i++)
		assert(script_widget_index(bad[i], ids, 1001, 2) == -1);
	assert(script_widget_index("1", ids, 1001, 0) == -1);
	assert(script_widget_index("1000", ids, 1001, 1) == -1);
	assert(script_widget_index("1", NULL, 1001, 2) == -1);
	assert(script_widget_index("1", ids, 1, 2) == -1);
	assert(script_widget_index("1", ids, SIZE_MAX, 2) == -1);
	ids[1] = INT_MAX;
	assert(script_widget_index("1", ids, 1001, 2) == -1);
}

int main(void)
{
	test_integer_bounds();
	test_widget_mapping();
	return 0;
}
