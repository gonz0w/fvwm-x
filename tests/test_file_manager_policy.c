/* This program is free software; you can redistribute it and/or modify it
 * under the GNU General Public License, version 2 or any later version. */
#include "config.h"
#include "bin/file_manager_policy.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static bool parse(const char *line, struct file_manager_reply *reply)
{
	return file_manager_reply_parse(line, strlen(line), reply);
}

int main(void)
{
	struct file_manager_reply reply;
	char long_text[FILE_MANAGER_REPLY_BYTES + 2];
	static const char nul[] =
		"{\"version\":1,\"id\":\"1\",\"status\":\"accepted\","
		"\"detail\":\"ok\"}\0ignored";
	static const char *const invalid[] = {
		"{}", "[]", "null", "",
		"{\"version\":2,\"id\":\"1\",\"status\":\"accepted\","
		"\"detail\":\"\"}",
		"{\"version\":1,\"id\":1,\"status\":\"accepted\","
		"\"detail\":\"\"}",
		"{\"version\":1,\"id\":\"0\",\"status\":\"accepted\","
		"\"detail\":\"\"}",
		"{\"version\":1,\"id\":\"01\",\"status\":\"accepted\","
		"\"detail\":\"\"}",
		"{\"version\":1,\"id\":\"-1\",\"status\":\"accepted\","
		"\"detail\":\"\"}",
		"{\"version\":1,\"id\":\"18446744073709551616\","
		"\"status\":\"accepted\",\"detail\":\"\"}",
		"{\"version\":1,\"id\":\"1\",\"status\":\"complete\","
		"\"detail\":\"\"}",
		"{\"version\":1,\"id\":\"1\",\"status\":\"accepted\"}",
		"{\"version\":1,\"id\":\"1\",\"status\":\"accepted\","
		"\"detail\":0}",
		"{\"version\":1,\"id\":\"1\",\"status\":\"accepted\","
		"\"detail\":\"\",\"extra\":true}",
		"{\"version\":1,\"id\":\"1\",\"status\":\"accepted\","
		"\"detail\":\"\",\"id\":\"2\"}",
		"{\"version\":1,\"id\":\"1\",\"status\":\"accepted\","
		"\"detail\":{\"nested\":true}}",
		"{\"version\":1,\"id\":\"1\",\"status\":\"accepted\","
		"\"detail\":\"\\u0000hidden\"}",
		"{\"version\":1,\"id\":\"1\",\"status\":\"accepted\","
		"\"detail\":\"\\ud800\"}",
		"{\"version\":1,\"id\":\"1\",\"status\":\"accepted\","
		"\"detail\":\"\\n\"}",
		"{\"version\":1,\"id\":\"1\",\"status\":\"accepted\","
		"\"detail\":\"ok\"}false"
	};
	static const char *const status[] = {
		"accepted", "invalid", "refused", "unknown"
	};
	char frame[256];

	for (size_t i = 0; i < sizeof(status) / sizeof(status[0]); i++)
	{
		assert(snprintf(frame, sizeof(frame),
			"{\"version\":1,\"id\":\"18446744073709551615\","
			"\"status\":\"%s\",\"detail\":\"receipt\"}",
			status[i]) > 0);
		assert(parse(frame, &reply));
		assert(reply.id == UINT64_MAX);
		assert(reply.status == (enum file_manager_reply_status)i);
		assert(strcmp(reply.detail, "receipt") == 0);
	}
	for (size_t i = 0; i < sizeof(invalid) / sizeof(invalid[0]); i++)
		assert(!parse(invalid[i], &reply));
	assert(!file_manager_reply_parse(nul, sizeof(nul) - 1, &reply));
	/* Accept the exact frame bound, then reject one extra byte. */
	memset(long_text, ' ', sizeof(long_text));
	memcpy(long_text, frame, strlen(frame));
	assert(file_manager_reply_parse(long_text,
		FILE_MANAGER_REPLY_BYTES, &reply));
	assert(!file_manager_reply_parse(long_text,
		FILE_MANAGER_REPLY_BYTES + 1, &reply));
	memset(long_text, 'x', sizeof(long_text));
	long_text[sizeof(long_text) - 1] = '\0';
	assert(!parse(long_text, &reply));
	assert(file_manager_text_valid("", 0));
	assert(file_manager_text_valid("abc", 3));
	assert(!file_manager_text_valid("abcd", 3));
	assert(!file_manager_text_valid(NULL, 3));
	assert(file_manager_text_valid("\xc3\xa9\xf0\x9f\x98\x80", 6));
	assert(!file_manager_text_valid("\xc0\xaf", 2));
	assert(!file_manager_text_valid("\xed\xa0\x80", 3));
	assert(!file_manager_text_valid("\xf4\x90\x80\x80", 4));
	assert(!file_manager_text_valid("\xf0\x9f", 2));
	assert(!file_manager_text_valid("\x80", 1));
	assert(!file_manager_text_valid("\xc2\x80", 2));
	assert(!file_manager_text_valid("line\nbreak", 10));
	assert(!file_manager_text_valid("\x7f", 1));
	return 0;
}
