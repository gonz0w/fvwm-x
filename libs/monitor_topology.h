#ifndef FVWM_MONITOR_TOPOLOGY_H
#define FVWM_MONITOR_TOPOLOGY_H

#include <X11/Xlib.h>
#include <X11/extensions/Xrandr.h>

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct cJSON cJSON;

typedef struct
{
	void *context;
	void *(*malloc_fn)(void *context, size_t size);
	void *(*calloc_fn)(void *context, size_t count, size_t size);
	void (*free_fn)(void *context, void *pointer);
} monitor_topology_allocator;

typedef struct
{
	Atom	  name_atom;
	char	 *name;
	int	  x;
	int	  y;
	int	  width;
	int	  height;
	int	  mm_width;
	int	  mm_height;
	Bool	  primary;
	RROutput *outputs;
	int	  output_count;
} monitor_topology_monitor;

typedef struct
{
	monitor_topology_monitor  *monitors;
	int			   monitor_count;
	uint64_t		   epoch;
	monitor_topology_allocator allocator;
} monitor_topology_snapshot;

typedef enum
{
	MONITOR_TOPOLOGY_PUBLISH_INITIAL,
	MONITOR_TOPOLOGY_PUBLISH_CHANGED,
	MONITOR_TOPOLOGY_PUBLISH_UNCHANGED,
	MONITOR_TOPOLOGY_PUBLISH_INVALID
} monitor_topology_publish_result;

monitor_topology_snapshot *
monitor_topology_snapshot_create(const monitor_topology_monitor *monitors,
    int monitor_count, const monitor_topology_allocator *allocator);
bool
monitor_topology_snapshot_validate(const monitor_topology_snapshot *snapshot);
bool
monitor_topology_snapshot_equal(const monitor_topology_snapshot *left,
    const monitor_topology_snapshot				*right);
monitor_topology_publish_result
monitor_topology_snapshot_publish(monitor_topology_snapshot **committed,
    monitor_topology_snapshot				     *candidate);
void
monitor_topology_snapshot_destroy(monitor_topology_snapshot *snapshot);
bool
monitor_topology_json_add(
    cJSON *parent, const char *key, const monitor_topology_snapshot *snapshot);

#endif
