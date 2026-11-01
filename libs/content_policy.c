/* This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */
#include "config.h"
#include "libs/content_policy.h"
#include <string.h>

static bool same_ref(struct content_ref first, struct content_ref second)
{
	return first.epoch == second.epoch && first.item == second.item &&
		first.generation == second.generation;
}

static struct content_item *find_item(struct content_policy *policy,
	struct content_ref ref)
{
	if (ref.epoch != policy->epoch || !ref.item || !ref.generation)
		return NULL;
	for (size_t i = 0; i < policy->limits.items; i++)
	{
		struct content_item *item = &policy->items[i];

		if (item->phase != CONTENT_EMPTY && same_ref(item->ref, ref))
			return item;
	}
	return NULL;
}

static bool room(size_t used, size_t added, size_t limit)
{
	return used <= limit && added <= limit - used;
}

static bool bytes_room(struct content_bytes used, struct content_bytes added,
	struct content_bytes limit)
{
	return room(used.encoded, added.encoded, limit.encoded) &&
		room(used.decoded, added.decoded, limit.decoded) &&
		room(used.derived, added.derived, limit.derived);
}

static void bytes_add(struct content_bytes *used, struct content_bytes added)
{
	used->encoded += added.encoded;
	used->decoded += added.decoded;
	used->derived += added.derived;
}

static bool storage_room(struct content_storage used,
	struct content_storage added, struct content_storage limit)
{
	return room(used.mapped, added.mapped, limit.mapped) &&
		room(used.locked, added.locked, limit.locked);
}

bool content_storage_charge(size_t bytes, size_t page,
	size_t mapped_extra_pages, size_t locked_extra_pages,
	struct content_storage *output)
{
	size_t pages, maximum;

	if (!output)
		return false;
	*output = (struct content_storage){0};
	if (!page || mapped_extra_pages > 8 ||
		locked_extra_pages > mapped_extra_pages)
		return false;
	if (!bytes)
		bytes = 1;
	pages = bytes / page + (bytes % page != 0);
	maximum = SIZE_MAX / page;
	if (!room(pages, mapped_extra_pages, maximum))
		return false;
	output->mapped = (pages + mapped_extra_pages) * page;
	output->locked = (pages + locked_extra_pages) * page;
	return true;
}

static enum content_result hint_check(enum content_hint hint)
{
	switch (hint)
	{
	case CONTENT_HINT_ABSENT:
	case CONTENT_HINT_ORDINARY:
		return CONTENT_OK;
	case CONTENT_HINT_SENSITIVE:
	case CONTENT_HINT_MALFORMED:
	case CONTENT_HINT_UNAVAILABLE:
		return CONTENT_EXCLUDED;
	default:
		return CONTENT_INVALID;
	}
}

static enum content_result clock_check(struct content_policy *policy,
	uint64_t now_ms)
{
	if (!policy || !policy->configured)
		return CONTENT_UNCONFIGURED;
	if (now_ms < policy->last_ms)
		return CONTENT_INVALID;
	/* Even a refused operation observed this time. A later stale clock
	 * sample must not make previously expired content accessible again. */
	policy->last_ms = now_ms;
	return CONTENT_OK;
}

static void retire_item(struct content_item *item)
{
	item->phase = CONTENT_RETIRED;
	item->pinned = false;
}

static enum content_result live_item(struct content_policy *policy,
	struct content_ref ref, uint64_t now_ms, struct content_item **output)
{
	enum content_result result = clock_check(policy, now_ms);
	struct content_item *item;

	*output = NULL;
	if (result != CONTENT_OK)
		return result;
	item = find_item(policy, ref);
	if (!item || item->phase != CONTENT_LIVE)
		return CONTENT_STALE;
	if (now_ms >= item->descriptor.deadline_ms)
		return CONTENT_EXPIRED;
	*output = item;
	return CONTENT_OK;
}

enum content_result content_policy_init(struct content_policy *policy,
	uint64_t epoch, const struct content_limits *limits,
	struct content_item *items, struct content_lease *leases,
	uint64_t now_ms)
{
	if (!policy || !limits || !items || !leases || !epoch ||
		!limits->items || limits->items > CONTENT_ITEMS_MAX ||
		!limits->leases || limits->leases > CONTENT_LEASES_MAX ||
		!limits->per_item ||
		limits->per_item > CONTENT_REPRESENTATIONS_MAX ||
		!limits->representations ||
		limits->representations > limits->items * limits->per_item ||
		!limits->work || limits->work > limits->items ||
		!limits->lifetime_ms)
		return CONTENT_INVALID;
	*policy = (struct content_policy){
		.limits = *limits, .items = items, .leases = leases,
		.epoch = epoch, .last_ms = now_ms, .configured = true
	};
	memset(items, 0, limits->items * sizeof(*items));
	memset(leases, 0, limits->leases * sizeof(*leases));
	return CONTENT_OK;
}

static enum content_result candidate_check(struct content_policy *policy,
	const struct content_candidate *candidate, uint64_t now_ms,
	struct content_bytes *bytes)
{
	enum content_result result;
	const struct content_limits *limits = &policy->limits;

	*bytes = (struct content_bytes){0};
	if (!candidate || !candidate->count ||
		candidate->count > CONTENT_REPRESENTATIONS_MAX)
		return CONTENT_INVALID;
	result = hint_check(candidate->hint);
	if (result != CONTENT_OK)
		return result;
	if (candidate->deadline_ms <= now_ms)
		return CONTENT_EXPIRED;
	if (candidate->deadline_ms - now_ms > limits->lifetime_ms)
		return CONTENT_INVALID;
	if (candidate->count > limits->per_item)
		return CONTENT_FULL;
	if (candidate->storage.locked > candidate->storage.mapped)
		return CONTENT_INVALID;
	if (!storage_room((struct content_storage){0}, candidate->storage,
		limits->per_item_storage))
		return CONTENT_FULL;
	for (size_t i = 0; i < candidate->count; i++)
	{
		const struct content_representation *rep = &candidate->reps[i];

		if (!rep->id || rep->kind < CONTENT_TEXT ||
			rep->kind > CONTENT_URI)
			return CONTENT_INVALID;
		for (size_t j = 0; j < i; j++)
			if (candidate->reps[j].id == rep->id)
				return CONTENT_INVALID;
		if (!bytes_room((struct content_bytes){0}, rep->bytes,
			limits->per_representation) ||
			!bytes_room(*bytes, rep->bytes, limits->total))
			return CONTENT_FULL;
		bytes_add(bytes, rep->bytes);
	}
	return CONTENT_OK;
}

enum content_result content_reserve(struct content_policy *policy,
	const struct content_candidate *candidate, bool source_allowed,
	const struct content_ref *replace, uint64_t now_ms,
	struct content_ref *output)
{
	struct content_ref prior = replace ? *replace : (struct content_ref){0};
	struct content_item *old = NULL, *slot = NULL;
	struct content_bytes bytes;
	struct content_ref ref;
	enum content_result result = clock_check(policy, now_ms);

	if (!output)
		return CONTENT_INVALID;
	*output = (struct content_ref){0};
	if (result != CONTENT_OK)
		return result;
	if (!policy->enabled || policy->paused || !source_allowed)
		return CONTENT_EXCLUDED;
	result = candidate_check(policy, candidate, now_ms, &bytes);
	if (result != CONTENT_OK)
		return result;
	if (replace)
	{
		result = live_item(policy, prior, now_ms, &old);
		if (result != CONTENT_OK)
			return result;
		/* Replacement must not extend the original lifetime or weaken
		 * its classification. No speculative parallel replacement. */
		if (candidate->deadline_ms > old->descriptor.deadline_ms ||
			candidate->hint != old->descriptor.hint)
			return CONTENT_INVALID;
		if (prior.generation == UINT64_MAX)
			return CONTENT_EXHAUSTED;
		ref = prior;
		ref.generation++;
	}
	else
	{
		if (policy->next_item == UINT64_MAX)
			return CONTENT_EXHAUSTED;
		ref = (struct content_ref){policy->epoch,
			policy->next_item + 1, 1};
	}
	if (!room(policy->usage.items, 1, policy->limits.items) ||
		!room(policy->usage.representations, candidate->count,
			policy->limits.representations) ||
		!room(policy->usage.work, 1, policy->limits.work) ||
		!storage_room(policy->usage.storage, candidate->storage,
			policy->limits.storage) ||
		!bytes_room(policy->usage.bytes, bytes, policy->limits.total))
		return CONTENT_FULL;
	for (size_t i = 0; i < policy->limits.items; i++)
		if (policy->items[i].phase == CONTENT_EMPTY)
		{
			slot = &policy->items[i];
			break;
		}
	if (!slot)
		return CONTENT_FULL;
	/* Every refusal above leaves the old generation and charges intact. */
	*slot = (struct content_item){
		.phase = CONTENT_RESERVED, .ref = ref,
		.descriptor = {
			.hint = candidate->hint,
			.deadline_ms = candidate->deadline_ms,
			.count = candidate->count,
			.storage = candidate->storage
		},
		.charged = bytes, .work = true
	};
	for (size_t i = 0; i < candidate->count; i++)
		slot->descriptor.reps[i] = candidate->reps[i];
	if (old)
		retire_item(old);
	else
		policy->next_item = ref.item;
	policy->usage.items++;
	policy->usage.representations += candidate->count;
	policy->usage.work++;
	bytes_add(&policy->usage.bytes, bytes);
	policy->usage.storage.mapped += candidate->storage.mapped;
	policy->usage.storage.locked += candidate->storage.locked;
	*output = ref;
	return CONTENT_OK;
}

enum content_result content_finish(struct content_policy *policy,
	struct content_ref ref, enum content_hint hint, bool success,
	uint64_t now_ms)
{
	struct content_item *item;
	enum content_result result = clock_check(policy, now_ms);

	if (result != CONTENT_OK)
		return result;
	item = find_item(policy, ref);
	if (!item || !item->work)
		return CONTENT_STALE;
	item->work = false;
	policy->usage.work--;
	result = hint_check(hint);
	if (item->phase != CONTENT_RESERVED)
		return CONTENT_STALE;
	if (!success)
	{
		retire_item(item);
		return CONTENT_OK;
	}
	if (now_ms >= item->descriptor.deadline_ms)
		result = CONTENT_EXPIRED;
	else if (!policy->enabled || policy->paused ||
		hint != item->descriptor.hint)
		result = CONTENT_EXCLUDED;
	if (result != CONTENT_OK)
		retire_item(item);
	else
		item->phase = CONTENT_LIVE;
	return result;
}

enum content_result content_retire(struct content_policy *policy,
	struct content_ref ref)
{
	struct content_item *item;

	if (!policy || !policy->configured)
		return CONTENT_UNCONFIGURED;
	item = find_item(policy, ref);
	if (!item)
		return CONTENT_STALE;
	retire_item(item);
	return CONTENT_OK;
}

enum content_result content_expire(struct content_policy *policy,
	uint64_t now_ms)
{
	enum content_result result = clock_check(policy, now_ms);

	if (result != CONTENT_OK)
		return result;
	for (size_t i = 0; i < policy->limits.items; i++)
	{
		struct content_item *item = &policy->items[i];

		if (item->phase != CONTENT_EMPTY &&
			now_ms >= item->descriptor.deadline_ms)
			retire_item(item);
	}
	return CONTENT_OK;
}

enum content_result content_clear(struct content_policy *policy)
{
	if (!policy || !policy->configured)
		return CONTENT_UNCONFIGURED;
	for (size_t i = 0; i < policy->limits.items; i++)
		if (policy->items[i].phase != CONTENT_EMPTY)
			retire_item(&policy->items[i]);
	return CONTENT_OK;
}

enum content_result content_pin(struct content_policy *policy,
	struct content_ref ref, bool pinned, uint64_t now_ms)
{
	struct content_item *item;
	enum content_result result = live_item(policy, ref, now_ms, &item);

	if (result == CONTENT_OK)
		item->pinned = pinned;
	return result;
}

enum content_result content_acquire(struct content_policy *policy,
	struct content_ref ref, uint32_t representation, uint64_t now_ms,
	struct content_lease *output)
{
	struct content_item *item;
	struct content_lease *slot = NULL;
	bool found = false;
	enum content_result result = live_item(policy, ref, now_ms, &item);

	if (!output)
		return CONTENT_INVALID;
	*output = (struct content_lease){0};
	if (result != CONTENT_OK)
		return result;
	for (size_t i = 0; i < item->descriptor.count; i++)
		if (item->descriptor.reps[i].id == representation)
			found = true;
	if (!found)
		return CONTENT_INVALID;
	if (policy->next_lease == UINT64_MAX)
		return CONTENT_EXHAUSTED;
	if (!room(policy->usage.leases, 1, policy->limits.leases))
		return CONTENT_FULL;
	for (size_t i = 0; i < policy->limits.leases; i++)
		if (!policy->leases[i].id)
		{
			slot = &policy->leases[i];
			break;
		}
	if (!slot)
		return CONTENT_FULL;
	*slot = (struct content_lease){ref, ++policy->next_lease,
		representation};
	item->leases++;
	policy->usage.leases++;
	*output = *slot;
	return CONTENT_OK;
}

static struct content_lease *find_lease(struct content_policy *policy,
	struct content_lease lease)
{
	if (!lease.id)
		return NULL;
	for (size_t i = 0; i < policy->limits.leases; i++)
	{
		struct content_lease *slot = &policy->leases[i];

		if (slot->id != lease.id || !same_ref(slot->ref, lease.ref) ||
			slot->representation != lease.representation)
			continue;
		return slot;
	}
	return NULL;
}

enum content_result content_release(struct content_policy *policy,
	struct content_lease lease)
{
	struct content_item *item;
	struct content_lease *slot;

	if (!policy || !policy->configured)
		return CONTENT_UNCONFIGURED;
	item = find_item(policy, lease.ref);
	slot = find_lease(policy, lease);
	if (!item || !slot)
		return CONTENT_STALE;
	*slot = (struct content_lease){0};
	item->leases--;
	policy->usage.leases--;
	return CONTENT_OK;
}

enum content_result content_lease_check(struct content_policy *policy,
	struct content_lease lease, uint64_t now_ms)
{
	struct content_item *item;
	enum content_result result = live_item(
		policy, lease.ref, now_ms, &item);

	if (result != CONTENT_OK)
		return result;
	return find_lease(policy, lease) ? CONTENT_OK : CONTENT_STALE;
}

enum content_result content_cleanup_ack(struct content_policy *policy,
	struct content_ref ref)
{
	struct content_item *item;

	if (!policy || !policy->configured)
		return CONTENT_UNCONFIGURED;
	item = find_item(policy, ref);
	if (!item)
		return CONTENT_STALE;
	if (item->phase != CONTENT_RETIRED || item->work || item->leases)
		return CONTENT_BUSY;
	policy->usage.items--;
	policy->usage.representations -= item->descriptor.count;
	policy->usage.bytes.encoded -= item->charged.encoded;
	policy->usage.bytes.decoded -= item->charged.decoded;
	policy->usage.bytes.derived -= item->charged.derived;
	policy->usage.storage.mapped -= item->descriptor.storage.mapped;
	policy->usage.storage.locked -= item->descriptor.storage.locked;
	*item = (struct content_item){0};
	return CONTENT_OK;
}

enum content_result content_describe(struct content_policy *policy,
	struct content_ref ref, uint64_t now_ms,
	struct content_candidate *output)
{
	struct content_item *item;
	enum content_result result = live_item(policy, ref, now_ms, &item);

	if (!output)
		return CONTENT_INVALID;
	*output = (struct content_candidate){0};
	if (result == CONTENT_OK)
		*output = item->descriptor;
	return result;
}

bool content_dedup_compatible(const struct content_candidate *first,
	const struct content_candidate *second)
{
	return first && second && first->count && second->count &&
		first->count <= CONTENT_REPRESENTATIONS_MAX &&
		second->count <= CONTENT_REPRESENTATIONS_MAX &&
		hint_check(first->hint) == CONTENT_OK &&
		first->hint == second->hint && first->deadline_ms &&
		first->deadline_ms == second->deadline_ms;
}
