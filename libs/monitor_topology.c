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

#include "monitor_topology.h"

monitor_topology_snapshot *
monitor_topology_snapshot_create(const monitor_topology_monitor *monitors,
    int monitor_count, const monitor_topology_allocator *allocator)
{
	return NULL;
}

bool
monitor_topology_snapshot_validate(const monitor_topology_snapshot *snapshot)
{
	return false;
}

bool
monitor_topology_snapshot_equal(const monitor_topology_snapshot *left,
    const monitor_topology_snapshot				*right)
{
	return false;
}

monitor_topology_publish_result
monitor_topology_snapshot_publish(
    monitor_topology_snapshot **committed, monitor_topology_snapshot *candidate)
{
	return MONITOR_TOPOLOGY_PUBLISH_INVALID;
}

void
monitor_topology_snapshot_destroy(monitor_topology_snapshot *snapshot)
{
}
