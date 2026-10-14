/* This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include "modules/FvwmScript/script_storage.h"
#include <assert.h>
#include <stdint.h>

static void test_value_boundaries(void)
{
	script_storage storage = {.value_limit = 16, .total_limit = 64};
	assert(script_storage_replace(&storage, 0, 16) == SCRIPT_STORAGE_OK);
	assert(storage.used == 17);
	assert(script_storage_replace(&storage, 17, 17) ==
		SCRIPT_STORAGE_VALUE_LIMIT);
	assert(storage.used == 17);
	assert(script_storage_replace(&storage, 17, 0) == SCRIPT_STORAGE_OK);
	assert(storage.used == 1);
	assert(script_storage_check_value(&storage, SIZE_MAX) ==
		SCRIPT_STORAGE_VALUE_LIMIT);
}

static void test_shared_total_and_replacement(void)
{
	script_storage storage = {.value_limit = 64, .total_limit = 32};
	assert(script_storage_replace(&storage, 0, 15) == SCRIPT_STORAGE_OK);
	assert(script_storage_replace(&storage, 0, 15) == SCRIPT_STORAGE_OK);
	assert(storage.used == 32);
	for (int i = 0; i < 20; i++)
	{
		assert(script_storage_replace(&storage, 16, 16) ==
			SCRIPT_STORAGE_TOTAL_LIMIT);
		assert(storage.used == 32);
	}
	assert(script_storage_replace(&storage, 16, 7) == SCRIPT_STORAGE_OK);
	assert(storage.used == 24);
	assert(script_storage_replace(&storage, 16, 23) == SCRIPT_STORAGE_OK);
	assert(storage.used == 32);
	assert(script_storage_replace(&storage, 0, 0) ==
		SCRIPT_STORAGE_TOTAL_LIMIT);
	assert(script_storage_check_replace(&storage, 33, 0) ==
		SCRIPT_STORAGE_TOTAL_LIMIT);
	assert(storage.used == 32);
}

static void test_overflow_and_unusual_limits(void)
{
	script_storage storage = {.value_limit = SIZE_MAX, .total_limit = SIZE_MAX};
	assert(script_storage_replace(&storage, 0, SIZE_MAX) ==
		SCRIPT_STORAGE_VALUE_LIMIT);
	assert(storage.used == 0);
	assert(script_storage_replace(&storage, 0, SIZE_MAX - 1) ==
		SCRIPT_STORAGE_OK);
	assert(storage.used == SIZE_MAX);
	assert(script_storage_replace(&storage, 0, 0) ==
		SCRIPT_STORAGE_TOTAL_LIMIT);
	assert(script_storage_replace(&storage, SIZE_MAX, 0) == SCRIPT_STORAGE_OK);
	assert(storage.used == 1);
	storage.total_limit = 0;
	assert(script_storage_replace(&storage, 1, 0) ==
		SCRIPT_STORAGE_TOTAL_LIMIT);
	assert(storage.used == 1);
}

static void test_runtime_defaults(void)
{
	script_storage *storage = script_storage_runtime();
	assert(storage->value_limit == 1048576);
	assert(storage->total_limit == 33554432);
	assert(storage->used == 0);
}

int main(void)
{
	test_value_boundaries();
	test_shared_total_and_replacement();
	test_overflow_and_unusual_limits();
	test_runtime_defaults();
	return 0;
}
