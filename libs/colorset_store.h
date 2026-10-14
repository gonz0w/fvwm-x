#ifndef FVWMLIB_COLORSET_STORE_H
#define FVWMLIB_COLORSET_STORE_H

#include <stdbool.h>
#include <stddef.h>

typedef struct
{
	int id;
	void *record;
} colorset_store_entry;

typedef struct
{
	colorset_store_entry *entries;
	size_t count;
	size_t capacity;
	size_t limit;
	size_t record_size;
} colorset_store;

/* Records keep stable addresses. IDs are nonnegative and need not be dense.
 * Refusal leaves the existing records and count unchanged. */
void *colorset_store_find(const colorset_store *store, int id);
void *colorset_store_ensure(colorset_store *store, int id, bool *created);
int colorset_store_id_at(const colorset_store *store, size_t position);
/* Frees records, not resources referenced from within them. */
void colorset_store_destroy(colorset_store *store);
/* Parse an int32 ID without narrowing; callers decide on negative sentinels
 * and whether trailing text is a delimiter or an error. */
bool colorset_store_parse_id(const char *text, int base, int *id,
	char **end);

#endif
