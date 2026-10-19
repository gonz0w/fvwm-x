/* This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */
#include "config.h"
#include "fvwm/image_async.h"
#include "fvwm/image_async_wait.h"
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
static unsigned worker_forks;
static unsigned changes;
static unsigned unexpected_errors;
static pid_t last_child;
static Display *display;
static Bool defer_commit;

pid_t __real_fork(void);
pid_t __wrap_fork(void)
{
	worker_forks++;
	last_child = __real_fork();
	return last_child;
}

static int error_handler(Display *dpy, XErrorEvent *error)
{
	(void)dpy;
	if (!image_async_handle_error(error))
	{
		unexpected_errors++;
	}
	return 0;
}

static picture_async_commit policy(FvwmPicture *view, void *context)
{
	assert(context == &changes && view->count >= 2);
	return defer_commit ? PICTURE_ASYNC_DEFER : PICTURE_ASYNC_APPLY;
}

static void changed(FvwmPicture *view, void *context)
{
	assert(context == &changes && picture_is_usable(view));
	assert(!view->pending);
	changes++;
}

static void wait_once(Bool outer, int ready_fd)
{
	struct pollfd descriptors[4] = {
		{ConnectionNumber(display), POLLIN, 0}, {ready_fd, POLLIN, 0}
	};
	struct timeval timeout = {0, 10000};
	bool image_timeout;

	image_async_descriptors(&descriptors[2]);
	XFlush(display);
	int result = image_async_wait(descriptors, 4, &timeout,
		image_async_timeout_ms(outer), &image_timeout);

	assert(result >= 0 || errno == EINTR);
	if (ready_fd >= 0)
	{
		assert(result > 0 && (descriptors[1].revents & POLLIN));
	}
	image_async_transport();
	while (XPending(display))
	{
		XEvent event;

		XNextEvent(display, &event);
		image_async_handle_event(&event);
	}
}

static FvwmPicture *request(const char *path)
{
	FvwmPicture *view = PCacheFvwmPictureAtScale(display,
	    DefaultRootWindow(display), NULL, path,
	    (FvwmPictureAttributes){0}, (monitor_scale_factor){4}, 0, 0, 1);

	assert(view != NULL && view->is_view && view->pending);
	return view;
}

int main(int argc, char **argv)
{
	assert(argc == 2 && setenv("FVWM_IMAGE_SVG_WORKER", argv[1], 1) == 0);
	alarm(20);
	display = XOpenDisplay(NULL);
	assert(display != NULL);
	PictureInitCMapRoot(display, True, NULL, False, True);
	FRenderInit(display);
	XSetErrorHandler(error_handler);
	struct sigaction action = {.sa_handler = fvwmReapChildren};
	struct sigaction previous;

	sigemptyset(&action.sa_mask);
	assert(sigaction(SIGCHLD, &action, &previous) == 0);
	char folder[] = "/tmp/fvwm-image-core-XXXXXX";
	char path[256];

	assert(mkdtemp(folder) != NULL);
	assert(snprintf(path, sizeof(path), "%s/first.svg", folder) > 0);
	int fd = open(path, O_CREAT | O_WRONLY, 0600);

	assert(fd >= 0 && write(fd, "red", 3) == 3 && close(fd) == 0);
	assert(!image_async_start(display, DefaultRootWindow(display),
	    NULL, changed, &changes) && errno == EINVAL);
	assert(image_async_start(display, DefaultRootWindow(display),
	    policy, changed, &changes));
	FvwmPicture *view = request(path);

	image_async_transport();
	assert(worker_forks == 0 && image_async_timeout_ms(False) == -1);
	assert(image_async_timeout_ms(True) == 0);
	image_async_outer_boundary();
	assert(worker_forks == 1);
	int timer[2];

	assert(pipe(timer) == 0 && write(timer[1], "t", 1) == 1);
	/* A synthetic due timer remains independently readable during the
	 * worker delay. Nested service cannot change an owner. */
	for (unsigned i = 0;
	     i < 1000 && image_async_timeout_ms(False) >= 0; i++)
	{
		wait_once(False, timer[0]);
		assert(changes == 0 && view->pending &&
		    !picture_is_usable(view));
		struct timespec delay = {0, 1000000};

		nanosleep(&delay, NULL);
	}
	assert(image_async_timeout_ms(False) == -1);
	assert(image_async_timeout_ms(True) == 0);
	assert(close(timer[0]) == 0 && close(timer[1]) == 0);
	defer_commit = True;
	for (unsigned i = 0; i < 1000 && image_async_timeout_ms(True) >= 0; i++)
	{
		image_async_outer_boundary();
		wait_once(True, -1);
	}
	assert(changes == 0 && view->pending && !picture_is_usable(view));
	/* Deferred owner does not spin. */
	assert(image_async_timeout_ms(True) == -1);
	defer_commit = False;
	image_async_outer_boundary();
	assert(changes == 1 && !view->pending && picture_is_usable(view));
	PDestroyFvwmPicture(display, view);
	assert(unlink(path) == 0);
	assert(snprintf(path, sizeof(path),
	    "%s/shutdown-stall.svg", folder) > 0);
	fd = open(path, O_CREAT | O_WRONLY, 0600);
	assert(fd >= 0 && write(fd, "red", 3) == 3 && close(fd) == 0);
	view = request(path);
	image_async_outer_boundary();
	assert(worker_forks == 2);
	pid_t worker = last_child;
	/* Exec/module children inherit the shim but may not service or cancel
	 * the parent's worker. Real fork keeps worker counts exact. */
	sigset_t blocked, prior_mask;

	sigemptyset(&blocked);
	sigaddset(&blocked, SIGCHLD);
	assert(sigprocmask(SIG_BLOCK, &blocked, &prior_mask) == 0);
	pid_t child = __real_fork();

	assert(child >= 0);
	if (child == 0)
	{
		image_async_cancel_worker();
		assert(image_async_shutdown());
		image_async_outer_boundary();
		assert(image_async_timeout_ms(True) == -1);
		_exit(0);
	}
	int status = 0;

	pid_t waited = waitpid(child, &status, 0);

	assert(waited == child && WIFEXITED(status) &&
	    WEXITSTATUS(status) == 0);
	assert(sigprocmask(SIG_SETMASK, &prior_mask, NULL) == 0);
	assert(kill(worker, 0) == 0);
	unsigned long serial = NextRequest(display);

	image_async_cancel_worker();
	assert(NextRequest(display) == serial); /* Fatal path issues no X. */
	assert(image_async_shutdown() && image_async_shutdown());
	assert(!view->pending && view->view_release == NULL);
	PDestroyFvwmPicture(display, view);
	for (unsigned i = 0; i < 1000 && kill(worker, 0) == 0; i++)
	{
		struct timespec delay = {0, 1000000};

		nanosleep(&delay, NULL);
	}
	assert(waitpid(worker, NULL, WNOHANG) == -1 && errno == ECHILD);
	assert(sigaction(SIGCHLD, &previous, NULL) == 0);
	XSync(display, False);
	assert(unexpected_errors == 0);
	assert(unlink(path) == 0 && rmdir(folder) == 0);
	XCloseDisplay(display);
	puts("core image adapter preserves nested servicing and owner commits");
	return 0;
}
