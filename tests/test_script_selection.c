/* This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include "config.h"
#include "modules/FvwmScript/script_selection_policy.h"
#include <assert.h>

static void test_primary_wire_admission(void)
{
	char text[SCRIPT_SELECTION_MAX_BYTES + 1];

	memset(text, 0xff, sizeof(text));
	assert(script_selection_text_valid(text, 0));
	assert(script_selection_text_valid(text, SCRIPT_SELECTION_MAX_BYTES));
	assert(!script_selection_text_valid(text, sizeof(text)));
	assert(!script_selection_text_valid(NULL, 0));
	text[3] = 0;
	assert(!script_selection_text_valid(text, 4));
	assert(script_selection_text_valid(text, 3));
}

static void test_timestamp_serials(void)
{
	uint64_t acquired = UINT64_C(1000000000);
	uint64_t now = acquired + 1;

	assert(script_selection_time_valid(0, 100, acquired, now));
	assert(script_selection_time_valid(100, 100, acquired, now));
	assert(script_selection_time_valid(101, 100, acquired, now));
	assert(!script_selection_time_valid(99, 100, acquired, now));
	assert(script_selection_time_valid(2, UINT32_MAX - 2, acquired, now));
	assert(!script_selection_time_valid(UINT32_MAX - 2, 2, acquired, now));
	assert(!script_selection_time_valid(UINT32_C(0x80000064), 100,
		acquired, now));
	assert(script_selection_time_valid(99, 100, 0, now));
	assert(script_selection_time_valid(99, 100, UINT64_MAX, now));
	assert(script_selection_time_valid(99, 100, acquired, UINT64_MAX));
	assert(script_selection_time_valid(99, 100, acquired, acquired - 1));
	uint64_t ambiguous = acquired +
		UINT64_C(0x80000000) * UINT64_C(1000000);

	assert(!script_selection_time_valid(99, 100, acquired, ambiguous - 1));
	assert(script_selection_time_valid(99, 100, acquired, ambiguous));
}

static void test_multiple_admission(void)
{
	assert(script_selection_pairs_valid(0));
	assert(script_selection_pairs_valid(8));
	assert(script_selection_pairs_valid(128 * 8));
	assert(!script_selection_pairs_valid(4));
	assert(!script_selection_pairs_valid(9));
	assert(!script_selection_pairs_valid(129 * 8));
	assert(!script_selection_pairs_valid(SIZE_MAX));
	assert(script_selection_pair_valid(10, 20, 30, 40));
	assert(!script_selection_pair_valid(0, 20, 30, 40));
	assert(!script_selection_pair_valid(10, 0, 30, 40));
	assert(!script_selection_pair_valid(30, 20, 30, 40));
	assert(!script_selection_pair_valid(10, 40, 30, 40));
	/* Repeated destination properties are legal, ordered replacements. */
	assert(script_selection_pair_valid(11, 20, 30, 40));
}

static void test_aggregate_wire_budget(void)
{
	size_t used = 0;

	/* Control output is charged first. Three maximum messages fit; the
	 * fourth fails without spending budget needed by later small pairs. */
	assert(script_selection_charge(&used, SCRIPT_SELECTION_PAIR_BYTES));
	for (unsigned int i = 0; i < 3; i++)
		assert(script_selection_charge(&used,
			SCRIPT_SELECTION_MAX_BYTES));
	size_t before = used;

	assert(!script_selection_charge(&used, SCRIPT_SELECTION_MAX_BYTES));
	assert(used == before);
	assert(script_selection_charge(&used, 4));
	assert(script_selection_charge(&used,
		SCRIPT_SELECTION_OUTPUT_BYTES - used));
	assert(used == SCRIPT_SELECTION_OUTPUT_BYTES);
	assert(!script_selection_charge(&used, 1));
	assert(script_selection_charge(&used, 0));
	assert(!script_selection_charge(&used, SIZE_MAX));
	assert(used == SCRIPT_SELECTION_OUTPUT_BYTES);
	used++;
	assert(!script_selection_charge(&used, 0));
}

static void test_epoch_exhaustion(void)
{
	uint64_t epoch = 0;

	assert(script_selection_next_epoch(&epoch) && epoch == 1);
	assert(script_selection_next_epoch(&epoch) && epoch == 2);
	epoch = UINT64_MAX - 1;
	assert(script_selection_next_epoch(&epoch) && epoch == UINT64_MAX);
	assert(!script_selection_next_epoch(&epoch) && epoch == UINT64_MAX);
}

int main(void)
{
	test_primary_wire_admission();
	test_timestamp_serials();
	test_multiple_admission();
	test_aggregate_wire_budget();
	test_epoch_exhaustion();
	return 0;
}
