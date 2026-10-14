/* This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or any later version.
 */
#include "config.h"
#include "libs/FScreen.h"
#include "libs/Module.h"
#include "libs/fvwmlib.h"
#include "fvwm/fvwm.h"
#include "modules/FvwmPager/FvwmPager.h"
#include <assert.h>
#include <limits.h>
#include <stdlib.h>

Display *dpy;
bool HilightDesks;
int ndesks = 2;
struct fpmonitor *monitor_to_track;
struct fpmonitors fp_monitor_q;
enum monitor_tracking monitor_mode;
static struct monitor *current;
static unsigned moves;
static int total_width = 800, total_height = 600;

struct monitor *monitor_get_current(void)
{
	return current;
}

int monitor_get_all_widths(void)
{
	return total_width;
}

int monitor_get_all_heights(void)
{
	return total_height;
}

int XMoveWindow(Display *display, Window window, int x, int y)
{
	assert(display == dpy && window != None);
	assert(x == -32768 && y == -32768);
	moves++;
	return 1;
}

int main(void)
{
	struct screen_info screens[2] = {
		{.name = "first", .x = 0, .y = 0, .w = 400, .h = 600},
		{.name = "second", .x = 400, .y = 0, .w = 400, .h = 600}};
	struct monitor monitors[2] = {{.si = &screens[0]}, {.si = &screens[1]}};
	TAILQ_INIT(&fp_monitor_q);
	assert(fpmonitor_get_all_widths() == 800);
	assert(fpmonitor_get_all_heights() == 600);
	struct fpmonitor *first = fpmonitor_new(&monitors[0]);
	HilightDesks = true;
	struct fpmonitor *second = fpmonitor_new(&monitors[1]);
	assert(first->CPagerWin == NULL && second->CPagerWin != NULL);
	assert(first->scr_width == 800 && first->scr_height == 600);
	assert(first->virtual_scr.VxPages == 1 && first->virtual_scr.VyPages == 1);
	assert(first->virtual_scr.VWidth == 800 && first->virtual_scr.VHeight == 600);
	assert(first->virtual_scr.VxMax == 0 && first->virtual_scr.VyMax == 0);
	assert(fpmonitor_from_xy(10, 10) == first);
	assert(fpmonitor_from_xy(410, 10) == second);
	current = &monitors[0];
	monitor_to_track = first;
	assert(fpmonitor_this(NULL) == first);
	fpmonitor_disable(first);
	assert(monitor_to_track == second && moves == 0);
	assert(fpmonitor_this(&monitors[0]) == NULL);
	assert(fpmonitor_this(NULL) == second);
	assert(fpmonitor_from_n(0) == second && fpmonitor_count() == 1);
	monitor_to_track = NULL;
	assert(fpmonitor_from_xy(10, 10) == NULL);
	assert(fpmonitor_from_xy(410, 10) == second);
	monitor_to_track = second;
	second->CPagerWin[1] = 123;
	fpmonitor_disable(second);
	assert(monitor_to_track == NULL && moves == 1);
	assert(fpmonitor_this(NULL) == NULL && fpmonitor_count() == 0);
	first->disabled = false;
	monitors[0].flags = MONITOR_DISABLED;
	assert(fpmonitor_this(&monitors[0]) == NULL);
	monitors[0].flags = 0;
	assert(fpmonitor_this(NULL) == first);
	assert(fpmonitor_from_xy(10, 10) == first);
	first->scr_width = 0;
	total_width = 0;
	assert(fpmonitor_from_xy(10, 10) == NULL);
	total_width = INT_MAX;
	screens[0].x = INT_MAX - 2;
	screens[0].w = 10;
	assert(fpmonitor_from_xy(INT_MAX - 1, 10) == first);
	first->scr_height = 0;
	total_height = 0;
	assert(fpmonitor_from_xy(INT_MAX - 1, 10) == NULL);
	total_height = 600;
	current = &monitors[1];
	monitor_to_track = first;
	second->disabled = false;
	fpmonitor_disable(first);
	assert(monitor_to_track == second);
	TAILQ_REMOVE(&fp_monitor_q, first, entry);
	TAILQ_REMOVE(&fp_monitor_q, second, entry);
	free(first);
	free(second->CPagerWin);
	free(second);
	return 0;
}
