/* This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */
#include "config.h"
#include "libs/selection_source_policy.h"

static enum selection_source_result source_status(
	const struct selection_source_policy *policy)
{
	if (!policy || !policy->initialized)
		return SELECTION_SOURCE_INVALID;
	switch (policy->phase)
	{
	case SELECTION_SOURCE_PHASE_READY:
		return SELECTION_SOURCE_OK;
	case SELECTION_SOURCE_PHASE_UNSYNCHRONIZED:
		return SELECTION_SOURCE_RETRY;
	case SELECTION_SOURCE_PHASE_UNAVAILABLE:
		return SELECTION_SOURCE_UNAVAILABLE;
	case SELECTION_SOURCE_PHASE_CLOSED:
		return SELECTION_SOURCE_CLOSED;
	case SELECTION_SOURCE_PHASE_EXHAUSTED:
		return SELECTION_SOURCE_EXHAUSTED;
	}
	return SELECTION_SOURCE_INVALID;
}

static enum selection_source_result source_advance(
	struct selection_source_policy *policy)
{
	if (policy->generation == UINT64_MAX)
	{
		policy->phase = SELECTION_SOURCE_PHASE_EXHAUSTED;
		policy->owner = 0;
		return SELECTION_SOURCE_EXHAUSTED;
	}
	policy->generation++;
	return SELECTION_SOURCE_OK;
}

enum selection_source_result selection_source_init(
	struct selection_source_policy *policy, uint64_t epoch, bool available)
{
	if (!policy)
		return SELECTION_SOURCE_INVALID;
	*policy = (struct selection_source_policy){0};
	if (!epoch)
		return SELECTION_SOURCE_INVALID;
	*policy = (struct selection_source_policy){
		.epoch = epoch, .initialized = true,
		.phase = available ? SELECTION_SOURCE_PHASE_UNSYNCHRONIZED :
			SELECTION_SOURCE_PHASE_UNAVAILABLE
	};
	return available ? SELECTION_SOURCE_RETRY :
		SELECTION_SOURCE_UNAVAILABLE;
}

enum selection_source_result selection_source_changed(
	struct selection_source_policy *policy)
{
	enum selection_source_result result = source_status(policy);

	if (result != SELECTION_SOURCE_OK && result != SELECTION_SOURCE_RETRY)
		return result;
	result = source_advance(policy);
	if (result != SELECTION_SOURCE_OK)
		return result;
	policy->owner = 0;
	policy->phase = SELECTION_SOURCE_PHASE_UNSYNCHRONIZED;
	return SELECTION_SOURCE_RETRY;
}

enum selection_source_result selection_source_uncertain(
	struct selection_source_policy *policy)
{
	enum selection_source_result result = source_status(policy);

	if (result == SELECTION_SOURCE_OK || result == SELECTION_SOURCE_RETRY)
	{
		policy->owner = 0;
		policy->phase = SELECTION_SOURCE_PHASE_UNSYNCHRONIZED;
		return SELECTION_SOURCE_RETRY;
	}
	return result;
}

enum selection_source_result selection_source_token_get(
	const struct selection_source_policy *policy,
	struct selection_source_token *token)
{
	if (!token)
		return SELECTION_SOURCE_INVALID;
	*token = (struct selection_source_token){0};
	enum selection_source_result result = source_status(policy);

	if (result != SELECTION_SOURCE_OK)
		return result;
	if (!policy->owner)
		return SELECTION_SOURCE_EMPTY;
	*token = (struct selection_source_token){
		.epoch = policy->epoch, .generation = policy->generation,
		.owner = policy->owner
	};
	return SELECTION_SOURCE_OK;
}

enum selection_source_result selection_source_snapshot(
	struct selection_source_policy *policy, uint32_t owner, bool complete,
	struct selection_source_token *token)
{
	if (!token)
		return SELECTION_SOURCE_INVALID;
	*token = (struct selection_source_token){0};
	enum selection_source_result result = source_status(policy);

	if (result != SELECTION_SOURCE_OK && result != SELECTION_SOURCE_RETRY)
		return result;
	if (!complete)
		return selection_source_uncertain(policy);
	result = source_advance(policy);
	if (result != SELECTION_SOURCE_OK)
		return result;
	policy->phase = SELECTION_SOURCE_PHASE_READY;
	policy->owner = owner;
	return selection_source_token_get(policy, token);
}

bool selection_source_current(const struct selection_source_policy *policy,
	struct selection_source_token token)
{
	return source_status(policy) == SELECTION_SOURCE_OK && token.owner &&
		token.epoch && token.generation &&
		token.epoch == policy->epoch &&
		token.generation == policy->generation &&
		token.owner == policy->owner;
}

void selection_source_close(struct selection_source_policy *policy)
{
	if (policy && policy->initialized)
	{
		policy->owner = 0;
		policy->phase = SELECTION_SOURCE_PHASE_CLOSED;
	}
}
