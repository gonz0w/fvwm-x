/* This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */
#include "config.h"
#include <assert.h>
#include <stdio.h>
#include "fvwm/parent_chain_policy.h"

static void test_acyclic_and_terminal(void)
{
	int nodes[10000] = {0};
	fvwm_parent_chain_state state;

	fvwm_parent_chain_init(&state, nodes);
	for (size_t index = 1; index < 10000; index++)
	{
		assert(!fvwm_parent_chain_repeated(&state, nodes + index));
	}
	assert(!fvwm_parent_chain_repeated(&state, NULL));
}

static void test_cycles_and_prefixes(void)
{
	int nodes[128] = {0};

	for (size_t prefix = 0; prefix < 32; prefix++)
	{
		for (size_t length = 1; length < 32; length++)
		{
			fvwm_parent_chain_state state;
			bool repeated = false;
			size_t current = 0;

			fvwm_parent_chain_init(&state, nodes);
			for (size_t step = 0; step < 4 * (prefix + length); step++)
			{
				current++;
				if (current == prefix + length)
				{
					current = prefix;
				}
				if (fvwm_parent_chain_repeated(&state,
					nodes + current))
				{
					repeated = true;
					break;
				}
			}
			assert(repeated);
		}
	}
}

static void test_checked_counter_boundary(void)
{
	int nodes[2] = {0};
	fvwm_parent_chain_state state = {
		.anchor = nodes, .power = SIZE_MAX, .distance = SIZE_MAX - 1};

	assert(!fvwm_parent_chain_repeated(&state, nodes + 1));
	assert(state.power == SIZE_MAX && state.distance == 0);
	assert(fvwm_parent_chain_repeated(&state, nodes + 1));
}

int main(void)
{
	test_acyclic_and_terminal();
	test_cycles_and_prefixes();
	test_checked_counter_boundary();
	puts("parent-chain: 10000-node acyclic path, 992 cycles, size bounds passed");
	return 0;
}
