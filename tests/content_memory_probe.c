/* This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */
#include "config.h"
#include "libs/content_policy.h"
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/resource.h>
#include <time.h>
#include <unistd.h>
#ifdef __linux__
#include <sys/prctl.h>
#endif
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

#if defined(CONTENT_PROBE_SODIUM_MMAP_NOCORE) && \
	(!defined(CONTENT_PROBE_SODIUM) || !defined(__OpenBSD__) || \
	 !defined(MAP_CONCEAL) || !defined(MAP_ANON) || \
	 CONTENT_PROBE_SODIUM_MMAP_NOCORE != 1 || MAP_CONCEAL == 0)
#error Configured sodium mmap attestation requires the exact OpenBSD profile
#endif

/* Non-installed synthetic comparison, never a production allocator. It emits
 * bounded numeric observations only. Configured native support/dump evidence,
 * library allocator model and dependency closure still need qualification. */
enum preparation {
	PREPARED, ALLOCATION_REFUSED, LOCK_REFUSED, DUMP_UNAVAILABLE,
	DUMP_REFUSED, ACCESS_REFUSED, WIPE_UNAVAILABLE
};
enum fault { FAULT_NONE, FAULT_ALLOCATION, FAULT_LOCK, FAULT_DUMP,
	FAULT_ACCESS, FAULT_CANCEL };
enum access_control {
	CONTROL_NONE, CONTROL_RW, CONTROL_NOACCESS, CONTROL_READONLY
};
struct trial_options {
	enum fault fault;
	enum access_control access;
	bool zero_lock;
	int dumpguard;
	struct rlimit memlock;
};
struct trial_result {
	bool prepared, lock_refused;
};
struct buffer {
	unsigned char *data;
	void *mapping;
	size_t rounded, mapped;
	bool locked, lock_attempted;
	int lock_errno, region_kind;
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

/* Only this disposable probe changes its own dump policy. Linux core pipes
 * can ignore RLIMIT_CORE, so they additionally require nondumpability. BSD
 * limits still need the owning profile's collector preflight. Neither is a
 * measurement of regional exclusion or protection from privileged inspection.
 */
static int prepare_dump_guard(void)
{
	struct rlimit limit = {0, 0}, applied;

	if (setrlimit(RLIMIT_CORE, &limit) < 0 ||
		getrlimit(RLIMIT_CORE, &applied) < 0 ||
		applied.rlim_cur != 0 || applied.rlim_max != 0)
		return 0;
#ifdef __linux__
	if (prctl(PR_SET_DUMPABLE, 0L, 0L, 0L, 0L) < 0 ||
		prctl(PR_GET_DUMPABLE, 0L, 0L, 0L, 0L) != 0)
		return 0;
	return 2;
#elif defined(__FreeBSD__) || defined(__OpenBSD__)
	return 1;
#else
	return 0;
#endif
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
	buffer->region_kind = 1;
#elif defined(MADV_NOCORE)
	if (madvise(buffer->data, buffer->rounded, MADV_NOCORE) < 0)
		return DUMP_REFUSED;
	buffer->region_kind = 1;
#elif !defined(CONTENT_PROBE_SODIUM) && \
	(defined(MAP_CONCEAL) || defined(MAP_NOCORE))
	/* This probe itself applied the checked mapping flag. */
	buffer->region_kind = 2;
#elif defined(CONTENT_PROBE_SODIUM_MMAP_NOCORE)
	/* The owning build must bind exact effective utility macros/object and
	 * static link input. This is configured-source evidence, not a dump
	 * observation or evidence of ignored internal protection-call results. */
	buffer->region_kind = 3;
#else
	/* Do not infer the dependency's configured mmap path from an OS macro.
	 * Its mapping contract needs exact package attestation. */
	return DUMP_UNAVAILABLE;
#endif
	if (fault == FAULT_LOCK)
		return LOCK_REFUSED;
	buffer->lock_attempted = true;
	errno = 0;
#ifdef CONTENT_PROBE_SODIUM
	if (sodium_mlock(buffer->data, buffer->rounded) < 0)
#else
	if (mlock(buffer->data, buffer->rounded) < 0)
#endif
	{
		buffer->lock_errno = errno;
		return LOCK_REFUSED;
	}
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

static bool produce(struct content_policy *policy, struct content_ref ref,
	struct buffer *buffer, size_t capacity, bool cancel, size_t *written)
{
	size_t wanted = capacity ? capacity - 1 : 0;

	*written = 0;
	if (access_mode(buffer, PROT_READ | PROT_WRITE) < 0)
		return false;
	do
	{
		size_t chunk = wanted - *written;

		if (chunk > 4096)
			chunk = 4096;
		if (content_producer_check(policy, ref, 1,
			(struct content_bytes){*written, 0, 0},
			(struct content_bytes){chunk, 0, 0}, 11) != CONTENT_OK)
			return false;
		/* Public synthetic bytes, with a contiguous initialized prefix. */
		memset(buffer->data + *written, 0x5a, chunk);
		*written += chunk;
		if (cancel)
		{
			if (content_retire(policy, ref) != CONTENT_OK ||
				content_producer_check(policy, ref, 1,
					(struct content_bytes){*written, 0, 0},
					(struct content_bytes){0}, 11) != CONTENT_STALE)
				return false;
			break;
		}
	} while (*written < wanted);
	/* Publication never overlaps this probe's writable access. */
	return access_mode(buffer, PROT_NONE) == 0;
}

static bool consume(struct content_policy *policy, struct content_ref ref,
	struct content_lease lease, struct buffer *buffer, size_t written)
{
	struct content_candidate view;

	if (content_lease_check(policy, lease, 14) != CONTENT_OK ||
		content_describe(policy, ref, 14, &view) != CONTENT_OK ||
		view.count != 1 || view.reps[0].bytes.encoded != written ||
		access_mode(buffer, PROT_READ) < 0)
		return false;
	for (size_t i = 0; i < view.reps[0].bytes.encoded; i++)
		if (((volatile unsigned char *)buffer->data)[i] != 0x5a)
			return false;
	return access_mode(buffer, PROT_NONE) == 0;
}

/* This probe never changes IDs/capabilities. A caller supplies an existing
 * unprivileged profile; root or unavailable Linux capability evidence refuses
 * the strict lock-negative experiment rather than pretending it passed. */
static bool unprivileged_lock_profile(void)
{
	if (getuid() == 0 || geteuid() == 0)
		return false;
#ifdef __linux__
	FILE *status = fopen("/proc/self/status", "r");
	char line[256];
	bool observed = false, allowed = false, read_failed;
	int close_status;
	size_t total = 0;

	if (!status)
		return false;
	while (fgets(line, sizeof(line), status))
	{
		total += strlen(line);
		if (total > 65536)
			break;
		if (!strncmp(line, "CapEff:", 7))
		{
			char *end;
			unsigned long long value;

			errno = 0;
			value = strtoull(line + 7, &end, 16);
			while (*end == ' ' || *end == '\t')
				end++;
			observed = !errno && end != line + 7 &&
				(*end == '\n' || *end == '\0');
			allowed = observed && !(value & (1ULL << 14));
			break;
		}
	}
	read_failed = ferror(status) != 0;
	close_status = fclose(status);

	if (read_failed || close_status == EOF)
		return false;
	return observed && allowed;
#else
	return true;
#endif
}

/* Deliberately test OS permissions with public bytes, outside any production
 * writer contract. This process never forks. The separately owned direct-child
 * supervisor records readiness and the actual terminal signal. */
static bool enforce_access(struct content_policy *policy,
	struct content_lease lease, struct buffer *buffer,
	const struct trial_options *options)
{
	enum access_control control = options->access;
	struct sigaction action = {0};
	sigset_t signals;
	bool unprivileged = unprivileged_lock_profile();
	uid_t uid = getuid(), euid = geteuid();
	int mode = control == CONTROL_RW ? PROT_READ | PROT_WRITE :
		control == CONTROL_READONLY ? PROT_READ : PROT_NONE;

	action.sa_handler = SIG_DFL;
	if (sigemptyset(&action.sa_mask) < 0 ||
		sigemptyset(&signals) < 0 ||
		sigaddset(&signals, SIGSEGV) < 0 ||
		sigaddset(&signals, SIGBUS) < 0 ||
		sigaction(SIGSEGV, &action, NULL) < 0 ||
		sigaction(SIGBUS, &action, NULL) < 0 ||
		sigprocmask(SIG_UNBLOCK, &signals, NULL) < 0 ||
		content_lease_check(policy, lease, 14) != CONTENT_OK ||
		access_mode(buffer, mode) < 0)
		return false;
	if (printf("{\"candidate\":\"%s\",\"ready\":true,"
		"\"control\":%d,\"sigsegv\":%d,\"sigbus\":%d,"
		"\"dumpguard\":%d,\"region_kind\":%d,\"page\":%zu,"
		"\"uid\":%llu,\"euid\":%llu,"
		"\"memlock_soft\":%llu,\"memlock_hard\":%llu,"
		"\"unprivileged_lock_profile\":%s}\n", CANDIDATE, control,
		SIGSEGV, SIGBUS, options->dumpguard, buffer->region_kind,
		buffer->rounded,
		(unsigned long long)uid,
		(unsigned long long)euid,
		(unsigned long long)options->memlock.rlim_cur,
		(unsigned long long)options->memlock.rlim_max,
		unprivileged ? "true" : "false") < 0 ||
		fflush(stdout) == EOF)
		return false;
	*(volatile unsigned char *)buffer->data = 0x6b;
	/* A write in either protected mode is a failed control, never success.
	 * Expected-fault children do not reach cleanup or acknowledge it. */
	return control == CONTROL_RW && access_mode(buffer, PROT_NONE) == 0;
}

static bool measure(size_t bytes, size_t page,
	const struct trial_options *options, unsigned run, unsigned row,
	struct trial_result *observation)
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
	bool lifecycle = true, acquired = false, produced = false;
	size_t written = 0;
	int saved_errno, lock_errno, region_kind;
	bool lock_attempted;
	enum fault fault = options->fault;

	*observation = (struct trial_result){0};

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
	if (content_acquire(&policy, ref, 1, 10, &lease) != CONTENT_STALE ||
		lease.id != 0)
		return false;
	errno = 0;
	status = prepare(&buffer, bytes, page, fault);
	saved_errno = errno;
	lock_attempted = buffer.lock_attempted;
	lock_errno = buffer.lock_errno;
	region_kind = buffer.region_kind;
	observation->prepared = status == PREPARED;
	observation->lock_refused = status == LOCK_REFUSED &&
		lock_attempted && lock_errno != 0;
	prepared = nanoseconds();
	if (status == PREPARED && !options->zero_lock)
	{
		struct content_bytes actual;
		enum content_result completed;

		produced = produce(&policy, ref, &buffer, bytes,
			fault == FAULT_CANCEL, &written);
		actual = (struct content_bytes){written, 0, 0};
		completed = content_finish(&policy, ref, candidate.hint,
			produced, &actual, 1, 12);
		if (fault == FAULT_CANCEL)
		{
			if (completed != CONTENT_STALE ||
				content_acquire(&policy, ref, 1, 13, &lease)
					!= CONTENT_STALE ||
				content_finish(&policy, ref, candidate.hint,
					true, &actual, 1, 13) != CONTENT_STALE)
				lifecycle = false;
			accessed = produced;
		}
		else if (!produced || completed != CONTENT_OK)
			lifecycle = false;
		else
			acquired = content_acquire(&policy, ref, 1, 13, &lease)
				== CONTENT_OK;
		if (acquired)
		{
			accessed = consume(&policy, ref, lease, &buffer, written);
			if (accessed && options->access != CONTROL_NONE)
				accessed = enforce_access(&policy, lease, &buffer,
					options);
		}
		if (content_retire(&policy, ref) != CONTENT_OK)
			lifecycle = false;
		if (acquired)
		{
			if (content_lease_check(&policy, lease, 15)
				!= CONTENT_STALE ||
				content_cleanup_ack(&policy, ref) != CONTENT_BUSY)
				lifecycle = false;
			if (content_release(&policy, lease) != CONTENT_OK)
				lifecycle = false;
		}
	}
	else if (content_finish(&policy, ref, candidate.hint, false,
		NULL, 0, 11) != CONTENT_OK)
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
		"\"bytes\":%zu,\"written_bytes\":%zu,"
		"\"page\":%zu,\"mapped_model\":%zu,\"locked_model\":%zu,"
		"\"preparation\":%d,\"errno\":%d,\"injected\":%d,"
		"\"lock_attempted\":%d,\"lock_errno\":%d,\"region_kind\":%d,"
		"\"producer_closed\":%d,\"reader_acquired\":%d,"
		"\"accessed\":%d,\"wipe_observed\":%d,\"cleanup\":%d,"
		"\"accounting\":%d,\"prepare_ns\":%llu,\"finish_ns\":%llu}\n",
		CANDIDATE, run, row + 1, bytes, written, page, charge.mapped, charge.locked,
		status, saved_errno, fault, lock_attempted, lock_errno, region_kind,
		produced, acquired, accessed, wiped, released, okay,
		(unsigned long long)(prepared - started),
		(unsigned long long)(finished - prepared));
	return okay;
}

int main(int argc, char **argv)
{
	unsigned repeats = 1;
	struct trial_options options = {0};
	unsigned fault_options = 0;
	bool lock_observed = true;
	long native_page;
	struct rusage usage;
	uint64_t started = nanoseconds(), initialized;

	for (int i = 1; i < argc; i++)
	{
		if (!strcmp(argv[i], "--zero-lock-limit"))
			options.zero_lock = true;
		else if (!strcmp(argv[i], "--enforce") && i + 1 < argc)
		{
			if (options.access != CONTROL_NONE)
				return 2;
			i++;
			if (!strcmp(argv[i], "rw-control"))
				options.access = CONTROL_RW;
			else if (!strcmp(argv[i], "noaccess-write"))
				options.access = CONTROL_NOACCESS;
			else if (!strcmp(argv[i], "readonly-write"))
				options.access = CONTROL_READONLY;
			else
				return 2;
		}
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
		{
			options.fault = FAULT_ALLOCATION;
			fault_options++;
		}
		else if (!strcmp(argv[i], "--fail-lock"))
		{
			options.fault = FAULT_LOCK;
			fault_options++;
		}
		else if (!strcmp(argv[i], "--fail-dump"))
		{
			options.fault = FAULT_DUMP;
			fault_options++;
		}
		else if (!strcmp(argv[i], "--cancel-producer"))
		{
			options.fault = FAULT_CANCEL;
			fault_options++;
		}
		else if (!strcmp(argv[i], "--fail-access"))
		{
			options.fault = FAULT_ACCESS;
			fault_options++;
		}
		else
			return 2;
	}
	if (fault_options > 1 ||
		(options.zero_lock && options.fault != FAULT_NONE) ||
		(options.access != CONTROL_NONE &&
			(options.zero_lock || fault_options || repeats != 1)))
		return 2;
	if (!started)
		return 77;
	options.dumpguard = prepare_dump_guard();
	if (!options.dumpguard ||
		getrlimit(RLIMIT_MEMLOCK, &options.memlock) < 0)
		return 77;
	if (options.zero_lock)
	{
		struct rlimit limit = {0, 0}, applied;

		if (!unprivileged_lock_profile())
			return 77;
		if (setrlimit(RLIMIT_MEMLOCK, &limit) < 0 ||
			getrlimit(RLIMIT_MEMLOCK, &applied) < 0 ||
			applied.rlim_cur != 0 || applied.rlim_max != 0)
			return 77;
		options.memlock = applied;
#ifdef __linux__
		const int capabilities_checked = 1;
#else
		const int capabilities_checked = 0;
#endif
		if (printf("{\"candidate\":\"%s\",\"lock_profile\":true,"
			"\"uid\":%llu,\"euid\":%llu,\"lock_limit_zero\":true,"
			"\"capabilities_checked\":%d,\"ipc_lock_capability\":false,"
			"\"dumpguard\":%d}\n",
			CANDIDATE, (unsigned long long)getuid(),
			(unsigned long long)geteuid(), capabilities_checked,
			options.dumpguard) < 0 ||
			fflush(stdout) == EOF)
			return 1;
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
	struct trial_result observation;

	if (options.access != CONTROL_NONE)
	{
		if (!measure(page, page, &options, 0, 0, &observation))
			return 1;
		return observation.prepared ? 0 : 77;
	}

	for (unsigned run = 0; run < repeats; run++)
		for (size_t i = 0; i < sizeof(sizes) / sizeof(sizes[0]); i++)
		{
			if (!measure(sizes[i], page, &options, run, row++,
				&observation))
				return 1;
			lock_observed = lock_observed && observation.lock_refused;
		}
	if (getrusage(RUSAGE_SELF, &usage) < 0)
		return 1;
	printf("{\"candidate\":\"%s\",\"summary\":true,"
		"\"dumpguard\":%d,\"initialization_ns\":%llu,\"peak_rss_raw\":%ld,"
		"\"user_us\":%llu,\"system_us\":%llu,"
		"\"memlock_soft\":%llu,\"memlock_hard\":%llu}\n", CANDIDATE,
		options.dumpguard,
		(unsigned long long)(initialized - started), usage.ru_maxrss,
		(unsigned long long)usage.ru_utime.tv_sec * 1000000ULL +
			usage.ru_utime.tv_usec,
		(unsigned long long)usage.ru_stime.tv_sec * 1000000ULL +
			usage.ru_stime.tv_usec,
		(unsigned long long)options.memlock.rlim_cur,
		(unsigned long long)options.memlock.rlim_max);
	return options.zero_lock && !lock_observed ? 77 : 0;
}
