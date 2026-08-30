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

#include "fvwm/topology_refresh_policy.h"

#include <assert.h>
#include <stdint.h>

#define NS_PER_MS UINT64_C(1000000)
#define MS(value) (UINT64_C(value) * NS_PER_MS)

static void
test_marks_preserve_first_and_update_last(void)
{
	fvwm_topology_refresh_state state = FVWM_TOPOLOGY_REFRESH_STATE_INIT;

	fvwm_topology_refresh_mark(&state, MS(100));
	assert(state.dirty);
	assert(state.first_dirty_ns == MS(100));
	assert(state.last_dirty_ns == MS(100));

	fvwm_topology_refresh_mark(&state, MS(125));
	assert(state.first_dirty_ns == MS(100));
	assert(state.last_dirty_ns == MS(125));
}

static void
test_quiet_deadline_extends_with_marks(void)
{
	fvwm_topology_refresh_state state = FVWM_TOPOLOGY_REFRESH_STATE_INIT;

	fvwm_topology_refresh_mark(&state, MS(100));
	assert(fvwm_topology_refresh_timeout_ms(&state, MS(100)) == 50);

	fvwm_topology_refresh_mark(&state, MS(140));
	assert(!fvwm_topology_refresh_is_due(&state, MS(150)));
	assert(fvwm_topology_refresh_timeout_ms(&state, MS(150)) == 40);
	assert(fvwm_topology_refresh_is_due(&state, MS(190)));
}

static void
test_hard_cap_wins_during_continuous_events(void)
{
	fvwm_topology_refresh_state state = FVWM_TOPOLOGY_REFRESH_STATE_INIT;
	uint64_t		    mark_ms;

	fvwm_topology_refresh_mark(&state, MS(100));
	for (mark_ms = 140; mark_ms <= 340; mark_ms += 40) {
		fvwm_topology_refresh_mark(&state, mark_ms * NS_PER_MS);
	}

	assert(!fvwm_topology_refresh_is_due(&state, MS(349)));
	assert(fvwm_topology_refresh_timeout_ms(&state, MS(349)) == 1);
	assert(fvwm_topology_refresh_is_due(&state, MS(350)));
	assert(fvwm_topology_refresh_timeout_ms(&state, MS(350)) == 0);
}

static void
test_exact_boundaries_and_timeout_ceiling(void)
{
	fvwm_topology_refresh_state state = FVWM_TOPOLOGY_REFRESH_STATE_INIT;

	fvwm_topology_refresh_mark(&state, MS(20));
	assert(!fvwm_topology_refresh_is_due(&state, MS(70) - UINT64_C(1)));
	assert(fvwm_topology_refresh_timeout_ms(&state, MS(70) - UINT64_C(1))
	       == 1);
	assert(fvwm_topology_refresh_is_due(&state, MS(70)));
	assert(fvwm_topology_refresh_timeout_ms(&state, MS(70)) == 0);
}

static void
test_saturating_deadline_addition(void)
{
	fvwm_topology_refresh_state state = FVWM_TOPOLOGY_REFRESH_STATE_INIT;

	fvwm_topology_refresh_mark(&state, UINT64_MAX - MS(10));
	assert(!fvwm_topology_refresh_is_due(&state, UINT64_MAX - 1));
	assert(fvwm_topology_refresh_timeout_ms(&state, UINT64_MAX - 1) == 1);
	assert(fvwm_topology_refresh_is_due(&state, UINT64_MAX));
}

static void
test_completion_resets_and_later_marks_rearm(void)
{
	fvwm_topology_refresh_state state;
	unsigned int		    outcome;

	fvwm_topology_refresh_init(&state);
	assert(!state.dirty);
	assert(!fvwm_topology_refresh_is_due(&state, UINT64_MAX));
	assert(fvwm_topology_refresh_timeout_ms(&state, 0) == -1);

	for (outcome = 0; outcome < 3; outcome++) {
		uint64_t mark = MS(100 + outcome * 100);

		fvwm_topology_refresh_mark(&state, mark);
		assert(state.dirty);
		assert(state.first_dirty_ns == mark);
		fvwm_topology_refresh_complete(&state);
		assert(!state.dirty);
		assert(state.first_dirty_ns == 0);
		assert(state.last_dirty_ns == 0);
		assert(fvwm_topology_refresh_timeout_ms(&state, mark) == -1);
	}

	fvwm_topology_refresh_mark(&state, MS(500));
	assert(state.dirty);
	assert(state.first_dirty_ns == MS(500));
	assert(state.last_dirty_ns == MS(500));
}

int
main(void)
{
	test_marks_preserve_first_and_update_last();
	test_quiet_deadline_extends_with_marks();
	test_hard_cap_wins_during_continuous_events();
	test_exact_boundaries_and_timeout_ceiling();
	test_saturating_deadline_addition();
	test_completion_resets_and_later_marks_rearm();

	return 0;
}
