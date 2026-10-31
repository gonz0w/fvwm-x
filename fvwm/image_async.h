#ifndef FVWM_IMAGE_ASYNC_H
#define FVWM_IMAGE_ASYNC_H

#include "libs/picture_async.h"

/* One startup/shutdown lifecycle per process. Call from the WM parent only,
 * with callbacks that resolve current live owners. */
Bool image_async_start_core(Display *dpy, Window drawable);
Bool image_async_start(Display *dpy, Window drawable,
	picture_async_policy_fn policy, picture_async_changed_fn changed,
	void *context);
Bool image_async_cursor(const char *path, monitor_scale_factor factor,
	const int hotspot[2], FvwmPicture **view);
FvwmPicture *image_async_effect(FvwmPicture *source,
	picture_color *background, picture_color *tint,
	unsigned tint_percent, unsigned alpha_percent);
/* Terminal event-loop service retains effect reader leases until actual reap. */
Bool image_async_shutdown_ready(void);
FvwmPicture *image_async_background(const struct background_plan *plan,
	unsigned width, unsigned height);
Bool image_async_enabled(void);
void image_async_outer_boundary(void);
void image_async_transport(void);
void image_async_descriptors(struct pollfd descriptors[2]);
int image_async_timeout_ms(Bool outer_boundary);
Bool image_async_handle_event(const XEvent *event);
Bool image_async_handle_error(const XErrorEvent *event);
Bool image_async_shutdown(void);
/* X connection loss: stop the owned worker without issuing X requests. */
void image_async_cancel_worker(void);

#endif
