/* This program is free software; you can redistribute it and/or modify it
 * under the GNU General Public License, version 2 or any later version. */
#include "config.h"
#include "tray_container.h"
#include "keyboard.h"
#include "draw.h"
#include "libs/FProperty.h"
#include "libs/Module.h"
#include "libs/XError.h"
#include "libs/log.h"
#include "libs/tray_container_protocol.h"
#include <X11/Xatom.h>
#ifdef HAVE_XFIXES
#include <X11/extensions/Xfixes.h>
#endif
#include <limits.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

static struct
{
	button_info *button;
	Window slot, offer, host, owner, seen, external_focus;
	Atom selection, host_property, capability, slot_property,
	    offer_property;
	Atom attach, acknowledge, detach, focus, return_focus;
	Atom focus_request, focus_ack;
	int event_base, error_base, x, y, width, height;
	int focus_direction;
	unsigned int leave_requests;
	double deadline;
	Bool initialized, available, protected, attached, enabled;
	Bool pending, focus_pending, focus_reply;
} buttons_tray;

static struct
{
	Window first, second;
	unsigned long serial;
	Bool failed;
} buttons_tray_trap;

static double now(void)
{
	struct timespec stamp;
	if (clock_gettime(CLOCK_MONOTONIC, &stamp) < 0)
		return 0;
	return stamp.tv_sec + stamp.tv_nsec / 1000000000.0;
}

static int foreign_error(Display *display, XErrorEvent *event)
{
	if (display == Dpy && event->serial >= buttons_tray_trap.serial &&
	    event->error_code == BadWindow &&
	    (event->resourceid == buttons_tray_trap.first ||
	     event->resourceid == buttons_tray_trap.second))
	{
		buttons_tray_trap.failed = True;
		return 0;
	}
	buttons_tray_trap.failed = True;
	return ferror_call_next_error_handler(display, event);
}

static void begin(Window first, Window second)
{
	XSync(Dpy, False);
	buttons_tray_trap.first = first;
	buttons_tray_trap.second = second;
	buttons_tray_trap.serial = NextRequest(Dpy);
	buttons_tray_trap.failed = False;
	ferror_set_temp_error_handler(foreign_error);
}

static Bool end(void)
{
	XSync(Dpy, False);
	ferror_reset_temp_error_handler();
	return !buttons_tray_trap.failed;
}

static Bool parent_of(Window window, Window *parent)
{
	Window root = None, *children = NULL;
	unsigned int count = 0;
	*parent = None;
	Status got = XQueryTree(Dpy, window, &root, parent, &children, &count);
	if (children)
		XFree(children);
	return got && root == Root;
}

Bool buttons_tray_normalize_focus(XEvent *event)
{
	Window target = buttons_keyboard_focus_window();
	if (target == MyWindow ||
	    (event->type != FocusIn && event->type != FocusOut) ||
	    event->xfocus.window != MyWindow)
		return False;
	/* Parent notifications describe scope entry, not keyboard ownership.
	 * Only a real, still-current managed entry may select our leaf. */
	if (event->type == FocusOut || event->xany.send_event)
		return True;
	Window parent = None, focused = None;
	XWindowAttributes attributes;
	int revert;
	XGrabServer(Dpy);
	begin(target, MyWindow);
	XGetInputFocus(Dpy, &focused, &revert);
	Bool valid = focused == MyWindow && parent_of(target, &parent) &&
	    parent == MyWindow &&
	    XGetWindowAttributes(Dpy, target, &attributes) &&
	    attributes.root == Root && attributes.class == InputOnly &&
	    attributes.map_state == IsViewable;
	if (valid && !buttons_tray_trap.failed)
		XSetInputFocus(Dpy, target, RevertToPointerRoot, CurrentTime);
	(void)end();
	XUngrabServer(Dpy);
	XSync(Dpy, False);
	return True;
}

static void words(unsigned long output[5])
{
	output[0] = buttons_tray.slot;
	output[1] = buttons_tray.offer;
	output[2] = buttons_tray.host;
	output[3] = buttons_tray.owner;
	output[4] = FVWM_TRAY_CONTAINER_VERSION;
}

static void container_send(Atom atom, int value)
{
	if (!buttons_tray.owner || !buttons_tray.offer)
		return;
	XEvent event = {0};
	event.xclient.type = ClientMessage;
	event.xclient.window = buttons_tray.owner;
	event.xclient.message_type = atom;
	event.xclient.format = 32;
	event.xclient.data.l[0] = buttons_tray.slot;
	event.xclient.data.l[1] = buttons_tray.offer;
	event.xclient.data.l[2] = buttons_tray.host;
	event.xclient.data.l[3] = buttons_tray.owner;
	event.xclient.data.l[4] = value;
	begin(buttons_tray.owner, buttons_tray.host);
	XSendEvent(Dpy, buttons_tray.owner, False, NoEventMask, &event);
	(void)end();
}

static void clear_offer(void)
{
	if (buttons_tray.offer)
		XDestroyWindow(Dpy, buttons_tray.offer);
	buttons_tray.offer = buttons_tray.owner = buttons_tray.host = None;
	buttons_tray.pending = buttons_tray.attached = False;
	buttons_tray.focus_pending = False;
	buttons_tray.deadline = 0;
	if (buttons_tray.slot)
	{
		XUnmapWindow(Dpy, buttons_tray.slot);
		XDeleteProperty(Dpy, buttons_tray.slot,
		    buttons_tray.slot_property);
	}
	if (buttons_tray.button)
		RedrawButton(buttons_tray.button, DRAW_FORCE, NULL);
}

static Bool release(void)
{
	if (!buttons_tray.host)
	{
		clear_offer();
		return True;
	}
	Window parent = None;
	Bool released = False;
	/* Only one saved foreign host is touched, while its actual parent and
	 * release are protected from concurrent destruction/reuse. Never
	 * destroy
	 * a slot ancestor if release fails; connection-close recovery needs
	 * it. */
	XGrabServer(Dpy);
	begin(buttons_tray.host, buttons_tray.owner);
	Bool exists = parent_of(buttons_tray.host, &parent);
	if (exists && parent == buttons_tray.slot)
	{
		XUnmapWindow(Dpy, buttons_tray.host);
		XReparentWindow(Dpy, buttons_tray.host, Root, 0, 0);
		exists = parent_of(buttons_tray.host, &parent);
	}
	Bool checked = end();
	if (!exists && buttons_tray_trap.failed)
		released = True;
	else if (checked && parent == Root)
	{
#ifdef HAVE_XFIXES
		if (buttons_tray.protected)
		{
			begin(buttons_tray.host, buttons_tray.owner);
			XFixesChangeSaveSet(Dpy, buttons_tray.host,
			    SetModeDelete,
			    SaveSetRoot, SaveSetUnmap);
			released = end();
		}
		else
#endif
			released = True;
	}
	XUngrabServer(Dpy);
	XSync(Dpy, False);
	if (released)
	{
		buttons_tray.protected = False;
		clear_offer();
	}
	return released;
}

Bool buttons_tray_requested(void)
{
	return buttons_tray.button != NULL;
}

Bool buttons_tray_claim(button_info *button)
{
	if (!button || button->flags.b_Container || button->flags.b_Swallow ||
	    button->flags.b_Panel ||
	    (buttons_tray.button && buttons_tray.button != button))
	{
		fvwm_debug(MyName, "only one native Tray slot is supported");
		return False;
	}
	buttons_tray.button = button;
	return True;
}

void buttons_tray_forget(button_info *button)
{
	if (!buttons_tray.initialized && buttons_tray.button == button)
	{
		buttons_tray.button = NULL;
		button->flags.b_Tray = 0;
	}
}

static void initialize(void)
{
	buttons_tray.initialized = True;
	char selection[64];
	snprintf(selection, sizeof(selection), "_NET_SYSTEM_TRAY_S%d",
	    DefaultScreen(Dpy));
	buttons_tray.selection = XInternAtom(Dpy, selection, False);
	buttons_tray.host_property = XInternAtom(Dpy, "_FVWM_TRAY_HOST", False);
	const char *names[] = {FVWM_TRAY_CONTAINER_CAP,
	    FVWM_TRAY_CONTAINER_SLOT, FVWM_TRAY_CONTAINER_OFFER,
	    FVWM_TRAY_CONTAINER_ATTACH, FVWM_TRAY_CONTAINER_ACK,
	    FVWM_TRAY_CONTAINER_DETACH, FVWM_TRAY_CONTAINER_FOCUS,
	    FVWM_TRAY_CONTAINER_RETURN, FVWM_TRAY_CONTAINER_FOCUS_REQUEST,
	    FVWM_TRAY_CONTAINER_FOCUS_ACK};
	Atom atoms[10];
	XInternAtoms(Dpy, (char **)names, 10, False, atoms);
	buttons_tray.capability = atoms[0];
	buttons_tray.slot_property = atoms[1];
	buttons_tray.offer_property = atoms[2];
	buttons_tray.attach = atoms[3];
	buttons_tray.acknowledge = atoms[4];
	buttons_tray.detach = atoms[5];
	buttons_tray.focus = atoms[6];
	buttons_tray.return_focus = atoms[7];
	buttons_tray.focus_request = atoms[8];
	buttons_tray.focus_ack = atoms[9];
#ifdef HAVE_XFIXES
	int major = 1, minor = 0;
	buttons_tray.available =
	    XFixesQueryExtension(Dpy, &buttons_tray.event_base,
	    &buttons_tray.error_base) &&
	    XFixesQueryVersion(Dpy, &major, &minor) && major >= 1;
	if (buttons_tray.available)
		XFixesSelectSelectionInput(Dpy, MyWindow,
		    buttons_tray.selection, XFixesSetSelectionOwnerNotifyMask |
		    XFixesSelectionWindowDestroyNotifyMask |
		    XFixesSelectionClientCloseNotifyMask);
#endif
	if (!buttons_tray.available)
	{
		fvwm_debug(MyName, "native Tray slot needs XFixes >=1");
		return;
	}
	buttons_tray.slot = XCreateSimpleWindow(Dpy, MyWindow,
	    0, 0, 1, 1, 0, 0, 0);
	XSelectInput(Dpy, buttons_tray.slot, StructureNotifyMask);
	XStoreName(Dpy, buttons_tray.slot, "FVWM native tray slot");
}

static Bool property(Window window, Atom atom, Atom type,
	uint32_t *output, unsigned int count)
{
	unsigned char *data = NULL;
	size_t bytes = 0;
	Bool valid = FPropertyRead(Dpy, window, atom, type,
	    (FPropertyLimits){count * 4U, count * 4U, 32},
	    &data, &bytes) == FPROPERTY_OK && bytes == count * 4U;
	if (valid)
		memcpy(output, data, bytes);
	free(data);
	return valid;
}

static void discover(void)
{
	if (!buttons_tray.available || !buttons_tray.enabled ||
	    buttons_tray.pending || buttons_tray.attached ||
	    buttons_tray.protected)
		return;
	Window owner = XGetSelectionOwner(Dpy, buttons_tray.selection);
	if (!owner || owner == buttons_tray.seen)
		return;
	buttons_tray.seen = owner;
	uint32_t host = 0, capability[4];
	Window parent = None;
	XWindowAttributes attributes;
	begin(owner, owner);
	Bool valid = property(owner, buttons_tray.host_property, XA_WINDOW,
	    &host, 1) && property(owner, buttons_tray.capability, XA_CARDINAL,
	    capability, 4) && capability[0] == FVWM_TRAY_CONTAINER_VERSION &&
	    capability[1] == host && (capability[2] & 1U) &&
	    host && host != owner && host != Root && host != MyWindow;
	if (valid)
		XSelectInput(Dpy, owner,
		    PropertyChangeMask | StructureNotifyMask);
	valid = end() && valid;
	if (!valid || capability[3])
		return;
	XGrabServer(Dpy);
	begin(host, owner);
	valid = XGetSelectionOwner(Dpy, buttons_tray.selection) == owner &&
	    XGetWindowAttributes(Dpy, host, &attributes) &&
	    attributes.root == Root && attributes.class == InputOutput &&
	    parent_of(host, &parent) && parent == Root;
	valid = end() && valid;
	if (valid)
	{
		buttons_tray.host = host;
		buttons_tray.owner = owner;
		XSetWindowAttributes offer_attributes = {0};
		offer_attributes.override_redirect = True;
		offer_attributes.event_mask = StructureNotifyMask;
		buttons_tray.offer = XCreateWindow(Dpy, Root, -1, -1, 1, 1,
		    0, 0, InputOnly, CopyFromParent,
		    CWOverrideRedirect | CWEventMask, &offer_attributes);
		unsigned long values[5];
		words(values);
		XChangeProperty(Dpy, buttons_tray.offer,
		    buttons_tray.offer_property, XA_CARDINAL, 32,
		    PropModeReplace, (unsigned char *)values, 5);
		XChangeProperty(Dpy, buttons_tray.slot,
		    buttons_tray.slot_property, XA_CARDINAL, 32,
		    PropModeReplace, (unsigned char *)values, 5);
		begin(host, owner);
#ifdef HAVE_XFIXES
		XFixesChangeSaveSet(Dpy, host, SetModeInsert,
		    SaveSetRoot, SaveSetUnmap);
#endif
		valid = end();
		buttons_tray.protected = valid;
	}
	XUngrabServer(Dpy);
	XSync(Dpy, False);
	if (valid)
	{
		buttons_tray.pending = True;
		buttons_tray.deadline = now() + 2.0;
		container_send(buttons_tray.attach,
		    FVWM_TRAY_CONTAINER_VERSION);
	}
	else if (buttons_tray.host)
		(void)release();
}

void buttons_tray_layout(button_info *button)
{
	if (!button || button != buttons_tray.button || !button->flags.b_Tray)
		return;
	if (!buttons_tray.initialized)
		initialize();
	if (!buttons_tray.available)
		return;
	int x, y, width, height;
	GetInternalSize(button, &x, &y, &width, &height);
	Bool enabled = !button->flags.b_Disabled && width > 0 && height > 0;
	if (enabled != buttons_tray.enabled)
	{
		buttons_tray.enabled = enabled;
		buttons_tray.seen = None;
		if (!enabled)
			(void)release();
	}
	if (!enabled)
	{
		XUnmapWindow(Dpy, buttons_tray.slot);
		return;
	}
	if (x != buttons_tray.x || y != buttons_tray.y ||
	    width != buttons_tray.width || height != buttons_tray.height)
	{
		buttons_tray.x = x;
		buttons_tray.y = y;
		buttons_tray.width = width;
		buttons_tray.height = height;
		XMoveResizeWindow(Dpy, buttons_tray.slot, x, y, width, height);
	}
	if (buttons_tray.attached)
		XMapWindow(Dpy, buttons_tray.slot);
	else
		XUnmapWindow(Dpy, buttons_tray.slot);
	discover();
}

void buttons_tray_service(void)
{
	if (!buttons_tray.button || !buttons_tray.available)
		return;
	buttons_tray_layout(buttons_tray.button);
	if ((buttons_tray.pending || buttons_tray.focus_pending) &&
	    now() >= buttons_tray.deadline)
	{
		if (buttons_tray.pending)
			(void)release();
		buttons_tray.focus_pending = False;
		buttons_tray.deadline = 0;
	}
	discover();
}

int buttons_tray_timeout(void)
{
	if (!buttons_tray.deadline)
		return -1;
	double remaining = buttons_tray.deadline - now();
	if (remaining <= 0)
		return 0;
	return remaining < 2.0 ? (int)(remaining * 1000.0) + 1 : 2000;
}

static Bool activate(button_info *button, int direction, Bool reply)
{
	if (button != buttons_tray.button || !buttons_tray.attached ||
	    !buttons_tray.enabled || direction < -1 || direction > 1)
		return False;
	Window focused;
	int revert;
	XGetInputFocus(Dpy, &focused, &revert);
	if (focused == buttons_keyboard_focus_window())
	{
		buttons_tray.focus_pending = False;
		buttons_tray.deadline = 0;
		container_send(reply ? buttons_tray.focus_ack :
		    buttons_tray.focus, direction);
	}
	else
	{
		char command[96];
		snprintf(command, sizeof(command), "Silent WindowId %lu Focus",
		    MyWindow);
		SendText(fd, command, 0);
		buttons_tray.focus_pending = True;
		buttons_tray.focus_reply = reply;
		buttons_tray.focus_direction = direction;
		buttons_tray.deadline = now() + 2.0;
	}
	return True;
}

Bool buttons_tray_activate(button_info *button, int direction)
{
	return activate(button, direction, False);
}

static Bool identity(const XClientMessageEvent *event)
{
	uint32_t actual[5], expected[5] = {buttons_tray.slot,
	    buttons_tray.offer, buttons_tray.host, buttons_tray.owner, 0};
	for (unsigned int i = 0; i < 5; i++)
		actual[i] = (uint32_t)event->data.l[i];
	return event->format == 32 &&
	    tray_container_identity_equal(actual, expected);
}

static Bool return_inside_scope(void)
{
	uint32_t host = 0, capability[4] = {0}, slot[5] = {0}, offer[5] = {0};
	uint32_t expected[5] = {buttons_tray.slot, buttons_tray.offer,
	    buttons_tray.host, buttons_tray.owner, FVWM_TRAY_CONTAINER_VERSION};
	Window parent = None, focused = None;
	Window target = buttons_keyboard_focus_window();
	XWindowAttributes attributes;
	int revert;
	Bool inside = False;
	/* Core Focus preserves an already-focused application's child widget.
	 * RETURN authorizes only this internal move, never managed entry from
	 * another application. Keep validation and focus commit indivisible. */
	XGrabServer(Dpy);
	begin(buttons_tray.host, buttons_tray.owner);
	Bool valid = buttons_tray.attached && buttons_tray.enabled &&
	    XGetSelectionOwner(Dpy, buttons_tray.selection) ==
	        buttons_tray.owner &&
	    property(buttons_tray.owner, buttons_tray.host_property, XA_WINDOW,
	        &host, 1) && host == buttons_tray.host &&
	    property(buttons_tray.owner, buttons_tray.capability, XA_CARDINAL,
	        capability, 4) &&
	    capability[0] == FVWM_TRAY_CONTAINER_VERSION &&
	    capability[1] == buttons_tray.host && (capability[2] & 1U) &&
	    capability[3] == buttons_tray.slot &&
	    parent_of(buttons_tray.host, &parent) &&
	    parent == buttons_tray.slot &&
	    XGetWindowAttributes(Dpy, buttons_tray.host, &attributes) &&
	    attributes.map_state == IsViewable;
	buttons_tray_trap.second = buttons_tray.slot;
	valid = valid && parent_of(buttons_tray.slot, &parent) &&
	    parent == MyWindow &&
	    property(buttons_tray.slot, buttons_tray.slot_property, XA_CARDINAL,
	        slot, 5) && !memcmp(slot, expected, sizeof(slot));
	buttons_tray_trap.second = buttons_tray.offer;
	valid = valid && property(buttons_tray.offer,
	    buttons_tray.offer_property, XA_CARDINAL, offer, 5) &&
	    !memcmp(offer, expected, sizeof(offer)) &&
	    XGetWindowAttributes(Dpy, MyWindow, &attributes) &&
	    attributes.map_state == IsViewable;
	if (valid && target != MyWindow)
	{
		buttons_tray_trap.second = target;
		valid = parent_of(target, &parent) && parent == MyWindow &&
		    XGetWindowAttributes(Dpy, target, &attributes) &&
		    attributes.class == InputOnly &&
		    attributes.map_state == IsViewable;
	}
	XGetInputFocus(Dpy, &focused, &revert);
	for (unsigned int depth = 0; valid && depth < 16; depth++)
	{
		if (focused == MyWindow || focused == target)
		{
			inside = depth == 0;
			break;
		}
		if (focused == buttons_tray.host)
		{
			inside = True;
			break;
		}
		if (focused == None || focused == PointerRoot ||
		    focused == Root)
			break;
		/* Each synchronous query admits only its live foreign XID. */
		buttons_tray_trap.second = focused;
		valid = parent_of(focused, &parent);
		focused = parent;
	}
	if (valid && inside && !buttons_tray_trap.failed)
	{
		XSetInputFocus(Dpy, target, RevertToPointerRoot, CurrentTime);
		XGetInputFocus(Dpy, &focused, &revert);
		valid = focused == target;
	}
	valid = end() && valid && inside;
	XUngrabServer(Dpy);
	XSync(Dpy, False);
	return valid;
}

static void return_to_button(int direction)
{
	if (!return_inside_scope())
		return;
	buttons_tray.focus_pending = False;
	buttons_tray.deadline = 0;
	buttons_keyboard_return_tray(buttons_tray.button, direction);
}

Bool buttons_tray_event(XEvent *event)
{
	if (!buttons_tray.available)
		return False;
#ifdef HAVE_XFIXES
	if (event->type == buttons_tray.event_base + XFixesSelectionNotify)
	{
		XFixesSelectionNotifyEvent *selection =
		    (XFixesSelectionNotifyEvent *)event;
		if (selection->selection == buttons_tray.selection)
		{
			(void)release();
			buttons_tray.seen = None;
			discover();
			return True;
		}
	}
#endif
	if (event->type == PropertyNotify &&
	    event->xproperty.window == buttons_tray.seen &&
	    event->xproperty.atom == buttons_tray.capability)
	{
		if (!buttons_tray.attached && !buttons_tray.pending)
		{
			buttons_tray.seen = None;
			discover();
		}
		return True;
	}
	if (event->type == ClientMessage &&
	    event->xclient.window == buttons_tray.offer &&
	    identity(&event->xclient))
	{
		if (event->xclient.message_type == buttons_tray.acknowledge)
		{
			if (event->xclient.data.l[4] == 1 &&
			    buttons_tray.pending)
			{
				Window parent = None;
				begin(buttons_tray.host, buttons_tray.owner);
				Bool valid = parent_of(buttons_tray.host,
				    &parent) &&
				    parent == buttons_tray.slot;
				valid = end() && valid;
				if (valid)
				{
					buttons_tray.pending = False;
					buttons_tray.attached = True;
					buttons_tray.deadline = 0;
					XMapWindow(Dpy, buttons_tray.slot);
				}
			}
			else if (!event->xclient.data.l[4])
				(void)release();
			return True;
		}
		if (event->xclient.message_type == buttons_tray.return_focus &&
		    buttons_tray.attached && event->xclient.data.l[4] >= -1 &&
		    event->xclient.data.l[4] <= 1)
		{
			return_to_button(event->xclient.data.l[4]);
			return True;
		}
		if (event->xclient.message_type == buttons_tray.focus_request &&
		    buttons_tray.attached && event->xclient.data.l[4] >= -1 &&
		    event->xclient.data.l[4] <= 1)
		{
			(void)activate(buttons_tray.button,
			    event->xclient.data.l[4], True);
			return True;
		}
	}
	if (event->type == FocusIn &&
	    event->xfocus.window == buttons_keyboard_focus_window() &&
	    !event->xany.send_event)
	{
		Window focused = None;
		int revert;
		XGetInputFocus(Dpy, &focused, &revert);
		if (focused != buttons_keyboard_focus_window())
			return False;
		if (buttons_tray.focus_pending)
		{
			buttons_tray.focus_pending = False;
			buttons_tray.deadline = 0;
			container_send(buttons_tray.focus_reply ?
			    buttons_tray.focus_ack : buttons_tray.focus,
			    buttons_tray.focus_direction);
		}
	}
	if (event->type == UnmapNotify && event->xunmap.window == MyWindow)
		(void)release();
	if (event->type == DestroyNotify &&
	    (event->xdestroywindow.window == buttons_tray.host ||
	     event->xdestroywindow.window == buttons_tray.owner))
	{
		(void)release();
		buttons_tray.seen = None;
		discover();
		return True;
	}
	return False;
}

void buttons_tray_cleanup(void)
{
	if (!buttons_tray.initialized)
		return;
	Bool released = release();
	if (released && buttons_tray.slot)
		XDestroyWindow(Dpy, buttons_tray.slot);
	/* If release failed, keep the saved host and ancestor intact until
	 * XCloseDisplay; the server must apply the save set before teardown. */
	buttons_tray.slot = None;
}

void buttons_tray_focus_change(Window window, Window frame)
{
	/* Unmanaged Tray proxies also receive focus notices. Only a managed
	 * frame identifies a target the core can restore on scope exit. */
	if (buttons_tray.button && window && frame && window != MyWindow)
		buttons_tray.external_focus = window;
}

void buttons_tray_destroyed(Window window)
{
	if (buttons_tray.external_focus == window)
		buttons_tray.external_focus = None;
}

void buttons_tray_leave(void)
{
	if (buttons_tray.button && buttons_tray.external_focus)
	{
		char command[96];
		snprintf(command, sizeof(command), "Silent WindowId %lu Focus",
		    buttons_tray.external_focus);
		if (buttons_tray.leave_requests < UINT_MAX)
			buttons_tray.leave_requests++;
		SendText(fd, command, 0);
	}
}

Window buttons_tray_external_focus(void)
{
	return buttons_tray.external_focus;
}

unsigned int buttons_tray_leave_requests(void)
{
	return buttons_tray.leave_requests;
}
