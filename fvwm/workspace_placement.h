#ifndef FVWM_WORKSPACE_PLACEMENT_H
#define FVWM_WORKSPACE_PLACEMENT_H

#include "workspace_layout_policy.h"
#include <stdint.h>

struct monitor;
struct FvwmWindow;

struct workspace_placement_target {
	uint64_t identity, role;
	int64_t virtual_x, virtual_y;
	uint32_t revision;
	rectangle frame;
	int desk;
};

struct workspace_placement_destination {
	uint64_t epoch, role, workspace_revision;
	rectangle area;
	int desk, viewport_x, viewport_y;
};

bool workspace_placement_destination(const struct monitor *monitor,
	struct workspace_placement_destination *out);
bool workspace_placement_destination_is_current(
	const struct workspace_placement_destination *expected);
bool workspace_placement_select_desk(
	struct workspace_placement_destination *destination, int desk);
bool workspace_placement_refresh_area(
	struct workspace_placement_destination *destination);
bool workspace_panel_command(uint64_t revision,
	const struct workspace_placement_destination *destination,
	char *command, size_t size);
bool workspace_placement_observe(const struct FvwmWindow *fw,
	struct workspace_placement_target *out);
bool workspace_placement_collect(const struct monitor *monitor,
	struct workspace_placement_target targets[WORKSPACE_LAYOUT_MAX],
	size_t *count);
enum workspace_placement_mode
{
	WORKSPACE_PLACEMENT_DOCK,
	WORKSPACE_PLACEMENT_TILE,
	WORKSPACE_PLACEMENT_FLOAT
};

bool workspace_placement_apply(const struct workspace_placement_target *expected,
	const struct workspace_placement_destination *destination,
	rectangle frame, enum workspace_placement_mode mode, struct workspace_placement_target *observed);

#endif
