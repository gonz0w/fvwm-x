/* This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */
#include "config.h"
#include "libs/content_payload.h"
#include <errno.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/resource.h>
#include <unistd.h>
#ifdef __linux__
#include <sys/prctl.h>
#endif

/* A separately compiled native test supplies these narrow syscall wrappers.
 * Production calls are direct, with no fault settings or dispatch table. */
#ifdef CONTENT_PAYLOAD_TEST
extern void *content_payload_test_map(void *, size_t, int, int, int, off_t);
extern int content_payload_test_protect(void *, size_t, int);
extern int content_payload_test_advise(void *, size_t, int);
extern int content_payload_test_lock(const void *, size_t);
extern int content_payload_test_unlock(const void *, size_t);
extern int content_payload_test_unmap(void *, size_t);
#define region_map content_payload_test_map
#define region_protect content_payload_test_protect
#define region_advise content_payload_test_advise
#define region_lock content_payload_test_lock
#define region_unlock content_payload_test_unlock
#define region_unmap content_payload_test_unmap
#else
#define region_map mmap
#define region_protect mprotect
#define region_advise madvise
#define region_lock mlock
#define region_unlock munlock
#define region_unmap munmap
#endif

static unsigned process_kind(void)
{
#ifdef __linux__
	return 2;
#elif defined(__FreeBSD__) || defined(__OpenBSD__)
	return 1;
#else
	return 0;
#endif
}

static bool error_init(struct content_payload_error *error)
{
	if (!error)
		return false;
	*error = (struct content_payload_error){.policy = CONTENT_OK};
	return true;
}

static enum content_payload_result policy_error(
	struct content_payload_error *error, enum content_result result)
{
	error->policy = result;
	return result == CONTENT_OK ? CONTENT_PAYLOAD_OK :
		CONTENT_PAYLOAD_REFUSED;
}

static enum content_payload_result os_error(struct content_payload_error *error,
	enum content_payload_operation operation)
{
	error->operation = operation;
	error->number = errno ? errno : EIO;
	return CONTENT_PAYLOAD_OS_FAILURE;
}

static enum content_payload_result verify_process(
	const struct content_process_guard *guard,
	struct content_payload_error *error)
{
	struct rlimit limit;
	unsigned kind = process_kind();

	if (!kind)
	{
		error->operation = CONTENT_PAYLOAD_PROCESS;
		return CONTENT_PAYLOAD_UNSUPPORTED;
	}
	if (!guard || guard->pid != getpid() || guard->kind != kind)
		return policy_error(error, CONTENT_STALE);
	if (getrlimit(RLIMIT_CORE, &limit) < 0)
		return os_error(error, CONTENT_PAYLOAD_PROCESS);
	if (limit.rlim_cur || limit.rlim_max)
		return policy_error(error, CONTENT_EXCLUDED);
#ifdef __linux__
	int dumpable = prctl(PR_GET_DUMPABLE, 0L, 0L, 0L, 0L);

	if (dumpable < 0)
		return os_error(error, CONTENT_PAYLOAD_PROCESS);
	if (dumpable)
		return policy_error(error, CONTENT_EXCLUDED);
#endif
	return CONTENT_PAYLOAD_OK;
}

enum content_payload_result content_payload_process_prepare(
	struct content_process_guard *output,
	struct content_payload_error *error)
{
	struct rlimit zero = {0, 0};
	struct content_process_guard guard = {getpid(), process_kind()};

	if (output)
		*output = (struct content_process_guard){0};
	if (!error_init(error) || !output)
		return CONTENT_PAYLOAD_INVALID;
	if (!guard.kind)
	{
		error->operation = CONTENT_PAYLOAD_PROCESS;
		return CONTENT_PAYLOAD_UNSUPPORTED;
	}
	if (setrlimit(RLIMIT_CORE, &zero) < 0)
		return os_error(error, CONTENT_PAYLOAD_PROCESS);
#ifdef __linux__
	if (prctl(PR_SET_DUMPABLE, 0L, 0L, 0L, 0L) < 0)
		return os_error(error, CONTENT_PAYLOAD_PROCESS);
#endif
	enum content_payload_result result = verify_process(&guard, error);

	if (result == CONTENT_PAYLOAD_OK)
		*output = guard;
	return result;
}

enum content_payload_result content_payload_init(
	struct content_payload_owner *owner,
	const struct content_process_guard *guard, uint64_t epoch,
	const struct content_limits *limits, struct content_item *items,
	struct content_lease *leases, struct content_payload_slot *slots,
	uint64_t now_ms, struct content_payload_error *error)
{
	long page;
	enum content_payload_result result;
	struct content_process_guard initial_guard = guard ? *guard :
		(struct content_process_guard){0};
	struct content_limits initial_limits;

	if (limits)
		initial_limits = *limits;
	if (owner)
		*owner = (struct content_payload_owner){0};
	if (!error_init(error) || !owner || !slots)
		return CONTENT_PAYLOAD_INVALID;
	result = verify_process(guard ? &initial_guard : NULL, error);
	if (result != CONTENT_PAYLOAD_OK)
		return result;
	errno = 0;
	page = sysconf(_SC_PAGESIZE);
	if (page <= 0 || page > 65536 || (page & (page - 1)))
	{
		if (!errno)
			errno = EINVAL;
		return os_error(error, CONTENT_PAYLOAD_PAGE);
	}
	enum content_result admitted = content_policy_init(&owner->policy,
		epoch, limits ? &initial_limits : NULL, items, leases, now_ms);

	if (admitted != CONTENT_OK)
		return policy_error(error, admitted);
	memset(slots, 0, limits->items * sizeof(*slots));
	owner->slots = slots;
	owner->guard = initial_guard;
	owner->page = (size_t)page;
	return CONTENT_PAYLOAD_OK;
}

static bool cleanup_pending(const struct content_payload_owner *owner)
{
	for (size_t i = 0; i < owner->policy.limits.items; i++)
		if (owner->policy.items[i].phase == CONTENT_RETIRED)
			return true;
	return false;
}

static enum content_payload_result leave(struct content_payload_owner *owner,
	enum content_payload_result result, struct content_payload_error *error)
{
	error->cleanup_pending = cleanup_pending(owner);
	owner->busy = false;
	return result;
}

static enum content_payload_result poison(struct content_payload_owner *owner,
	struct content_payload_error *error)
{
	if (!owner->fatal)
		owner->fatal_error = *error;
	owner->fatal = true;
	content_clear(&owner->policy);
	error->cleanup_pending = cleanup_pending(owner);
	return CONTENT_PAYLOAD_FATAL;
}

static enum content_payload_result enter(struct content_payload_owner *owner,
	bool allow_fatal, struct content_payload_error *error)
{
	if (!owner || !owner->policy.configured)
		return policy_error(error, CONTENT_UNCONFIGURED);
	error->cleanup_pending = cleanup_pending(owner);
	if (owner->busy)
	{
		error->policy = CONTENT_BUSY;
		return CONTENT_PAYLOAD_BUSY;
	}
	if (owner->guard.pid != getpid())
		return policy_error(error, CONTENT_STALE);
	if (owner->fatal && !allow_fatal)
	{
		*error = owner->fatal_error;
		error->cleanup_pending = cleanup_pending(owner);
		return CONTENT_PAYLOAD_FATAL;
	}
	owner->busy = true;
	return CONTENT_PAYLOAD_OK;
}

static enum content_payload_result observe(struct content_payload_owner *owner,
	uint64_t now_ms, struct content_payload_error *error)
{
	return policy_error(error,
		content_observe_time(&owner->policy, now_ms));
}

static enum content_payload_result process_access(
	struct content_payload_owner *owner,
	struct content_payload_error *error)
{
	enum content_payload_result result =
		verify_process(&owner->guard, error);

	return result == CONTENT_PAYLOAD_OK ? result : poison(owner, error);
}

static bool same_ref(struct content_ref first, struct content_ref second)
{
	return first.epoch == second.epoch && first.item == second.item &&
		first.generation == second.generation;
}

static struct content_payload_slot *find_slot(
	struct content_payload_owner *owner, struct content_ref ref)
{
	for (size_t i = 0; i < owner->policy.limits.items; i++)
		if (owner->slots[i].item &&
			same_ref(owner->slots[i].ref, ref))
			return &owner->slots[i];
	return NULL;
}

static struct content_payload_region *find_region(
	struct content_payload_slot *slot, uint32_t representation)
{
	if (slot)
		for (size_t i = 0; i < slot->count; i++)
			if (slot->regions[i].representation == representation)
				return &slot->regions[i];
	return NULL;
}

static bool add(size_t first, size_t second, size_t *output)
{
	if (second > SIZE_MAX - first)
		return false;
	*output = first + second;
	return true;
}

static bool region_charge(struct content_bytes bytes, size_t page,
	struct content_storage *charge)
{
	size_t total;

	return add(bytes.encoded, bytes.decoded, &total) &&
		add(total, bytes.derived, &total) &&
		content_storage_charge(total, page, 2, 0, charge);
}

static bool partition(struct content_payload_region *region,
	enum content_payload_domain domain, size_t *start,
	size_t *capacity, size_t **written)
{
	*start = *capacity = 0;
	*written = NULL;
	switch (domain)
	{
	case CONTENT_PAYLOAD_ENCODED:
		*capacity = region->capacity.encoded;
		*written = &region->written.encoded;
		break;
	case CONTENT_PAYLOAD_DECODED:
		*start = region->capacity.encoded;
		*capacity = region->capacity.decoded;
		*written = &region->written.decoded;
		break;
	case CONTENT_PAYLOAD_DERIVED:
		*start = region->capacity.encoded + region->capacity.decoded;
		*capacity = region->capacity.derived;
		*written = &region->written.derived;
		break;
	default:
		return false;
	}
	return true;
}

static struct content_bytes domain_bytes(enum content_payload_domain domain,
	size_t value)
{
	struct content_bytes bytes = {0};

	if (domain == CONTENT_PAYLOAD_ENCODED)
		bytes.encoded = value;
	else if (domain == CONTENT_PAYLOAD_DECODED)
		bytes.decoded = value;
	else if (domain == CONTENT_PAYLOAD_DERIVED)
		bytes.derived = value;
	return bytes;
}

static enum content_payload_result close_region(
	struct content_payload_owner *owner,
	struct content_payload_region *region,
	struct content_payload_error *error)
{
	if (!region->mapping || region->closed)
		return CONTENT_PAYLOAD_OK;
	if (region_protect(region->data, region->rounded, PROT_NONE) < 0)
	{
		os_error(error, CONTENT_PAYLOAD_PROTECT);
		return poison(owner, error);
	}
	region->closed = true;
	return CONTENT_PAYLOAD_OK;
}

static enum content_payload_result prepare_region(
	struct content_payload_owner *owner,
	struct content_payload_region *region,
	struct content_payload_error *error)
{
	struct content_storage cost;
	int flags = MAP_PRIVATE | MAP_ANON;

	if (!region_charge(region->capacity, owner->page, &cost))
		return policy_error(error, CONTENT_INVALID);
	region->rounded = cost.locked;
	region->mapped = cost.mapped;
#if defined(MAP_CONCEAL)
	flags |= MAP_CONCEAL;
#elif defined(MAP_NOCORE)
	flags |= MAP_NOCORE;
#endif
	region->mapping = region_map(NULL, cost.mapped, PROT_NONE,
		flags, -1, 0);
	if (region->mapping == MAP_FAILED)
	{
		region->mapping = NULL;
		return os_error(error, CONTENT_PAYLOAD_MAP);
	}
	region->data = (unsigned char *)region->mapping + owner->page;
	region->closed = false;
	if (region_protect(region->data, region->rounded,
		PROT_READ | PROT_WRITE) < 0)
		return os_error(error, CONTENT_PAYLOAD_PROTECT);
#if defined(MADV_DONTDUMP)
	if (region_advise(region->data, region->rounded, MADV_DONTDUMP) < 0)
		return os_error(error, CONTENT_PAYLOAD_DUMP);
#elif defined(MADV_NOCORE)
	if (region_advise(region->data, region->rounded, MADV_NOCORE) < 0)
		return os_error(error, CONTENT_PAYLOAD_DUMP);
#elif !defined(MAP_CONCEAL) && !defined(MAP_NOCORE)
	error->operation = CONTENT_PAYLOAD_DUMP;
	return CONTENT_PAYLOAD_UNSUPPORTED;
#endif
	if (region_lock(region->data, region->rounded) < 0)
		return os_error(error, CONTENT_PAYLOAD_LOCK);
	region->locked = true;
	return close_region(owner, region, error);
}

enum content_payload_result content_payload_set_admission(
	struct content_payload_owner *owner, bool enabled, bool paused,
	struct content_payload_error *error)
{
	enum content_payload_result result;

	if (!error_init(error))
		return CONTENT_PAYLOAD_INVALID;
	result = enter(owner, false, error);
	if (result != CONTENT_PAYLOAD_OK)
		return result;
	owner->policy.enabled = enabled;
	owner->policy.paused = paused;
	return leave(owner, CONTENT_PAYLOAD_OK, error);
}

enum content_payload_result content_payload_reserve(
	struct content_payload_owner *owner,
	const struct content_candidate *candidate, bool source_allowed,
	const struct content_ref *replace, uint64_t now_ms,
	struct content_ref *output, struct content_payload_error *error)
{
	struct content_candidate request;
	/* Match the shared policy's replace/output aliasing convention. */
	struct content_ref prior = replace ? *replace : (struct content_ref){0};
	struct content_ref ref;
	struct content_payload_slot *slot = NULL;
	enum content_payload_result result;

	if (output)
		*output = (struct content_ref){0};
	if (!error_init(error) || !output)
		return CONTENT_PAYLOAD_INVALID;
	result = enter(owner, false, error);
	if (result != CONTENT_PAYLOAD_OK)
		return result;
	result = observe(owner, now_ms, error);
	if (result != CONTENT_PAYLOAD_OK)
		goto out;
	if (!candidate || !candidate->count ||
		candidate->count > CONTENT_REPRESENTATIONS_MAX)
	{
		result = policy_error(error, CONTENT_INVALID);
		goto out;
	}
	request = *candidate;
	request.storage = (struct content_storage){0};
	for (size_t i = 0; i < request.count; i++)
	{
		struct content_storage cost;

		if (!region_charge(request.reps[i].bytes, owner->page, &cost) ||
			!add(request.storage.mapped, cost.mapped,
				&request.storage.mapped) ||
			!add(request.storage.locked, cost.locked,
				&request.storage.locked))
		{
			result = policy_error(error, CONTENT_INVALID);
			goto out;
		}
	}
	result = process_access(owner, error);
	if (result != CONTENT_PAYLOAD_OK)
		goto out;
	result = policy_error(error, content_reserve(&owner->policy, &request,
		source_allowed, replace ? &prior : NULL, now_ms, &ref));
	if (result != CONTENT_PAYLOAD_OK)
		goto out;
	for (size_t i = 0; i < owner->policy.limits.items; i++)
		if (same_ref(owner->policy.items[i].ref, ref))
		{
			slot = &owner->slots[i];
			if (slot->item)
			{
				error->policy = CONTENT_BUSY;
				result = poison(owner, error);
				goto out;
			}
			*slot = (struct content_payload_slot){
				.item = &owner->policy.items[i], .ref = ref,
				.count = request.count
			};
			break;
		}
	if (!slot)
	{
		error->policy = CONTENT_STALE;
		result = poison(owner, error);
		goto out;
	}
	for (size_t i = 0; i < slot->count; i++)
	{
		struct content_payload_region *region = &slot->regions[i];

		region->capacity = request.reps[i].bytes;
		region->representation = request.reps[i].id;
		result = prepare_region(owner, region, error);
		if (result != CONTENT_PAYLOAD_OK)
		{
			content_finish(&owner->policy, ref, request.hint, false,
				NULL, 0, now_ms);
			error->cleanup_pending = true;
			if (close_region(owner, region, error) ==
				CONTENT_PAYLOAD_FATAL)
				result = CONTENT_PAYLOAD_FATAL;
			goto out;
		}
	}
	*output = ref;
out:
	return leave(owner, result, error);
}

static enum content_payload_result access_failure(
	struct content_payload_owner *owner, struct content_payload_slot *slot,
	struct content_payload_region *region,
	struct content_payload_error *error)
{
	content_retire(&owner->policy, slot->ref);
	error->cleanup_pending = true;
	return close_region(owner, region, error) == CONTENT_PAYLOAD_FATAL ?
		CONTENT_PAYLOAD_FATAL : CONTENT_PAYLOAD_OS_FAILURE;
}

enum content_payload_result content_payload_write(
	struct content_payload_owner *owner, struct content_ref ref,
	uint32_t representation, enum content_payload_domain domain,
	size_t expected_offset, const void *source, size_t length,
	uint64_t now_ms, struct content_payload_error *error)
{
	struct content_payload_slot *slot;
	struct content_payload_region *region;
	size_t start, capacity, *written;
	enum content_payload_result result;

	if (!error_init(error))
		return CONTENT_PAYLOAD_INVALID;
	result = enter(owner, false, error);
	if (result != CONTENT_PAYLOAD_OK)
		return result;
	result = observe(owner, now_ms, error);
	if (result != CONTENT_PAYLOAD_OK)
		goto out;
	slot = find_slot(owner, ref);
	region = find_region(slot, representation);
	if (!region ||
		!partition(region, domain, &start, &capacity, &written) ||
		length > CONTENT_PAYLOAD_CHUNK_MAX || (!source && length) ||
		expected_offset != *written || expected_offset > capacity ||
		length > capacity - expected_offset)
	{
		result = policy_error(error, region ? CONTENT_INVALID :
			CONTENT_STALE);
		goto out;
	}
	result = process_access(owner, error);
	if (result != CONTENT_PAYLOAD_OK)
		goto out;
	result = policy_error(error, content_producer_check(&owner->policy, ref,
		representation, domain_bytes(domain, expected_offset),
		domain_bytes(domain, length), now_ms));
	if (result != CONTENT_PAYLOAD_OK || !length)
		goto out;
	if (!region->closed)
	{
		error->operation = CONTENT_PAYLOAD_PROTECT;
		result = poison(owner, error);
		goto out;
	}
	region->closed = false;
	if (region_protect(region->data, region->rounded,
		PROT_READ | PROT_WRITE) < 0)
	{
		os_error(error, CONTENT_PAYLOAD_PROTECT);
		result = access_failure(owner, slot, region, error);
		goto out;
	}
	/* No event boundary separates this gate and the bounded copy. */
	result = policy_error(error, content_producer_check(&owner->policy, ref,
		representation, domain_bytes(domain, expected_offset),
		domain_bytes(domain, length), now_ms));
	if (result == CONTENT_PAYLOAD_OK)
	{
		memcpy(region->data + start + expected_offset, source, length);
		*written += length;
	}
	if (close_region(owner, region, error) == CONTENT_PAYLOAD_FATAL)
		result = CONTENT_PAYLOAD_FATAL;
out:
	return leave(owner, result, error);
}

enum content_payload_result content_payload_publish(
	struct content_payload_owner *owner, struct content_ref ref,
	enum content_hint hint, uint64_t now_ms,
	struct content_payload_error *error)
{
	struct content_bytes actual[CONTENT_REPRESENTATIONS_MAX];
	struct content_payload_slot *slot;
	enum content_payload_result result;

	if (!error_init(error))
		return CONTENT_PAYLOAD_INVALID;
	result = enter(owner, false, error);
	if (result != CONTENT_PAYLOAD_OK)
		return result;
	result = observe(owner, now_ms, error);
	if (result != CONTENT_PAYLOAD_OK)
		goto out;
	slot = find_slot(owner, ref);
	if (!slot)
	{
		result = policy_error(error, CONTENT_STALE);
		goto out;
	}
	result = process_access(owner, error);
	if (result != CONTENT_PAYLOAD_OK)
		goto out;
	/* Terminal completion must reach content_finish even when admission or
	 * deadline now refuses publication. A late retired completion only
	 * drains its exact work token; it never inspects unprepared regions. */
	if (slot->item->phase != CONTENT_RESERVED || !slot->item->work)
	{
		result = policy_error(error, content_finish(&owner->policy, ref,
			hint, false, NULL, 0, now_ms));
		goto out;
	}
	for (size_t i = 0; i < slot->count; i++)
	{
		if (!slot->regions[i].closed || !slot->regions[i].locked)
		{
			error->operation = CONTENT_PAYLOAD_PROTECT;
			result = poison(owner, error);
			goto out;
		}
		actual[i] = slot->regions[i].written;
	}
	result = policy_error(error, content_finish(&owner->policy, ref, hint,
		true, actual, slot->count, now_ms));
out:
	return leave(owner, result, error);
}

enum content_payload_result content_payload_with_read(
	struct content_payload_owner *owner, struct content_ref ref,
	uint32_t representation, enum content_payload_domain domain,
	size_t offset, size_t length, content_payload_consumer consumer,
	void *context, uint64_t now_ms, struct content_payload_error *error)
{
	struct content_payload_slot *slot;
	struct content_payload_region *region;
	struct content_lease lease = {0};
	size_t start, capacity, *written;
	enum content_payload_result result;

	if (!error_init(error))
		return CONTENT_PAYLOAD_INVALID;
	result = enter(owner, false, error);
	if (result != CONTENT_PAYLOAD_OK)
		return result;
	result = observe(owner, now_ms, error);
	if (result != CONTENT_PAYLOAD_OK)
		goto out;
	slot = find_slot(owner, ref);
	region = find_region(slot, representation);
	if (!consumer || !region ||
		!partition(region, domain, &start, &capacity, &written) ||
		length > CONTENT_PAYLOAD_CHUNK_MAX || offset > *written ||
		length > *written - offset)
	{
		result = policy_error(error, region ? CONTENT_INVALID :
			CONTENT_STALE);
		goto out;
	}
	result = process_access(owner, error);
	if (result != CONTENT_PAYLOAD_OK)
		goto out;
	result = policy_error(error, content_acquire(&owner->policy, ref,
		representation, now_ms, &lease));
	if (result != CONTENT_PAYLOAD_OK)
		goto out;
	if (length)
	{
		if (!region->closed)
		{
			error->operation = CONTENT_PAYLOAD_PROTECT;
			result = poison(owner, error);
			goto release;
		}
		region->closed = false;
		if (region_protect(region->data, region->rounded,
			PROT_READ) < 0)
		{
			os_error(error, CONTENT_PAYLOAD_PROTECT);
			result = access_failure(owner, slot, region, error);
			goto release;
		}
	}
	result = policy_error(error, content_lease_check(&owner->policy,
		lease, now_ms));
	if (result == CONTENT_PAYLOAD_OK &&
		!consumer(length ? region->data + start + offset : NULL,
			length, context))
		result = CONTENT_PAYLOAD_CONSUMER_FAILED;
	if (close_region(owner, region, error) == CONTENT_PAYLOAD_FATAL)
		result = CONTENT_PAYLOAD_FATAL;
release:
	if (content_release(&owner->policy, lease) != CONTENT_OK)
	{
		error->policy = CONTENT_STALE;
		result = poison(owner, error);
	}
out:
	return leave(owner, result, error);
}

enum content_payload_result content_payload_retire(
	struct content_payload_owner *owner, struct content_ref ref,
	struct content_payload_error *error)
{
	enum content_payload_result result;

	if (!error_init(error))
		return CONTENT_PAYLOAD_INVALID;
	result = enter(owner, true, error);
	if (result != CONTENT_PAYLOAD_OK)
		return result;
	result = policy_error(error, content_retire(&owner->policy, ref));
	return leave(owner, result, error);
}

enum content_payload_result content_payload_clear(
	struct content_payload_owner *owner,
	struct content_payload_error *error)
{
	enum content_payload_result result;

	if (!error_init(error))
		return CONTENT_PAYLOAD_INVALID;
	result = enter(owner, true, error);
	if (result != CONTENT_PAYLOAD_OK)
		return result;
	result = policy_error(error, content_clear(&owner->policy));
	return leave(owner, result, error);
}

enum content_payload_result content_payload_expire(
	struct content_payload_owner *owner, uint64_t now_ms,
	struct content_payload_error *error)
{
	enum content_payload_result result;

	if (!error_init(error))
		return CONTENT_PAYLOAD_INVALID;
	result = enter(owner, true, error);
	if (result != CONTENT_PAYLOAD_OK)
		return result;
	result = policy_error(error, content_expire(&owner->policy, now_ms));
	return leave(owner, result, error);
}

static enum content_payload_result clean_region(
	struct content_payload_owner *owner,
	struct content_payload_region *region,
	struct content_payload_error *error)
{
	enum content_payload_result result = CONTENT_PAYLOAD_OK;

	if (region->wiped < region->rounded)
	{
		size_t amount = region->rounded - region->wiped;

		if (amount > CONTENT_PAYLOAD_CHUNK_MAX)
			amount = CONTENT_PAYLOAD_CHUNK_MAX;
		region->closed = false;
		if (region_protect(region->data, region->rounded,
			PROT_READ | PROT_WRITE) < 0)
		{
			os_error(error, CONTENT_PAYLOAD_PROTECT);
			return poison(owner, error);
		}
#if defined(__linux__) || defined(__FreeBSD__) || defined(__OpenBSD__)
		explicit_bzero(region->data + region->wiped, amount);
#else
		error->operation = CONTENT_PAYLOAD_PROTECT;
		return poison(owner, error);
#endif
		region->wiped += amount;
		if (close_region(owner, region, error) == CONTENT_PAYLOAD_FATAL)
			return CONTENT_PAYLOAD_FATAL;
		if (region->wiped < region->rounded)
			return CONTENT_PAYLOAD_OK;
	}
	if (region->locked)
	{
		if (region_unlock(region->data, region->rounded) < 0)
			result = os_error(error, CONTENT_PAYLOAD_UNLOCK);
		else
			region->locked = false;
	}
	if (region_unmap(region->mapping, region->mapped) < 0)
		return os_error(error, CONTENT_PAYLOAD_UNMAP);
	/* Successful unmap also ends the remaining lock obligation. */
	region->mapping = NULL;
	region->data = NULL;
	region->locked = false;
	region->closed = true;
	return result;
}

enum content_payload_result content_payload_cleanup_step(
	struct content_payload_owner *owner, bool *progress,
	struct content_payload_error *error)
{
	enum content_payload_result result;

	if (progress)
		*progress = false;
	if (!error_init(error) || !progress)
		return CONTENT_PAYLOAD_INVALID;
	result = enter(owner, true, error);
	if (result != CONTENT_PAYLOAD_OK)
		return result;
	for (size_t scanned = 0; scanned < owner->policy.limits.items;
		scanned++)
	{
		size_t index = owner->cleanup_cursor++;
		struct content_payload_slot *slot;
		struct content_payload_region *region = NULL;

		if (owner->cleanup_cursor == owner->policy.limits.items)
			owner->cleanup_cursor = 0;
		slot = &owner->slots[index];
		if (!slot->item || slot->item->phase != CONTENT_RETIRED)
			continue;
		if (slot->item->work)
		{
			/* Producer work has ended here. Retired
			 * completion drains once without reviving the ref. */
			content_finish(&owner->policy, slot->ref,
				slot->item->descriptor.hint, false, NULL, 0,
				owner->policy.last_ms);
			*progress = true;
		}
		if (slot->item->work || slot->item->leases)
			continue;
		for (size_t i = 0; i < slot->count; i++)
			if (slot->regions[i].mapping)
			{
				region = &slot->regions[i];
				break;
			}
		if (region)
		{
			result = clean_region(owner, region, error);
			*progress = true;
			if (result != CONTENT_PAYLOAD_OK || region->mapping)
				goto out;
		}
		for (size_t i = 0; i < slot->count; i++)
			if (slot->regions[i].mapping)
				goto out;
		result = policy_error(error,
			content_cleanup_ack(&owner->policy, slot->ref));
		if (result == CONTENT_PAYLOAD_OK)
		{
			*slot = (struct content_payload_slot){0};
			*progress = true;
		}
		goto out;
	}
out:
	return leave(owner, result, error);
}
