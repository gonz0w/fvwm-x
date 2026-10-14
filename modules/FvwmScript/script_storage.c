/* This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include "script_storage.h"
#include <stdint.h>

script_storage *script_storage_runtime(void)
{
	static script_storage storage = {
		.value_limit = SCRIPT_STORAGE_DEFAULT_VALUE,
		.total_limit = SCRIPT_STORAGE_DEFAULT_TOTAL
	};
	return &storage;
}

script_storage_result script_storage_check_value(const script_storage *storage,
	size_t length)
{
	return length > storage->value_limit || length == SIZE_MAX ?
		SCRIPT_STORAGE_VALUE_LIMIT : SCRIPT_STORAGE_OK;
}

script_storage_result script_storage_check_replace(const script_storage *storage,
	size_t old_bytes, size_t length)
{
	script_storage_result result = script_storage_check_value(storage, length);
	size_t retained;

	if (result != SCRIPT_STORAGE_OK)
	{
		return result;
	}
	if (old_bytes > storage->used)
	{
		return SCRIPT_STORAGE_TOTAL_LIMIT;
	}
	retained = storage->used - old_bytes;
	if (retained > storage->total_limit ||
	    length + 1 > storage->total_limit - retained)
	{
		return SCRIPT_STORAGE_TOTAL_LIMIT;
	}
	return SCRIPT_STORAGE_OK;
}

script_storage_result script_storage_replace(script_storage *storage,
	size_t old_bytes, size_t length)
{
	script_storage_result result =
		script_storage_check_replace(storage, old_bytes, length);
	if (result == SCRIPT_STORAGE_OK)
	{
		storage->used = storage->used - old_bytes + length + 1;
	}
	return result;
}
