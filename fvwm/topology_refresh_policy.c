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

#include "topology_refresh_policy.h"

void
fvwm_topology_refresh_init(fvwm_topology_refresh_state *state)
{
	state->dirty	      = false;
	state->first_dirty_ns = 0;
	state->last_dirty_ns  = 0;
}

void
fvwm_topology_refresh_mark(fvwm_topology_refresh_state *state, uint64_t now_ns)
{
}

bool
fvwm_topology_refresh_is_due(
    const fvwm_topology_refresh_state *state, uint64_t now_ns)
{
	return false;
}

int
fvwm_topology_refresh_timeout_ms(
    const fvwm_topology_refresh_state *state, uint64_t now_ns)
{
	return -1;
}

void
fvwm_topology_refresh_complete(fvwm_topology_refresh_state *state)
{
}
