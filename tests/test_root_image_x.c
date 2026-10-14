/* This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */
#include "config.h"
#include "fvwm/execcontext.h"
#include "fvwm/builtins.h"
#include "fvwm/externs.h"
#include "fvwm/image_async.h"
#include "fvwm/commands.h"
#include "libs/FRenderInit.h"
#include "libs/fvwmsignal.h"

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

static unsigned errors;
static unsigned forks;
static pid_t last_child;

pid_t __real_fork(void);
pid_t __wrap_fork(void)
{
	forks++;
	last_child = __real_fork();
	return last_child;
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
	struct timespec delay = {0, 1000000};

	nanosleep(&delay, NULL);
}

static Pixmap current_tile(void)
{
	XGCValues values;

	assert(XGetGCValues(dpy, Scr.XorGC, GCFillStyle | GCTile, &values));
	return values.fill_style == FillTiled ? values.tile : None;
}

static Pixel tile_rgb(void)
{
	Pixmap tile = current_tile();
	XImage *image = XGetImage(dpy, tile, 0, 0, 1, 1, AllPlanes,
	    ZPixmap);
	XColor color;

	assert(image != NULL);
	color.pixel = XGetPixel(image, 0, 0);
	assert(XQueryColor(dpy, DefaultColormap(dpy, Scr.screen), &color));
	XDestroyImage(image);
	return ((Pixel)(color.red >> 8) << 16) |
	    ((Pixel)(color.green >> 8) << 8) | (color.blue >> 8);
}

static void ready(Pixel color)
{
	for (unsigned i = 0; i < 10000; i++)
	{
		pump();
		if (current_tile() != None && tile_rgb() == color)
		{
			return;
		}
	}
	assert(0);
}

static void source(char *path, const char *folder, const char *name,
	const char *data)
{
	assert(snprintf(path, 256, "%s/%s", folder, name) > 0);
	int fd = open(path, O_CREAT | O_WRONLY, 0600);
	size_t length = strlen(data);

	assert(fd >= 0 && write(fd, data, length) == (ssize_t)length);
	assert(close(fd) == 0);
}

int __wrap_main(int argc, char **argv)
{
	assert(argc == 2 && setenv("FVWM_IMAGE_SVG_WORKER", argv[1], 1) == 0);
	alarm(30);
	dpy = XOpenDisplay(NULL);
	assert(dpy != NULL);
	Scr.screen = DefaultScreen(dpy);
	Scr.Root = Scr.NoFocusWin = DefaultRootWindow(dpy);
	PictureInitCMapRoot(dpy, True, NULL, False, True);
	FRenderInit(dpy);
	XSetErrorHandler(image_error);
	struct sigaction action = {.sa_handler = fvwmReapChildren};
	struct sigaction prior;

	sigemptyset(&action.sa_mask);
	assert(sigaction(SIGCHLD, &action, &prior) == 0);
	assert(image_async_start(dpy, Scr.Root, discard, unexpected, NULL));
	char folder[] = "/tmp/fvwm-root-owner-XXXXXX";
	char red[256], blue[256], stall[256], incomplete[256];

	assert(mkdtemp(folder) != NULL);
	source(red, folder, "red.svg", "red");
	source(blue, folder, "blue.svg", "blue");
	source(stall, folder, "stall.svg", "red");
	source(incomplete, folder, "incomplete.svg", "red");
	CMD_XorValue(NULL, NULL, "0", NULL);
	CMD_XorPixmap(NULL, NULL, stall, NULL);
	pump();
	assert(current_tile() == None);
	CMD_XorPixmap(NULL, NULL, red, NULL);
	ready(0xff0000);
	Pixmap old = current_tile();

	CMD_XorPixmap(NULL, NULL, stall, NULL);
	pump();
	assert(current_tile() == old && tile_rgb() == 0xff0000);
	CMD_XorPixmap(NULL, NULL, blue, NULL);
	ready(0x0000ff);
	old = current_tile();
	CMD_XorPixmap(NULL, NULL, incomplete, NULL);
	unsigned before = forks;

	for (unsigned i = 0; i < 300; i++)
	{
		pump();
	}
	assert(forks == before + 1 && current_tile() == old &&
	    tile_rgb() == 0x0000ff);
	CMD_XorPixmap(NULL, NULL, stall, NULL);
	pump();
	CMD_XorValue(NULL, NULL, "0", NULL);
	for (unsigned i = 0; i < 100; i++)
	{
		pump();
	}
	assert(current_tile() == None);
	CMD_XorPixmap(NULL, NULL, red, NULL);
	ready(0xff0000);
	CMD_XorPixmap(NULL, NULL, "", NULL);
	assert(current_tile() == None);
	assert(image_async_shutdown());
	for (unsigned i = 0; i < 1000 && kill(last_child, 0) == 0; i++)
	{
		pump();
	}
	assert(waitpid(last_child, NULL, WNOHANG) == -1 && errno == ECHILD);
	assert(sigaction(SIGCHLD, &prior, NULL) == 0);
	XFreeGC(dpy, Scr.XorGC);
	Scr.XorGC = NULL;
	XSync(dpy, False);
	assert(errors == 0);
	assert(unlink(red) == 0 && unlink(blue) == 0 && unlink(stall) == 0 &&
	    unlink(incomplete) == 0 && rmdir(folder) == 0);
	XCloseDisplay(dpy);
	puts("root tile first/replacement/refusal/reset/cancellation passed");
	return 0;
}
