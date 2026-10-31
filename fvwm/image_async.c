/* This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */
#include "config.h"
#include "image_async.h"
#include "libs/log.h"

#include <errno.h>
#include <unistd.h>

static picture_async *manager;
static pid_t owner;
static picture_async_policy_fn owner_policy;
static picture_async_changed_fn owner_changed;
static void *owner_context;

static picture_async_commit image_async_unbound_policy(
	FvwmPicture *view, void *context)
{
	(void)view;
	(void)context;
	/* Core resources must bind a live owner before publication. */
	fvwm_debug(__func__, "Unbound deferred image refused");
	return PICTURE_ASYNC_DISCARD;
}

static void image_async_unbound_changed(FvwmPicture *view, void *context)
{
	(void)view;
	(void)context;
}

Bool image_async_start_core(Display *dpy, Window drawable)
{
	return image_async_start(dpy, drawable, image_async_unbound_policy,
	    image_async_unbound_changed, NULL);
}

static Bool image_async_is_parent(void)
{
	/* Forked Exec/module children inherit pointers but own none of these
	 * resources. They must never signal the parent's worker. */
	return owner != 0 && owner == getpid();
}

Bool image_async_start(Display *dpy, Window drawable,
	picture_async_policy_fn policy, picture_async_changed_fn changed,
	void *context)
{
	if (owner != 0 || policy == NULL || changed == NULL)
	{
		errno = EINVAL;
		return False;
	}
	manager = picture_async_create(dpy, drawable);
	if (manager == NULL)
	{
		return False;
	}
	owner = getpid();
	owner_policy = policy;
	owner_changed = changed;
	owner_context = context;
	return True;
}

Bool image_async_cursor(const char *path, monitor_scale_factor factor,
	const int hotspot[2], FvwmPicture **view)
{
	return picture_async_cursor(image_async_is_parent() ? manager : NULL,
	    path, factor, hotspot, view);
}

FvwmPicture *image_async_effect(FvwmPicture *source,
	picture_color *background, picture_color *tint,
	unsigned tint_percent, unsigned alpha_percent)
{
	return picture_async_effect(image_async_is_parent() ? manager : NULL,
	    source, background, tint, tint_percent, alpha_percent);
}

FvwmPicture *image_async_background(const struct background_plan *plan,
	unsigned width, unsigned height)
{
	return picture_async_background(image_async_is_parent() ? manager : NULL,
		plan, width, height);
}

Bool image_async_enabled(void)
{
	return image_async_is_parent() && manager != NULL;
}

Bool image_async_shutdown_ready(void)
{
	return picture_async_shutdown_ready(image_async_is_parent() ? manager : NULL);
}

void image_async_outer_boundary(void)
{
	if (image_async_is_parent() && manager != NULL)
	{
		picture_async_service(manager, True);
		picture_async_commit_ready(manager, owner_policy, owner_changed,
		    owner_context);
	}
}

void image_async_transport(void)
{
	if (image_async_is_parent())
	{
		picture_async_service(manager, False);
	}
}

void image_async_descriptors(struct pollfd descriptors[2])
{
	picture_async_descriptors(image_async_is_parent() ? manager : NULL,
	    descriptors);
}

int image_async_timeout_ms(Bool outer_boundary)
{
	return picture_async_timeout_ms(
	    image_async_is_parent() ? manager : NULL, outer_boundary);
}

Bool image_async_handle_event(const XEvent *event)
{
	return image_async_is_parent() &&
	    picture_async_handle_event(manager, event);
}

Bool image_async_handle_error(const XErrorEvent *event)
{
	/* After shutdown the library retains only bounded owned request ranges
	 * so late cleanup errors bypass the ordinary window-error path. */
	return image_async_is_parent() &&
	    picture_async_handle_error(manager, event);
}

Bool image_async_shutdown(void)
{
	if (!image_async_is_parent() || manager == NULL)
	{
		return True;
	}
	if (!picture_async_destroy(manager))
	{
		return False;
	}
	manager = NULL;
	owner_policy = NULL;
	owner_changed = NULL;
	owner_context = NULL;
	return True;
}

void image_async_cancel_worker(void)
{
	if (image_async_is_parent())
	{
		picture_async_cancel_worker(manager);
	}
}
