#ifndef FVWM_SELECTION_SOURCE_POLICY_H
#define FVWM_SELECTION_SOURCE_POLICY_H

#include <stdbool.h>
#include <stdint.h>

enum selection_source_phase {
	SELECTION_SOURCE_PHASE_UNAVAILABLE,
	SELECTION_SOURCE_PHASE_UNSYNCHRONIZED,
	SELECTION_SOURCE_PHASE_READY,
	SELECTION_SOURCE_PHASE_CLOSED,
	SELECTION_SOURCE_PHASE_EXHAUSTED
};
enum selection_source_result {
	SELECTION_SOURCE_OK,
	SELECTION_SOURCE_EMPTY,
	SELECTION_SOURCE_RETRY,
	SELECTION_SOURCE_INVALID,
	SELECTION_SOURCE_UNAVAILABLE,
	SELECTION_SOURCE_CLOSED,
	SELECTION_SOURCE_EXHAUSTED,
	SELECTION_SOURCE_BUSY,
	SELECTION_SOURCE_ERROR
};
struct selection_source_token {
	uint64_t epoch, generation;
	uint32_t owner;
};
struct selection_source_policy {
	uint64_t epoch, generation;
	uint32_t owner;
	enum selection_source_phase phase;
	bool initialized;
};

/* One serialized caller-owned observer. Fresh storage once; epoch is nonzero
 * and unique across observer lifetimes, including recreated storage. Never
 * reset/reinitialize a lifetime whose tokens or callbacks can still exist.
 * Owner is an X protocol ID; no timestamps establish token identity. */
enum selection_source_result selection_source_init(
	struct selection_source_policy *policy, uint64_t epoch, bool available);
/* One admitted server notification, even for the same owner/time or while
 * already unsynchronized. Advance without wrap and revoke permission. */
enum selection_source_result selection_source_changed(
	struct selection_source_policy *policy);
/* A partial/uncertain scan revokes permission without repeatedly advancing on
 * an unchanged unrelated prefix. Only a later clean snapshot can recover. */
enum selection_source_result selection_source_uncertain(
	struct selection_source_policy *policy);
/* Native caller has completed query-then-drain ordering. complete=false never
 * publishes. A clean snapshot always advances generation, even for owner zero;
 * empty selection has no capturable token. Output resets on every failure. */
enum selection_source_result selection_source_snapshot(
	struct selection_source_policy *policy, uint32_t owner, bool complete,
	struct selection_source_token *token);
enum selection_source_result selection_source_token_get(
	const struct selection_source_policy *policy,
	struct selection_source_token *token);
bool selection_source_current(const struct selection_source_policy *policy,
	struct selection_source_token token);
/* Invalidate first; do not reset epoch/counter or release caller-owned storage.
 * Native subscriptions/windows and callback lifetimes have separate cleanup. */
void selection_source_close(struct selection_source_policy *policy);

#endif
