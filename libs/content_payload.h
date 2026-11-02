#ifndef FVWM_CONTENT_PAYLOAD_H
#define FVWM_CONTENT_PAYLOAD_H

#include "libs/content_policy.h"
#include <sys/types.h>

#define CONTENT_PAYLOAD_CHUNK_MAX 4096

enum content_payload_domain {
	CONTENT_PAYLOAD_ENCODED = 1,
	CONTENT_PAYLOAD_DECODED,
	CONTENT_PAYLOAD_DERIVED
};
enum content_payload_result {
	CONTENT_PAYLOAD_OK,
	CONTENT_PAYLOAD_INVALID,
	CONTENT_PAYLOAD_REFUSED,
	CONTENT_PAYLOAD_BUSY,
	CONTENT_PAYLOAD_UNSUPPORTED,
	CONTENT_PAYLOAD_OS_FAILURE,
	CONTENT_PAYLOAD_CONSUMER_FAILED,
	CONTENT_PAYLOAD_FATAL
};
enum content_payload_operation {
	CONTENT_PAYLOAD_NONE,
	CONTENT_PAYLOAD_PROCESS,
	CONTENT_PAYLOAD_PAGE,
	CONTENT_PAYLOAD_MAP,
	CONTENT_PAYLOAD_PROTECT,
	CONTENT_PAYLOAD_DUMP,
	CONTENT_PAYLOAD_LOCK,
	CONTENT_PAYLOAD_UNLOCK,
	CONTENT_PAYLOAD_UNMAP
};
struct content_payload_error {
	enum content_result policy;
	enum content_payload_operation operation;
	int number;
	/* Retired cleanup remains, not merely a live item retaining storage. */
	bool cleanup_pending;
};
/* Local process attestation only: never serialize or carry across fork/exec.
 * The isolated owner must not change credentials/dump policy with live data. */
struct content_process_guard {
	pid_t pid;
	unsigned kind;
};
struct content_payload_region {
	void *mapping;
	unsigned char *data;
	size_t mapped, rounded, wiped;
	struct content_bytes capacity, written;
	uint32_t representation;
	bool locked, closed;
};
/* Metadata storage is caller-bounded and remains alive until final cleanup.
 * item points into the owner's fixed policy array, never a recycled item. */
struct content_payload_slot {
	struct content_item *item;
	struct content_ref ref;
	size_t count;
	struct content_payload_region regions[CONTENT_REPRESENTATIONS_MAX];
};
/* One serialized C-only owner. Do not mutate its policy/arrays independently.
 * No installed caller, worker, numeric defaults or destructor is implied.
 * Keep all arrays alive until usage is empty and every mapping is released. */
struct content_payload_owner {
	struct content_policy policy;
	struct content_payload_slot *slots;
	struct content_process_guard guard;
	struct content_payload_error fatal_error;
	size_t page, cleanup_cursor;
	bool busy, fatal;
};
typedef bool (*content_payload_consumer)(const unsigned char *bytes,
	size_t length, void *context);

/* Explicit process-wide preflight for an owned isolated service only. Partial
 * failure may already have changed this process; it promises no rollback.
 * Linking and init never change these process settings. */
enum content_payload_result content_payload_process_prepare(
	struct content_process_guard *output,
	struct content_payload_error *error);
/* Fresh initialization once. Verify guard read-only, derive native page size,
 * then initialize fixed arrays. Error outputs are required and initialized. */
enum content_payload_result content_payload_init(
	struct content_payload_owner *owner,
	const struct content_process_guard *guard, uint64_t epoch,
	const struct content_limits *limits, struct content_item *items,
	struct content_lease *leases, struct content_payload_slot *slots,
	uint64_t now_ms, struct content_payload_error *error);
/* Caller-selected producer admission only. Init remains disabled; pausing or
 * disabling admission does not select a desktop-lock read/history policy. */
enum content_payload_result content_payload_set_admission(
	struct content_payload_owner *owner, bool enabled, bool paused,
	struct content_payload_error *error);
/* Ignore candidate.storage: derive/sum all representation charges first.
 * Reserve before mapping; prepare every region before any producer write.
 * On preparation failure output stays zero, but cleanup retains admission. */
enum content_payload_result content_payload_reserve(
	struct content_payload_owner *owner,
	const struct content_candidate *candidate, bool source_allowed,
	const struct content_ref *replace, uint64_t now_ms,
	struct content_ref *output, struct content_payload_error *error);
/* Append at the exact initialized domain prefix, at most CHUNK_MAX bytes.
 * source is a valid borrowed native input for this call, never a retained
 * payload pointer. The outside-owner copy boundary is the caller's duty. */
enum content_payload_result content_payload_write(
	struct content_payload_owner *owner, struct content_ref ref,
	uint32_t representation, enum content_payload_domain domain,
	size_t expected_offset, const void *source, size_t length,
	uint64_t now_ms, struct content_payload_error *error);
enum content_payload_result content_payload_publish(
	struct content_payload_owner *owner, struct content_ref ref,
	enum content_hint hint, uint64_t now_ms,
	struct content_payload_error *error);
/* A synchronous bounded C consumer only: no blocking, event pumping, pointer
 * retention, nonlocal exits or Go/Lua callback. Reject reentry. Empty reads
 * pass NULL, 0 without opening memory. Always close access and release the
 * exact scoped lease. */
enum content_payload_result content_payload_with_read(
	struct content_payload_owner *owner, struct content_ref ref,
	uint32_t representation, enum content_payload_domain domain,
	size_t offset, size_t length, content_payload_consumer consumer,
	void *context, uint64_t now_ms, struct content_payload_error *error);
enum content_payload_result content_payload_retire(
	struct content_payload_owner *owner, struct content_ref ref,
	struct content_payload_error *error);
enum content_payload_result content_payload_clear(
	struct content_payload_owner *owner,
	struct content_payload_error *error);
enum content_payload_result content_payload_expire(
	struct content_payload_owner *owner, uint64_t now_ms,
	struct content_payload_error *error);
/* At most one CHUNK_MAX wipe and one region release per call. Failed/partial
 * cleanup retains its address, lock/wipe state and full policy charge. A
 * successful unmap also ends any lock obligation. Acknowledge last. Fatal
 * owners permit retirement/cleanup only and require their future service to
 * stop; logical retirement does not prove OS access was closed. */
enum content_payload_result content_payload_cleanup_step(
	struct content_payload_owner *owner, bool *progress,
	struct content_payload_error *error);

#endif
