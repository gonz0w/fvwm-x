#ifndef FVWMSCRIPT_SCRIPT_BLOCK_H
#define FVWMSCRIPT_SCRIPT_BLOCK_H

#include "types.h"
#include <stdbool.h>
#include <stddef.h>

/* Complete parser output only. Case blocks are embedded in per-object arrays;
 * the three special roots and conditional children are heap Bloc records. */
typedef struct
{
	Bloc *init;
	Bloc *periodic;
	Bloc *quit;
	Bloc **object_blocks;
	CaseObj *object_cases;
	size_t object_count;
	bool published;
} script_block_owner;

/* Clear an embedded block's contents without freeing the record itself. */
void script_block_clear(Bloc *block);
/* Clear a heap root and its record, removing the owner's reference. */
void script_block_destroy(Bloc **block);
/* Refuse retirement while ExecBloc retains any instruction or argument. */
bool script_block_owner_release_init(script_block_owner *owner,
	unsigned int active_depth);
bool script_block_owner_clear(script_block_owner *owner,
	unsigned int active_depth);

#endif
