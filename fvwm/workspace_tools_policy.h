#ifndef FVWM_WORKSPACE_TOOLS_POLICY_H
#define FVWM_WORKSPACE_TOOLS_POLICY_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define SCRATCHPAD_MAX 16
#define SCRATCHPAD_TEXT_MAX 128
#define SCRATCHPAD_LEASE_MS 30000

enum scratchpad_verb { SCRATCHPAD_TOGGLE, SCRATCHPAD_RETRY,
	SCRATCHPAD_STATUS, SCRATCHPAD_LAUNCHED, SCRATCHPAD_LAUNCH_FAILED };
enum scratchpad_state { SCRATCHPAD_UNMATCHED, SCRATCHPAD_CLAIMED,
	SCRATCHPAD_PENDING, SCRATCHPAD_UNCERTAIN, SCRATCHPAD_AMBIGUOUS,
	SCRATCHPAD_UNSUPPORTED };
enum scratchpad_action { SCRATCHPAD_NONE, SCRATCHPAD_LAUNCH,
	SCRATCHPAD_SUMMON, SCRATCHPAD_DISMISS };

struct scratchpad_request {
	char cookie[65], serial[17], name[33], fingerprint[65];
	char class_name[129], instance[129], lease[49];
	enum scratchpad_verb verb;
};
struct scratchpad_entry {
	uintptr_t owner;
	char cookie[65], serial[17], name[33], fingerprint[65];
	char class_name[129], instance[129], lease[49];
	uint64_t target, prior_focus, deadline;
	enum scratchpad_state state;
	bool desired_visible;
};
struct scratchpad_registry {
	struct scratchpad_entry entries[SCRATCHPAD_MAX];
	bool restarted;
};
struct scratchpad_observation {
	unsigned int matches;
	uint64_t target;
	bool unsupported, visible;
};
struct scratchpad_result {
	enum scratchpad_state state;
	enum scratchpad_action action;
	bool desired_visible;
	char lease[49];
	uint64_t target;
	const char *reason;
};

bool scratchpad_request_parse(const char *text, struct scratchpad_request *out);
void scratchpad_registry_initialize(struct scratchpad_registry *registry,
	bool restarted);
void scratchpad_registry_owner_gone(struct scratchpad_registry *registry,
	uintptr_t owner);
struct scratchpad_entry *scratchpad_registry_find(
	struct scratchpad_registry *registry, const char *name);
bool scratchpad_registry_apply(struct scratchpad_registry *registry,
	uintptr_t owner, const struct scratchpad_request *request,
	const struct scratchpad_observation *observation, uint64_t now,
	const char *new_lease, struct scratchpad_result *result);
bool workspace_center(int area_x, int area_y, int area_width, int area_height,
	int width, int height, int *x, int *y);

#endif
