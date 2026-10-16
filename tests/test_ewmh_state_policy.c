/* This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */
#include "config.h"

#include <assert.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include "fvwm/ewmh_client_list.h"
#include "fvwm/ewmh_atom_index.h"

static bool refuse;
static unsigned allocations, releases;
void *__real_malloc(size_t);
void *__real_realloc(void *, size_t);
void __real_free(void *);
void *__wrap_malloc(size_t bytes)
{
	allocations++;
	return refuse ? NULL : __real_malloc(bytes);
}
void *__wrap_realloc(void *old, size_t bytes)
{
	allocations++;
	return refuse ? NULL : __real_realloc(old, bytes);
}
void __wrap_free(void *value)
{
	if (value != NULL)
		releases++;
	__real_free(value);
}

static Window identity(size_t i)
{
	return ULONG_MAX - i;
}

static void test_atom_index(void)
{
	ewmh_atom_index_entry entries[128], original[128];
	unsigned state = 0x7a5f1921U;

	assert(ewmh_atom_index_find(NULL, 0, None) == NULL);
	for (unsigned round = 0; round < 1000; round++)
	{
		for (size_t i = 0; i < 128; i++)
		{
			state = state * 1664525U + 1013904223U;
			entries[i].atom = round == 0 ? None :
				(state % 32 == 0 ? ULONG_MAX : state % 64);
			entries[i].order = i;
			entries[i].value = &original[i];
			original[i] = entries[i];
		}
		ewmh_atom_index_sort(entries, 128);
		for (size_t key = 0; key < 66; key++)
		{
			Atom atom = key == 65 ? ULONG_MAX : key;
			void *expected = NULL;

			for (size_t i = 0; i < 128; i++)
				if (original[i].atom == atom)
				{
					expected = original[i].value;
					break;
				}
			assert(ewmh_atom_index_find(entries, 128, atom) ==
				expected);
		}
	}
	puts("atom index differential priority/None/miss/rebuild cases: "
		"passed");
}

int main(void)
{
	ewmh_client_list list;
	size_t capacity = 99;

	assert(ewmh_client_list_grow_capacity(0, &capacity) && capacity == 64);
	assert(ewmh_client_list_grow_capacity(64, &capacity));
	assert(capacity == 128);
	assert(ewmh_client_list_grow_capacity(INT_MAX - 1, &capacity));
	assert(capacity == INT_MAX);
	assert(!ewmh_client_list_grow_capacity(INT_MAX, &capacity));
	assert(capacity == 0);
	assert(!ewmh_client_list_grow_capacity(SIZE_MAX, &capacity));
	assert(capacity == 0);
	puts("Xlib count and allocation arithmetic boundaries: passed");

	ewmh_client_list_init(&list);
	assert(list.count == 0 && list.windows == list.local);
	for (size_t i = 0; i < 64; i++)
		assert(ewmh_client_list_append(&list, identity(i)));
	assert(allocations == 0);
	refuse = true;
	assert(!ewmh_client_list_append(&list, identity(64)));
	assert(list.count == 64 && list.windows == list.local);
	for (size_t i = 0; i < list.count; i++)
		assert(list.windows[i] == identity(i));
	refuse = false;
	for (size_t i = 64; i < 128; i++)
		assert(ewmh_client_list_append(&list, identity(i)));
	Window *saved = list.windows;
	refuse = true;
	assert(!ewmh_client_list_append(&list, identity(128)));
	assert(list.count == 128 && list.windows == saved);
	refuse = false;
	for (size_t i = 128; i < 1024; i++)
		assert(ewmh_client_list_append(&list, identity(i)));
	for (size_t i = 0; i < list.count; i++)
		assert(list.windows[i] == identity(i));
	assert(allocations == 6 && releases == 0);
	puts("inline/spilled order and refused growth preserve ownership: "
		"passed");
	ewmh_client_list_finish(&list);
	ewmh_client_list_finish(&list);
	assert(releases == 1 && list.count == 0 && list.windows == list.local);
	assert(ewmh_client_list_append(&list, None));
	assert(list.count == 1 && list.windows[0] == None);
	ewmh_client_list_finish(&list);
	assert(releases == 1);
	puts("finish is idempotent and fresh empty owner is reusable: passed");
	test_atom_index();
	return 0;
}
