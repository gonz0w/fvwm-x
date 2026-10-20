#ifndef FVWM_FSCREEN_ADAPTERS_H
#define FVWM_FSCREEN_ADAPTERS_H

#include "FScreen.h"

/* Operation-private adapters; public monitor clients need no layout. */
typedef struct
{
	void *context;
	XRRMonitorInfo *(*get_monitors)(void *, Display *, Window, Bool, int *);
	char *(*get_atom_name)(void *, Display *, Atom);
	void (*free_atom_name)(void *, char *);
	void (*free_monitors)(void *, XRRMonitorInfo *);
	/* Optional borrowed root facts for an empty initial/fallback topology. */
	bool (*get_root_monitor)(void *, Display *, Window,
	    monitor_topology_monitor *);
	monitor_topology_allocator topology_allocator;
} fscreen_randr_adapter;

typedef struct
{
	void *context;
	struct monitor *(*allocate_monitor)(void *);
	struct screen_info *(*allocate_screen)(void *);
	char *(*copy_name)(void *, const char *);
	void (*free_monitor)(void *, struct monitor *);
	void (*free_screen)(void *, struct screen_info *);
	void (*free_name)(void *, char *);
} fscreen_legacy_allocator;

#ifdef FSCREEN_TESTING
fscreen_topology_refresh_result
fscreen_refresh_topology_with_adapters(Display *, Window,
    monitor_topology_snapshot **, struct monitors *, struct screen_infos *,
    const fscreen_randr_adapter *, const fscreen_legacy_allocator *);
#endif

#endif
