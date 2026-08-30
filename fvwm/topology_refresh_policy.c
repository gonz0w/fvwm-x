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

#include <limits.h>

#define NS_PER_MS UINT64_C(1000000)

static uint64_t
saturating_add_ms(uint64_t start_ns, uint64_t duration_ms)
{
	uint64_t duration_ns;

	if (duration_ms > UINT64_MAX / NS_PER_MS) {
		return UINT64_MAX;
	}
	duration_ns = duration_ms * NS_PER_MS;
	if (start_ns > UINT64_MAX - duration_ns) {
		return UINT64_MAX;
	}

	return start_ns + duration_ns;
}

static uint64_t
refresh_deadline_ns(const fvwm_topology_refresh_state *state)
{
	uint64_t quiet_deadline;
	uint64_t maximum_deadline;

	quiet_deadline =
	    saturating_add_ms(state->last_dirty_ns, FVWM_TOPOLOGY_QUIET_MS);
	maximum_deadline = saturating_add_ms(
	    state->first_dirty_ns, FVWM_TOPOLOGY_MAX_LATENCY_MS);

	return quiet_deadline < maximum_deadline ? quiet_deadline
						 : maximum_deadline;
}

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
	if (!state->dirty) {
		state->dirty	      = true;
		state->first_dirty_ns = now_ns;
	}
	state->last_dirty_ns = now_ns;
}

bool
fvwm_topology_refresh_is_due(
    const fvwm_topology_refresh_state *state, uint64_t now_ns)
{
	return state->dirty && now_ns >= refresh_deadline_ns(state);
}

int
fvwm_topology_refresh_timeout_ms(
    const fvwm_topology_refresh_state *state, uint64_t now_ns)
{
	uint64_t deadline_ns;
	uint64_t remaining_ms;
	uint64_t remaining_ns;

	if (!state->dirty) {
		return -1;
	}
	deadline_ns = refresh_deadline_ns(state);
	if (now_ns >= deadline_ns) {
		return 0;
	}

	remaining_ns = deadline_ns - now_ns;
	remaining_ms = remaining_ns / NS_PER_MS;
	if (remaining_ns % NS_PER_MS != 0) {
		remaining_ms++;
	}
	if (remaining_ms > INT_MAX) {
		return INT_MAX;
	}

	return (int)remaining_ms;
}

void
fvwm_topology_refresh_complete(fvwm_topology_refresh_state *state)
{
	fvwm_topology_refresh_init(state);
}
