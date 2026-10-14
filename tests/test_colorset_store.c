/* This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include "libs/colorset_store.h"
#include <assert.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>

static void test_sparse_stable_and_refusal(void)
{
	colorset_store store = {.limit = 3, .record_size = sizeof(int)};
	bool created = true;
	int *last = colorset_store_ensure(&store, INT_MAX, &created);
	assert(last != NULL && created && *last == 0);
	*last = 42;
	int *zero = colorset_store_ensure(&store, 0, &created);
	assert(zero != NULL && created);
	int *middle = colorset_store_ensure(&store, 100000, &created);
	assert(middle != NULL && created);
	assert(store.count == 3 && *last == 42);
	assert(colorset_store_id_at(&store, 0) == 0);
	assert(colorset_store_id_at(&store, 1) == 100000);
	assert(colorset_store_id_at(&store, 2) == INT_MAX);
	assert(colorset_store_id_at(&store, 3) == -1);
	colorset_store_entry *entries = store.entries;
	assert(colorset_store_ensure(&store, 99, &created) == NULL);
	assert(!created && store.count == 3 && store.entries == entries);
	assert(colorset_store_ensure(&store, -1, &created) == NULL);
	assert(!created && colorset_store_find(&store, 99) == NULL);
	assert(colorset_store_find(&store, -1) == NULL);
	assert(colorset_store_ensure(&store, INT_MAX, &created) == last);
	assert(!created && *last == 42);
	assert(colorset_store_find(&store, 0) == zero);
	assert(colorset_store_find(&store, 100000) == middle);
	colorset_store_destroy(&store);
	colorset_store_destroy(&store);
	assert(store.count == 0 && store.entries == NULL);
}

static void test_growth_and_count(void)
{
	colorset_store store = {.limit = 4096, .record_size = sizeof(int)};
	bool created;
	int *anchor = colorset_store_ensure(&store, INT_MAX, &created);
	for (int i = 4094; i >= 0; i--)
	{
		int *value = colorset_store_ensure(&store, i * 100, &created);
		assert(value != NULL && created && *value == 0);
		*value = i;
	}
	assert(store.count == 4096 && store.capacity <= store.limit);
	for (int i = 0; i < 4095; i++)
	{
		assert(colorset_store_id_at(&store, i) == i * 100);
		assert(*(int *)colorset_store_find(&store, i * 100) == i);
	}
	assert(colorset_store_find(&store, INT_MAX) == anchor);
	assert(colorset_store_ensure(&store, 1, &created) == NULL);
	colorset_store_destroy(&store);

	store = (colorset_store){.limit = 65536, .record_size = sizeof(int)};
	for (int i = 0; i < 65536; i++)
	{
		assert(colorset_store_ensure(&store, i, &created) != NULL);
	}
	assert(store.count == 65536 && store.capacity == 65536);
	assert(colorset_store_ensure(&store, INT_MAX, &created) == NULL);
	colorset_store_destroy(&store);
}

static void test_invalid_capacity_and_id_parse(void)
{
	colorset_store store = {.limit = SIZE_MAX, .record_size = sizeof(int)};
	bool created;
	assert(colorset_store_ensure(&store, 0, &created) == NULL);
	assert(!created && store.count == 0);
	store.limit = 0;
	assert(colorset_store_ensure(&store, 0, &created) == NULL);
	store.limit = 1;
	store.record_size = 0;
	assert(colorset_store_ensure(&store, 0, &created) == NULL);
	int id;
	char *end;
	assert(colorset_store_parse_id("2147483647 tail", 10, &id, &end));
	assert(id == INT32_MAX && *end == ' ');
	assert(colorset_store_parse_id("-2147483648", 10, &id, NULL));
	assert(id == INT32_MIN);
	assert(colorset_store_parse_id("7fffffff", 16, &id, &end));
	assert(id == INT32_MAX && *end == '\0');
	const char *bad[] = {"2147483648", "-2147483649",
		"9999999999999999999999999999", "", "words", NULL};
	for (size_t i = 0; i < sizeof(bad) / sizeof(*bad); i++)
	{
		assert(!colorset_store_parse_id(bad[i], 10, &id, &end));
		assert(id == 0 && end == bad[i]);
	}
	assert(!colorset_store_parse_id("80000000", 16, &id, NULL));
	assert(!colorset_store_parse_id("0", 8, &id, NULL));
}

int main(void)
{
	test_sparse_stable_and_refusal();
	test_growth_and_count();
	test_invalid_capacity_and_id_parse();
	puts("colorset store: passed");
	return 0;
}
