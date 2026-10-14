#ifndef FVWM_STACK_RANK_POLICY_H
#define FVWM_STACK_RANK_POLICY_H

#include <stdint.h>

/* Preserve rank + parent + 1 whenever representable. These temporary ordering
 * keys can accumulate across marking consumers; endpoint ties retain the
 * existing stable sort rather than overflowing the signed representation. */
static inline int64_t fvwm_stack_rank_add_parent(int64_t rank, int64_t parent)
{
	int64_t increment;

	if (parent == INT64_MAX)
	{
		return rank >= 0 ? INT64_MAX : INT64_MAX + (rank + 1);
	}
	increment = parent + 1;
	if (increment > 0 && rank > INT64_MAX - increment)
	{
		return INT64_MAX;
	}
	if (increment < 0 && rank < INT64_MIN - increment)
	{
		return INT64_MIN;
	}

	return rank + increment;
}

#endif
