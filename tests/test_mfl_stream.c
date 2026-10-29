/* This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include "config.h"
#include "modules/FvwmMFL/stream.h"
#include <assert.h>
#include <string.h>

static void test_every_command_split(void)
{
	const char line[] = "Echo split-command\n";
	char command[MFL_CLIENT_LINE_MAX + 1];
	struct evbuffer *input = evbuffer_new();

	for (size_t split = 0; split < sizeof(line) - 1; split++)
	{
		evbuffer_add(input, line, split);
		assert(mfl_read_command(input, command) ==
		    MFL_STREAM_INCOMPLETE);
		assert(evbuffer_get_length(input) == split);
		evbuffer_add(input, line + split, sizeof(line) - 1 - split);
		assert(mfl_read_command(input, command) == MFL_STREAM_READY);
		assert(strcmp(command, "Echo split-command") == 0);
		assert(evbuffer_get_length(input) == 0);
	}
	evbuffer_add(input, "one\n\ntwo\npartial", 16);
	assert(mfl_read_command(input, command) == MFL_STREAM_READY);
	assert(strcmp(command, "one") == 0);
	assert(mfl_read_command(input, command) == MFL_STREAM_READY);
	assert(command[0] == '\0');
	assert(mfl_read_command(input, command) == MFL_STREAM_READY);
	assert(strcmp(command, "two") == 0);
	assert(mfl_read_command(input, command) ==
		    MFL_STREAM_INCOMPLETE);
	evbuffer_free(input);
}

static void test_limits_and_nul(void)
{
	char command[MFL_CLIENT_LINE_MAX + 1];
	char line[MFL_CLIENT_LINE_MAX + 2];
	struct evbuffer *input = evbuffer_new();

	memset(line, 'x', sizeof(line));
	evbuffer_add(input, line, MFL_CLIENT_LINE_MAX);
	assert(mfl_read_command(input, command) ==
		    MFL_STREAM_INCOMPLETE);
	evbuffer_add(input, "\n", 1);
	assert(mfl_read_command(input, command) == MFL_STREAM_READY);
	assert(strlen(command) == MFL_CLIENT_LINE_MAX);
	evbuffer_add(input, line, MFL_CLIENT_LINE_MAX + 1);
	assert(mfl_read_command(input, command) == MFL_STREAM_INVALID);
	evbuffer_add(input, "\n", 1);
	assert(mfl_read_command(input, command) == MFL_STREAM_INVALID);
	evbuffer_drain(input, evbuffer_get_length(input));
	evbuffer_add(input, "Echo\0hidden\n", 12);
	assert(mfl_read_command(input, command) == MFL_STREAM_INVALID);
	assert(command[0] == '\0');
	evbuffer_free(input);
}

static void test_incremental_packets(void)
{
	unsigned long wire[] = {START_FLAG, MX_ECHO, 5, 10, 123};
	unsigned long packet[FvwmPacketMaxSize];
	struct evbuffer *input = evbuffer_new();

	for (size_t split = 0; split < sizeof(wire); split++)
	{
		evbuffer_add(input, wire, split);
		assert(mfl_read_packet(input, packet) == MFL_STREAM_INCOMPLETE);
		assert(evbuffer_get_length(input) == split);
		evbuffer_add(input, (char *)wire + split, sizeof(wire) - split);
		assert(mfl_read_packet(input, packet) == MFL_STREAM_READY);
		assert(memcmp(wire, packet, sizeof(wire)) == 0);
	}
	const unsigned long invalid_sizes[] = {0, 3, FvwmPacketMaxSize + 1,
	    (unsigned long)-1};
	for (size_t i = 0;
	    i < sizeof(invalid_sizes) / sizeof(invalid_sizes[0]); i++)
	{
		wire[2] = invalid_sizes[i];
		evbuffer_add(input, wire, sizeof(wire));
		assert(mfl_read_packet(input, packet) == MFL_STREAM_INVALID);
		evbuffer_drain(input, evbuffer_get_length(input));
	}
	wire[0] = 0;
	wire[2] = 5;
	evbuffer_add(input, wire, sizeof(wire));
	assert(mfl_read_packet(input, packet) == MFL_STREAM_INVALID);
	evbuffer_free(input);
}

static void test_output_caps_and_wire_format(void)
{
	struct evbuffer *output = evbuffer_new();
	unsigned long window, length, continuation;
	char text[5];
	char overlong[MAX_MODULE_INPUT_TEXT_LEN + 2];

	memset(overlong, 'x', sizeof(overlong));
	overlong[sizeof(overlong) - 1] = '\0';
	assert(!mfl_queue_command(output, overlong));
	assert(evbuffer_get_length(output) == 0);

	assert(mfl_queue_command(output, "Echo"));
	evbuffer_remove(output, &window, sizeof(window));
	evbuffer_remove(output, &length, sizeof(length));
	evbuffer_remove(output, text, 4);
	evbuffer_remove(output, &continuation, sizeof(continuation));
	assert(window == 0 && length == 4 && continuation == 1);
	assert(memcmp(text, "Echo", 4) == 0);
	char padding[MFL_QUEUE_LIMIT];
	memset(padding, 'x', sizeof(padding));
	evbuffer_add(output, padding, MFL_QUEUE_LIMIT - 2);
	assert(mfl_queue_line(output, "x"));
	assert(evbuffer_get_length(output) == MFL_QUEUE_LIMIT);
	assert(!mfl_queue_line(output, "x"));
	assert(!mfl_queue_command(output, "Echo"));
	assert(evbuffer_get_length(output) == MFL_QUEUE_LIMIT);
	evbuffer_free(output);
}

int main(void)
{
	test_every_command_split();
	test_limits_and_nul();
	test_incremental_packets();
	test_output_caps_and_wire_format();
	return 0;
}
