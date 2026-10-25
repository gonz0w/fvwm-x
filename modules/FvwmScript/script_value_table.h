#ifndef FVWMSCRIPT_SCRIPT_VALUE_TABLE_H
#define FVWMSCRIPT_SCRIPT_VALUE_TABLE_H

#include "script_storage.h"
#include <stdbool.h>
#include <stddef.h>

/* Complete parser output. Values can be replaced in their existing slots;
 * retain the arrays and frozen extent instead of snapshots of their entries. */
typedef struct
{
	char **names;
	char **values;
	size_t count;
	script_storage *storage;
	bool published;
} script_value_table;

/* Refuse retirement while an executing command can borrow a table value. */
bool script_value_table_clear(script_value_table *table,
	unsigned int active_depth);

#endif
