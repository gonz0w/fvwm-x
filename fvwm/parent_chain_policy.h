#ifndef FVWM_PARENT_CHAIN_POLICY_H
#define FVWM_PARENT_CHAIN_POLICY_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct
{
	const void *anchor;
	size_t power;
	size_t distance;
} fvwm_parent_chain_state;

static inline void fvwm_parent_chain_init(
	fvwm_parent_chain_state *state, const void *start)
{
	state->anchor = start;
	state->power = 1;
	state->distance = 0;
}

/* Brent's repetition check needs one parent lookup per step and no allocation.
 * Valid state has distance < power, so increment cannot overflow. */
static inline bool fvwm_parent_chain_repeated(
	fvwm_parent_chain_state *state, const void *next)
{
	if (next == NULL)
	{
		return false;
	}
	if (next == state->anchor)
	{
		return true;
	}
	state->distance++;
	if (state->distance == state->power)
	{
		state->anchor = next;
		state->distance = 0;
		state->power = state->power <= SIZE_MAX / 2 ?
			state->power * 2 : SIZE_MAX;
	}
	return false;
}

#endif
