/* This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */
#include "config.h"
#include "libs/content_policy.h"
#include <assert.h>
#include <string.h>

static struct content_policy policy;
static struct content_item items[3];
static struct content_lease leases[3];
static struct content_limits limits;
static struct content_candidate candidate;

static void setup(void)
{
	limits = (struct content_limits){
		.items = 3, .representations = 4, .per_item = 2,
		.leases = 3, .work = 2,
		.total = {100, 200, 100}, .per_representation = {80, 160, 80},
		.lifetime_ms = 1000
	};
	candidate = (struct content_candidate){
		.hint = CONTENT_HINT_ABSENT, .deadline_ms = 500, .count = 1,
		.reps = {{1, CONTENT_TEXT, {40, 80, 20}}}
	};
	assert(content_policy_init(&policy, 7, &limits, items, leases, 10)
		== CONTENT_OK);
	policy.enabled = true;
}

static struct content_ref reserve(void)
{
	struct content_ref ref;

	assert(content_reserve(&policy, &candidate, true, NULL,
		policy.last_ms, &ref)
		== CONTENT_OK);
	return ref;
}

static void publish(struct content_ref ref)
{
	assert(content_finish(&policy, ref, candidate.hint, true,
		policy.last_ms + 1)
		== CONTENT_OK);
}

static void admission(void)
{
	struct content_ref ref = {99, 99, 99};
	struct content_usage before;

	setup();
	policy.enabled = false;
	assert(content_reserve(&policy, &candidate, true, NULL, 10, &ref)
		== CONTENT_EXCLUDED && ref.epoch == 0);
	policy.enabled = true;
	before = policy.usage;
	for (int hint = CONTENT_HINT_SENSITIVE;
		hint <= CONTENT_HINT_UNAVAILABLE; hint++)
	{
		candidate.hint = hint;
		assert(content_reserve(&policy, &candidate, true, NULL, 10,
			&ref) == CONTENT_EXCLUDED);
	}
	assert(memcmp(&before, &policy.usage, sizeof(before)) == 0);
	candidate.hint = CONTENT_HINT_ABSENT;
	assert(content_reserve(&policy, &candidate, false, NULL, 10, &ref)
		== CONTENT_EXCLUDED);
	candidate.count = 2;
	candidate.reps[1] = candidate.reps[0];
	assert(content_reserve(&policy, &candidate, true, NULL, 10, &ref)
		== CONTENT_INVALID);
	candidate.reps[1].id = 2;
	candidate.reps[1].bytes.encoded = SIZE_MAX;
	assert(content_reserve(&policy, &candidate, true, NULL, 10, &ref)
		== CONTENT_FULL);
	assert(policy.next_item == 0 && policy.usage.items == 0);
	candidate.count = 1;
	ref = reserve();
	candidate.reps[0].id = 99;
	assert(items[0].descriptor.reps[0].id == 1);
	assert(content_finish(&policy, ref, CONTENT_HINT_SENSITIVE, true, 11)
		== CONTENT_EXCLUDED);
	assert(items[0].phase == CONTENT_RETIRED && policy.usage.work == 0);
	assert(content_cleanup_ack(&policy, ref) == CONTENT_OK);
}

static void cancellation(void)
{
	struct content_ref ref, other;
	struct content_lease lease, bad;

	setup();
	ref = reserve();
	assert(content_cleanup_ack(&policy, ref) == CONTENT_BUSY);
	assert(content_retire(&policy, ref) == CONTENT_OK);
	assert(content_cleanup_ack(&policy, ref) == CONTENT_BUSY);
	other = reserve();
	assert(content_reserve(&policy, &candidate, true, NULL, 10, &other)
		== CONTENT_FULL);
	assert(policy.usage.items == 2 && policy.usage.bytes.encoded == 80);
	assert(content_finish(&policy, ref, CONTENT_HINT_ABSENT, true, 11)
		== CONTENT_STALE);
	assert(content_finish(&policy, ref, CONTENT_HINT_ABSENT, true, 11)
		== CONTENT_STALE && policy.usage.work == 1);
	assert(content_cleanup_ack(&policy, ref) == CONTENT_OK);
	assert(content_cleanup_ack(&policy, ref) == CONTENT_STALE);
	/* Reuse the array slot, never its item identity. */
	other = reserve();
	assert(other.item != ref.item);
	assert(content_finish(&policy, ref, CONTENT_HINT_ABSENT, false, 11)
		== CONTENT_STALE && policy.usage.work == 2);
	publish(other);
	assert(content_acquire(&policy, other, 1, 12, &lease) == CONTENT_OK);
	bad = lease;
	bad.ref.epoch++;
	assert(content_release(&policy, bad) == CONTENT_STALE);
	assert(content_retire(&policy, other) == CONTENT_OK);
	assert(content_cleanup_ack(&policy, other) == CONTENT_BUSY);
	assert(content_release(&policy, lease) == CONTENT_OK);
	assert(content_release(&policy, lease) == CONTENT_STALE);
	assert(content_cleanup_ack(&policy, other) == CONTENT_OK);
}

static void replacement(void)
{
	struct content_ref first, second, output;
	struct content_lease lease;
	struct content_candidate view;

	setup();
	first = reserve();
	publish(first);
	assert(content_acquire(&policy, first, 1, 12, &lease) == CONTENT_OK);
	assert(content_reserve(&policy, &candidate, true, &first, 13, &second)
		== CONTENT_OK);
	assert(second.item == first.item && second.generation == 2);
	assert(policy.usage.bytes.encoded == 80);
	assert(content_describe(&policy, first, 13, &view) == CONTENT_STALE);
	assert(view.count == 0);
	assert(content_reserve(&policy, &candidate, true, &first, 13, &output)
		== CONTENT_STALE);
	assert(content_cleanup_ack(&policy, first) == CONTENT_BUSY);
	assert(content_release(&policy, lease) == CONTENT_OK);
	assert(content_cleanup_ack(&policy, first) == CONTENT_OK);
	assert(content_clear(&policy) == CONTENT_OK);
	assert(content_finish(&policy, second, CONTENT_HINT_ABSENT, true, 14)
		== CONTENT_STALE);
	assert(content_cleanup_ack(&policy, second) == CONTENT_OK);
	assert(policy.usage.items == 0 && policy.usage.bytes.encoded == 0);
}

static void expiry_and_identity(void)
{
	struct content_ref ref, output;
	struct content_lease lease;
	struct content_candidate view;

	setup();
	ref = reserve();
	publish(ref);
	assert(content_pin(&policy, ref, true, 12) == CONTENT_OK);
	assert(content_describe(&policy, ref, 500, &view) == CONTENT_EXPIRED);
	assert(content_acquire(&policy, ref, 1, 499, &lease)
		== CONTENT_INVALID && lease.id == 0);
	assert(content_acquire(&policy, ref, 1, 500, &lease)
		== CONTENT_EXPIRED && lease.id == 0);
	assert(content_expire(&policy, 500) == CONTENT_OK);
	assert(content_cleanup_ack(&policy, ref) == CONTENT_OK);
	assert(content_expire(&policy, 499) == CONTENT_INVALID);
	setup();
	policy.next_item = UINT64_MAX;
	assert(content_reserve(&policy, &candidate, true, NULL, 10, &output)
		== CONTENT_EXHAUSTED && policy.usage.items == 0);
	setup();
	ref = reserve();
	publish(ref);
	policy.next_lease = UINT64_MAX;
	assert(content_acquire(&policy, ref, 1, 12, &lease)
		== CONTENT_EXHAUSTED && policy.usage.leases == 0);
	items[0].ref.generation = UINT64_MAX;
	ref = items[0].ref;
	assert(content_reserve(&policy, &candidate, true, &ref, 13, &output)
		== CONTENT_EXHAUSTED && policy.usage.items == 1);
	assert(content_dedup_compatible(&candidate, &candidate));
	view = candidate;
	view.deadline_ms--;
	assert(!content_dedup_compatible(&candidate, &view));
}

static void publication_and_bounds(void)
{
	struct content_ref ref, output;
	struct content_lease held[3], extra;
	struct content_candidate view;

	setup();
	ref = reserve();
	assert(content_finish(&policy, ref, CONTENT_HINT_ABSENT, true, 500)
		== CONTENT_EXPIRED);
	assert(policy.usage.work == 0 && policy.usage.items == 1);
	assert(content_cleanup_ack(&policy, ref) == CONTENT_OK);
	setup();
	ref = reserve();
	policy.paused = true;
	assert(content_finish(&policy, ref, CONTENT_HINT_ABSENT, true, 11)
		== CONTENT_EXCLUDED);
	assert(content_cleanup_ack(&policy, ref) == CONTENT_OK);
	setup();
	candidate.reps[7].id = 999;
	ref = reserve();
	publish(ref);
	assert(content_describe(&policy, ref, 12, &view) == CONTENT_OK);
	assert(view.reps[7].id == 0);
	for (size_t i = 0; i < 3; i++)
		assert(content_acquire(&policy, ref, 1, 12, &held[i])
			== CONTENT_OK);
	assert(content_acquire(&policy, ref, 1, 12, &extra) == CONTENT_FULL);
	assert(content_release(&policy, held[0]) == CONTENT_OK);
	assert(content_acquire(&policy, ref, 1, 12, &extra) == CONTENT_OK);
	assert(extra.id != held[0].id);
	assert(content_release(&policy, held[0]) == CONTENT_STALE);
	/* Refused replacement leaves the old live object accessible. */
	candidate.reps[0].bytes.encoded = 80;
	assert(content_reserve(&policy, &candidate, true, &ref, 13, &output)
		== CONTENT_FULL);
	assert(content_describe(&policy, ref, 13, &view) == CONTENT_OK);
	ref.epoch++;
	assert(content_describe(&policy, ref, 13, &view) == CONTENT_STALE);
	assert(content_retire(&policy, ref) == CONTENT_STALE);
	assert(content_acquire(&policy, ref, 1, 13, &extra) == CONTENT_STALE);
}

int main(void)
{
	admission();
	cancellation();
	replacement();
	expiry_and_identity();
	publication_and_bounds();
	return 0;
}
