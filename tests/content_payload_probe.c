/* This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */
#include "config.h"
#include "libs/content_payload.h"
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/resource.h>
#include <unistd.h>

/* Manual, non-forking public-byte controls for the production owner variant.
 * Never register expected faults as passing routine tests, use sanitizer fault
 * interception, or treat a numeric exit code as actual signal evidence. */
enum payload_control { CONTROL_RW = 1, CONTROL_CLOSED, CONTROL_READONLY,
	CONTROL_ZERO_LOCK };
static const unsigned char payload_fixture[] = {0, 0xff, 42};
struct payload_trial {
	struct content_payload_owner owner;
	struct content_item items[2];
	struct content_lease leases[4];
	struct content_payload_slot slots[2];
	struct content_payload_error error;
	struct content_process_guard guard;
	struct rlimit memlock;
	enum payload_control control;
	bool eligible;
};

/* Same bounded eligibility rule as the earlier memory-comparison probe.
 * No IDs/capabilities change; the dispatcher selects an existing profile. */
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

static bool restore_signals(void)
{
	struct sigaction action = {0};
	sigset_t signals;

	action.sa_handler = SIG_DFL;
	return sigemptyset(&action.sa_mask) == 0 &&
		sigemptyset(&signals) == 0 &&
		sigaddset(&signals, SIGSEGV) == 0 &&
		sigaddset(&signals, SIGBUS) == 0 &&
		sigaction(SIGSEGV, &action, NULL) == 0 &&
		sigaction(SIGBUS, &action, NULL) == 0 &&
		sigprocmask(SIG_UNBLOCK, &signals, NULL) == 0;
}

static bool ready(const struct payload_trial *trial)
{
	return printf("{\"probe\":\"native-content-payload\",\"ready\":true,"
		"\"control\":%d,\"uid\":%llu,\"euid\":%llu,"
		"\"eligible\":%s,\"dumpguard\":%u,\"page\":%zu,"
		"\"memlock_soft\":%llu,\"memlock_hard\":%llu,"
		"\"closed\":%s,\"leases\":%zu,\"work\":%zu,"
		"\"live\":%s,\"produced\":%zu,"
		"\"mapped_model\":%zu,\"locked_model\":%zu,"
		"\"sigsegv\":%d,\"sigbus\":%d}\n",
		trial->control, (unsigned long long)getuid(),
		(unsigned long long)geteuid(),
		trial->eligible ? "true" : "false",
		trial->guard.kind, trial->owner.page,
		(unsigned long long)trial->memlock.rlim_cur,
		(unsigned long long)trial->memlock.rlim_max,
		trial->slots[0].regions[0].closed ? "true" : "false",
		trial->owner.policy.usage.leases,
		trial->owner.policy.usage.work,
		trial->items[0].phase == CONTENT_LIVE ? "true" : "false",
		trial->slots[0].regions[0].written.encoded,
		trial->owner.policy.usage.storage.mapped,
		trial->owner.policy.usage.storage.locked,
		SIGSEGV, SIGBUS) >= 0 &&
		fflush(stdout) == 0;
}

static bool check_read(const unsigned char *bytes, size_t length, void *context)
{
	(void)context;
	return length == sizeof(payload_fixture) &&
		!memcmp(bytes, payload_fixture, length);
}

static bool readonly_write(const unsigned char *bytes, size_t length,
	void *context)
{
	struct payload_trial *trial = context;

	if (!check_read(bytes, length, NULL) || !ready(trial))
		return false;
	/* Intentional public-byte permission experiment, not a production
	 * consumer. The separate supervisor must confirm native SIGSEGV/BUS. */
	*(volatile unsigned char *)bytes = 7;
	return false;
}

static bool clean(struct payload_trial *trial)
{
	if (content_payload_clear(&trial->owner, &trial->error) !=
		CONTENT_PAYLOAD_OK)
		return false;
	for (size_t i = 0; trial->owner.policy.usage.items && i < 64; i++)
	{
		bool progress;

		if (content_payload_cleanup_step(&trial->owner, &progress,
			&trial->error) != CONTENT_PAYLOAD_OK || !progress)
			return false;
	}
	const struct content_usage *usage = &trial->owner.policy.usage;

	return !usage->items && !usage->work && !usage->leases &&
		!usage->representations && !usage->bytes.encoded &&
		!usage->bytes.decoded && !usage->bytes.derived &&
		!usage->storage.mapped && !usage->storage.locked;
}

static bool done(const struct payload_trial *trial, bool negative)
{
	return printf("{\"probe\":\"native-content-payload\",\"done\":true,"
		"\"control\":%d,\"lock_refused\":%s,"
		"\"uid\":%llu,\"euid\":%llu,\"eligible\":%s,"
		"\"dumpguard\":%u,\"page\":%zu,"
		"\"memlock_soft\":%llu,\"memlock_hard\":%llu,"
		"\"clean\":true,\"items\":0,\"work\":0,\"leases\":0,"
		"\"mapped_model\":0,\"locked_model\":0}\n",
		trial->control, negative ? "true" : "false",
		(unsigned long long)getuid(), (unsigned long long)geteuid(),
		trial->eligible ? "true" : "false", trial->guard.kind,
		trial->owner.page, (unsigned long long)trial->memlock.rlim_cur,
		(unsigned long long)trial->memlock.rlim_max) >= 0 &&
		fflush(stdout) == 0;
}

int main(int argc, char **argv)
{
	struct payload_trial trial = {0};
	struct content_ref ref;
	struct content_limits limits = {
		.items = 2, .representations = 2, .per_item = 1,
		.leases = 4, .work = 2,
		.total = {131072, 131072, 131072},
		.per_representation = {65536, 65536, 65536},
		.storage = {1048576, 524288},
		.per_item_storage = {524288, 262144}, .lifetime_ms = 5000
	};
	struct content_candidate candidate = {
		.hint = CONTENT_HINT_ABSENT, .deadline_ms = 1000, .count = 1,
		.reps = {{.id = 11, .kind = CONTENT_TEXT}}
	};

	if (argc != 2)
		return 2;
	if (!strcmp(argv[1], "rw-control"))
		trial.control = CONTROL_RW;
	else if (!strcmp(argv[1], "noaccess-write"))
		trial.control = CONTROL_CLOSED;
	else if (!strcmp(argv[1], "readonly-write"))
		trial.control = CONTROL_READONLY;
	else if (!strcmp(argv[1], "zero-lock-limit"))
		trial.control = CONTROL_ZERO_LOCK;
	else
		return 2;
	trial.eligible = unprivileged_lock_profile();
	if (!restore_signals() ||
		content_payload_process_prepare(&trial.guard, &trial.error) !=
			CONTENT_PAYLOAD_OK ||
		getrlimit(RLIMIT_MEMLOCK, &trial.memlock) < 0)
		return 77;
	if (trial.control == CONTROL_ZERO_LOCK)
	{
		struct rlimit zero = {0, 0}, applied;

		if (!trial.eligible || setrlimit(RLIMIT_MEMLOCK, &zero) < 0 ||
			getrlimit(RLIMIT_MEMLOCK, &applied) < 0 ||
			applied.rlim_cur || applied.rlim_max)
			return 77;
		trial.memlock = applied;
	}
	enum content_payload_result result = content_payload_init(&trial.owner,
		&trial.guard, 1, &limits,
		trial.items, trial.leases, trial.slots,
		100, &trial.error);

	if (result != CONTENT_PAYLOAD_OK)
		return result == CONTENT_PAYLOAD_OS_FAILURE ||
			result == CONTENT_PAYLOAD_UNSUPPORTED ? 77 : 1;
	if (content_payload_set_admission(&trial.owner, true, false,
		&trial.error) != CONTENT_PAYLOAD_OK)
		return 1;
	candidate.reps[0].bytes.encoded = trial.owner.page;
	result = content_payload_reserve(&trial.owner,
		&candidate, true, NULL, 100, &ref, &trial.error);

	if (trial.control == CONTROL_ZERO_LOCK)
	{
		bool refused = result == CONTENT_PAYLOAD_OS_FAILURE &&
			trial.error.operation == CONTENT_PAYLOAD_LOCK &&
			trial.error.number > 0 && !ref.epoch &&
			trial.error.cleanup_pending &&
			trial.owner.policy.usage.items == 1 &&
			!trial.owner.policy.usage.work &&
			!trial.owner.policy.usage.leases &&
			trial.items[0].phase == CONTENT_RETIRED &&
			trial.owner.policy.usage.storage.mapped > 0 &&
			trial.owner.policy.usage.storage.locked > 0 &&
			!trial.slots[0].regions[0].written.encoded;
		int failure = trial.error.number;
		unsigned operation = trial.error.operation;
		size_t mapped = trial.owner.policy.usage.storage.mapped;
		size_t locked = trial.owner.policy.usage.storage.locked;
		bool closed = trial.slots[0].regions[0].closed;

		if (!clean(&trial))
			return 1;
		if (!refused)
			return result == CONTENT_PAYLOAD_OS_FAILURE ||
				result == CONTENT_PAYLOAD_UNSUPPORTED ? 77 : 1;
		if (printf("{\"probe\":\"native-content-payload\","
			"\"negative\":true,\"operation\":%u,\"errno\":%d,"
			"\"mapped_before\":%zu,\"locked_before\":%zu,"
			"\"closed_before\":%s,\"items_before\":1,"
			"\"work_before\":0,\"leases_before\":0,"
			"\"produced_before\":0}\n",
			operation, failure, mapped, locked,
			closed ? "true" : "false") < 0)
			return 1;
		return done(&trial, true) ? 0 : 1;
	}
	if (result != CONTENT_PAYLOAD_OK)
	{
		if (!clean(&trial))
			return 1;
		return result == CONTENT_PAYLOAD_OS_FAILURE ||
			result == CONTENT_PAYLOAD_UNSUPPORTED ? 77 : 1;
	}
	if (content_payload_write(&trial.owner, ref, 11,
		CONTENT_PAYLOAD_ENCODED, 0, payload_fixture,
		sizeof(payload_fixture), 100, &trial.error) !=
			CONTENT_PAYLOAD_OK ||
		content_payload_publish(&trial.owner, ref, candidate.hint, 100,
			&trial.error) != CONTENT_PAYLOAD_OK ||
		content_payload_with_read(&trial.owner, ref, 11,
			CONTENT_PAYLOAD_ENCODED, 0, sizeof(payload_fixture),
			check_read, NULL, 100, &trial.error) !=
				CONTENT_PAYLOAD_OK)
		return 1;
	if (trial.control == CONTROL_READONLY)
	{
		content_payload_with_read(&trial.owner, ref, 11,
			CONTENT_PAYLOAD_ENCODED, 0, sizeof(payload_fixture),
			readonly_write, &trial, 100, &trial.error);
		return 1;
	}
	if (!ready(&trial))
		return 1;
	if (trial.control == CONTROL_CLOSED)
	{
		/* Internal diagnostic address, never an API lease. */
		*(volatile unsigned char *)trial.slots[0].regions[0].data = 7;
		return 1;
	}
	return clean(&trial) && done(&trial, false) ? 0 : 1;
}
