/* This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#include "config.h"
#include "script_block.h"
#include <stdlib.h>

void script_block_clear(Bloc *block)
{
	if (block == NULL)
		return;
	for (int i = 0; i <= block->NbInstr; i++)
	{
		Instr *instruction = &block->TabInstr[i];
		int children = instruction->Type == 14 ? 2 :
			(instruction->Type == 15 || instruction->Type == 16);

		/* Only these trailing slots contain owned child pointers. All
		 * other long arguments are values, including function encodings. */
		for (int child = children; child > 0; child--)
		{
			Bloc *nested = (Bloc *)instruction->TabArg[
				instruction->NbArg - child];

			script_block_destroy(&nested);
		}
		free(instruction->TabArg);
	}
	free(block->TabInstr);
	block->TabInstr = NULL;
	block->NbInstr = -1;
}

void script_block_destroy(Bloc **block)
{
	if (*block == NULL)
		return;
	script_block_clear(*block);
	free(*block);
	*block = NULL;
}

bool script_block_owner_release_init(script_block_owner *owner,
	unsigned int active_depth)
{
	if (active_depth != 0)
		return false;
	script_block_destroy(&owner->init);
	return true;
}

bool script_block_owner_clear(script_block_owner *owner,
	unsigned int active_depth)
{
	if (active_depth != 0)
		return false;

	script_block_destroy(&owner->init);
	script_block_destroy(&owner->periodic);
	script_block_destroy(&owner->quit);
	for (size_t object = 0; object < owner->object_count; object++)
	{
		CaseObj *cases = &owner->object_cases[object];

		/* Empty Main on a later realloc slot leaves these two pointer
		 * fields uninitialized. The inclusive last index is authoritative. */
		if (cases->NbCase < 0)
			continue;
		for (int item = 0; item <= cases->NbCase; item++)
			script_block_clear(&owner->object_blocks[object][item]);
		free(owner->object_blocks[object]);
		free(cases->LstCase);
	}
	free(owner->object_blocks);
	free(owner->object_cases);
	*owner = (script_block_owner){0};
	return true;
}
