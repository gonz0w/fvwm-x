/* This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */
#include "config.h"
#include "startup_group_policy.h"

#include <limits.h>
#include <string.h>

_Static_assert(sizeof(struct startup_group_state) <= STARTUP_GROUP_STATE_MAX,
	"startup group policy exceeds its fixed-storage budget");

void startup_group_init(struct startup_group_state *state,
	uint32_t property_mask, uint32_t structure_mask)
{
	memset(state, 0, sizeof(*state));
	state->property_mask = property_mask;
	state->structure_mask = structure_mask;
}

struct startup_group_leader *startup_group_find_leader(
	struct startup_group_state *state, uint64_t window)
{
	if (window != 0 && state->leader_count != 0)
		for (unsigned int i = 0; i < STARTUP_GROUP_LEADER_MAX; i++)
			if (state->leaders[i].window == window)
				return &state->leaders[i];
	return NULL;
}

struct startup_group_binding *startup_group_find_binding(
	struct startup_group_state *state, uint64_t client)
{
	if (client != 0)
		for (unsigned int i = 0; i < STARTUP_GROUP_CLIENT_MAX; i++)
			if (state->clients[i].client == client)
				return &state->clients[i];
	return NULL;
}

static void detach_binding(struct startup_group_state *state,
	struct startup_group_binding *binding)
{
	struct startup_group_leader *leader;

	if (binding->invalidated || binding->leader == 0)
		return;
	leader = startup_group_find_leader(state, binding->leader);
	if (leader != NULL && leader->references != 0)
	{
		leader->references--;
		if (leader->references == 0)
		{
			leader->mask_dirty = true;
			state->queued = true;
		}
	}
}

bool startup_group_bind(struct startup_group_state *state, uint64_t client,
	uint64_t identity, uint64_t group, uint64_t serial)
{
	struct startup_group_binding *binding;
	struct startup_group_leader *leader;
	bool existing;

	if (client == 0 || identity == 0)
		return false;
	binding = startup_group_find_binding(state, client);
	existing = binding != NULL;
	if (binding != NULL && binding->identity == identity &&
	    binding->leader == group && !binding->invalidated)
	{
		binding->dirty = state->pending;
		state->queued |= binding->dirty;
		return true;
	}
	if (binding == NULL)
		for (unsigned int i = 0; i < STARTUP_GROUP_CLIENT_MAX; i++)
			if (state->clients[i].client == 0)
			{
				binding = &state->clients[i];
				break;
			}
	if (binding == NULL)
		return group == 0;
	if (existing)
		detach_binding(state, binding);
	memset(binding, 0, sizeof(*binding));
	if (group == 0)
		return true;
	leader = startup_group_find_leader(state, group);
	if (leader == NULL)
		for (unsigned int i = 0; i < STARTUP_GROUP_LEADER_MAX; i++)
			if (state->leaders[i].window == 0)
			{
				leader = &state->leaders[i];
				memset(leader, 0, sizeof(*leader));
				leader->window = group;
				leader->serial = serial;
				leader->mask_dirty = true;
				state->queued = true;
				state->leader_count++;
				break;
			}
	if (leader == NULL)
	{
		if (existing)
		{
			binding->client = client;
			binding->identity = identity;
			binding->leader = group;
			binding->invalidated = true;
		}
		return false;
	}
	leader->references++;
	binding->client = client;
	binding->identity = identity;
	binding->leader = group;
	binding->dirty = state->pending;
	state->queued |= binding->dirty;
	return true;
}

void startup_group_unbind(struct startup_group_state *state, uint64_t client,
	uint64_t identity)
{
	struct startup_group_binding *binding =
		startup_group_find_binding(state, client);

	if (binding == NULL || binding->identity != identity)
		return;
	detach_binding(state, binding);
	memset(binding, 0, sizeof(*binding));
}

void startup_group_defer(struct startup_group_state *state, uint64_t client,
	uint64_t identity)
{
	struct startup_group_binding *binding =
		startup_group_find_binding(state, client);

	if (state->pending && binding != NULL && binding->identity == identity &&
	    !binding->invalidated)
	{
		binding->dirty = true;
		state->queued = true;
	}
}

void startup_group_pending(struct startup_group_state *state, bool pending,
	bool rescan)
{
	bool changed = pending != state->pending;

	if (!changed && !rescan)
		return;
	state->pending = pending;
	if (changed)
		for (unsigned int i = 0; i < STARTUP_GROUP_LEADER_MAX; i++)
			if (state->leaders[i].window != 0)
			{
				state->leaders[i].mask_dirty = true;
				state->queued = true;
			}
	for (unsigned int i = 0; i < STARTUP_GROUP_CLIENT_MAX; i++)
	{
		struct startup_group_binding *binding = &state->clients[i];

		if (!pending)
			binding->dirty = false;
		else if ((changed || rescan) && binding->client != 0 &&
		    !binding->invalidated)
		{
			binding->dirty = true;
			state->queued = true;
		}
	}
}

static bool client_ready(const struct startup_group_state *state,
	const struct startup_group_binding *binding)
{
	if (!state->pending || !binding->dirty || binding->invalidated)
		return false;
	for (unsigned int i = 0; i < STARTUP_GROUP_LEADER_MAX; i++)
		if (state->leaders[i].window == binding->leader)
			return state->leaders[i].acquired &&
				!state->leaders[i].mask_dirty;
	return false;
}

bool startup_group_outer_pending(const struct startup_group_state *state)
{
	return state->queued;
}

bool startup_group_next_work(struct startup_group_state *state,
	struct startup_group_work *work)
{
	const unsigned int total = STARTUP_GROUP_LEADER_MAX +
		STARTUP_GROUP_CLIENT_MAX;

	work->kind = STARTUP_GROUP_NO_WORK;
	work->index = 0;
	if (!state->queued)
		return false;
	for (unsigned int scanned = 0; scanned < total; scanned++)
	{
		unsigned int index = state->cursor;

		state->cursor = (state->cursor + 1) % total;
		if (index < STARTUP_GROUP_LEADER_MAX)
		{
			struct startup_group_leader *leader = &state->leaders[index];

			if (leader->window == 0 || !leader->mask_dirty)
				continue;
			leader->mask_dirty = false;
			work->kind = STARTUP_GROUP_MASK_WORK;
			work->index = index;
			return true;
		}
		index -= STARTUP_GROUP_LEADER_MAX;
		if (state->clients[index].client != 0 &&
		    client_ready(state, &state->clients[index]))
		{
			state->clients[index].dirty = false;
			work->kind = STARTUP_GROUP_CLIENT_WORK;
			work->index = index;
			return true;
		}
	}
	state->queued = false;
	return false;
}

void startup_group_acquired(struct startup_group_state *state, uint64_t group,
	uint32_t base_mask, uint64_t serial, bool managed)
{
	struct startup_group_leader *leader = startup_group_find_leader(state, group);

	if (leader == NULL)
		return;
	if (!leader->base_known)
		startup_group_base(leader, base_mask);
	leader->acquired = true;
	leader->serial = serial;
	leader->managed = managed;
	leader->mask_dirty = false;
	if (state->pending)
		for (unsigned int i = 0; i < STARTUP_GROUP_CLIENT_MAX; i++)
			if (state->clients[i].leader == group &&
			    !state->clients[i].invalidated)
			{
				state->clients[i].dirty = true;
				state->queued = true;
			}
}

void startup_group_mode(struct startup_group_state *state, uint64_t group,
	bool managed)
{
	struct startup_group_leader *leader = startup_group_find_leader(state, group);

	if (leader != NULL && leader->managed != managed)
	{
		leader->managed = managed;
		leader->mask_dirty = true;
		state->queued = true;
	}
}

void startup_group_abandon(struct startup_group_state *state, uint64_t group)
{
	struct startup_group_leader *leader = startup_group_find_leader(state, group);

	if (leader == NULL)
		return;
	for (unsigned int i = 0; i < STARTUP_GROUP_CLIENT_MAX; i++)
		if (state->clients[i].client != 0 &&
		    state->clients[i].leader == group)
		{
			state->clients[i].invalidated = true;
			state->clients[i].dirty = false;
		}
	memset(leader, 0, sizeof(*leader));
	state->leader_count--;
}

bool startup_group_notify(struct startup_group_state *state, uint64_t group,
	uint64_t serial, bool destroyed)
{
	struct startup_group_leader *leader = startup_group_find_leader(state, group);

	if (leader == NULL || serial < leader->serial)
		return false;
	if (destroyed)
		startup_group_abandon(state, group);
	else if (state->pending)
		for (unsigned int i = 0; i < STARTUP_GROUP_CLIENT_MAX; i++)
			if (state->clients[i].client != 0 &&
			    state->clients[i].leader == group &&
			    !state->clients[i].invalidated)
			{
				state->clients[i].dirty = true;
				state->queued = true;
			}
	return true;
}

bool startup_group_release(struct startup_group_state *state, uint64_t group)
{
	struct startup_group_leader *leader = startup_group_find_leader(state, group);

	if (leader == NULL || leader->references != 0)
		return false;
	memset(leader, 0, sizeof(*leader));
	state->leader_count--;
	return true;
}

uint32_t startup_group_effective(const struct startup_group_state *state,
	const struct startup_group_leader *leader)
{
	uint32_t mask = leader->base_mask;

	if (leader->references != 0 && leader->acquired)
	{
		if (!leader->managed)
			mask |= state->structure_mask;
		if (state->pending)
			mask |= state->property_mask;
	}
	for (unsigned int i = 0; i < 32; i++)
		if (leader->suppressed[i] != 0)
			mask &= ~(UINT32_C(1) << i);
	return mask;
}

void startup_group_base(struct startup_group_leader *leader, uint32_t mask)
{
	leader->base_mask = mask;
	leader->base_known = true;
}

bool startup_group_suppress(struct startup_group_leader *leader, uint32_t mask)
{
	for (unsigned int i = 0; i < 32; i++)
		if ((mask & (UINT32_C(1) << i)) != 0 &&
		    leader->suppressed[i] == UINT32_MAX)
			return false;
	for (unsigned int i = 0; i < 32; i++)
		if ((mask & (UINT32_C(1) << i)) != 0)
			leader->suppressed[i]++;
	return true;
}

void startup_group_unsuppress(struct startup_group_leader *leader,
	uint32_t mask)
{
	for (unsigned int i = 0; i < 32; i++)
		if ((mask & (UINT32_C(1) << i)) != 0 &&
		    leader->suppressed[i] != 0)
			leader->suppressed[i]--;
}
