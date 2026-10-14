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
#include "fvwm/menu_image.h"
#include "fvwm/menuitem.h"
#include "fvwm/menuroot.h"
#include "fvwm/menustyle.h"
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

static void finish(FvwmPicture *picture)
{
	assert(picture != NULL);
	for (unsigned i = 0; i < 10000 && picture->pending; i++)
	{
		pump();
	}
	assert(!picture->pending && picture_is_usable(picture));
}

static Pixel pixel_rgb(FvwmPicture *picture)
{
	XImage *image = XGetImage(dpy, picture->picture, 0, 0,
	    picture->width, picture->height, AllPlanes, ZPixmap);
	XColor color;

	assert(image != NULL);
	color.pixel = XGetPixel(image, 0, 0);
	assert(XQueryColor(dpy, Pcmap, &color));
	XDestroyImage(image);
	return ((Pixel)(color.red >> 8) << 16) |
	    ((Pixel)(color.green >> 8) << 8) | (color.blue >> 8);
}

static void check_pixel(FvwmPicture *picture, unsigned width, Pixel pixel)
{
	assert(picture_is_usable(picture) && picture->width == width);
	assert(pixel_rgb(picture) == pixel);
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

static void check_menu_owners(const char *folder)
{
	char path[256];
	MenuRootStatic state = {0}, moved = {0};
	MenuRoot menu = {.s = &state}, continuation = {.s = &moved};
	FvwmPictureAttributes attributes = {.mask = 0};

	source(path, folder, "menu.svg", "red");
	MenuItem *item = menuitem_create();

	MI_PICTURE(item) = PCacheFvwmPicture(dpy, Scr.NoFocusWin, NULL,
	    path, attributes);
	menu_image_bind_root(&menu, &MI_PICTURE(item));
	state.usage_count = 1;
	for (unsigned i = 0; i < 300; i++)
	{
		pump();
	}
	assert(MI_PICTURE(item)->pending && !state.flags.is_updated);
	/* Relocate an item before freeing its old continuation's state. */
	menu_image_bind_root(&continuation, &MI_PICTURE(item));
	memset(&state, 0xff, sizeof(state));
	finish(MI_PICTURE(item));
	assert(moved.flags.is_updated);
	MenuItem *snapshot = menuitem_clone(item);

	assert(MI_PICTURE(snapshot) != MI_PICTURE(item) &&
	    !MI_PICTURE(snapshot)->is_view);
	Pixmap pixels = MI_PICTURE(snapshot)->picture;

	menuitem_free(item);
	assert(MI_PICTURE(snapshot)->picture == pixels);
	check_pixel(MI_PICTURE(snapshot), 2, 0xff0000);
	menuitem_free(snapshot);
	assert(unlink(path) == 0);

	source(path, folder, "style.svg", "blue");
	MenuStyle temporary = {0}, style = {0};

	ST_SIDEPIC(&temporary) = PCacheFvwmPicture(dpy, Scr.NoFocusWin,
	    NULL, path, attributes);
	menu_image_bind_style(&temporary);
	style = temporary;
	menu_image_bind_style(&style);
	memset(&temporary, 0xff, sizeof(temporary));
	style.usage_count = 1;
	for (unsigned i = 0; i < 300; i++)
	{
		pump();
	}
	assert(ST_SIDEPIC(&style)->pending && !ST_IS_UPDATED(&style));
	style.usage_count = 0;
	finish(ST_SIDEPIC(&style));
	assert(ST_IS_UPDATED(&style));
	menu_image_destroy(&ST_SIDEPIC(&style));
	assert(unlink(path) == 0);

	source(path, folder, "menu-cancel.svg", "red");
	item = menuitem_create();
	MI_PICTURE(item) = PCacheFvwmPicture(dpy, Scr.NoFocusWin, NULL,
	    path, attributes);
	menu_image_bind_root(&continuation, &MI_PICTURE(item));
	image_async_outer_boundary();
	menuitem_free(item);
	assert(unlink(path) == 0);
}

/* Reuse unchanged production objects, substituting only the process entry.
 * Exercise actual face ownership without ordinary WM exit-time allocations. */
int __wrap_main(int argc, char **argv)
{
	assert(argc == 2 && setenv("FVWM_IMAGE_SVG_WORKER", argv[1], 1) == 0);
	alarm(30);
	dpy = XOpenDisplay(NULL);
	assert(dpy != NULL);
	PictureInitCMapRoot(dpy, True, NULL, False, True);
	FRenderInit(dpy);
	Scr.NoFocusWin = DefaultRootWindow(dpy);
	XSetErrorHandler(image_error);
	struct sigaction action = {.sa_handler = fvwmReapChildren};
	struct sigaction prior;

	sigemptyset(&action.sa_mask);
	assert(sigaction(SIGCHLD, &action, &prior) == 0);
	assert(image_async_start(dpy, Scr.NoFocusWin, discard, unexpected, NULL));
	char folder[] = "/tmp/fvwm-decor-owner-XXXXXX";
	char red[256], blue[256], stall[256], multi[256], spec[1024];

	assert(mkdtemp(folder) != NULL);
	source(red, folder, "red.svg", "red");
	source(blue, folder, "blue.svg", "blue");
	source(stall, folder, "stall.svg", "red");
	source(multi, folder, "multi.svg", "red");
	DecorFace temporary = {0};
	DecorFace face = {0};

	assert(snprintf(spec, sizeof(spec), "TiledPixmap %s", red) > 0);
	assert(ReadDecorFace(spec, &temporary, -1, True));
	FvwmPicture *view = temporary.u.p;

	assert(view->is_view && view->pending && view->count == 1);
	assert(picture_async_context(view, NULL) == NULL);
	replace_decor_face(&face, &temporary);
	assert(temporary.u.p == NULL && face.u.p == view);
	finish(view);
	assert(face.flags.has_changed && Scr.DefaultDecor.flags.has_changed &&
	    Scr.flags.do_need_window_update);
	check_pixel(view, 2, 0xff0000);
	unsigned before = forks;
	FvwmPicture *scaled = decor_picture_at_scale(view,
	    (monitor_scale_factor){8});

	assert(scaled == NULL);
	for (unsigned i = 0; i < 10000 && scaled == NULL; i++)
	{
		/* Repeated paints must retain, rather than cancel/restart,
		 * their pending scale lease. */
		pump();
		scaled = decor_picture_at_scale(view, (monitor_scale_factor){8});
	}
	assert(scaled != NULL && forks == before + 1);
	check_pixel(scaled, 4, 0xff0000);
	PDestroyFvwmPicture(dpy, scaled);
	assert(snprintf(spec, sizeof(spec), "TiledPixmap %s", blue) > 0);
	assert(ReadDecorFace(spec, &temporary, -1, True));
	replace_decor_face(&face, &temporary);
	view = face.u.p;
	scaled = decor_picture_at_scale(view, (monitor_scale_factor){8});
	check_pixel(scaled, 4, 0xff0000);
	PDestroyFvwmPicture(dpy, scaled);
	finish(view);
	for (unsigned i = 0; i < 10000; i++)
	{
		scaled = decor_picture_at_scale(view, (monitor_scale_factor){8});
		if (scaled != NULL)
		{
			Bool ready = pixel_rgb(scaled) == 0x0000ff;

			PDestroyFvwmPicture(dpy, scaled);
			if (ready)
			{
				break;
			}
		}
		pump();
	}
	scaled = decor_picture_at_scale(view, (monitor_scale_factor){8});
	check_pixel(scaled, 4, 0x0000ff);
	PDestroyFvwmPicture(dpy, scaled);
	FreeDecorFace(dpy, &face);
	assert(snprintf(spec, sizeof(spec), "MultiPixmap Main %s, Buttons %s",
	    multi, multi) > 0);
	assert(ReadDecorFace(spec, &face, -1, True));
	FvwmPicture **parts = face.u.mp.pixmaps;

	/* Buttons expands into independent left/right parts. The parser
	 * releases the intermediate Buttons slot before installation. */
	assert(parts[TBMP_BUTTONS] == NULL &&
	    parts[TBMP_MAIN] != parts[TBMP_LEFT_BUTTONS] &&
	    parts[TBMP_MAIN] != parts[TBMP_RIGHT_BUTTONS] &&
	    parts[TBMP_LEFT_BUTTONS] != parts[TBMP_RIGHT_BUTTONS]);
	finish(parts[TBMP_MAIN]);
	finish(parts[TBMP_LEFT_BUTTONS]);
	finish(parts[TBMP_RIGHT_BUTTONS]);
	FreeDecorFace(dpy, &face);
	check_menu_owners(folder);
	assert(snprintf(spec, sizeof(spec), "Pixmap %s", stall) > 0);
	assert(ReadDecorFace(spec, &face, -1, True));
	image_async_outer_boundary();
	FreeDecorFace(dpy, &face);
	assert(image_async_shutdown());
	for (unsigned i = 0; i < 1000 && kill(last_child, 0) == 0; i++)
	{
		pump();
	}
	assert(waitpid(last_child, NULL, WNOHANG) == -1 && errno == ECHILD);
	assert(sigaction(SIGCHLD, &prior, NULL) == 0);
	XSync(dpy, False);
	assert(errors == 0);
	assert(unlink(red) == 0 && unlink(blue) == 0 && unlink(stall) == 0 &&
	    unlink(multi) == 0 && rmdir(folder) == 0);
	XCloseDisplay(dpy);
	puts("decor/menu transfer, scale retention and cancellation passed");
	return 0;
}
