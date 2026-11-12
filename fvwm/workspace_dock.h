#ifndef FVWM_WORKSPACE_DOCK_H
#define FVWM_WORKSPACE_DOCK_H

#include "workspace_docking_policy.h"
#include "workspace_placement.h"

#define WORKSPACE_DOCK_OFFSET_MAX 65535
#define WORKSPACE_DOCK_SELECTOR_WIRE_MAX 440

struct workspace_dock_window_input {
	struct workspace_docking_selector selector;
	int32_t offset_x, offset_y;
};

struct workspace_dock_profile_input {
	char connector[WORKSPACE_DOCKING_NAME_MAX + 1];
	size_t count;
	int desk;
	bool has_desk;
	struct workspace_dock_window_input windows[WORKSPACE_DOCKING_WINDOW_MAX];
};

struct workspace_dock_plan_entry {
	struct workspace_docking_selector selector;
	struct workspace_placement_target expected;
	int32_t offset_x, offset_y;
};

struct workspace_dock_plan {
	struct workspace_placement_destination destination;
	struct workspace_dock_plan_entry entries[WORKSPACE_DOCKING_WINDOW_MAX];
	size_t requested, count;
	int desk;
	bool primary_fallback, has_desk, prepared;
};

struct workspace_dock_result {
	size_t applied, skipped;
	bool primary_fallback;
};

enum workspace_dock_status {
	WORKSPACE_DOCK_OK,
	WORKSPACE_DOCK_INVALID,
	WORKSPACE_DOCK_AMBIGUOUS,
	WORKSPACE_DOCK_UNAVAILABLE,
	WORKSPACE_DOCK_STALE,
	WORKSPACE_DOCK_BUSY,
};

/* Prepare from current managed windows and committed topology. Missing and
 * ineligible windows are skipped. Duplicate live selectors refuse the plan.
 */
enum workspace_dock_status workspace_dock_prepare(
	const struct workspace_dock_profile_input *input,
	struct workspace_dock_plan *plan);

typedef bool (*workspace_dock_before_placement)(
	const struct workspace_placement_destination *destination, void *context);

/* Consume a prepared plan once. Never bind a replacement allocation. The
 * optional synchronous panel action precedes final work-area placement.
 */
enum workspace_dock_status workspace_dock_apply(
	struct workspace_dock_plan *plan, struct workspace_dock_result *result,
	workspace_dock_before_placement before_placement, void *context);

struct fmodule;
void workspace_dock_owner_gone(struct fmodule *owner);

#endif
