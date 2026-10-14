/* This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */
#include "config.h"
#include "libs/picture_async.h"
#include "libs/picture_argb.h"
#include "libs/FRenderInit.h"
#include "libs/fvwmsignal.h"

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/wait.h>
#include <unistd.h>

static unsigned forks;
static pid_t last_child;
static Display *display;
static Window root;
static char folder[256];
static picture_async *current_manager;
static unsigned unexpected_errors;
static Atom input_marker;
static Bool input_observed;
static Window ack_window;

static int error_handler(Display *dpy, XErrorEvent *error)
{
	(void)dpy;
	if (!picture_async_handle_error(current_manager, error))
	{
		unexpected_errors++;
		fprintf(stderr,
		    "unowned X error: request=%u error=%u serial=%lu\n",
		    error->request_code, error->error_code, error->serial);
	}
	return 0;
}

static void drain_x(picture_async *manager)
{
	while (XPending(display))
	{
		XEvent event;

		XNextEvent(display, &event);
		if (!picture_async_handle_event(manager, &event) &&
		    event.type == PropertyNotify &&
		    event.xproperty.atom == input_marker)
		{
			input_observed = True;
		}
	}
}

pid_t __real_fork(void);
pid_t __wrap_fork(void)
{
	forks++;
	last_child = __real_fork();
	return last_child;
}

Window __real_XCreateWindow(Display *, Window, int, int, unsigned,
	unsigned, unsigned, int, unsigned, Visual *, unsigned long,
	XSetWindowAttributes *);

Window __wrap_XCreateWindow(Display *dpy, Window parent, int x, int y,
	unsigned width, unsigned height, unsigned border, int depth,
	unsigned window_class, Visual *visual, unsigned long mask,
	XSetWindowAttributes *attributes)
{
	Window window = __real_XCreateWindow(dpy, parent, x, y, width, height,
	    border, depth, window_class, visual, mask, attributes);

	if (window_class == InputOnly && width == 1 && height == 1)
	{
		ack_window = window;
	}
	return window;
}

typedef struct
{
	Bool defer;
	Bool discard;
	unsigned changes;
	FvwmPicture **victim;
} owner_policy;

static picture_async_commit policy(FvwmPicture *view, void *context)
{
	owner_policy *owner = context;

	if (owner->discard || view->count == 1)
	{
		return PICTURE_ASYNC_DISCARD;
	}
	return owner->defer ? PICTURE_ASYNC_DEFER : PICTURE_ASYNC_APPLY;
}

static void changed(FvwmPicture *view, void *context)
{
	owner_policy *owner = context;

	assert(picture_is_usable(view) && !view->pending);
	owner->changes++;
	if (owner->victim != NULL && *owner->victim != NULL)
	{
		PDestroyFvwmPicture(display, *owner->victim);
		*owner->victim = NULL;
	}
}

static int64_t now_ms(void)
{
	struct timespec now;

	assert(clock_gettime(CLOCK_MONOTONIC, &now) == 0);
	return (int64_t)now.tv_sec * 1000 + now.tv_nsec / 1000000;
}

static void poll_manager(picture_async *manager, Bool outer)
{
	struct pollfd endpoints[3];
	int timeout = picture_async_timeout_ms(manager, outer);

	picture_async_descriptors(manager, endpoints);
	endpoints[2] = (struct pollfd){ConnectionNumber(display), POLLIN, 0};
	if (timeout < 0 || timeout > 10)
	{
		timeout = 10;
	}
	assert(poll(endpoints, 3, timeout) >= 0 || errno == EINTR);
	drain_x(manager);
}

static void transport_only(picture_async *manager)
{
	int64_t deadline = now_ms() + 10000;

	while (picture_async_timeout_ms(manager, False) >= 0)
	{
		assert(now_ms() < deadline);
		picture_async_service(manager, False);
		poll_manager(manager, False);
	}
}

static void finish(picture_async *manager, FvwmPicture *view,
	owner_policy *owner)
{
	int64_t deadline = now_ms() + 10000;

	while (view->pending)
	{
		assert(now_ms() < deadline);
		picture_async_service(manager, True);
		picture_async_commit_ready(manager, policy, changed, owner);
		poll_manager(manager, True);
	}
}

static char *file(const char *name, const char *bytes)
{
	char *path;

	int needed = snprintf(NULL, 0, "%s/%s", folder, name);

	assert(needed >= 0);
	path = malloc((size_t)needed + 1);
	assert(path != NULL);
	assert(snprintf(path, (size_t)needed + 1, "%s/%s", folder, name)
	    == needed);
	int fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0600);
	size_t length = strlen(bytes);

	assert(fd >= 0 && write(fd, bytes, length) == (ssize_t)length);
	assert(close(fd) == 0);
	return path;
}

static FvwmPicture *request(const char *path, unsigned quarters,
	unsigned generation)
{
	FvwmPicture *view = PCacheFvwmPictureAtScale(display, root, NULL, path,
	    (FvwmPictureAttributes){0}, (monitor_scale_factor){quarters},
	    0, 0, generation);

	assert(view != NULL && view->is_view && view->pending);
	return view;
}

static FvwmPicture *previous(void)
{
	uint32_t blue = 0xff0000ff;
	picture_argb_builder *builder = picture_argb_start(display, root,
	    &blue, 1, 1, 1, (FvwmPictureAttributes){0});

	assert(builder != NULL && picture_argb_step(builder));
	FvwmPicture *picture = picture_argb_take(builder);

	assert(picture != NULL && picture_argb_destroy(builder));
	return picture;
}

typedef struct
{
	Bool defer;
	Bool applied;
	unsigned settled;
	unsigned released;
	unsigned policies;
	FvwmPicture *view;
} bound_owner;

static picture_async_commit bound_policy(FvwmPicture *view, void *context)
{
	bound_owner *owner = context;

	assert(owner->view == view);
	owner->policies++;
	return owner->defer ? PICTURE_ASYNC_DEFER : PICTURE_ASYNC_APPLY;
}

static void bound_settled(FvwmPicture *view, Bool applied, void *context)
{
	bound_owner *owner = context;

	assert(owner->view == view && !view->pending);
	assert(!applied || picture_is_usable(view));
	owner->settled++;
	owner->applied = applied;
	owner->view = NULL;
	/* The commit batch retains the lease through this callback. */
	PDestroyFvwmPicture(display, view);
}

static void bound_release(void *context)
{
	bound_owner *owner = context;

	owner->released++;
}

static void test_owner_binding(picture_async *manager)
{
	owner_policy fallback = {.discard = True};
	bound_owner owner = {.defer = True};
	char *path = file("bound-owner.svg", "red");

	owner.view = request(path, 4, 1);
	assert(picture_async_bind(owner.view, bound_policy, bound_settled,
	    &owner, bound_release));
	assert(!picture_async_bind(owner.view, bound_policy, bound_settled,
	    &owner, bound_release) && errno == EINVAL);
	int64_t deadline = now_ms() + 10000;

	while (owner.policies == 0)
	{
		assert(now_ms() < deadline);
		picture_async_service(manager, True);
		picture_async_commit_ready(manager, policy, changed, &fallback);
		poll_manager(manager, True);
	}
	assert(owner.view->pending && owner.settled == 0 &&
	    owner.released == 0 && fallback.changes == 0);
	owner.defer = False;
	picture_async_commit_ready(manager, policy, changed, &fallback);
	assert(owner.view == NULL && owner.settled == 1 && owner.applied &&
	    owner.released == 1 && fallback.changes == 0);
	assert(unlink(path) == 0);
	free(path);
	path = file("bound-incomplete.svg", "red");
	owner = (bound_owner){0};
	owner.view = request(path, 4, 1);
	assert(picture_async_bind(owner.view, bound_policy, bound_settled,
	    &owner, bound_release));
	deadline = now_ms() + 10000;
	while (owner.view != NULL)
	{
		assert(now_ms() < deadline);
		picture_async_service(manager, True);
		picture_async_commit_ready(manager, policy, changed, &fallback);
		poll_manager(manager, True);
	}
	assert(owner.settled == 1 && !owner.applied && owner.released == 1);
	assert(unlink(path) == 0);
	free(path);
	path = file("cancel-bound.svg", "red");
	owner = (bound_owner){0};
	owner.view = request(path, 4, 1);
	assert(picture_async_bind(owner.view, bound_policy, bound_settled,
	    &owner, bound_release));
	PDestroyFvwmPicture(display, owner.view);
	owner.view = NULL;
	assert(owner.settled == 0 && owner.released == 1);
	assert(unlink(path) == 0);
	free(path);
}

static void test_coalescing(picture_async *manager)
{
	char *path = file("a name;$(literal).svg", "red");
	FvwmPicture *old = previous();
	FvwmPicture *views[65];
	unsigned before = forks;
	owner_policy owner = {.defer = True};

	for (unsigned i = 0; i < 65; i++)
	{
		views[i] = request(path, 4, 7);
		if (i % 2 == 0)
		{
			picture_view_set_backing(display, views[i], old);
		}
	}
	assert(views[0] != views[1] && old->count == 34);
	assert(forks == before &&
	    picture_async_timeout_ms(manager, False) == -1);
	picture_async_service(manager, False);
	assert(forks == before);
	picture_async_service(manager, True);
	assert(forks == before + 1);
	transport_only(manager);
	for (unsigned i = 0; i < 65; i++)
	{
		assert(views[i]->width == (i % 2 == 0 ? 1 : 0));
	}
	/* Nested transport did not convert or replace any owner's snapshot. */
	for (unsigned i = 0; i < 1000 && picture_async_busy(manager); i++)
	{
		picture_async_service(manager, True);
		picture_async_commit_ready(manager, policy, changed, &owner);
		poll_manager(manager, True);
	}
	assert(!picture_async_busy(manager));
	assert(owner.changes == 0 &&
	    picture_async_timeout_ms(manager, True) == -1);
	assert(old->count == 34 && views[0]->width == 1 &&
	    views[1]->width == 0);
	owner.defer = False;
	owner.victim = &views[10];
	for (unsigned i = 0; i < 4; i++)
	{
		picture_async_commit_ready(manager, policy, changed, &owner);
	}
	assert(owner.changes == 64 && views[10] == NULL);
	for (unsigned i = 0; i < 65; i++)
	{
		if (views[i] != NULL)
		{
			assert(!views[i]->pending && views[i]->width == 2);
			assert(views[i]->backing == views[0]->backing);
			assert(strcmp(views[i]->name, path) == 0);
		}
	}
	assert(old->count == 1 && forks == before + 1);
	FvwmPicture *hit = PCacheFvwmPictureAtScale(display, root, NULL, path,
	    (FvwmPictureAttributes){0}, (monitor_scale_factor){4}, 0, 0, 7);

	assert(hit != NULL && !hit->is_view && hit == views[0]->backing);
	for (unsigned i = 0; i < 65; i++)
	{
		PDestroyFvwmPicture(display, views[i]);
	}
	assert(hit->count == 1);
	PDestroyFvwmPicture(display, hit);
	PDestroyFvwmPicture(display, old);
	assert(unlink(path) == 0);
	free(path);
}

static void test_queue_and_cancel(picture_async *manager)
{
	char *path = file("stall.svg", "red");
	unsigned before = forks;
	FvwmPicture *cancelled = request(path, 4, 1);

	PDestroyFvwmPicture(display, cancelled);
	assert(picture_async_timeout_ms(manager, True) == -1);
	assert(forks == before);
	FvwmPicture *active = request(path, 4, 1);
	FvwmPicture *queued = request(path, 8, 1);

	picture_async_service(manager, True);
	assert(forks == before + 1);
	assert(picture_async_timeout_ms(manager, False) > 0);
	assert(picture_async_timeout_ms(manager, True) > 0);
	PDestroyFvwmPicture(display, queued);
	PDestroyFvwmPicture(display, active);
	transport_only(manager);
	assert(picture_async_timeout_ms(manager, True) == -1);
	assert(fvwm_child_watch_available());
	assert(unlink(path) == 0);
	free(path);
}

static void test_replacement(picture_async *manager, Bool started)
{
	char *path = file("replacement.svg", "red");
	char *swap = file("new-inode.svg", "blu");
	FvwmPicture *old = previous();
	FvwmPicture *view = request(path, 4, 9);
	owner_policy owner = {0};
	unsigned before = forks;

	picture_view_set_backing(display, view, old);
	if (started)
	{
		picture_async_service(manager, True);
		assert(forks == before + 1);
	}
	assert(rename(swap, path) == 0);
	finish(manager, view, &owner);
	assert(owner.changes == 0 && view->backing == old && view->width == 1);
	assert(forks == before + (started ? 1 : 0));
	PDestroyFvwmPicture(display, view);
	PDestroyFvwmPicture(display, old);
	assert(unlink(path) == 0);
	free(path);
	free(swap);
}

static void test_failure_and_discard(picture_async *manager)
{
	char *path = file("incomplete.svg", "red");
	FvwmPicture *view = request(path, 4, 8);
	owner_policy owner = {0};

	finish(manager, view, &owner);
	assert(!picture_is_usable(view) && owner.changes == 0);
	PDestroyFvwmPicture(display, view);
	assert(unlink(path) == 0);
	free(path);
	path = file("old-generation.svg", "red");
	view = request(path, 8, 8);
	FvwmPicture *old = previous();

	picture_view_set_backing(display, view, old);
	owner.discard = True;
	finish(manager, view, &owner);
	assert(owner.changes == 0 && view->backing == old && view->width == 1);
	PDestroyFvwmPicture(display, view);
	PDestroyFvwmPicture(display, old);
	assert(unlink(path) == 0);
	free(path);
	path = file("unusual-extension.data", "red");
	view = request(path, 4, 12);
	owner.discard = False;
	finish(manager, view, &owner);
	assert(owner.changes == 1 && view->width == 2);
	PDestroyFvwmPicture(display, view);
	assert(unlink(path) == 0);
	free(path);
}

static void test_x_service(picture_async *manager)
{
	char *path = file("independent-input.svg", "red");
	FvwmPicture *view = request(path, 4, 1);
	input_marker = XInternAtom(display, "_FVWM_ASYNC_TEST", False);
	input_observed = False;

	XSelectInput(display, root, PropertyChangeMask);
	picture_async_service(manager, True);
	XChangeProperty(display, root, input_marker, XA_STRING, 8,
	    PropModeReplace,
	    (const unsigned char *)"ready", 5);
	XFlush(display);
	int64_t deadline = now_ms() + 5000;

	while (!input_observed)
	{
		assert(now_ms() < deadline && view->pending);
		picture_async_service(manager, False);
		drain_x(manager);
		poll_manager(manager, False);
	}
	assert(!picture_is_usable(view));
	owner_policy owner = {0};

	finish(manager, view, &owner);
	assert(owner.changes == 1);
	PDestroyFvwmPicture(display, view);
	assert(unlink(path) == 0);
	free(path);
}

static void test_server_acknowledgement(picture_async *manager)
{
	char *path = file("scoped-errors.svg", "red");
	FvwmPicture *view = request(path, 4, 1);
	owner_policy owner = {0};

	picture_async_service(manager, True);
	transport_only(manager);
	picture_async_service(manager, True);
	assert(view->pending && !picture_is_usable(view));
	assert(picture_async_timeout_ms(manager, True) == -1);
	XEvent forged = {0};

	forged.xproperty.type = PropertyNotify;
	forged.xproperty.display = display;
	forged.xproperty.window = ack_window;
	forged.xproperty.atom = XA_CARDINAL;
	forged.xproperty.state = PropertyNewValue;
	forged.xproperty.serial = NextRequest(display) - 1;
	forged.xproperty.send_event = True;
	assert(!picture_async_handle_event(manager, &forged));
	forged.xproperty.send_event = False;
	forged.xproperty.serial--;
	assert(picture_async_handle_event(manager, &forged));
	assert(picture_async_timeout_ms(manager, True) == -1);
	/* An unrelated invalid request issued between acknowledged steps must
	 * reach the ordinary handler instead of being hidden by the job. */
	unsigned before = unexpected_errors;

	XDestroyWindow(display, None);
	XFlush(display);
	finish(manager, view, &owner);
	assert(owner.changes == 1 && unexpected_errors == before + 1);
	PDestroyFvwmPicture(display, view);
	assert(unlink(path) == 0);
	free(path);
	path = file("server-refuse.svg", "red");
	view = request(path, 4, 2);
	FvwmPicture *old = previous();

	picture_view_set_backing(display, view, old);
	before = unexpected_errors;
	owner.changes = 0;
	finish(manager, view, &owner);
	assert(owner.changes == 0 && unexpected_errors == before);
	assert(view->backing == old && view->width == 1);
	PDestroyFvwmPicture(display, view);
	PDestroyFvwmPicture(display, old);
	assert(unlink(path) == 0);
	free(path);
}

static void test_format_fallback(picture_async *manager)
{
	const char *xpm = "/* XPM */\nstatic char *image[] = {\n"
	    "\"1 1 1 1\",\n\"r c #ff0000\",\n\"r\"};\n";
	char *path = file("misnamed.svg", xpm);
	FvwmPicture *view = request(path, 4, 19);
	owner_policy owner = {0};
	unsigned before = forks;

	finish(manager, view, &owner);
#if XpmSupport
	assert(owner.changes == 1 && view->width == 1 && view->height == 1);
#else
	assert(owner.changes == 0 && !picture_is_usable(view));
	puts("XPM is disabled: misnamed XPM retains absent-image behavior");
#endif
	assert(forks == before + 1);
	PDestroyFvwmPicture(display, view);
	assert(unlink(path) == 0);
	free(path);

#if XpmSupport
	path = file("ordinary.xpm", xpm);
	before = forks;
	FvwmPicture *picture = PCacheFvwmPicture(display, root, NULL, path,
	    (FvwmPictureAttributes){0});

	assert(picture != NULL && !picture->is_view && !picture->pending);
	assert(forks == before && picture->width == 1);
	PDestroyFvwmPicture(display, picture);
	assert(unlink(path) == 0);
	free(path);

	/* A stored gzip stream with the same XPM text: XPM expansion precedes
	 * SVG for .gz, and never uses the synchronous decompressor adapter. */
	path = file("compressed.data.gz", "");
	FILE *stream = fopen(path, "wb");

	assert(stream != NULL);
	const unsigned char gzip[] = {
	    0x1f, 0x8b, 8, 0, 0, 0, 0, 0, 0, 3};
	assert(fwrite(gzip, 1, sizeof(gzip), stream) == sizeof(gzip));
	size_t length = strlen(xpm);
	unsigned char block[] = {1, length & 255, length >> 8,
	    (~length) & 255, ((~length) >> 8) & 255};
	assert(fwrite(block, 1, sizeof(block), stream) == sizeof(block));
	assert(fwrite(xpm, 1, length, stream) == length);
	uint32_t crc = 0xffffffff;

	for (size_t i = 0; i < length; i++)
	{
		crc ^= (unsigned char)xpm[i];
		for (unsigned j = 0; j < 8; j++)
		{
			crc = (crc >> 1) ^ (0xedb88320U & (0U - (crc & 1U)));
		}
	}
	crc = ~crc;
	for (unsigned i = 0; i < 4; i++)
	{
		assert(fputc((crc >> (i * 8)) & 255, stream) != EOF);
	}
	for (unsigned i = 0; i < 4; i++)
	{
		assert(fputc((length >> (i * 8)) & 255, stream) != EOF);
	}
	assert(fclose(stream) == 0);
	view = request(path, 4, 20);
	before = forks;
	owner.changes = 0;
	picture_async_service(manager, True);
	assert(forks == before + 1);
	transport_only(manager);
	assert(view->pending && owner.changes == 0);
	finish(manager, view, &owner);
	assert(owner.changes == 1 && view->width == 1 && view->height == 1);
	assert(forks == before + 1);
	PDestroyFvwmPicture(display, view);
	assert(unlink(path) == 0);
	free(path);
#endif
}

static void test_raster_and_bitmap(picture_async *manager)
{
#if PngSupport
	const unsigned char png[] = {
	    0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a, 0x00, 0x00,
	    0x00, 0x0d, 0x49, 0x48, 0x44, 0x52, 0x00, 0x00, 0x00, 0x01,
	    0x00, 0x00, 0x00, 0x01, 0x08, 0x06, 0x00, 0x00, 0x00, 0x1f,
	    0x15, 0xc4, 0x89, 0x00, 0x00, 0x00, 0x0d, 0x49, 0x44, 0x41,
	    0x54, 0x78, 0x9c, 0x63, 0xf8, 0xcf, 0xc0, 0xf0, 0x1f, 0x00,
	    0x05, 0x00, 0x01, 0xff, 0x89, 0x99, 0x3d, 0x1d, 0x00, 0x00,
	    0x00, 0x00, 0x49, 0x45, 0x4e, 0x44, 0xae, 0x42, 0x60, 0x82,
	};
	char *path = file("png-named.svg", "");
	int fd = open(path, O_WRONLY | O_TRUNC);

	assert(fd >= 0 && write(fd, png, sizeof(png)) == sizeof(png));
	assert(close(fd) == 0);
	unsigned before = forks;
	FvwmPicture *picture = PCacheFvwmPicture(display, root, NULL, path,
	    (FvwmPictureAttributes){0});

	assert(picture != NULL && !picture->is_view && picture->width == 1);
	assert(forks == before);
	PDestroyFvwmPicture(display, picture);
	assert(unlink(path) == 0);
	free(path);
#endif
	char *bitmap_path = file("bitmap-named.svg",
	    "#define image_width 8\n#define image_height 1\n"
	    "static unsigned char image_bits[] = {\n0xff};\n");
	FvwmPicture *view = request(bitmap_path, 4, 21);
	owner_policy owner = {0};

	finish(manager, view, &owner);
	assert(owner.changes == 1 && view->width == 8 && view->depth == 1);
	PDestroyFvwmPicture(display, view);
	assert(unlink(bitmap_path) == 0);
	free(bitmap_path);

	/* Decompressor refusal must still pass original bytes to SVG. */
	char *refused_path = file("not-gzip.gz", "red");
	unsigned starts = forks;

	view = request(refused_path, 4, 22);
	owner.changes = 0;
	finish(manager, view, &owner);
	assert(owner.changes == 1 && view->width == 2);
	assert(forks == starts + (XpmSupport ? 2 : 1));
	XImage *image = XGetImage(display, view->picture, 0, 0, 1, 1,
	    AllPlanes, ZPixmap);
	XColor color;

	assert(image != NULL);
	color.pixel = XGetPixel(image, 0, 0);
	XQueryColor(display, view->color_cmap, &color);
	assert(color.red > color.blue);
	XDestroyImage(image);
	PDestroyFvwmPicture(display, view);
	assert(unlink(refused_path) == 0);
	free(refused_path);
}

int main(int argc, char **argv)
{
	assert(argc == 2 && setenv("FVWM_IMAGE_SVG_WORKER", argv[1], 1) == 0);
	alarm(40);
	strcpy(folder, "/tmp/fvwm-picture-async-XXXXXX");
	assert(mkdtemp(folder) != NULL);
	display = XOpenDisplay(NULL);
	assert(display != NULL);
	root = DefaultRootWindow(display);
	PictureInitCMapRoot(display, True, NULL, False, True);
	FRenderInit(display);
	XSetErrorHandler(error_handler);
	struct sigaction action = {.sa_handler = fvwmReapChildren};
	struct sigaction previous_action;

	sigemptyset(&action.sa_mask);
	assert(sigaction(SIGCHLD, &action, &previous_action) == 0);
	picture_async *manager = picture_async_create(display, root);

	assert(manager != NULL);
	current_manager = manager;
	assert(picture_async_create(display, root) == NULL && errno == EBUSY);
	test_coalescing(manager);
	test_owner_binding(manager);
	test_queue_and_cancel(manager);
	test_replacement(manager, False);
	test_replacement(manager, True);
	test_failure_and_discard(manager);
	test_x_service(manager);
	test_server_acknowledgement(manager);
	test_format_fallback(manager);
	test_raster_and_bitmap(manager);
	char *path = file("shutdown-stall.svg", "red");
	FvwmPicture *view = request(path, 4, 1);
	FvwmPicture *old = previous();

	picture_view_set_backing(display, view, old);
	picture_async_service(manager, True);
	assert(picture_async_destroy(manager));
	current_manager = NULL;
	assert(!view->pending && view->view_release == NULL);
	assert(view->backing == old && view->width == 1);
	PDestroyFvwmPicture(display, view);
	assert(old->count == 1);
	PDestroyFvwmPicture(display, old);
	manager = picture_async_create(display, root);
	assert(manager != NULL);
	current_manager = manager;
	assert(picture_async_destroy(manager));
	current_manager = NULL;
	assert(unlink(path) == 0);
	free(path);
	assert(rmdir(folder) == 0);
	for (unsigned i = 0; i < 1000 && kill(last_child, 0) == 0; i++)
	{
		struct timespec delay = {0, 1000000};

		nanosleep(&delay, NULL);
	}
	assert(waitpid(last_child, NULL, WNOHANG) == -1 && errno == ECHILD);
	assert(sigaction(SIGCHLD, &previous_action, NULL) == 0);
	XSync(display, False);
	assert(unexpected_errors == 1);
	printf("SVG leases preserve owner snapshots and X service "
	    "at depth %u\n", Pdepth);
	XCloseDisplay(display);
	return 0;
}
