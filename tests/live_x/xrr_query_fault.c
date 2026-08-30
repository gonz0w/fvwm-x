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
#include <stdlib.h>
#include <unistd.h>

#if defined(FVWM_XRR_QUERY_FAULT_PRELOAD) ==                                \
    defined(FVWM_XRR_QUERY_FAULT_WRAP)
#error "select exactly one XRR query fault mode"
#endif

#define FAULT_ENV "FVWM_TEST_XRR_FAIL_FILE"

static Bool
consume_failure_sentinel(void)
{
	const char *path;

	path = getenv(FAULT_ENV);
	if (path == NULL || *path == '\0')
		return False;

	/* unlink(2) is the one atomic claim; later calls delegate normally. */
	return unlink(path) == 0 ? True : False;
}

static XRRMonitorInfo *
fail_query(int *nmonitors)
{
	if (nmonitors != NULL)
		*nmonitors = -1;
	return NULL;
}

#if defined(FVWM_XRR_QUERY_FAULT_PRELOAD)
#include <dlfcn.h>

typedef XRRMonitorInfo *(*xrr_get_monitors_fn)(Display *, Window, Bool, int *);

XRRMonitorInfo *
XRRGetMonitors(Display *display, Window window, Bool active, int *nmonitors)
{
	static xrr_get_monitors_fn real_xrr_get_monitors;

	if (consume_failure_sentinel())
		return fail_query(nmonitors);
	if (real_xrr_get_monitors == NULL) {
		real_xrr_get_monitors =
		    (xrr_get_monitors_fn)dlsym(RTLD_NEXT, "XRRGetMonitors");
	}
	if (real_xrr_get_monitors == NULL)
		return fail_query(nmonitors);

	return real_xrr_get_monitors(display, window, active, nmonitors);
}
#endif

#if defined(FVWM_XRR_QUERY_FAULT_WRAP)
extern XRRMonitorInfo *__real_XRRGetMonitors(
    Display *, Window, Bool, int *);

XRRMonitorInfo *
__wrap_XRRGetMonitors(
    Display *display, Window window, Bool active, int *nmonitors)
{
	if (consume_failure_sentinel())
		return fail_query(nmonitors);

	return __real_XRRGetMonitors(display, window, active, nmonitors);
}
#endif
