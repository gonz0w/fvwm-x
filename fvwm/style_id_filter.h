#ifndef FVWM_STYLE_ID_FILTER_H
#define FVWM_STYLE_ID_FILTER_H

#include <stdbool.h>
#include <stdint.h>
#include "fvwm.h"

/* Fixed operation-owned storage. Collisions only retain the existing scan. */
typedef struct
{
	uint64_t seen[2][128];
	uint64_t repeated[2][128];
} style_id_filter;

void style_id_filter_add(style_id_filter *filter, const style_id_t *id);
bool style_id_filter_may_repeat(const style_id_filter *filter,
	const style_id_t *id);

#endif
