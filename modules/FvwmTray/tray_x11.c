/* This program is free software; you can redistribute it and/or modify it
 * under the GNU General Public License, version 2 or any later version. */
#include "config.h"
#include "tray.h"
#include "libs/FProperty.h"
#include "libs/XError.h"
#include "libs/log.h"
#include <X11/Xatom.h>
#include <X11/keysym.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#define XEMBED_EMBEDDED_NOTIFY 0
#define XEMBED_WINDOW_ACTIVATE 1
#define XEMBED_WINDOW_DEACTIVATE 2
#define XEMBED_REQUEST_FOCUS 3
#define XEMBED_FOCUS_IN 4
#define XEMBED_FOCUS_OUT 5
#define XEMBED_FOCUS_NEXT 6
#define XEMBED_FOCUS_PREV 7

/* Xlib's temporary handler is process-global and not nestable. Each operation
 * fences previous requests, then admits BadWindow only for its foreign XID. */
static struct
{
	Display *display;
	Window client;
	unsigned long serial;
	bool vanished, active, unexpected;
} tray_trap;
static tray_ui *tray_error_owner;

static int tray_error(Display *display, XErrorEvent *event)
{
	(void)display;
	if (tray_error_owner)
	{
		tray_error_owner->failed = true;
		tray_error_owner->x_error_code = event->error_code;
		tray_error_owner->x_error_resource = event->resourceid;
	}
	if (tray_trap.active)
		tray_trap.unexpected = true;
	return 0;
}

static int foreign_error(Display *display, XErrorEvent *event)
{
	if (display == tray_trap.display && event->error_code == BadWindow &&
	    event->resourceid == tray_trap.client &&
	    event->serial >= tray_trap.serial)
	{
		tray_trap.vanished = true;
		return 0;
	}
	return ferror_call_next_error_handler(display, event);
}

static void foreign_begin(tray_ui *ui, Window client)
{
	XSync(ui->display, False);
	tray_trap.display = ui->display;
	tray_trap.client = client;
	tray_trap.serial = NextRequest(ui->display);
	tray_trap.vanished = false;
	tray_trap.unexpected = false;
	tray_trap.active = true;
	ferror_set_temp_error_handler(foreign_error);
}

static bool foreign_end(tray_ui *ui)
{
	XSync(ui->display, False);
	ferror_reset_temp_error_handler();
	tray_trap.active = false;
	return !tray_trap.vanished && !tray_trap.unexpected;
}

static void refused(tray_ui *ui)
{
	if (ui->refusals < UINT32_MAX)
		ui->refusals++;
	unsigned long count = ui->refusals;
	Atom atom = XInternAtom(ui->display, "_FVWM_TRAY_REFUSALS", False);
	XChangeProperty(ui->display, ui->owner, atom, XA_CARDINAL, 32,
	    PropModeReplace, (unsigned char *)&count, 1);
	if (ui->refusals <= 8)
		fvwm_debug(__func__,
		    "FvwmTray: refused dock or property update");
}

static bool own_window(const tray_ui *ui, Window window)
{
	if (!window || window == ui->root || window == ui->owner ||
	    window == ui->host || window == ui->proxy)
		return true;
	for (unsigned int i = 0; i < ui->model.count; i++)
		if (window == ui->sites[i].socket)
			return true;
	return false;
}

static bool read_info(tray_ui *ui, Window client, bool *mapped)
{
	unsigned char *bytes = NULL;
	size_t length = 0;
	*mapped = false;
	FPropertyResult result = FPropertyRead(ui->display, client, ui->info,
	    ui->info, (FPropertyLimits){8, 8, 32}, &bytes, &length);
	bool valid = result == FPROPERTY_OK && length == 8 &&
	    tray_model_info((const uint32_t *)bytes, mapped);
	free(bytes);
	return valid;
}

static bool parent_of(tray_ui *ui, Window client, Window *parent)
{
	Window root = None, *children = NULL;
	unsigned int count = 0;
	*parent = None;
	Status got = XQueryTree(ui->display, client, &root, parent,
	    &children, &count);
	if (children)
		XFree(children);
	return got && root == ui->root;
}

static bool send_embed(tray_ui *ui, Window client, long message, long detail,
	long first, long second, Time timestamp)
{
	XEvent event = {0};
	event.xclient.type = ClientMessage;
	event.xclient.display = ui->display;
	event.xclient.window = client;
	event.xclient.message_type = ui->xembed;
	event.xclient.format = 32;
	event.xclient.data.l[0] = timestamp;
	event.xclient.data.l[1] = message;
	event.xclient.data.l[2] = detail;
	event.xclient.data.l[3] = first;
	event.xclient.data.l[4] = second;
	int index = tray_model_index(&ui->model, (uint32_t)client);
	if (index < 0)
		return false;
	XGrabServer(ui->display);
	foreign_begin(ui, client);
	Window parent = None;
	Status sent = parent_of(ui, client, &parent) &&
	    parent == ui->sites[index].socket &&
	    XSendEvent(ui->display, client, False, NoEventMask, &event);
	bool valid = foreign_end(ui) && sent;
	XUngrabServer(ui->display);
	XSync(ui->display, False);
	return valid;
}


static void retire(tray_ui *ui, unsigned int index)
{
	if (index >= ui->model.count)
		return;
	Window client = ui->model.clients[index].id;
	tray_site site = ui->sites[index];
	if (ui->model.focused == client)
		tray_x11_unfocus(ui, true);
	/* A fixed server transaction prevents a client from changing its parent
	 * or reusing its XID between the state check and save-set release. */
	XGrabServer(ui->display);
	foreign_begin(ui, client);
	Window parent = None;
	bool exists = parent_of(ui, client, &parent);
	if (exists && parent == site.socket)
	{
		XUnmapWindow(ui->display, client);
		XSetWindowBorderWidth(ui->display, client, site.border_width);
		XReparentWindow(ui->display, client, ui->root, 0, 0);
	}
	bool returned = foreign_end(ui);
	bool gone = tray_trap.vanished;
	if (exists && returned && site.saved)
	{
		foreign_begin(ui, client);
		XRemoveFromSaveSet(ui->display, client);
		returned = foreign_end(ui);
	}
	/* Never destroy a container after an unexplained failed release.
	 * Connection teardown retains the save set and performs recovery. */
	if (returned || gone)
		XDestroyWindow(ui->display, site.socket);
	else
		ui->release_pending = true;
	XUngrabServer(ui->display);
	XSync(ui->display, False);
	bool was_visible = ui->layout.cells[index].visible;
	tray_model_remove(&ui->model, (uint32_t)client);
	memmove(&ui->layout.cells[index], &ui->layout.cells[index + 1],
	    (ui->model.count - index) * sizeof(ui->layout.cells[0]));
	memset(&ui->layout.cells[ui->model.count], 0,
	    sizeof(ui->layout.cells[0]));
	if (was_visible && ui->layout.count)
		ui->layout.count--;
	memmove(&ui->sites[index], &ui->sites[index + 1],
	    (ui->model.count - index) * sizeof(ui->sites[0]));
	memset(&ui->sites[ui->model.count], 0, sizeof(ui->sites[0]));
	ui->dirty = true;
}

static void activation(tray_ui *ui, bool active, Time timestamp)
{
	if (ui->active == active)
		return;
	ui->active = active;
	for (unsigned int i = 0; i < ui->model.count;)
	{
		if (!send_embed(ui, ui->model.clients[i].id,
		    active ? XEMBED_WINDOW_ACTIVATE : XEMBED_WINDOW_DEACTIVATE,
		    0, 0, 0, timestamp))
			retire(ui, i);
		else
			i++;
	}
	ui->dirty = true;
}

void tray_x11_unfocus(tray_ui *ui, bool restore)
{
	if (ui->model.focused)
		send_embed(ui, ui->model.focused, XEMBED_FOCUS_OUT,
		    0, 0, 0, CurrentTime);
	ui->model.focused = 0;
	ui->focus_budget = 0;
	if (restore && ui->previous_focus && ui->active)
	{
		char command[96];
		snprintf(command, sizeof(command), "Silent WindowId %lu Focus",
		    ui->previous_focus);
		SendText(ui->channel, command, 0);
	}
	ui->dirty = true;
}

static void focus_apply(tray_ui *ui, uint32_t id, int direction,
	Time timestamp)
{
	uint32_t previous = ui->model.focused;
	if (!ui->visible || !ui->layout.count)
		return;
	if (id)
	{
		int index = tray_model_index(&ui->model, id);
		if (index < 0 || !ui->layout.cells[index].visible)
			return;
		ui->model.focused = id;
	}
	else
		id = tray_model_focus(&ui->model, &ui->layout, direction);
	if (!id)
		return;
	Window focused = None;
	int revert;
	XGetInputFocus(ui->display, &focused, &revert);
	if (focused != ui->proxy && focused != ui->host &&
	    focused != None && focused != PointerRoot)
		ui->previous_focus = focused;
	if (previous && previous != id)
		send_embed(ui, previous, XEMBED_FOCUS_OUT, 0, 0, 0, timestamp);
	XSetInputFocus(ui->display, ui->proxy, RevertToPointerRoot, timestamp);
	XGetInputFocus(ui->display, &focused, &revert);
	activation(ui, focused == ui->proxy, timestamp);
	int detail = direction < 0 ? 2 : (direction > 0 ? 1 : 0);
	if (!send_embed(ui, id, XEMBED_FOCUS_IN, detail, 0, 0, timestamp))
	{
		int index = tray_model_index(&ui->model, id);
		if (index >= 0)
			retire(ui, (unsigned int)index);
	}
	ui->dirty = true;
}

void tray_x11_focus(tray_ui *ui, uint32_t id, int direction, Time timestamp)
{
	ui->focus_budget = ui->layout.count ? ui->layout.count - 1 : 0;
	focus_apply(ui, id, direction, timestamp);
}

static void dock(tray_ui *ui, Window client)
{
	XWindowAttributes attributes;
	bool mapped = false;
	if (own_window(ui, client) || tray_model_index(&ui->model,
	    (uint32_t)client) >= 0 || ui->model.count == TRAY_CLIENT_MAX ||
	    ui->model.generation == UINT64_MAX)
	{
		refused(ui);
		return;
	}
	unsigned int index = ui->model.count;
	Window socket = None, parent = None;
	bool saved = false;
	/* Validate and adopt one immutable server-side window lifetime. The
	 * grab contains only fixed requests and small local policy operations;
	 * it is always released before diagnostics or protocol callbacks. */
	XGrabServer(ui->display);
	foreign_begin(ui, client);
	Window host_parent = None;
	if (parent_of(ui, ui->host, &host_parent) && host_parent != ui->root)
		ui->stopped = true;
	Status got = XGetWindowAttributes(ui->display, client, &attributes);
	bool valid = !ui->stopped && got && attributes.root == ui->root &&
	    attributes.class == InputOutput &&
	    attributes.depth == DefaultDepth(ui->display,
	    DefaultScreen(ui->display)) &&
	    attributes.visual == DefaultVisual(ui->display,
	    DefaultScreen(ui->display)) &&
	    tray_model_size(attributes.width, attributes.height) &&
	    parent_of(ui, client, &parent) && parent == ui->root &&
	    read_info(ui, client, &mapped);
	valid = foreign_end(ui) && valid;
	if (valid)
	{
		socket = XCreateSimpleWindow(ui->display, ui->host, 0, 0,
		    attributes.width, attributes.height, 0, 0, 0);
		XSelectInput(ui->display, socket, SubstructureRedirectMask |
		    SubstructureNotifyMask | StructureNotifyMask |
		    ButtonPressMask);
		unsigned long serial = NextRequest(ui->display);
		foreign_begin(ui, client);
		XSelectInput(ui->display, client,
		    StructureNotifyMask | PropertyChangeMask);
		XAddToSaveSet(ui->display, client);
		XUnmapWindow(ui->display, client);
		XSetWindowBorderWidth(ui->display, client, 0);
		XReparentWindow(ui->display, client, socket, 0, 0);
		valid = foreign_end(ui);
		saved = valid;
		if (valid)
		{
			tray_admission admitted = tray_model_add(&ui->model,
			    (uint32_t)client, attributes.width,
			    attributes.height,
			    mapped);
			valid = admitted == TRAY_ADMITTED;
			if (valid)
				ui->sites[index] = (tray_site){
				    .socket = socket, .serial = serial,
				    .saved = true,
				    .border_width =
				    (unsigned int)attributes.border_width};
		}
	}
	if (!valid && socket)
	{
		foreign_begin(ui, client);
		bool exists = parent_of(ui, client, &parent);
		if (exists && parent == socket)
		{
			XUnmapWindow(ui->display, client);
			XSetWindowBorderWidth(ui->display, client,
			    attributes.border_width);
			XReparentWindow(ui->display, client, ui->root, 0, 0);
		}
		bool returned = foreign_end(ui);
		bool gone = tray_trap.vanished;
		if (exists && returned && saved)
		{
			foreign_begin(ui, client);
			XRemoveFromSaveSet(ui->display, client);
			returned = foreign_end(ui);
		}
		if (returned || gone)
			XDestroyWindow(ui->display, socket);
		else
			ui->release_pending = true;
	}
	XUngrabServer(ui->display);
	XSync(ui->display, False);
	if (!valid)
	{
		refused(ui);
		return;
	}
	/* The first notification follows the initial actual mapping. Render is
	 * outside the server transaction and cannot expose a partial member. */
	ui->dirty = true;
	tray_render(ui);
	int current = tray_model_index(&ui->model, (uint32_t)client);
	if (current >= 0)
	{
		bool notified = send_embed(ui, client,
		    XEMBED_EMBEDDED_NOTIFY, 0,
		    (long)socket, 0, CurrentTime);
		if (notified && ui->active)
			notified = send_embed(ui, client,
			    XEMBED_WINDOW_ACTIVATE,
			    0, 0, 0, CurrentTime);
		if (!notified)
			retire(ui, (unsigned int)current);
	}
}

void tray_x11_layout(tray_ui *ui)
{
	int focused = tray_model_index(&ui->model, ui->model.focused);
	if (focused >= 0 && !ui->layout.cells[focused].visible)
		tray_x11_unfocus(ui, true);
	for (unsigned int i = 0; i < ui->model.count;)
	{
		const tray_cell *cell = &ui->layout.cells[i];
		Window client = ui->model.clients[i].id;
		Window socket = ui->sites[i].socket, parent = None;
		XGrabServer(ui->display);
		foreign_begin(ui, client);
		bool member = parent_of(ui, client, &parent) &&
		    parent == socket;
		if (member)
		{
			const tray_client *record = &ui->model.clients[i];
			/* Hidden clients also receive the accepted size.
			 * ConfigureNotify must describe actual pixels. */
			XResizeWindow(ui->display, socket,
			    record->width, record->height);
			XSetWindowBorderWidth(ui->display, client, 0);
			XMoveResizeWindow(ui->display, client, 0, 0,
			    record->width, record->height);
			if (cell->visible)
			{
				XMoveWindow(ui->display, socket,
				    cell->x, cell->y);
				XMapWindow(ui->display, client);
				XMapWindow(ui->display, socket);
			}
			else
			{
				XUnmapWindow(ui->display, client);
				XUnmapWindow(ui->display, socket);
			}
		}
		bool valid = foreign_end(ui) && member;
		XUngrabServer(ui->display);
		XSync(ui->display, False);
		if (!valid)
		{
			retire(ui, i);
			/* Reproject after compacting membership. */
			break;
		}
		i++;
	}
}

static void info_changed(tray_ui *ui, unsigned int index)
{
	Window client = ui->model.clients[index].id, parent = None;
	bool mapped = false;
	XGrabServer(ui->display);
	foreign_begin(ui, client);
	bool member = parent_of(ui, client, &parent) &&
	    parent == ui->sites[index].socket;
	bool valid = member && read_info(ui, client, &mapped);
	valid = foreign_end(ui) && valid;
	XUngrabServer(ui->display);
	XSync(ui->display, False);
	if (!valid)
	{
		refused(ui);
		retire(ui, index);
		return;
	}
	if (!mapped && ui->model.focused == client)
		tray_x11_unfocus(ui, true);
	tray_model_mapped(&ui->model, (uint32_t)client, mapped);
	ui->dirty = true;
}

static void geometry_changed(tray_ui *ui, unsigned int index)
{
	Window client = ui->model.clients[index].id, parent = None;
	XWindowAttributes attributes;
	XGrabServer(ui->display);
	foreign_begin(ui, client);
	bool member = parent_of(ui, client, &parent) &&
	    parent == ui->sites[index].socket &&
	    XGetWindowAttributes(ui->display, client, &attributes);
	bool valid = foreign_end(ui) && member;
	XUngrabServer(ui->display);
	XSync(ui->display, False);
	if (!valid)
	{
		retire(ui, index);
		return;
	}
	if (!tray_model_size(attributes.width, attributes.height))
	{
		refused(ui);
		ui->dirty = true;
		return;
	}
	const tray_client *old = &ui->model.clients[index];
	if (old->width != (unsigned int)attributes.width ||
	    old->height != (unsigned int)attributes.height)
	{
		tray_model_resize(&ui->model, old->id,
		    attributes.width, attributes.height);
		ui->dirty = true;
	}
	if (attributes.x || attributes.y || attributes.border_width)
		ui->dirty = true;
}

static void configure(tray_ui *ui, unsigned int index,
	XConfigureRequestEvent *request)
{
	const tray_client *client = &ui->model.clients[index];
	uint32_t id = client->id;
	unsigned int width = request->value_mask & CWWidth
	    ? (unsigned int)request->width : client->width;
	unsigned int height = request->value_mask & CWHeight
	    ? (unsigned int)request->height : client->height;
	if (!tray_model_resize(&ui->model, id, width, height))
		refused(ui);
	ui->dirty = true;
	tray_render(ui);
	int current = tray_model_index(&ui->model, id);
	if (current < 0)
		return;
	client = &ui->model.clients[current];
	XEvent event = {0};
	event.xconfigure.type = ConfigureNotify;
	event.xconfigure.display = ui->display;
	event.xconfigure.event = event.xconfigure.window = id;
	event.xconfigure.width = client->width;
	event.xconfigure.height = client->height;
	event.xconfigure.override_redirect = False;
	Window parent = None;
	XGrabServer(ui->display);
	foreign_begin(ui, id);
	bool member = parent_of(ui, id, &parent) &&
	    parent == ui->sites[current].socket;
	if (member)
		XSendEvent(ui->display, id, False, StructureNotifyMask, &event);
	bool valid = foreign_end(ui) && member;
	XUngrabServer(ui->display);
	XSync(ui->display, False);
	if (!valid)
		retire(ui, (unsigned int)current);
}

static bool hit_target(const tray_cell *target, int x, int y)
{
	return target->visible && x >= target->x && y >= target->y &&
	    x - target->x < target->width && y - target->y < target->height;
}

void tray_x11_event(tray_ui *ui, XEvent *event)
{
	if (event->type == SelectionClear &&
	    event->xselectionclear.selection == ui->selection)
	{
		ui->claimed = false;
		ui->stopped = true;
		return;
	}
	if (event->type == ClientMessage &&
	    event->xclient.window == ui->owner &&
	    event->xclient.message_type == ui->opcode)
	{
		if (event->xclient.format == 32 &&
		    event->xclient.data.l[1] == 0)
			dock(ui, (Window)(uint32_t)event->xclient.data.l[2]);
		else
			refused(ui);
		return;
	}
	if (event->type == ReparentNotify && !event->xany.send_event &&
	    event->xreparent.window == ui->host &&
	    event->xreparent.parent != ui->root)
	{
		/* Foreign containers are outside this first profile. */
		ui->stopped = true;
		return;
	}
	if ((event->type == FocusIn || event->type == FocusOut) &&
	    event->xfocus.window == ui->proxy && !event->xany.send_event)
	{
		activation(ui, event->type == FocusIn, CurrentTime);
		return;
	}
	if (event->type == ButtonPress && !event->xany.send_event)
	{
		if (event->xbutton.window == ui->host)
		{
			if (hit_target(&ui->page_prev, event->xbutton.x,
			    event->xbutton.y))
				tray_page(ui, -1, false, event->xbutton.time);
			else if (hit_target(&ui->page_next, event->xbutton.x,
			    event->xbutton.y))
				tray_page(ui, 1, false, event->xbutton.time);
			else
				tray_x11_focus(ui, 0, 0, event->xbutton.time);
		}
		for (unsigned int i = 0; i < ui->model.count; i++)
			if (event->xbutton.window == ui->sites[i].socket)
				tray_x11_focus(ui, ui->model.clients[i].id,
				    0, event->xbutton.time);
		return;
	}
	if ((event->type == KeyPress || event->type == KeyRelease) &&
	    event->xkey.window == ui->proxy && !event->xany.send_event)
	{
		if (event->type == KeyPress && ui->active)
			ui->focus_budget = ui->layout.count
			    ? ui->layout.count - 1 : 0;
		KeySym key = XLookupKeysym(&event->xkey, 0);
		if (key == XK_Escape)
		{
			if (event->type == KeyPress)
			{
				tray_x11_unfocus(ui, true);
				if (ui->anchored)
				{
					ui->visible = false;
					ui->dirty = true;
				}
			}
			return;
		}
		if (key == XK_Page_Up || key == XK_Page_Down)
		{
			if (event->type == KeyPress)
				tray_page(ui, key == XK_Page_Up ? -1 : 1,
				    true, event->xkey.time);
			return;
		}
		if (key == XK_F6)
		{
			if (event->type == KeyPress)
				tray_x11_focus(ui, 0,
				    event->xkey.state & ShiftMask ? -1 : 1,
				    event->xkey.time);
			return;
		}
		if (ui->model.focused)
		{
			XEvent forwarded = *event;
			forwarded.xkey.window = ui->model.focused;
			int index = tray_model_index(&ui->model,
			    ui->model.focused);
			Window parent = None;
			XGrabServer(ui->display);
			foreign_begin(ui, ui->model.focused);
			bool member = index >= 0 && parent_of(ui,
			    ui->model.focused, &parent) &&
			    parent == ui->sites[index].socket;
			if (member)
				XSendEvent(ui->display, ui->model.focused,
				    False,
				    NoEventMask, &forwarded);
			bool valid = foreign_end(ui) && member;
			XUngrabServer(ui->display);
			XSync(ui->display, False);
			if (!valid && index >= 0)
				retire(ui, (unsigned int)index);
		}
		return;
	}
	for (unsigned int i = 0; i < ui->model.count; i++)
	{
		Window client = ui->model.clients[i].id;
		if (event->type == ClientMessage &&
		    event->xclient.window == ui->sites[i].socket &&
		    event->xclient.message_type == ui->xembed &&
		    event->xclient.format == 32)
		{
			long opcode = event->xclient.data.l[1];
			Time timestamp =
			    (Time)(uint32_t)event->xclient.data.l[0];
			if (opcode == XEMBED_REQUEST_FOCUS)
				tray_x11_focus(ui, (uint32_t)client, 0,
				    timestamp);
			else if (opcode == XEMBED_FOCUS_NEXT ||
			    opcode == XEMBED_FOCUS_PREV)
			{
				if (ui->model.focused == client)
				{
					if (ui->focus_budget)
					{
						ui->focus_budget--;
						focus_apply(ui, 0,
						    opcode == XEMBED_FOCUS_PREV
						    ? -1 : 1,
						    timestamp);
					}
					else
						tray_x11_unfocus(ui, true);
				}
			}
			return;
		}
		Window subject = event->xany.window;
		if (event->type == DestroyNotify)
			subject = event->xdestroywindow.window;
		else if (event->type == ReparentNotify)
			subject = event->xreparent.window;
		else if (event->type == ConfigureRequest)
			subject = event->xconfigurerequest.window;
		else if (event->type == MapRequest)
			subject = event->xmaprequest.window;
		else if (event->type == ConfigureNotify)
			subject = event->xconfigure.window;
		else if (event->type == MapNotify)
			subject = event->xmap.window;
		else if (event->type == UnmapNotify)
			subject = event->xunmap.window;
		if (subject != client || event->xany.send_event ||
		    event->xany.serial < ui->sites[i].serial)
			continue;
		if (event->type == DestroyNotify ||
		    event->type == ReparentNotify)
		{
			/* Check actual membership for a queued event. An
			 * reused XIDs outside our socket are not moved. */
			Window parent = None;
			foreign_begin(ui, client);
			bool member = parent_of(ui, client, &parent) &&
			    parent == ui->sites[i].socket;
			foreign_end(ui);
			if (!member)
				retire(ui, i);
		}
		else if (event->type == PropertyNotify &&
		    event->xproperty.atom == ui->info)
			info_changed(ui, i);
		else if (event->type == ConfigureRequest)
			configure(ui, i, &event->xconfigurerequest);
		else if (event->type == ConfigureNotify)
			geometry_changed(ui, i);
		else if (event->type == MapRequest || event->type == MapNotify)
		{
			if (!ui->layout.cells[i].visible)
				ui->dirty = true;
		}
		else if (event->type == UnmapNotify &&
		    ui->layout.cells[i].visible)
			ui->dirty = true;
		return;
	}
}

void tray_x11_hints(tray_ui *ui)
{
	unsigned long orientation = ui->vertical ? 1 : 0;
	unsigned long visual = XVisualIDFromVisual(DefaultVisual(ui->display,
	    DefaultScreen(ui->display)));
	unsigned long host = ui->host, pid = (unsigned long)getpid();
	Atom pid_atom = XInternAtom(ui->display, "_NET_WM_PID", False);
	XChangeProperty(ui->display, ui->owner, ui->orientation_atom,
	    XA_CARDINAL, 32, PropModeReplace, (unsigned char *)&orientation, 1);
	XChangeProperty(ui->display, ui->owner, ui->visual_atom,
	    XInternAtom(ui->display, "VISUALID", False), 32, PropModeReplace,
	    (unsigned char *)&visual, 1);
	XChangeProperty(ui->display, ui->owner, ui->host_atom, XA_WINDOW, 32,
	    PropModeReplace, (unsigned char *)&host, 1);
	XChangeProperty(ui->display, ui->owner, pid_atom, XA_CARDINAL, 32,
	    PropModeReplace, (unsigned char *)&pid, 1);
	XChangeProperty(ui->display, ui->host, pid_atom, XA_CARDINAL, 32,
	    PropModeReplace, (unsigned char *)&pid, 1);
}

static bool startup_clock(struct timespec *now)
{
	return clock_gettime(CLOCK_MONOTONIC, now) == 0;
}

static Time server_timestamp(tray_ui *ui)
{
	unsigned char byte = 1;
	XEvent event;
	Time timestamp = CurrentTime;

	XChangeProperty(ui->display, ui->owner, ui->timestamp, XA_INTEGER, 8,
	    PropModeReplace, &byte, 1);
	XSync(ui->display, False);
	for (unsigned int i = 0; i < 32 &&
	    XCheckWindowEvent(ui->display, ui->owner, PropertyChangeMask,
	    &event); i++)
		if (event.type == PropertyNotify &&
		    event.xproperty.atom == ui->timestamp)
			timestamp = event.xproperty.time;
	return timestamp;
}

static int startup_remaining(const struct timespec *started)
{
	struct timespec now;

	if (!startup_clock(&now) || now.tv_sec < started->tv_sec ||
	    now.tv_sec - started->tv_sec > 2)
		return 0;
	long elapsed = (now.tv_sec - started->tv_sec) * 1000 +
	    (now.tv_nsec - started->tv_nsec) / 1000000;
	return elapsed >= 2000 ? 0 : (int)(2000 - elapsed);
}

static Time acquire_selection(tray_ui *ui)
{
	struct timespec started;

	if (!startup_clock(&started))
		return CurrentTime;
	/* One total monotonic grace period includes interrupted polls. It
	 * cannot preempt a single Xlib request to an unresponsive server. */
	for (;;)
	{
		if (!startup_remaining(&started) || !tray_startup_pause(ui, 0))
			return CurrentTime;
		Time timestamp = server_timestamp(ui);
		if (timestamp == CurrentTime || ui->failed)
			return CurrentTime;
		/* Only the fixed ownership check/claim/verification is grabbed.
		 * A fresh timestamp for each attempt handles a previous
		 * owner's release invalidating an earlier sample. */
		XGrabServer(ui->display);
		if (XGetSelectionOwner(ui->display, ui->selection) == None)
		{
			XSetSelectionOwner(ui->display, ui->selection,
			    ui->owner, timestamp);
			ui->claimed = XGetSelectionOwner(ui->display,
			    ui->selection) == ui->owner;
		}
		XUngrabServer(ui->display);
		XSync(ui->display, False);
		if (ui->claimed || ui->failed)
			return ui->failed ? CurrentTime : timestamp;
		int remaining = startup_remaining(&started);
		if (!remaining)
			return CurrentTime;
		int wait = remaining < 50 ? remaining : 50;
		if (!tray_startup_pause(ui, wait))
			return CurrentTime;
	}
}

bool tray_x11_start(tray_ui *ui)
{
	tray_error_owner = ui;
	XSetErrorHandler(tray_error);
	ui->root = DefaultRootWindow(ui->display);
	int screen = DefaultScreen(ui->display);
	char selection[64];
	snprintf(selection, sizeof(selection), "_NET_SYSTEM_TRAY_S%d", screen);
	ui->selection = XInternAtom(ui->display, selection, False);
	ui->manager = XInternAtom(ui->display, "MANAGER", False);
	ui->opcode = XInternAtom(ui->display, "_NET_SYSTEM_TRAY_OPCODE", False);
	ui->xembed = XInternAtom(ui->display, "_XEMBED", False);
	ui->info = XInternAtom(ui->display, "_XEMBED_INFO", False);
	ui->timestamp = XInternAtom(ui->display, "_FVWM_TRAY_TIMESTAMP", False);
	ui->orientation_atom = XInternAtom(ui->display,
	    "_NET_SYSTEM_TRAY_ORIENTATION", False);
	ui->visual_atom = XInternAtom(ui->display, "_NET_SYSTEM_TRAY_VISUAL",
	    False);
	ui->host_atom = XInternAtom(ui->display, "_FVWM_TRAY_HOST", False);
	ui->workarea_atom = XInternAtom(ui->display, "_NET_WORKAREA", False);
	XSetWindowAttributes attributes = {0};
	attributes.override_redirect = True;
	attributes.event_mask = PropertyChangeMask | StructureNotifyMask;
	ui->owner = XCreateWindow(ui->display, ui->root, -1, -1, 1, 1, 0,
	    0, InputOnly, CopyFromParent, CWOverrideRedirect | CWEventMask,
	    &attributes);
	attributes.event_mask = ExposureMask | StructureNotifyMask |
	    ButtonPressMask;
	ui->host = XCreateWindow(ui->display, ui->root, 0, 0, 1, 1, 0,
	    CopyFromParent, InputOutput, CopyFromParent,
	    CWOverrideRedirect | CWEventMask, &attributes);
	ui->proxy = XCreateSimpleWindow(ui->display, ui->host, -1, -1,
	    1, 1, 0, 0, 0);
	XSelectInput(ui->display, ui->proxy, KeyPressMask | KeyReleaseMask |
	    FocusChangeMask);
	XStoreName(ui->display, ui->host, "FvwmTray");
	XClassHint class_hint = {"FvwmTray", "FvwmTray"};
	XSetClassHint(ui->display, ui->host, &class_hint);
	XSelectInput(ui->display, ui->root, PropertyChangeMask);
	tray_x11_hints(ui);
	unsigned long count = 0;
	Atom refusals = XInternAtom(ui->display, "_FVWM_TRAY_REFUSALS", False);
	XChangeProperty(ui->display, ui->owner, refusals, XA_CARDINAL, 32,
	    PropModeReplace, (unsigned char *)&count, 1);
	Time timestamp = acquire_selection(ui);
	if (!ui->claimed || timestamp == CurrentTime || ui->failed)
		return false;
	XEvent event;
	memset(&event, 0, sizeof(event));
	event.xclient.type = ClientMessage;
	event.xclient.window = ui->root;
	event.xclient.message_type = ui->manager;
	event.xclient.format = 32;
	event.xclient.data.l[0] = timestamp;
	event.xclient.data.l[1] = ui->selection;
	event.xclient.data.l[2] = ui->owner;
	XSendEvent(ui->display, ui->root, False, StructureNotifyMask, &event);
	return !ui->failed;
}

void tray_x11_stop(tray_ui *ui)
{
	if (!ui->display)
		return;
	tray_x11_unfocus(ui, true);
	while (ui->model.count)
		retire(ui, ui->model.count - 1);
	if (ui->claimed &&
	    XGetSelectionOwner(ui->display, ui->selection) == ui->owner)
		XSetSelectionOwner(ui->display, ui->selection, None,
		    CurrentTime);
	if (ui->host && !ui->release_pending)
		XDestroyWindow(ui->display, ui->host);
	if (ui->owner)
		XDestroyWindow(ui->display, ui->owner);
	XSync(ui->display, False);
	tray_error_owner = NULL;
}
