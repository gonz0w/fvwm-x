#ifndef FVWM_PROPERTY_POLICY_H
#define FVWM_PROPERTY_POLICY_H

#include <stdbool.h>
#include <stddef.h>

#define FVWM_SELECTION_PAIR_MAX_BYTES 1024U

static inline bool
fvwm_selection_pair_length_is_valid(size_t bytes)
{
	return bytes <= FVWM_SELECTION_PAIR_MAX_BYTES && bytes % 8 == 0;
}

static inline bool
fvwm_openlook_attr_length_is_valid(unsigned long items)
{
	return items == 3 || items == 5;
}

#endif
