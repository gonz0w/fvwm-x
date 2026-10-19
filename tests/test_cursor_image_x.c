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
#include "fvwm/cursor.h"
#include "libs/Fcursor.h"
#ifdef HAVE_XFIXES
#include <X11/extensions/Xfixes.h>
#endif
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
#include "libs/picture_types.h"
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

static void source(char *path, const char *folder, const char *name,
	const char *data)
{
	assert(snprintf(path, 256, "%s/%s", folder, name) > 0);
	int fd = open(path, O_CREAT | O_WRONLY, 0600);
	size_t length = strlen(data);

	assert(fd >= 0 && write(fd, data, length) == (ssize_t)length);
	assert(close(fd) == 0);
}

static Bool refuse_cursor;

#if XRenderSupport
Cursor __real_XRenderCreateCursor(Display *, Picture, unsigned, unsigned);
Cursor __wrap_XRenderCreateCursor(Display *display, Picture picture,
	unsigned x, unsigned y)
{
	return __real_XRenderCreateCursor(display,
	    refuse_cursor ? None : picture, x, y);
}
#endif

Cursor __real_XCreatePixmapCursor(Display *, Pixmap, Pixmap, XColor *,
	XColor *, unsigned, unsigned);
Cursor __wrap_XCreatePixmapCursor(Display *display, Pixmap source,
	Pixmap mask, XColor *fore, XColor *back, unsigned x, unsigned y)
{
	return __real_XCreatePixmapCursor(display,
	    refuse_cursor ? None : source, mask, fore, back, x, y);
}

static Cursor sampled;
static unsigned sampled_width, sampled_height;

static picture_async_commit accept(FvwmPicture *view, void *context)
{
	(void)view;
	(void)context;
	return PICTURE_ASYNC_APPLY;
}

static void sampled_ready(FvwmPicture *view, Bool applied, void *context)
{
	(void)context;
	assert(applied);
	sampled_width = view->width;
	sampled_height = view->height;
	sampled = picture_async_take_cursor(view);
	assert(sampled != None && picture_async_take_cursor(view) == None);
}

static void ready(Cursor old)
{
	for (unsigned i = 0; i < 10000 && Scr.FvwmCursors[CRS_ROOT] == old; i++)
	{
		pump();
	}
	assert(Scr.FvwmCursors[CRS_ROOT] != old);
}

static void check_cursor(unsigned width, unsigned height,
	unsigned x_hot, unsigned y_hot, unsigned long pixel)
{
#ifdef HAVE_XFIXES
	XSync(dpy, False);
	XFixesCursorImage *image = XFixesGetCursorImage(dpy);

	if (image != NULL && (image->width != width ||
	    image->height != height ||
	    image->xhot != x_hot || image->yhot != y_hot ||
	    (image->pixels[0] & 0xffffffffUL) != pixel))
	{
		fprintf(stderr, "cursor observed %ux%u hot=%u,%u pixel=%08lx; "
		    "expected %ux%u hot=%u,%u pixel=%08lx\n", image->width,
		    image->height, image->xhot, image->yhot,
		    image->pixels[0] & 0xffffffffUL, width, height,
		    x_hot, y_hot,
		    pixel);
	}
	assert(image != NULL && image->width == width &&
	    image->height == height && image->xhot == x_hot &&
	    image->yhot == y_hot && (image->pixels[0] & 0xffffffffUL) == pixel);
	XFree(image);
#else
	(void)width;
	(void)height;
	(void)x_hot;
	(void)y_hot;
	(void)pixel;
#endif
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
	Bool color_cursor = False;

#if defined(HAVE_XCURSOR) && XRenderSupport
	color_cursor = XcursorSupportsARGB(dpy);
#endif
	FScreenInit(dpy);
	Scr.FvwmCursors = CreateCursors(dpy);
	XWarpPointer(dpy, None, Scr.Root, 0, 0, 0, 0, 10, 10);
	XSetErrorHandler(image_error);
	struct sigaction action = {.sa_handler = fvwmReapChildren};
	struct sigaction prior;

	sigemptyset(&action.sa_mask);
	assert(sigaction(SIGCHLD, &action, &prior) == 0);
	assert(image_async_start(dpy, Scr.Root, discard, unexpected, NULL));
	char folder[] = "/tmp/fvwm-cursor-owner-XXXXXX";
	char red[256], blue[256], stall[256], incomplete[256], half[256];
	char xpm[256], compressed[256], refused[256], command[512];

	assert(mkdtemp(folder) != NULL);
	source(red, folder, "red.svg", "red");
	source(blue, folder, "blue.svg", "blue");
	source(stall, folder, "stall.svg", "red");
	source(incomplete, folder, "incomplete.svg", "red");
	source(half, folder, "half.svg", "half");
	source(refused, folder, "cursor-refuse.svg", "red");
	source(xpm, folder, "hotspot.svg",
	    "/* XPM */\nstatic char *x[] = {\n"
	    "\"2 2 2 1 1 0\",\n\". c None\",\n\"x c red\",\n"
	    "\".x\",\n\"xx\"};\n");
	static const unsigned char compressed_xpm[] = {
		0x1f, 0x8b, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02, 0xff,
		0xd3, 0xd7, 0x52, 0x88, 0x08, 0xf0, 0x55, 0xd0, 0xd2, 0xe7,
		0x2a, 0x2e, 0x49, 0x2c, 0xc9, 0x4c, 0x56, 0x48, 0xce, 0x48,
		0x2c, 0x52, 0xd0, 0xaa, 0x88, 0x8e, 0x55, 0xb0, 0x55, 0xa8,
		0xe6, 0x52, 0x32, 0x52, 0x00, 0x41, 0x43, 0x20, 0x34, 0x50,
		0xd2, 0xe1, 0x52, 0xd2, 0x53, 0x48, 0x56, 0xf0, 0xcb, 0xcf,
		0x4b, 0x05, 0xb1, 0x2b, 0x80, 0xec, 0xa2, 0xd4, 0x14, 0xb0,
		0x70, 0x05, 0x58, 0xa0, 0x42, 0xa9, 0xd6, 0x9a, 0x0b, 0x00,
		0x56, 0xa7, 0x55, 0x51, 0x52, 0x00, 0x00, 0x00,
	};

	assert(snprintf(compressed, sizeof(compressed), "%s/hotspot.xpm.gz",
	    folder) > 0);
	int compressed_fd = open(compressed, O_CREAT | O_WRONLY, 0600);

	assert(compressed_fd >= 0 && write(compressed_fd, compressed_xpm,
	    sizeof(compressed_xpm)) == (ssize_t)sizeof(compressed_xpm));
	assert(close(compressed_fd) == 0);
	Cursor original = Scr.FvwmCursors[CRS_ROOT];

	assert(snprintf(command, sizeof(command), "ROOT %s", stall) > 0);
	CMD_CursorStyle(NULL, NULL, command, NULL);
	pump();
	assert(Scr.FvwmCursors[CRS_ROOT] == original);
	assert(snprintf(command, sizeof(command), "ROOT %s 999 0", red) > 0);
	CMD_CursorStyle(NULL, NULL, command, NULL);
	ready(original);
	check_cursor(2, 2, 1, 0,
	    color_cursor ? 0xffff0000 : 0xffffffff);
	original = Scr.FvwmCursors[CRS_ROOT];
	assert(snprintf(command, sizeof(command), "ROOT %s", stall) > 0);
	CMD_CursorStyle(NULL, NULL, command, NULL);
	pump();
	assert(Scr.FvwmCursors[CRS_ROOT] == original);
	assert(snprintf(command, sizeof(command), "ROOT %s", blue) > 0);
	CMD_CursorStyle(NULL, NULL, command, NULL);
	ready(original);
	check_cursor(2, 2, 1, 1,
	    color_cursor ? 0xff0000ff : 0xffffffff);
	original = Scr.FvwmCursors[CRS_ROOT];
	assert(snprintf(command, sizeof(command), "ROOT %s", incomplete) > 0);
	CMD_CursorStyle(NULL, NULL, command, NULL);
	for (unsigned i = 0; i < 300; i++)
	{
		pump();
	}
	assert(Scr.FvwmCursors[CRS_ROOT] == original);
	refuse_cursor = True;
	assert(snprintf(command, sizeof(command), "ROOT %s", refused) > 0);
	CMD_CursorStyle(NULL, NULL, command, NULL);
	for (unsigned i = 0; i < 300; i++)
	{
		pump();
	}
	assert(Scr.FvwmCursors[CRS_ROOT] == original);
	refuse_cursor = False;
	assert(snprintf(command, sizeof(command), "ROOT %s", stall) > 0);
	CMD_CursorStyle(NULL, NULL, command, NULL);
	pump();
	CMD_CursorStyle(NULL, NULL, "ROOT left_ptr", NULL);
	assert(Scr.FvwmCursors[CRS_ROOT] != original);
	original = Scr.FvwmCursors[CRS_ROOT];
#if XpmSupport
	assert(snprintf(command, sizeof(command), "ROOT %s", xpm) > 0);
	CMD_CursorStyle(NULL, NULL, command, NULL);
	ready(original);
	check_cursor(2, 2, 1, 0, 0);
#endif

	/* The independent API lease verifies factor and alpha before transfer.
	 * The raw pixels never travel through a palette or XGetImage copy. */
	FvwmPicture *view = NULL;
	int hotspot[2] = {1, -1};

	assert(image_async_cursor(half, (monitor_scale_factor){8}, hotspot,
	    &view) && view != NULL);
	assert(picture_async_bind(view, accept, sampled_ready, NULL, NULL));
	for (unsigned i = 0; i < 10000 && view->pending; i++)
	{
		pump();
	}
	assert(sampled != None && sampled_width == 4 && sampled_height == 4);
	XDefineCursor(dpy, Scr.Root, sampled);
	check_cursor(4, 4, 2, 2, color_cursor ? 0x80406020 : 0);
	PDestroyFvwmPicture(dpy, view);
	XFreeCursor(dpy, sampled);
#if XpmSupport
	/* The compressed source uses its expanded snapshot metadata. */
	sampled = None;
	hotspot[0] = hotspot[1] = -1;
	assert(image_async_cursor(compressed, (monitor_scale_factor){8},
	    hotspot, &view) && view != NULL);
	assert(picture_async_bind(view, accept, sampled_ready, NULL, NULL));
	for (unsigned i = 0; i < 10000 && view->pending; i++)
	{
		pump();
	}
	assert(sampled != None && sampled_width == 4 && sampled_height == 4);
	XDefineCursor(dpy, Scr.Root, sampled);
	check_cursor(4, 4, 2, 0, 0);
	PDestroyFvwmPicture(dpy, view);
	XFreeCursor(dpy, sampled);
#else
	puts("XPM disabled: hotspot success checks are unavailable; "
	    "SVG cursor checks remain active");
#endif
	CMD_CursorStyle(NULL, NULL, "ROOT left_ptr", NULL);
#ifdef HAVE_XCURSOR
	if (!color_cursor)
	{
		/* The independent Xcursor library can still use ARGB when
		 * FVWM's XRender feature is disabled. With core preference,
		 * its alpha-128 threshold instead yields opaque white. The
		 * approved FVWM fallback keeps its alpha >130 rule. */
		Cursor legacy = PImageLoadCursorFromFile(dpy, Scr.Root, half,
		    1, -1, (monitor_scale_factor){8});

		assert(legacy != None);
		XDefineCursor(dpy, Scr.Root, legacy);
		Bool legacy_argb = XcursorSupportsARGB(dpy);

		check_cursor(4, 4, 2, 2,
		    legacy_argb ? 0x80406020 : 0xffffffff);
		XFreeCursor(dpy, legacy);
		CMD_CursorStyle(NULL, NULL, "ROOT left_ptr", NULL);
		printf("compatibility control: legacy alpha-128 pixel is %s; "
		    "prepared FVWM fallback pixel is transparent\n",
		    legacy_argb ? "ARGB 0x80406020" : "opaque white");
	}
#endif
	assert(image_async_shutdown());
	for (unsigned i = 0; i < 1000 && kill(last_child, 0) == 0; i++)
	{
		pump();
	}
	assert(waitpid(last_child, NULL, WNOHANG) == -1 && errno == ECHILD);
	assert(sigaction(SIGCHLD, &prior, NULL) == 0);
	for (unsigned i = 0; i < CRS_MAX; i++)
	{
		if (Scr.FvwmCursors[i] != None)
		{
			XFreeCursor(dpy, Scr.FvwmCursors[i]);
		}
	}
	XSync(dpy, False);
	assert(errors == 0);
	assert(unlink(red) == 0 && unlink(blue) == 0 && unlink(stall) == 0 &&
	    unlink(incomplete) == 0 && unlink(half) == 0 && unlink(xpm) == 0 &&
	    unlink(compressed) == 0 && unlink(refused) == 0 &&
	    rmdir(folder) == 0);
	XCloseDisplay(dpy);
	printf("cursor alpha/hotspot/scale/replacement/refusal/cancellation "
	    "passed; mode=%s\n", color_cursor ? "ARGB" : "core");
	return 0;
}
