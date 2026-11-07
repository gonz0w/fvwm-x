/* This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License, version 2 or later.
 */
#include "config.h"

#ifndef _GNU_SOURCE
#define _GNU_SOURCE 1
#endif

#include <X11/Xlib.h>
#include <xcb/xcb.h>
#include <xcb/xcbext.h>
#include <dlfcn.h>
#include <limits.h>
#include <pthread.h>
#include <signal.h>
#include <stdatomic.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include "fvwm_xcb_wait.h"

struct release_control
{
	Display *display;
	_Atomic uint32_t entered;
	int saw_wait;
};

static void require(int condition, const char *reason)
{
	if (!condition)
	{
		fprintf(stderr, "XCB wait qualification failed: %s\n", reason);
		exit(1);
	}
}

static double monotonic_seconds(void)
{
	struct timespec now;

	require(clock_gettime(CLOCK_MONOTONIC, &now) == 0, "monotonic clock");
	return now.tv_sec + now.tv_nsec / 1000000000.0;
}

static void *release_server(void *argument)
{
	struct release_control *control = argument;
	double deadline = monotonic_seconds() + 3;

	while (!atomic_load_explicit(&control->entered, memory_order_acquire) &&
	       monotonic_seconds() < deadline)
	{
		usleep(1000);
	}
	control->saw_wait = atomic_load_explicit(&control->entered,
		memory_order_acquire) != 0;
	/* Always release this connection's grab, including missing-hook failures. */
	XUngrabServer(control->display);
	XSync(control->display, False);
	return NULL;
}

static void start_controller(struct release_control *control, pthread_t *thread,
	Display *display)
{
	control->display = display;
	control->saw_wait = 0;
	atomic_init(&control->entered, 0);
	XGrabServer(display);
	XSync(display, False);
	if (pthread_create(thread, NULL, release_server, control) != 0)
	{
		XUngrabServer(display);
		XSync(display, False);
		require(0, "controller thread");
	}
}

static void check_result(const char *name,
	const struct fvwm_xcb_wait_result *result, uint32_t reply_calls,
	int positive)
{
	require(result->abi == FVWM_XCB_WAIT_ABI && !result->invalid &&
		!result->overflow && result->reply_calls == reply_calls,
		"observer scope or call count");
	if (positive)
	{
		require(result->logical_waits == 1 && result->wait_calls >= 1,
			"outstanding reply must enter wait branch");
	}
	else
	{
		require(result->logical_waits == 0 && result->wait_calls == 0 &&
			result->partial_wait_calls == 0, "zero wait control");
	}
	for (uint32_t index = 0; index < result->records; ++index)
	{
		require(result->record[index].completed,
			"unfinished reply observation");
	}
	printf("{\"case\":\"%s\",\"reply_calls\":%u,"
		"\"logical_waits\":%u,\"wait_calls\":%u,"
		"\"partial_wait_calls\":%u,\"valid\":true}\n",
		name, result->reply_calls, result->logical_waits,
		result->wait_calls, result->partial_wait_calls);
}

static void get_focus(xcb_connection_t *connection,
	xcb_get_input_focus_cookie_t cookie)
{
	xcb_generic_error_t *error = NULL;
	xcb_get_input_focus_reply_t *reply;

	reply = xcb_get_input_focus_reply(connection, cookie, &error);
	require(reply != NULL && error == NULL, "input focus reply");
	free(reply);
}

static void bound_library(const char *expected, void *xlib)
{
	Dl_info observer, reply;
	char wanted[PATH_MAX], actual[PATH_MAX];
	void *resolved;

	require(realpath(expected, wanted) != NULL, "private library path");
	require(dladdr((void *) fvwm_xcb_wait_begin, &observer) != 0 &&
		dladdr((void *) xcb_wait_for_reply64, &reply) != 0,
		"private function library identities");
	require(realpath(observer.dli_fname, actual) != NULL &&
		strcmp(actual, wanted) == 0, "observer loaded from private library");
	require(realpath(reply.dli_fname, actual) != NULL &&
		strcmp(actual, wanted) == 0, "reply API loaded from private library");
	resolved = dlsym(xlib, "xcb_wait_for_reply64");
	require(resolved == (void *) xcb_wait_for_reply64,
		"Xlib dependency resolves the same private XCB reply API");
}

int main(int argc, char **argv)
{
	Display *controller, *display;
	xcb_connection_t *connection, *other, *xlib_connection;
	xcb_connection_t *(*get_connection)(Display *);
	void *xlib_xcb, *xlib;
	xcb_get_input_focus_cookie_t first, barrier;
	struct fvwm_xcb_wait_result result;
	struct release_control release;
	pthread_t thread;
	Window root;
	int revert;

	require(argc == 2, "usage: probe /absolute/private/libxcb.so.1.1.0");
	/* Process termination closes the controller connection and releases its
	 * owned server grab even if an X library call exceeds the whole deadline.
	 */
	alarm(30);
	require(XInitThreads() != 0, "Xlib thread initialization");
	xlib = dlopen("libX11.so.6", RTLD_NOW | RTLD_LOCAL);
	xlib_xcb = dlopen("libX11-xcb.so.1", RTLD_NOW | RTLD_LOCAL);
	require(xlib != NULL && xlib_xcb != NULL, "existing Xlib/XCB bridge");
	get_connection = (xcb_connection_t *(*)(Display *))
		dlsym(xlib_xcb, "XGetXCBConnection");
	require(get_connection != NULL, "XGetXCBConnection");
	bound_library(argv[1], xlib);
	controller = XOpenDisplay(NULL);
	display = XOpenDisplay(NULL);
	connection = xcb_connect(NULL, NULL);
	other = xcb_connect(NULL, NULL);
	require(controller != NULL && display != NULL &&
		xcb_connection_has_error(connection) == 0 &&
		xcb_connection_has_error(other) == 0, "owned X connections");
	xlib_connection = get_connection(display);
	require(xlib_connection != NULL, "Xlib connection identity");
	XSync(display, False);
	get_focus(connection, xcb_get_input_focus(connection));
	get_focus(other, xcb_get_input_focus(other));

	require(fvwm_xcb_wait_begin(connection, NULL), "async scope start");
	xcb_no_operation(connection);
	require(xcb_flush(connection) > 0, "async flush");
	require(fvwm_xcb_wait_end(&result), "async scope end");
	check_result("async", &result, 0, 0);

	first = xcb_get_input_focus(connection);
	barrier = xcb_get_input_focus(connection);
	get_focus(connection, barrier);
	require(fvwm_xcb_wait_begin(connection, NULL), "buffered scope start");
	get_focus(connection, first);
	require(fvwm_xcb_wait_end(&result), "buffered scope end");
	check_result("buffered", &result, 1, 0);

	start_controller(&release, &thread, controller);
	require(fvwm_xcb_wait_begin(connection, &release.entered),
		"direct outstanding scope start");
	get_focus(connection, xcb_get_input_focus(connection));
	require(fvwm_xcb_wait_end(&result), "direct outstanding scope end");
	require(pthread_join(thread, NULL) == 0 && release.saw_wait,
		"direct outstanding controller acknowledgment");
	check_result("outstanding-xcb", &result, 1, 1);

	start_controller(&release, &thread, controller);
	require(fvwm_xcb_wait_begin(xlib_connection, &release.entered),
		"Xlib outstanding scope start");
	require(XGetInputFocus(display, &root, &revert) != 0,
		"Xlib input focus reply");
	require(fvwm_xcb_wait_end(&result), "Xlib outstanding scope end");
	require(pthread_join(thread, NULL) == 0 && release.saw_wait,
		"Xlib outstanding controller acknowledgment");
	check_result("outstanding-xlib", &result, 1, 1);

	start_controller(&release, &thread, controller);
	require(fvwm_xcb_wait_begin(connection, &release.entered),
		"checked-void scope start");
	{
		xcb_void_cookie_t cookie = xcb_no_operation_checked(connection);
		xcb_generic_error_t *error = xcb_request_check(connection, cookie);
		require(error == NULL, "checked void result");
	}
	require(fvwm_xcb_wait_end(&result), "checked-void scope end");
	require(pthread_join(thread, NULL) == 0 && release.saw_wait,
		"checked-void controller acknowledgment");
	check_result("checked-void", &result, 1, 1);

	require(fvwm_xcb_wait_begin(connection, NULL), "other scope start");
	get_focus(other, xcb_get_input_focus(other));
	require(fvwm_xcb_wait_end(&result), "other scope end");
	check_result("other-connection", &result, 0, 0);

	xcb_disconnect(other);
	xcb_disconnect(connection);
	XCloseDisplay(display);
	XCloseDisplay(controller);
	dlclose(xlib_xcb);
	dlclose(xlib);
	alarm(0);
	puts("{\"status\":\"passed\",\"controls\":6,"
		"\"native_partial_read_control\":\"pending\","
		"\"scope\":\"calling-thread logical reply waits\"}");
	return 0;
}
