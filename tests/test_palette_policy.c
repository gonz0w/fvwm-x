/* This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or any later version.
 */
#include "config.h"
#include "fvwm/palette_policy.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static const char generation[] =
	"0123456789abcdef0123456789abcdef0000000000000001";

static void test_requests(void)
{
	struct palette_request request;
	char text[1002];
	const char *bad[] = {NULL, "", "a 1 - -",
		"a;Exec 0000000000000001 - -",
		"a 0000000000000001 - 0",
		"a 0000000000000001 - - extra",
		"a 0000000000000001 short 0"};
	const char *bad_indices[] = {"-", "-1", "+1", "640", "4294967296",
		"999999999999999999999999", "0;Exec"};

	assert(palette_request_parse("a-b 0000000000000001 - -", &request));
	assert(request.release && !request.index);
	snprintf(text, sizeof(text), "a-b 0000000000000001 %s 639", generation);
	assert(palette_request_parse(text, &request));
	assert(!request.release && request.index == 639);
	assert(strcmp(request.cookie, "a-b") == 0);
	for (size_t i = 0; i < sizeof(bad) / sizeof(bad[0]); i++)
	{
		assert(!palette_request_parse(bad[i], &request));
		assert(!request.cookie[0]);
	}
	for (size_t i = 0;
	     i < sizeof(bad_indices) / sizeof(bad_indices[0]); i++)
	{
		snprintf(text, sizeof(text), "a 0000000000000001 %s %s",
			generation, bad_indices[i]);
		assert(!palette_request_parse(text, &request));
		assert(!request.cookie[0]);
	}
	memset(text, 'a', sizeof(text) - 1);
	text[sizeof(text) - 1] = '\0';
	assert(!palette_request_parse(text, &request));
}

static void test_ownership_and_pages(void)
{
	struct palette_slot slots[PALETTE_OWNER_MAX] = {0};

	assert(palette_slot_select(slots, 0) == -1);
	for (uintptr_t i = 1; i <= PALETTE_OWNER_MAX; i++)
	{
		int index = palette_slot_select(slots, i);

		assert(index == (int)i - 1);
		slots[index].owner = i;
		strcpy(slots[index].cookie, "a-b");
		strcpy(slots[index].generation, generation);
	}
	assert(palette_slot_select(slots, 5) == -1);
	assert(palette_slot_select(slots, 1) == 0);
	assert(palette_slot_matches(slots, 1, "a-b", generation));
	assert(palette_slot_matches(slots, 1, "a-b", NULL));
	assert(!palette_slot_matches(slots, 2, "a-b", generation));
	assert(!palette_slot_matches(slots, 1, "new-view", generation));
	assert(!palette_slot_matches(slots, 1, "a-b",
		"0123456789abcdef0123456789abcdef0000000000000002"));
	memset(slots + 1, 0, sizeof(slots[1]));
	assert(palette_slot_select(slots, 5) == 1);
	assert(!palette_slot_matches(slots + 1, 2, "a-b", generation));
	assert(palette_page_valid(0, 16, 0));
	assert(palette_page_valid(640, 1, 640));
	assert(!palette_page_valid(641, 1, 640));
	assert(!palette_page_valid(0, 17, 640));
	assert(!palette_page_valid(0, 0, 640));
	assert(!palette_page_valid(SIZE_MAX, 16, 640));
	assert(!palette_page_valid(0, 16, SIZE_MAX));
	assert(PALETTE_OWNER_MAX * (sizeof(struct palette_slot) +
		PALETTE_ROW_MAX * sizeof(struct palette_row)) <=
		PALETTE_BUDGET_MAX);
}

static void test_allocation_identity(void)
{
	uint64_t next = 1, first, second, exhausted;
	char id[PALETTE_ID_MAX + 1], refreshed[PALETTE_ID_MAX + 1];
	struct palette_row row = {.kind = PALETTE_WINDOW};

	assert(palette_next_identity(&next, &first) && first == 1);
	assert(palette_next_identity(&next, &second) && second == 2);
	row.identity = first;
	palette_row_id(id, generation, &row);
	palette_row_id(refreshed,
		"0123456789abcdef0123456789abcdef00000000000000ff", &row);
	assert(strcmp(id, refreshed) == 0);
	/* Reusing an XID/address cannot reuse the allocation identity. */
	row.identity = second;
	palette_row_id(refreshed, generation, &row);
	assert(strcmp(id, refreshed) != 0);
	palette_row_id(id, "1123456789abcdef0123456789abcdef0000000000000001",
		&row);
	assert(strcmp(id, refreshed) != 0);
	next = UINT64_MAX;
	assert(palette_next_identity(&next, &exhausted));
	assert(exhausted == UINT64_MAX && next == 0);
	assert(!palette_next_identity(&next, &exhausted) && !exhausted);
	row.kind = PALETTE_DESKTOP;
	row.output = 7;
	row.role = 12;
	row.desk = 0;
	palette_row_id(id, generation, &row);
	row.desk = 1;
	palette_row_id(refreshed, generation, &row);
	assert(strcmp(id, refreshed) != 0);
	row.desk = 0;
	row.role++;
	palette_row_id(refreshed, generation, &row);
	assert(strcmp(id, refreshed) != 0);
	assert(strlen(refreshed) <= PALETTE_ID_MAX);
	assert(strspn(refreshed, "0123456789abcdef-dw") == strlen(refreshed));
}

static void test_literal_bounded_utf8(void)
{
	char output[PALETTE_TEXT_MAX + 1], input[256];

	assert(palette_text(output, "literal $[desk.n]; Exec \"bad\"",
		"fallback"));
	assert(strcmp(output, "literal $[desk.n]; Exec \"bad\"") == 0);
	assert(palette_text(output, NULL, "Unnamed window"));
	assert(strcmp(output, "Unnamed window") == 0);
	assert(palette_text(output, "", "Unnamed window"));
	assert(!palette_text(output, "one\ntwo\tthree\177", "fallback"));
	assert(strcmp(output, "one?two?three?") == 0);
	assert(palette_text(output, "caf\xc3\xa9 \xe2\x98\x83 \xf0\x9f\x98\x80",
		"fallback"));
	assert(!palette_text(output, "\xc0\xaf\xed\xa0\x80\xf4\x90\x80\x80",
		"fallback"));
	assert(strspn(output, "?") == strlen(output));
	assert(!palette_text(output, "\xc2\x85", "fallback"));
	assert(strcmp(output, "?") == 0);
	memset(input, 'a', sizeof(input) - 1);
	input[sizeof(input) - 1] = '\0';
	assert(!palette_text(output, input, "fallback"));
	assert(strlen(output) == PALETTE_TEXT_MAX);
	assert(strcmp(output + PALETTE_TEXT_MAX - 3, "...") == 0);
	/* The final scalar cannot be split, even when the input prefix limit
	 * cuts through a four-byte sequence. */
	memset(input, 'a', 158);
	memcpy(input + 158, "\xf0\x9f\x98\x80", 5);
	assert(!palette_text(output, input, "fallback"));
	assert(strlen(output) <= PALETTE_TEXT_MAX);
	assert(palette_text(input, output, "fallback"));
}

int main(void)
{
	test_requests();
	test_ownership_and_pages();
	test_allocation_identity();
	test_literal_bounded_utf8();
	puts("palette policy: passed");
	return 0;
}
