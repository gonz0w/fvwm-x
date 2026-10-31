/* This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */
#include "config.h"
#include "bin/native_settings_policy.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static const char desktop[] = "0123456789abcdef0123456789abcdef";
static const char run[] = "abcdef0123456789abcdef0123456789";

static bool
parse_fields(const char *fields, ns_request *request)
{
	char line[NS_LINE_MAX];
	int length = snprintf(line, sizeof(line), "FNS1 APPLY %s %s %s\n",
	    desktop, run, fields);

	assert(length > 0 && (size_t)length < sizeof(line));
	return ns_parse(line, (size_t)length, request);
}

static uint32_t
read32(const unsigned char *bytes)
{
	return (uint32_t)bytes[0] | ((uint32_t)bytes[1] << 8) |
	    ((uint32_t)bytes[2] << 16) | ((uint32_t)bytes[3] << 24);
}

static void
test_parser(void)
{
	ns_request request;

	assert(parse_fields("1 1 24 53616e73203132 41647761697461 446d7a",
	    &request));
	assert(!strcmp(request.font, "Sans 12"));
	assert(!strcmp(request.icon, "Adwaita"));
	assert(!strcmp(request.cursor, "Dmz"));
	assert(request.sequence == 1 && request.motion == 1 &&
	    request.cursor_size == 24);
	assert(parse_fields("18446744073709551615 0 128 - - -", &request));
	assert(request.sequence == UINT64_MAX);
	assert(request.font[0] == 0 && request.icon[0] == 0);
	static const char *bad[] = {
		"0 1 0 - - -", "18446744073709551616 1 0 - - -",
		"-1 1 0 - - -", "+1 1 0 - - -", "1 2 0 - - -",
		"1 1 129 - - -", "1 1 -1 - - -", "1 1 0 0 - -",
		"1 1 0 00 - -", "1 1 0 0a - -", "1 1 0 7f - -",
		"1 1 0 c280 - -", "1 1 0 e280a8 - -",
		"1 1 0 e280a9 - -", "1 1 0 c0af - -",
		"1 1 0 eda080 - -", "1 1 0 f4908080 - -",
		"1 1 0 e282 - -", "1 1 0 ff - -",
		"1 1 0 - 2e -", "1 1 0 - 2e2e -",
		"1 1 0 - - 2f746d70", "1 1 0 - c3a9 -",
		"1 1 0 - - 414A", "1 1 0 - - 612062",
		"1 1 0 - - - extra", "1 1 0 - -", "1  1 0 - - -",
		"1 1 0 - - - ", " 1 1 0 - - -", "1\t1 0 - - -"
	};
	for (size_t i = 0; i < sizeof(bad) / sizeof(bad[0]); i++)
	{
		memset(&request, 0xff, sizeof(request));
		assert(!parse_fields(bad[i], &request));
		assert(request.sequence == 0 && request.desktop[0] == 0);
	}
	assert(parse_fields("1 1 0 e282ac - -", &request));
	char font[259], fields[300];
	for (size_t i = 0; i < 258; i++)
		font[i] = i % 2 ? '1' : '4';
	font[256] = 0;
	snprintf(fields, sizeof(fields), "1 1 0 %s - -", font);
	assert(parse_fields(fields, &request));
	assert(strlen(request.font) == 128);
	font[256] = '4';
	font[257] = '1';
	font[258] = 0;
	snprintf(fields, sizeof(fields), "1 1 0 %s - -", font);
	assert(!parse_fields(fields, &request));
	char line[NS_LINE_MAX + 1];
	int length = snprintf(line, sizeof(line),
	    "FNS1 APPLY %s %s 1 1 0 - - -\n", desktop, run);
	assert(length > 0);
	assert(!ns_parse(line, (size_t)length - 1, &request));
	line[11] = 'A';
	assert(!ns_parse(line, (size_t)length, &request));
	memset(line, 'a', sizeof(line));
	line[NS_LINE_MAX] = '\n';
	assert(!ns_parse(line, sizeof(line), &request));
}

static void
test_model_and_codec(void)
{
	ns_model model = {0};
	ns_request first, next;
	unsigned char bytes[NS_LINE_MAX];

	assert(ns_encode(&model, bytes, sizeof(bytes)) == 0);
	assert(parse_fields("1 1 0 - - -", &first));
	next = first;
	next.sequence = 2;
	assert(ns_admit(&model, &next) == NS_STALE);
	assert(ns_admit(&model, &first) == NS_ACCEPT);
	assert(ns_commit(&model, &first));
	assert(model.serial == 1 && model.changed[4] == 1);
	assert(ns_encode(&model, bytes, sizeof(bytes)) == 44);
	assert(bytes[0] == 0 && read32(bytes + 4) == 1);
	assert(read32(bytes + 8) == 1 && bytes[12] == 0);
	assert(!memcmp(bytes + 16, "Gtk/EnableAnimations", 20));
	assert(read32(bytes + 36) == 1 && read32(bytes + 40) == 1);
	assert(ns_admit(&model, &first) == NS_DUPLICATE);
	next = first;
	next.sequence++;
	assert(ns_admit(&model, &next) == NS_ACCEPT);
	assert(!ns_commit(&model, &next));
	assert(model.serial == 1);
	assert(ns_admit(&model, &first) == NS_STALE);
	first = next;
	first.motion = 0;
	assert(ns_admit(&model, &first) == NS_STALE);
	first = next;
	first.run[0] = '0';
	assert(ns_admit(&model, &first) == NS_STALE);
	first = next;
	first.desktop[0] = 'f';
	assert(ns_admit(&model, &first) == NS_STALE);
	assert(parse_fields("3 0 24 53616e73203132 4164 446d7a", &next));
	assert(ns_commit(&model, &next));
	assert(model.serial == 2);
	for (size_t i = 0; i < NS_KEY_COUNT; i++)
		assert(model.changed[i] == 2);
	size_t length = ns_encode(&model, bytes, sizeof(bytes));
	assert(length > 44 && read32(bytes + 8) == 5);
	assert(bytes[12] == 1 && bytes[14] == 12 && bytes[15] == 0);
	assert(!memcmp(bytes + 16, "Gtk/FontName", 12));
	assert(read32(bytes + 28) == 2 && read32(bytes + 32) == 7);
	assert(!memcmp(bytes + 36, "Sans 12", 7) && bytes[43] == 0);
	assert(ns_encode(&model, bytes, length - 1) == 0);
	model.serial = UINT32_MAX;
	next.sequence++;
	next.motion = 1;
	assert(ns_commit(&model, &next));
	assert(model.serial == 0 && model.changed[4] == 0);
	assert(model.changed[0] == 2);
	assert(ns_encode(&model, bytes, sizeof(bytes)) == length);
	assert(read32(bytes + 4) == 0);
	next.sequence++;
	next.font[0] = 0;
	next.icon[0] = 0;
	next.cursor[0] = 0;
	next.cursor_size = 0;
	assert(ns_commit(&model, &next));
	assert(ns_encode(&model, bytes, sizeof(bytes)) == 44);
	assert(model.serial == 1 && model.changed[4] == 0);
}

static void
test_resources(void)
{
	ns_request request;
	char block[NS_BLOCK_MAX], database[NS_RESOURCE_MAX + 1];
	ns_block parsed;
	bool found;
	size_t start;

	assert(parse_fields("1 1 24 - - 41647761697461", &request));
	const char prefix[] = "! user\nOther.key: unchanged\n";
	size_t prefix_length = sizeof(prefix) - 1;
	size_t block_length = ns_resource_block(&request, 0x1234, false,
	    block, sizeof(block));
	assert(block_length > 0);
	memcpy(database, prefix, prefix_length);
	memcpy(database + prefix_length, block, block_length);
	size_t length = prefix_length + block_length;
	assert(ns_resource_scan(database, length, &parsed, &found) ==
	    NS_RESOURCE_OK && found);
	assert(parsed.start == prefix_length && parsed.length == block_length);
	assert(parsed.owner == 0x1234 && !strcmp(parsed.run, run));
	assert(parsed.values.cursor_size == 24 &&
	    !strcmp(parsed.values.cursor, "Adwaita"));
	assert(ns_resource_owned(database, length, block, block_length,
	    false, &start) && start == prefix_length);
	memcpy(database + length, "Other.tail: keep\n", 17);
	assert(!ns_resource_owned(database, length + 17, block, block_length,
	    false, &start));
	assert(ns_resource_owned(database, length + 17, block, block_length,
	    true, &start));
	database[start + 4] = 'X';
	assert(!ns_resource_owned(database, length + 17, block, block_length,
	    true, &start));
	assert(ns_resource_scan("a: b\\\n", 6, &parsed, &found) ==
	    NS_RESOURCE_UNSAFE);
	assert(ns_resource_scan("a: b\\", 5, &parsed, &found) ==
	    NS_RESOURCE_UNSAFE);
	assert(ns_resource_scan("a: b\\\\\n", 7, &parsed, &found) ==
	    NS_RESOURCE_OK && !found);
	assert(ns_resource_scan("a\0b", 3, &parsed, &found) ==
	    NS_RESOURCE_UNSAFE);
	assert(ns_resource_scan("FVWM_NATIVE_SETTINGS", 20, &parsed, &found) ==
	    NS_RESOURCE_UNSAFE);
	memset(database, 'a', sizeof(database));
	assert(ns_resource_scan(database, NS_RESOURCE_MAX, &parsed, &found) ==
	    NS_RESOURCE_OK && !found);
	assert(ns_resource_scan(database, sizeof(database), &parsed, &found) ==
	    NS_RESOURCE_LIMIT);
	block_length = ns_resource_block(&request, 1, true,
	    block, sizeof(block));
	memcpy(database, "Other: v", 8);
	memcpy(database + 8, block, block_length);
	assert(ns_resource_scan(database, 8 + block_length, &parsed, &found) ==
	    NS_RESOURCE_OK && found);
	assert(parsed.start == 8 && parsed.separator);
	assert(ns_resource_owned(database, 8 + block_length, block,
	    block_length, false, &start) && start == 8);
	memcpy(database + 8 + block_length, block, block_length);
	assert(ns_resource_scan(database, 8 + 2 * block_length, &parsed,
	    &found) == NS_RESOURCE_UNSAFE);
	/* A marker beginning after an escaped logical line is unsafe even
	 * when its bytes are otherwise canonical. */
	block_length = ns_resource_block(&request, 1, false,
	    block, sizeof(block));
	memcpy(database, "Other: v\\\n", 10);
	memcpy(database + 10, block, block_length);
	assert(ns_resource_scan(database, 10 + block_length, &parsed,
	    &found) == NS_RESOURCE_UNSAFE);
	request.cursor[0] = 0;
	request.cursor_size = 0;
	assert(ns_resource_block(&request, 1, false, block,
	    sizeof(block)) == 0);
	request.cursor_size = 128;
	assert(ns_resource_block(&request, 1, false, block, sizeof(block)) > 0);
	assert(ns_resource_scan(block, strlen(block), &parsed, &found) ==
	    NS_RESOURCE_OK && found && parsed.values.cursor_size == 128);
	request.cursor_size = 0;
	strcpy(request.cursor, "Dmz");
	assert(ns_resource_block(&request, 1, false, block, sizeof(block)) > 0);
	assert(ns_resource_scan(block, strlen(block), &parsed, &found) ==
	    NS_RESOURCE_OK && found && !strcmp(parsed.values.cursor, "Dmz"));
}

int
main(void)
{
	test_parser();
	test_model_and_codec();
	test_resources();
	puts("native settings parser, serial, codec and resources passed");
	return 0;
}
