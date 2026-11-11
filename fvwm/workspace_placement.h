#ifndef FVWM_WORKSPACE_PLACEMENT_H
#define FVWM_WORKSPACE_PLACEMENT_H

#include "workspace_layout_policy.h"
#include <stdint.h>

struct monitor;

struct workspace_placement_target {
	uint64_t identity, role;
	int64_t virtual_x, virtual_y;
	uint32_t revision;
	rectangle frame;
	int desk;
};

struct workspace_placement_destination {
	uint64_t epoch, role;
	rectangle area;
	int desk, viewport_x, viewport_y;
};

bool workspace_placement_destination(const struct monitor *monitor,
	struct workspace_placement_destination *out);
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
