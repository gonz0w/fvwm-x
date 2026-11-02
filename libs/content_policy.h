#ifndef FVWM_CONTENT_POLICY_H
#define FVWM_CONTENT_POLICY_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define CONTENT_ITEMS_MAX 4096
#define CONTENT_LEASES_MAX 4096
#define CONTENT_REPRESENTATIONS_MAX 8

enum content_kind { CONTENT_TEXT = 1, CONTENT_IMAGE, CONTENT_URI };
enum content_hint {
	CONTENT_HINT_ABSENT, CONTENT_HINT_ORDINARY, CONTENT_HINT_SENSITIVE,
	CONTENT_HINT_MALFORMED, CONTENT_HINT_UNAVAILABLE
};
enum content_result {
	CONTENT_OK, CONTENT_INVALID, CONTENT_UNCONFIGURED, CONTENT_EXCLUDED,
	CONTENT_FULL, CONTENT_STALE, CONTENT_EXPIRED, CONTENT_BUSY,
	CONTENT_EXHAUSTED
};
enum content_phase {
	CONTENT_EMPTY, CONTENT_RESERVED, CONTENT_LIVE, CONTENT_RETIRED
};

struct content_ref {
	uint64_t epoch, item, generation;
};
struct content_bytes {
	size_t encoded, decoded, derived;
};
struct content_storage {
	size_t mapped, locked;
};
struct content_representation {
	uint32_t id;
	enum content_kind kind;
	struct content_bytes bytes;
};
/* Numeric metadata only. No borrowed pointers, payloads, labels or hashes.
 * Byte counts reserve worst-case resources, including decode/preview work. */
struct content_candidate {
	enum content_hint hint;
	uint64_t deadline_ms;
	size_t count;
	struct content_storage storage;
	struct content_representation reps[CONTENT_REPRESENTATIONS_MAX];
};
struct content_limits {
	size_t items, representations, per_item, leases, work;
	struct content_bytes total, per_representation;
	struct content_storage storage, per_item_storage;
	uint64_t lifetime_ms;
};
struct content_usage {
	size_t items, representations, leases, work;
	struct content_bytes bytes;
	struct content_storage storage;
};
struct content_item {
	enum content_phase phase;
	struct content_ref ref;
	struct content_candidate descriptor;
	struct content_bytes charged;
	size_t leases;
	bool work, pinned;
};
struct content_lease {
	struct content_ref ref;
	uint64_t id;
	uint32_t representation;
};
/* One serialized native owner owns these arrays and all mutation. Initialize
 * fresh storage once with a nonzero epoch unique across owner lifetimes. Never
 * reinitialize/free it while work, payload cleanup or leases remain. Counters
 * and indices are implementation state, not public identities or a wire ABI. */
struct content_policy {
	struct content_limits limits;
	struct content_usage usage;
	struct content_item *items;
	struct content_lease *leases;
	uint64_t epoch, next_item, next_lease, last_ms;
	bool configured, enabled, paused;
};

enum content_result content_policy_init(struct content_policy *policy,
	uint64_t epoch, const struct content_limits *limits,
	struct content_item *items, struct content_lease *leases,
	uint64_t now_ms);
/* Replacement revokes the old generation at reservation, not publication.
 * Failure does not roll back to old content. Both generations stay charged. */
enum content_result content_reserve(struct content_policy *policy,
	const struct content_candidate *candidate, bool source_allowed,
	const struct content_ref *replace, uint64_t now_ms,
	struct content_ref *output);
/* Check immediately before each native write while the exact producer token
 * is reserved. This is metadata permission, not a payload pointer or an OS
 * access-protection change. Refusal retains work and all resource charges. */
enum content_result content_producer_check(struct content_policy *policy,
	struct content_ref ref, uint32_t representation,
	struct content_bytes offset, struct content_bytes length,
	uint64_t now_ms);
/* The exact ref is also the single producer-work token. Finish consumes it
 * once, including after retirement. A changed hint refuses publication.
 * Successful producers attest initialized prefixes in reservation order and
 * close writable access before this call. Actual lengths cannot exceed the
 * reservation; malformed success retires the item. Failed completion ignores
 * actual/count. Full reservation charges survive until backend cleanup. */
enum content_result content_finish(struct content_policy *policy,
	struct content_ref ref, enum content_hint hint, bool success,
	const struct content_bytes *actual, size_t count,
	uint64_t now_ms);
enum content_result content_retire(struct content_policy *policy,
	struct content_ref ref);
enum content_result content_expire(struct content_policy *policy,
	uint64_t now_ms);
enum content_result content_clear(struct content_policy *policy);
enum content_result content_pin(struct content_policy *policy,
	struct content_ref ref, bool pinned, uint64_t now_ms);
enum content_result content_acquire(struct content_policy *policy,
	struct content_ref ref, uint32_t representation, uint64_t now_ms,
	struct content_lease *output);
enum content_result content_release(struct content_policy *policy,
	struct content_lease lease);
/* Recheck immediately before native access; an outstanding lease retains
 * resources after retirement but no longer grants permission to read them. */
enum content_result content_lease_check(struct content_policy *policy,
	struct content_lease lease, uint64_t now_ms);
/* Explicit allocator model, not a measurement. Round at least one byte to
 * pages, then account bounded allocator/guard overhead before allocation. */
bool content_storage_charge(size_t bytes, size_t page,
	size_t mapped_extra_pages, size_t locked_extra_pages,
	struct content_storage *output);
/* Caller invokes this only AFTER actual backend cleanup. An early call is
 * refused, never remembered as permission to release future resources. */
enum content_result content_cleanup_ack(struct content_policy *policy,
	struct content_ref ref);
/* Safe numeric projection/action validation; this does not acquire a payload
 * lease or grant access to one. Caller still validates its view generation. */
enum content_result content_describe(struct content_policy *policy,
	struct content_ref ref, uint64_t now_ms,
	struct content_candidate *output);
/* Compatibility only; never evidence that original bytes are equal. */
bool content_dedup_compatible(const struct content_candidate *first,
	const struct content_candidate *second);

#endif
