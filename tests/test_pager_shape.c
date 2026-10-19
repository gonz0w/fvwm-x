/* This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or any later version.
 */
#include "config.h"
#include "libs/monitor_types.h"
#include "libs/FShape.h"
#include "libs/Module.h"
#include "libs/fvwmlib.h"
#include "fvwm/fvwm.h"
#include "modules/FvwmPager/FvwmPager.h"
#include "modules/FvwmPager/pager_shape.h"
#include <assert.h>
#include <limits.h>
#include <string.h>

Display *dpy;
ScreenInfo Scr;
bool ShapeLabels = true, LabelsBelow, fAlwaysCurrentDesk;
int Rows = 2, Columns = 3, ndesks = 5, label_h = 3;
int desk_w = 9, desk_h = 6, desk1;
struct fpmonitors fp_monitor_q;
struct fpmonitor *monitor_to_track;
static XRectangle captured[8];
static int captured_count, calls;

void XShapeCombineRectangles(Display *display, Window destination, int kind,
	int x, int y, XRectangle *rectangles, int count, int operation, int ordering)
{
	assert(display == dpy && destination == Scr.pager_w);
	assert(kind == FShapeBounding && operation == FShapeSet);
	assert(x == 0 && y == 0 && ordering == 0);
	assert(count > 0 && count <= 8);
	memcpy(captured, rectangles, count * sizeof(*rectangles));
	captured_count = count;
	calls++;
}

int main(void)
{
	struct monitor monitors[2] = {0};
	struct fpmonitor first = {.m = &monitors[0]};
	struct fpmonitor second = {.m = &monitors[1]};
	monitors[0].virtual_scr.CurrentDesk = 1;
	monitors[1].virtual_scr.CurrentDesk = 4;
	TAILQ_INIT(&fp_monitor_q);
	TAILQ_INSERT_TAIL(&fp_monitor_q, &first, entry);
	TAILQ_INSERT_TAIL(&fp_monitor_q, &second, entry);
	pager_update_window_shape();
	assert(captured_count == 5 && calls == 1);
	for (int i = 0; i < ndesks; i++)
	{
		bool highlighted = i == 1 || i == 4;
		assert(captured[i].x == (i % Columns) * 10);
		assert(captured[i].y == (i / Columns) * 11 +
			(highlighted ? 0 : 3));
		assert(captured[i].width == 10);
		assert(captured[i].height == (highlighted ? 11 : 8));
	}
	monitor_to_track = &second;
	LabelsBelow = true;
	pager_update_window_shape();
	assert(captured[1].y == 0 && captured[1].height == 8);
	assert(captured[4].y == 11 && captured[4].height == 11);
	second.disabled = true;
	pager_update_window_shape();
	assert(captured[4].height == 8);
	Rows = Columns = INT_MAX;
	ndesks = 1;
	monitor_to_track = NULL;
	pager_update_window_shape();
	assert(captured_count == 1 && captured[0].x == 0 && captured[0].y == 0);
	assert(captured[0].width == 10 && captured[0].height == 8);
	desk_w = desk_h = label_h = 65535;
	pager_update_window_shape();
	assert(captured[0].width == 65535 && captured[0].height == 65535);
	label_h = 3;
	int previous = calls;
	ShapeLabels = false;
	pager_update_window_shape();
	ShapeLabels = true;
	fAlwaysCurrentDesk = true;
	pager_update_window_shape();
	fAlwaysCurrentDesk = false;
	label_h = 0;
	pager_update_window_shape();
	assert(calls == previous);
	return 0;
}
