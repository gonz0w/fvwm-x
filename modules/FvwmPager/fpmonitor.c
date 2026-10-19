/* -*-c-*- */
/* This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.         See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see: <http://www.gnu.org/licenses/>
 */

#include "config.h"
#include "libs/monitor_types.h"
#include "libs/fvwmlib.h"
#include "libs/Module.h"
#include "libs/Strings.h"
#include "fvwm/fvwm.h"
#include "FvwmPager.h"
#include <stdint.h>

struct fpmonitor *fpmonitor_new(struct monitor *m)
{
	struct fpmonitor	*fp;

	fp = fxcalloc(1, sizeof(*fp));
	if (HilightDesks)
		fp->CPagerWin = fxcalloc(1, ndesks * sizeof(*fp->CPagerWin));
	else
		fp->CPagerWin = NULL;
	fp->m = m;
	/* Configuration acquisition may end before DesktopSize arrives. Keep a
	 * usable single-page desktop until a complete core record replaces it. */
	fp->scr_width = monitor_get_all_widths();
	fp->scr_height = monitor_get_all_heights();
	if (fp->scr_width <= 0)
		fp->scr_width = 1;
	if (fp->scr_height <= 0)
		fp->scr_height = 1;
	fp->virtual_scr.VxPages = 1;
	fp->virtual_scr.VyPages = 1;
	fp->virtual_scr.VWidth = fp->scr_width;
	fp->virtual_scr.VHeight = fp->scr_height;
	fp->disabled = false;
	TAILQ_INSERT_TAIL(&fp_monitor_q, fp, entry);

	return (fp);
}

struct fpmonitor *fpmonitor_this(struct monitor *m_find)
{
	struct monitor *m;
	struct fpmonitor *fp = NULL;

	if (m_find != NULL) {
		/* We've been asked to find a specific monitor. */
		m = m_find;
	} else if (monitor_to_track != NULL && !monitor_to_track->disabled) {
		return (monitor_to_track);
	} else {
		m = monitor_get_current();
	}

	if (m == NULL || m->flags & MONITOR_DISABLED)
		return (NULL);
	TAILQ_FOREACH(fp, &fp_monitor_q, entry) {
		if (fp->m == m && !fp->disabled)
			return (fp);
	}
	return (NULL);
}

struct fpmonitor *fpmonitor_by_name(const char *name)
{
	struct fpmonitor *fm;

	if (name == NULL || StrEquals(name, "none"))
		return (NULL);

	TAILQ_FOREACH(fm, &fp_monitor_q, entry) {
		if (fm->m != NULL && (strcmp(name, fm->m->si->name) == 0))
			return (fm);
	}
	return (NULL);
}

struct fpmonitor *fpmonitor_from_desk(int desk)
{
	struct fpmonitor *fp;

	TAILQ_FOREACH(fp, &fp_monitor_q, entry)
		if (!fp->disabled && fp->m->virtual_scr.CurrentDesk == desk)
			return fp;

	return NULL;
}

struct fpmonitor *fpmonitor_from_xy(int x, int y)
{
	struct fpmonitor *fp;

	if (monitor_to_track == NULL && monitor_mode != MONITOR_TRACKING_G) {
		int width = fpmonitor_get_all_widths();
		int height = fpmonitor_get_all_heights();
		if (width <= 0 || height <= 0)
			return NULL;
		x %= width;
		y %= height;

		TAILQ_FOREACH(fp, &fp_monitor_q, entry) {
			if (fp->disabled || fp->m->flags & MONITOR_DISABLED)
				continue;
			if (x >= fp->m->si->x &&
			    x < (int64_t)fp->m->si->x + fp->m->si->w &&
			    y >= fp->m->si->y &&
			    y < (int64_t)fp->m->si->y + fp->m->si->h)
				return fp;
		}
	} else {
		return fpmonitor_this(NULL);
	}

	return NULL;
}

struct fpmonitor *fpmonitor_from_n(int n)
{
	struct fpmonitor *fp;
	int c = 0;

	TAILQ_FOREACH(fp, &fp_monitor_q, entry) {
		if (fp->disabled)
			continue;
		if (c == n)
			return fp;
		c++;
	}
	return fp;
}

void fpmonitor_disable(struct fpmonitor *fp)
{
	int i;

	fp->disabled = true;
	if (fp->CPagerWin != NULL) {
		for (i = 0; i < ndesks; i++) {
			if (fp->CPagerWin[i] != None)
				XMoveWindow(dpy, fp->CPagerWin[i], -32768,-32768);
		}
	}

	if (fp == monitor_to_track) {
		/* Drop the disabled selection before asking for an active fallback.
		 * preferred_monitor remains available for later reconnection. */
		monitor_to_track = NULL;
		monitor_to_track = fpmonitor_this(NULL);
		if (monitor_to_track == NULL)
			monitor_to_track = fpmonitor_from_n(0);
	}
}

int fpmonitor_get_all_widths(void)
{
	struct fpmonitor *fp = TAILQ_FIRST(&fp_monitor_q);

	if (fp == NULL || fp->scr_width <= 0)
		return (monitor_get_all_widths());

	return (fp->scr_width);
}

int fpmonitor_get_all_heights(void)
{
	struct fpmonitor *fp = TAILQ_FIRST(&fp_monitor_q);

	if (fp == NULL || fp->scr_height <= 0)
		return (monitor_get_all_heights());
	return (fp->scr_height);
}

int fpmonitor_count(void)
{
	struct fpmonitor *fp;
	int c = 0;

	TAILQ_FOREACH(fp, &fp_monitor_q, entry)
		if (!fp->disabled)
			c++;
	return (c);
}
