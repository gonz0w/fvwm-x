#ifndef FVWM_WORKSPACE_LAYOUT_POLICY_H
#define FVWM_WORKSPACE_LAYOUT_POLICY_H

#include "libs/fvwmrect.h"
#include <stdbool.h>
#include <stddef.h>

#define WORKSPACE_LAYOUT_MAX 64

enum workspace_layout_kind {
	WORKSPACE_LAYOUT_COLUMNS,
	WORKSPACE_LAYOUT_MASTER_STACK,
};

struct workspace_layout_plan {
	size_t count;
	rectangle cells[WORKSPACE_LAYOUT_MAX];
};

/* Cells are in caller order and use actual pixels. Columns divide the width.
 * Master-stack gives the first cell the left half and divides the right half
 * vertically. Earlier cells receive the remainder pixels. A single cell
 * receives the whole work area. Invalid bounds or empty cells clear the plan.
 */
bool workspace_layout_plan(enum workspace_layout_kind kind, rectangle area,
	size_t count, struct workspace_layout_plan *out);

#endif
