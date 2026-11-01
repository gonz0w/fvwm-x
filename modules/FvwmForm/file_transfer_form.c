/* This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or any later version. */
#include "config.h"
#include "libs/fvwmlib.h"
#include "libs/file_transfer_protocol.h"
#include "FvwmForm.h"
#include "file_transfer_form.h"

#include <X11/Xatom.h>
#include <X11/keysym.h>
#include <X11/cursorfont.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <unistd.h>
#include <fcntl.h>
#include <poll.h>
#include <errno.h>
#include <ctype.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define TRANSFER_QUEUE 8
#ifndef MSG_NOSIGNAL
#define MSG_NOSIGNAL 0
#endif

/* One explicitly opted-in list per Form. Storage outlives event callbacks and
 * is retired before the Form's list storage or libevent base. */
static struct
{
	bool configured, connecting, ready, pressed, dragging, destroyed;
	bool frame_destroyed, failed;
	char name[256], path[sizeof(((struct sockaddr_un *)0)->sun_path)];
	unsigned char nonce[16];
	uint64_t epoch;
	uint32_t generation;
	Item *list;
	int fd, index, threshold;
	XButtonEvent press;
	struct file_transfer_record snapshot;
	struct event *read_event, *write_event, *deadline;
	unsigned char input[FILE_TRANSFER_WIRE_BYTES];
	size_t received, sent;
	unsigned int queued;
	struct
	{
		enum file_transfer_message kind;
		unsigned char wire[FILE_TRANSFER_WIRE_BYTES];
	} output[TRANSFER_QUEUE];
	Atom aware, xdnd[6];
	Cursor copy_cursor, move_cursor, reject_cursor;
	enum file_transfer_feedback feedback;
} transfer = {.fd = -1};

static struct file_transfer_record transfer_record(
	enum file_transfer_message kind)
{
	struct file_transfer_record record = {0};

	record.kind = kind;
	record.epoch = transfer.epoch;
	record.generation = transfer.generation;
	record.frame = (uint32_t)CF.frame;
	record.list = transfer.list ? (uint32_t)transfer.list->header.win : 0;
	memcpy(record.nonce, transfer.nonce, sizeof(record.nonce));
	return record;
}

static void transfer_awareness(bool ready)
{
	unsigned long version = 5;

	transfer.ready = ready;
	if (CF.frame == None || transfer.aware == None ||
	    transfer.frame_destroyed)
		return;
	if (ready)
		XChangeProperty(dpy, CF.frame, transfer.aware, XA_ATOM, 32,
			PropModeReplace, (unsigned char *)&version, 1);
	else
		XDeleteProperty(dpy, CF.frame, transfer.aware);
}

static void transfer_ungrab(void)
{
	if (!transfer.pressed)
		return;
	transfer.pressed = transfer.dragging = false;
	XUngrabPointer(dpy, CurrentTime);
	if (CF.grab_server && CF.server_grabbed && !transfer.frame_destroyed)
		XGrabPointer(dpy, CF.frame, True, 0, GrabModeAsync,
			GrabModeAsync, None, None, CurrentTime);
}

static void transfer_feedback(enum file_transfer_feedback feedback)
{
	Cursor cursor = CF.pointer[input_pointer];

	transfer.feedback = feedback;
	if (transfer.destroyed || transfer.list == NULL)
		return;
	if (feedback == FILE_TRANSFER_COPY)
		cursor = transfer.copy_cursor;
	else if (feedback == FILE_TRANSFER_MOVE)
		cursor = transfer.move_cursor;
	else if (feedback == FILE_TRANSFER_REJECTED)
		cursor = transfer.reject_cursor;
	XDefineCursor(dpy, transfer.list->header.win, cursor);
	if (transfer.pressed)
		XChangeActivePointerGrab(dpy,
			ButtonReleaseMask | PointerMotionMask,
			cursor, CurrentTime);
	RedrawItem(transfer.list, 0, NULL);
}

static void transfer_disconnect(const char *reason)
{
	if (reason != NULL)
		fprintf(stderr, "FvwmForm: file transfer %s\n", reason);
	transfer_awareness(false);
	transfer_ungrab();
	transfer_feedback(FILE_TRANSFER_IDLE);
	if (transfer.read_event != NULL)
		event_del(transfer.read_event);
	if (transfer.write_event != NULL)
		event_del(transfer.write_event);
	if (transfer.deadline != NULL)
		event_del(transfer.deadline);
	if (transfer.fd >= 0)
		close(transfer.fd);
	transfer.fd = -1;
	transfer.connecting = false;
	transfer.received = transfer.sent = transfer.queued = 0;
}

static bool transfer_queue(const struct file_transfer_record *record)
{
	unsigned int slot = transfer.queued;

	if (transfer.fd < 0)
		return false;
	/* Never replace a partial record or cross an ordering boundary.
	 * Only the last, wholly unsent motion can be coalesced. */
	if (record->kind == FILE_TRANSFER_MOTION && slot > 0 &&
	    transfer.output[slot - 1].kind == FILE_TRANSFER_MOTION &&
	    (slot > 1 || transfer.sent == 0))
		slot--;
	if (slot == TRANSFER_QUEUE)
	{
		/* EOF explicitly cancels in the helper. Do not overwrite a
		 * partial record or fabricate a release after overflow. */
		transfer_disconnect("queue overflow; cancelled");
		return false;
	}
	if (!file_transfer_encode(record, transfer.output[slot].wire))
	{
		transfer_disconnect("invalid metadata; cancelled");
		return false;
	}
	transfer.output[slot].kind = record->kind;
	transfer.queued = slot + 1;
	if (event_add(transfer.write_event, NULL) < 0)
	{
		transfer_disconnect("write service unavailable");
		return false;
	}
	return true;
}

static void transfer_cancel(Time time)
{
	if (transfer.pressed)
	{
		struct file_transfer_record record =
			transfer_record(FILE_TRANSFER_CANCEL);

		record.timestamp = (uint32_t)time;
		transfer_queue(&record);
	}
	transfer_ungrab();
	transfer_feedback(FILE_TRANSFER_IDLE);
}

static void transfer_write(evutil_socket_t fd, short events, void *context)
{
	(void)events;
	(void)context;
	if (transfer.connecting)
	{
		int error = 0;
		socklen_t length = sizeof(error);

		if (getsockopt(fd, SOL_SOCKET, SO_ERROR,
			&error, &length) < 0 || error)
		{
			transfer_disconnect("connection refused");
			return;
		}
		transfer.connecting = false;
	}
	for (unsigned int serviced = 0;
	     transfer.queued > 0 && serviced < TRANSFER_QUEUE; serviced++)
	{
		ssize_t count = send(fd,
			transfer.output[0].wire + transfer.sent,
			FILE_TRANSFER_WIRE_BYTES - transfer.sent, MSG_NOSIGNAL);

		if (count < 0 && (errno == EAGAIN || errno == EWOULDBLOCK))
			return;
		if (count < 0 && errno == EINTR)
			return;
		if (count <= 0)
		{
			transfer_disconnect("write failed; cancelled");
			return;
		}
		transfer.sent += (size_t)count;
		if (transfer.sent != FILE_TRANSFER_WIRE_BYTES)
			return;
		transfer.sent = 0;
		transfer.queued--;
		memmove(transfer.output, transfer.output + 1,
			transfer.queued * sizeof(transfer.output[0]));
	}
	if (transfer.queued == 0)
		event_del(transfer.write_event);
}

static void transfer_read(evutil_socket_t fd, short events, void *context)
{
	(void)events;
	(void)context;
	for (unsigned int serviced = 0; serviced < TRANSFER_QUEUE; serviced++)
	{
		struct file_transfer_record record;
		ssize_t count = recv(fd, transfer.input + transfer.received,
			FILE_TRANSFER_WIRE_BYTES - transfer.received, 0);

		if (count < 0 && (errno == EAGAIN || errno == EWOULDBLOCK ||
		    errno == EINTR))
			return;
		if (count <= 0)
		{
			transfer_disconnect("peer closed; cancelled");
			return;
		}
		transfer.received += (size_t)count;
		if (transfer.received != FILE_TRANSFER_WIRE_BYTES)
			return;
		transfer.received = 0;
		if (!file_transfer_decode(transfer.input,
			sizeof(transfer.input), &record) ||
		    record.kind != FILE_TRANSFER_FEEDBACK ||
		    record.epoch != transfer.epoch ||
		    memcmp(record.nonce, transfer.nonce,
			sizeof(record.nonce)) != 0 ||
		    record.frame != (uint32_t)CF.frame ||
		    record.list != (uint32_t)transfer.list->header.win)
		{
			transfer_disconnect("invalid feedback; cancelled");
			return;
		}
		/* Old feedback cannot reactivate the current view. */
		if (record.generation != transfer.generation)
			continue;
		if (record.detail == FILE_TRANSFER_IDLE)
		{
			event_del(transfer.deadline);
			transfer_awareness(true);
		}
		transfer_feedback(record.detail);
	}
}

static void transfer_timeout(evutil_socket_t fd, short events, void *context)
{
	(void)fd;
	(void)events;
	(void)context;
	transfer_disconnect("attachment timed out");
}

/* Whitespace-delimited literal metadata: quotes and backslashes are ordinary
 * bytes, never Form quoting or expansion syntax. */
void form_transfer_configure(char *text)
{
	char name[256], path[256], nonce[33], epoch[21], extra;
	uint64_t value = 0;
	unsigned char bytes[16];

	if (transfer.configured || strlen(text) > 512 ||
	    sscanf(text, "%255s %255s %32s %20s %c", name, path, nonce,
		    epoch, &extra) != 4 || path[0] != '/' ||
	    strlen(path) >= sizeof(transfer.path) || strlen(nonce) != 32 ||
	    epoch[0] < '1' || epoch[0] > '9')
		goto invalid;
	for (size_t i = 0; epoch[i]; i++)
	{
		unsigned int digit = (unsigned char)epoch[i] - '0';

		if (digit > 9 || value > (UINT64_MAX - digit) / 10)
			goto invalid;
		value = value * 10 + digit;
	}
	for (int i = 0; i < 32; i++)
	{
		unsigned char c = (unsigned char)nonce[i];
		unsigned int digit;

		if (c >= '0' && c <= '9')
			digit = c - '0';
		else if (c >= 'a' && c <= 'f')
			digit = c - 'a' + 10;
		else
			goto invalid;
		if ((i & 1) == 0)
			bytes[i / 2] = digit << 4;
		else
			bytes[i / 2] |= digit;
	}
	{
		unsigned char nonzero = 0;

		for (size_t i = 0; i < sizeof(bytes); i++)
			nonzero |= bytes[i];
		if (!nonzero)
			goto invalid;
	}
	strcpy(transfer.name, name);
	strcpy(transfer.path, path);
	memcpy(transfer.nonce, bytes, sizeof(bytes));
	transfer.epoch = value;
	transfer.configured = true;
	return;
invalid:
	fprintf(stderr, "FvwmForm: invalid FileTransfer metadata\n");
}

bool form_transfer_requested(void)
{
	return transfer.configured;
}

void form_transfer_start(struct event_base *base)
{
	static const char *const names[] = {"XdndEnter", "XdndPosition",
		"XdndLeave", "XdndDrop", "XdndStatus", "XdndFinished"};
	struct sockaddr_un address = {0};
	struct stat st;
	struct timeval timeout = {5, 0};
	struct file_transfer_record record;
	char directory[sizeof(transfer.path)], *slash;
	int flags, result;

	if (!transfer.configured)
		return;
	for (Item *p = root_item_ptr; p != NULL; p = p->header.next)
		if (p->type == I_LIST &&
		    strcmp(p->header.name, transfer.name) == 0)
		{
			if (transfer.list != NULL)
				goto invalid;
			transfer.list = p;
		}
	if (base == NULL || transfer.list == NULL ||
	    transfer.list->list.state->generation <= 0 ||
	    CF.frame == None || transfer.list->header.win == None)
		goto invalid;
	strcpy(directory, transfer.path);
	slash = strrchr(directory, '/');
	if (slash == directory)
		goto invalid;
	*slash = '\0';
	if (lstat(directory, &st) < 0 || !S_ISDIR(st.st_mode) ||
	    st.st_uid != getuid() || (st.st_mode & 077) != 0 ||
	    lstat(transfer.path, &st) < 0 || !S_ISSOCK(st.st_mode) ||
	    st.st_uid != getuid())
		goto invalid;
	transfer.fd = socket(AF_UNIX, SOCK_STREAM, 0);
	if (transfer.fd < 0)
		goto invalid;
	flags = fcntl(transfer.fd, F_GETFL);
	if (flags < 0 || fcntl(transfer.fd, F_SETFL, flags | O_NONBLOCK) < 0 ||
	    fcntl(transfer.fd, F_SETFD, FD_CLOEXEC) < 0)
		goto invalid;
	address.sun_family = AF_UNIX;
	strcpy(address.sun_path, transfer.path);
#if defined(__FreeBSD__) || defined(__OpenBSD__)
	address.sun_len = sizeof(address);
#endif
#ifdef SO_NOSIGPIPE
	{
		int enabled = 1;

		if (setsockopt(transfer.fd, SOL_SOCKET, SO_NOSIGPIPE,
			&enabled, sizeof(enabled)) < 0)
			goto invalid;
	}
#endif
	result = connect(transfer.fd, (struct sockaddr *)&address,
		sizeof(address));
	if (result < 0 && errno != EINPROGRESS)
		goto invalid;
	transfer.connecting = result < 0;
	transfer.read_event = event_new(base, transfer.fd, EV_READ | EV_PERSIST,
		transfer_read, NULL);
	transfer.write_event = event_new(base, transfer.fd,
		EV_WRITE | EV_PERSIST, transfer_write, NULL);
	transfer.deadline = evtimer_new(base, transfer_timeout, NULL);
	if (transfer.read_event == NULL || transfer.write_event == NULL ||
	    transfer.deadline == NULL ||
	    event_add(transfer.read_event, NULL) < 0 ||
	    evtimer_add(transfer.deadline, &timeout) < 0)
		goto invalid;
	transfer.aware = XInternAtom(dpy, "XdndAware", False);
	for (size_t i = 0; i < sizeof(names) / sizeof(names[0]); i++)
		transfer.xdnd[i] = XInternAtom(dpy, names[i], False);
	transfer.copy_cursor = XCreateFontCursor(dpy, XC_plus);
	transfer.move_cursor = XCreateFontCursor(dpy, XC_fleur);
	transfer.reject_cursor = XCreateFontCursor(dpy, XC_X_cursor);
	transfer.generation = transfer.list->list.state->generation;
	/* A fast release can reach the server before our explicit grab runs.
	 * The implicit press grab must already select it to retire pressed. */
	XSelectInput(dpy, transfer.list->header.win,
		ButtonPressMask | ButtonReleaseMask | Button1MotionMask |
		ExposureMask | StructureNotifyMask |
		EnterWindowMask | LeaveWindowMask);
	record = transfer_record(FILE_TRANSFER_ATTACH);
	transfer_queue(&record);
	return;
invalid:
	transfer_disconnect("attachment unavailable");
}

void form_transfer_cleanup(void)
{
	if (transfer.fd >= 0)
	{
		struct file_transfer_record record;

		transfer_cancel(CurrentTime);
		record = transfer_record(FILE_TRANSFER_DETACH);
		if (transfer_queue(&record))
			transfer_write(transfer.fd, EV_WRITE, NULL);
	}
	transfer_disconnect(NULL);
	if (transfer.read_event != NULL)
		event_free(transfer.read_event);
	if (transfer.write_event != NULL)
		event_free(transfer.write_event);
	if (transfer.deadline != NULL)
		event_free(transfer.deadline);
	transfer.read_event = transfer.write_event = transfer.deadline = NULL;
	if (transfer.copy_cursor != None)
		XFreeCursor(dpy, transfer.copy_cursor);
	if (transfer.move_cursor != None)
		XFreeCursor(dpy, transfer.move_cursor);
	if (transfer.reject_cursor != None)
		XFreeCursor(dpy, transfer.reject_cursor);
	transfer.copy_cursor = transfer.move_cursor = None;
	transfer.reject_cursor = None;
	transfer.list = NULL;
}

int form_transfer_fd(void)
{
	return transfer.fd;
}

short form_transfer_poll_events(void)
{
	return POLLIN | (transfer.queued || transfer.connecting ? POLLOUT : 0);
}

bool form_transfer_waiting(void)
{
	return transfer.fd >= 0 && !transfer.ready;
}

void form_transfer_reset(void)
{
	if (transfer.fd >= 0)
		transfer_cancel(CurrentTime);
}

void form_transfer_generation(void)
{
	struct file_transfer_record record;
	struct timeval timeout = {5, 0};

	if (transfer.fd >= 0 && transfer.failed)
	{
		transfer_disconnect("window lost; cancelled");
		return;
	}
	if (transfer.fd < 0 || transfer.list == NULL ||
	    transfer.generation ==
		(uint32_t)transfer.list->list.state->generation)
		return;
	transfer_cancel(CurrentTime);
	transfer_awareness(false);
	transfer.generation = transfer.list->list.state->generation;
	record = transfer_record(FILE_TRANSFER_GENERATION);
	if (transfer_queue(&record))
		evtimer_add(transfer.deadline, &timeout);
}

static void transfer_selection(struct file_transfer_record *record)
{
	form_list_state *state = transfer.list->list.state;

	record->rows = state->count;
	record->selection_bytes = (state->count + 7) / 8;
	memcpy(record->selection, state->bits, record->selection_bytes);
}

static int transfer_hit(const XButtonEvent *event)
{
	Item *list = transfer.list;
	form_list_state *state = list->list.state;
	int columns = form_list_columns(state);
	int width = (list->header.size_x - 2 * BOX_SPC - LIST_GUTTER) / columns;
	int column, row, index;

	if (width <= 0 || event->x < BOX_SPC || event->y < BOX_SPC ||
	    event->x >= list->header.size_x - BOX_SPC - LIST_GUTTER)
		return -1;
	column = (event->x - BOX_SPC) / width;
	row = (event->y - BOX_SPC) / list->list.row_height;
	index = state->top + row * columns + column;
	return column < columns && row < state->rows && index < state->count ?
		index : -1;
}

bool form_transfer_event(XEvent *event)
{
	struct file_transfer_record record;
	const XClientMessageEvent *message = &event->xclient;
	Window list_window;

	if (transfer.list == NULL)
		return false;
	list_window = (Window)(unsigned int)transfer.list->header.win;
	/* Idle list releases, including those queued before cancellation,
	 * are never Form button activations. */
	if (event->type == ButtonRelease &&
	    event->xbutton.window == list_window &&
	    !transfer.pressed)
		return true;
	if (transfer.fd < 0)
		return false;
	form_transfer_generation();
	if (transfer.fd < 0)
		return event->type == ButtonRelease ||
			event->type == MotionNotify;
	if ((event->type == DestroyNotify || event->type == UnmapNotify) &&
	    (event->xany.window == CF.frame ||
	    event->xany.window == list_window))
	{
		transfer.destroyed = event->type == DestroyNotify;
		transfer.frame_destroyed |= transfer.destroyed &&
			event->xany.window == CF.frame;
		transfer_cancel(CurrentTime);
		transfer_disconnect("window retired; cancelled");
		return false;
	}
	if (event->type == ClientMessage && event->xclient.window == CF.frame &&
	    event->xclient.format == 32 && transfer.ready)
	{
		for (size_t i = 0; i < 6; i++)
			if (event->xclient.message_type == transfer.xdnd[i])
			{
				record = transfer_record(
					FILE_TRANSFER_CLIENT_MESSAGE);
				record.detail =
					(uint32_t)event->xclient.message_type;
				record.format = 32;
				for (int word = 0; word < 5; word++)
					record.data[word] =
						(uint32_t)message->data.l[word];
				transfer_queue(&record);
				return true;
			}
	}
	if (event->type == KeyPress && event->xkey.window == CF.frame)
	{
		KeySym key = XLookupKeysym(&event->xkey, 0);

		if (key == XK_Escape && transfer.pressed)
		{
			transfer_cancel(event->xkey.time);
			return true;
		}
		if (transfer.pressed)
			return true;
		if (transfer.ready && CF.cur_list == transfer.list &&
		    !event->xkey.send_event &&
		    (event->xkey.state & ControlMask) &&
		    !(event->xkey.state & (Mod1Mask | Mod4Mask)) &&
		    (key == XK_c || key == XK_x || key == XK_v))
		{
			if (key != XK_v &&
			    transfer.list->list.state->selected == 0)
				return true;
			record = transfer_record(FILE_TRANSFER_ACTION);
			record.timestamp = (uint32_t)event->xkey.time;
			record.detail = FILE_TRANSFER_ACTION_PASTE;
			if (key == XK_c)
				record.detail = FILE_TRANSFER_ACTION_COPY;
			else if (key == XK_x)
				record.detail = FILE_TRANSFER_ACTION_CUT;
			if (key != XK_v)
				transfer_selection(&record);
			transfer_queue(&record);
			return true;
		}
	}
	if (transfer.pressed && event->xany.send_event &&
	    (event->type == ButtonRelease || event->type == MotionNotify))
		return true;
	if (transfer.pressed &&
	    ((event->type == LeaveNotify &&
	      event->xcrossing.mode == NotifyUngrab) ||
	     (event->type == MotionNotify &&
	      !(event->xmotion.state & Button1Mask))))
	{
		transfer_cancel(CurrentTime);
		return true;
	}
	if (event->type == ButtonPress &&
	    event->xbutton.window == list_window &&
	    event->xbutton.button == Button1 && transfer.ready &&
	    !event->xbutton.send_event)
	{
		int index = transfer_hit(&event->xbutton);

		if (index < 0)
			return false;
		if (XGrabPointer(dpy, list_window, False,
			ButtonReleaseMask | PointerMotionMask, GrabModeAsync,
			GrabModeAsync, None, CF.pointer[input_pointer],
			event->xbutton.time) != GrabSuccess)
		{
			fprintf(stderr,
				"FvwmForm: file transfer grab refused\n");
			return true;
		}
		transfer.pressed = true;
		transfer.dragging = false;
		transfer.press = event->xbutton;
		transfer.index = index;
		transfer.threshold = form_scale_drag_threshold();
		return true;
	}
	if (transfer.pressed && event->type == MotionNotify)
	{
		int64_t dx = (int64_t)event->xmotion.x_root -
			transfer.press.x_root;
		int64_t dy = (int64_t)event->xmotion.y_root -
			transfer.press.y_root;

		if (!transfer.dragging && (dx >= transfer.threshold ||
		    dx <= -transfer.threshold || dy >= transfer.threshold ||
		    dy <= -transfer.threshold))
		{
			if (!form_list_drag_select(transfer.list,
				transfer.index))
			{
				transfer_cancel(event->xmotion.time);
				return true;
			}
			transfer.snapshot =
				transfer_record(FILE_TRANSFER_BEGIN);
			transfer_selection(&transfer.snapshot);
			transfer.snapshot.timestamp =
				(uint32_t)transfer.press.time;
			transfer.snapshot.root_x = transfer.press.x_root;
			transfer.snapshot.root_y = transfer.press.y_root;
			transfer.snapshot.state = transfer.press.state;
			if (!transfer_queue(&transfer.snapshot))
				return true;
			transfer.dragging = true;
		}
		if (transfer.dragging)
		{
			record = transfer_record(FILE_TRANSFER_MOTION);
			record.timestamp = (uint32_t)event->xmotion.time;
			record.root_x = event->xmotion.x_root;
			record.root_y = event->xmotion.y_root;
			record.state = event->xmotion.state;
			transfer_queue(&record);
		}
		return true;
	}
	if (transfer.pressed && event->type == ButtonRelease &&
	    event->xbutton.button == Button1)
	{
		if (transfer.dragging)
		{
			record = transfer_record(FILE_TRANSFER_RELEASE);
			record.timestamp = (uint32_t)event->xbutton.time;
			record.root_x = event->xbutton.x_root;
			record.root_y = event->xbutton.y_root;
			record.state = event->xbutton.state;
			transfer_queue(&record);
		}
		else if (transfer_hit(&event->xbutton) == transfer.index)
			form_list_click(transfer.list, &transfer.press);
		transfer_ungrab();
		transfer_feedback(FILE_TRANSFER_IDLE);
		return true;
	}
	return false;
}

bool form_transfer_xerror(const XErrorEvent *error)
{
	if (transfer.fd < 0 || transfer.list == NULL ||
	    error->error_code != BadWindow ||
	    (error->resourceid != CF.frame &&
	    error->resourceid !=
		(Window)(unsigned int)transfer.list->header.win))
		return false;
	/* The X error callback only marks retirement. The ordinary owner loop
	 * retires awareness, input and the peer on its next turn. */
	transfer.failed = transfer.destroyed = true;
	transfer.frame_destroyed |= error->resourceid == CF.frame;
	return true;
}

void form_transfer_border(Item *item)
{
	DrawTable *dt;
	int stroke;

	if (item != transfer.list || transfer.feedback == FILE_TRANSFER_IDLE ||
	    transfer.feedback == FILE_TRANSFER_PENDING ||
	    transfer.feedback == FILE_TRANSFER_DONE)
		return;
	dt = item->header.dt_ptr;
	stroke = CF.ui_metrics.stroke ? CF.ui_metrics.stroke : 1;
	/* Distinct line styles retain feedback in monochrome themes. */
	XSetForeground(dpy, dt->dt_item_GC, dt->dt_colors[c_item_fg]);
	XSetLineAttributes(dpy, dt->dt_item_GC, stroke,
		transfer.feedback == FILE_TRANSFER_REJECTED ? LineOnOffDash :
		LineSolid, CapButt, JoinMiter);
	for (int inset = stroke; inset <= stroke *
	    (transfer.feedback == FILE_TRANSFER_MOVE ? 2 : 1); inset += stroke)
		if (item->header.size_x > 2 * inset &&
		    item->header.size_y > 2 * inset)
			XDrawRectangle(dpy, item->header.win, dt->dt_item_GC,
				inset, inset,
				item->header.size_x - 1 - 2 * inset,
				item->header.size_y - 1 - 2 * inset);
	XSetLineAttributes(dpy, dt->dt_item_GC, 0,
		LineSolid, CapButt, JoinMiter);
}
