/* -*-c-*- */
/* This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see: <http://www.gnu.org/licenses/>
 */

#include "config.h"

#include <X11/extensions/Xrandr.h>
#include <X11/extensions/randr.h>
#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <limits.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "FEvent.h"
#include "FScreen.h"
#include "Parse.h"
#include "PictureBase.h"
#include "defaults.h"
#include "fvwmlib.h"
#include "log.h"
#include "queue.h"
#include "strtonum.h"

#define GLOBAL_SCREEN_NAME "_global"

/* In fact, only corners matter -- there will never be GRAV_NONE */
enum
{
	GRAV_POS  = 0,
	GRAV_NONE = 1,
	GRAV_NEG  = 2
};
static int grav_matrix[3][3] = {
    {NorthWestGravity, NorthGravity, NorthEastGravity},
    {WestGravity, CenterGravity, EastGravity},
    {SouthWestGravity, SouthGravity, SouthEastGravity}};
#define DEFAULT_GRAVITY NorthWestGravity

static Display			 *disp;
static bool			  is_randr_present;
static bool			  randr_initialised;
static monitor_topology_snapshot *committed_topology;

typedef struct
{
	struct monitors			monitors;
	const fscreen_legacy_allocator *allocator;
} fscreen_legacy_stage;

static struct monitor *
monitor_new(const char *);
static struct monitor *
monitor_by_name(const char *);
static void
monitor_set_topology_coords(struct monitor *, const monitor_topology_monitor *);
static void
monitor_assign_number(struct monitors *);
static bool
legacy_stage_prepare(const monitor_topology_snapshot *, struct monitors *,
    const fscreen_legacy_allocator *, fscreen_legacy_stage *);
static void
legacy_stage_destroy(fscreen_legacy_stage *);
static void
legacy_reconcile_apply(const monitor_topology_snapshot *, struct monitors *,
    struct screen_infos *, fscreen_legacy_stage *, bool);

enum monitor_tracking  monitor_mode;
bool		       is_tracking_shared;
struct screen_infos    screen_info_q, screen_info_q_temp;
struct monitors	       monitor_q;
int		       randr_event;
const char	      *prev_focused_monitor;
static struct monitor *monitor_global = NULL;

RB_GENERATE(monitors, monitor, entry, monitor_compare);

static void
GetMouseXY(XEvent *eventp, int *x, int *y)
{
	XEvent e;

	if (eventp == NULL) {
		eventp = &e;
		e.type = 0;
	}
	fev_get_evpos_or_query(disp, DefaultRootWindow(disp), eventp, x, y);
}

static struct monitor *
monitor_new(const char *name)
{
	struct monitor *m;

	m	    = fxcalloc(1, sizeof *m);
	m->flags    = 0;
	m->si	    = screen_info_new();
	m->si->name = fxstrdup(name);

	return (m);
}

static void
monitor_scan_edges(struct monitors *legacy_monitors, struct monitor *m)
{
	struct monitor *m_loop;
	int64_t		m_right;
	int64_t		m_bottom;

	if (m == NULL)
		return;

	m->edge.top = m->edge.bottom = m->edge.left = m->edge.right =
	    MONITOR_OUTSIDE_EDGE;
	if (m->flags & MONITOR_DISABLED)
		return;
	m_right	 = (int64_t)m->si->x + m->si->w;
	m_bottom = (int64_t)m->si->y + m->si->h;

	RB_FOREACH(m_loop, monitors, legacy_monitors)
	{
		int64_t loop_right;
		int64_t loop_bottom;
		bool	horizontal_overlap;
		bool	vertical_overlap;

		if (m_loop == m || (m_loop->flags & MONITOR_DISABLED))
			continue;
		loop_right  = (int64_t)m_loop->si->x + m_loop->si->w;
		loop_bottom = (int64_t)m_loop->si->y + m_loop->si->h;
		horizontal_overlap =
		    m->si->x < loop_right && m_loop->si->x < m_right;
		vertical_overlap =
		    m->si->y < loop_bottom && m_loop->si->y < m_bottom;

		/* Top Edge */
		if (m->si->y == loop_bottom && horizontal_overlap)
			m->edge.top = MONITOR_INSIDE_EDGE;
		/* Bottom Edge */
		if (m_bottom == m_loop->si->y && horizontal_overlap)
			m->edge.bottom = MONITOR_INSIDE_EDGE;
		/* Left Edge */
		if (m->si->x == loop_right && vertical_overlap)
			m->edge.left = MONITOR_INSIDE_EDGE;
		/* Right Edge */
		if (m_right == m_loop->si->x && vertical_overlap)
			m->edge.right = MONITOR_INSIDE_EDGE;
	}
}

struct monitor *
monitor_by_number(int n)
{
	struct monitor *m_loop;

	RB_FOREACH(m_loop, monitors, &monitor_q)
	{
		if (m_loop->number == n)
			return (m_loop);
	}
	return (NULL);
}

void
monitor_refresh_global(void)
{
	/* Creating the global monitor outside of the monitor_q structure
	 * allows for only the relevant information to be presented to callers
	 * which are asking for a global screen.
	 *
	 * This structure therefore only uses some of the fields:
	 *
	 *   si->name,
	 *   si->{x, y, w, h}
	 */
	if (monitor_global == NULL)
		monitor_global = monitor_new(GLOBAL_SCREEN_NAME);

	/* At this point, the global screen has been initialised.  Refresh the
	 * coordinate list.
	 */
	monitor_global->si->x = 0;
	monitor_global->si->y = 0;
	monitor_global->si->w = monitor_get_all_widths();
	monitor_global->si->h = monitor_get_all_heights();
}

struct screen_info *
screen_info_new(void)
{
	struct screen_info *si;

	si = fxcalloc(1, sizeof *si);

	return (si);
}

struct screen_info *
screen_info_by_name(const char *name)
{
	struct screen_info *si;

	TAILQ_FOREACH(si, &screen_info_q, entry)
	{
		if (strcmp(si->name, name) == 0)
			return (si);
	}

	return (NULL);
}

struct monitor *
monitor_get_global(void)
{
	monitor_refresh_global();

	return monitor_global;
}

struct monitor *
monitor_get_prev(void)
{
	struct monitor *m, *mret = NULL;
	struct monitor *this = monitor_get_current();

	RB_FOREACH(m, monitors, &monitor_q)
	{
		if (m == this)
			continue;
		if (m->is_prev) {
			mret = m;
			break;
		}
	}

	/* Can be NULL -- is checked in expand.c */
	return (mret);
}

struct monitor *
monitor_get_current(void)
{
	int		   JunkX = 0, JunkY = 0, x, y;
	Window		   JunkRoot, JunkChild;
	unsigned int	   JunkMask;
	struct monitor	  *mon;
	static const char *cur_mon;

	prev_focused_monitor = cur_mon;

	FQueryPointer(disp, DefaultRootWindow(disp), &JunkRoot, &JunkChild,
	    &JunkX, &JunkY, &x, &y, &JunkMask);

	mon = FindScreenOfXY(x, y);

	if (mon != NULL)
		cur_mon = mon->si->name;

	return (mon);
}

struct monitor *
monitor_resolve_name(const char *scr)
{
	struct monitor *m   = NULL;
	int		pos = -1;
	const char     *errstr;

	if (scr == NULL) {
		return NULL;
	}

	/* Try and look up the monitor as a number.  If that succeeds,
	 * try and match that number as something valid in the monitor
	 * information we have.
	 */
	pos = strtonum(scr, 0, INT_MAX, &errstr);
	if (errstr == NULL)
		return (monitor_by_number(pos));

	/* "@g" is for the global screen. */
	if (strcmp(scr, "g") == 0) {
		m = monitor_get_global();
	}
	/* "@c" is for the current screen. */
	else if (strcmp(scr, "c") == 0)
		m = monitor_get_current();
	/* "@p" is for the primary screen. */
	else if (strcmp(scr, "p") == 0)
		m = monitor_by_primary();
	else
		/* Assume the monitor name is a literal RandR name (such as
		 * HDMI2). */
		m = monitor_by_name(scr);

	return (m);
}

static struct monitor *
monitor_by_name(const char *name)
{
	struct monitor *m, *mret;

	if (name == NULL) {
		fvwm_debug(__func__,
		    "%s: name is NULL; shouldn't happen.  "
		    "Returning current monitor\n",
		    __func__);
		return (monitor_get_current());
	}

	mret = NULL;
	RB_FOREACH(m, monitors, &monitor_q)
	{
		if (strcmp(m->si->name, name) == 0) {
			mret = m;
			break;
		}
	}

	return (mret);
}

static bool
legacy_identity_matches(
    const struct monitor *monitor, const monitor_topology_monitor *fact)
{
	if (monitor == NULL || monitor->si == NULL || fact == NULL) {
		return false;
	}
	if (monitor->si->name_atom != None) {
		return monitor->si->name_atom == fact->name_atom
		       && strcmp(monitor->si->name, fact->name) == 0;
	}
	return strcmp(monitor->si->name, fact->name) == 0;
}

static struct monitor *
legacy_monitor_by_identity(
    struct monitors *legacy_monitors, const monitor_topology_monitor *fact)
{
	struct monitor *monitor;

	RB_FOREACH(monitor, monitors, legacy_monitors)
	{
		if (legacy_identity_matches(monitor, fact)) {
			return monitor;
		}
	}
	return NULL;
}

struct monitor *
fscreen_monitor_by_output(const monitor_topology_snapshot *snapshot,
    struct monitors *legacy_monitors, RROutput output)
{
	int monitor_index;
	int output_index;

	if (snapshot == NULL || legacy_monitors == NULL) {
		return NULL;
	}
	for (monitor_index = 0; monitor_index < snapshot->monitor_count;
	    monitor_index++) {
		const monitor_topology_monitor *fact =
		    &snapshot->monitors[monitor_index];

		for (output_index = 0; output_index < fact->output_count;
		    output_index++) {
			if (fact->outputs[output_index] == output) {
				return legacy_monitor_by_identity(
				    legacy_monitors, fact);
			}
		}
	}
	return RB_MIN(monitors, legacy_monitors);
}

static bool
topology_has_output(const monitor_topology_snapshot *snapshot, RROutput output)
{
	int monitor_index;
	int output_index;

	if (snapshot == NULL) {
		return false;
	}
	for (monitor_index = 0; monitor_index < snapshot->monitor_count;
	    monitor_index++) {
		const monitor_topology_monitor *fact =
		    &snapshot->monitors[monitor_index];

		for (output_index = 0; output_index < fact->output_count;
		    output_index++) {
			if (fact->outputs[output_index] == output) {
				return true;
			}
		}
	}
	return false;
}

struct monitor *
monitor_by_output(int output)
{
	struct monitor *monitor = fscreen_monitor_by_output(
	    committed_topology, &monitor_q, (RROutput)output);

	if (!topology_has_output(committed_topology, (RROutput)output)) {
		fvwm_debug(__func__,
		    "%s: couldn't find monitor with output '%d', "
		    "returning first output.\n",
		    __func__, output);
	}
	return monitor;
}

struct monitor *
monitor_by_primary(void)
{
	struct monitor *m = NULL, *m_loop;

	RB_FOREACH(m_loop, monitors, &monitor_q)
	{
		if (m_loop->flags & MONITOR_PRIMARY) {
			m = m_loop;
			break;
		}
	}

	return (m);
}

struct monitor *
monitor_by_last_primary(void)
{
	struct monitor *m = NULL, *m_loop;

	RB_FOREACH(m_loop, monitors, &monitor_q)
	{
		if (m_loop->was_primary && !(m_loop->flags & MONITOR_PRIMARY)) {
			m = m_loop;
			break;
		}
	}

	return (m);
}

static void
monitor_check_primary(struct monitors *legacy_monitors)
{
	struct monitor *monitor;

	RB_FOREACH(monitor, monitors, legacy_monitors)
	{
		if (!(monitor->flags & MONITOR_DISABLED)
		    && (monitor->flags & MONITOR_PRIMARY)) {
			return;
		}
	}
	monitor = RB_MIN(monitors, legacy_monitors);
	if (monitor != NULL && !(monitor->flags & MONITOR_DISABLED)) {
		monitor->flags |= MONITOR_PRIMARY;
	}
}

int
monitor_get_all_widths(void)
{
	return (DisplayWidth(disp, DefaultScreen(disp)));
}

int
monitor_get_all_heights(void)
{
	return (DisplayHeight(disp, DefaultScreen(disp)));
}

void
monitor_assign_virtual(struct monitor *ref)
{
	struct monitor *m;

	if (monitor_mode == MONITOR_TRACKING_M || is_tracking_shared)
		return;

	RB_FOREACH(m, monitors, &monitor_q)
	{
		if (m == ref)
			continue;

		memcpy(
		    &m->virtual_scr, &ref->virtual_scr, sizeof(m->virtual_scr));
	}
}

void
FScreenSelect(Display *dpy)
{
	XRRSelectInput(dpy, DefaultRootWindow(dpy),
	    RRScreenChangeNotifyMask | RRCrtcChangeNotifyMask
		| RROutputChangeNotifyMask | RROutputPropertyNotifyMask
		| RRResourceChangeNotifyMask);
}

static void *
topology_malloc(void *context, size_t size)
{
	return malloc(size);
}

static void *
topology_calloc(void *context, size_t count, size_t size)
{
	return calloc(count, size);
}

static void
topology_free(void *context, void *pointer)
{
	free(pointer);
}

static XRRMonitorInfo *
randr_get_monitors(
    void *context, Display *dpy, Window root, Bool active, int *count)
{
	return XRRGetMonitors(dpy, root, active, count);
}

static char *
randr_get_atom_name(void *context, Display *dpy, Atom atom)
{
	return XGetAtomName(dpy, atom);
}

static void
randr_free_atom_name(void *context, char *name)
{
	XFree(name);
}

static void
randr_free_monitors(void *context, XRRMonitorInfo *monitors)
{
	XRRFreeMonitors(monitors);
}

static const fscreen_randr_adapter real_randr_adapter = {
    .context	    = NULL,
    .get_monitors   = randr_get_monitors,
    .get_atom_name  = randr_get_atom_name,
    .free_atom_name = randr_free_atom_name,
    .free_monitors  = randr_free_monitors,
    .topology_allocator =
	{
	    .context   = NULL,
	    .malloc_fn = topology_malloc,
	    .calloc_fn = topology_calloc,
	    .free_fn   = topology_free,
	},
};

static struct monitor *
legacy_allocate_monitor(void *context)
{
	return calloc(1, sizeof(struct monitor));
}

static struct screen_info *
legacy_allocate_screen(void *context)
{
	return calloc(1, sizeof(struct screen_info));
}

static char *
legacy_copy_name(void *context, const char *source)
{
	size_t size = strlen(source) + 1;
	char  *copy = malloc(size);

	if (copy != NULL) {
		memcpy(copy, source, size);
	}
	return copy;
}

static void
legacy_free_monitor(void *context, struct monitor *monitor)
{
	free(monitor);
}

static void
legacy_free_screen(void *context, struct screen_info *screen)
{
	free(screen);
}

static void
legacy_free_name(void *context, char *name)
{
	free(name);
}

static const fscreen_legacy_allocator real_legacy_allocator = {
    .context	      = NULL,
    .allocate_monitor = legacy_allocate_monitor,
    .allocate_screen  = legacy_allocate_screen,
    .copy_name	      = legacy_copy_name,
    .free_monitor     = legacy_free_monitor,
    .free_screen      = legacy_free_screen,
    .free_name	      = legacy_free_name,
};

const monitor_topology_snapshot *
FScreenGetTopologySnapshot(void)
{
	return committed_topology;
}

fscreen_topology_refresh_result
fscreen_refresh_topology_with_adapters(Display *dpy, Window root,
    monitor_topology_snapshot **committed, struct monitors *legacy_monitors,
    struct screen_infos		   *legacy_screens,
    const fscreen_randr_adapter	   *randr_adapter,
    const fscreen_legacy_allocator *legacy_allocator)
{
	monitor_topology_monitor       *source_monitors = NULL;
	monitor_topology_snapshot      *candidate	= NULL;
	monitor_topology_publish_result publish_result;
	XRRMonitorInfo		       *randr_monitors = NULL;
	int				monitor_count  = -1;
	int				i;
	bool				initial;
	bool				needs_reconcile;
	fscreen_legacy_stage		stage;

	RB_INIT(&stage.monitors);
	stage.allocator = NULL;
	if (committed == NULL || randr_adapter == NULL
	    || randr_adapter->get_monitors == NULL
	    || randr_adapter->get_atom_name == NULL
	    || randr_adapter->free_atom_name == NULL
	    || randr_adapter->free_monitors == NULL
	    || randr_adapter->topology_allocator.malloc_fn == NULL
	    || randr_adapter->topology_allocator.calloc_fn == NULL
	    || randr_adapter->topology_allocator.free_fn == NULL) {
		fvwm_debug(__func__, "invalid topology adapter");
		return FSCREEN_TOPOLOGY_REFRESH_FAILED;
	}
	initial	       = *committed == NULL;
	randr_monitors = randr_adapter->get_monitors(
	    randr_adapter->context, dpy, root, True, &monitor_count);
	if (randr_monitors == NULL || monitor_count <= 0
	    || (size_t)monitor_count > SIZE_MAX / sizeof(*source_monitors)) {
		fvwm_debug(__func__,
		    monitor_count == -1
			? "RandR monitor query failed; retaining last topology"
			: "RandR returned no valid active topology; retaining "
			  "last topology");
		goto fail;
	}
	source_monitors = randr_adapter->topology_allocator.calloc_fn(
	    randr_adapter->topology_allocator.context, (size_t)monitor_count,
	    sizeof(*source_monitors));
	if (source_monitors == NULL) {
		fvwm_debug(__func__, "could not allocate topology source "
				     "facts; retaining last topology");
		goto fail;
	}
	for (i = 0; i < monitor_count; i++) {
		XRRMonitorInfo		 *source = &randr_monitors[i];
		monitor_topology_monitor *target = &source_monitors[i];

		if (source->name == None || source->noutput < 0
		    || (source->noutput > 0 && source->outputs == NULL)) {
			fvwm_debug(__func__,
			    "RandR monitor facts are incomplete; "
			    "retaining last topology");
			goto fail;
		}
		target->name = randr_adapter->get_atom_name(
		    randr_adapter->context, dpy, source->name);
		if (target->name == NULL) {
			fvwm_debug(__func__, "could not resolve RandR monitor "
					     "name; retaining last topology");
			goto fail;
		}
		target->name_atom    = source->name;
		target->x	     = source->x;
		target->y	     = source->y;
		target->width	     = source->width;
		target->height	     = source->height;
		target->mm_width     = source->mwidth;
		target->mm_height    = source->mheight;
		target->primary	     = source->primary ? True : False;
		target->outputs	     = source->outputs;
		target->output_count = source->noutput;
	}
	candidate = monitor_topology_snapshot_create(
	    source_monitors, monitor_count, &randr_adapter->topology_allocator);
	if (candidate == NULL) {
		fvwm_debug(__func__, "topology copy or validation failed; "
				     "retaining last topology");
		goto fail;
	}
	for (i = 0; i < monitor_count; i++) {
		randr_adapter->free_atom_name(
		    randr_adapter->context, source_monitors[i].name);
	}
	randr_adapter->topology_allocator.free_fn(
	    randr_adapter->topology_allocator.context, source_monitors);
	randr_adapter->free_monitors(randr_adapter->context, randr_monitors);
	needs_reconcile =
	    initial || !monitor_topology_snapshot_equal(*committed, candidate);
	if (needs_reconcile
	    && (legacy_monitors != NULL || legacy_screens != NULL)) {
		if (legacy_monitors == NULL || legacy_screens == NULL
		    || !legacy_stage_prepare(
			candidate, legacy_monitors, legacy_allocator, &stage)) {
			fvwm_debug(__func__, "legacy topology staging failed; "
					     "retaining last topology");
			monitor_topology_snapshot_destroy(candidate);
			legacy_stage_destroy(&stage);
			return FSCREEN_TOPOLOGY_REFRESH_FAILED;
		}
	}

	publish_result =
	    monitor_topology_snapshot_publish(committed, candidate);
	if (publish_result == MONITOR_TOPOLOGY_PUBLISH_INITIAL
	    || publish_result == MONITOR_TOPOLOGY_PUBLISH_CHANGED) {
		if (legacy_monitors != NULL) {
			legacy_reconcile_apply(*committed, legacy_monitors,
			    legacy_screens, &stage, initial);
		}
		legacy_stage_destroy(&stage);
		return FSCREEN_TOPOLOGY_REFRESH_CHANGED;
	}
	if (publish_result == MONITOR_TOPOLOGY_PUBLISH_UNCHANGED) {
		legacy_stage_destroy(&stage);
		return FSCREEN_TOPOLOGY_REFRESH_UNCHANGED;
	}
	legacy_stage_destroy(&stage);
	fvwm_debug(
	    __func__, "topology publication failed; retaining last topology");
	return FSCREEN_TOPOLOGY_REFRESH_FAILED;

fail:
	if (source_monitors != NULL) {
		for (i = 0; i < monitor_count; i++) {
			if (source_monitors[i].name != NULL) {
				randr_adapter->free_atom_name(
				    randr_adapter->context,
				    source_monitors[i].name);
			}
		}
		randr_adapter->topology_allocator.free_fn(
		    randr_adapter->topology_allocator.context, source_monitors);
	}
	if (randr_monitors != NULL) {
		randr_adapter->free_monitors(
		    randr_adapter->context, randr_monitors);
	}
	return FSCREEN_TOPOLOGY_REFRESH_FAILED;
}

fscreen_topology_refresh_result
FScreenRefreshTopology(Display *dpy)
{
	return fscreen_refresh_topology_with_adapters(dpy,
	    RootWindow(dpy, DefaultScreen(dpy)), &committed_topology,
	    &monitor_q, &screen_info_q, &real_randr_adapter,
	    &real_legacy_allocator);
}

void
monitor_output_change(Display *dpy, XRRScreenChangeNotifyEvent *e)
{
	(void)FScreenRefreshTopology(dpy);
}

static void
monitor_set_topology_coords(
    struct monitor *m, const monitor_topology_monitor *topology_monitor)
{
	if (m == NULL || topology_monitor == NULL)
		return;

	m->si->x	 = topology_monitor->x;
	m->si->y	 = topology_monitor->y;
	m->si->w	 = topology_monitor->width;
	m->si->h	 = topology_monitor->height;
	m->si->rr_output = topology_monitor->output_count > 0
			       ? topology_monitor->outputs[0]
			       : None;
	if (topology_monitor->primary) {
		m->flags |= MONITOR_PRIMARY;
		m->was_primary = false;
	} else {
		if (m->flags & MONITOR_PRIMARY) {
			m->flags &= ~MONITOR_PRIMARY;
			m->was_primary = true;
		}
	}
}

static void
monitor_assign_number(struct monitors *legacy_monitors)
{
	struct monitor *m;
	int		n = 0;

	RB_FOREACH(m, monitors, legacy_monitors)
	{
		if (m->flags & MONITOR_DISABLED)
			continue;
		m->number = n++;
	}
}

int
monitor_compare(struct monitor *a, struct monitor *b)
{
	int adisable = a->flags & MONITOR_DISABLED;
	int bdisable = b->flags & MONITOR_DISABLED;
	int coord_comp;
	int size_comp;

	if (adisable && !bdisable)
		return 1;

	if (!adisable && bdisable)
		return -1;

	coord_comp = a->si->y == b->si->y ? (a->si->x < b->si->x      ? -1
						: a->si->x > b->si->x ? 1
								      : 0)
					  : (a->si->y < b->si->y ? -1 : 1);

	if (coord_comp != 0)
		return coord_comp;

	size_comp = a->si->w == b->si->w ? (a->si->h < b->si->h	     ? -1
					       : a->si->h > b->si->h ? 1
								     : 0)
					 : (a->si->w < b->si->w ? -1 : 1);

	if (size_comp != 0)
		return size_comp;

	return strcmp(a->si->name, b->si->name);
}

int64_t
fscreen_monitor_distance_to_point(const struct monitor *monitor, int x, int y)
{
	int64_t left	 = monitor->si->x;
	int64_t top	 = monitor->si->y;
	int64_t right	 = left + monitor->si->w;
	int64_t bottom	 = top + monitor->si->h;
	int64_t point_x	 = x;
	int64_t point_y	 = y;
	int64_t distance = 0;

	if (point_x < left)
		distance += left - point_x;
	else if (point_x > right)
		distance += point_x - right;
	if (point_y < top)
		distance += top - point_y;
	else if (point_y > bottom)
		distance += point_y - bottom;
	return distance;
}

static bool
legacy_allocator_is_valid(const fscreen_legacy_allocator *allocator)
{
	return allocator != NULL && allocator->allocate_monitor != NULL
	       && allocator->allocate_screen != NULL
	       && allocator->copy_name != NULL
	       && allocator->free_monitor != NULL
	       && allocator->free_screen != NULL
	       && allocator->free_name != NULL;
}

static void
legacy_monitor_initialize(
    struct monitor *monitor, const monitor_topology_monitor *fact)
{
	monitor->flags	       = MONITOR_NEW;
	monitor->si->name_atom = fact->name_atom;
	monitor_set_topology_coords(monitor, fact);
	monitor->virtual_scr.edge_thickness	 = 2;
	monitor->virtual_scr.last_edge_thickness = 2;
}

static void
legacy_stage_destroy(fscreen_legacy_stage *stage)
{
	struct monitor *monitor;
	struct monitor *next;

	if (stage == NULL || stage->allocator == NULL) {
		return;
	}
	RB_FOREACH_SAFE(monitor, monitors, &stage->monitors, next)
	{
		RB_REMOVE(monitors, &stage->monitors, monitor);
		stage->allocator->free_name(
		    stage->allocator->context, (char *)monitor->si->name);
		stage->allocator->free_screen(
		    stage->allocator->context, monitor->si);
		stage->allocator->free_monitor(
		    stage->allocator->context, monitor);
	}
	stage->allocator = NULL;
	RB_INIT(&stage->monitors);
}

static bool
legacy_stage_prepare(const monitor_topology_snapshot *snapshot,
    struct monitors *legacy_monitors, const fscreen_legacy_allocator *allocator,
    fscreen_legacy_stage *stage)
{
	int i;

	if (snapshot == NULL || legacy_monitors == NULL || stage == NULL
	    || !legacy_allocator_is_valid(allocator)) {
		return false;
	}
	RB_INIT(&stage->monitors);
	stage->allocator = allocator;
	for (i = 0; i < snapshot->monitor_count; i++) {
		const monitor_topology_monitor *fact = &snapshot->monitors[i];
		struct monitor		       *monitor;
		struct screen_info	       *screen = NULL;
		char			       *name   = NULL;

		if (legacy_monitor_by_identity(legacy_monitors, fact) != NULL) {
			continue;
		}
		monitor = allocator->allocate_monitor(allocator->context);
		if (monitor != NULL) {
			screen = allocator->allocate_screen(allocator->context);
		}
		if (screen != NULL) {
			name = allocator->copy_name(
			    allocator->context, fact->name);
		}
		if (monitor == NULL || screen == NULL || name == NULL) {
			allocator->free_name(allocator->context, name);
			allocator->free_screen(allocator->context, screen);
			allocator->free_monitor(allocator->context, monitor);
			legacy_stage_destroy(stage);
			return false;
		}
		monitor->si	  = screen;
		monitor->si->name = name;
		legacy_monitor_initialize(monitor, fact);
		if (RB_INSERT(monitors, &stage->monitors, monitor) != NULL) {
			allocator->free_name(allocator->context, name);
			allocator->free_screen(allocator->context, screen);
			allocator->free_monitor(allocator->context, monitor);
			legacy_stage_destroy(stage);
			return false;
		}
	}
	return true;
}

static bool
legacy_monitor_changed(
    const struct monitor *monitor, const monitor_topology_monitor *fact)
{
	RROutput first_output =
	    fact->output_count > 0 ? fact->outputs[0] : None;

	return monitor->si->x != fact->x || monitor->si->y != fact->y
	       || monitor->si->w != fact->width
	       || monitor->si->h != fact->height
	       || monitor->si->rr_output != first_output
	       || ((monitor->flags & MONITOR_PRIMARY) != 0)
		      != (fact->primary != False);
}

static void
legacy_reconcile_apply(const monitor_topology_snapshot *snapshot,
    struct monitors *legacy_monitors, struct screen_infos *legacy_screens,
    fscreen_legacy_stage *stage, bool initial)
{
	struct monitor *monitor;
	struct monitor *next;
	int		i;

	RB_FOREACH(monitor, monitors, legacy_monitors)
	{
		monitor->emit = 0;
		monitor->flags &=
		    ~(MONITOR_NEW | MONITOR_CHANGED | MONITOR_FOUND);
	}
	for (i = 0; i < snapshot->monitor_count; i++) {
		const monitor_topology_monitor *fact = &snapshot->monitors[i];
		bool				changed;
		bool				was_disabled;

		monitor = legacy_monitor_by_identity(legacy_monitors, fact);
		if (monitor == NULL) {
			monitor =
			    legacy_monitor_by_identity(&stage->monitors, fact);
			RB_REMOVE(monitors, &stage->monitors, monitor);
			monitor->flags |= MONITOR_FOUND;
			monitor->emit = initial ? 0 : MONITOR_ENABLED;
			TAILQ_INSERT_TAIL(legacy_screens, monitor->si, entry);
			RB_INSERT(monitors, legacy_monitors, monitor);
			fvwm_debug(__func__, "Added new monitor: %s (%p)",
			    monitor->si->name, monitor);
			continue;
		}
		changed	     = legacy_monitor_changed(monitor, fact);
		was_disabled = (monitor->flags & MONITOR_DISABLED) != 0;
		RB_REMOVE(monitors, legacy_monitors, monitor);
		monitor->flags &=
		    ~(MONITOR_NEW | MONITOR_CHANGED | MONITOR_DISABLED);
		monitor->flags |= MONITOR_FOUND;
		monitor->si->name_atom = fact->name_atom;
		monitor_set_topology_coords(monitor, fact);
		if (was_disabled) {
			monitor->emit = MONITOR_ENABLED;
		} else if (changed) {
			monitor->flags |= MONITOR_CHANGED;
			monitor->emit = MONITOR_CHANGED;
		} else {
			monitor->emit = 0;
		}
		RB_INSERT(monitors, legacy_monitors, monitor);
	}
	RB_FOREACH_SAFE(monitor, monitors, legacy_monitors, next)
	{
		if (monitor->flags & MONITOR_FOUND) {
			monitor->flags &= ~MONITOR_FOUND;
			continue;
		}
		if (!(monitor->flags & MONITOR_DISABLED)) {
			RB_REMOVE(monitors, legacy_monitors, monitor);
			if (monitor->flags & MONITOR_PRIMARY) {
				monitor->was_primary = true;
			}
			monitor->flags |= MONITOR_DISABLED;
			monitor->flags &= ~MONITOR_PRIMARY;
			monitor->emit = MONITOR_DISABLED;
			RB_INSERT(monitors, legacy_monitors, monitor);
		}
	}
	monitor_assign_number(legacy_monitors);
	monitor_check_primary(legacy_monitors);
	RB_FOREACH(monitor, monitors, legacy_monitors)
	{
		monitor_scan_edges(legacy_monitors, monitor);
	}
}

void
FScreenInit(Display *dpy)
{
	XRRScreenResources *res = NULL;
	struct monitor	   *m;
	int		    err_base = 0, major, minor;

	if (randr_initialised)
		return;

	disp		  = dpy;
	randr_event	  = 0;
	is_randr_present  = false;
	randr_initialised = false;

	if (RB_EMPTY(&monitor_q))
		RB_INIT(&monitor_q);

	if (TAILQ_EMPTY(&screen_info_q))
		TAILQ_INIT(&screen_info_q);

	if (!XRRQueryExtension(dpy, &randr_event, &err_base)
	    || !XRRQueryVersion(dpy, &major, &minor)) {
		fvwm_debug(__func__, "RandR not present");
		goto randr_fail;
	}

	if (major > 1 || (major == 1 && minor >= 5))
		is_randr_present = true;

	if (!is_randr_present) {
		/* Something went wrong. */
		fvwm_debug(__func__, "Couldn't initialise XRandR: %s\n",
		    strerror(errno));
		goto randr_fail;
	}

	fvwm_debug(__func__, "Using RandR %d.%d\n", major, minor);

	/* XRandR is present, so query the screens we have. */
	res = XRRGetScreenResources(dpy, DefaultRootWindow(dpy));

	if (res == NULL || (res != NULL && res->noutput == 0)) {
		XRRFreeScreenResources(res);
		fvwm_debug(__func__, "RandR present, yet no outputs found.");
		goto randr_fail;
	}
	XRRFreeScreenResources(res);

	if (FScreenRefreshTopology(dpy) != FSCREEN_TOPOLOGY_REFRESH_CHANGED
	    || committed_topology == NULL || committed_topology->epoch != 1) {
		fvwm_debug(__func__,
		    "Unable to commit initial RandR monitor topology");
		goto randr_fail;
	}
	randr_initialised  = true;
	is_tracking_shared = false;

	RB_FOREACH(m, monitors, &monitor_q)
	monitor_scan_edges(&monitor_q, m);

	return;

randr_fail:
	fprintf(stderr, "Unable to initialise RandR\n");
	exit(101);
}

void
monitor_refresh_module(Display *dpy)
{
	struct monitor *m = NULL;

	RB_FOREACH(m, monitors, &monitor_q)
	m->flags &= ~MONITOR_NEW;

	(void)FScreenRefreshTopology(dpy);

	RB_FOREACH(m, monitors, &monitor_q)
	{
		if (m->flags & MONITOR_NEW)
			fprintf(stderr, "MON: %s is NEW...\n", m->si->name);
	}
}

void
monitor_dump_state(struct monitor *m)
{
	struct monitor *mcur, *m2;

	mcur = monitor_get_current();

	fvwm_debug(__func__, "Monitor Debug\n");
	fvwm_debug(__func__, "\tnumber of outputs: %d\n", monitor_get_count());
	RB_FOREACH(m2, monitors, &monitor_q)
	{
		if (m2 == NULL) {
			fvwm_debug(
			    __func__, "monitor in list is NULL.  Bug!\n");
			continue;
		}
		if (m != NULL && m2 != m)
			continue;
		fvwm_debug(__func__,
		    "\tNumber:\t%d\n"
		    "\tName:\t%s\n"
		    "\tDisabled:\t%s\n"
		    "\tIs Primary:\t%s\n"
		    "\tIs Current:\t%s\n"
		    "\tIs Previous:\t%s\n"
		    "\tOutput:\t%d\n"
		    "\tCoords:\t{x: %d, y: %d, w: %d, h: %d}\n"
		    "\tVirtScr: {\n"
		    "\t\tVxMax: %d, VyMax: %d, Vx: %d, Vy: %d\n"
		    "\t\tEdgeScrollX: %d, EdgeScrollY: %d\n"
		    "\t\tCurrentDesk: %d\n"
		    "\t\tCurrentPage: {x: %d, y: %d}\n"
		    "\t\tMyDisplayWidth: %d, MyDisplayHeight: %d\n\t}\n"
		    "\tEWMH: {\n"
		    "\t\tBaseStrut Top:    %d\n"
		    "\t\tBaseStrut Bottom: %d\n"
		    "\t\tBaseStrut Left:   %d\n"
		    "\t\tBaseStrut Right:  %d\n\t}\n"
		    "\tDesktops:\t%s\n"
		    "\tFlags:%s\n\n",
		    m2->number, m2->si->name,
		    (m2->flags & MONITOR_DISABLED) ? "true" : "false",
		    (m2->flags & MONITOR_PRIMARY) ? "yes" : "no",
		    (mcur && m2 == mcur) ? "yes" : "no",
		    m2->is_prev ? "yes" : "no", (int)m2->si->rr_output,
		    m2->si->x, m2->si->y, m2->si->w, m2->si->h,
		    m2->virtual_scr.VxMax, m2->virtual_scr.VyMax,
		    m2->virtual_scr.Vx, m2->virtual_scr.Vy,
		    m2->virtual_scr.EdgeScrollX, m2->virtual_scr.EdgeScrollY,
		    m2->virtual_scr.CurrentDesk,
		    (int)(m2->virtual_scr.Vx / monitor_get_all_widths()),
		    (int)(m2->virtual_scr.Vy / monitor_get_all_heights()),
		    monitor_get_all_widths(), monitor_get_all_heights(),
		    m2->ewmhc.BaseStrut.top, m2->ewmhc.BaseStrut.bottom,
		    m2->ewmhc.BaseStrut.left, m2->ewmhc.BaseStrut.right,
		    m2->Desktops ? "yes" : "no",
		    monitor_mode == MONITOR_TRACKING_G	 ? "global"
		    : monitor_mode == MONITOR_TRACKING_M ? "per-monitor"
							 : "Unknown");
	}
}

int
monitor_get_count(void)
{
	struct monitor *m = NULL;
	int		c = 0;

	if (RB_EMPTY(&monitor_q))
		return c;

	RB_FOREACH(m, monitors, &monitor_q)
	{
		if (m->flags & MONITOR_DISABLED)
			continue;
		c++;
	}
	return (c);
}

struct monitor *
FindScreenOfXY(int x, int y)
{
	struct monitor *m, *m_min = NULL;
	int64_t		dmin	    = INT64_MAX;
	int		all_widths  = monitor_get_all_widths();
	int		all_heights = monitor_get_all_heights();

	x %= all_widths;
	y %= all_heights;
	if (x < 0)
		x += all_widths;
	if (y < 0)
		y += all_heights;

	int nr_monitors = monitor_get_count();

	RB_FOREACH(m, monitors, &monitor_q)
	{
		/* If we have more than one screen configured, then don't match
		 * on the global screen, as that's separate to XY positioning
		 * which is only concerned with the *specific* screen.
		 */
		if (nr_monitors > 0
		    && strcmp(m->si->name, GLOBAL_SCREEN_NAME) == 0)
			continue;
		if (m->flags & MONITOR_DISABLED)
			continue;
		if (x >= m->si->x && x < m->si->x + m->si->w && y >= m->si->y
		    && y < m->si->y + m->si->h)
			return (m);
	}

	/* No monitor found, point is in a dead area.
	 * Find closest monitor using the taxi cab metric.
	 */
	RB_FOREACH(m, monitors, &monitor_q)
	{
		int64_t d;

		/* Skip global screen */
		if (nr_monitors > 0
		    && strcmp(m->si->name, GLOBAL_SCREEN_NAME) == 0)
			continue;

		if (m->flags & MONITOR_DISABLED)
			continue;

		d = fscreen_monitor_distance_to_point(m, x, y);
		if (d < dmin) {
			dmin  = d;
			m_min = m;
		}
	}

	/* Shouldn't happen. */
	if (m_min == NULL) {
		fvwm_debug(__func__, "Couldn't find any monitor");
		exit(106);
	}
	return (m_min);
}

/* Returns the primary screen if arg.name is NULL. */
static struct monitor *
FindScreen(fscreen_scr_arg *arg, fscreen_scr_t screen)
{
	struct monitor *m = NULL;
	fscreen_scr_arg tmp;

	if (monitor_get_count() == 0) {
		return (monitor_get_global());
	}

	switch (screen) {
	case FSCREEN_GLOBAL:
		m = monitor_get_global();
		break;
	case FSCREEN_PRIMARY:
		m = monitor_by_primary();
		break;
	case FSCREEN_CURRENT:
		/* translate to xypos format */
		if (!arg) {
			tmp.mouse_ev = NULL;
			arg	     = &tmp;
		}
		GetMouseXY(arg->mouse_ev, &arg->xypos.x, &arg->xypos.y);
		/* fall through */
	case FSCREEN_XYPOS:
		/* translate to screen number */
		if (!arg) {
			tmp.xypos.x = 0;
			tmp.xypos.y = 0;
			arg	    = &tmp;
		}
		m = FindScreenOfXY(arg->xypos.x, arg->xypos.y);
		break;
	case FSCREEN_BY_NAME:
		if (arg == NULL || arg->name == NULL) {
			m = monitor_by_primary();
		} else {
			m = monitor_resolve_name(arg->name);
		}
		break;
	default:
		/* XXX: Possible error condition here? */
		break;
	}

	return (m);
}

/* Given pointer coordinates, return the screen the pointer is on.
 *
 * Perhaps most useful with $[pointer.screen]
 */
const char *
FScreenOfPointerXY(int x, int y)
{
	struct monitor *m;

	m = FindScreenOfXY(x, y);

	return (m != NULL) ? m->si->name : "unknown";
}

/* Returns the specified screens geometry rectangle.  screen can be a screen
 * number or any of the values FSCREEN_GLOBAL, FSCREEN_CURRENT,
 * FSCREEN_PRIMARY or FSCREEN_XYPOS.  The arg union only has a meaning for
 * FSCREEN_CURRENT and FSCREEN_XYARG.  For FSCREEN_CURRENT its mouse_ev member
 * may be given.  It is tried to find out the pointer position from the event
 * first before querying the pointer.  For FSCREEN_XYPOS the xpos member is used
 * to fetch the x/y position of the point on the screen.  If arg is NULL, the
 * position 0 0 is assumed instead.
 *
 * Any of the arguments arg, x, y, w and h may be NULL.
 *
 * FSCREEN_GLOBAL:  return the global screen dimensions
 * FSCREEN_CURRENT: return dimensions of the screen with the pointer
 * FSCREEN_PRIMARY: return the primary screen dimensions
 * FSCREEN_XYPOS:   return dimensions of the screen with the given coordinates
 * FSCREEN_BY_NAME: return dimensions of the screen with the given name
 *                  of of the primary screen if arg.name is NULL.
 *
 * The function returns False if the global screen was returned and more than
 * one screen is configured.  Otherwise it returns True.
 */
Bool
FScreenGetScrRect(
    fscreen_scr_arg *arg, fscreen_scr_t screen, int *x, int *y, int *w, int *h)
{
	struct monitor *m = FindScreen(arg, screen);
	if (m == NULL) {
		fvwm_debug(__func__, "m is NULL, using global screen\n");
		m = monitor_get_global();
	}

	if (x)
		*x = m->si->x;
	if (y)
		*y = m->si->y;
	if (w)
		*w = m->si->w;
	if (h)
		*h = m->si->h;

	return !((monitor_get_count() > 1) && m == monitor_global);
}

/* Translates the coodinates *x *y from the screen specified by arg_src and
 * screen_src to coordinates on the screen specified by arg_dest and
 * screen_dest. (see FScreenGetScrRect for more details). */
void
FScreenTranslateCoordinates(fscreen_scr_arg *arg_src, fscreen_scr_t screen_src,
    fscreen_scr_arg *arg_dest, fscreen_scr_t screen_dest, int *x, int *y)
{
	int x_src;
	int y_src;
	int x_dest;
	int y_dest;

	FScreenGetScrRect(arg_src, screen_src, &x_src, &y_src, NULL, NULL);
	FScreenGetScrRect(arg_dest, screen_dest, &x_dest, &y_dest, NULL, NULL);

	if (x) {
		*x = *x + x_src - x_dest;
	}
	if (y) {
		*y = *y + y_src - y_dest;
	}

	return;
}

/* Arguments work exactly like for FScreenGetScrRect() */
int
FScreenClipToScreen(
    fscreen_scr_arg *arg, fscreen_scr_t screen, int *x, int *y, int w, int h)
{
	int sx	   = 0;
	int sy	   = 0;
	int sw	   = 0;
	int sh	   = 0;
	int lx	   = (x) ? *x : 0;
	int ly	   = (y) ? *y : 0;
	int x_grav = GRAV_POS;
	int y_grav = GRAV_POS;

	FScreenGetScrRect(arg, screen, &sx, &sy, &sw, &sh);
	if (lx + w > sx + sw) {
		lx     = sx + sw - w;
		x_grav = GRAV_NEG;
	}
	if (ly + h > sy + sh) {
		ly     = sy + sh - h;
		y_grav = GRAV_NEG;
	}
	if (lx < sx) {
		lx     = sx;
		x_grav = GRAV_POS;
	}
	if (ly < sy) {
		ly     = sy;
		y_grav = GRAV_POS;
	}
	if (x) {
		*x = lx;
	}
	if (y) {
		*y = ly;
	}

	return grav_matrix[y_grav][x_grav];
}

/* Arguments work exactly like for FScreenGetScrRect() */
void
FScreenCenterOnScreen(
    fscreen_scr_arg *arg, fscreen_scr_t screen, int *x, int *y, int w, int h)
{
	int sx = 0;
	int sy = 0;
	int sw = 0;
	int sh = 0;
	int lx;
	int ly;

	FScreenGetScrRect(arg, screen, &sx, &sy, &sw, &sh);
	lx = (sw - w) / 2;
	ly = (sh - h) / 2;
	if (lx < 0)
		lx = 0;
	if (ly < 0)
		ly = 0;
	lx += sx;
	ly += sy;
	if (x) {
		*x = lx;
	}
	if (y) {
		*y = ly;
	}
}

void
FScreenGetResistanceRect(int wx, int wy, unsigned int ww, unsigned int wh,
    int *x0, int *y0, int *x1, int *y1)
{
	fscreen_scr_arg arg;

	arg.xypos.x = wx + ww / 2;
	arg.xypos.y = wy + wh / 2;
	FScreenGetScrRect(&arg, FSCREEN_XYPOS, x0, y0, x1, y1);
	*x1 += *x0;
	*y1 += *y0;
}

/* Arguments work exactly like for FScreenGetScrRect() */
Bool
FScreenIsRectangleOnScreen(
    fscreen_scr_arg *arg, fscreen_scr_t screen, rectangle *rec)
{
	int sx = 0;
	int sy = 0;
	int sw = 0;
	int sh = 0;

	FScreenGetScrRect(arg, screen, &sx, &sy, &sw, &sh);

	return (rec->x + rec->width > sx && rec->x < sx + sw
		   && rec->y + rec->height > sy && rec->y < sy + sh)
		   ? True
		   : False;
}

/*
 * FScreenParseGeometry
 *     Does the same as XParseGeometry, but handles additional "@scr".
 *     Since it isn't safe to define "ScreenValue" constant (actual values
 *     of other "XXXValue" are specified in Xutil.h, not by us, so there can
 *     be a clash).  *_screen_return is set to the screen's name or NULL if no
 *     present in parse_string.
 *
 */
int
FScreenParseGeometryWithScreen(char *parsestring, int *x_return, int *y_return,
    unsigned int *width_return, unsigned int *height_return,
    char **screen_return)
{
	char *geom_str;
	int   ret;
	char *at;

	if (screen_return != NULL) {
		*screen_return = NULL;
	}
	/* Safety net */
	if (parsestring == NULL || *parsestring == '\0') {
		return 0;
	}

	/* No screen specified; parse geometry as standard. */
	at = strchr(parsestring, '@');
	if (at == NULL) {
		geom_str = parsestring;
	} else {
		/* If the geometry specification contains an '@' symbol, assume
		 * the screen is specified.  This must be the name of the
		 * monitor in question.
		 */
		int atpos;

		if (screen_return != NULL) {
			*screen_return = at + 1;
		}
		atpos		= (int)(at - parsestring);
		geom_str	= fxstrdup(parsestring);
		geom_str[atpos] = 0;
	}

	/* Do the parsing */
	ret = XParseGeometry(
	    geom_str, x_return, y_return, width_return, height_return);

	if (geom_str != parsestring) {
		free(geom_str);
	}

	return ret;
}

/* Same as above, but dump screen return value to keep compatible with the X
 * function. */
int
FScreenParseGeometry(char *parsestring, int *x_return, int *y_return,
    unsigned int *width_return, unsigned int *height_return)
{
	struct monitor *m = NULL;
	int		rc, x, y, w, h;

	x = 0;
	y = 0;
	w = monitor_get_all_widths();
	h = monitor_get_all_heights();

	{
		char *scr;

		rc = FScreenParseGeometryWithScreen(parsestring, x_return,
		    y_return, width_return, height_return, &scr);

		if ((m = monitor_resolve_name(scr)) == NULL) {
			/* If the monitor we tried to look up is NULL, it will
			 * be because there was no geometry string specifying
			 * the monitor name.
			 *
			 * In such cases, we should assume that the user has
			 * requested the geometry to be relevant to the global
			 * screen.
			 */
			m = monitor_get_global();
		}
		x = m->si->x;
		y = m->si->y;
		w = m->si->w;
		h = m->si->h;
	}

	/* adapt geometry to selected screen */
	if (rc & XValue) {
		if (rc & XNegative)
			*x_return -= (monitor_get_all_widths() - w - x);
		else
			*x_return += x;
	}
	if (rc & YValue) {
		if (rc & YNegative)
			*y_return -= (monitor_get_all_heights() - h - y);
		else
			*y_return += y;
	}
	return rc;
}

/*  FScreenGetGeometry
 *      Parses the geometry in a form: XGeometry[@screen], i.e.
 *          [=][<width>{xX}<height>][{+-}<xoffset>{+-}<yoffset>][@<screen>]
 *          where <screen> is either a number or "G" (global) "C" (current)
 *          or "P" (primary)
 *
 *  Args:
 *      parsestring, x_r, y_r, w_r, h_r  the same as in XParseGeometry
 *      hints  window hints structure, may be NULL
 *      flags  bitmask of allowed flags (XValue, WidthValue, XNegative...)
 *
 *  Note1:
 *      hints->width and hints->height will be used to calc negative geometry
 *      if width/height isn't specified in the geometry itself.
 *
 *  Note2:
 *      This function's behaviour is crafted to sutisfy/emulate the
 *      FvwmWinList::MakeMeWindow()'s behaviour.
 *
 *  Note3:
 *      A special value of `flags' when [XY]Value are there but [XY]Negative
 *      aren't, means that in case of negative geometry specification
 *      x_r/y_r values will be promoted to the screen border, but w/h
 *      wouldn't be subtracted, so that the program can do x-=w later
 *      ([XY]Negative *will* be returned, albeit absent in `flags').
 *          This option is supposed for proggies like FvwmButtons, which
 *      receive geometry specification long before they are able to actually
 *      use it (and which calculate w/h themselves).
 *          (The same effect can't be obtained with omitting {Width,Height}Value
 *      in the flags, since the app may wish to get the dimensions but apply
 *      some constraints later (as FvwmButtons do, BTW...).)
 *          This option can be also useful in cases where dimensions are
 *      specified not in pixels but in some other units (e.g., charcells).
 */
int
FScreenGetGeometry(char *parsestring, int *x_return, int *y_return,
    int *width_return, int *height_return, XSizeHints *hints, int flags)
{
	fscreen_scr_arg arg;
	int		ret;
	int		saved;
	int		x, y;
	unsigned int	w = 0, h = 0;
	int		grav, x_grav, y_grav;
	int		scr_x, scr_y;
	int		scr_w, scr_h;

	/* I. Do the parsing and strip off extra bits */
	{
		char *scr;

		ret = FScreenParseGeometryWithScreen(
		    parsestring, &x, &y, &w, &h, &scr);
		saved = ret & (XNegative | YNegative);
		ret &= flags;

		arg.mouse_ev = NULL;
		arg.name     = scr;
		FScreenGetScrRect(
		    &arg, FSCREEN_BY_NAME, &scr_x, &scr_y, &scr_w, &scr_h);
	}

	/* II. Interpret and fill in the values */

	/* Fill in dimensions for future negative calculations if
	 * omitted/forbidden */
	/* Maybe should use *x_return,*y_return if hints==NULL?
	 * Unreliable... */
	if (hints != NULL && hints->flags & PSize) {
		if ((ret & WidthValue) == 0) {
			w = hints->width;
		}
		if ((ret & HeightValue) == 0) {
			h = hints->height;
		}
	} else {
		/* This branch is required for case when size *is* specified,
		 * but masked off */
		if ((ret & WidthValue) == 0) {
			w = 0;
		}
		if ((ret & HeightValue) == 0) {
			h = 0;
		}
	}

	/* Advance coords to the screen... */
	x += scr_x;
	y += scr_y;

	/* ...and process negative geometries */
	if (saved & XNegative) {
		x += scr_w;
	}
	if (saved & YNegative) {
		y += scr_h;
	}
	if (ret & XNegative) {
		x -= w;
	}
	if (ret & YNegative) {
		y -= h;
	}

	/* Restore negative bits */
	ret |= saved;

	/* Guess orientation */
	x_grav = (ret & XNegative) ? GRAV_NEG : GRAV_POS;
	y_grav = (ret & YNegative) ? GRAV_NEG : GRAV_POS;
	grav   = grav_matrix[y_grav][x_grav];

	/* Return the values */
	if (ret & XValue) {
		*x_return = x;
		if (hints != NULL) {
			hints->x = x;
		}
	}
	if (ret & YValue) {
		*y_return = y;
		if (hints != NULL) {
			hints->y = y;
		}
	}
	if (ret & WidthValue) {
		*width_return = w;
		if (hints != NULL) {
			hints->width = w;
		}
	}
	if (ret & HeightValue) {
		*height_return = h;
		if (hints != NULL) {
			hints->height = h;
		}
	}
	if (1 /*flags & GravityValue*/ && grav != DEFAULT_GRAVITY) {
		if (hints != NULL && hints->flags & PWinGravity) {
			hints->win_gravity = grav;
		}
	}
	if (hints != NULL && ret & XValue && ret & YValue)
		hints->flags |= USPosition;

	return ret;
}

/*  FScreenMangleScreenIntoUSPosHints
 *      A hack to mangle the screen number into the XSizeHints structure.
 *      If the USPosition flag is set, hints->x is set to the magic number and
 *      hints->y is set to the screen number.  If the USPosition flag is clear,
 *      x and y are set to zero.
 *
 *  Note:  This is a *hack* to allow modules to specify the target screen for
 *  their windows and have the StartsOnScreen style set for them at the same
 *  time.  Do *not* rely on the mechanism described above.
 */
void
FScreenMangleScreenIntoUSPosHints(fscreen_scr_t screen, XSizeHints *hints)
{
	if (hints->flags & USPosition) {
		hints->x = FSCREEN_MANGLE_USPOS_HINTS_MAGIC;
		hints->y = (short)screen;
	} else {
		hints->x = 0;
		hints->y = 0;
	}

	return;
}

/*  FScreenMangleScreenIntoUSPosHints
 *      A hack to mangle the screen number into the XSizeHints structure.
 *      If the USPosition flag is set, hints->x is set to the magic number and
 *      hints->y is set to the screen spec.  If the USPosition flag is clear,
 *      x and y are set to zero.
 *
 *  Note:  This is a *hack* to allow modules to specify the target screen for
 *  their windows and have the StartsOnScreen style set for them at the same
 *  time.  Do *not* rely on the mechanism described above.
 */
int
FScreenFetchMangledScreenFromUSPosHints(XSizeHints *hints)
{
	int screen;

	if ((hints->flags & USPosition)
	    && hints->x == FSCREEN_MANGLE_USPOS_HINTS_MAGIC) {
		screen = hints->y;
	} else
		screen = 0;

	return screen;
}
