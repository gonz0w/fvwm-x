/* This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */
#include "config.h"
#include "libs/startup_notification_policy.h"

#include <X11/Xlib.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <poll.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#define ANNOUNCE_MS 2000

struct launch_metadata {
	const char *name;
	const char *icon;
	const char *application_id;
	const char *wmclass;
};

static int x_error;

static int announcement_x_error(Display *display, XErrorEvent *event)
{
	(void)display;
	x_error = event->error_code;
	return 0;
}

static int64_t launch_now(void)
{
	struct timespec now;

	if (clock_gettime(CLOCK_MONOTONIC, &now) != 0)
		return -1;
	return (int64_t)now.tv_sec * 1000 + now.tv_nsec / 1000000;
}

static bool append_field(char *message, size_t *used,
	const char *key, const char *value)
{
	size_t key_length = strlen(key);

	if (*used + key_length + 4 >= STARTUP_MESSAGE_MAX)
		return false;
	message[(*used)++] = ' ';
	memcpy(message + *used, key, key_length);
	*used += key_length;
	message[(*used)++] = '=';
	message[(*used)++] = '"';
	for (; *value; value++)
	{
		if (*used + 3 >= STARTUP_MESSAGE_MAX)
			return false;
		if (*value == '"' || *value == '\\')
			message[(*used)++] = '\\';
		message[(*used)++] = *value;
	}
	message[(*used)++] = '"';
	message[*used] = '\0';
	return true;
}

static bool write_announcement(const char *id,
	const struct launch_metadata *metadata, bool remove)
{
	Display *display = XOpenDisplay(NULL);
	Atom atoms[2];
	Window producer, root;
	char message[STARTUP_MESSAGE_MAX];
	char screen[32];
	size_t used;

	if (!display)
		return false;
	XSetErrorHandler(announcement_x_error);
	atoms[0] = XInternAtom(display, "_NET_STARTUP_INFO_BEGIN", False);
	atoms[1] = XInternAtom(display, "_NET_STARTUP_INFO", False);
	root = DefaultRootWindow(display);
	producer = XCreateSimpleWindow(display, root, 0, 0, 1, 1, 0, 0, 0);
	strcpy(message, remove ? "remove:" : "new:");
	used = strlen(message);
	if (!append_field(message, &used, "ID", id))
		return false;
	if (!remove)
	{
		snprintf(screen, sizeof(screen), "%d", DefaultScreen(display));
		if (!append_field(message, &used, "NAME", metadata->name) ||
		    !append_field(message, &used, "SCREEN", screen) ||
		    (metadata->icon && !append_field(message, &used,
			"ICON", metadata->icon)) ||
		    (metadata->application_id && !append_field(message, &used,
			"APPLICATION_ID", metadata->application_id)) ||
		    (metadata->wmclass && !append_field(message, &used,
			"WMCLASS", metadata->wmclass)))
			return false;
	}
	for (size_t offset = 0; offset <= used; offset += 20)
	{
		XEvent event;
		size_t remaining = used + 1 - offset;

		memset(&event, 0, sizeof(event));
		event.xclient.type = ClientMessage;
		event.xclient.display = display;
		event.xclient.window = producer;
		event.xclient.message_type = atoms[offset ? 1 : 0];
		event.xclient.format = 8;
		memcpy(event.xclient.data.b, message + offset,
			remaining < 20 ? remaining : 20);
		if (!XSendEvent(display, root, False,
			PropertyChangeMask, &event))
			return false;
	}
	/* Acknowledgement is sent only after the server processes the full
	 * announcement. The producer's later destruction does not cancel it. */
	XSync(display, False);
	return !x_error;
}

static bool announce(const char *id,
	const struct launch_metadata *metadata, bool remove)
{
	int descriptors[2] = {-1, -1};
	int status = 0;
	int64_t start = launch_now();
	pid_t child;
	bool acknowledged = false, reaped = false;
	struct sigaction previous, normal;

	if (start < 0 || pipe(descriptors) != 0)
		return false;
	memset(&normal, 0, sizeof(normal));
	normal.sa_handler = SIG_DFL;
	sigemptyset(&normal.sa_mask);
	/* Preserve the incoming exec contract after owning/reaping this child,
	 * including an inherited ignored SIGCHLD disposition. */
	if (sigaction(SIGCHLD, &normal, &previous) != 0)
		goto failed;
	child = fork();
	if (child == 0)
	{
		const int signals[] = {SIGALRM, SIGTERM, SIGINT,
			SIGHUP, SIGQUIT,
			SIGPIPE};
		sigset_t empty;

		for (size_t i = 0;
		    i < sizeof(signals) / sizeof(signals[0]); i++)
			if (sigaction(signals[i], &normal, NULL) != 0)
				_exit(1);
		sigemptyset(&empty);
		if (sigprocmask(SIG_SETMASK, &empty, NULL) != 0)
			_exit(1);
		/* Bound the child even if its parent dies while Xlib blocks.
		 * It owns no application and never signals another PID. */
		alarm(2);
		if (dup2(descriptors[1], 3) < 0)
			_exit(1);
#if HAVE_CLOSEFROM
		closefrom(4);
#else
		long open_max = sysconf(_SC_OPEN_MAX);

		if (open_max < 0 || open_max > INT_MAX)
			_exit(1);
		for (int fd = 4; fd < open_max; fd++)
			close(fd);
#endif
		if (write_announcement(id, metadata, remove))
		{
			const unsigned char ack = 1;

			if (write(3, &ack, 1) == 1)
				_exit(0);
		}
		/* Exit closes the X fd. XCloseDisplay could add a blocking
		 * round trip after acknowledgement. Retain no X state. */
		_exit(1);
	}
	close(descriptors[1]);
	descriptors[1] = -1;
	if (child < 0)
	{
		sigaction(SIGCHLD, &previous, NULL);
		goto failed;
	}
	while (!reaped)
	{
		int64_t now = launch_now();
		pid_t waited = waitpid(child, &status, WNOHANG);

		if (waited == child)
		{
			reaped = true;
			break;
		}
		if (waited < 0 && errno != EINTR)
		{
			/* ECHILD means no retained identity. Never signal a PID
			 * after wait says it is no longer ours. */
			reaped = true;
			break;
		}
		if (now < 0 || now - start >= ANNOUNCE_MS)
			break;
		struct pollfd input = {descriptors[0], POLLIN, 0};
		int ready = poll(&input, 1, (int)(ANNOUNCE_MS - (now - start)));

		if (ready > 0 && (input.revents & (POLLIN | POLLHUP)))
		{
			unsigned char ack;

			if (read(descriptors[0], &ack, 1) == 1 && ack == 1)
				acknowledged = true;
			/* EOF can precede waitpid's visibility by a slice;
			 * keep reaping bounded without spinning on POLLHUP. */
			struct timespec pause = {0, 1000000};

			nanosleep(&pause, NULL);
		}
		else if (ready < 0 && errno != EINTR)
			break;
	}
	if (!reaped)
	{
		kill(child, SIGKILL);
		while (waitpid(child, &status, 0) < 0 && errno == EINTR)
			;
	}
	/* A fast child can exit before the first read-ready turn. Drain its
	 * single acknowledgement only after it has been reaped. */
	if (!acknowledged)
	{
		unsigned char ack;

		if (read(descriptors[0], &ack, 1) == 1 && ack == 1)
			acknowledged = true;
	}
	close(descriptors[0]);
	sigaction(SIGCHLD, &previous, NULL);
	return acknowledged;
failed:
	if (descriptors[0] >= 0)
		close(descriptors[0]);
	if (descriptors[1] >= 0)
		close(descriptors[1]);
	return false;
}

static bool launch_id(char id[STARTUP_TEXT_MAX + 1])
{
	unsigned char random[16];
	static const char hex[] = "0123456789abcdef";
	int fd = open("/dev/urandom", O_RDONLY | O_NONBLOCK);
	size_t used = 0;

	if (fd < 0)
		return false;
	while (used < sizeof(random))
	{
		ssize_t n = read(fd, random + used, sizeof(random) - used);

		if (n > 0)
			used += (size_t)n;
		else if (n < 0 && errno == EINTR)
			continue;
		else
			break;
	}
	close(fd);
	if (used != sizeof(random))
		return false;
	memcpy(id, "fvwm-x/", 7);
	for (size_t i = 0; i < sizeof(random); i++)
	{
		id[7 + 2 * i] = hex[random[i] >> 4];
		id[8 + 2 * i] = hex[random[i] & 15];
	}
	strcpy(id + 39, "_TIME0");
	return true;
}

int main(int argc, char **argv)
{
	struct launch_metadata metadata = {0};
	char id[STARTUP_TEXT_MAX + 1];
	int command = 0;
	bool active = false;

	for (int i = 1; i < argc; i++)
	{
		const char **value = NULL;

		if (strcmp(argv[i], "--") == 0)
		{
			command = i + 1;
			break;
		}
		if (strcmp(argv[i], "--name") == 0)
			value = &metadata.name;
		else if (strcmp(argv[i], "--icon") == 0)
			value = &metadata.icon;
		else if (strcmp(argv[i], "--application-id") == 0)
			value = &metadata.application_id;
		else if (strcmp(argv[i], "--wmclass") == 0)
			value = &metadata.wmclass;
		if (!value || *value || ++i >= argc ||
		    !startup_notification_text_valid(
			argv[i], value == &metadata.name))
			goto usage;
		*value = argv[i];
	}
	if (!metadata.name || !command || command >= argc || !argv[command][0])
		goto usage;
	if (unsetenv("DESKTOP_STARTUP_ID") != 0)
	{
		fprintf(stderr, "fvwm-startup-launch: "
			"cannot clear startup environment: %s\n",
			strerror(errno));
		return 126;
	}
	if (launch_id(id) && announce(id, &metadata, false))
	{
		if (setenv("DESKTOP_STARTUP_ID", id, 1) == 0)
			active = true;
		else
			announce(id, &metadata, true);
	}
	execvp(argv[command], argv + command);
	int failure = errno;

	if (active)
		announce(id, &metadata, true);
	fprintf(stderr, "fvwm-startup-launch: cannot execute %s: %s\n",
		argv[command], strerror(failure));
	return failure == ENOENT ? 127 : 126;
usage:
	fprintf(stderr, "usage: fvwm-startup-launch --name NAME [--icon ICON] "
		"[--application-id ID] [--wmclass CLASS] -- "
		"COMMAND [ARG ...]\n");
	return 2;
}
