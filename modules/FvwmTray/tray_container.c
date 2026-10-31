/* This program is free software; you can redistribute it and/or modify it
 * under the GNU General Public License, version 2 or any later version. */
#include "config.h"
#include "tray.h"
#include "libs/FProperty.h"
#include "libs/XError.h"
#include <X11/Xatom.h>
#ifdef HAVE_XFIXES
#include <X11/extensions/Xfixes.h>
#endif
#include <stdlib.h>
#include <string.h>

static struct
{
	Display *display;
	Window first, second;
	unsigned long serial;
	bool failed;
} container_trap;

static int container_error(Display *display, XErrorEvent *event)
{
	if (display == container_trap.display &&
	    event->serial >= container_trap.serial &&
	    event->error_code == BadWindow &&
	    (event->resourceid == container_trap.first ||
	     event->resourceid == container_trap.second))
	{
		container_trap.failed = true;
		return 0;
	}
	container_trap.failed = true;
	return ferror_call_next_error_handler(display, event);
}

static void begin(tray_ui *ui, Window first, Window second)
{
	XSync(ui->display, False);
	container_trap.display = ui->display;
	container_trap.first = first;
	container_trap.second = second;
	container_trap.serial = NextRequest(ui->display);
	container_trap.failed = false;
	ferror_set_temp_error_handler(container_error);
}

static bool end(tray_ui *ui)
{
	XSync(ui->display, False);
	ferror_reset_temp_error_handler();
	return !container_trap.failed;
}

static bool property(tray_ui *ui, Window window, Atom atom,
	const uint32_t expected[5])
{
	unsigned char *data = NULL;
	size_t bytes = 0;
	bool valid = FPropertyRead(ui->display, window, atom, XA_CARDINAL,
	    (FPropertyLimits){20, 20, 32}, &data, &bytes) == FPROPERTY_OK &&
	    bytes == 20 && !memcmp(data, expected, 20);
	free(data);
	return valid;
}

static bool parent_of(tray_ui *ui, Window window, Window *parent)
{
	Window root = None, *children = NULL;
	unsigned int count = 0;
	*parent = None;
	Status got = XQueryTree(ui->display, window, &root, parent,
	    &children, &count);
	if (children)
		XFree(children);
	return got && root == ui->root;
}

static void container_send(tray_ui *ui, Atom atom, int value)
{
	tray_container *state = &ui->container;
	if (!state->offer)
		return;
	XEvent event = {0};
	event.xclient.type = ClientMessage;
	event.xclient.window = state->offer;
	event.xclient.message_type = atom;
	event.xclient.format = 32;
	event.xclient.data.l[0] = state->slot;
	event.xclient.data.l[1] = state->offer;
	event.xclient.data.l[2] = ui->host;
	event.xclient.data.l[3] = ui->owner;
	event.xclient.data.l[4] = value;
	begin(ui, state->offer, state->slot);
	XSendEvent(ui->display, state->offer, False, NoEventMask, &event);
	(void)end(ui);
}

void tray_container_publish(tray_ui *ui)
{
	unsigned long words[4] = {FVWM_TRAY_CONTAINER_VERSION, ui->host,
	    ui->container.capable ? 1U : 0U, ui->container.slot};
	XChangeProperty(ui->display, ui->owner, ui->container.capability,
	    XA_CARDINAL, 32, PropModeReplace, (unsigned char *)words, 4);
}

void tray_container_init(tray_ui *ui)
{
	tray_container *state = &ui->container;
	const char *names[] = {FVWM_TRAY_CONTAINER_CAP,
	    FVWM_TRAY_CONTAINER_SLOT, FVWM_TRAY_CONTAINER_OFFER,
	    FVWM_TRAY_CONTAINER_ATTACH, FVWM_TRAY_CONTAINER_ACK,
	    FVWM_TRAY_CONTAINER_DETACH, FVWM_TRAY_CONTAINER_FOCUS,
	    FVWM_TRAY_CONTAINER_RETURN, FVWM_TRAY_CONTAINER_FOCUS_REQUEST,
	    FVWM_TRAY_CONTAINER_FOCUS_ACK};
	Atom atoms[10];
	XInternAtoms(ui->display, (char **)names, 10, False, atoms);
	state->capability = atoms[0];
	state->slot_property = atoms[1];
	state->offer_property = atoms[2];
	state->attach = atoms[3];
	state->acknowledge = atoms[4];
	state->detach = atoms[5];
	state->focus = atoms[6];
	state->return_focus = atoms[7];
	state->focus_request = atoms[8];
	state->focus_ack = atoms[9];
#ifdef HAVE_XFIXES
	int major = 1, minor = 0;
	state->capable = XFixesQueryVersion(ui->display, &major, &minor) &&
	    major >= 1;
#endif
	tray_container_publish(ui);
}

bool tray_container_geometry(tray_ui *ui)
{
	tray_container *state = &ui->container;
	if (!state->attached)
		return false;
	XWindowAttributes attributes;
	Window child = None, parent = None;
	int x = 0, y = 0;
	begin(ui, state->slot, state->offer);
	bool valid = XGetWindowAttributes(ui->display, state->slot,
	    &attributes) && attributes.root == ui->root &&
	    attributes.class == InputOutput && attributes.width > 0 &&
	    attributes.height > 0 && attributes.width <= 32767 &&
	    attributes.height <= 32767 &&
	    XTranslateCoordinates(ui->display, state->slot, ui->root,
	    0, 0, &x, &y, &child) && x >= -32768 && x <= 32767 &&
	    y >= -32768 && y <= 32767 &&
	    parent_of(ui, ui->host, &parent) && parent == state->slot;
	valid = end(ui) && valid;
	if (!valid)
		return false;
	state->width = attributes.width;
	state->height = attributes.height;
	state->root_x = x;
	state->root_y = y;
	return true;
}

void tray_container_return(tray_ui *ui, int direction)
{
	if (!ui->container.attached)
	{
		tray_x11_unfocus(ui, true);
		return;
	}
	ui->container.focus_pending = false;
	tray_x11_unfocus(ui, false);
	container_send(ui, ui->container.return_focus, direction < 0 ? -1 :
	    direction > 0 ? 1 : 0);
}

bool tray_container_focus_request(tray_ui *ui, uint32_t id,
	int direction, Time timestamp)
{
	tray_container *state = &ui->container;
	(void)timestamp;
	if (!state->attached)
		return false;
	Window focused = None;
	int revert;
	XGetInputFocus(ui->display, &focused, &revert);
	if (focused == ui->proxy)
		return false;
	int index = tray_model_index(&ui->model, id);
	if (id && (index < 0 || !ui->layout.cells[index].visible))
		return true;
	state->focus_id = id;
	state->focus_generation = id ? ui->model.clients[index].generation : 0;
	state->focus_direction = direction;
	state->focus_pending = true;
	state->focus_deadline = tray_sni_now() + 2.0;
	container_send(ui, state->focus_request, direction);
	return true;
}

int tray_container_timeout(const tray_ui *ui, double stamp)
{
	return tray_container_pending_timeout(ui->container.focus_pending,
	    ui->container.focus_deadline, stamp);
}

void tray_container_service(tray_ui *ui, double stamp)
{
	if (tray_container_timeout(ui, stamp) != 0)
		return;
	ui->container.focus_pending = false;
	ui->container.focus_id = 0;
	ui->container.focus_generation = 0;
	ui->container.focus_deadline = 0;
	tray_sni_cancel_pending_focus(ui);
}

void tray_container_detach(tray_ui *ui)
{
	tray_container *state = &ui->container;
	if (!state->attached)
		return;
	state->focus_pending = false;
	tray_x11_unfocus(ui, false);
	/* The host is ours. It is never destroyed while it can contain an
	 * unreleased foreign icon; root-target save sets protect those
	 * icons. */
	XUnmapWindow(ui->display, ui->host);
	XReparentWindow(ui->display, ui->host, ui->root, 0, 0);
	XSync(ui->display, False);
	container_send(ui, state->acknowledge, 0);
	state->attached = false;
	state->slot = state->offer = state->focus_scope = None;
	ui->visible = false;
	ui->anchored = false;
	ui->dirty = true;
	tray_container_publish(ui);
}

static bool identity(tray_ui *ui, const uint32_t words[5])
{
	return tray_container_identity_valid(words) && words[2] == ui->host &&
	    words[3] == ui->owner && words[0] != ui->root &&
	    words[1] != ui->root;
}

static bool attach(tray_ui *ui, const uint32_t words[5])
{
	tray_container *state = &ui->container;
	if (!state->capable || state->attached ||
	    words[4] != FVWM_TRAY_CONTAINER_VERSION || !identity(ui, words))
		return false;
	Window slot = words[0], offer = words[1], parent = None, scope = None;
	XWindowAttributes attributes;
	XGrabServer(ui->display);
	begin(ui, slot, offer);
	bool valid = XGetSelectionOwner(ui->display, ui->selection) ==
	    ui->owner && property(ui, offer, state->offer_property, words) &&
	    property(ui, slot, state->slot_property, words) &&
	    XGetWindowAttributes(ui->display, slot, &attributes) &&
	    attributes.root == ui->root && attributes.class == InputOutput &&
	    attributes.width > 0 && attributes.height > 0 &&
	    attributes.width <= 32767 && attributes.height <= 32767 &&
	    parent_of(ui, slot, &scope) && scope != ui->root &&
	    scope != None &&
	    parent_of(ui, ui->host, &parent) && parent == ui->root;
	Window ancestor = slot;
	for (unsigned int depth = 0; valid && depth < 16; depth++)
	{
		if (!parent_of(ui, ancestor, &parent) || parent == ui->host ||
		    parent == ui->owner || parent == ui->proxy)
			valid = false;
		else if (parent == ui->root)
			break;
		else if (depth == 15 || parent == None)
			valid = false;
		ancestor = parent;
	}
	valid = end(ui) && valid;
	if (valid)
	{
		state->slot = slot;
		state->offer = offer;
		state->focus_scope = scope;
		state->width = attributes.width;
		state->height = attributes.height;
		state->attached = true;
		ui->anchored = false;
		ui->visible = true;
		begin(ui, slot, offer);
		XSelectInput(ui->display, slot, StructureNotifyMask);
		XSelectInput(ui->display, offer, StructureNotifyMask);
		XUnmapWindow(ui->display, ui->host);
		XReparentWindow(ui->display, ui->host, slot, 0, 0);
		valid = end(ui);
		if (!valid)
		{
			XReparentWindow(ui->display, ui->host, ui->root, 0, 0);
			state->attached = false;
			state->slot = state->offer = state->focus_scope = None;
			ui->visible = false;
		}
	}
	XUngrabServer(ui->display);
	XSync(ui->display, False);
	if (valid)
	{
		ui->dirty = true;
		tray_container_publish(ui);
		container_send(ui, state->acknowledge, 1);
	}
	return valid;
}

static bool current_focus_scope(tray_ui *ui)
{
	tray_container *state = &ui->container;
	uint32_t expected[5] = {state->slot, state->offer, ui->host,
	    ui->owner, FVWM_TRAY_CONTAINER_VERSION};
	Window parent = None, focused = None;
	int revert;
	XWindowAttributes attributes;
	begin(ui, state->slot, state->offer);
	bool valid = state->focus_scope &&
	    XGetSelectionOwner(ui->display, ui->selection) == ui->owner &&
	    property(ui, state->slot, state->slot_property, expected) &&
	    property(ui, state->offer, state->offer_property, expected) &&
	    parent_of(ui, state->slot, &parent) &&
	    parent == state->focus_scope &&
	    parent_of(ui, ui->host, &parent) && parent == state->slot &&
	    XGetWindowAttributes(ui->display, ui->host, &attributes) &&
	    attributes.map_state == IsViewable;
	XGetInputFocus(ui->display, &focused, &revert);
	bool inside = false;
	for (unsigned int depth = 0; valid && depth < 16; depth++)
	{
		if (focused == state->focus_scope)
		{
			inside = true;
			break;
		}
		if (focused == None || focused == PointerRoot ||
		    focused == ui->root)
			break;
		/* Each synchronous tree query owns its expected foreign XID. */
		container_trap.second = focused;
		valid = parent_of(ui, focused, &parent);
		focused = parent;
	}
	return end(ui) && valid && inside;
}

static void grant_focus(tray_ui *ui, bool reply, int direction)
{
	tray_container *state = &ui->container;
	tray_container_service(ui, tray_sni_now());
	if (direction < -1 || direction > 1 ||
	    (reply && !state->focus_pending))
		return;
	uint32_t id = reply ? state->focus_id : 0;
	uint64_t generation = state->focus_generation;
	if (reply)
		direction = state->focus_direction;
	state->focus_pending = false;
	if (ui->dirty)
		tray_render(ui);
	int index = tray_model_index(&ui->model, id);
	if (!state->attached || (id && (index < 0 ||
	    ui->model.clients[index].generation != generation)))
	{
		tray_sni_cancel_pending_focus(ui);
		return;
	}
	id = tray_x11_focus_target(ui, id, direction);
	if (!id)
	{
		tray_sni_cancel_pending_focus(ui);
		return;
	}
	/* Only fixed X requests run under this grab. Notification helpers own
	 * their own grabs and must not release this focus transaction early. */
	bool committed = false;
	XGrabServer(ui->display);
	if (current_focus_scope(ui))
	{
		Window focused = None;
		int revert;
		XSetInputFocus(ui->display, ui->proxy,
		    RevertToPointerRoot, CurrentTime);
		XGetInputFocus(ui->display, &focused, &revert);
		committed = focused == ui->proxy;
	}
	XUngrabServer(ui->display);
	XSync(ui->display, False);
	if (committed)
		tray_x11_focus_committed(ui, id, direction);
	else
		tray_sni_cancel_pending_focus(ui);
}

bool tray_container_event(tray_ui *ui, XEvent *event)
{
	tray_container *state = &ui->container;
	if (event->type == ClientMessage &&
	    event->xclient.window == ui->owner &&
	    event->xclient.format == 32)
	{
		Atom type = event->xclient.message_type;
		if (type != state->attach && type != state->detach &&
		    type != state->focus && type != state->focus_ack)
			return false;
		uint32_t words[5];
		for (unsigned int i = 0; i < 5; i++)
			words[i] = (uint32_t)event->xclient.data.l[i];
		if (type == state->attach)
			(void)attach(ui, words);
		else if (state->attached && identity(ui, words) &&
		    words[0] == state->slot && words[1] == state->offer)
		{
			if (type == state->detach)
				tray_container_detach(ui);
			else
				grant_focus(ui, type == state->focus_ack,
				    (int32_t)words[4]);
		}
		return true;
	}
	if (!state->attached || event->xany.send_event)
		return false;
	if (event->type == ReparentNotify &&
	    event->xreparent.window == ui->host)
	{
		if (event->xreparent.parent == ui->root)
		{
			state->attached = false;
			state->focus_pending = false;
			state->slot = state->offer = state->focus_scope = None;
			ui->visible = false;
			ui->dirty = true;
			tray_container_publish(ui);
		}
		else if (event->xreparent.parent != state->slot)
			ui->stopped = true;
		return true;
	}
	if (event->type == DestroyNotify &&
	    (event->xdestroywindow.window == state->slot ||
	     event->xdestroywindow.window == state->offer))
	{
		tray_container_detach(ui);
		return true;
	}
	if (event->type == ConfigureNotify &&
	    event->xconfigure.window == state->slot)
	{
		ui->dirty = true;
		return true;
	}
	if (event->type == UnmapNotify &&
	    event->xunmap.window == state->slot)
	{
		tray_container_return(ui, 0);
		return true;
	}
	return false;
}
