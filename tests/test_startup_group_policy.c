/* This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */
#include "config.h"
#include "libs/startup_group_policy.h"
#include "libs/startup_notification_policy.h"

#include <assert.h>
#include <stdio.h>

#define PROPERTY UINT32_C(0x00400000)
#define STRUCTURE UINT32_C(0x00020000)
#define FOCUS UINT32_C(0x00200000)
#define OTHER UINT32_C(0x00000020)

_Static_assert(sizeof(struct startup_group_state) <= STARTUP_GROUP_STATE_MAX,
	"startup group state exceeds its added-storage budget");
_Static_assert(sizeof(struct startup_group_state) +
	sizeof(struct startup_notification_state) < 512 * 1024,
	"combined startup observer exceeds its fixed-storage budget");

static void discard_ready_work(struct startup_group_state *state)
{
	struct startup_group_work work;
	unsigned int steps = 0;

	while (startup_group_next_work(state, &work))
	{
		assert(++steps <= 2 * STARTUP_GROUP_CLIENT_MAX);
		if (work.kind == STARTUP_GROUP_MASK_WORK)
		{
			struct startup_group_leader *leader =
				&state->leaders[work.index];

			if (leader->references == 0)
				assert(startup_group_release(state, leader->window));
			else if (!leader->acquired)
				startup_group_acquired(state, leader->window,
					0, leader->serial, false);
		}
	}
}

static void base_claims_and_nested_suppression(void)
{
	struct startup_group_state state;
	struct startup_group_leader *leader;

	startup_group_init(&state, PROPERTY, STRUCTURE);
	assert(startup_group_bind(&state, 10, 1, 90, 10));
	startup_group_acquired(&state, 90, FOCUS | OTHER, 20, false);
	leader = startup_group_find_leader(&state, 90);
	assert(leader != NULL && leader->references == 1);
	assert(startup_group_effective(&state, leader) ==
		(FOCUS | OTHER | STRUCTURE));
	startup_group_pending(&state, true, true);
	assert(startup_group_effective(&state, leader) ==
		(FOCUS | OTHER | STRUCTURE | PROPERTY));
	assert(startup_group_suppress(leader, STRUCTURE));
	assert(startup_group_suppress(leader, STRUCTURE));
	startup_group_base(leader, OTHER);
	assert(startup_group_effective(&state, leader) == (OTHER | PROPERTY));
	assert(startup_group_suppress(leader, PROPERTY));
	assert(startup_group_effective(&state, leader) == OTHER);
	startup_group_unsuppress(leader, PROPERTY);
	startup_group_unsuppress(leader, STRUCTURE);
	assert(startup_group_effective(&state, leader) == (OTHER | PROPERTY));
	startup_group_base(leader, FOCUS);
	startup_group_unsuppress(leader, STRUCTURE);
	assert(startup_group_effective(&state, leader) ==
		(FOCUS | STRUCTURE | PROPERTY));
	/* A managed leader must not defeat the core's withdrawal suppression. */
	startup_group_mode(&state, 90, true);
	assert(startup_group_effective(&state, leader) == (FOCUS | PROPERTY));
	startup_group_pending(&state, false, false);
	assert(startup_group_effective(&state, leader) == FOCUS);
	startup_group_mode(&state, 90, false);
	assert(startup_group_effective(&state, leader) == (FOCUS | STRUCTURE));
	startup_group_unbind(&state, 10, 1);
	assert(startup_group_effective(&state, leader) == FOCUS);
	assert(startup_group_release(&state, 90));
	assert(startup_group_find_leader(&state, 90) == NULL);
}

static void each_window_keeps_its_own_base(void)
{
	struct startup_group_state state;
	struct startup_group_leader *first, *second;

	startup_group_init(&state, PROPERTY, STRUCTURE);
	assert(startup_group_bind(&state, 10, 1, 90, 10));
	assert(startup_group_bind(&state, 11, 2, 91, 10));
	startup_group_acquired(&state, 90, STRUCTURE | FOCUS, 20, true);
	startup_group_acquired(&state, 91, STRUCTURE | OTHER, 20, true);
	first = startup_group_find_leader(&state, 90);
	second = startup_group_find_leader(&state, 91);
	assert(startup_group_suppress(first, STRUCTURE));
	assert(startup_group_suppress(second, STRUCTURE));
	assert(startup_group_effective(&state, first) == FOCUS);
	assert(startup_group_effective(&state, second) == OTHER);
	startup_group_unsuppress(first, STRUCTURE);
	startup_group_unsuppress(second, STRUCTURE);
	assert(startup_group_effective(&state, first) == (STRUCTURE | FOCUS));
	assert(startup_group_effective(&state, second) == (STRUCTURE | OTHER));
}

static void references_identity_and_destroyed_reuse(void)
{
	struct startup_group_state state;
	struct startup_group_binding *binding;
	struct startup_group_leader *leader;

	startup_group_init(&state, PROPERTY, STRUCTURE);
	assert(startup_group_bind(&state, 10, 1, 90, 10));
	assert(startup_group_bind(&state, 11, 2, 90, 10));
	startup_group_acquired(&state, 90, OTHER, 100, false);
	leader = startup_group_find_leader(&state, 90);
	assert(leader->references == 2);
	startup_group_unbind(&state, 10, 999);
	assert(leader->references == 2);
	assert(startup_group_bind(&state, 10, 3, 90, 110));
	assert(leader->references == 2);
	assert(!startup_group_notify(&state, 90, 99, true));
	assert(startup_group_find_leader(&state, 90) == leader);
	assert(startup_group_notify(&state, 90, 100, true));
	assert(startup_group_find_leader(&state, 90) == NULL);
	assert(startup_group_find_binding(&state, 10)->invalidated);
	assert(startup_group_find_binding(&state, 11)->invalidated);
	assert(!startup_group_notify(&state, 90, 100, true));
	/* The same server XID now belongs to a fresh lifetime. */
	assert(startup_group_bind(&state, 12, 4, 90, 200));
	startup_group_acquired(&state, 90, FOCUS, 210, false);
	startup_group_unbind(&state, 10, 3);
	leader = startup_group_find_leader(&state, 90);
	assert(leader != NULL && leader->references == 1);
	binding = startup_group_find_binding(&state, 11);
	assert(binding->invalidated && binding->identity == 2);
	assert(!startup_group_notify(&state, 90, 209, true));
	assert(leader->references == 1);
	/* Only fresh hints/lifecycle admission revives the old dependent. */
	assert(startup_group_bind(&state, 11, 2, 90, 220));
	assert(!binding->invalidated && leader->references == 2);
}

static void capacity_preserves_existing_owners(void)
{
	struct startup_group_state state;
	unsigned int i;

	startup_group_init(&state, PROPERTY, STRUCTURE);
	for (i = 0; i < STARTUP_GROUP_CLIENT_MAX; i++)
		assert(startup_group_bind(&state, 10 + i, 1 + i, 1000 + i, 1));
	assert(!startup_group_bind(&state, 999, 999, 9999, 2));
	for (i = 0; i < STARTUP_GROUP_CLIENT_MAX; i++)
	{
		assert(startup_group_find_binding(&state, 10 + i)->leader ==
			1000 + i);
		assert(startup_group_find_leader(&state, 1000 + i)->references == 1);
	}
	assert(!startup_group_release(&state, 1000));
	startup_group_unbind(&state, 10, 1);
	assert(startup_group_release(&state, 1000));
	assert(startup_group_bind(&state, 999, 999, 9999, 2));
	assert(startup_group_find_binding(&state, 999)->leader == 9999);
	/* Retiring mask records cannot be stolen before their base is restored. */
	for (i = 1; i < STARTUP_GROUP_CLIENT_MAX; i++)
		startup_group_unbind(&state, 10 + i, 1 + i);
	startup_group_unbind(&state, 999, 999);
	assert(!startup_group_bind(&state, 888, 888, 8888, 3));
	assert(startup_group_find_binding(&state, 888) == NULL);
	assert(startup_group_release(&state, 1001));
	assert(startup_group_bind(&state, 888, 888, 8888, 4));
}

static void pending_epochs_and_fair_continuation(void)
{
	struct startup_group_state state;
	struct startup_group_work work;
	unsigned int i, seen = 0;

	startup_group_init(&state, PROPERTY, STRUCTURE);
	for (i = 0; i < 3; i++)
		assert(startup_group_bind(&state, 10 + i, 1 + i, 90, 1));
	startup_group_acquired(&state, 90, OTHER, 10, false);
	discard_ready_work(&state);
	assert(!startup_group_outer_pending(&state));
	startup_group_pending(&state, true, true);
	for (i = 0; i < 8 && seen != 7; i++)
	{
		assert(startup_group_next_work(&state, &work));
		if (work.kind == STARTUP_GROUP_CLIENT_WORK)
			seen |= 1U << (state.clients[work.index].client - 10);
		/* A flood marks work without resetting the round-robin cursor. */
		assert(startup_group_notify(&state, 90, 20 + i, false));
	}
	assert(seen == 7);
	startup_group_pending(&state, false, false);
	while (startup_group_next_work(&state, &work))
		assert(work.kind == STARTUP_GROUP_MASK_WORK);
	assert(!startup_group_outer_pending(&state));
	startup_group_pending(&state, true, false);
	assert(startup_group_outer_pending(&state));
	discard_ready_work(&state);
	assert(!startup_group_outer_pending(&state));
	/* Inline observation queues only a current, live dependent identity. */
	startup_group_defer(&state, 10, 999);
	assert(!startup_group_outer_pending(&state));
	startup_group_defer(&state, 10, 1);
	assert(startup_group_outer_pending(&state));
	discard_ready_work(&state);
	/* Another accepted launch while pending revisits existing groups. */
	startup_group_pending(&state, true, true);
	assert(startup_group_outer_pending(&state));
}

int main(void)
{
	base_claims_and_nested_suppression();
	each_window_keeps_its_own_base();
	references_identity_and_destroyed_reuse();
	capacity_preserves_existing_owners();
	pending_epochs_and_fair_continuation();
	puts("startup group ownership policy: five groups passed");
	return 0;
}
