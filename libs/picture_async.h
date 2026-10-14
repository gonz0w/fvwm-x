#ifndef FVWMLIB_PICTURE_ASYNC_H
#define FVWMLIB_PICTURE_ASYNC_H

#include "picture_view.h"
#include "picture_color.h"
#include <poll.h>

typedef struct picture_async picture_async;
typedef enum
{
	PICTURE_ASYNC_APPLY,
	PICTURE_ASYNC_DEFER,
	PICTURE_ASYNC_DISCARD
} picture_async_commit;
typedef picture_async_commit (*picture_async_policy_fn)(FvwmPicture *view,
	void *context);
typedef void (*picture_async_changed_fn)(FvwmPicture *view, void *context);
typedef void (*picture_async_settled_fn)(FvwmPicture *view, Bool applied,
	void *context);

/* Bind one pending lease directly to its owner; no resource-owner scan is
 * needed at completion. The context finalizer runs on last lease release.
 * settled runs once after success or refusal, but never for a deferred lease
 * or one cancelled by its owner. It may release the owner's reference. */
Bool picture_async_bind(FvwmPicture *view, picture_async_policy_fn policy,
	picture_async_settled_fn settled, void *context,
	void (*release)(void *));
/* Borrow a binding's context only when its callback identifies that owner
 * type. Used to relocate/invalidate a lease when its containing face moves. */
void *picture_async_context(FvwmPicture *view,
	picture_async_settled_fn settled);

/* Opt-in process-local SVG loader. The drawable must outlive the manager.
 * One worker is active; equivalent cache requests share decoding but receive
 * separate owner-specific views. Queue entries own keys, not image buffers. */
picture_async *picture_async_create(Display *dpy, Window drawable);
/* True means handled (including refusal with *view == NULL). Known native
 * cursors and ordinary PNG/XPM/bitmap inputs retain the existing synchronous
 * cursor path. Deferred cursor jobs share the same worker/readiness/ack owner. */
Bool picture_async_cursor(picture_async *manager, const char *path,
	monitor_scale_factor factor, const int hotspot[2], FvwmPicture **view);
/* File-color effect preparation shares the same one-active-job queue. Borrows
 * source/color owners here and retains them through worker exit and conversion.
 * The returned uncached view must be bound to its revision owner. */
FvwmPicture *picture_async_effect(picture_async *manager, FvwmPicture *source,
	picture_color *background, picture_color *tint,
	unsigned tint_percent, unsigned alpha_percent);
/* Cancel the active effect reader and step its reap without releasing source
 * leases. False means shutdown must keep servicing before closing X. */
Bool picture_async_shutdown_ready(picture_async *manager);
/* Transfer the prepared cursor from one ready, uncached cursor lease. */
Cursor picture_async_take_cursor(FvwmPicture *view);
void picture_async_descriptors(picture_async *manager,
	struct pollfd descriptors[2]);
int picture_async_timeout_ms(const picture_async *manager, Bool outer_boundary);
/* Always pumps at most one transport step. Only an outer safe boundary may
 * prepare sources/start workers or convert pixels. Nested loops pass False. */
void picture_async_service(picture_async *manager, Bool outer_boundary);
/* Forward ordinary X errors/events through these ownership filters. They only
 * record owned errors/acknowledgements, never invoke layout or draw callbacks.
 * Unrelated errors/events return False for the existing dispatcher/handler. */
Bool picture_async_handle_error(picture_async *manager,
	const XErrorEvent *error);
Bool picture_async_handle_event(picture_async *manager, const XEvent *event);
Bool picture_async_busy(const picture_async *manager);
/* Fatal X I/O path only: cancel transport without touching X resources.
 * The process must exit afterwards; this is not normal owner teardown. */
void picture_async_cancel_worker(picture_async *manager);
/* At most 32 live views are inspected per call. Call only at the owner's safe
 * boundary; DEFER pins its old snapshot, DISCARD preserves old/absent fields.
 * Callbacks receive a temporarily retained view, never a borrowed window. */
void picture_async_commit_ready(picture_async *manager,
	picture_async_policy_fn policy, picture_async_changed_fn changed,
	void *context);
/* Shutdown with FVWM's child reaper still installed. Cancels without waiting,
 * detaches all surviving leases, and releases only owned resources. */
Bool picture_async_destroy(picture_async *manager);

#endif
