#ifndef FVWM_STYLE_ID_FILTER_H
#define FVWM_STYLE_ID_FILTER_H

#include <stdbool.h>
#include <stdint.h>
#include "fvwm.h"

/* Fixed operation-owned storage. Collisions only retain the existing scan.
 * first stores conservative lower bounds on one-based list ordinals. Ranks
 * above UINT16_MAX clamp down, preserving the prefix-scan safety proof. */
typedef struct
{
	uint64_t seen[2][128];
	uint64_t repeated[2][128];
	uint16_t first[2][1024];
} style_id_filter;

uint64_t style_id_hash(const style_id_t *id);
/* Record initial list positions in increasing, nonzero ordinal order. */
void style_id_filter_record(style_id_filter *filter, uint64_t hash,
	size_t ordinal);
bool style_id_filter_hash_may_repeat(const style_id_filter *filter,
	uint64_t hash);
size_t style_id_filter_first(const style_id_filter *filter, uint64_t hash);
void style_id_filter_add(style_id_filter *filter, const style_id_t *id);
bool style_id_filter_may_repeat(const style_id_filter *filter,
	const style_id_t *id);

#endif
