/* This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */
#include "config.h"
#include "bin/clipboard_policy.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static void frames(void)
{
	struct clipboard_request request;
	const char *valid[] = {"1 1 1 19 0 0 0", "1 2 2 19 0 0 3",
		"1 3 3 19 7 2 1", "1 4 4 19 7 2 0", "1 5 5 19 0 0 0",
		"1 6 6 19 0 0 2", "1 7 7 19 0 0 0"};
	const char *invalid[] = {"", "1 0 1 19 0 0 0", "1 1 1 20 0 0 0",
		"1 1 1 19 1 0 0", "1 1 3 19 0 0 1", "1 1 4 19 1 2 1",
		"1 1 6 19 0 0 3", "1 1 8 19 0 0 0", "1 1 1 19 0 0 0 0",
		"1 1 1 19 0 0 0\n", "1  1 1 19 0 0 0", "1 01 1 19 0 0 0",
		"1 +1 1 19 0 0 0", "1 1 1 19 0 0 -1",
		"1 18446744073709551616 1 19 0 0 0"};

	for (size_t i = 0; i < sizeof(valid) / sizeof(*valid); i++)
	{
		assert(clipboard_request_parse(valid[i], strlen(valid[i]), 19,
			0, &request));
		assert(request.sequence == i + 1);
		assert(!clipboard_request_parse(valid[i], strlen(valid[i]), 19,
			request.sequence, &request));
		assert(request.sequence == 0);
	}
	for (size_t i = 0; i < sizeof(invalid) / sizeof(*invalid); i++)
	{
		memset(&request, 0xff, sizeof(request));
		assert(!clipboard_request_parse(invalid[i], strlen(invalid[i]),
			19, 0, &request));
		assert(request.sequence == 0 && request.ref.item == 0);
	}
	const char *last = "1 18446744073709551615 1 19 0 0 0";
	assert(clipboard_request_parse(last, strlen(last), 19, UINT64_MAX - 1,
		&request));
	assert(!clipboard_request_parse(last, strlen(last), 19, UINT64_MAX,
		&request));
}

static void limits(void)
{
	const char *arguments[] = {"19", "4", "32768", "131072", "196608",
		"131072", "10000"};
	struct clipboard_config config;

	assert(clipboard_configure(arguments, &config));
	assert(config.epoch == 19 && config.items == 4 && config.bytes == 32768);
	arguments[2] = "32769";
	assert(!clipboard_configure(arguments, &config));
	assert(config.epoch == 0);
	arguments[2] = "32768";
	arguments[1] = "4097";
	assert(!clipboard_configure(arguments, &config));
	arguments[1] = "4";
	arguments[5] = "196609";
	assert(!clipboard_configure(arguments, &config));
	arguments[5] = "131072";
	arguments[6] = "0";
	assert(!clipboard_configure(arguments, &config));
}

static void discovery(void)
{
	bool supported = true;
	uint32_t normal[] = {11, 12, 13}, sensitive[] = {11, 14};
	uint32_t malformed[] = {11, 0};

	assert(clipboard_targets(normal, 3, 11, 14, &supported) ==
		CONTENT_HINT_ABSENT && supported);
	assert(clipboard_targets(sensitive, 2, 11, 14, &supported) ==
		CONTENT_HINT_SENSITIVE && !supported);
	assert(clipboard_targets(malformed, 2, 11, 14, &supported) ==
		CONTENT_HINT_MALFORMED && !supported);
	assert(clipboard_targets(NULL, 0, 11, 14, &supported) ==
		CONTENT_HINT_ABSENT && !supported);
	assert(clipboard_targets(NULL, 1, 11, 14, &supported) ==
		CONTENT_HINT_MALFORMED && !supported);
	assert(clipboard_targets(normal, 257, 11, 14, &supported) ==
		CONTENT_HINT_MALFORMED && !supported);
}

int main(void)
{
	frames();
	limits();
	discovery();
	puts("clipboard policy: frames, limits and sensitivity discovery passed");
	return 0;
}
