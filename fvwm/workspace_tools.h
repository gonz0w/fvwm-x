#ifndef FVWM_WORKSPACE_TOOLS_H
#define FVWM_WORKSPACE_TOOLS_H

#include "workspace_tools_policy.h"
#include "libs/cJSON.h"

struct fmodule;
struct FvwmWindow;
bool workspace_window_is_application(const struct FvwmWindow *fw);
struct scratchpad_plan {
	uintptr_t owner;
	char name[33], serial[17];
	uint64_t target, prior_focus;
	enum scratchpad_action action;
	bool owns_focus;
};
void scratchpad_initialize(bool restarted);
void scratchpad_owner_gone(struct fmodule *owner);
cJSON *scratchpad_prepare(struct fmodule *owner,
	const struct scratchpad_request *request, struct scratchpad_plan *plan,
	const char **error);
/* Re-resolve stable identities; no core pointer crosses dispatch. */
bool scratchpad_command(const struct scratchpad_plan *plan, unsigned int step,
	char *command, size_t size, const char **error);
#endif
