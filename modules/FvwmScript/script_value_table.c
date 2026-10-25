/* This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#include "config.h"
#include "script_value_table.h"
#include <stdlib.h>

bool script_value_table_clear(script_value_table *table,
	unsigned int active_depth)
{
	if (active_depth != 0)
		return false;
	if (!table->published)
		return true;

	for (size_t i = 0; i < table->count; i++)
	{
		free(table->names[i]);
		free(table->values[i]);
	}
	free(table->names);
	free(table->values);
	/* Fixed names and current values together own the whole accounting. */
	table->storage->used = 0;
	*table = (script_value_table){0};
	return true;
}
