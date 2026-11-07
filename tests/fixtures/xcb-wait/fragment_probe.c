/* This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License, version 2 or later.
 */
#include "config.h"

#ifndef _GNU_SOURCE
#define _GNU_SOURCE 1
#endif

#include <xcb/xcb.h>
#include <xcb/xcbext.h>
#include <dlfcn.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <poll.h>
#include <pthread.h>
#include <signal.h>
#include <stdatomic.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>
#include "fvwm_xcb_wait.h"

#define TEST_WINDOW UINT32_C(0x200000)
#define TEST_PROPERTY UINT32_C(1)

static const unsigned char payload[16] = "fvwm-xcb-frag-01";

struct peer_control
{
	int fd;
	int fragmented;
	int saw_logical;
	int saw_partial;
	_Atomic uint32_t logical;
	_Atomic uint32_t partial;
	_Atomic uint32_t complete;
	_Atomic uint32_t finish;
};

static void require(int condition, const char *reason)
{
	if (!condition)
	{
		fprintf(stderr, "XCB fragment qualification failed: %s\n",
			reason);
		exit(1);
	}
}

static double monotonic_seconds(void)
{
	struct timespec now;

	require(clock_gettime(CLOCK_MONOTONIC, &now) == 0, "monotonic clock");
	return now.tv_sec + now.tv_nsec / 1000000000.0;
}

static int wait_ack(_Atomic uint32_t *ack)
{
	double deadline = monotonic_seconds() + 3;

	while (!atomic_load_explicit(ack, memory_order_acquire) &&
	       monotonic_seconds() < deadline)
	{
		usleep(1000);
	}
	return atomic_load_explicit(ack, memory_order_acquire) != 0;
}

static void transfer(int fd, unsigned char *bytes, size_t size, int writing)
{
	double deadline = monotonic_seconds() + 3;
	size_t done = 0;

	while (done < size)
	{
		struct pollfd pollfd = {fd, writing ? POLLOUT : POLLIN, 0};
		ssize_t count;
		int ready;

		require(monotonic_seconds() < deadline, "peer I/O deadline");
		ready = poll(&pollfd, 1, 10);
		if (ready < 0 && errno == EINTR)
		{
			continue;
		}
		require(ready >= 0, "peer poll");
		if (ready == 0)
		{
			continue;
		}
		count = writing ? send(fd, bytes + done, size - done, 0) :
			recv(fd, bytes + done, size - done, 0);
		if (count < 0 && (errno == EAGAIN || errno == EWOULDBLOCK ||
			errno == EINTR))
		{
			continue;
		}
		require(count > 0, "peer byte transfer");
		done += (size_t) count;
	}
}

static uint32_t read_wire(const unsigned char *bytes, unsigned int size,
	int little)
{
	uint32_t value = 0;

	for (unsigned int index = 0; index < size; ++index)
	{
		unsigned int shift = little ? index : size - index - 1;

		value |= (uint32_t) bytes[index] << (8 * shift);
	}
	return value;
}

static void write_wire(unsigned char *bytes, uint32_t value,
	unsigned int size, int little)
{
	for (unsigned int index = 0; index < size; ++index)
	{
		unsigned int shift = little ? index : size - index - 1;

		bytes[index] = (unsigned char) (value >> (8 * shift));
	}
}

static void *serve_reply(void *argument)
{
	struct peer_control *control = argument;
	unsigned char prefix[12], setup[40] = {0}, request[24];
	unsigned char reply[48] = {0};
	int little;

	/* This owned socket speaks only one unauthenticated setup and one exact
	 * GetProperty request. It never connects to an external X server.
	 */
	transfer(control->fd, prefix, sizeof(prefix), 0);
	require(prefix[0] == 'l' || prefix[0] == 'B', "setup byte order");
	little = prefix[0] == 'l';
	require(prefix[1] == 0 && read_wire(prefix + 2, 2, little) == 11 &&
		read_wire(prefix + 4, 2, little) == 0 &&
		read_wire(prefix + 6, 2, little) == 0 &&
		read_wire(prefix + 8, 2, little) == 0 &&
		read_wire(prefix + 10, 2, little) == 0,
		"exact unauthenticated setup request");
	setup[0] = 1;
	write_wire(setup + 2, 11, 2, little);
	write_wire(setup + 6, 8, 2, little);
	write_wire(setup + 12, TEST_WINDOW, 4, little);
	write_wire(setup + 16, UINT32_C(0x1fffff), 4, little);
	write_wire(setup + 26, UINT16_MAX, 2, little);
	transfer(control->fd, setup, sizeof(setup), 1);

	transfer(control->fd, request, sizeof(request), 0);
	require(request[0] == XCB_GET_PROPERTY && request[1] == 0 &&
		read_wire(request + 2, 2, little) == 6 &&
		read_wire(request + 4, 4, little) == TEST_WINDOW &&
		read_wire(request + 8, 4, little) == TEST_PROPERTY &&
		read_wire(request + 12, 4, little) == XCB_ATOM_STRING &&
		read_wire(request + 16, 4, little) == 0 &&
		read_wire(request + 20, 4, little) == 4,
		"exact GetProperty request");
	reply[0] = 1;
	reply[1] = 8;
	write_wire(reply + 2, 1, 2, little);
	write_wire(reply + 4, 4, 4, little);
	write_wire(reply + 8, XCB_ATOM_STRING, 4, little);
	write_wire(reply + 16, sizeof(payload), 4, little);
	memcpy(reply + 32, payload, sizeof(payload));
	if (control->fragmented)
	{
		control->saw_logical = wait_ack(&control->logical);
		transfer(control->fd, reply, 32, 1);
		control->saw_partial = wait_ack(&control->partial);
		/* Release the body after a missing acknowledgment. The main
		 * thread rejects the result after joining this owned peer.
		 */
		transfer(control->fd, reply + 32, sizeof(payload), 1);
	}
	else
	{
		/* One complete write precedes the observation scope. Kernel
		 * availability does not imply an XCB-buffered logical reply.
		 */
		require(send(control->fd, reply, sizeof(reply), 0) ==
			(ssize_t) sizeof(reply),
			"complete negative reply write");
		atomic_store_explicit(&control->complete, 1,
			memory_order_release);
	}
	/* Keep EOF outside the measured reply path. Shutdown can otherwise
	 * invalidate the connection before XCB returns its queued reply.
	 */
	require(wait_ack(&control->finish), "main reply consumption complete");
	require(close(control->fd) == 0, "peer close");
	return NULL;
}

static void bind_library(const char *expected)
{
	Dl_info observer, reply;
	char wanted[PATH_MAX], actual[PATH_MAX];

	require(realpath(expected, wanted) != NULL, "private library path");
	require(dladdr((void *) fvwm_xcb_wait_begin, &observer) != 0 &&
		dladdr((void *) xcb_wait_for_reply64, &reply) != 0,
		"private function library identities");
	require(realpath(observer.dli_fname, actual) != NULL &&
		strcmp(actual, wanted) == 0, "private observer library");
	require(realpath(reply.dli_fname, actual) != NULL &&
		strcmp(actual, wanted) == 0, "private reply library");
}

static void run_control(int fragmented)
{
	int sockets[2], flags;
	struct peer_control control = {0};
	struct fvwm_xcb_wait_result result;
	pthread_t thread;
	xcb_connection_t *connection;
	xcb_get_property_cookie_t cookie;
	xcb_get_property_reply_t *reply;
	xcb_generic_error_t *error = NULL;

	require(socketpair(AF_UNIX, SOCK_STREAM, 0, sockets) == 0,
		"owned socket pair");
	control.fd = sockets[1];
	control.fragmented = fragmented;
	atomic_init(&control.logical, 0);
	atomic_init(&control.partial, 0);
	atomic_init(&control.complete, 0);
	atomic_init(&control.finish, 0);
	flags = fcntl(control.fd, F_GETFL);
	require(flags >= 0 &&
		fcntl(control.fd, F_SETFL, flags | O_NONBLOCK) == 0,
		"nonblocking owned peer");
	require(pthread_create(&thread, NULL, serve_reply, &control) == 0,
		"owned peer thread");
	/* XCB owns the client descriptor after this call. */
	connection = xcb_connect_to_fd(sockets[0], NULL);
	require(connection != NULL && xcb_connection_has_error(connection) == 0,
		"private setup accepted");
	cookie = xcb_get_property(connection, 0, TEST_WINDOW, TEST_PROPERTY,
		XCB_ATOM_STRING, 0, 4);
	require(cookie.sequence == 1 && xcb_flush(connection) > 0,
		"exact first request flushed");
	if (!fragmented)
	{
		require(wait_ack(&control.complete),
			"complete negative reply ready");
	}
	require(fvwm_xcb_wait_begin(connection, &control.logical,
		&control.partial), "fragment scope start");
	reply = xcb_get_property_reply(connection, cookie, &error);
	require(fvwm_xcb_wait_end(&result), "fragment scope end");
	atomic_store_explicit(&control.finish, 1, memory_order_release);
	require(pthread_join(thread, NULL) == 0, "owned peer join");
	require(reply != NULL && error == NULL && reply->response_type == 1 &&
		reply->format == 8 && reply->sequence == cookie.sequence &&
		reply->length == 4 && reply->type == XCB_ATOM_STRING &&
		reply->bytes_after == 0 &&
		reply->value_len == sizeof(payload) &&
		xcb_get_property_value_length(reply) == (int) sizeof(payload) &&
		memcmp(xcb_get_property_value(reply), payload,
			sizeof(payload)) == 0,
		"exact reply header and payload");
	require(result.abi == FVWM_XCB_WAIT_ABI && !result.invalid &&
		!result.overflow && result.reply_calls == 1 &&
		result.records == 1 &&
		result.record[0].sequence == cookie.sequence &&
		result.record[0].completed &&
		result.record[0].reply_available &&
		result.record[0].wait_calls == result.wait_calls &&
		result.record[0].partial_wait_calls ==
			result.partial_wait_calls,
		"valid exact reply observation");
	if (fragmented)
	{
		require(control.saw_logical && control.saw_partial &&
			result.logical_waits == 1 && result.wait_calls >= 1 &&
			result.partial_wait_calls >= 1,
			"acknowledged logical and partial waits");
	}
	else
	{
		require(result.partial_wait_calls == 0,
			"complete reply must not enter partial wait");
	}
	printf("{\"case\":\"%s\",\"sequence\":%u,\"reply_calls\":%u,"
		"\"logical_waits\":%u,\"wait_calls\":%u,"
		"\"partial_wait_calls\":%u,\"payload_bytes\":16,"
		"\"valid\":true}\n",
		fragmented ? "fragmented-property" : "complete-property",
		cookie.sequence, result.reply_calls, result.logical_waits,
		result.wait_calls, result.partial_wait_calls);
	free(reply);
	xcb_disconnect(connection);
}

int main(int argc, char **argv)
{
	require(argc == 2, "usage: fragment_probe /absolute/private/libxcb.so");
	/* Exit closes both owned descriptors if a library call stalls. */
	alarm(30);
	require(signal(SIGPIPE, SIG_IGN) != SIG_ERR,
		"ignore owned peer SIGPIPE");
	bind_library(argv[1]);
	run_control(1);
	run_control(0);
	alarm(0);
	puts("{\"status\":\"passed\",\"controls\":2,"
		"\"scope\":\"owned fragmented GetProperty transport\"}");
	return 0;
}
