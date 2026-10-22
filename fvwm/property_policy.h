#ifndef FVWM_PROPERTY_POLICY_H
#define FVWM_PROPERTY_POLICY_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define FVWM_SELECTION_PAIR_MAX_BYTES 1024U

static inline bool
fvwm_selection_pair_length_is_valid(size_t bytes)
{
	return bytes <= FVWM_SELECTION_PAIR_MAX_BYTES && bytes % 8 == 0;
}

/* X timestamps are 32-bit serial numbers. CurrentTime is the explicit
 * request-time exception; order is defined within half the serial space. */
static inline bool
fvwm_selection_request_time_is_valid(uint32_t requested, uint32_t acquired)
{
	return requested == 0 || (uint32_t)(requested - acquired) <
	    UINT32_C(0x80000000);
}

static inline bool
fvwm_openlook_attr_length_is_valid(unsigned long items)
{
	return items == 3 || items == 5;
}

#endif
