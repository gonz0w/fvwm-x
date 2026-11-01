/* This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */
#include "config.h"
#include "libs/content_policy.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/resource.h>
#include <time.h>
#include <unistd.h>
#ifdef CONTENT_PROBE_SODIUM
#include <sodium.h>
#define CANDIDATE "sodium-1.0.22"
#define MAPPED_EXTRA 4
#define LOCKED_EXTRA 1
#else
#define CANDIDATE "base-os"
#define MAPPED_EXTRA 2
#define LOCKED_EXTRA 0
#endif

/* Non-installed synthetic comparison, never a production allocator. It emits
 * bounded numeric observations only. Configured native support/dump evidence,
 * library allocator model and dependency closure still need qualification. */
enum preparation {
	PREPARED, ALLOCATION_REFUSED, LOCK_REFUSED, DUMP_UNAVAILABLE,
	DUMP_REFUSED, ACCESS_REFUSED, WIPE_UNAVAILABLE
};
enum fault { FAULT_NONE, FAULT_ALLOCATION, FAULT_LOCK, FAULT_DUMP,
	FAULT_ACCESS };
struct buffer {
	unsigned char *data;
	void *mapping;
	size_t rounded, mapped;
	bool locked;
};

static uint64_t nanoseconds(void)
{
	struct timespec value;

	if (clock_gettime(CLOCK_MONOTONIC, &value) < 0 || value.tv_sec < 0 ||
		value.tv_nsec < 0 || value.tv_nsec >= 1000000000L ||
		(uint64_t)value.tv_sec >
		(UINT64_MAX - (uint64_t)value.tv_nsec) / 1000000000ULL)
		return 0;
	return (uint64_t)value.tv_sec * 1000000000ULL + value.tv_nsec;
}

static bool no_core(void)
{
	struct rlimit limit = {0, 0}, applied;

	return setrlimit(RLIMIT_CORE, &limit) == 0 &&
		getrlimit(RLIMIT_CORE, &applied) == 0 &&
		applied.rlim_cur == 0 && applied.rlim_max == 0;
}

static int access_mode(struct buffer *buffer, int mode)
{
#ifdef CONTENT_PROBE_SODIUM
	if (mode == PROT_NONE)
		return sodium_mprotect_noaccess(buffer->data);
	if (mode == PROT_READ)
		return sodium_mprotect_readonly(buffer->data);
	return sodium_mprotect_readwrite(buffer->data);
#else
	return mprotect(buffer->data, buffer->rounded, mode);
#endif
}

static enum preparation prepare(struct buffer *buffer, size_t bytes,
	size_t page, enum fault fault)
{
	struct content_storage cost;

	*buffer = (struct buffer){0};
	if (!content_storage_charge(bytes, page, MAPPED_EXTRA, LOCKED_EXTRA,
		&cost))
		return ALLOCATION_REFUSED;
	buffer->rounded = cost.locked - LOCKED_EXTRA * page;
	buffer->mapped = cost.mapped;
	if (fault == FAULT_ALLOCATION)
		return ALLOCATION_REFUSED;
#ifdef CONTENT_PROBE_SODIUM
	buffer->data = sodium_malloc(buffer->rounded);
	if (!buffer->data)
		return ALLOCATION_REFUSED;
	/* The guarded allocation's lock attempt is not a success guarantee. */
	if ((uintptr_t)buffer->data % page != 0)
		return ACCESS_REFUSED;
#else
#if !defined(__linux__) && !defined(__FreeBSD__) && !defined(__OpenBSD__)
	/* No hand-written wipe on an unqualified base-system profile. */
	return WIPE_UNAVAILABLE;
#else
	int flags = MAP_PRIVATE | MAP_ANON;

#if defined(MAP_CONCEAL)
	flags |= MAP_CONCEAL;
#elif defined(MAP_NOCORE)
	flags |= MAP_NOCORE;
#endif
	buffer->mapping = mmap(NULL, buffer->mapped, PROT_NONE, flags, -1, 0);
	if (buffer->mapping == MAP_FAILED)
	{
		buffer->mapping = NULL;
		return ALLOCATION_REFUSED;
	}
	buffer->data = (unsigned char *)buffer->mapping + page;
#endif
#endif
	if (fault == FAULT_ACCESS ||
		access_mode(buffer, PROT_READ | PROT_WRITE) < 0)
		return ACCESS_REFUSED;
	if (fault == FAULT_DUMP)
		return DUMP_REFUSED;
#if defined(MADV_DONTDUMP)
	if (madvise(buffer->data, buffer->rounded, MADV_DONTDUMP) < 0)
		return DUMP_REFUSED;
#elif defined(MADV_NOCORE)
	if (madvise(buffer->data, buffer->rounded, MADV_NOCORE) < 0)
		return DUMP_REFUSED;
#elif !defined(CONTENT_PROBE_SODIUM) && \
	(defined(MAP_CONCEAL) || defined(MAP_NOCORE))
	/* This probe itself applied the checked mapping flag. */
#else
	/* Do not infer the dependency's configured mmap path from an OS macro.
	 * Its mapping contract needs exact package attestation. */
	return DUMP_UNAVAILABLE;
#endif
	if (fault == FAULT_LOCK)
		return LOCK_REFUSED;
#ifdef CONTENT_PROBE_SODIUM
	if (sodium_mlock(buffer->data, buffer->rounded) < 0)
#else
	if (mlock(buffer->data, buffer->rounded) < 0)
#endif
		return LOCK_REFUSED;
	buffer->locked = true;
	if (access_mode(buffer, PROT_NONE) < 0)
		return ACCESS_REFUSED;
	return PREPARED;
}

static bool cleanup(struct buffer *buffer, bool *wipe_observed)
{
	bool okay = true;

	*wipe_observed = false;
	if (!buffer->data)
		return true;
	if (access_mode(buffer, PROT_READ | PROT_WRITE) < 0)
		return false;
#ifdef CONTENT_PROBE_SODIUM
	sodium_memzero(buffer->data, buffer->rounded);
#elif defined(__linux__) || defined(__FreeBSD__) || defined(__OpenBSD__)
	explicit_bzero(buffer->data, buffer->rounded);
#else
	return false;
#endif
	for (size_t i = 0; i < buffer->rounded; i++)
		if (((volatile unsigned char *)buffer->data)[i] != 0)
			return false;
	*wipe_observed = true;
#ifdef CONTENT_PROBE_SODIUM
	/* Its void free reports no munmap/munlock status. The observation here
	 * is wipe-before-free and return, not physical reclamation. */
	sodium_free(buffer->data);
#else
	if (buffer->locked && munlock(buffer->data, buffer->rounded) < 0)
		okay = false;
	if (munmap(buffer->mapping, buffer->mapped) < 0)
		return false;
#endif
	*buffer = (struct buffer){0};
	return okay;
}

static bool exercise_access(struct content_policy *policy,
	struct content_lease lease, struct buffer *buffer, size_t bytes)
{
	if (content_lease_check(policy, lease, 13) != CONTENT_OK ||
		access_mode(buffer, PROT_READ | PROT_WRITE) < 0)
		return false;
	/* Public synthetic fixture, never real clipboard input. */
	memset(buffer->data, 0x5a, bytes);
	if (access_mode(buffer, PROT_NONE) < 0 ||
		content_lease_check(policy, lease, 14) != CONTENT_OK ||
		access_mode(buffer, PROT_READ) < 0)
		return false;
	for (size_t i = 0; i < bytes; i++)
		if (((volatile unsigned char *)buffer->data)[i] != 0x5a)
			return false;
	return access_mode(buffer, PROT_NONE) == 0;
}

static bool measure(size_t bytes, size_t page, enum fault fault,
	unsigned run, unsigned row)
{
	struct content_policy policy;
	struct content_item items[2];
	struct content_lease leases[2], lease;
	struct content_ref ref;
	struct buffer buffer;
	struct content_storage charge;
	struct content_limits limits = {
		.items = 2, .representations = 2, .per_item = 1,
		.leases = 2, .work = 2,
		.total = {2 * 1024 * 1024, 0, 0},
		.per_representation = {1024 * 1024, 0, 0},
		.storage = {8 * 1024 * 1024, 8 * 1024 * 1024},
		.per_item_storage = {4 * 1024 * 1024, 4 * 1024 * 1024},
		.lifetime_ms = 1000
	};
	struct content_candidate candidate = {
		.hint = CONTENT_HINT_ABSENT, .deadline_ms = 1000, .count = 1,
		.reps = {{1, CONTENT_TEXT, {0, 0, 0}}}
	};
	uint64_t started = nanoseconds(), prepared, finished;
	enum preparation status;
	bool accessed = false, wiped = false, released, okay;
	bool lifecycle = true, acquired = false;
	int saved_errno;

	if (!started || !content_storage_charge(bytes, page, MAPPED_EXTRA,
		LOCKED_EXTRA, &charge))
		return false;
	candidate.storage = charge;
	candidate.reps[0].bytes.encoded = bytes;
	if (content_policy_init(&policy, row + 1, &limits, items, leases, 10)
		!= CONTENT_OK)
		return false;
	policy.enabled = true;
	if (content_reserve(&policy, &candidate, true, NULL, 10, &ref)
		!= CONTENT_OK)
		return false;
	errno = 0;
	status = prepare(&buffer, bytes, page, fault);
	saved_errno = errno;
	prepared = nanoseconds();
	if (status == PREPARED)
	{
		if (content_finish(&policy, ref, candidate.hint, true, 11)
			!= CONTENT_OK)
			lifecycle = false;
		else
			acquired = content_acquire(&policy, ref, 1, 12, &lease)
				== CONTENT_OK;
		if (acquired)
			accessed = exercise_access(
				&policy, lease, &buffer, bytes);
		if (content_retire(&policy, ref) != CONTENT_OK)
			lifecycle = false;
		if (acquired)
		{
			if (content_lease_check(&policy, lease, 15)
				!= CONTENT_STALE ||
				content_cleanup_ack(&policy, ref) !=
				CONTENT_BUSY)
				lifecycle = false;
			if (content_release(&policy, lease) != CONTENT_OK)
				lifecycle = false;
		}
	}
	else if (content_finish(&policy, ref, candidate.hint, false, 11)
		!= CONTENT_OK)
		lifecycle = false;
	released = cleanup(&buffer, &wiped);
	okay = lifecycle && released && (status != PREPARED || accessed);
	if (okay)
		okay = content_cleanup_ack(&policy, ref) == CONTENT_OK &&
			policy.usage.items == 0 && policy.usage.work == 0 &&
			policy.usage.leases == 0 &&
			policy.usage.storage.mapped == 0 &&
			policy.usage.storage.locked == 0;
	finished = nanoseconds();
	if (!prepared || !finished || prepared < started || finished < prepared)
		return false;
	printf("{\"candidate\":\"%s\",\"run\":%u,\"owner_epoch\":%u,"
		"\"bytes\":%zu,"
		"\"page\":%zu,\"mapped_model\":%zu,\"locked_model\":%zu,"
		"\"preparation\":%d,\"errno\":%d,\"injected\":%d,"
		"\"accessed\":%d,\"wipe_observed\":%d,\"cleanup\":%d,"
		"\"accounting\":%d,\"prepare_ns\":%llu,\"finish_ns\":%llu}\n",
		CANDIDATE, run, row + 1, bytes, page, charge.mapped, charge.locked,
		status, saved_errno, fault, accessed, wiped, released, okay,
		(unsigned long long)(prepared - started),
		(unsigned long long)(finished - prepared));
	return okay;
}

int main(int argc, char **argv)
{
	unsigned repeats = 1;
	enum fault fault = FAULT_NONE;
	bool zero_lock = false;
	long native_page;
	struct rusage usage;
	uint64_t started = nanoseconds(), initialized;

	for (int i = 1; i < argc; i++)
	{
		if (!strcmp(argv[i], "--zero-lock-limit"))
			zero_lock = true;
		else if (!strcmp(argv[i], "--repeat") && i + 1 < argc)
		{
			char *end;
			unsigned long value;

			errno = 0;
			value = strtoul(argv[++i], &end, 10);
			if (errno || !*argv[i] || *end ||
				value < 1 || value > 20)
				return 2;
			repeats = (unsigned)value;
		}
		else if (!strcmp(argv[i], "--fail-allocation"))
			fault = FAULT_ALLOCATION;
		else if (!strcmp(argv[i], "--fail-lock"))
			fault = FAULT_LOCK;
		else if (!strcmp(argv[i], "--fail-dump"))
			fault = FAULT_DUMP;
		else if (!strcmp(argv[i], "--fail-access"))
			fault = FAULT_ACCESS;
		else
			return 2;
	}
	if (!started || !no_core())
		return 77;
	if (zero_lock)
	{
		struct rlimit limit = {0, 0}, applied;

		if (setrlimit(RLIMIT_MEMLOCK, &limit) < 0 ||
			getrlimit(RLIMIT_MEMLOCK, &applied) < 0 ||
			applied.rlim_cur != 0 || applied.rlim_max != 0)
			return 77;
	}
#ifdef CONTENT_PROBE_SODIUM
	if (strcmp(sodium_version_string(), "1.0.22") || sodium_init() < 0)
		return 77;
#endif
	native_page = sysconf(_SC_PAGESIZE);
	initialized = nanoseconds();
	if (native_page <= 0 || native_page > 65536 ||
		!initialized || initialized < started)
		return 77;
	size_t page = (size_t)native_page;
	size_t sizes[] = {0, 1, page - 1, page, page + 1, 65536};
	unsigned row = 0;

	for (unsigned run = 0; run < repeats; run++)
		for (size_t i = 0; i < sizeof(sizes) / sizeof(sizes[0]); i++)
			if (!measure(sizes[i], page, fault, run, row++))
				return 1;
	if (getrusage(RUSAGE_SELF, &usage) < 0)
		return 1;
	printf("{\"candidate\":\"%s\",\"summary\":true,"
		"\"initialization_ns\":%llu,\"peak_rss_raw\":%ld,"
		"\"user_us\":%llu,\"system_us\":%llu}\n", CANDIDATE,
		(unsigned long long)(initialized - started), usage.ru_maxrss,
		(unsigned long long)usage.ru_utime.tv_sec * 1000000ULL +
			usage.ru_utime.tv_usec,
		(unsigned long long)usage.ru_stime.tv_sec * 1000000ULL +
			usage.ru_stime.tv_usec);
	return 0;
}
