#ifndef FVWM_STACK_RESTACK_H
#define FVWM_STACK_RESTACK_H

#include <stddef.h>
#include "fvwm.h"
#include "libs/Module.h"

/* Numeric packet records are owned across synchronous module callbacks. */
typedef struct
{
	unsigned long *records;
	size_t count;
	Bool send_raise;
	unsigned long raised[3];
} fvwm_stack_restack_snapshot;

/* Normalize the ring boundaries and copy before any callback. False leaves
 * an empty owner and no changes to the ring or managed records. */
Bool fvwm_stack_restack_prepare(
	fvwm_stack_restack_snapshot *snapshot, const FvwmWindow *ring,
	const FvwmWindow *first, const FvwmWindow *last);

/* Fill one bounded packet; the next part repeats its last numeric record.
 * Zero means complete. The offset belongs to this synchronous broadcast. */
size_t fvwm_stack_restack_packet(const fvwm_stack_restack_snapshot *snapshot,
	size_t *offset, unsigned long packet[FvwmPacketMaxSize],
	unsigned long timestamp);

void fvwm_stack_restack_finish(fvwm_stack_restack_snapshot *snapshot);

#endif
