#ifndef FVWMSCRIPT_SCRIPT_INDEX_H
#define FVWMSCRIPT_SCRIPT_INDEX_H

#include <stdbool.h>
#include <stddef.h>

/* Invalid text/range leaves value at zero. */
bool script_index_parse(const char *text, int minimum, int maximum, int *value);

/* Resolve a declared widget ID; reject missing or inconsistent mappings. */
int script_widget_index(const char *text, const int *ids, size_t id_count,
	size_t object_count);

#endif
