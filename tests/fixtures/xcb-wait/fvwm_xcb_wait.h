#ifndef FVWM_XCB_WAIT_H
#define FVWM_XCB_WAIT_H

#include <stdint.h>
#include <stdatomic.h>

#define FVWM_XCB_WAIT_CAPACITY 64
#define FVWM_XCB_WAIT_ABI 2

struct fvwm_xcb_wait_record
{
	uint64_t sequence;
	uint32_t wait_calls;
	uint32_t partial_wait_calls;
	int reply_available;
	int completed;
};

struct fvwm_xcb_wait_result
{
	uint32_t abi;
	uint32_t records;
	uint32_t reply_calls;
	uint32_t logical_waits;
	uint32_t wait_calls;
	uint32_t partial_wait_calls;
	int invalid;
	int overflow;
	struct fvwm_xcb_wait_record record[FVWM_XCB_WAIT_CAPACITY];
};

#if defined(__GNUC__)
#define FVWM_XCB_WAIT_PUBLIC __attribute__((visibility("default")))
#define FVWM_XCB_WAIT_PRIVATE __attribute__((visibility("hidden")))
#else
#define FVWM_XCB_WAIT_PUBLIC
#define FVWM_XCB_WAIT_PRIVATE
#endif

/* Optional atomic storage belongs to the caller and must remain live until
 * scope end. Each pointer acknowledges its matching wait branch to the
 * controller.
 */
FVWM_XCB_WAIT_PUBLIC int fvwm_xcb_wait_begin(void *connection,
	_Atomic uint32_t *entered_wait, _Atomic uint32_t *entered_partial);
FVWM_XCB_WAIT_PUBLIC int fvwm_xcb_wait_end(struct fvwm_xcb_wait_result *result);

/* Private hooks are called only by the matching patched XCB implementation. */
FVWM_XCB_WAIT_PRIVATE void fvwm_xcb_wait_reply_begin(void *connection,
	uint64_t sequence);
FVWM_XCB_WAIT_PRIVATE void fvwm_xcb_wait_reply_wait(void *connection,
	uint64_t sequence);
FVWM_XCB_WAIT_PRIVATE void fvwm_xcb_wait_partial(void *connection);
FVWM_XCB_WAIT_PRIVATE void fvwm_xcb_wait_reply_end(void *connection,
	uint64_t sequence, int reply_available);

#endif
