/* This program is free software; you can redistribute it and/or modify it
 * under the GNU General Public License, version 2 or any later version. */
#include "config.h"
#include "bin/file_transfer_policy.h"
#include <assert.h>
#include <string.h>

static bool
parse(const char *s, struct ft_command *c)
{
	return ft_command_parse(s, strlen(s), c);
}

int
main(void)
{
	struct ft_command c;
	char path[4110], many[FILE_TRANSFER_MAX_SELECTED * 16 + 17];
	uint64_t n;
	static const char *const invalid[] = {
		"", "{}", "[]",
		"{\"v\":1,\"id\":\"01\",\"command\":\"shutdown\"}",
		"{\"v\":1,\"id\":1,\"command\":\"shutdown\"}",
		"{\"v\":1,\"id\":\"1\",\"command\":\"shutdown\",\"extra\":0}",
		"{\"v\":1,\"id\":\"1\",\"command\":\"shutdown\",\"id\":\"2\"}",
		"{\"v\":1,\"id\":\"1\",\"command\":\"context\",\"epoch\":\"2\","
		"\"generation\":1.5,\"enabled\":true}",
		"{\"v\":1,\"id\":\"1\",\"command\":\"read\",\"time\":0}",
		"{\"v\":1,\"id\":\"1\",\"command\":\"drag-data\","
		"\"transfer\":\"3\",\"action\":\"copy\",\"time\":7,"
		"\"uris\":\"file:///tmp/a\"}",
		"{\"v\":1,\"id\":\"1\",\"command\":\"offer\",\"action\":\"cut\","
		"\"uris\":\"file:///tmp/a\"}",
		"{\"v\":1,\"id\":\"1\",\"command\":\"offer\",\"action\":\"copy\","
		"\"uris\":\"file:///tmp/\\u0000\"}",
		"{\"v\":1,\"id\":\"1\",\"command\":\"complete\","
		"\"transfer\":\"3\",\"action\":\"copy\",\"success\":\"true\"}"
	};
	assert(ft_decimal("18446744073709551615", &n) && n == UINT64_MAX);
	assert(!ft_decimal("18446744073709551616", &n));
	assert(!ft_decimal("0", &n));
	assert(ft_time_valid(1, UINT32_MAX));
	assert(!ft_time_valid(UINT32_MAX, 1));
	assert(!ft_time_valid(0, 0));
	assert(parse("{\"v\":1,\"id\":\"1\",\"command\":\"read\"}", &c));
	assert(c.kind == FT_READ && !c.has_time);
	assert(parse("{\"v\":1,\"id\":\"2\",\"command\":\"admit\","
		"\"transfer\":\"18446744073709551615\"}", &c));
	assert(c.transfer == UINT64_MAX);
	assert(parse("{\"v\":1,\"id\":\"3\",\"command\":\"offer\","
		"\"action\":\"move\",\"uris\":\"file:///tmp/a\\r\\n\",\"time\":7}", &c));
	assert(c.move && c.time == 7 && c.has_time);
	for (size_t i = 0; i < sizeof(invalid) / sizeof(invalid[0]); i++)
		assert(!parse(invalid[i], &c));
	assert(ft_uris_valid("file:///tmp/a%FF\r\n", "host"));
	assert(ft_uris_valid("file://host/tmp/a\nfile://localhost/tmp/b", "host"));
	assert(!ft_uris_valid("file://foreign/tmp/a", "host"));
	assert(!ft_uris_valid("file://host:123/tmp/a", "host"));
	assert(!ft_uris_valid("file:///", "host"));
	assert(!ft_uris_valid("file:///tmp/%2e%2e/a", "host"));
	assert(!ft_uris_valid("file:///tmp/%2e", "host"));
	assert(!ft_uris_valid("file:///tmp/a%00b", "host"));
	assert(!ft_uris_valid("file:///tmp/a?b", "host"));
	assert(!ft_uris_valid("file:///tmp/a%", "host"));
	assert(!ft_uris_valid("https://host/tmp/a", "host"));
	assert(ft_uris_valid("file:///tmp/a/", "host"));
	assert(!ft_uris_valid("file:////", "host"));
	memcpy(path, "file:///", 8);
	memset(path + 8, 'a', 4095);
	path[4103] = 0;
	assert(ft_uris_valid(path, "host"));
	path[4103] = 'a';
	path[4104] = 0;
	assert(!ft_uris_valid(path, "host"));
	many[0] = 0;
	for (size_t i = 0; i < FILE_TRANSFER_MAX_SELECTED; i++)
		strcat(many, "file:///tmp/a\n");
	assert(ft_uris_valid(many, "host"));
	strcat(many, "file:///tmp/a\n");
	assert(!ft_uris_valid(many, "host"));
	return 0;
}
