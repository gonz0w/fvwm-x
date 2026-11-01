#ifndef FVWMSCRIPT_SCRIPT_SELECTION_POLICY_H
#define FVWMSCRIPT_SCRIPT_SELECTION_POLICY_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#define SCRIPT_SELECTION_MAX_BYTES (32U * 1024U)
#define SCRIPT_SELECTION_TARGETS 256U
#define SCRIPT_SELECTION_PAIR_BYTES 1024U
#define SCRIPT_SELECTION_OUTPUT_BYTES (128U * 1024U)

/* STRING keeps legacy bytes; it does not claim they are UTF-8. */
static inline bool script_selection_text_valid(const char *text, size_t length)
{
	return text != NULL && length <= SCRIPT_SELECTION_MAX_BYTES &&
		memchr(text, '\0', length) == NULL;
}

/* X times are serials, not ordinary unsigned dates. After half a wrap of
 * elapsed monotonic time their order is ambiguous; CurrentTime is legal.
 * This retains the core property policy without a module/core dependency. */
static inline bool script_selection_time_valid(uint32_t requested,
	uint32_t acquired, uint64_t acquired_ns, uint64_t now_ns)
{
	return requested == 0 || acquired_ns == 0 ||
		acquired_ns == UINT64_MAX || now_ns == UINT64_MAX ||
		now_ns < acquired_ns ||
		(now_ns - acquired_ns) / UINT64_C(1000000) >=
		UINT32_C(0x80000000) ||
		(uint32_t)(requested - acquired) < UINT32_C(0x80000000);
}

static inline bool script_selection_pairs_valid(size_t bytes)
{
	return bytes <= SCRIPT_SELECTION_PAIR_BYTES && bytes % 8 == 0;
}

static inline bool script_selection_pair_valid(uint32_t target,
	uint32_t property, uint32_t multiple, uint32_t control)
{
	return target != 0 && property != 0 && target != multiple &&
		property != control;
}

static inline bool script_selection_charge(size_t *used, size_t bytes)
{
	if (*used > SCRIPT_SELECTION_OUTPUT_BYTES ||
	    bytes > SCRIPT_SELECTION_OUTPUT_BYTES - *used)
		return false;
	*used += bytes;
	return true;
}

static inline bool script_selection_next_epoch(uint64_t *epoch)
{
	if (*epoch == UINT64_MAX)
		return false;
	(*epoch)++;
	return true;
}

#endif
