/* This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include "fvwm/configure_merge.h"
#include <assert.h>

static void test_latest_fields_and_stacking_order(void)
{
	XConfigureRequestEvent older = {.value_mask = CWWidth | CWX | CWY |
	    CWHeight | CWBorderWidth | CWSibling | CWStackMode,
	    .width = 200, .height = 120, .x = 123, .y = -42,
	    .border_width = 3, .above = 77, .detail = Above};
	XConfigureRequestEvent newer = {.value_mask = CWWidth | CWY |
	    CWStackMode, .width = 300, .y = 10, .detail = Below};

	fvwm_merge_configure_fields(&older, &newer);
	assert(newer.width == 300 && newer.y == 10);
	assert(newer.x == 123 && newer.height == 120 &&
	    newer.border_width == 3);
	assert(newer.detail == Below && !(newer.value_mask & CWSibling));
	assert(older.value_mask == (CWSibling | CWStackMode));
	assert(older.above == 77 && older.detail == Above);
}

static void test_hint_barrier_targets_only_matching_window(void)
{
	XEvent event = {.xproperty = {.type = PropertyNotify, .window = 42,
	    .atom = XA_WM_NORMAL_HINTS}};

	assert(fvwm_configure_merge_hint_barrier(&event, 42));
	assert(!fvwm_configure_merge_hint_barrier(&event, 43));
	event.xproperty.atom = XA_WM_NAME;
	assert(!fvwm_configure_merge_hint_barrier(&event, 42));
}

int main(void)
{
	test_latest_fields_and_stacking_order();
	test_hint_barrier_targets_only_matching_window();
	return 0;
}
