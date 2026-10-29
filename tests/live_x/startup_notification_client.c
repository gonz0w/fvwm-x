/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or any later version.
 *
 * Independent X11 producer/window control for startup_notification.py.
 * It does not include or call the startup observer or launch helper.
 */
#include "config.h"

#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>

#define SLOTS 128
#define LINE_LIMIT 16384

static Display *display;
static Window root;
static Window windows[SLOTS];

static int number(const char *text, unsigned int limit, unsigned int *out)
{
	char *end;
	unsigned long value;

	if (text == NULL || *text == '\0' || *text == '-')
		return 0;
	errno = 0;
	value = strtoul(text, &end, 10);
	if (errno || *end || value > limit)
		return 0;
	*out = (unsigned int)value;
	return 1;
}

static int digit(char value)
{
	if (value >= '0' && value <= '9')
		return value - '0';
	if (value >= 'a' && value <= 'f')
		return value - 'a' + 10;
	return -1;
}

static int unhex(const char *text, unsigned char *out, size_t cap,
	size_t *length)
{
	size_t size, i;
	int high, low;

	*length = 0;
	if (text == NULL)
		return 0;
	if (strcmp(text, "-") == 0)
		return 1;
	size = strlen(text);
	if (size % 2 || size / 2 > cap)
		return 0;
	for (i = 0; i < size / 2; i++)
	{
		high = digit(text[2 * i]);
		low = digit(text[2 * i + 1]);
		if (high < 0 || low < 0)
			return 0;
		out[i] = (unsigned char)(16 * high + low);
	}
	*length = size / 2;
	return 1;
}

static int property(unsigned int slot, const char *text)
{
	unsigned char value[4096];
	size_t length;

	if (!unhex(text, value, sizeof(value), &length))
		return 0;
	XChangeProperty(display, windows[slot],
		XInternAtom(display, "_NET_STARTUP_ID", False),
		XInternAtom(display, "UTF8_STRING", False), 8,
		PropModeReplace, value, (int)length);
	return 1;
}

static int command(char *line)
{
	char *operation, *argument, *second, *third;
	unsigned int slot, other, format, duration;
	unsigned char value[256], second_value[256];
	size_t length, second_length;
	XEvent event;
	XClassHint class_hint;
	XWMHints hints;
	XSizeHints size_hints;
	XWMHints *current_hints;
	XWindowAttributes attributes;
	XSetWindowAttributes set_attributes;
	unsigned char *property_data;
	Atom property_type;
	int property_format;
	unsigned long property_items, property_after;
	unsigned long cardinal, tick;
	unsigned int count, i;
	struct timespec start, now, delay = {0, 2000000};

	operation = strtok(line, " \t\r\n");
	if (operation == NULL)
		return 0;
	if (strcmp(operation, "quit") == 0)
		return 2;
	argument = strtok(NULL, " \t\r\n");
	if (!number(argument, SLOTS - 1, &slot))
		return 0;
	if (strcmp(operation, "create") == 0)
	{
		if (windows[slot] != None)
			return 0;
		windows[slot] = XCreateSimpleWindow(display, root,
			40 + 3 * slot, 40 + 3 * slot, 240, 100, 0, 0, 0xdddddd);
		memset(&size_hints, 0, sizeof(size_hints));
		size_hints.flags = USPosition | USSize;
		size_hints.x = size_hints.y = 40 + 3 * slot;
		size_hints.width = 240;
		size_hints.height = 100;
		XSetWMNormalHints(display, windows[slot], &size_hints);
		XStoreName(display, windows[slot], "Startup fixture client");
	}
	else
	{
		if (windows[slot] == None)
			return 0;
		second = strtok(NULL, " \t\r\n");
		third = strtok(NULL, " \t\r\n");
		if (strcmp(operation, "destroy") == 0)
		{
			XDestroyWindow(display, windows[slot]);
			windows[slot] = None;
		}
		else if (strcmp(operation, "synthetic-destroy") == 0)
		{
			memset(&event, 0, sizeof(event));
			event.xdestroywindow.type = DestroyNotify;
			event.xdestroywindow.display = display;
			event.xdestroywindow.event = root;
			event.xdestroywindow.window = windows[slot];
			/* The server marks this event synthetic; no window is destroyed. */
			if (!XSendEvent(display, root, False,
				SubstructureNotifyMask, &event))
				return 0;
		}
		else if (strcmp(operation, "fragment") == 0)
		{
			if (!number(third, 32, &format) ||
			    (format != 8 && format != 32) ||
			    !unhex(strtok(NULL, " \t\r\n"), value, 20, &length))
				return 0;
			memset(&event, 0, sizeof(event));
			event.xclient.type = ClientMessage;
			event.xclient.display = display;
			event.xclient.window = windows[slot];
			event.xclient.message_type = XInternAtom(display,
				second != NULL && strcmp(second, "begin") == 0 ?
				"_NET_STARTUP_INFO_BEGIN" : "_NET_STARTUP_INFO", False);
			event.xclient.format = (int)format;
			memcpy(event.xclient.data.b, value, length);
			if (!XSendEvent(display, root, False,
				PropertyChangeMask, &event))
				return 0;
		}
		else if (strcmp(operation, "id") == 0)
		{
			if (!property(slot, second))
				return 0;
		}
		else if (strcmp(operation, "bad-id") == 0)
		{
			cardinal = 1;
			XChangeProperty(display, windows[slot],
				XInternAtom(display, "_NET_STARTUP_ID", False),
				XA_CARDINAL, 32, PropModeReplace,
				(unsigned char *)&cardinal, 1);
		}
		else if (strcmp(operation, "clear-id") == 0)
			XDeleteProperty(display, windows[slot],
				XInternAtom(display, "_NET_STARTUP_ID", False));
		else if (strcmp(operation, "group") == 0 ||
			 strcmp(operation, "leader") == 0)
		{
			if (!number(second, SLOTS - 1, &other) ||
			    windows[other] == None)
				return 0;
			if (strcmp(operation, "group") == 0)
			{
				memset(&hints, 0, sizeof(hints));
				current_hints = XGetWMHints(display, windows[slot]);
				if (current_hints != NULL)
				{
					hints = *current_hints;
					XFree(current_hints);
				}
				hints.flags |= WindowGroupHint;
				hints.window_group = windows[other];
				XSetWMHints(display, windows[slot], &hints);
			}
			else
			{
				cardinal = windows[other];
				XChangeProperty(display, windows[slot],
					XInternAtom(display, "WM_CLIENT_LEADER", False),
					XA_WINDOW, 32, PropModeReplace,
					(unsigned char *)&cardinal, 1);
			}
		}
		else if (strcmp(operation, "clear-group") == 0 ||
			 strcmp(operation, "iconic") == 0 ||
			 strcmp(operation, "icon-window") == 0)
		{
			memset(&hints, 0, sizeof(hints));
			current_hints = XGetWMHints(display, windows[slot]);
			if (current_hints != NULL)
			{
				hints = *current_hints;
				XFree(current_hints);
			}
			if (strcmp(operation, "clear-group") == 0)
			{
				hints.flags &= ~WindowGroupHint;
				hints.window_group = None;
			}
			else if (strcmp(operation, "iconic") == 0)
			{
				hints.flags |= StateHint;
				hints.initial_state = IconicState;
			}
			else
			{
				if (!number(second, SLOTS - 1, &other) ||
				    windows[other] == None)
					return 0;
				hints.flags |= IconWindowHint;
				hints.icon_window = windows[other];
			}
			XSetWMHints(display, windows[slot], &hints);
		}
		else if (strcmp(operation, "override") == 0)
		{
			if (!number(second, 1, &other))
				return 0;
			memset(&set_attributes, 0, sizeof(set_attributes));
			set_attributes.override_redirect = other ? True : False;
			XChangeWindowAttributes(display, windows[slot],
				CWOverrideRedirect, &set_attributes);
		}
		else if (strcmp(operation, "reparent") == 0)
		{
			if (!number(second, SLOTS - 1, &other) ||
			    windows[other] == None || other == slot)
				return 0;
			XReparentWindow(display, windows[slot], windows[other], 0, 0);
		}
		else if (strcmp(operation, "unmap") == 0)
			XUnmapWindow(display, windows[slot]);
		else if (strcmp(operation, "withdraw") == 0)
		{
			if (!XWithdrawWindow(display, windows[slot],
				DefaultScreen(display)))
				return 0;
		}
		else if (strcmp(operation, "mask") == 0 ||
			 strcmp(operation, "own-mask") == 0)
		{
			if (!XGetWindowAttributes(display, windows[slot], &attributes))
				return 0;
			cardinal = (unsigned long)(strcmp(operation, "mask") == 0 ?
				attributes.all_event_masks : attributes.your_event_mask);
			printf("OK %s %u %lu\n", operation, slot, cardinal);
			fflush(stdout);
			return 1;
		}
		else if (strcmp(operation, "listed") == 0)
		{
			property_data = NULL;
			cardinal = 0;
			if (XGetWindowProperty(display, root,
				XInternAtom(display, "_NET_CLIENT_LIST", False),
				0, 4096, False, XA_WINDOW, &property_type,
				&property_format, &property_items, &property_after,
				&property_data) != Success)
				return 0;
			if (property_type != None)
			{
				if (property_type != XA_WINDOW || property_format != 32 ||
				    property_items > 4096 || property_after != 0)
				{
					if (property_data != NULL)
						XFree(property_data);
					return 0;
				}
				for (i = 0; i < property_items; i++)
					if (((unsigned long *)property_data)[i] == windows[slot])
						cardinal = 1;
			}
			if (property_data != NULL)
				XFree(property_data);
			printf("OK %s %u %lu\n", operation, slot, cardinal);
			fflush(stdout);
			return 1;
		}
		else if (strcmp(operation, "state") == 0)
		{
			property_data = NULL;
			cardinal = WithdrawnState;
			if (XGetWindowProperty(display, windows[slot],
				XInternAtom(display, "WM_STATE", False), 0, 2, False,
				AnyPropertyType, &property_type, &property_format,
				&property_items, &property_after, &property_data) != Success)
				return 0;
			if (property_type != None)
			{
				if (property_format != 32 || property_items < 1 ||
				    property_after != 0)
				{
					if (property_data != NULL)
						XFree(property_data);
					return 0;
				}
				cardinal = ((unsigned long *)property_data)[0];
			}
			if (property_data != NULL)
				XFree(property_data);
			printf("OK %s %u %lu\n", operation, slot, cardinal);
			fflush(stdout);
			return 1;
		}
		else if (strcmp(operation, "class") == 0)
		{
			if (!unhex(second, value, sizeof(value) - 1, &length) ||
			    !unhex(third, second_value, sizeof(second_value) - 1,
				&second_length) || memchr(value, 0, length) ||
			    memchr(second_value, 0, second_length))
				return 0;
			value[length] = second_value[second_length] = 0;
			class_hint.res_name = (char *)value;
			class_hint.res_class = (char *)second_value;
			XSetClassHint(display, windows[slot], &class_hint);
		}
		else if (strcmp(operation, "map") == 0)
			XMapWindow(display, windows[slot]);
		else if (strcmp(operation, "watch") == 0)
			XSelectInput(display, root, PropertyChangeMask);
		else if (strcmp(operation, "drain") == 0)
		{
			XSync(display, False);
			count = 0;
			while (XPending(display))
			{
				if (++count > 4096)
					return 0;
				XNextEvent(display, &event);
				if (event.type != ClientMessage ||
				    (event.xclient.message_type != XInternAtom(display,
					"_NET_STARTUP_INFO_BEGIN", False) &&
				     event.xclient.message_type != XInternAtom(display,
					"_NET_STARTUP_INFO", False)))
					continue;
				printf("EVENT %lu %u %d ", event.xclient.window,
					event.xclient.message_type == XInternAtom(display,
					"_NET_STARTUP_INFO_BEGIN", False) ? 1 : 0,
					event.xclient.format);
				for (i = 0; i < 20; i++)
					printf("%02x", (unsigned char)event.xclient.data.b[i]);
				putchar('\n');
			}
		}
		else if (strcmp(operation, "busy") == 0)
		{
			if (!number(second, 70000, &duration))
				return 0;
			clock_gettime(CLOCK_MONOTONIC, &start);
			tick = 0;
			do
			{
				XChangeProperty(display, windows[slot],
					XInternAtom(display, "_STARTUP_FIXTURE_TICK", False),
					XA_CARDINAL, 32, PropModeReplace,
					(unsigned char *)&tick, 1);
				XFlush(display);
				tick++;
				nanosleep(&delay, NULL);
				clock_gettime(CLOCK_MONOTONIC, &now);
			} while ((now.tv_sec - start.tv_sec) * 1000 +
				(now.tv_nsec - start.tv_nsec) / 1000000 < duration);
		}
		else
			return 0;
	}
	XSync(display, False);
	printf("OK %s %u %lu\n", operation, slot, windows[slot]);
	fflush(stdout);
	return 1;
}

int main(void)
{
	char line[LINE_LIMIT];
	int result;

	display = XOpenDisplay(NULL);
	if (display == NULL)
	{
		fputs("startup fixture cannot open display\n", stderr);
		return 2;
	}
	root = DefaultRootWindow(display);
	printf("READY %lu %d\n", root, DefaultScreen(display));
	fflush(stdout);
	while (fgets(line, sizeof(line), stdin) != NULL)
	{
		if (strchr(line, '\n') == NULL)
			break;
		result = command(line);
		if (result == 2)
		{
			XCloseDisplay(display);
			return 0;
		}
		if (result == 0)
		{
			fputs("startup fixture invalid command\n", stderr);
			break;
		}
	}
	XCloseDisplay(display);
	return ferror(stdin) ? 2 : 1;
}
