/* This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or any later version.
 */
#include "config.h"
#include "libs/Module.h"
#include "libs/defaults.h"
#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static Screen screen = {.width = 1920, .height = 1080};
static _XPrivDisplay display;
static unsigned case_number;

static void check(const char *input, rectangle *geometry, char *foreground,
	char *background, const char *expected)
{
	char *result = module_expand_action((Display *)display, 0,
		(char *)input, geometry, foreground, background);

	case_number++;
	if (expected != NULL &&
		(result == NULL || strcmp(result, expected) != 0))
	{
		fprintf(stderr, "Case %u: expected %.80s; got %.80s\n",
			case_number, expected, result ? result : "NULL");
	}
	if (expected == NULL)
	{
		assert(result == NULL);
	}
	else
	{
		assert(result != NULL && strcmp(result, expected) == 0);
		assert(strlen(result) <= MAX_MODULE_INPUT_TEXT_LEN);
	}
	free(result);
}

static void test_tokens(void)
{
	rectangle geometry = {10, 20, 30, 40};

	check("", NULL, NULL, NULL, "");
	check(NULL, NULL, NULL, NULL, NULL);
	check("Echo 'quoted, text' # literal", NULL, NULL, NULL,
		"Echo 'quoted, text' # literal");
	check("$ $$ $$$ $$$$ $unknown $FgTAIL $BG", &geometry,
		"red", "blue", "$ $ $$ $$ $unknown redTAIL blue");
	/* Preserve the existing missing-background '$g' spelling in this
	 * bounds/arithmetic repair, alongside the other literal fallbacks. */
	check("$fg $bg $left $-left $width", NULL, NULL, NULL,
		"$fg $g $left $-left $width");
	check("$left $-left $right $-right $top $-top $bottom $-bottom "
		"$width $height", &geometry, NULL, NULL,
		"10 1909 40 1879 20 1059 60 1019 30 40");
}

static void test_capacity(void)
{
	char input[MAX_MODULE_INPUT_TEXT_LEN + 8];
	char expected[MAX_MODULE_INPUT_TEXT_LEN + 2];
	rectangle geometry = {0, 0, 1, 1};

	memset(input, 'x', sizeof(input));
	memset(expected, 'x', sizeof(expected));
	for (size_t length = MAX_MODULE_INPUT_TEXT_LEN - 1;
		length <= MAX_MODULE_INPUT_TEXT_LEN + 1; length++)
	{
		input[length] = expected[length] = 0;
		check(input, NULL, NULL, NULL,
			length <= MAX_MODULE_INPUT_TEXT_LEN ? expected : NULL);
		input[length] = expected[length] = 'x';
	}
	input[MAX_MODULE_INPUT_TEXT_LEN - 1] = '$';
	input[MAX_MODULE_INPUT_TEXT_LEN] = 0;
	expected[MAX_MODULE_INPUT_TEXT_LEN - 1] = '$';
	expected[MAX_MODULE_INPUT_TEXT_LEN] = 0;
	check(input, NULL, NULL, NULL, expected);
	expected[MAX_MODULE_INPUT_TEXT_LEN - 1] = 'x';
	input[MAX_MODULE_INPUT_TEXT_LEN - 1] = 'x';
	strcpy(input + MAX_MODULE_INPUT_TEXT_LEN, "$fg");
	check(input, NULL, "", NULL, expected);
	check(input, NULL, "y", NULL, NULL);
	check(input, NULL, NULL, NULL, NULL);
	strcpy(input + MAX_MODULE_INPUT_TEXT_LEN - 1, "$width");
	expected[MAX_MODULE_INPUT_TEXT_LEN - 1] = '1';
	check(input, &geometry, NULL, NULL, expected);
	geometry.width = INT_MIN;
	check(input, &geometry, NULL, NULL, NULL);
	memset(input, 'r', MAX_MODULE_INPUT_TEXT_LEN);
	input[MAX_MODULE_INPUT_TEXT_LEN] = 0;
	check("$fg", NULL, input, NULL, input);
	input[MAX_MODULE_INPUT_TEXT_LEN] = 'r';
	input[MAX_MODULE_INPUT_TEXT_LEN + 1] = 0;
	check("$bg", NULL, NULL, input, NULL);
}

static void test_wide_geometry(void)
{
	rectangle geometry = {INT_MAX, INT_MIN, INT_MAX, INT_MIN};
	char expected[256];

	snprintf(expected, sizeof(expected), "%lld %lld %lld %lld %lld %lld",
		(long long)INT_MAX * 2, (long long)INT_MIN * 2,
		1920LL - INT_MAX - 1, 1920LL - INT_MAX * 2LL - 1,
		1080LL - INT_MIN - 1, 1080LL - INT_MIN * 2LL - 1);
	check("$right $bottom $-left $-right $-top $-bottom", &geometry,
		NULL, NULL, expected);
}

int main(void)
{
	display = calloc(1, sizeof(*display));
	assert(display != NULL);
	display->screens = &screen;
	test_tokens();
	test_capacity();
	test_wide_geometry();
	free(display);
	puts("module action expansion controls passed");
	return 0;
}
