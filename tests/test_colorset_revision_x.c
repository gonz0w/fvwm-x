/* This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */
#include "config.h"
#include "fvwm/execcontext.h"
#include "fvwm/colorset.h"
#include "fvwm/fvwm.h"
#include "fvwm/screen.h"
#include "libs/Colorset.h"
#include "fvwm/externs.h"
#include "fvwm/image_async.h"
#include "libs/FRenderInit.h"
#include "libs/Picture.h"
#include "libs/PictureUtils.h"

#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

static FvwmPicture *retained;
static Bool capture;
static unsigned errors;

Bool __real_picture_async_bind(FvwmPicture *, picture_async_policy_fn,
    picture_async_settled_fn, void *, void (*)(void *));

Bool __wrap_picture_async_bind(FvwmPicture *view, picture_async_policy_fn policy,
    picture_async_settled_fn settled, void *context,
    void (*finalize)(void *))
{
	if (capture)
	{
		assert(retained == NULL);
		retained = PCloneFvwmPicture(view);
		capture = False;
	}
	return __real_picture_async_bind(view, policy, settled, context, finalize);
}

Bool __wrap_FRenderGetExtensionSupported(void)
{
	return False;
}

static int image_error(Display *display, XErrorEvent *event)
{
	(void)display;
	if (!image_async_handle_error(event))
	{
		errors++;
	}
	return 0;
}

static picture_async_commit discard(FvwmPicture *view, void *context)
{
	(void)view;
	(void)context;
	return PICTURE_ASYNC_DISCARD;
}

static void unexpected(FvwmPicture *view, void *context)
{
	(void)view;
	(void)context;
	assert(0);
}

static void pump(void)
{
	image_async_outer_boundary();
	XFlush(dpy);
	while (XPending(dpy))
	{
		XEvent event;

		XNextEvent(dpy, &event);
		image_async_handle_event(&event);
	}
	struct pollfd descriptors[3];

	image_async_descriptors(descriptors);
	descriptors[2] = (struct pollfd){ConnectionNumber(dpy), POLLIN, 0};
	assert(poll(descriptors, 3, 1) >= 0 || errno == EINTR);
}

int __wrap_main(int argc, char **argv)
{
	assert(argc == 2 && setenv("FVWM_IMAGE_EFFECT_WORKER", argv[1], 1) == 0);
	alarm(30);
	dpy = XOpenDisplay(NULL);
	assert(dpy != NULL);
	PictureInitCMapRoot(dpy, True, NULL, False, True);
	FRenderInit(dpy);
	Scr.NoFocusWin = DefaultRootWindow(dpy);
	Scr.Root = DefaultRootWindow(dpy);
	XSetErrorHandler(image_error);
	assert(image_async_start(dpy, Scr.NoFocusWin, discard, unexpected, NULL));
	char temporary[] = "/tmp/fvwm-colorset-revision-XXXXXX";
	char path[256];
	int fd = mkstemp(temporary);
	static const unsigned char png[] = {
	0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a, 0x00, 0x00, 0x00, 0x0d,
	0x49, 0x48, 0x44, 0x52, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x02,
	0x08, 0x02, 0x00, 0x00, 0x00, 0xfd, 0xd4, 0x9a, 0x73, 0x00, 0x00, 0x00,
	0x10, 0x49, 0x44, 0x41, 0x54, 0x78, 0x9c, 0x63, 0xf8, 0xcf, 0xc0, 0x00,
	0x44, 0x0c, 0x10, 0x0a, 0x00, 0x1f, 0xee, 0x03, 0xfd, 0x8b, 0x5f, 0x14,
	0xd4, 0x00, 0x00, 0x00, 0x00, 0x49, 0x45, 0x4e, 0x44, 0xae, 0x42, 0x60,
	0x82,
	};

	assert(fd >= 0 && write(fd, png, sizeof(png)) == sizeof(png));
	assert(close(fd) == 0);
	assert(snprintf(path, sizeof(path), "%s.png", temporary) > 0);
	assert(rename(temporary, path) == 0);
	char command[512];

	assert(snprintf(command, sizeof(command),
	    "bg blue, Pixmap %s, Tint green 50, ImageAlpha 50", path) > 0);
	for (unsigned revision = 0; revision < 4; revision++)
	{
		capture = True;
		parse_colorset(1, command);
		assert(retained != NULL && retained->pending);
		/* Model the scheduler batch's temporary reference. Plain removes
		 * the owner interest and publishes before that reference retires. */
		parse_colorset(1, "Plain");
		assert(GetColorset(1)->preparation == NULL && GetColorset(1)->pixmap == None);
		for (unsigned turn = 0; retained->pending; turn++)
		{
			assert(turn < 10000);
			pump();
		}
		assert(!picture_is_usable(retained));
		assert(GetColorset(1)->pixmap == None);
		PDestroyFvwmPicture(dpy, retained);
		retained = NULL;
		parse_colorset(1, command);
		for (unsigned turn = 0; GetColorset(1)->preparation != NULL; turn++)
		{
			assert(turn < 10000);
			pump();
		}
		assert(GetColorset(1)->pixmap != None);
		parse_colorset(1, "Plain");
		cleanup_colorsets();
	}
	assert(image_async_shutdown());
	XSync(dpy, False);
	assert(errors == 0);
	assert(unlink(path) == 0);
	XCloseDisplay(dpy);
	puts("Cancelled commit-batch views retain expired revision contexts");
	return 0;
}
