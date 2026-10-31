/* This program is free software; you can redistribute it and/or modify
 * it under the GNU General Public License, version 2 or any later version.
 * Independent actual-X client for the standalone tray fixture. */
#include "config.h"
#include "libs/cJSON.h"
#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>
#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define CLIENT_LIMIT 64
#define COMMAND_LIMIT 4096
#define LINE_LIMIT 8192
#define EVENT_LIMIT 512

static Display *display;
static Window root, owner_window;
static Window windows[CLIENT_LIMIT];
static Atom selection, info_atom, embed_atom, opcode_atom, manager_atom;
static Atom host_atom;
static unsigned int window_count;
static Window reading_window;
static int expected_errors, unexpected_errors;

static int on_error(Display *dpy, XErrorEvent *event)
{
	(void)dpy;
	if (reading_window && event->resourceid == reading_window &&
	    event->error_code == BadWindow &&
	    (event->request_code == 3 || event->request_code == 15))
		expected_errors++;
	else
	{
		unexpected_errors++;
		if (unexpected_errors <= 16)
			fprintf(stderr, "tray fixture X error code=%u request=%u "
				"minor=%u resource=%lu serial=%lu\n",
				(unsigned int)event->error_code,
				(unsigned int)event->request_code,
				(unsigned int)event->minor_code,
				event->resourceid, event->serial);
	}
	return 0;
}

static bool integer(cJSON *object, const char *name, uint32_t fallback,
	uint32_t *result)
{
	cJSON *item = cJSON_GetObjectItemCaseSensitive(object, name);
	double value;

	*result = fallback;
	if (!item)
		return true;
	if (!cJSON_IsNumber(item))
		return false;
	value = item->valuedouble;
	if (!isfinite(value) || value < 0 || value > UINT32_MAX ||
	    value != (double)(uint32_t)value)
		return false;
	*result = (uint32_t)value;
	return true;
}

static const char *text(cJSON *object, const char *name, const char *fallback)
{
	cJSON *item = cJSON_GetObjectItemCaseSensitive(object, name);

	if (!item)
		return fallback;
	if (!cJSON_IsString(item) || !item->valuestring ||
	    strlen(item->valuestring) > 256)
		return NULL;
	return item->valuestring;
}

static bool owned(Window window)
{
	for (unsigned int i = 0; i < window_count; i++)
		if (windows[i] == window)
			return true;
	return false;
}

static bool parent_of(Window window, Window *parent)
{
	Window actual_root = None, *children = NULL;
	unsigned int count = 0;
	*parent = None;
	Status got = XQueryTree(display, window, &actual_root, parent,
	    &children, &count);
	if (children)
		XFree(children);
	return got && actual_root == root;
}

static bool resize_repaint(Window window, uint32_t width, uint32_t height,
	uint32_t count, cJSON *result)
{
	Window socket = None, host = None;
	Atom type = None;
	int format = 0;
	unsigned long items = 0, after = 0;
	unsigned char *data = NULL;
	XWindowAttributes attributes = {0}, host_attributes;
	int errors_before = unexpected_errors;
	XGrabServer(display);
	Window owner = XGetSelectionOwner(display, selection);
	bool valid = owner != None &&
	    XGetWindowAttributes(display, window, &attributes) &&
	    parent_of(window, &socket) && socket != root &&
	    parent_of(socket, &host) && host != root && host != socket &&
	    XGetWindowProperty(display, owner, host_atom, 0, 1, False,
	        XA_WINDOW, &type, &format, &items, &after, &data) == Success &&
	    type == XA_WINDOW && format == 32 && items == 1 && after == 0 &&
	    data && ((unsigned long *)data)[0] == host &&
	    XGetWindowAttributes(display, host, &host_attributes) &&
	    host_attributes.class == InputOutput &&
	    host_attributes.map_state == IsViewable;
	Bool override = attributes.override_redirect;
	if (data)
		XFree(data);
	if (valid)
	{
		/* One bounded, independently validated fixture host. The real
		 * ConfigureNotify follows every queued Expose, so an older
		 * native projection must not overwrite the new direct size. */
		for (uint32_t i = 0; i < count; i++)
			XClearArea(display, host, 0, 0, 1, 1, True);
		unsigned long serial = NextRequest(display);
		XResizeWindow(display, window, width, height);
		XSync(display, False);
		cJSON_AddNumberToObject(result, "resize_serial", serial);
		cJSON_AddNumberToObject(result, "host", host);
		cJSON_AddNumberToObject(result, "expose_count", count);
		cJSON_AddBoolToObject(result, "override", override);
	}
	XUngrabServer(display);
	XSync(display, False);
	return valid && errors_before == unexpected_errors;
}

static void send_message(Window receiver, Window source, Atom atom,
	int format, long opcode, long detail, long first, long second)
{
	XEvent event = {0};

	event.xclient.type = ClientMessage;
	event.xclient.display = display;
	event.xclient.window = source;
	event.xclient.message_type = atom;
	event.xclient.format = format;
	event.xclient.data.l[0] = CurrentTime;
	event.xclient.data.l[1] = opcode;
	event.xclient.data.l[2] = detail;
	event.xclient.data.l[3] = first;
	event.xclient.data.l[4] = second;
	XSendEvent(display, receiver, False, NoEventMask, &event);
}

static cJSON *events(void)
{
	cJSON *array = cJSON_CreateArray();
	unsigned int count = 0;

	XSync(display, False);
	while (XPending(display) && count++ < EVENT_LIMIT)
	{
		XEvent event;
		cJSON *record = cJSON_CreateObject();

		XNextEvent(display, &event);
		cJSON_AddNumberToObject(record, "window", event.xany.window);
		cJSON_AddNumberToObject(record, "type", event.type);
		cJSON_AddBoolToObject(record, "synthetic", event.xany.send_event);
		if (event.type == ClientMessage && event.xclient.format == 32 &&
		    event.xclient.message_type == embed_atom)
		{
			cJSON_AddStringToObject(record, "kind", "xembed");
			cJSON_AddNumberToObject(record, "opcode",
				(uint32_t)event.xclient.data.l[1]);
			cJSON_AddNumberToObject(record, "detail",
				(uint32_t)event.xclient.data.l[2]);
			cJSON_AddNumberToObject(record, "data1",
				(uint32_t)event.xclient.data.l[3]);
			cJSON_AddNumberToObject(record, "data2",
				(uint32_t)event.xclient.data.l[4]);
		}
		else if (event.type == ClientMessage &&
		    event.xclient.message_type == manager_atom &&
		    event.xclient.format == 32)
		{
			cJSON_AddStringToObject(record, "kind", "manager");
			cJSON_AddNumberToObject(record, "timestamp",
				(uint32_t)event.xclient.data.l[0]);
			cJSON_AddNumberToObject(record, "selection",
				(uint32_t)event.xclient.data.l[1]);
			cJSON_AddNumberToObject(record, "owner",
				(uint32_t)event.xclient.data.l[2]);
		}
		else if (event.type == ConfigureNotify)
		{
			cJSON_AddStringToObject(record, "kind", "configure");
			cJSON_AddNumberToObject(record, "serial",
			    event.xconfigure.serial);
			cJSON_AddNumberToObject(record, "width",
			    event.xconfigure.width);
			cJSON_AddNumberToObject(record, "height",
			    event.xconfigure.height);
			cJSON_AddNumberToObject(record, "x",
			    event.xconfigure.x);
			cJSON_AddNumberToObject(record, "y",
			    event.xconfigure.y);
			cJSON_AddBoolToObject(record, "override",
			    event.xconfigure.override_redirect);
		}
		else if (event.type == KeyPress || event.type == KeyRelease)
		{
			cJSON_AddStringToObject(record, "kind", "key");
			cJSON_AddNumberToObject(record, "keysym",
				XLookupKeysym(&event.xkey, 0));
			cJSON_AddNumberToObject(record, "code", event.xkey.keycode);
		}
		else if (event.type == ButtonPress || event.type == ButtonRelease)
			cJSON_AddStringToObject(record, "kind", "button");
		else
			cJSON_AddStringToObject(record, "kind", "structure");
		cJSON_AddItemToArray(array, record);
	}
	if (XPending(display))
		unexpected_errors++;
	return array;
}

static cJSON *state(Window window)
{
	cJSON *result = cJSON_CreateObject();
	XWindowAttributes attributes;
	Window actual_root = None, parent = None, *children = NULL;
	unsigned int count = 0;
	int before = expected_errors;
	bool exists;

	XSync(display, False);
	reading_window = window;
	exists = XGetWindowAttributes(display, window, &attributes) != 0;
	if (exists)
		exists = XQueryTree(display, window, &actual_root, &parent,
			&children, &count) != 0;
	XSync(display, False);
	reading_window = None;
	exists = exists && before == expected_errors;
	if (children)
		XFree(children);
	cJSON_AddBoolToObject(result, "exists", exists);
	if (exists)
	{
		cJSON_AddNumberToObject(result, "root", actual_root);
		cJSON_AddNumberToObject(result, "parent", parent);
		cJSON_AddNumberToObject(result, "x", attributes.x);
		cJSON_AddNumberToObject(result, "y", attributes.y);
		cJSON_AddNumberToObject(result, "width", attributes.width);
		cJSON_AddNumberToObject(result, "height", attributes.height);
		cJSON_AddNumberToObject(result, "map", attributes.map_state);
		cJSON_AddNumberToObject(result, "depth", attributes.depth);
		cJSON_AddBoolToObject(result, "override", attributes.override_redirect);
	}
	return result;
}

static cJSON *execute(cJSON *command)
{
	const char *verb = text(command, "verb", "");
	uint32_t id, width, height, flags, version, value, format, parent;
	cJSON *result = cJSON_CreateObject();

	if (!verb || !integer(command, "id", 0, &id))
		goto invalid;
	if (!strcmp(verb, "identity"))
	{
		cJSON_AddNumberToObject(result, "pid", getpid());
		cJSON_AddNumberToObject(result, "root", root);
		cJSON_AddNumberToObject(result, "selection", selection);
	}
	else if (!strcmp(verb, "owner"))
		cJSON_AddNumberToObject(result, "owner",
			XGetSelectionOwner(display, selection));
	else if (!strcmp(verb, "claim"))
	{
		XSetSelectionOwner(display, selection, owner_window, CurrentTime);
		XSync(display, False);
		cJSON_AddNumberToObject(result, "owner",
			XGetSelectionOwner(display, selection));
	}
	else if (!strcmp(verb, "release"))
	{
		if (XGetSelectionOwner(display, selection) == owner_window)
			XSetSelectionOwner(display, selection, None, CurrentTime);
	}
	else if (!strcmp(verb, "create"))
	{
		XSetWindowAttributes attributes = {0};
		unsigned long info[2] = {0, 1};
		const char *name = text(command, "name", "Tray fixture icon");
		const char *kind = text(command, "kind", "input-output");
		const char *visual_name = text(command, "visual", "default");
		cJSON *override = cJSON_GetObjectItemCaseSensitive(command,
			"override_redirect");
		Visual *visual = DefaultVisual(display, DefaultScreen(display));
		int depth = DefaultDepth(display, DefaultScreen(display));
		int window_class = InputOutput;
		unsigned long mask = CWOverrideRedirect | CWEventMask |
			CWBackPixel | CWBorderPixel;

		if (window_count == CLIENT_LIMIT || !name || !kind || !visual_name ||
		    (override && !cJSON_IsBool(override)) ||
		    !integer(command, "width", 24, &width) || width < 1 ||
		    width > 512 || !integer(command, "height", 24, &height) ||
		    height < 1 || height > 512)
			goto invalid;
		if (!strcmp(kind, "input-only"))
		{
			window_class = InputOnly;
			depth = 0;
			visual = CopyFromParent;
			mask = CWOverrideRedirect | CWEventMask;
		}
		else if (strcmp(kind, "input-output"))
			goto invalid;
		if (!strcmp(visual_name, "other"))
		{
			XVisualInfo candidate;

			if (window_class != InputOutput)
				goto invalid;
			if (!XMatchVisualInfo(display, DefaultScreen(display),
				32, TrueColor, &candidate) || candidate.visual == visual)
			{
				cJSON_AddStringToObject(result, "unavailable",
					"No distinct 32-bit TrueColor visual");
				goto finish;
			}
			depth = candidate.depth;
			visual = candidate.visual;
			attributes.colormap = XCreateColormap(display, root,
				visual, AllocNone);
			mask |= CWColormap;
		}
		else if (strcmp(visual_name, "default"))
			goto invalid;
		attributes.override_redirect = !override || cJSON_IsTrue(override);
		attributes.background_pixel = 0x72a6d2;
		attributes.event_mask = StructureNotifyMask | PropertyChangeMask |
			KeyPressMask | KeyReleaseMask | ButtonPressMask |
			ButtonReleaseMask;
		if (window_class == InputOutput)
			attributes.event_mask |= ExposureMask;
		Window window = XCreateWindow(display, root, 40, 40, width,
			height, 0, depth, window_class, visual, mask, &attributes);
		windows[window_count++] = window;
		XStoreName(display, window, name);
		XChangeProperty(display, window, info_atom, info_atom, 32,
			PropModeReplace, (unsigned char *)info, 2);
		cJSON_AddNumberToObject(result, "id", window);
	}
	else if (!strcmp(verb, "events"))
		;
	else if (!strcmp(verb, "state"))
	{
		if (!owned(id))
			goto invalid;
		cJSON_Delete(result);
		result = state(id);
	}
	else if (!strcmp(verb, "dock"))
	{
		Window receiver = XGetSelectionOwner(display, selection);

		if (!receiver || !integer(command, "format", 32, &format) ||
		    !integer(command, "opcode", 0, &value) ||
		    (format != 8 && format != 16 && format != 32))
			goto invalid;
		send_message(receiver, receiver, opcode_atom, format, value, id,
			0, 0);
	}
	else if (!strcmp(verb, "xembed"))
	{
		Window actual_root, receiver, *children = NULL;
		unsigned int count;

		if (!owned(id) || !integer(command, "opcode", 3, &value))
			goto invalid;
		if (!XQueryTree(display, id, &actual_root, &receiver,
			&children, &count))
			goto invalid;
		if (children)
			XFree(children);
		send_message(receiver, receiver, embed_atom, 32, value, 0, 0, 0);
	}
	else if (!strcmp(verb, "info"))
	{
		const char *mode = text(command, "mode", "valid");
		unsigned long words[3];

		if (!owned(id) || !mode ||
		    !integer(command, "flags", 1, &flags) ||
		    !integer(command, "version", 0, &version))
			goto invalid;
		words[0] = version;
		words[1] = flags;
		words[2] = 0;
		if (!strcmp(mode, "missing"))
			XDeleteProperty(display, id, info_atom);
		else if (!strcmp(mode, "valid") || !strcmp(mode, "oversize") ||
		    !strcmp(mode, "wrong-type"))
			XChangeProperty(display, id, info_atom,
				!strcmp(mode, "wrong-type") ? XA_CARDINAL : info_atom,
				32, PropModeReplace, (unsigned char *)words,
				!strcmp(mode, "oversize") ? 3 : 2);
		else
			goto invalid;
	}
	else if (!strcmp(verb, "resize"))
	{
		if (!owned(id) || !integer(command, "width", 24, &width) ||
		    !width || width > 512 ||
		    !integer(command, "height", 24, &height) ||
		    !height || height > 512)
			goto invalid;
		XResizeWindow(display, id, width, height);
	}
	else if (!strcmp(verb, "resize-repaint"))
	{
		uint32_t count;
		if (!owned(id) || !integer(command, "width", 24, &width) ||
		    !width || width > 512 ||
		    !integer(command, "height", 24, &height) ||
		    !height || height > 512 ||
		    !integer(command, "count", 128, &count) || !count || count > 128 ||
		    !resize_repaint(id, width, height, count, result))
			goto invalid;
	}
	else if (!strcmp(verb, "map") || !strcmp(verb, "unmap"))
	{
		if (!owned(id))
			goto invalid;
		if (!strcmp(verb, "map"))
			XMapWindow(display, id);
		else
			XUnmapWindow(display, id);
	}
	else if (!strcmp(verb, "reparent"))
	{
		if (!owned(id) || !integer(command, "parent", root, &parent))
			goto invalid;
		XReparentWindow(display, id, parent, 20, 20);
	}
	else if (!strcmp(verb, "destroy"))
	{
		if (!owned(id))
			goto invalid;
		XDestroyWindow(display, id);
	}
	else
		goto invalid;
finish:
	XSync(display, False);
	cJSON_AddItemToObject(result, "events", events());
	cJSON_AddNumberToObject(result, "unexpected_x_errors", unexpected_errors);
	return result;
invalid:
	cJSON_AddStringToObject(result, "error", "invalid bounded fixture command");
	return result;
}

int main(int argc, char **argv)
{
	char line[LINE_LIMIT + 2], selection_name[80];
	unsigned int commands = 0;
	int status = 0;

	if (argc != 1)
		return 2;
	(void)argv;
	display = XOpenDisplay(NULL);
	if (!display)
		return 2;
	XSetErrorHandler(on_error);
	root = DefaultRootWindow(display);
	snprintf(selection_name, sizeof(selection_name), "_NET_SYSTEM_TRAY_S%d",
		DefaultScreen(display));
	selection = XInternAtom(display, selection_name, False);
	info_atom = XInternAtom(display, "_XEMBED_INFO", False);
	embed_atom = XInternAtom(display, "_XEMBED", False);
	opcode_atom = XInternAtom(display, "_NET_SYSTEM_TRAY_OPCODE", False);
	manager_atom = XInternAtom(display, "MANAGER", False);
	host_atom = XInternAtom(display, "_FVWM_TRAY_HOST", False);
	owner_window = XCreateSimpleWindow(display, root, 0, 0, 1, 1, 0, 0, 0);
	XSelectInput(display, root, StructureNotifyMask);
	XSync(display, False);
	while (commands++ < COMMAND_LIMIT && fgets(line, sizeof(line), stdin))
	{
		cJSON *command, *result;
		char *encoded;

		if (!strchr(line, '\n'))
		{
			status = 2;
			break;
		}
		command = cJSON_ParseWithLength(line, strlen(line));
		result = cJSON_IsObject(command) ? execute(command) : NULL;
		if (!result)
		{
			cJSON_Delete(command);
			status = 2;
			break;
		}
		encoded = cJSON_PrintUnformatted(result);
		if (!encoded)
		{
			cJSON_Delete(result);
			cJSON_Delete(command);
			status = 2;
			break;
		}
		puts(encoded);
		fflush(stdout);
		free(encoded);
		cJSON_Delete(result);
		cJSON_Delete(command);
	}
	XCloseDisplay(display);
	return status || unexpected_errors ? 2 : 0;
}
