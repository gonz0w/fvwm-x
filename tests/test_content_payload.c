/* This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */
#include "config.h"
#include "libs/content_payload.h"
#include <assert.h>
#include <errno.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/resource.h>
#include <unistd.h>

/* Native mappings with narrowly injected syscall refusals. These synthetic
 * errors test ownership/retry logic, not actual OS exhaustion/enforcement. */
enum test_operation { TEST_MAP, TEST_PROTECT, TEST_ADVISE, TEST_LOCK,
	TEST_UNLOCK, TEST_UNMAP, TEST_OPERATIONS };
static unsigned test_calls[TEST_OPERATIONS];
static unsigned test_fail_at[TEST_OPERATIONS];
static unsigned test_fail_count[TEST_OPERATIONS];
static size_t test_page;
static uint64_t test_epoch = 1;
static struct content_process_guard test_guard;
struct test_mapping {
	void *base;
	size_t length;
	int access;
	bool active, locked;
};
static struct test_mapping test_mappings[16];

static bool test_failure(enum test_operation operation)
{
	test_calls[operation]++;
	if (test_fail_count[operation] &&
		test_calls[operation] >= test_fail_at[operation])
	{
		test_fail_count[operation]--;
		errno = EIO;
		return true;
	}
	return false;
}

static void test_schedule(enum test_operation operation,
	unsigned after, unsigned count)
{
	test_fail_at[operation] = test_calls[operation] + after;
	test_fail_count[operation] = count;
}

static struct test_mapping *test_find(const void *data, size_t length)
{
	for (size_t i = 0; i < 16; i++)
		if (test_mappings[i].active &&
			data == (unsigned char *)test_mappings[i].base +
				test_page)
		{
			assert(length ==
				test_mappings[i].length - 2 * test_page);
			return &test_mappings[i];
		}
	assert(false);
	return NULL;
}

void *content_payload_test_map(void *address, size_t length, int protection,
	int flags, int fd, off_t offset)
{
	void *mapping;

	assert(!address && protection == PROT_NONE && fd == -1 && !offset);
	assert(length >= 3 * test_page && !(length % test_page));
	if (test_failure(TEST_MAP))
		return MAP_FAILED;
	mapping = mmap(address, length, protection, flags, fd, offset);
	if (mapping == MAP_FAILED)
		return mapping;
	for (size_t i = 0; i < 16; i++)
		if (!test_mappings[i].active)
		{
			test_mappings[i] = (struct test_mapping){
				.base = mapping, .length = length,
				.access = PROT_NONE, .active = true
			};
			return mapping;
		}
	assert(false);
	return MAP_FAILED;
}

int content_payload_test_protect(void *data, size_t length, int protection)
{
	struct test_mapping *mapping = test_find(data, length);

	if (test_failure(TEST_PROTECT))
		return -1;
	int result = mprotect(data, length, protection);

	if (!result)
		mapping->access = protection;
	return result;
}

int content_payload_test_advise(void *data, size_t length, int advice)
{
	test_find(data, length);
	return test_failure(TEST_ADVISE) ? -1 : madvise(data, length, advice);
}

int content_payload_test_lock(const void *data, size_t length)
{
	struct test_mapping *mapping = test_find(data, length);

	if (test_failure(TEST_LOCK))
		return -1;
	int result = mlock(data, length);

	if (!result)
		mapping->locked = true;
	return result;
}

int content_payload_test_unlock(const void *data, size_t length)
{
	struct test_mapping *mapping = test_find(data, length);

	assert(mapping->locked);
	if (test_failure(TEST_UNLOCK))
		return -1;
	int result = munlock(data, length);

	if (!result)
		mapping->locked = false;
	return result;
}

int content_payload_test_unmap(void *address, size_t length)
{
	struct test_mapping *mapping = NULL;

	for (size_t i = 0; i < 16; i++)
		if (test_mappings[i].active && test_mappings[i].base == address)
			mapping = &test_mappings[i];
	assert(mapping && mapping->length == length);
	unsigned char *data = (unsigned char *)address + test_page;
	size_t rounded = length - 2 * test_page;

	/* Observe wiping only in valid memory; restore its previous protection
	 * before the real/injected unmap. This is test instrumentation only. */
	assert(mprotect(data, rounded, PROT_READ) == 0);
	for (size_t i = 0; i < rounded; i++)
		assert(((volatile unsigned char *)data)[i] == 0);
	assert(mprotect(data, rounded, mapping->access) == 0);
	if (test_failure(TEST_UNMAP))
		return -1;
	int result = munmap(address, length);

	if (!result)
	{
		mapping->active = false;
		mapping->locked = false;
	}
	return result;
}

struct fixture {
	struct content_payload_owner owner;
	struct content_item items[4];
	struct content_lease leases[4];
	struct content_payload_slot slots[4];
	struct content_payload_error error;
};

static struct content_candidate test_candidate(void)
{
	return (struct content_candidate){
		.hint = CONTENT_HINT_ABSENT, .deadline_ms = 1000, .count = 1,
		.reps = {{.id = 11, .kind = CONTENT_TEXT,
			.bytes = {.encoded = 16}}}
	};
}

static void test_init(struct fixture *fixture, bool enabled)
{
	struct content_limits limits = {
		.items = 4, .representations = 8, .per_item = 2,
		.leases = 4, .work = 4,
		.total = {1048576, 1048576, 1048576},
		.per_representation = {1048576, 1048576, 1048576},
		.storage = {1048576, 524288},
		.per_item_storage = {524288, 262144}, .lifetime_ms = 5000
	};

	for (size_t i = 0; i < 16; i++)
		assert(!test_mappings[i].active);
	memset(test_calls, 0, sizeof(test_calls));
	memset(test_fail_at, 0, sizeof(test_fail_at));
	memset(test_fail_count, 0, sizeof(test_fail_count));
	assert(content_payload_init(&fixture->owner, &test_guard, test_epoch++,
		&limits, fixture->items, fixture->leases, fixture->slots, 100,
		&fixture->error) == CONTENT_PAYLOAD_OK);
	assert(!fixture->owner.policy.enabled);
	assert(content_payload_set_admission(&fixture->owner, enabled, false,
		&fixture->error) == CONTENT_PAYLOAD_OK);
}

static struct content_ref test_reserve(struct fixture *fixture,
	struct content_candidate candidate)
{
	struct content_ref ref;

	assert(content_payload_reserve(&fixture->owner, &candidate, true,
		NULL, 100, &ref, &fixture->error) == CONTENT_PAYLOAD_OK);
	assert(ref.epoch && ref.item && ref.generation);
	return ref;
}

static void test_drain(struct fixture *fixture)
{
	for (size_t i = 0; fixture->owner.policy.usage.items && i < 256; i++)
	{
		bool progress;
		enum content_payload_result result =
			content_payload_cleanup_step(&fixture->owner, &progress,
				&fixture->error);

		assert(result == CONTENT_PAYLOAD_OK ||
			result == CONTENT_PAYLOAD_OS_FAILURE ||
			result == CONTENT_PAYLOAD_FATAL);
		assert(progress);
	}
	struct content_usage empty = {0};

	assert(!memcmp(&fixture->owner.policy.usage, &empty, sizeof(empty)));
	assert(!fixture->error.cleanup_pending);
	for (size_t i = 0; i < 16; i++)
		assert(!test_mappings[i].active);
}

struct read_case {
	struct fixture *fixture;
	const unsigned char *expected;
	size_t length, calls;
	bool reenter, refuse;
};

static bool test_consumer(const unsigned char *bytes, size_t length,
	void *context)
{
	struct read_case *read = context;

	read->calls++;
	assert(length == read->length);
	if (length)
		assert(bytes && !memcmp(bytes, read->expected, length));
	else
		assert(!bytes);
	if (read->reenter)
	{
		struct content_payload_error error;

		assert(content_payload_clear(&read->fixture->owner, &error) ==
			CONTENT_PAYLOAD_BUSY);
		assert(content_payload_set_admission(&read->fixture->owner,
			false, true, &error) == CONTENT_PAYLOAD_BUSY);
		assert(read->fixture->owner.busy);
	}
	return !read->refuse;
}

static void test_admission_and_bounds(void)
{
	struct fixture fixture;
	struct content_candidate candidate = test_candidate();
	struct content_ref ref = {9, 9, 9};

	test_init(&fixture, false);
	assert(content_payload_reserve(&fixture.owner, &candidate, true, NULL,
		100, &ref, &fixture.error) == CONTENT_PAYLOAD_REFUSED);
	assert(fixture.error.policy == CONTENT_EXCLUDED && !ref.epoch);
	assert(!test_calls[TEST_MAP]);
	assert(content_payload_set_admission(&fixture.owner, true, true,
		&fixture.error) == CONTENT_PAYLOAD_OK);
	assert(content_payload_reserve(&fixture.owner, &candidate, true, NULL,
		100, &ref, &fixture.error) == CONTENT_PAYLOAD_REFUSED);
	assert(content_payload_set_admission(&fixture.owner, true, false,
		&fixture.error) == CONTENT_PAYLOAD_OK);
	assert(content_payload_reserve(&fixture.owner, &candidate, false, NULL,
		100, &ref, &fixture.error) == CONTENT_PAYLOAD_REFUSED);
	candidate.hint = CONTENT_HINT_SENSITIVE;
	assert(content_payload_reserve(&fixture.owner, &candidate, true, NULL,
		100, &ref, &fixture.error) == CONTENT_PAYLOAD_REFUSED);
	candidate.hint = CONTENT_HINT_ABSENT;
	candidate.reps[0].bytes = (struct content_bytes){SIZE_MAX, 1, 0};
	assert(content_payload_reserve(&fixture.owner, &candidate, true, NULL,
		100, &ref, &fixture.error) == CONTENT_PAYLOAD_REFUSED);
	assert(fixture.error.policy == CONTENT_INVALID);
	assert(!test_calls[TEST_MAP] && !fixture.owner.policy.usage.items);
}

static void test_domains_and_publication(void)
{
	struct fixture fixture;
	struct content_candidate candidate = test_candidate(), described;
	const unsigned char encoded[] = {0, 0xff, 3, 4, 5, 6, 7, 8};
	const unsigned char decoded[] = {9, 0, 10};
	struct content_storage first, second;

	test_init(&fixture, true);
	candidate.count = 2;
	candidate.reps[0].bytes = (struct content_bytes){test_page + 1, 11, 0};
	candidate.reps[1] = (struct content_representation){
		.id = 22, .kind = CONTENT_IMAGE
	};
	candidate.storage = (struct content_storage){SIZE_MAX, SIZE_MAX};
	assert(content_storage_charge(test_page + 12, test_page, 2, 0, &first));
	assert(content_storage_charge(0, test_page, 2, 0, &second));
	struct content_ref ref = test_reserve(&fixture, candidate);

	assert(fixture.owner.policy.usage.storage.mapped ==
		first.mapped + second.mapped);
	assert(fixture.owner.policy.usage.storage.locked ==
		first.locked + second.locked);
	assert(content_payload_write(&fixture.owner, ref, 11,
		CONTENT_PAYLOAD_ENCODED, 0, encoded, sizeof(encoded), 100,
		&fixture.error) == CONTENT_PAYLOAD_OK);
	assert(content_payload_write(&fixture.owner, ref, 11,
		CONTENT_PAYLOAD_DECODED, 0, decoded, sizeof(decoded), 100,
		&fixture.error) == CONTENT_PAYLOAD_OK);
	assert(content_payload_write(&fixture.owner, ref, 22,
		CONTENT_PAYLOAD_DERIVED, 0, NULL, 0, 100,
		&fixture.error) == CONTENT_PAYLOAD_OK);
	for (size_t offset = 0; offset < 3; offset++)
		assert(content_payload_write(&fixture.owner, ref, 11,
			CONTENT_PAYLOAD_ENCODED, offset, encoded, 1, 100,
			&fixture.error) == CONTENT_PAYLOAD_REFUSED);
	assert(content_payload_write(&fixture.owner, ref, 11,
		CONTENT_PAYLOAD_ENCODED, 10, encoded, 1, 100,
		&fixture.error) == CONTENT_PAYLOAD_REFUSED);
	assert(content_payload_write(&fixture.owner, ref, 11,
		CONTENT_PAYLOAD_ENCODED, sizeof(encoded), NULL, 1, 100,
		&fixture.error) == CONTENT_PAYLOAD_REFUSED);
	assert(content_payload_write(&fixture.owner, ref, 11,
		CONTENT_PAYLOAD_ENCODED, SIZE_MAX, encoded, 1, 200,
		&fixture.error) == CONTENT_PAYLOAD_REFUSED);
	assert(content_payload_publish(&fixture.owner, ref, candidate.hint, 199,
		&fixture.error) == CONTENT_PAYLOAD_REFUSED);
	assert(fixture.error.policy == CONTENT_INVALID);
	assert(content_payload_publish(&fixture.owner, ref, candidate.hint, 200,
		&fixture.error) == CONTENT_PAYLOAD_OK);
	assert(content_describe(&fixture.owner.policy, ref, 200, &described) ==
		CONTENT_OK);
	assert(described.reps[0].bytes.encoded == sizeof(encoded));
	assert(described.reps[0].bytes.decoded == sizeof(decoded));
	assert(!described.reps[1].bytes.derived);
	struct read_case read = {
		.fixture = &fixture, .expected = encoded,
		.length = sizeof(encoded), .reenter = true
	};

	assert(content_payload_with_read(&fixture.owner, ref, 11,
		CONTENT_PAYLOAD_ENCODED, 0, sizeof(encoded),
		test_consumer, &read,
		200, &fixture.error) == CONTENT_PAYLOAD_OK);
	assert(read.calls == 1 && !fixture.owner.policy.usage.leases);
	read.expected = decoded;
	read.length = sizeof(decoded);
	read.refuse = true;
	assert(content_payload_with_read(&fixture.owner, ref, 11,
		CONTENT_PAYLOAD_DECODED, 0, sizeof(decoded),
		test_consumer, &read,
		200, &fixture.error) == CONTENT_PAYLOAD_CONSUMER_FAILED);
	assert(!fixture.owner.policy.usage.leases);
	assert(content_payload_with_read(&fixture.owner, ref, 11,
		CONTENT_PAYLOAD_ENCODED, sizeof(encoded), 1,
		test_consumer, &read,
		200, &fixture.error) == CONTENT_PAYLOAD_REFUSED);
	read.length = 0;
	read.refuse = false;
	assert(content_payload_with_read(&fixture.owner, ref, 22,
		CONTENT_PAYLOAD_DERIVED, 0, 0, test_consumer, &read, 200,
		&fixture.error) == CONTENT_PAYLOAD_OK);
	assert(content_payload_clear(&fixture.owner, &fixture.error) ==
		CONTENT_PAYLOAD_OK);
	assert(fixture.error.cleanup_pending);
	test_drain(&fixture);
}

static void test_hint_and_pause_retirement(void)
{
	struct fixture fixture;
	struct content_candidate candidate = test_candidate();

	test_init(&fixture, true);
	struct content_ref ref = test_reserve(&fixture, candidate);
	size_t charged = fixture.owner.policy.usage.storage.mapped;

	assert(content_payload_publish(&fixture.owner, ref,
		CONTENT_HINT_SENSITIVE, 100, &fixture.error) ==
		CONTENT_PAYLOAD_REFUSED);
	assert(fixture.error.policy == CONTENT_EXCLUDED);
	assert(fixture.error.cleanup_pending &&
		!fixture.owner.policy.usage.work);
	assert(fixture.owner.policy.usage.storage.mapped == charged);
	assert(content_payload_publish(&fixture.owner, ref, candidate.hint, 100,
		&fixture.error) == CONTENT_PAYLOAD_REFUSED);
	assert(!fixture.owner.fatal);
	test_drain(&fixture);
	ref = test_reserve(&fixture, candidate);
	assert(content_payload_set_admission(&fixture.owner, true, true,
		&fixture.error) == CONTENT_PAYLOAD_OK);
	assert(content_payload_write(&fixture.owner, ref, 11,
		CONTENT_PAYLOAD_ENCODED, 0, NULL, 0, 100,
		&fixture.error) == CONTENT_PAYLOAD_REFUSED);
	assert(content_payload_publish(&fixture.owner, ref, candidate.hint, 100,
		&fixture.error) == CONTENT_PAYLOAD_REFUSED);
	assert(fixture.error.cleanup_pending);
	test_drain(&fixture);
	assert(content_payload_set_admission(&fixture.owner, true, false,
		&fixture.error) == CONTENT_PAYLOAD_OK);
	ref = test_reserve(&fixture, candidate);
	assert(content_payload_publish(&fixture.owner, ref, candidate.hint,
		1000, &fixture.error) == CONTENT_PAYLOAD_REFUSED);
	assert(fixture.error.policy == CONTENT_EXPIRED);
	assert(fixture.error.cleanup_pending &&
		!fixture.owner.policy.usage.work);
	test_drain(&fixture);
}

static void test_replacement_and_expiry(void)
{
	struct fixture fixture;
	struct content_candidate candidate = test_candidate();
	struct content_ref next;
	struct read_case read = {.fixture = &fixture};

	test_init(&fixture, true);
	struct content_ref prior = test_reserve(&fixture, candidate);

	assert(content_payload_publish(&fixture.owner, prior, candidate.hint,
		100, &fixture.error) == CONTENT_PAYLOAD_OK);
	next = prior;
	assert(content_payload_reserve(&fixture.owner, &candidate, true, &next,
		100, &next, &fixture.error) == CONTENT_PAYLOAD_OK);
	assert(next.item == prior.item &&
		next.generation == prior.generation + 1);
	assert(fixture.owner.policy.usage.items == 2);
	assert(content_payload_with_read(&fixture.owner, prior, 11,
		CONTENT_PAYLOAD_ENCODED, 0, 0, test_consumer, &read, 100,
		&fixture.error) == CONTENT_PAYLOAD_REFUSED);
	assert(!read.calls);
	assert(content_payload_publish(&fixture.owner, prior, candidate.hint,
		100, &fixture.error) == CONTENT_PAYLOAD_REFUSED);
	assert(!fixture.owner.fatal);
	assert(content_payload_expire(&fixture.owner, 1000, &fixture.error) ==
		CONTENT_PAYLOAD_OK);
	assert(fixture.error.cleanup_pending);
	test_drain(&fixture);
}

static void test_prepare_refusals(void)
{
	const enum test_operation operations[] = {
		TEST_MAP, TEST_PROTECT, TEST_LOCK,
#if defined(MADV_DONTDUMP) || defined(MADV_NOCORE)
		TEST_ADVISE,
#endif
	};

	for (size_t i = 0; i < sizeof(operations) / sizeof(*operations); i++)
	{
		struct fixture fixture;
		struct content_candidate candidate = test_candidate();
		struct content_ref ref = {9, 9, 9};

		test_init(&fixture, true);
		test_schedule(operations[i], 1, 1);
		assert(content_payload_reserve(&fixture.owner, &candidate, true,
			NULL, 100, &ref, &fixture.error) ==
			CONTENT_PAYLOAD_OS_FAILURE);
		assert(!ref.epoch && fixture.error.cleanup_pending);
		assert(fixture.owner.policy.usage.items == 1 &&
			!fixture.owner.policy.usage.work);
		assert(!fixture.owner.fatal);
		test_drain(&fixture);
	}
	struct fixture fixture;
	struct content_candidate candidate = test_candidate();
	struct content_ref next;

	test_init(&fixture, true);
	struct content_ref prior = test_reserve(&fixture, candidate);

	assert(content_payload_publish(&fixture.owner, prior, candidate.hint,
		100, &fixture.error) == CONTENT_PAYLOAD_OK);
	test_schedule(TEST_LOCK, 1, 1);
	assert(content_payload_reserve(&fixture.owner, &candidate, true, &prior,
		100, &next, &fixture.error) == CONTENT_PAYLOAD_OS_FAILURE);
	assert(!next.epoch && fixture.owner.policy.usage.items == 2);
	assert(content_payload_publish(&fixture.owner, prior, candidate.hint,
		100, &fixture.error) == CONTENT_PAYLOAD_REFUSED);
	assert(!fixture.owner.fatal);
	test_drain(&fixture);
}

static void test_cleanup_retries(void)
{
	for (unsigned scenario = 0; scenario < 3; scenario++)
	{
		struct fixture fixture;
		struct content_candidate candidate = test_candidate();
		bool progress;

		test_init(&fixture, true);
		struct content_ref ref = test_reserve(&fixture, candidate);
		size_t charge = fixture.owner.policy.usage.storage.mapped;

		assert(content_payload_publish(&fixture.owner, ref,
			candidate.hint, 100, &fixture.error) ==
			CONTENT_PAYLOAD_OK);
		assert(content_payload_retire(&fixture.owner, ref,
			&fixture.error) == CONTENT_PAYLOAD_OK);
		assert(fixture.error.cleanup_pending);
		if (scenario != 1)
			test_schedule(TEST_UNLOCK, 1, 1);
		if (scenario != 2)
			test_schedule(TEST_UNMAP, 1, 1);
		enum content_payload_result result = CONTENT_PAYLOAD_OK;

		for (size_t i = 0; result == CONTENT_PAYLOAD_OK && i < 32; i++)
		{
			result = content_payload_cleanup_step(&fixture.owner,
				&progress, &fixture.error);
			assert(progress && fixture.error.cleanup_pending);
		}
		assert(result == CONTENT_PAYLOAD_OS_FAILURE);
		assert(progress && fixture.error.cleanup_pending);
		assert(fixture.owner.policy.usage.storage.mapped == charge);
		test_drain(&fixture);
		assert(test_calls[TEST_UNLOCK] == (scenario == 0 ? 2u : 1u));
		assert(test_calls[TEST_UNMAP] == (scenario == 2 ? 1u : 2u));
	}
}

static void test_fatal_closure(void)
{
	struct fixture fixture;
	struct content_candidate candidate = test_candidate();
	const unsigned char bytes[] = {1, 2, 3};
	struct content_ref ignored;

	test_init(&fixture, true);
	struct content_ref ref = test_reserve(&fixture, candidate);

	test_schedule(TEST_PROTECT, 2, 1);
	assert(content_payload_write(&fixture.owner, ref, 11,
		CONTENT_PAYLOAD_ENCODED, 0, bytes, sizeof(bytes), 100,
		&fixture.error) == CONTENT_PAYLOAD_FATAL);
	assert(fixture.owner.fatal && fixture.error.cleanup_pending);
	assert(content_payload_reserve(&fixture.owner, &candidate, true, NULL,
		100, &ignored, &fixture.error) == CONTENT_PAYLOAD_FATAL);
	assert(!ignored.epoch);
	assert(content_payload_set_admission(&fixture.owner, true, false,
		&fixture.error) == CONTENT_PAYLOAD_FATAL);
	test_schedule(TEST_PROTECT, 1, 1);
	bool progress;

	assert(content_payload_cleanup_step(&fixture.owner, &progress,
		&fixture.error) == CONTENT_PAYLOAD_FATAL);
	assert(progress && fixture.error.cleanup_pending);
	test_drain(&fixture);
	assert(fixture.owner.fatal);
}

static void test_guard_and_live_cleanup(void)
{
	struct fixture fixture;
	struct content_process_guard stale = test_guard;
	struct content_limits limits = {0};
	struct rlimit before, after;

	stale.pid = 0;
	assert(getrlimit(RLIMIT_CORE, &before) == 0);
	assert(content_payload_init(&fixture.owner, &stale, test_epoch++,
		&limits, fixture.items, fixture.leases, fixture.slots, 100,
		&fixture.error) ==
		CONTENT_PAYLOAD_REFUSED);
	assert(!fixture.owner.policy.configured);
	assert(getrlimit(RLIMIT_CORE, &after) == 0);
	assert(before.rlim_cur == after.rlim_cur &&
		before.rlim_max == after.rlim_max);
	struct fixture reference;

	test_init(&reference, false);
	limits = reference.owner.policy.limits;
	fixture.owner.guard = test_guard;
	fixture.owner.policy.limits = limits;
	assert(content_payload_init(&fixture.owner, &fixture.owner.guard,
		test_epoch++, &fixture.owner.policy.limits, fixture.items,
		fixture.leases, fixture.slots, 100, &fixture.error) ==
		CONTENT_PAYLOAD_OK);
	assert(content_payload_set_admission(&fixture.owner, true, false,
		&fixture.error) == CONTENT_PAYLOAD_OK);
	struct content_candidate candidate = test_candidate();
	struct content_ref ref = test_reserve(&fixture, candidate);
	bool progress = true;

	assert(content_payload_cleanup_step(&fixture.owner, &progress,
		&fixture.error) == CONTENT_PAYLOAD_OK);
	assert(!progress && !fixture.error.cleanup_pending);
	assert(content_payload_clear(&fixture.owner, &fixture.error) ==
		CONTENT_PAYLOAD_OK);
	assert(fixture.owner.policy.usage.work == 1);
	assert(content_payload_publish(&fixture.owner, ref, candidate.hint, 100,
		&fixture.error) == CONTENT_PAYLOAD_REFUSED);
	assert(fixture.error.policy == CONTENT_STALE &&
		fixture.error.cleanup_pending &&
		!fixture.owner.policy.usage.work);
	test_drain(&fixture);
	assert(content_payload_publish(&fixture.owner, ref, candidate.hint, 100,
		&fixture.error) == CONTENT_PAYLOAD_REFUSED);
	assert(!fixture.owner.fatal && !fixture.error.cleanup_pending);
}

static void test_held_lease_and_reader_refusal(void)
{
	struct fixture fixture;
	struct content_candidate candidate = test_candidate();
	struct content_lease held;
	struct read_case read = {.fixture = &fixture};
	bool progress;

	test_init(&fixture, true);
	struct content_ref ref = test_reserve(&fixture, candidate);

	assert(content_payload_publish(&fixture.owner, ref, candidate.hint, 100,
		&fixture.error) == CONTENT_PAYLOAD_OK);
	/* The fixture holds an extra policy lease to verify the composition
	 * boundary; ordinary callers must not mutate the owner's policy. */
	assert(content_acquire(&fixture.owner.policy, ref, 11, 100, &held) ==
		CONTENT_OK);
	assert(content_payload_retire(&fixture.owner, ref, &fixture.error) ==
		CONTENT_PAYLOAD_OK);
	assert(content_payload_cleanup_step(&fixture.owner, &progress,
		&fixture.error) == CONTENT_PAYLOAD_OK);
	assert(!progress && fixture.error.cleanup_pending);
	assert(!test_calls[TEST_UNMAP]);
	assert(content_release(&fixture.owner.policy, held) == CONTENT_OK);
	test_drain(&fixture);
	ref = test_reserve(&fixture, candidate);
	const unsigned char byte = 42;

	assert(content_payload_write(&fixture.owner, ref, 11,
		CONTENT_PAYLOAD_ENCODED, 0, &byte, 1, 100, &fixture.error) ==
		CONTENT_PAYLOAD_OK);
	assert(content_payload_publish(&fixture.owner, ref, candidate.hint, 100,
		&fixture.error) == CONTENT_PAYLOAD_OK);
	read.expected = &byte;
	read.length = 1;
	test_schedule(TEST_PROTECT, 1, 1);
	assert(content_payload_with_read(&fixture.owner, ref, 11,
		CONTENT_PAYLOAD_ENCODED, 0, 1, test_consumer, &read, 100,
		&fixture.error) == CONTENT_PAYLOAD_OS_FAILURE);
	assert(!read.calls && !fixture.owner.policy.usage.leases);
	assert(!fixture.owner.fatal && fixture.error.cleanup_pending);
	test_drain(&fixture);
}

static void test_partial_preparation_and_chunk_limits(void)
{
	struct fixture fixture;
	struct content_candidate candidate = test_candidate();
	struct content_ref ref;
	unsigned char bytes[CONTENT_PAYLOAD_CHUNK_MAX] = {7};

	test_init(&fixture, true);
	candidate.count = 2;
	candidate.reps[1] = (struct content_representation){
		.id = 22, .kind = CONTENT_IMAGE, .bytes = {.encoded = 1}
	};
	test_schedule(TEST_MAP, 2, 1);
	assert(content_payload_reserve(&fixture.owner, &candidate, true, NULL,
		100, &ref, &fixture.error) == CONTENT_PAYLOAD_OS_FAILURE);
	assert(!ref.epoch && test_calls[TEST_LOCK] == 1);
	assert(fixture.owner.policy.usage.representations == 2);
	test_drain(&fixture);
	candidate = test_candidate();
	candidate.reps[0].bytes.encoded = sizeof(bytes) + 1;
	ref = test_reserve(&fixture, candidate);
	assert(content_payload_write(&fixture.owner, ref, 11,
		CONTENT_PAYLOAD_ENCODED, 0, bytes, sizeof(bytes) + 1, 100,
		&fixture.error) == CONTENT_PAYLOAD_REFUSED);
	assert(content_payload_write(&fixture.owner, ref, 11,
		CONTENT_PAYLOAD_ENCODED, 0, bytes, sizeof(bytes), 100,
		&fixture.error) == CONTENT_PAYLOAD_OK);
	assert(content_payload_write(&fixture.owner, ref, 11,
		CONTENT_PAYLOAD_ENCODED, sizeof(bytes), bytes, 1, 100,
		&fixture.error) == CONTENT_PAYLOAD_OK);
	assert(content_payload_publish(&fixture.owner, ref, candidate.hint, 100,
		&fixture.error) == CONTENT_PAYLOAD_OK);
	assert(content_payload_clear(&fixture.owner, &fixture.error) ==
		CONTENT_PAYLOAD_OK);
	unsigned before = test_calls[TEST_UNMAP];
	bool progress;

	assert(content_payload_cleanup_step(&fixture.owner, &progress,
		&fixture.error) == CONTENT_PAYLOAD_OK);
	assert(progress && test_calls[TEST_UNMAP] == before);
	assert(fixture.error.cleanup_pending);
	test_drain(&fixture);
}

int main(void)
{
	struct content_payload_error error;
	struct rlimit limit;
	long page = sysconf(_SC_PAGESIZE);

	if (page <= 0 || page > 65536 ||
		content_payload_process_prepare(&test_guard, &error) !=
			CONTENT_PAYLOAD_OK ||
		getrlimit(RLIMIT_MEMLOCK, &limit) < 0 ||
		limit.rlim_cur < (rlim_t)(8 * page))
		return 77;
	test_page = (size_t)page;
	test_guard_and_live_cleanup();
	test_admission_and_bounds();
	test_domains_and_publication();
	test_hint_and_pause_retirement();
	test_replacement_and_expiry();
	test_prepare_refusals();
	test_cleanup_retries();
	test_held_lease_and_reader_refusal();
	test_partial_preparation_and_chunk_limits();
	test_fatal_closure();
	return 0;
}
