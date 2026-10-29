#ifndef FVWM_STARTUP_GROUP_POLICY_H
#define FVWM_STARTUP_GROUP_POLICY_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define STARTUP_GROUP_CLIENT_MAX 256
#define STARTUP_GROUP_LEADER_MAX 256
#define STARTUP_GROUP_STEP_MAX 8
#define STARTUP_GROUP_STATE_MAX (64 * 1024)

/* Masks are protocol event bits. X-dependent acquisition and application stay
 * in the core; the policy never reads X or retains a window-record pointer. */
struct startup_group_leader {
	uint64_t window;
	uint64_t serial;
	uint32_t base_mask;
	uint32_t suppressed[32];
	unsigned int references;
	bool acquired;
	bool base_known;
	bool managed;
	bool mask_dirty;
};

struct startup_group_binding {
	uint64_t client;
	uint64_t identity;
	uint64_t leader;
	bool dirty;
	bool invalidated;
};

struct startup_group_state {
	struct startup_group_leader leaders[STARTUP_GROUP_LEADER_MAX];
	struct startup_group_binding clients[STARTUP_GROUP_CLIENT_MAX];
	uint32_t property_mask;
	uint32_t structure_mask;
	unsigned int cursor;
	unsigned int leader_count;
	bool pending;
	bool queued;
};

enum startup_group_work_kind {
	STARTUP_GROUP_NO_WORK,
	STARTUP_GROUP_MASK_WORK,
	STARTUP_GROUP_CLIENT_WORK
};

struct startup_group_work {
	enum startup_group_work_kind kind;
	unsigned int index;
};

void startup_group_init(struct startup_group_state *state,
	uint32_t property_mask, uint32_t structure_mask);
struct startup_group_leader *startup_group_find_leader(
	struct startup_group_state *state, uint64_t window);
struct startup_group_binding *startup_group_find_binding(
	struct startup_group_state *state, uint64_t client);
/* An explicit hints/lifecycle binding may revive an invalidated group. Failed
 * replacement retires the old association, never continuing to observe it. */
bool startup_group_bind(struct startup_group_state *state, uint64_t client,
	uint64_t identity, uint64_t leader, uint64_t serial);
void startup_group_unbind(struct startup_group_state *state, uint64_t client,
	uint64_t identity);
void startup_group_defer(struct startup_group_state *state, uint64_t client,
	uint64_t identity);
void startup_group_pending(struct startup_group_state *state, bool pending,
	bool rescan);
bool startup_group_next_work(struct startup_group_state *state,
	struct startup_group_work *work);
bool startup_group_outer_pending(const struct startup_group_state *state);
void startup_group_acquired(struct startup_group_state *state, uint64_t leader,
	uint32_t base_mask, uint64_t serial, bool managed);
void startup_group_mode(struct startup_group_state *state, uint64_t leader,
	bool managed);
/* A destroyed leader invalidates all old dependent bindings. The caller
 * filters synthetic DestroyNotify. Older queued serials do not touch a new
 * acquisition. No X restoration is performed by this pure policy. */
bool startup_group_notify(struct startup_group_state *state, uint64_t leader,
	uint64_t serial, bool destroyed);
void startup_group_abandon(struct startup_group_state *state, uint64_t leader);
bool startup_group_release(struct startup_group_state *state, uint64_t leader);
uint32_t startup_group_effective(const struct startup_group_state *state,
	const struct startup_group_leader *leader);
void startup_group_base(struct startup_group_leader *leader, uint32_t mask);
bool startup_group_suppress(struct startup_group_leader *leader, uint32_t mask);
void startup_group_unsuppress(struct startup_group_leader *leader,
	uint32_t mask);

#endif
