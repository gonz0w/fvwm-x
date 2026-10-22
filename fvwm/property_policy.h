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

/* X timestamps are 32-bit serial numbers. Only reject a pre-acquisition
 * serial while its order is unambiguous. Long uptime or an unavailable clock
 * must not turn valid current requests into refusals after half a wrap. */
static inline bool
fvwm_selection_request_time_is_valid(uint32_t requested, uint32_t acquired,
    uint64_t acquired_ns, uint64_t now_ns)
{
	return requested == 0 || acquired_ns == 0 ||
	    acquired_ns == UINT64_MAX || now_ns == UINT64_MAX ||
	    now_ns < acquired_ns ||
	    (now_ns - acquired_ns) / UINT64_C(1000000) >=
	    UINT32_C(0x80000000) || (uint32_t)(requested - acquired) <
	    UINT32_C(0x80000000);
}

static inline bool
fvwm_openlook_attr_length_is_valid(unsigned long items)
{
	return items == 3 || items == 5;
}

#endif
