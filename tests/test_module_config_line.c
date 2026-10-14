/* This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or any later version.
 */
#include "config.h"
#include "libs/Module.h"
#include "libs/defaults.h"
#include "libs/fvwmsignal.h"
#include "libs/safemalloc.h"
#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#define FIXTURE_INPUT 4242
#define FIXTURE_OUTPUT 4243
static unsigned long wire[3 * FvwmPacketMaxSize];
static size_t available, offset;
static unsigned requests, formatted_allocations;
static unsigned char request[3 * sizeof(unsigned long) +
	MAX_MODULE_INPUT_TEXT_LEN];
static size_t request_size;
ssize_t __real_read(int, void *, size_t);
ssize_t __real_write(int, const void *, size_t);

int __wrap_xasprintf(char **output, const char *format, ...)
{
	va_list arguments;

	formatted_allocations++;
	va_start(arguments, format);
	int result = xvasprintf(output, format, arguments);

	va_end(arguments);
	return result;
}

ssize_t __wrap_read(int fd, void *buffer, size_t count)
{
	if (fd != FIXTURE_INPUT)
	{
		return __real_read(fd, buffer, count);
	}
	if (count > 5)
	{
		count = 5;
	}
	if (count > available - offset)
	{
		count = available - offset;
	}
	memcpy(buffer, (char *)wire + offset, count);
	offset += count;
	return count;
}

ssize_t __wrap_write(int fd, const void *buffer, size_t count)
{
	if (fd != FIXTURE_OUTPUT)
	{
		return __real_write(fd, buffer, count);
	}
	assert(count <= sizeof(request));
	memcpy(request, buffer, count);
	request_size = count;
	requests++;
	return count;
}

static void test_request_construction(int *descriptors)
{
	const char prefix[] = "Send_ConfigInfo ";
	char match[MAX_MODULE_INPUT_TEXT_LEN];
	const size_t lengths[] = {0, 1, 7,
		MAX_MODULE_INPUT_TEXT_LEN - (sizeof(prefix) - 1)};

	memset(match, 'x', sizeof(match));
	for (unsigned i = 0; i < sizeof(lengths) / sizeof(lengths[0]); i++)
	{
		unsigned long window, length;
		char saved = match[lengths[i]];

		match[lengths[i]] = 0;
		requests = formatted_allocations = request_size = 0;
		InitGetConfigLine(descriptors, match);
		assert(requests == 1 && formatted_allocations == 0);
		memcpy(&window, request, sizeof(window));
		memcpy(&length, request + sizeof(window), sizeof(length));
		assert(window == 0 &&
			length == sizeof(prefix) - 1 + lengths[i]);
		assert(request_size == 3 * sizeof(window) + length);
		assert(memcmp(request + 2 * sizeof(window), prefix,
			sizeof(prefix) - 1) == 0);
		assert(memcmp(request + 2 * sizeof(window) + sizeof(prefix) - 1,
			match, lengths[i]) == 0);
		match[lengths[i]] = saved;
	}
	match[lengths[3] + 1] = 0;
	requests = 0;
	InitGetConfigLine(descriptors, match);
	assert(requests == 0 && isTerminated);
	char *line = (char *)wire;

	GetConfigLine(descriptors, &line);
	assert(line == NULL);
	isTerminated = 0;
	requests = formatted_allocations = 0;
	InitGetConfigLine(descriptors, NULL);
	assert(requests == 1 && formatted_allocations == 0 && !isTerminated);
	unsigned long length;

	memcpy(&length, request + sizeof(unsigned long), sizeof(length));
	assert(length == sizeof(prefix) - 1);
	assert(memcmp(request + 2 * sizeof(unsigned long), prefix, length) == 0);
}

static unsigned long *append_packet(unsigned long type, size_t body_words)
{
	size_t words = FvwmPacketHeaderSize + body_words;
	unsigned long *packet = (unsigned long *)((char *)wire + available);

	assert(words <= FvwmPacketMaxSize);
	assert(available + words * sizeof(*wire) <= sizeof(wire));
	memset(packet, 0, words * sizeof(*wire));
	packet[0] = START_FLAG;
	packet[1] = type;
	packet[2] = words;
	available += words * sizeof(*wire);
	return packet + FvwmPacketHeaderSize;
}

static void append_line(const char *line)
{
	size_t bytes = strlen(line) + 1;
	size_t text_words = (bytes + sizeof(*wire) - 1) / sizeof(*wire);
	unsigned long *body = append_packet(M_CONFIG_INFO, 3 + text_words);

	memcpy(body + 3, line, bytes);
}

static void reset(int *descriptors)
{
	available = offset = 0;
	isTerminated = 0;
	InitGetConfigLine(descriptors, "*Fixture");
}

static void test_valid_views_and_end(int *descriptors)
{
	const char native_bytes[] = {(char)0xff, 'x', 0};
	const char *lines[] = {"", " \t*Fixture \"comma, # literal\"",
		" \r\n", native_bytes};
	const char *expected[] = {"", "*Fixture \"comma, # literal\"",
		"", native_bytes};
	char *line;

	for (unsigned i = 0; i < sizeof(lines) / sizeof(lines[0]); i++)
	{
		reset(descriptors);
		append_packet(M_NEW_DESK, 1);
		append_line(lines[i]);
		append_packet(M_END_CONFIG_INFO, 0);
		GetConfigLine(descriptors, &line);
		assert(line != NULL && strcmp(line, expected[i]) == 0);
		GetConfigLine(descriptors, &line);
		assert(line == NULL && offset == available);
	}
	char maximum[(FvwmPacketBodyMaxSize - 3) * sizeof(*wire)];

	memset(maximum, ' ', sizeof(maximum) - 1);
	maximum[sizeof(maximum) - 1] = 0;
	reset(descriptors);
	append_line(maximum);
	GetConfigLine(descriptors, &line);
	assert(line != NULL && *line == 0 && offset == available);
}

static void test_refused_views_and_following_line(int *descriptors)
{
	char *line;

	for (unsigned body_words = 0; body_words <= 3; body_words++)
	{
		reset(descriptors);
		append_packet(M_CONFIG_INFO, body_words);
		append_line("following");
		line = (char *)wire;
		GetConfigLine(descriptors, &line);
		assert(line == NULL);
		GetConfigLine(descriptors, &line);
		assert(line != NULL && strcmp(line, "following") == 0);
	}
	const size_t sizes[] = {4, FvwmPacketBodyMaxSize};

	for (unsigned i = 0; i < sizeof(sizes) / sizeof(sizes[0]); i++)
	{
		reset(descriptors);
		unsigned long *body = append_packet(M_CONFIG_INFO, sizes[i]);

		memset(body + 3, 'x', (sizes[i] - 3) * sizeof(*wire));
		GetConfigLine(descriptors, &line);
		assert(line == NULL && offset == available);
	}
	reset(descriptors);
	line = (char *)wire;
	GetConfigLine(descriptors, &line);
	assert(line == NULL);
	append_line("not read after termination");
	isTerminated = 1;
	GetConfigLine(descriptors, &line);
	assert(line == NULL && offset == 0);
	isTerminated = 0;
}

static void test_owned_pipe(void)
{
	int endpoints[2];
	int descriptors[] = {FIXTURE_OUTPUT, FIXTURE_INPUT};
	char *line;

	reset(descriptors);
	append_line("*Fixture actual pipe");
	assert(pipe(endpoints) == 0);
	assert(__real_write(endpoints[1], wire, available) ==
		(ssize_t)available);
	descriptors[1] = endpoints[0];
	GetConfigLine(descriptors, &line);
	assert(line != NULL && strcmp(line, "*Fixture actual pipe") == 0);
	close(endpoints[0]);
	close(endpoints[1]);
}

int main(void)
{
	int descriptors[] = {FIXTURE_OUTPUT, FIXTURE_INPUT};

	test_request_construction(descriptors);
	test_valid_views_and_end(descriptors);
	test_refused_views_and_following_line(descriptors);
	test_owned_pipe();
	puts("bounded configuration views preserve native bytes "
		"and refuse missing text");
	return 0;
}
