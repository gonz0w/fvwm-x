/* This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */
#include "config.h"
#include "bin/clipboard_policy.h"
#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

/* Independent public-pattern producer and actual executable peer. This test
 * requires a runner-owned private X server; never unlock the child on a user's
 * desktop. Only numbers/results are read from the capture process. */
struct peer { pid_t pid; int input, output, error; uint64_t sequence; };
struct producer { pid_t pid; int input, output; };
struct observed { unsigned targets, payloads; };
static Window retired_requestor;

static uint64_t milliseconds(void)
{
	struct timespec now;

	assert(clock_gettime(CLOCK_MONOTONIC, &now) == 0);
	return (uint64_t)now.tv_sec * 1000 + now.tv_nsec / 1000000;
}

static void pause_ms(unsigned delay)
{
	struct timespec duration = {.tv_sec = delay / 1000,
		.tv_nsec = (long)(delay % 1000) * 1000000};

	while (nanosleep(&duration, &duration) < 0)
		assert(errno == EINTR);
}

static void exact_read(int descriptor, void *output, size_t length)
{
	unsigned char *bytes = output;
	uint64_t end = milliseconds() + 4000;

	while (length)
	{
		struct pollfd fd = {descriptor, POLLIN, 0};
		uint64_t now = milliseconds();

		assert(now < end);
		int ready = poll(&fd, 1, (int)(end - now));

		assert(ready > 0 || (ready < 0 && errno == EINTR));
		if (ready < 0)
			continue;
		ssize_t count = read(descriptor, bytes, length);

		assert(count > 0);
		bytes += count;
		length -= (size_t)count;
	}
}

static int producer_error(Display *display, XErrorEvent *event)
{
	(void)display;
	if (event->error_code == BadWindow && event->resourceid == retired_requestor)
		return 0;
	_exit(70);
}

static void producer_loop(int input, int output)
{
	Display *display = XOpenDisplay(NULL);
	assert(display);
	Atom selection = XInternAtom(display, "CLIPBOARD", False);
	Atom targets = XInternAtom(display, "TARGETS", False);
	Atom text = XInternAtom(display, "UTF8_STRING", False);
	Atom hint = XInternAtom(display, "x-kde-passwordManagerHint", False);
	Atom incr = XInternAtom(display, "INCR", False);
	Window window = XCreateSimpleWindow(display, DefaultRootWindow(display),
		0, 0, 1, 1, 0, 0, 0);
	Window other = XCreateSimpleWindow(display, DefaultRootWindow(display),
		0, 0, 1, 1, 0, 0, 0);
	struct observed counts = {0};
	XSelectionRequestEvent transfer = {0};
	unsigned chunk = 0;
	char mode = 'a';

	XSetErrorHandler(producer_error);
	XSetSelectionOwner(display, selection, window, CurrentTime);
	XSync(display, False);
	assert(write(output, &counts, sizeof(counts)) == sizeof(counts));
	for (;;)
	{
		struct pollfd fds[] = {{input, POLLIN, 0},
			{ConnectionNumber(display), POLLIN, 0}};

		if (!XPending(display))
		{
			int ready = poll(fds, 2, 30000);

			assert(ready > 0 || (ready < 0 && errno == EINTR));
			if (ready < 0)
				continue;
		}
		if (fds[0].revents & (POLLIN | POLLHUP))
		{
			char command;

			if (read(input, &command, 1) != 1 || command == 'q')
				break;
			if (command != '?')
			{
				if (command == 'R')
					mode = 'a'; /* Release without an owner event. */
				else
				{
					mode = command;
					if (mode == 'A')
						XSetSelectionOwner(display, selection,
							other, CurrentTime);
					XSetSelectionOwner(display, selection,
						mode == 'o' ? None : window, CurrentTime);
				}
				XSync(display, False);
			}
			assert(write(output, &counts, sizeof(counts)) == sizeof(counts));
		}
		if (!XPending(display))
			continue;
		XEvent event;

		XNextEvent(display, &event);
		if (event.type == PropertyNotify && transfer.requestor &&
			event.xproperty.window == transfer.requestor &&
			event.xproperty.atom == transfer.property &&
			event.xproperty.state == PropertyDelete)
		{
			unsigned char pattern[4096];

			memset(pattern, 'X', sizeof(pattern));
			retired_requestor = transfer.requestor;
			XChangeProperty(display, transfer.requestor, transfer.property,
				text, 8, PropModeReplace, pattern,
				chunk++ < 3 ? (int)sizeof(pattern) : 0);
			XSync(display, False);
			if (chunk == 4)
				transfer.requestor = None;
		}
		if (event.type != SelectionRequest)
			continue;
		XSelectionRequestEvent request = event.xselectionrequest;
		XEvent reply = {.xselection = {.type = SelectionNotify,
			.requestor = request.requestor, .selection = request.selection,
			.target = request.target, .property = request.property,
			.time = request.time}};

		retired_requestor = request.requestor;
		if (request.target == targets)
		{
			counts.targets++;
			if (mode == 't')
				continue;
			if (mode == 'D')
				pause_ms(650);
			Atom advertised[] = {targets, text, hint};

			if (mode == 'u')
				reply.xselection.property = None;
			else if (mode == 'm')
				XChangeProperty(display, request.requestor,
					request.property, XA_STRING, 8, PropModeReplace,
					(const unsigned char *)"invalid", 7);
			else
				XChangeProperty(display, request.requestor,
					request.property, XA_ATOM, 32, PropModeReplace,
					(const unsigned char *)advertised,
					mode == 's' ? 3 : 2);
		}
		else if (request.target == text)
		{
			counts.payloads++;
			if (mode == 'p')
				continue;
			if (mode == 'i')
			{
				unsigned long length = 12288;

				transfer = request;
				chunk = 0;
				XSelectInput(display, request.requestor, PropertyChangeMask);
				XChangeProperty(display, request.requestor,
					request.property, incr, 32, PropModeReplace,
					(const unsigned char *)&length, 1);
			}
			else {
				const char *bytes = mode >= '1' && mode <= '8' ?
					"abcdefgh" : mode == 'd' ? "abcdefghi" :
					mode == 'b' ? "longer!" : "hello";
				int length = mode >= '1' && mode <= '8' ?
					mode - '0' : mode == 'd' ? 9 : mode == 'b' ? 7 : 5;

				XChangeProperty(display, request.requestor,
					request.property, text, 8, PropModeReplace,
					(const unsigned char *)bytes, length);
			}
		}
		else
			reply.xselection.property = None;
		XSendEvent(display, request.requestor, False, 0, &reply);
		XSync(display, False);
	}
	XCloseDisplay(display);
	_exit(0);
}

static struct producer start_producer(void)
{
	int input[2], output[2];

	assert(pipe(input) == 0 && pipe(output) == 0);
	for (unsigned i = 0; i < 2; i++)
	{
		assert(fcntl(input[i], F_SETFD, FD_CLOEXEC) == 0);
		assert(fcntl(output[i], F_SETFD, FD_CLOEXEC) == 0);
	}
	pid_t pid = fork();

	assert(pid >= 0);
	if (!pid)
	{
		close(input[1]);
		close(output[0]);
		producer_loop(input[0], output[1]);
	}
	close(input[0]);
	close(output[1]);
	struct observed ready;

	exact_read(output[0], &ready, sizeof(ready));
	return (struct producer){pid, input[1], output[0]};
}

static struct observed producer_mode(struct producer producer, char mode)
{
	struct observed counts;

	assert(write(producer.input, &mode, 1) == 1);
	exact_read(producer.output, &counts, sizeof(counts));
	return counts;
}

static struct peer start_capture(const char *path, const char *ttl)
{
	int input[2], output[2], errors[2];

	assert(pipe(input) == 0 && pipe(output) == 0 && pipe(errors) == 0);
	for (unsigned i = 0; i < 2; i++)
	{
		assert(fcntl(input[i], F_SETFD, FD_CLOEXEC) == 0);
		assert(fcntl(output[i], F_SETFD, FD_CLOEXEC) == 0);
		assert(fcntl(errors[i], F_SETFD, FD_CLOEXEC) == 0);
	}
	pid_t pid = fork();

	assert(pid >= 0);
	if (!pid)
	{
		assert(dup2(input[0], STDIN_FILENO) == STDIN_FILENO);
		assert(dup2(output[1], STDOUT_FILENO) == STDOUT_FILENO);
		assert(dup2(errors[1], STDERR_FILENO) == STDERR_FILENO);
		close(input[0]); close(input[1]);
		close(output[0]); close(output[1]);
		close(errors[0]); close(errors[1]);
		execl(path, path, "--serve", "19", "8", "32768", "262144",
			"524288", "262144", ttl, (char *)NULL);
		_exit(71);
	}
	close(input[0]); close(output[1]); close(errors[1]);
	return (struct peer){pid, input[1], output[0], errors[0], 0};
}

static void send_control(struct peer *peer, unsigned verb, uint64_t item,
	uint64_t generation, uint64_t value)
{
	char frame[CLIPBOARD_REPLY_MAX];
	int length = snprintf(frame, sizeof(frame), "1 %llu %u 19 %llu %llu %llu\n",
		(unsigned long long)++peer->sequence, verb,
		(unsigned long long)item, (unsigned long long)generation,
		(unsigned long long)value);

	assert(length > 0 && (size_t)length < sizeof(frame));
	assert(write(peer->input, frame, (size_t)length) == length);
}

static void read_reply(struct peer *peer, uint64_t *reply)
{
	char frame[CLIPBOARD_REPLY_MAX];
	size_t used = 0;

	do
	{
		assert(used < sizeof(frame));
		exact_read(peer->output, frame + used, 1);
	} while (frame[used++] != '\n');
	assert(clipboard_numbers(frame, used - 1, reply, CLIPBOARD_REPLY_FIELDS));
	assert(reply[0] == 1 && reply[1] == peer->sequence && reply[3] == 19);
}

static void control(struct peer *peer, unsigned verb, uint64_t item,
	uint64_t generation, uint64_t value, uint64_t *reply)
{
	send_control(peer, verb, item, generation, value);
	read_reply(peer, reply);
}

static void empty_history(struct peer *peer)
{
	uint64_t reply[CLIPBOARD_REPLY_FIELDS];
	uint64_t end = milliseconds() + 3000;

	control(peer, CLIPBOARD_CLEAR, 0, 0, 0, reply);
	assert(reply[2] == CLIPBOARD_OK);
	do
	{
		pause_ms(20);
		control(peer, CLIPBOARD_STATUS, 0, 0, 0, reply);
		assert(milliseconds() < end);
	} while (reply[11]);
}

static void wait_item(struct peer *peer, size_t length, uint64_t *result)
{
	uint64_t end = milliseconds() + 3000;

	for (;;)
	{
		pause_ms(30);
		for (unsigned i = 0; i < 8; i++)
		{
			control(peer, CLIPBOARD_LIST, 0, 0, i, result);
			if (result[2] == CLIPBOARD_OK && result[7] == length)
				return;
		}
		assert(milliseconds() < end);
	}
}

static void stop_capture(struct peer peer, bool clean)
{
	close(peer.input);
	int status;

	assert(waitpid(peer.pid, &status, 0) == peer.pid);
	assert(WIFEXITED(status) && WEXITSTATUS(status) == (clean ? 0 : 1));
	char diagnostic[1024];
	size_t used = 0;

	for (;;)
	{
		assert(used + 1 < sizeof(diagnostic));
		ssize_t count = read(peer.error, diagnostic + used,
			sizeof(diagnostic) - used - 1);

		if (count < 0 && errno == EINTR)
			continue;
		assert(count >= 0);
		if (!count)
			break;
		used += (size_t)count;
	}
	diagnostic[used] = '\0';
	if (clean)
		assert(used == 0);
	else
		assert(!strcmp(diagnostic,
			"fvwm-clipboard-capture: native cleanup unconfirmed\n"));
	close(peer.error);
	close(peer.output);
}

int main(int argc, char **argv)
{
	assert((argc == 2 || (argc == 3 &&
		(!strcmp(argv[2], "--cleanup-failure") ||
		 !strcmp(argv[2], "--deadline")))) &&
		getenv("FVWM_TEST_PRIVATE_X") &&
		!strcmp(getenv("FVWM_TEST_PRIVATE_X"), "1"));
	signal(SIGPIPE, SIG_IGN);
	struct producer producer = start_producer();
	bool deadline = argc == 3 && !strcmp(argv[2], "--deadline");
	if (deadline)
		producer_mode(producer, 'D');
	struct peer peer = start_capture(argv[1], "10000");
	uint64_t reply[CLIPBOARD_REPLY_FIELDS], first[CLIPBOARD_REPLY_FIELDS];

	control(&peer, CLIPBOARD_LIST, 0, 0, 0, reply);
	assert(reply[2] == CLIPBOARD_BLOCKED && reply[10] == CLIPBOARD_UNKNOWN);
	control(&peer, CLIPBOARD_STATE, 0, 0, CLIPBOARD_UNLOCKED, reply);
	if (deadline)
	{
		pause_ms(1600);
		control(&peer, CLIPBOARD_STATUS, 0, 0, 0, reply);
		struct observed counts = producer_mode(producer, '?');

		assert(reply[11] == 0 && counts.targets > 0 && !counts.payloads);
		stop_capture(peer, true);
		close(producer.input);
		int status;

		assert(waitpid(producer.pid, &status, 0) == producer.pid);
		assert(WIFEXITED(status) && WEXITSTATUS(status) == 0);
		close(producer.output);
		puts("clipboard capture: slow preparation cannot restart deadline");
		return 0;
	}
	wait_item(&peer, 5, first);
	if (argc == 3)
	{
		stop_capture(peer, false);
		close(producer.input);
		int status;
		assert(waitpid(producer.pid, &status, 0) == producer.pid);
		assert(WIFEXITED(status) && WEXITSTATUS(status) == 0);
		close(producer.output);
		puts("clipboard capture: persistent cleanup failure; no clean claim");
		return 0;
	}
	control(&peer, CLIPBOARD_PIN, first[4], first[5], 1, reply);
	assert(reply[2] == CLIPBOARD_OK);
	struct observed repeated = producer_mode(producer, 'a');
	pause_ms(250);
	struct observed converted = producer_mode(producer, '?');
	assert(converted.targets > repeated.targets &&
		converted.payloads > repeated.payloads);
	control(&peer, CLIPBOARD_STATUS, 0, 0, 0, reply);
	assert(reply[11] == 1); /* Exact nonempty dedup, pinned survivor. */
	producer_mode(producer, 'b');
	wait_item(&peer, 7, reply);
	repeated = producer_mode(producer, 'b');
	pause_ms(250);
	converted = producer_mode(producer, '?');
	assert(converted.targets > repeated.targets &&
		converted.payloads > repeated.payloads);
	control(&peer, CLIPBOARD_STATUS, 0, 0, 0, reply);
	assert(reply[11] == 2); /* Unequal-length cursor cannot obstruct equality. */
	control(&peer, CLIPBOARD_STATE, 0, 0, CLIPBOARD_LOCKED, reply);
	control(&peer, CLIPBOARD_LIST, 0, 0, 0, reply);
	assert(reply[2] == CLIPBOARD_BLOCKED && reply[11] == 0);
	control(&peer, CLIPBOARD_STATE, 0, 0, CLIPBOARD_UNLOCKED, reply);
	control(&peer, CLIPBOARD_PIN, first[4], first[5] + 1, 0, reply);
	assert(reply[2] == CLIPBOARD_STALE);
	empty_history(&peer);
	control(&peer, CLIPBOARD_PIN, first[4], first[5], 0, reply);
	assert(reply[2] == CLIPBOARD_STALE);
	for (const char *mode = "smu"; *mode; mode++)
	{
		struct observed before = producer_mode(producer, *mode);

		pause_ms(200);
		struct observed after = producer_mode(producer, '?');

		assert(after.targets > before.targets &&
			after.payloads == before.payloads);
		control(&peer, CLIPBOARD_STATUS, 0, 0, 0, reply);
		assert(reply[11] == 0);
	}
	/* Freeze only the directly owned, unreaped native child. Queue both
	 * its X owner notification and CLEAR before allowing the next dispatch. */
	assert(kill(peer.pid, SIGSTOP) == 0);
	int stopped;

	assert(waitpid(peer.pid, &stopped, WUNTRACED) == peer.pid);
	assert(WIFSTOPPED(stopped));
	repeated = producer_mode(producer, 'a');
	send_control(&peer, CLIPBOARD_CLEAR, 0, 0, 0);
	assert(kill(peer.pid, SIGCONT) == 0);
	read_reply(&peer, reply);
	assert(reply[2] == CLIPBOARD_OK);
	pause_ms(200);
	converted = producer_mode(producer, '?');
	assert(converted.payloads == repeated.payloads);
	control(&peer, CLIPBOARD_STATUS, 0, 0, 0, reply);
	assert(reply[11] == 0);
	producer_mode(producer, 'i');
	wait_item(&peer, 12288, reply);
	empty_history(&peer);
	producer_mode(producer, 'A'); /* Same-window ABA, fresh ordered token. */
	wait_item(&peer, 5, reply);
	empty_history(&peer);
	for (const char *mode = "tp"; *mode; mode++)
	{
		struct observed before = producer_mode(producer, *mode);
		uint64_t end = milliseconds() + 3000;
		struct observed after;

		do
	{
			pause_ms(20);
			after = producer_mode(producer, '?');
			assert(milliseconds() < end);
		} while (*mode == 't' ? after.targets == before.targets :
			after.payloads == before.payloads);
		control(&peer, CLIPBOARD_STATUS, 0, 0, 0, reply);
		producer_mode(producer, 'R'); /* No new owner notification. */
		wait_item(&peer, 5, reply);
		empty_history(&peer);
	}
	/* Fill all eight slots with distinct public byte lengths; full/pinned
	 * history must refuse new capture before asking for payload bytes. */
	for (char mode = '1'; mode <= '8'; mode++)
	{
		producer_mode(producer, mode);
		wait_item(&peer, (size_t)(mode - '0'), reply);
	}
	struct observed full_before = producer_mode(producer, 'd');
	pause_ms(200);
	struct observed full_after = producer_mode(producer, '?');
	assert(full_after.targets > full_before.targets &&
		full_after.payloads == full_before.payloads);
	control(&peer, CLIPBOARD_STATUS, 0, 0, 0, reply);
	assert(reply[11] == 8);
	empty_history(&peer);
	producer_mode(producer, 'p');
	pause_ms(100);
	producer_mode(producer, 'o'); /* Loss during entered transfer. */
	pause_ms(100);
	control(&peer, CLIPBOARD_STATUS, 0, 0, 0, reply);
	assert(reply[11] == 0);
	producer_mode(producer, 'a');
	wait_item(&peer, 5, reply);
	empty_history(&peer);
	producer_mode(producer, 'p');
	pause_ms(100);
	control(&peer, CLIPBOARD_STATE, 0, 0, CLIPBOARD_LOCKED, reply);
	assert(reply[2] == CLIPBOARD_OK && reply[10] == CLIPBOARD_LOCKED);
	control(&peer, CLIPBOARD_LIST, 0, 0, 0, reply);
	assert(reply[2] == CLIPBOARD_BLOCKED);
	stop_capture(peer, true); /* Lock already canceled this producer. */
	repeated = producer_mode(producer, 'p');
	peer = start_capture(argv[1], "10000");
	control(&peer, CLIPBOARD_STATE, 0, 0, CLIPBOARD_UNLOCKED, reply);
	uint64_t eof_end = milliseconds() + 3000;

	do
	{
		pause_ms(20);
		converted = producer_mode(producer, '?');
		assert(milliseconds() < eof_end);
	} while (converted.payloads == repeated.payloads);
	stop_capture(peer, true); /* EOF while its producer is actually entered. */
	producer_mode(producer, 'a');
	peer = start_capture(argv[1], "300");
	control(&peer, CLIPBOARD_STATE, 0, 0, CLIPBOARD_UNLOCKED, reply);
	wait_item(&peer, 5, first);
	control(&peer, CLIPBOARD_PIN, first[4], first[5], 1, reply);
	pause_ms(400);
	control(&peer, CLIPBOARD_PIN, first[4], first[5], 0, reply);
	assert(reply[2] == CLIPBOARD_STALE);
	stop_capture(peer, true);
	close(producer.input);
	int status;

	assert(waitpid(producer.pid, &status, 0) == producer.pid);
	assert(WIFEXITED(status) && WEXITSTATUS(status) == 0);
	close(producer.output);
	puts("clipboard capture: transfer, hints, refs, retention, "
		"lock and EOF passed");
	return 0;
}
