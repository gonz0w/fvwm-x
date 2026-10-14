/* This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include "config.h"
#include "modules/FvwmForm/form_input.h"
#include "libs/Module.h"
#include "libs/safemalloc.h"
#include <assert.h>
#include <limits.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

static size_t allocated_copy_size;

/* Observe helper allocation requests without an allocator-specific size API. */
void *fxmalloc(size_t size)
{
	void *copy = malloc(size);

	assert(copy != NULL);
	allocated_copy_size = size;
	return copy;
}

static void test_packet_views(void)
{
	const unsigned long types[] = {M_CONFIG_INFO, M_STRING, M_ERROR};

	for (size_t words = 0; words <= FvwmPacketBodyMaxSize; words++)
	{
		unsigned long *body = words ? malloc(words * sizeof(*body)) : NULL;

		assert(words == 0 || body != NULL);
		if (body != NULL)
		{
			memset(body, 'x', words * sizeof(*body));
		}
		for (size_t i = 0; i < sizeof(types) / sizeof(types[0]); i++)
		{
			assert(!form_packet_is_valid(types[i], body, words));
		}
		if (words > 3)
		{
			((char *)body)[words * sizeof(*body) - 1] = '\0';
			for (size_t i = 0; i < sizeof(types) / sizeof(types[0]); i++)
			{
				assert(form_packet_is_valid(types[i], body, words));
			}
		}
		assert(form_packet_is_valid(MX_PROPERTY_CHANGE, body, words) ==
			(words >= 3));
		free(body);
	}
	assert(form_packet_is_valid(0, NULL, 0));
	assert(!form_packet_is_valid(M_ERROR, NULL, 4));
	assert(!form_packet_is_valid(0, NULL, FvwmPacketBodyMaxSize + 1));
}

static void test_text_ownership(void)
{
	const char *quoted[][2] = {
		{"", ""}, {"trailing\\", "trailing\\"},
		{"a\\\"b\" remainder", "a\"b"},
		{"one\\ two\nrest", "one two"},
	};
	const char *tokens[][2] = {
		{"", ""}, {"trailing\\", "trailing\\"},
		{"one\\ two rest", "one\\ two"}, {"one\trest", "one"},
	};

	for (size_t i = 0; i < sizeof(quoted) / sizeof(quoted[0]); i++)
	{
		char *copy = form_copy_quoted(quoted[i][0]);
		assert(strcmp(copy, quoted[i][1]) == 0);
		free(copy);
	}
	for (size_t i = 0; i < sizeof(tokens) / sizeof(tokens[0]); i++)
	{
		char *copy = form_copy_token(tokens[i][0]);
		assert(strcmp(copy, tokens[i][1]) == 0);
		free(copy);
	}
	char blank[] = "", newline[] = "\n", line[] = "text\n";
	assert(!form_chomp_line(blank) && !form_chomp_line(newline));
	assert(form_chomp_line(line) && strcmp(line, "text") == 0);
}

static void test_timeout_preparation(void)
{
	form_timeout_args args;
	const char *bad[] = {"", "20", "20 Nop", "20 \"Nop", "20 Nop \"text"};
	for (size_t i = 0; i < sizeof(bad) / sizeof(bad[0]); i++)
	{
		memset(&args, 0xff, sizeof(args));
		assert(!form_parse_timeout(bad[i], &args));
		assert(args.command == NULL && args.text == NULL && args.ticks == 0);
	}
	assert(form_parse_timeout("20 \"Nop \\\"quoted\\\"\" \"Remaining %%\"", &args));
	assert(args.ticks == 20 && args.quoted_command && args.quoted_text);
	char *command = form_copy_quoted(args.command);
	char *text = form_copy_quoted(args.text);
	assert(strcmp(command, "Nop \"quoted\"") == 0);
	assert(strcmp(text, "Remaining %%") == 0);
	free(command);
	free(text);
	assert(form_parse_timeout("999999999999999999999 Nop text", &args));
	assert(args.ticks == 99999 && !args.quoted_command && !args.quoted_text);
	assert(form_parse_timeout("-999999999999999999999 Nop \"\"", &args));
	assert(args.ticks == 0);
	assert(form_parse_timeout("12tail Nop \"text\"", &args));
	assert(args.ticks == 12 && args.command_length == 3);
	assert(form_parse_timeout("other Nop \"text\"", &args));
	assert(args.ticks == 0);
}

static void test_button_preparation(void)
{
	form_button_args args;
	const char *bad[] = {"", " \t", "continue \"", "quit \"unfinished\\"};

	for (size_t i = 0; i < sizeof(bad) / sizeof(bad[0]); i++)
	{
		memset(&args, 0xff, sizeof(args));
		assert(!form_parse_button(bad[i], &args));
		assert(args.mode == NULL && args.text == NULL && args.key == NULL);
	}
	assert(form_parse_button("continue \"Say \\\"hello\\\"\" F12", &args));
	assert(args.quoted_text && strcmp(args.key, "F12") == 0);
	char *copy = form_copy_quoted(args.text);
	assert(strcmp(copy, "Say \"hello\"") == 0);
	free(copy);
	assert(form_parse_button("restart \"\" ^M", &args));
	assert(args.quoted_text && strcmp(args.key, "^M") == 0);
	assert(form_parse_button("continue", &args));
	assert(!args.quoted_text && *args.key == '\0');
	assert(form_parse_button("continue F1", &args));
	assert(!args.quoted_text && strcmp(args.key, "F1") == 0);
}

static void test_argument_cursor(void)
{
	const char empty[] = "", final[] = "10", trailing[] = "10 \t\n";
	const char fields[] = "10\t\"initial text\"";

	assert(form_next_argument(empty) == empty);
	assert(form_next_argument(final) == final + 2);
	assert(form_next_argument(trailing) == trailing + strlen(trailing));
	assert(strcmp(form_next_argument(fields), "\"initial text\"") == 0);
}

static void test_copy_excludes_following_fields(void)
{
	char input[2048];

	memset(input, 'x', sizeof(input));
	memcpy(input, "short\"", 6);
	input[sizeof(input) - 1] = '\0';
	char *copy = form_copy_quoted(input);
	assert(strcmp(copy, "short") == 0 && allocated_copy_size == 6);
	free(copy);
	input[5] = ' ';
	copy = form_copy_token(input);
	assert(strcmp(copy, "short") == 0 && allocated_copy_size == 6);
	free(copy);
	copy = form_copy_token("one\\ two rest");
	assert(strcmp(copy, "one\\ two") == 0 && allocated_copy_size == 9);
	free(copy);
	copy = form_copy_quoted("one\\\"two\" rest");
	assert(strcmp(copy, "one\"two") == 0 && allocated_copy_size == 9);
	free(copy);
}

static void test_data_preparation(void)
{
	form_data_args args;
	const char *bad[] = {"", " \n", "file", "file \t"};

	for (size_t i = 0; i < sizeof(bad) / sizeof(bad[0]); i++)
	{
		memset(&args, 0xff, sizeof(args));
		assert(!form_parse_data(bad[i], &args));
		assert(args.filename == NULL && args.prefix == NULL);
	}
	assert(form_parse_data("file\\ name\tData_ remaining", &args));
	char *filename = form_copy_token(args.filename);
	char *prefix = form_copy_token(args.prefix);
	assert(strcmp(filename, "file\\ name") == 0);
	assert(strcmp(prefix, "Data_") == 0);
	free(filename);
	free(prefix);
}

static void test_array_capacity(void)
{
	int capacity, current = 0, growths = 0;

	for (int used = 0; used < 256; used++)
	{
		assert(form_array_capacity(current, used, 1, 8,
			sizeof(char *), &capacity));
		assert(capacity > used);
		growths += current != capacity;
		current = capacity;
	}
	assert(growths == 6 && current == 256);
	assert(form_array_capacity(32, 30, 2, 32, sizeof(char *), &capacity));
	assert(capacity == 32);
	assert(form_array_capacity(32, 31, 2, 32, sizeof(char *), &capacity));
	assert(capacity == 64);
	assert(form_array_capacity(8, 8, 1, 8, sizeof(char *), &capacity));
	assert(capacity == 16);
	assert(form_array_capacity(INT_MAX - 1, INT_MAX - 1, 1, 8, 1,
		&capacity));
	assert(capacity == INT_MAX);
	assert(!form_array_capacity(INT_MAX, INT_MAX, 1, 8, 1, &capacity));
	assert(capacity == 0);
	assert(!form_array_capacity(-1, 0, 1, 8, 1, &capacity));
	assert(!form_array_capacity(0, -1, 1, 8, 1, &capacity));
	assert(!form_array_capacity(0, 0, 0, 8, 1, &capacity));
	assert(!form_array_capacity(0, 0, 1, 0, 1, &capacity));
	assert(!form_array_capacity(0, 0, 1, 8, 0, &capacity));
	assert(form_array_capacity(0, 0, 1, 8, SIZE_MAX, &capacity));
	assert(capacity == 1);
	assert(!form_array_capacity(0, 1, 1, 8, SIZE_MAX, &capacity));
	assert(capacity == 0);
}

static void test_input_dimensions(void)
{
	int width, x, y;
	form_input_args args;
	const char *bad[] = {"", "field", "field 0", "field -1",
		"field other", "field 999999999999999999999"};

	for (size_t i = 0; i < sizeof(bad) / sizeof(bad[0]); i++)
	{
		memset(&args, 0xff, sizeof(args));
		assert(!form_parse_input(bad[i], &args));
		assert(args.name == NULL && args.initial == NULL && args.width == 0);
	}
	assert(form_parse_input("field\\ name 10tail \"initial\"", &args));
	assert(args.width == 10 && strcmp(args.initial, "\"initial\"") == 0);
	char *name = form_copy_token(args.name);
	assert(strcmp(name, "field\\ name") == 0);
	free(name);
	assert(form_parse_input("field 1", &args));
	assert(*args.initial == '\0');
	assert(form_input_width("2147483647", &width) && width == INT_MAX);
	assert(!form_input_width("2147483648", &width) && width == 0);
	assert(form_input_geometry(10, 13, 12, 15, &x, &y));
	assert(x == 77 && y == 28);
	assert(form_input_geometry(65523, 2, 12, 15, &x, &y));
	assert(x == 65535 && y == 17);
	assert(!form_input_geometry(65524, 2, 12, 15, &x, &y));
	assert(x == 0 && y == 0);
	assert(!form_input_geometry(INT_MAX, INT_MAX, 12, 15, &x, &y));
	assert(!form_input_geometry(0, 13, 12, 15, &x, &y));
	assert(!form_input_geometry(1, 0, 12, 15, &x, &y));
	assert(!form_input_geometry(1, 65521, 12, 15, &x, &y));
	assert(form_input_geometry(1, 65520, 12, 15, &x, &y));
	assert(y == 65535);
}

static void test_history_navigation(void)
{
	char *values[50] = {0};
	int position = 99;

	assert(!form_history_position(values, 50, 0, -1, &position));
	assert(position == -1);
	assert(!form_history_position(values, 50, 0, 1, &position));
	assert(!form_history_position(NULL, 50, 0, -1, &position));
	assert(!form_history_position(values, 0, 0, -1, &position));
	assert(!form_history_position(values, 50, -1, -1, &position));
	assert(!form_history_position(values, 50, 51, 1, &position));
	assert(!form_history_position(values, 50, 0, INT_MAX, &position));
	values[0] = "first";
	values[7] = "last";
	assert(form_history_position(values, 50, 8, -1, &position));
	assert(position == 7);
	assert(form_history_position(values, 50, 0, -1, &position));
	assert(position == 7);
	assert(form_history_position(values, 50, 7, 1, &position));
	assert(position == 0);
	for (int i = 0; i < 50; i++)
	{
		values[i] = "filled";
	}
	for (int i = 0; i < 50; i++)
	{
		assert(form_history_position(values, 50, i, -1, &position));
		assert(position == (i ? i - 1 : 49));
		assert(form_history_position(values, 50, i, 1, &position));
		assert(position == (i < 49 ? i + 1 : 0));
	}
	assert(form_history_position(values, 50, 50, -1, &position));
	assert(position == 49);
}

static void test_input_insertion_capacity(void)
{
	int capacity, bytes, characters;

	assert(form_input_growth(4, 3, 2, 2, 1,
		&capacity, &bytes, &characters));
	assert(capacity >= 6 && bytes == 5 && characters == 3);
	assert(form_input_growth(32, 0, 0, 1, 1,
		&capacity, &bytes, &characters));
	assert(capacity == 32 && bytes == 1 && characters == 1);
	assert(form_input_growth(INT_MAX, INT_MAX - 2, INT_MAX - 2, 1, 1,
		&capacity, &bytes, &characters));
	assert(capacity == INT_MAX && bytes == INT_MAX - 1);
	assert(!form_input_growth(INT_MAX, INT_MAX - 1, 0, 1, 1,
		&capacity, &bytes, &characters));
	assert(capacity == 0 && bytes == 0 && characters == 0);
	assert(!form_input_growth(4, 3, INT_MAX, 1, 1,
		&capacity, &bytes, &characters));
	assert(!form_input_growth(4, 3, 2, SIZE_MAX, 1,
		&capacity, &bytes, &characters));
	assert(!form_input_growth(4, 4, 0, 1, 1,
		&capacity, &bytes, &characters));
	assert(!form_input_growth(0, 0, 0, 1, 1,
		&capacity, &bytes, &characters));
	assert(!form_input_growth(4, -1, 0, 1, 1,
		&capacity, &bytes, &characters));
	assert(!form_input_growth(4, 0, -1, 1, 1,
		&capacity, &bytes, &characters));
	assert(!form_input_growth(4, 0, 0, 0, 1,
		&capacity, &bytes, &characters));
	assert(!form_input_growth(4, 0, 0, 1, 0,
		&capacity, &bytes, &characters));
}


int main(void)
{
	char *joined = fxmalloc(5);
	memcpy(joined, "base", 5);
	assert(form_append_text(&joined, " tail", 9));
	assert(strcmp(joined, "base tail") == 0);
	assert(!form_append_text(&joined, " overflow", 9));
	assert(strcmp(joined, "base tail") == 0);
	free(joined);
	test_packet_views();
	test_text_ownership();
	test_timeout_preparation();
	test_button_preparation();
	test_argument_cursor();
	test_copy_excludes_following_fields();
	test_data_preparation();
	test_array_capacity();
	test_input_dimensions();
	test_history_navigation();
	test_input_insertion_capacity();
	assert(form_readiness_timeout(false, false) == -1);
	assert(form_readiness_timeout(false, true) == 100);
	assert(form_readiness_timeout(true, false) == 0);
	assert(form_readiness_timeout(true, true) == 0);
	return 0;
}
