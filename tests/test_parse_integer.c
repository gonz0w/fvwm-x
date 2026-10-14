/* This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or any later version.
 */
#include "config.h"
#include "libs/Parse.h"
#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>

static void test_integer_tokens(void)
{
	int values[3] = {71, 72, 73};
	char *next;
	char text[] = "'2147483647' -2147483648 tail";
	assert(GetIntegerArguments(text, &next, values, 2) == 2);
	assert(values[0] == INT_MAX && values[1] == INT_MIN);
	assert(strcmp(PeekToken(next, NULL), "tail") == 0);
	assert(GetIntegerArguments("1 2 bad tail", &next, values, 3) == 2);
	assert(strcmp(PeekToken(next, NULL), "tail") == 0);
	assert(GetIntegerArguments("1suffix", NULL, values, 1) == 0);
	assert(GetIntegerArguments("+2 -3", NULL, values, 2) == 2);
	assert(values[0] == 2 && values[1] == -3);
	assert(GetIntegerArgumentsAnyBase("0x7fffffff -020 010", NULL, values, 3) == 3);
	assert(values[0] == INT_MAX && values[1] == -16 && values[2] == 8);
	assert(GetIntegerArgumentsAnyBase("-0x80000000", NULL, values, 1) == 1);
	assert(values[0] == INT_MIN);
	assert(GetIntegerArgumentsAnyBase("08", NULL, values, 1) == 0);
	int suffixes[2] = {0, 0};
	assert(GetSuffixedIntegerArguments("12P -5w", NULL, values, 2,
		"pw", suffixes) == 2);
	assert(values[0] == 12 && values[1] == -5);
	assert(suffixes[0] == 1 && suffixes[1] == 2);
	assert(GetSuffixedIntegerArguments("12ignoredP", NULL, values, 1,
		"pw", suffixes) == 1);
	assert(values[0] == 12 && suffixes[0] == 1);
	assert(GetIntegerArguments(NULL, &next, values, 1) == 0 && next == NULL);
	assert(GetIntegerArguments("1", &next, values, 0) == 0);
}

static void test_rectangle_and_percent(void)
{
	int width = 11, height = 12, value, unit = 37;
	assert(GetRectangleArguments("12x34suffix", &width, &height) == 2);
	assert(width == 12 && height == 34);
	assert(GetRectangleArguments("12--34", &width, &height) == 2);
	assert(width == 12 && height == -34);
	assert(GetRectangleArguments("12-34", &width, &height) == 2);
	assert(width == 12 && height == 34);
	assert(GetRectangleArguments("'12 34'", &width, &height) == 2);
	assert(width == 12 && height == 34);
	assert(GetRectangleArguments("12p34", &width, &height) == 2);
	assert(GetOnePercentArgument("-12P", &value, &unit) == 1);
	assert(value == -12 && unit == 100);
	unit = 37;
	assert(GetOnePercentArgument("12suffix", &value, &unit) == 1);
	assert(value == 12 && unit == 37);
	assert(GetOnePercentArgument(NULL, &value, &unit) == 0 && value == 0);
	int unit2 = 42;
	assert(GetTwoPercentArguments("12p 34", &width, &height, &unit, &unit2) == 2);
	assert(width == 12 && height == 34 && unit == 100 && unit2 == 42);
	assert(GetTwoPercentArguments("12x34", &width, &height, &unit, &unit2) == 2);
	assert(width == 12 && height == 34);
}

static void test_range_refusal(void)
{
	int value = 71, other = 72, unit = 37, suffix = 8;
	const char *bad[] = {"2147483648", "-2147483649",
		"99999999999999999999999999999999999999999"};
	for (size_t i = 0; i < sizeof(bad) / sizeof(bad[0]); i++)
	{
		char text[96];
		snprintf(text, sizeof(text), "%s", bad[i]);
		assert(GetIntegerArguments(text, NULL, &value, 1) == 0);
		assert(GetIntegerArgumentsAnyBase(text, NULL, &value, 1) == 0);
		assert(GetSuffixedIntegerArguments(text, NULL, &value, 1,
			"pw", &suffix) == 0);
		assert(GetOnePercentArgument(text, &value, &unit) == 0);
		assert(value == 0 && unit == 37);
		snprintf(text, sizeof(text), "%sx1", bad[i]);
		value = 71;
		other = 72;
		assert(GetRectangleArguments(text, &value, &other) == 0);
		assert(value == 71 && other == 72);
	}
	assert(GetIntegerArgumentsAnyBase("0x80000000", NULL, &value, 1) == 0);
	assert(GetIntegerArgumentsAnyBase("020000000000", NULL, &value, 1) == 0);
	assert(GetRectangleArguments("1x2147483648", &value, &other) == 0);
	assert(GetOnePercentArgument("2147483648P", &value, &unit) == 0);
	assert(unit == 37);
	assert(GetOnePercentArgument("invalidP", &value, &unit) == 0);
	assert(value == 0 && unit == 37);
}

int main(int argc, char **argv)
{
	if (argc == 2 && strcmp(argv[1], "--baseline") == 0)
	{
		int value = 0, other = 0, unit = 37;
		printf("{\"integer\":%d,", GetIntegerArguments("2147483648", NULL, &value, 1));
		printf("\"any_base\":%d,", GetIntegerArgumentsAnyBase("0x80000000", NULL, &value, 1));
		printf("\"rectangle\":%d,", GetRectangleArguments("2147483648x1", &value, &other));
		printf("\"percent\":%d}\n", GetOnePercentArgument("2147483648P", &value, &unit));
		return 0;
	}
	test_integer_tokens();
	test_rectangle_and_percent();
	test_range_refusal();
	return 0;
}
