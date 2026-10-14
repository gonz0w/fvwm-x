#ifndef FVWM_STACK_PARENT_H
#define FVWM_STACK_PARENT_H

#include <stddef.h>
#include <stdint.h>
#include "stack.h"

#define FVWM_STACK_PARENT_INLINE 32

struct fvwm_stack_parent_entry;
typedef struct
{
	FvwmWindow *inline_frames[FVWM_STACK_PARENT_INLINE];
	Window inline_clients[FVWM_STACK_PARENT_INLINE];
	struct fvwm_stack_parent_entry *index;
	FvwmWindow *leaf;
	Window leaf_client;
	size_t count;
	size_t depth;
} fvwm_stack_parent_path;

typedef enum
{
	FVWM_STACK_PARENT_READY,
	FVWM_STACK_PARENT_CYCLE,
	FVWM_STACK_PARENT_UNAVAILABLE
} fvwm_stack_parent_result;

/* Prepare without X work/callbacks. Membership and first-match lookup use only
 * the passed stack ring. Frames borrow records for the caller's synchronous
 * operation, like its former C call stack. Temporary branch ranks and scheduled
 * flags may change; failure discards storage, and the caller discards ranks.
 * The path must be new or finished; outputs are initialized on every call. */
fvwm_stack_parent_result fvwm_stack_parent_prepare(
	fvwm_stack_parent_path *path, FvwmWindow *ring, FvwmWindow *target,
	int mode, Bool allow_parent, int64_t branch_bonus, int64_t lower_penalty);

/* After prepare, keys are never consulted again. Pop returns children in
 * leaf-first continuation order with its captured client ID; NULL marks completion.
 * The caller resolves that identity before dereferencing a borrowed record.
 * Finish is idempotent. */
FvwmWindow *fvwm_stack_parent_pop(fvwm_stack_parent_path *path, Window *client);
void fvwm_stack_parent_finish(fvwm_stack_parent_path *path);

#endif
