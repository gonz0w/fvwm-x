/* This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License, version 2 or later.
 */
#include "config.h"

#include <assert.h>
#include <stddef.h>
#include <stdatomic.h>
#include <stdint.h>
#include "fvwm_xcb_wait.h"

int main(void)
{
	int first, other;
	_Atomic uint32_t entered = 0;
	struct fvwm_xcb_wait_result result;

	assert(!fvwm_xcb_wait_end(&result) && result.invalid);
	assert(!fvwm_xcb_wait_begin(NULL, NULL));
	assert(fvwm_xcb_wait_begin(&first, &entered));
	fvwm_xcb_wait_reply_begin(&other, 1);
	fvwm_xcb_wait_reply_wait(&other, 1);
	fvwm_xcb_wait_partial(&other);
	fvwm_xcb_wait_reply_end(&other, 1, 1);
	fvwm_xcb_wait_partial(&first);
	assert(fvwm_xcb_wait_end(&result));
	assert(result.records == 0 && result.partial_wait_calls == 0);
	assert(atomic_load(&entered) == 0);

	assert(fvwm_xcb_wait_begin(&first, &entered));
	fvwm_xcb_wait_reply_begin(&first, 12);
	fvwm_xcb_wait_reply_end(&first, 12, 1);
	fvwm_xcb_wait_reply_begin(&first, 13);
	fvwm_xcb_wait_reply_wait(&first, 13);
	fvwm_xcb_wait_reply_wait(&first, 13);
	fvwm_xcb_wait_partial(&first);
	fvwm_xcb_wait_partial(&other);
	fvwm_xcb_wait_reply_end(&first, 13, 1);
	assert(atomic_load(&entered) == 1);
	assert(fvwm_xcb_wait_end(&result));
	assert(result.reply_calls == 2 && result.records == 2);
	assert(result.logical_waits == 1 && result.wait_calls == 2);
	assert(result.partial_wait_calls == 1);
	assert(result.record[0].sequence == 12 &&
		result.record[0].wait_calls == 0 && result.record[0].completed);
	assert(result.record[1].sequence == 13 &&
		result.record[1].wait_calls == 2 &&
		result.record[1].partial_wait_calls == 1 &&
		result.record[1].completed && result.record[1].reply_available);

	assert(fvwm_xcb_wait_begin(&first, NULL));
	assert(!fvwm_xcb_wait_begin(&other, NULL));
	assert(!fvwm_xcb_wait_end(&result) && result.invalid);
	assert(fvwm_xcb_wait_begin(&first, NULL));
	fvwm_xcb_wait_reply_begin(&first, 1);
	assert(!fvwm_xcb_wait_end(&result) && result.invalid);
	assert(fvwm_xcb_wait_begin(&first, NULL));
	fvwm_xcb_wait_reply_wait(&first, 1);
	assert(!fvwm_xcb_wait_end(&result) && result.invalid);
	assert(fvwm_xcb_wait_begin(&first, NULL));
	fvwm_xcb_wait_reply_begin(&first, 1);
	fvwm_xcb_wait_reply_end(&first, 2, 0);
	assert(!fvwm_xcb_wait_end(&result) && result.invalid);

	assert(fvwm_xcb_wait_begin(&first, NULL));
	for (uint64_t sequence = 0; sequence <= FVWM_XCB_WAIT_CAPACITY;
	     ++sequence)
	{
		fvwm_xcb_wait_reply_begin(&first, sequence);
		fvwm_xcb_wait_reply_end(&first, sequence, 1);
	}
	assert(!fvwm_xcb_wait_end(&result));
	assert(result.invalid && result.overflow);
	assert(result.records == FVWM_XCB_WAIT_CAPACITY);
	return 0;
}
