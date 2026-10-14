/* This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include "config.h"
#include "modules/FvwmScript/script_value.h"
#include <assert.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void test_legacy_numbers(void)
{
	int value;
	const char *zero[] = {"", "text", "+", "-", "0x10"};

	for (size_t i = 0; i < sizeof(zero) / sizeof(zero[0]); i++)
	{
		assert(script_value_integer(zero[i], &value) && value == 0);
	}
	assert(script_value_integer(" \t-23tail", &value) && value == -23);
	assert(script_value_integer("+2147483647.9", &value));
	assert(value == INT_MAX);
	assert(script_value_integer("-2147483648", &value));
	assert(value == INT_MIN);
	assert(!script_value_integer("2147483648", &value));
	assert(value == 0 && errno == ERANGE);
	assert(!script_value_integer("-2147483649tail", &value));
	assert(value == 0 && errno == ERANGE);
	assert(!script_value_integer("999999999999999999999999", &value));
}

static void test_numeric_tags(void)
{
	int value;
	int numbers[] = {INT_MIN, -450000, -350000, -200000, -150000,
		-100000, -99999, -1, 0, 1, INT_MAX - 200000, INT_MAX};

	for (size_t i = 0; i < sizeof(numbers) / sizeof(numbers[0]); i++)
	{
		long encoded = script_value_encode_integer(numbers[i]);
		assert(encoded > 100000);
		assert(script_value_decode_integer(encoded, &value));
		assert(value == numbers[i]);
	}
	assert(!script_value_decode_integer(LONG_MIN, &value) && value == 0);
	assert(!script_value_decode_integer(LONG_MAX, &value) && value == 0);
	assert(!script_value_decode_integer(100001, &value) && value == 0);
	assert(!script_value_decode_integer(script_value_encode_integer(INT_MIN)-1,
		&value) && value == 0);
	assert(!script_value_decode_integer(script_value_encode_integer(INT_MAX)+1,
		&value) && value == 0);
}

static void test_arithmetic(void)
{
	int value;

	assert(script_value_calculate(SCRIPT_VALUE_ADD, INT_MAX, 0, &value));
	assert(value == INT_MAX);
	assert(!script_value_calculate(SCRIPT_VALUE_ADD, INT_MAX, 1, &value));
	assert(value == 0 && errno == ERANGE);
	assert(!script_value_calculate(SCRIPT_VALUE_ADD, INT_MIN, -1, &value));
	assert(script_value_calculate(SCRIPT_VALUE_MULTIPLY, INT_MIN, 1,
		&value) && value == INT_MIN);
	assert(!script_value_calculate(SCRIPT_VALUE_MULTIPLY, INT_MIN, -1,
		&value) && value == 0);
	assert(script_value_calculate(SCRIPT_VALUE_MULTIPLY, 46340, 46340,
		&value) && value == 2147395600);
	assert(!script_value_calculate(SCRIPT_VALUE_MULTIPLY, 46341, 46341,
		&value) && value == 0);
	assert(script_value_calculate(SCRIPT_VALUE_DIVIDE, -7, 3, &value));
	assert(value == -2);
	assert(script_value_calculate(SCRIPT_VALUE_REMAINDER, -7, 3, &value));
	assert(value == -1);
	for (int op = SCRIPT_VALUE_DIVIDE; op <= SCRIPT_VALUE_REMAINDER; op++)
	{
		assert(!script_value_calculate(op, 9, 0, &value));
		assert(value == 0 && errno == EDOM);
		assert(!script_value_calculate(op, INT_MIN, -1, &value));
		assert(value == 0 && errno == EDOM);
	}
	assert(!script_value_calculate(99, 1, 2, &value));
	assert(value == 0 && errno == EINVAL);
}

static void test_hex_numbers(void)
{
	int value;

	assert(script_value_hex_number("#0xFFtail", &value) && value == 255);
	assert(script_value_hex_number("#FFFFFFFF", &value) && value == -1);
	assert(script_value_hex_number("80000000", &value) && value == INT_MIN);
	assert(script_value_hex_number("#-FFFFFFFF", &value) && value == 1);
	assert(script_value_hex_number("-80000000", &value));
	assert(value == INT_MIN);
	assert(script_value_hex_number("not-hex", &value) && value == 0);
	assert(!script_value_hex_number("100000000", &value));
	assert(value == 0 && errno == ERANGE);
	assert(!script_value_hex_number("-100000000", &value));
	assert(!script_value_hex_number("FFFFFFFFFFFFFFFFFFFFFFFF", &value));
}

static void test_hex_padding(void)
{
	char *text = script_value_hex(15, 4);

	assert(strcmp(text, "000F") == 0);
	free(text);
	text = script_value_hex(-1, 16);
	assert(strcmp(text, "00000000FFFFFFFF") == 0);
	free(text);
	text = script_value_hex(255, -1);
	assert(strcmp(text, "FF") == 0);
	free(text);
	text = script_value_hex(0, 0);
	assert(strcmp(text, "0") == 0);
	free(text);
	text = script_value_hex(15, SCRIPT_VALUE_HEX_MAX_WIDTH);
	assert(strlen(text) == SCRIPT_VALUE_HEX_MAX_WIDTH);
	assert(strspn(text, "0") == SCRIPT_VALUE_HEX_MAX_WIDTH - 1);
	assert(text[SCRIPT_VALUE_HEX_MAX_WIDTH - 1] == 'F');
	free(text);
	assert(script_value_hex(1, SCRIPT_VALUE_HEX_MAX_WIDTH + 1) == NULL);
	assert(errno == EOVERFLOW);
}

static void test_substrings(void)
{
	const int bounds[][2] = {{1, 2}, {0, 0}, {-9, 3}, {3, INT_MAX},
		{INT_MAX, INT_MAX}, {5, 2}, {1, 1}};
	const char *expected[] = {"He", "H", "Hel", "llo", "", "", "H"};

	for (size_t i = 0; i < sizeof(bounds) / sizeof(bounds[0]); i++)
	{
		char *text = script_value_copy("Hello",
			bounds[i][0], bounds[i][1]);
		assert(strcmp(text, expected[i]) == 0);
		free(text);
	}
	char *text = script_value_copy("", INT_MIN, INT_MAX);
	assert(text[0] == '\0');
	free(text);
}

static void test_records(void)
{
	const char *expected[] = {"", "", "one", "", "four", ""};
	const char *invalid[] = {"0", "00", "000", "0001", "0005abc",
		"-001x", " 001x", "x001x", "0001ax", "0001a0004abc"};

	for (int i = 0; i < 6; i++)
	{
		char *text = script_value_record("0003one00000004four", i - 1);
		assert(text != NULL && strcmp(text, expected[i]) == 0);
		free(text);
	}
	for (size_t i = 0; i < sizeof(invalid) / sizeof(invalid[0]); i++)
	{
		assert(script_value_record(invalid[i], 1) == NULL);
		assert(errno == EINVAL);
	}
	char *text = script_value_record("", INT_MAX);
	assert(text != NULL && text[0] == '\0');
	free(text);
	/* Refuse a truncated tail even after the selected valid record. */
	assert(script_value_record("0003one000", 1) == NULL);
	char *many = malloc(4 * 2000 + 1);
	assert(many != NULL);
	for (size_t i = 0; i < 2000; i++)
		memcpy(many + 4 * i, "0000", 4);
	many[8000] = '\0';
	text = script_value_record(many, 2000);
	assert(text != NULL && text[0] == '\0');
	free(text);
	free(many);
}

int main(void)
{
	test_legacy_numbers();
	test_numeric_tags();
	test_arithmetic();
	test_hex_numbers();
	test_hex_padding();
	test_substrings();
	test_records();
	return 0;
}
