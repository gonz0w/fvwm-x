/* This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */
#include "config.h"
#include "libs/selection_source_policy.h"
#include <assert.h>
#include <stddef.h>

static void assert_empty(struct selection_source_token token)
{
	assert(!token.epoch && !token.generation && !token.owner);
}

static struct selection_source_token snapshot(
	struct selection_source_policy *policy, uint32_t owner)
{
	struct selection_source_token token;

	assert(selection_source_snapshot(policy, owner, true, &token) ==
		SELECTION_SOURCE_OK);
	assert(selection_source_current(policy, token));
	return token;
}

static void initialization_and_refusal(void)
{
	struct selection_source_policy policy;
	struct selection_source_token token = {1, 1, 1};

	assert(selection_source_init(NULL, 1, true) ==
		SELECTION_SOURCE_INVALID);
	assert(selection_source_init(&policy, 0, true) ==
		SELECTION_SOURCE_INVALID);
	assert(selection_source_token_get(&policy, &token) ==
		SELECTION_SOURCE_INVALID);
	assert_empty(token);
	assert(!selection_source_current(NULL, token));
	assert(selection_source_init(&policy, 1, false) ==
		SELECTION_SOURCE_UNAVAILABLE);
	assert(selection_source_snapshot(&policy, 1, true, &token) ==
		SELECTION_SOURCE_UNAVAILABLE);
	assert_empty(token);
	assert(selection_source_changed(&policy) ==
		SELECTION_SOURCE_UNAVAILABLE);
	selection_source_close(&policy);
	assert(selection_source_token_get(&policy, &token) ==
		SELECTION_SOURCE_CLOSED);
	assert_empty(token);
}

static void incomplete_and_recovery(void)
{
	struct selection_source_policy policy;
	struct selection_source_token token;

	assert(selection_source_init(&policy, 2, true) ==
		SELECTION_SOURCE_RETRY);
	assert(selection_source_token_get(&policy, &token) ==
		SELECTION_SOURCE_RETRY);
	assert_empty(token);
	struct selection_source_token first = snapshot(&policy, 11);

	assert(first.generation == 1 && first.epoch == 2);
	assert(selection_source_snapshot(&policy, 11, false, &token) ==
		SELECTION_SOURCE_RETRY);
	assert_empty(token);
	assert(!selection_source_current(&policy, first));
	uint64_t generation = policy.generation;

	for (unsigned i = 0; i < 8; i++)
		assert(selection_source_uncertain(&policy) ==
			SELECTION_SOURCE_RETRY);
	assert(policy.generation == generation);
	struct selection_source_token next = snapshot(&policy, 11);

	assert(next.generation > first.generation &&
		!selection_source_current(&policy, first));
	assert(selection_source_snapshot(&policy, 0, true, &token) ==
		SELECTION_SOURCE_EMPTY);
	assert_empty(token);
	assert(!selection_source_current(&policy, next));
}

static void same_owner_aba_and_restart(void)
{
	struct selection_source_policy policy, replacement;
	struct selection_source_token ignored;

	assert(selection_source_init(&policy, 3, true) ==
		SELECTION_SOURCE_RETRY);
	struct selection_source_token first = snapshot(&policy, 22);

	/* Event timestamps/owners are deliberately not policy parameters:
	 * same-owner and equal/wrapped timestamp notifications still revoke. */
	assert(selection_source_changed(&policy) == SELECTION_SOURCE_RETRY);
	assert(!selection_source_current(&policy, first));
	struct selection_source_token same = snapshot(&policy, 22);

	assert(same.generation > first.generation);
	assert(selection_source_changed(&policy) == SELECTION_SOURCE_RETRY);
	assert(selection_source_changed(&policy) == SELECTION_SOURCE_RETRY);
	assert(selection_source_snapshot(&policy, 33, false, &ignored) ==
		SELECTION_SOURCE_RETRY);
	assert_empty(ignored);
	struct selection_source_token middle = snapshot(&policy, 33);

	assert(selection_source_changed(&policy) == SELECTION_SOURCE_RETRY);
	struct selection_source_token back = snapshot(&policy, 22);

	assert(!selection_source_current(&policy, first) &&
		!selection_source_current(&policy, same) &&
		!selection_source_current(&policy, middle));
	uint64_t generation = policy.generation;

	selection_source_close(&policy);
	selection_source_close(&policy);
	assert(policy.epoch == 3 && policy.generation == generation);
	assert(!selection_source_current(&policy, back));
	assert(selection_source_snapshot(&policy, 22, true, &ignored) ==
		SELECTION_SOURCE_CLOSED);
	assert_empty(ignored);
	assert(selection_source_init(&replacement, 4, true) ==
		SELECTION_SOURCE_RETRY);
	struct selection_source_token fresh = snapshot(&replacement, 22);

	assert(fresh.generation == 1 && !selection_source_current(&replacement,
		first) && !selection_source_current(&replacement, back));
}

static void exhaustion_and_invalid_tokens(void)
{
	struct selection_source_policy policy;
	struct selection_source_token token;

	assert(selection_source_init(&policy, 5, true) ==
		SELECTION_SOURCE_RETRY);
	struct selection_source_token first = snapshot(&policy, UINT32_MAX);
	struct selection_source_token altered = first;

	altered.owner--;
	assert(!selection_source_current(&policy, altered));
	altered = first;
	altered.epoch++;
	assert(!selection_source_current(&policy, altered));
	altered = first;
	altered.generation++;
	assert(!selection_source_current(&policy, altered));
	assert(selection_source_token_get(&policy, NULL) ==
		SELECTION_SOURCE_INVALID);
	assert(selection_source_snapshot(&policy, 22, true, NULL) ==
		SELECTION_SOURCE_INVALID);
	assert(selection_source_current(&policy, first));
	policy.generation = UINT64_MAX - 1; /* Test-only exhaustion setup. */
	struct selection_source_token last = snapshot(&policy, 22);

	assert(last.generation == UINT64_MAX);
	assert(selection_source_changed(&policy) == SELECTION_SOURCE_EXHAUSTED);
	assert(policy.generation == UINT64_MAX &&
		!selection_source_current(&policy, last));
	assert(selection_source_snapshot(&policy, 22, true, &token) ==
		SELECTION_SOURCE_EXHAUSTED);
	assert_empty(token);
	assert(selection_source_uncertain(&policy) ==
		SELECTION_SOURCE_EXHAUSTED);
	selection_source_close(&policy);
	assert(policy.generation == UINT64_MAX);
}

int main(void)
{
	initialization_and_refusal();
	incomplete_and_recovery();
	same_owner_aba_and_restart();
	exhaustion_and_invalid_tokens();
	return 0;
}
