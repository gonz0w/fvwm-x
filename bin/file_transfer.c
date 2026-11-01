/* This program is free software; you can redistribute it and/or modify it
 * under the GNU General Public License, version 2 or any later version. */
#include "config.h"
#include "file_transfer_policy.h"
#include "libs/FProperty.h"
#include "libs/cJSON.h"
#include "libs/fd_safety.h"
#include "libs/safemalloc.h"
#include <X11/Xatom.h>
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <poll.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <time.h>
#include <unistd.h>

#define FT_OUTPUT_MAX (1024U * 1024U)
#define FT_RETAINED_MAX (2U * 1024U * 1024U)
#define FT_CHUNK 16384U
#define FT_PEERS 2
#define FT_QUEUE 8

struct ft_bytes
{
	unsigned int references;
	size_t length;
	char data[];
};
struct ft_output
{
	char *data;
	size_t length, sent;
};
struct ft_peer
{
	int fd;
	size_t used, output_count, sent;
	unsigned char input[FILE_TRANSFER_WIRE_BYTES];
	unsigned char output[FT_QUEUE][FILE_TRANSFER_WIRE_BYTES];
	struct file_transfer_record identity;
	bool attached;
	int64_t input_touched, output_touched;
};
struct ft_outgoing
{
	Window window;
	Atom property, target, selection;
	struct ft_bytes *bytes;
	size_t offset;
	int64_t began, touched;
};
struct ft_incoming
{
	Window window, owner;
	Atom selection, target;
	Time time;
	uint64_t transfer, command;
	uint64_t epoch;
	uint32_t generation;
	bool active, incr, targets, move, dnd;
	char *bytes;
	size_t used;
	int64_t began, touched;
};
struct ft_drag
{
	bool active, ready, waiting, accepted, released, dropped, move, dirty;
	bool requested_move;
	Window frame, target;
	unsigned int version;
	Time time;
	int x, y;
	unsigned int state;
	uint64_t transfer, epoch;
	uint32_t generation;
	int64_t began, touched;
};
struct ft_drop
{
	bool active, accepted, offered, proposed, admitted, move, dropped;
	Window source, target, selection_owner;
	unsigned int version;
	uint64_t transfer, epoch;
	uint32_t generation;
	Time time;
	int64_t deadline, touched;
};
struct ft_runtime
{
	Display *display;
	Window owner, root;
	Atom clipboard, xdnd_selection, uri, gnome, targets, timestamp;
	Atom multiple, atom_pair, incr, property, clock_property, username;
	Atom aware, enter, position, status, leave, drop, finished;
	Atom action_copy, action_move, type_list, action_list;
	int listener, active_peer;
	const char *socket_path;
	unsigned char nonce[16];
	char hostname[256], user[256];
	uint64_t expected_epoch, context_epoch, next_id, lease;
	uint32_t generation;
	Time clipboard_time, drag_time;
	bool enabled, stopping, failed, clipboard_move, peer_overflow;
	struct ft_bytes *clipboard_bytes, *drag_bytes;
	size_t retained;
	struct ft_peer peers[FT_PEERS];
	struct ft_outgoing outgoing[4];
	struct ft_incoming incoming;
	struct ft_drag drag;
	struct ft_drop incoming_drop;
	struct ft_output output[FT_QUEUE];
	size_t output_count, output_bytes, input_used;
	char input[FILE_TRANSFER_FRAME_BYTES + 1];
	struct { unsigned long serial; Window window; } expected[256];
	size_t expected_count;
	bool probe_active, probe_lost;
	Window probe_window;
	unsigned long probe_first;
	Window lost_windows[8];
	size_t lost_count;
	struct ft_command *timed_command;
	int64_t clock_deadline;
	uint64_t timed_epoch;
	uint32_t timed_generation;
};

static int64_t
now_ms(void)
{
	struct timespec t;
	if (clock_gettime(CLOCK_MONOTONIC, &t))
		return -1;
	return (int64_t)t.tv_sec * 1000 + t.tv_nsec / 1000000;
}

static bool
nonblocking(int fd)
{
	int flags = fcntl(fd, F_GETFL);
	return flags >= 0 && fcntl(fd, F_SETFL, flags | O_NONBLOCK) == 0 &&
		fvwm_fd_set_cloexec(fd, true);
}

/* Xlib supplies no context argument to its error callback. Only the one
 * owning thread accesses this pointer; expected errors match exact requests. */
static struct ft_runtime *x_runtime;

static void
expect_window(struct ft_runtime *r, Window window)
{
	if (r->expected_count == 256)
	{
		XSync(r->display, False);
		r->expected_count = 0;
	}
	r->expected[r->expected_count].serial = NextRequest(r->display);
	r->expected[r->expected_count++].window = window;
}

static int
x_error(Display *display, XErrorEvent *event)
{
	struct ft_runtime *r = x_runtime;
	if (r && display == r->display && event->error_code == BadWindow &&
	    r->probe_active && event->resourceid == r->probe_window &&
	    event->serial >= r->probe_first)
	{
		r->probe_lost = true;
		return 0;
	}
	if (r && display == r->display && event->error_code == BadWindow)
		for (size_t i = 0; i < r->expected_count; i++)
			if (r->expected[i].serial == event->serial &&
			    r->expected[i].window == event->resourceid)
			{
				if (r->lost_count < 8)
					r->lost_windows[r->lost_count++] = event->resourceid;
				else
					r->failed = r->stopping = true;
				return 0;
			}
	fputs("fvwm-file-transfer: unexpected X error\n", stderr);
	if (r)
		r->failed = r->stopping = true;
	return 0;
}

static void
change_property(struct ft_runtime *r, Window window, Atom prop,
	Atom type, int format, int mode, const unsigned char *data, int count)
{
	expect_window(r, window);
	XChangeProperty(r->display, window, prop, type, format, mode, data, count);
}

static void
select_input(struct ft_runtime *r, Window window, long mask)
{
	expect_window(r, window);
	XSelectInput(r->display, window, mask);
}

static bool
window_attributes(struct ft_runtime *r, Window window,
	XWindowAttributes *attributes)
{
	/* XGetWindowAttributes can issue more than one X request. Fence the
	 * operation so only this exact foreign window/request interval can fail. */
	XSync(r->display, False);
	r->expected_count = 0;
	r->probe_active = true;
	r->probe_lost = false;
	r->probe_window = window;
	r->probe_first = NextRequest(r->display);
	Status status = XGetWindowAttributes(r->display, window, attributes);
	XSync(r->display, False);
	r->probe_active = false;
	return status && !r->probe_lost && !r->failed;
}

static void
refresh_watch(struct ft_runtime *r, Window window)
{
	long mask = 0;
	if (!window || window == r->owner || window == r->root)
		return;
	for (size_t i = 0; i < FT_PEERS; i++)
		if (r->peers[i].attached &&
		    (r->peers[i].identity.frame == window ||
		    r->peers[i].identity.list == window))
			mask |= StructureNotifyMask;
	if ((r->drag.active &&
	    (r->drag.target == window || r->drag.frame == window)) ||
	    (r->incoming.active && r->incoming.owner == window) ||
	    (r->incoming_drop.active && (r->incoming_drop.source == window ||
	    r->incoming_drop.target == window)))
		mask |= StructureNotifyMask;
	for (size_t i = 0; i < 4; i++)
		if (r->outgoing[i].bytes && r->outgoing[i].window == window)
			mask |= StructureNotifyMask | PropertyChangeMask;
	select_input(r, window, mask);
}

static int
x_io_error(Display *display)
{
	(void)display;
	fputs("fvwm-file-transfer: X connection failed\n", stderr);
	_exit(1);
}

static void
bytes_release(struct ft_runtime *r, struct ft_bytes **slot)
{
	struct ft_bytes *b = *slot;
	*slot = NULL;
	if (b && --b->references == 0)
	{
		r->retained -= b->length;
		free(b);
	}
}

static struct ft_bytes *
bytes_new(struct ft_runtime *r, const char *s)
{
	size_t length = strlen(s);
	struct ft_bytes *b;
	if (length > FILE_TRANSFER_MAX_URI_BYTES + 6 ||
	    r->retained + r->output_bytes + r->input_used > FT_RETAINED_MAX ||
	    length > FT_RETAINED_MAX - r->retained - r->output_bytes -
	    r->input_used)
		return NULL;
	b = fxmalloc(sizeof(*b) + length + 1);
	b->references = 1;
	b->length = length;
	memcpy(b->data, s, length + 1);
	r->retained += length;
	return b;
}

static cJSON *
object(const char *event)
{
	cJSON *o = cJSON_CreateObject();
	if (!o)
		return NULL;
	cJSON_AddNumberToObject(o, "v", 1);
	if (event)
		cJSON_AddStringToObject(o, "event", event);
	return o;
}

static void
add_id(cJSON *o, const char *key, uint64_t id)
{
	char value[32];
	snprintf(value, sizeof(value), "%" PRIu64, id);
	cJSON_AddStringToObject(o, key, value);
}

static bool
output(struct ft_runtime *r, cJSON *o)
{
	char *data;
	size_t length;
	if (!o)
		goto failed;
	data = cJSON_PrintUnformatted(o);
	cJSON_Delete(o);
	if (!data)
		goto failed;
	length = strlen(data);
	if (length >= FILE_TRANSFER_FRAME_BYTES ||
	    r->output_count == FT_QUEUE ||
	    length + 1 > FT_OUTPUT_MAX - r->output_bytes ||
	    r->retained + r->output_bytes + r->input_used > FT_RETAINED_MAX ||
	    length + 1 > FT_RETAINED_MAX - r->retained - r->output_bytes -
	    r->input_used)
	{
		free(data);
		goto failed;
	}
	data = fxrealloc(data, length + 2, 1);
	data[length++] = '\n';
	data[length] = 0;
	r->output[r->output_count++] = (struct ft_output){data, length, 0};
	r->output_bytes += length;
	return true;
failed:
	r->failed = r->stopping = true;
	return false;
}

static bool
reply(struct ft_runtime *r, uint64_t id, bool ok,
	const char *code, uint64_t lease)
{
	cJSON *o = object(NULL);
	add_id(o, "id", id);
	cJSON_AddStringToObject(o, "status", ok ? "ok" : "error");
	cJSON_AddStringToObject(o, "code", code);
	if (lease)
		add_id(o, "lease", lease);
	return output(r, o);
}

static void
unavailable(struct ft_runtime *r, const char *code)
{
	cJSON *o = object("unavailable");
	cJSON_AddStringToObject(o, "code", code);
	output(r, o);
}

static void
result(struct ft_runtime *r, uint64_t transfer,
	const char *status, bool move)
{
	cJSON *o = object("result");
	add_id(o, "transfer", transfer);
	cJSON_AddStringToObject(o, "status", status);
	cJSON_AddStringToObject(o, "action", move ? "move" : "copy");
	output(r, o);
}

static void
send_message(struct ft_runtime *r, Window to, Atom type,
	Window actual, long a, long b, long c, long d, long e)
{
	XEvent event = {0};
	event.xclient.type = ClientMessage;
	event.xclient.display = r->display;
	event.xclient.window = actual;
	event.xclient.message_type = type;
	event.xclient.format = 32;
	event.xclient.data.l[0] = a;
	event.xclient.data.l[1] = b;
	event.xclient.data.l[2] = c;
	event.xclient.data.l[3] = d;
	event.xclient.data.l[4] = e;
	expect_window(r, to);
	XSendEvent(r->display, to, False, NoEventMask, &event);
}

static bool
peer_feedback(struct ft_runtime *r, enum file_transfer_feedback f)
{
	if (r->active_peer < 0)
		return false;
	struct ft_peer *p = &r->peers[r->active_peer];
	struct file_transfer_record record = p->identity;
	record.kind = FILE_TRANSFER_FEEDBACK;
	record.detail = f;
	if (p->output_count == FT_QUEUE)
	{
		r->peer_overflow = true;
		return false;
	}
	if (!file_transfer_encode(&record, p->output[p->output_count]))
		return false;
	/* Idle attachments get a fresh deadline when feedback first queues.
	 * Appending feedback cannot extend a stalled writer's deadline. */
	if (!p->output_count)
		p->output_touched = now_ms();
	p->output_count++;
	return true;
}

static bool
context_current(struct ft_runtime *r, uint64_t epoch,
	uint32_t generation)
{
	return r->enabled && r->active_peer >= 0 &&
		r->context_epoch == epoch && r->expected_epoch == epoch &&
		r->generation == generation &&
		r->peers[r->active_peer].identity.epoch == epoch &&
		r->peers[r->active_peer].identity.generation == generation;
}

static void
source_cancel(struct ft_runtime *r, const char *reason)
{
	if (!r->drag.active)
		return;
	if (r->drag.target && !r->drag.dropped)
		send_message(r, r->drag.target, r->leave, r->drag.target,
			r->drag.frame, 0, 0, 0, 0);
	Window frame = r->drag.frame, target = r->drag.target;
	result(r, r->drag.transfer, reason, r->drag.move);
	memset(&r->drag, 0, sizeof(r->drag));
	bytes_release(r, &r->drag_bytes);
	if (XGetSelectionOwner(r->display, r->xdnd_selection) == r->owner)
		XSetSelectionOwner(r->display, r->xdnd_selection, None, r->drag_time);
	refresh_watch(r, frame);
	refresh_watch(r, target);
	peer_feedback(r, FILE_TRANSFER_IDLE);
}

static void
incoming_reset(struct ft_runtime *r)
{
	Window owner = r->incoming.owner;
	if (r->incoming.window)
	{
		expect_window(r, r->incoming.window);
		XDestroyWindow(r->display, r->incoming.window);
	}
	if (r->incoming.bytes)
		r->retained -= FILE_TRANSFER_MAX_URI_BYTES + 7;
	free(r->incoming.bytes);
	memset(&r->incoming, 0, sizeof(r->incoming));
	refresh_watch(r, owner);
}

static void
target_finish(struct ft_runtime *r, bool success,
	const char *reason)
{
	struct ft_drop *d = &r->incoming_drop;
	Window source = d->source, target = d->target;
	if (!d->active)
		return;
	if (d->source && d->dropped)
	{
		send_message(r, d->source, r->finished, d->source,
			d->target, success ? 1 : 0,
			success ? (d->move ? r->action_move : r->action_copy) : None,
			0, 0);
		if (d->transfer)
			result(r, d->transfer, reason, d->move);
	}
	if (!d->source && d->transfer)
		result(r, d->transfer, reason, d->move);
	memset(d, 0, sizeof(*d));
	refresh_watch(r, source);
	refresh_watch(r, target);
	if (r->incoming.active && r->incoming.dnd)
		incoming_reset(r);
	peer_feedback(r, FILE_TRANSFER_IDLE);
}

static void
outgoing_reset(struct ft_runtime *r, struct ft_outgoing *o)
{
	Window window = o->window;
	bytes_release(r, &o->bytes);
	memset(o, 0, sizeof(*o));
	refresh_watch(r, window);
}

static bool
property(struct ft_runtime *r, Window window, Atom name,
	Atom type, int format, size_t minimum, size_t maximum,
	unsigned char **data, size_t *length)
{
	FPropertyLimits limits = {minimum, maximum, format};
	expect_window(r, window);
	return FPropertyRead(r->display, window, name, type, limits,
		data, length) == FPROPERTY_OK;
}

static void
selection_notify(struct ft_runtime *r,
	const XSelectionRequestEvent *request, Atom prop)
{
	XEvent e = {0};
	e.xselection.type = SelectionNotify;
	e.xselection.display = r->display;
	e.xselection.requestor = request->requestor;
	e.xselection.selection = request->selection;
	e.xselection.target = request->target;
	e.xselection.time = request->time;
	e.xselection.property = prop;
	expect_window(r, request->requestor);
	XSendEvent(r->display, request->requestor, False, NoEventMask, &e);
}

static bool
serve_target(struct ft_runtime *r,
	const XSelectionRequestEvent *q, Atom target, Atom prop)
{
	struct ft_bytes *bytes = q->selection == r->clipboard ?
		r->clipboard_bytes : r->drag_bytes;
	Time acquired = q->selection == r->clipboard ?
		r->clipboard_time : r->drag_time;
	if (!bytes || !prop || (q->time && !ft_time_valid(q->time, acquired)))
		return false;
	if (target == r->targets)
	{
		unsigned long atoms[] = {r->targets, r->timestamp, r->multiple,
			r->uri, q->selection == r->clipboard ? r->gnome : r->username};
		change_property(r, q->requestor, prop, XA_ATOM, 32,
			PropModeReplace, (unsigned char *)atoms, 5);
		return true;
	}
	if (target == r->timestamp)
	{
		unsigned long time = acquired;
		change_property(r, q->requestor, prop, XA_INTEGER, 32,
			PropModeReplace, (unsigned char *)&time, 1);
		return true;
	}
	if (target == r->username && q->selection == r->xdnd_selection)
	{
		change_property(r, q->requestor, prop, target, 8,
			PropModeReplace, (unsigned char *)r->user, strlen(r->user));
		return true;
	}
	if (target != r->uri &&
	    !(target == r->gnome && q->selection == r->clipboard))
		return false;
	struct ft_bytes *payload = bytes;
	if (target == r->gnome)
	{
		const char *prefix = r->clipboard_move ? "cut\n" : "copy\n";
		char *joined = fxmalloc(bytes->length + 6);
		strcpy(joined, prefix);
		size_t used = strlen(joined);
		for (size_t i = 0; i < bytes->length; i++)
			if (bytes->data[i] != '\r')
				joined[used++] = bytes->data[i];
		joined[used] = 0;
		payload = bytes_new(r, joined);
		free(joined);
		if (!payload)
			return false;
	}
	else
		payload->references++;
	if (payload->length <= FT_CHUNK)
	{
		change_property(r, q->requestor, prop, target, 8,
			PropModeReplace, (unsigned char *)payload->data, payload->length);
		bytes_release(r, &payload);
		return true;
	}
	struct ft_outgoing *slot = NULL;
	for (size_t i = 0; i < 4; i++)
	{
		if (r->outgoing[i].window == q->requestor &&
		    r->outgoing[i].property == prop)
		{
			bytes_release(r, &payload);
			return false;
		}
		if (!r->outgoing[i].bytes)
			slot = &r->outgoing[i];
	}
	if (!slot)
	{
		bytes_release(r, &payload);
		return false;
	}
	*slot = (struct ft_outgoing){q->requestor, prop, target, q->selection,
		payload, 0, now_ms(), now_ms()};
	refresh_watch(r, q->requestor);
	unsigned long size = payload->length;
	change_property(r, q->requestor, prop, r->incr, 32,
		PropModeReplace, (unsigned char *)&size, 1);
	return true;
}

static void
selection_request(struct ft_runtime *r,
	const XSelectionRequestEvent *q)
{
	Atom prop = q->property ? q->property : q->target;
	bool ok = false;
	if (q->owner != r->owner ||
	    (q->selection != r->clipboard && q->selection != r->xdnd_selection))
		goto done;
	if (q->target == r->multiple)
	{
		unsigned char *data = NULL;
		size_t length = 0;
		if (!q->property || !property(r, q->requestor, prop, r->atom_pair,
		    32, 0, 8 * 8, &data, &length) || length % 8)
		{
			free(data);
			goto done;
		}
		uint32_t *pairs = (uint32_t *)data;
		unsigned long result_pairs[16];
		for (size_t i = 0; i < length / 4; i += 2)
		{
			bool unique = pairs[i + 1] && pairs[i + 1] != prop;
			for (size_t j = 0; j < i; j += 2)
				if (pairs[j + 1] == pairs[i + 1])
					unique = false;
			result_pairs[i] = pairs[i];
			result_pairs[i + 1] = unique && pairs[i] != r->multiple &&
				serve_target(r, q, pairs[i], pairs[i + 1]) ?
				pairs[i + 1] : None;
		}
		change_property(r, q->requestor, prop, r->atom_pair, 32,
			PropModeReplace, (unsigned char *)result_pairs, length / 4);
		free(data);
		ok = true;
	}
	else
		ok = serve_target(r, q, q->target, prop);
done:
	selection_notify(r, q, ok ? prop : None);
}

static void
proposal(struct ft_runtime *r)
{
	struct ft_incoming *in = &r->incoming;
	const char *uris = in->bytes ? in->bytes : "";
	if (in->target == r->gnome)
	{
		if (!strncmp(uris, "cut\n", 4))
			in->move = true, uris += 4;
		else if (!strncmp(uris, "copy\n", 5))
			in->move = false, uris += 5;
		else
			goto invalid;
	}
	if (!ft_uris_valid(uris, r->hostname) ||
	    !context_current(r, in->epoch, in->generation))
		goto invalid;
	if (!in->dnd)
	{
		if (r->incoming_drop.active)
			goto invalid;
		r->incoming_drop = (struct ft_drop){
			.active = true, .accepted = true, .move = in->move,
			.transfer = in->transfer, .epoch = in->epoch,
			.generation = in->generation};
	}
	struct ft_drop *d = &r->incoming_drop;
	d->proposed = true;
	d->deadline = now_ms() + 30000;
	cJSON *o = object("proposal");
	if (in->command)
		add_id(o, "id", in->command);
	add_id(o, "transfer", in->transfer);
	add_id(o, "epoch", in->epoch);
	cJSON_AddNumberToObject(o, "generation", in->generation);
	cJSON_AddStringToObject(o, "kind", in->dnd ? "dnd" : "clipboard");
	cJSON_AddStringToObject(o, "action", in->move ? "move" : "copy");
	cJSON_AddStringToObject(o, "uris", uris);
	if (!in->dnd && in->owner == r->owner && r->lease)
		add_id(o, "lease", r->lease);
	output(r, o);
	incoming_reset(r);
	return;
invalid:
	if (in->dnd)
		target_finish(r, false, "failed");
	else
		result(r, in->transfer, "failed", in->move);
	incoming_reset(r);
}

static bool
incoming_begin(struct ft_runtime *r, bool dnd,
	Time time, uint64_t command)
{
	if (r->incoming.active || !time ||
	    !context_current(r, r->context_epoch, r->generation))
		return false;
	struct ft_incoming *in = &r->incoming;
	Atom selection = dnd ? r->xdnd_selection : r->clipboard;
	Window owner = XGetSelectionOwner(r->display, selection);
	if (!owner || r->retained + r->output_bytes + r->input_used >
	    FT_RETAINED_MAX || FILE_TRANSFER_MAX_URI_BYTES + 7 >
	    FT_RETAINED_MAX - r->retained - r->output_bytes - r->input_used)
		return false;
	in->window = XCreateSimpleWindow(r->display, r->root, 0, 0, 1, 1,
		0, 0, 0);
	if (!in->window)
		return false;
	select_input(r, in->window, PropertyChangeMask);
	in->bytes = fxcalloc(FILE_TRANSFER_MAX_URI_BYTES + 7, 1);
	r->retained += FILE_TRANSFER_MAX_URI_BYTES + 7;
	in->active = true;
	in->owner = owner;
	if (dnd)
		r->incoming_drop.selection_owner = owner;
	in->selection = selection;
	in->target = dnd ? r->uri : r->targets;
	in->targets = !dnd;
	in->time = time;
	in->command = command;
	in->transfer = dnd ? r->incoming_drop.transfer : ++r->next_id;
	in->dnd = dnd;
	in->move = dnd && r->incoming_drop.move;
	in->epoch = r->context_epoch;
	in->generation = r->generation;
	in->began = in->touched = now_ms();
	refresh_watch(r, owner);
	XConvertSelection(r->display, selection, in->target, r->property,
		in->window, time);
	return true;
}

static void
incoming_fail(struct ft_runtime *r, const char *reason)
{
	if (!r->incoming.active)
		return;
	if (r->incoming.dnd)
		target_finish(r, false, reason);
	else
		result(r, r->incoming.transfer, reason, r->incoming.move);
	incoming_reset(r);
}

static bool
incoming_targets(struct ft_runtime *r,
	const unsigned char *data, size_t length)
{
	struct ft_incoming *in = &r->incoming;
	bool uri = false, gnome = false;
	if (!length || length % 4 || length > 1024)
		return false;
	for (size_t i = 0; i < length / 4; i++)
	{
		uint32_t atom;
		memcpy(&atom, data + i * 4, 4);
		uri |= atom == r->uri;
		gnome |= atom == r->gnome;
	}
	if (!uri && !gnome)
		return false;
	in->targets = false;
	in->incr = false;
	in->used = 0;
	in->bytes[0] = 0;
	in->target = gnome ? r->gnome : r->uri;
	in->touched = now_ms();
	XDeleteProperty(r->display, in->window, r->property);
	XConvertSelection(r->display, in->selection, in->target,
		r->property, in->window, in->time);
	return true;
}

static void
incoming_notify(struct ft_runtime *r,
	const XSelectionEvent *event)
{
	struct ft_incoming *in = &r->incoming;
	unsigned char *data = NULL;
	size_t length = 0;
	if (!in->active || event->requestor != in->window ||
	    event->selection != in->selection || event->target != in->target ||
	    event->time != in->time)
		return;
	if (event->property != r->property ||
	    XGetSelectionOwner(r->display, in->selection) != in->owner)
		goto failed;
	if (property(r, in->window, r->property, r->incr, 32, 4, 4,
	    &data, &length))
	{
		uint32_t amount;
		memcpy(&amount, data, 4);
		free(data);
		data = NULL;
		if (amount > (in->targets ? 1024 : FILE_TRANSFER_MAX_URI_BYTES + 5))
			goto failed;
		in->incr = true;
		in->touched = now_ms();
		XDeleteProperty(r->display, in->window, r->property);
		return;
	}
	if (in->targets)
	{
		if (!property(r, in->window, r->property, XA_ATOM, 32,
		    4, 1024, &data, &length))
			goto failed;
		bool valid = incoming_targets(r, data, length);
		free(data);
		if (!valid)
			incoming_fail(r, "failed");
		return;
	}
	if (!property(r, in->window, r->property, in->target, 8, 1,
	    FILE_TRANSFER_MAX_URI_BYTES + 5, &data, &length) ||
	    memchr(data, 0, length))
		goto failed;
	memcpy(in->bytes, data, length);
	in->used = length;
	in->bytes[length] = 0;
	free(data);
	XDeleteProperty(r->display, in->window, r->property);
	proposal(r);
	return;
failed:
	free(data);
	incoming_fail(r, "failed");
}

static void
property_event(struct ft_runtime *r, const XPropertyEvent *event)
{
	for (size_t i = 0; i < 4; i++)
	{
		struct ft_outgoing *o = &r->outgoing[i];
		if (!o->bytes || event->window != o->window ||
		    event->atom != o->property || event->state != PropertyDelete)
			continue;
		size_t remaining = o->bytes->length - o->offset;
		size_t amount = remaining > FT_CHUNK ? FT_CHUNK : remaining;
		change_property(r, o->window, o->property, o->target, 8,
			PropModeReplace,
			(unsigned char *)o->bytes->data + o->offset, amount);
		o->offset += amount;
		o->touched = now_ms();
		if (!amount)
			outgoing_reset(r, o);
	}
	struct ft_incoming *in = &r->incoming;
	if (!in->active || !in->incr || event->window != in->window ||
	    event->atom != r->property || event->state != PropertyNewValue)
		return;
	unsigned char *data = NULL;
	size_t length = 0;
	if (XGetSelectionOwner(r->display, in->selection) != in->owner ||
	    !property(r, in->window, r->property,
	    in->targets ? XA_ATOM : in->target, in->targets ? 32 : 8, 0,
	    FT_CHUNK, &data, &length) ||
	    length > (in->targets ? 1024 : FILE_TRANSFER_MAX_URI_BYTES + 5) -
	    in->used || (!in->targets && memchr(data, 0, length)))
	{
		free(data);
		incoming_fail(r, "failed");
		return;
	}
	memcpy(in->bytes + in->used, data, length);
	in->used += length;
	in->bytes[in->used] = 0;
	in->touched = now_ms();
	free(data);
	XDeleteProperty(r->display, in->window, r->property);
	if (!length)
	{
		if (in->targets)
		{
			if (!incoming_targets(r, (unsigned char *)in->bytes, in->used))
				incoming_fail(r, "failed");
		}
		else
			proposal(r);
	}
}

static unsigned int
aware_version(struct ft_runtime *r, Window w)
{
	unsigned char *data = NULL;
	size_t length;
	unsigned int version = 0;
	if (property(r, w, r->aware, XA_ATOM, 32, 4, 4, &data, &length))
	{
		uint32_t value;
		memcpy(&value, data, 4);
		if (value >= 3)
			version = value > 5 ? 5 : value;
	}
	free(data);
	return version;
}

static Window
target_at(struct ft_runtime *r, int x, int y,
	unsigned int *version)
{
	Window parent = r->root, child = None, found = None;
	int dx, dy;
	*version = 0;
	/* Walk real descendants under root coordinates. A hostile/deep tree has
	 * a fixed request bound. XdndProxy is intentionally outside this profile. */
	for (size_t i = 0; i < 64; i++)
	{
		expect_window(r, parent);
		if (!XTranslateCoordinates(r->display, r->root, parent,
		    x, y, &dx, &dy, &child))
			break;
		unsigned int v = aware_version(r, parent);
		if (v)
			found = parent, *version = v;
		if (!child || child == parent)
			break;
		parent = child;
	}
	return found;
}

static void
source_position(struct ft_runtime *r)
{
	struct ft_drag *d = &r->drag;
	unsigned int version;
	Window target;
	if (!d->active || !d->ready || d->dropped || d->waiting)
		return;
	if (d->x < -32768 || d->x > 32767 ||
	    d->y < -32768 || d->y > 32767)
	{
		source_cancel(r, "failed");
		return;
	}
	target = target_at(r, d->x, d->y, &version);
	if (target != d->target)
	{
		Window old_target = d->target;
		if (d->target)
			send_message(r, d->target, r->leave, d->target,
				d->frame, 0, 0, 0, 0);
		d->target = target;
		refresh_watch(r, old_target);
		refresh_watch(r, target);
		d->accepted = false;
		d->version = version;
		if (target)
			send_message(r, target, r->enter, target, d->frame,
				(long)version << 24, r->uri, r->username, 0);
	}
	if (!target)
	{
		peer_feedback(r, FILE_TRANSFER_REJECTED);
		if (d->released)
			source_cancel(r, "failed");
		return;
	}
	uint32_t packed = (uint32_t)(uint16_t)d->x << 16 | (uint16_t)d->y;
	send_message(r, target, r->position, target, d->frame, 0,
		packed, d->time, d->requested_move ? r->action_move : r->action_copy);
	d->waiting = true;
	d->dirty = false;
	d->touched = now_ms();
}

static void
source_drop(struct ft_runtime *r)
{
	struct ft_drag *d = &r->drag;
	if (!d->active || !d->released || d->waiting)
		return;
	if (!d->accepted || !d->target)
	{
		source_cancel(r, "failed");
		return;
	}
	send_message(r, d->target, r->drop, d->target,
		d->frame, 0, d->time, 0, 0);
	d->dropped = true;
	d->touched = now_ms();
}

static void
target_status(struct ft_runtime *r, bool accepted)
{
	struct ft_drop *d = &r->incoming_drop;
	d->accepted = accepted;
	send_message(r, d->source, r->status, d->source,
		d->target, accepted ? 3 : 2, 0, 0,
		accepted ? (d->move ? r->action_move : r->action_copy) : None);
	peer_feedback(r, accepted ?
		(d->move ? FILE_TRANSFER_MOVE : FILE_TRANSFER_COPY) :
		FILE_TRANSFER_REJECTED);
}

static bool
list_contains(struct ft_runtime *r, uint32_t packed)
{
	if (r->active_peer < 0)
		return false;
	Window list = r->peers[r->active_peer].identity.list, child;
	XWindowAttributes attributes;
	int x, y;
	int root_x = (int16_t)(packed >> 16);
	int root_y = (int16_t)packed;

	if (!window_attributes(r, list, &attributes) ||
	    attributes.map_state != IsViewable)
		return false;
	expect_window(r, list);
	return XTranslateCoordinates(r->display, r->root, list,
		root_x, root_y, &x, &y, &child) &&
		x >= 0 && y >= 0 && x < attributes.width && y < attributes.height;
}

static void
client_message(struct ft_runtime *r,
	const struct file_transfer_record *record)
{
	const uint32_t *data = record->data;
	Atom type = record->detail;
	if (type == r->status && r->drag.active && r->drag.waiting &&
	    record->frame == r->drag.frame && data[0] == r->drag.target)
	{
		struct ft_drag *d = &r->drag;
		d->waiting = false;
		d->accepted = (data[1] & 1) &&
			(data[4] == r->action_copy || data[4] == r->action_move);
		if (d->accepted)
			d->move = data[4] == r->action_move;
		peer_feedback(r, d->accepted ?
			(d->move ? FILE_TRANSFER_MOVE : FILE_TRANSFER_COPY) :
			FILE_TRANSFER_REJECTED);
		if (d->dirty)
			source_position(r);
		else if (d->released)
			source_drop(r);
		return;
	}
	if (type == r->finished && r->drag.active && r->drag.dropped &&
	    record->frame == r->drag.frame && data[0] == r->drag.target)
	{
		bool success = r->drag.version < 5 ||
			((data[1] & 1) && data[2] ==
			(r->drag.move ? r->action_move : r->action_copy));
		source_cancel(r, success ? "success" : "failed");
		return;
	}
	if (type == r->enter)
	{
		if (r->incoming_drop.active || r->incoming.active ||
		    !context_current(r, record->epoch, record->generation))
			return;
		unsigned int version = data[1] >> 24;
		if (version < 3 || version > 5 || !data[0])
			return;
		bool offered = false;
		if (data[1] & 1)
		{
			unsigned char *types = NULL;
			size_t length;
			if (property(r, data[0], r->type_list, XA_ATOM, 32,
			    4, 256 * 4, &types, &length))
				for (size_t i = 0; i < length / 4; i++)
				{
					uint32_t atom;
					memcpy(&atom, types + i * 4, 4);
					offered |= atom == r->uri;
				}
			free(types);
		}
		else
			offered = data[2] == r->uri || data[3] == r->uri ||
				data[4] == r->uri;
		r->incoming_drop = (struct ft_drop){
			.active = true, .accepted = false, .offered = offered,
			.source = data[0], .target = record->frame,
			.version = version, .epoch = record->epoch,
			.generation = record->generation,
			.transfer = ++r->next_id, .deadline = now_ms() + 30000,
			.touched = now_ms()};
		refresh_watch(r, data[0]);
		return;
	}
	struct ft_drop *d = &r->incoming_drop;
	if (!d->active || !d->source || data[0] != d->source ||
	    record->frame != d->target || d->proposed)
		return;
	if (type == r->position)
	{
		d->touched = now_ms();
		bool action = data[4] == r->action_copy || data[4] == r->action_move;
		bool accepted = d->offered && action && list_contains(r, data[2]) &&
			ft_time_valid(data[3], d->time) &&
			context_current(r, d->epoch, d->generation);
		d->move = data[4] == r->action_move;
		if (accepted)
			d->time = data[3];
		target_status(r, accepted);
	}
	else if (type == r->leave)
	{
		/* No Finished response before a Drop. */
		Window source = d->source;
		memset(d, 0, sizeof(*d));
		refresh_watch(r, source);
		peer_feedback(r, FILE_TRANSFER_IDLE);
	}
	else if (type == r->drop)
	{
		d->dropped = true;
		if (!d->accepted || !ft_time_valid(data[2], d->time) ||
		    !context_current(r, d->epoch, d->generation))
		{
			d->time = data[2] ? data[2] : 1;
			target_finish(r, false, "failed");
			return;
		}
		d->time = data[2];
		if (!incoming_begin(r, true, d->time, 0))
			target_finish(r, false, "failed");
	}
}

static char *
selection_base64(const struct file_transfer_record *record)
{
	static const char alphabet[] =
		"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
	size_t length = record->selection_bytes;
	char *out = fxcalloc(((length + 2) / 3) * 4 + 1, 1);
	size_t used = 0;
	for (size_t i = 0; i < length; i += 3)
	{
		uint32_t n = (uint32_t)record->selection[i] << 16;
		if (i + 1 < length)
			n |= (uint32_t)record->selection[i + 1] << 8;
		if (i + 2 < length)
			n |= record->selection[i + 2];
		out[used++] = alphabet[n >> 18];
		out[used++] = alphabet[(n >> 12) & 63];
		out[used++] = i + 1 < length ? alphabet[(n >> 6) & 63] : '=';
		out[used++] = i + 2 < length ? alphabet[n & 63] : '=';
	}
	return out;
}

static void
native_action(struct ft_runtime *r,
	const struct file_transfer_record *record, bool drag)
{
	uint64_t transfer = ++r->next_id;
	char *bits = selection_base64(record);
	cJSON *o = object(drag ? "drag-begin" : "action");
	add_id(o, "transfer", transfer);
	add_id(o, "epoch", record->epoch);
	cJSON_AddNumberToObject(o, "generation", record->generation);
	cJSON_AddNumberToObject(o, "time", record->timestamp);
	cJSON_AddNumberToObject(o, "rows", record->rows);
	cJSON_AddStringToObject(o, "selection", bits);
	free(bits);
	if (!drag)
		cJSON_AddStringToObject(o, "kind",
			record->detail == FILE_TRANSFER_ACTION_COPY ? "copy" :
			record->detail == FILE_TRANSFER_ACTION_CUT ? "cut" : "paste");
	else
	{
		source_cancel(r, "cancelled");
		r->drag = (struct ft_drag){
			.active = true, .frame = record->frame,
			.time = record->timestamp, .x = record->root_x,
			.y = record->root_y, .state = record->state,
			.move = (record->state & ShiftMask) != 0,
			.requested_move = (record->state & ShiftMask) != 0,
			.transfer = transfer, .epoch = record->epoch,
			.generation = record->generation,
			.began = now_ms(), .touched = now_ms()};
	}
	output(r, o);
}

static void
peer_close(struct ft_runtime *r, size_t index)
{
	struct ft_peer *p = &r->peers[index];
	Window frame = p->identity.frame, list = p->identity.list;
	if ((int)index == r->active_peer)
	{
		r->active_peer = -1;
		source_cancel(r, "cancelled");
		target_finish(r, false, "cancelled");
		incoming_fail(r, "cancelled");
		r->active_peer = -1;
	}
	if (p->fd >= 0)
		close(p->fd);
	memset(p, 0, sizeof(*p));
	p->fd = -1;
	refresh_watch(r, frame);
	refresh_watch(r, list);
}

static void
publish_attachment(struct ft_runtime *r,
	const struct file_transfer_record *record)
{
	cJSON *o = object("attached");
	add_id(o, "epoch", record->epoch);
	cJSON_AddNumberToObject(o, "generation", record->generation);
	cJSON_AddNumberToObject(o, "frame", record->frame);
	cJSON_AddNumberToObject(o, "list", record->list);
	output(r, o);
}

static void
peer_record(struct ft_runtime *r, size_t index,
	const struct file_transfer_record *record)
{
	struct ft_peer *p = &r->peers[index];
	if (memcmp(record->nonce, r->nonce, 16))
		goto invalid;
	/* A retired Form can finish an already buffered attachment after the
	 * current epoch is ready. Retire only its authenticated peer. */
	if (record->epoch < r->expected_epoch)
	{
		peer_close(r, index);
		return;
	}
	if (record->epoch != r->expected_epoch)
		goto invalid;
	if (record->kind == FILE_TRANSFER_ATTACH)
	{
		if (p->attached || (r->active_peer >= 0 &&
		    r->peers[r->active_peer].identity.epoch == record->epoch))
			goto invalid;
		if (r->active_peer >= 0)
			peer_close(r, r->active_peer);
		p->identity = *record;
		p->attached = true;
		r->active_peer = index;
		refresh_watch(r, record->frame);
		refresh_watch(r, record->list);
		publish_attachment(r, record);
		peer_feedback(r, FILE_TRANSFER_IDLE);
		return;
	}
	if (!p->attached || r->active_peer != (int)index ||
	    p->identity.frame != record->frame ||
	    p->identity.list != record->list ||
	    (record->kind != FILE_TRANSFER_GENERATION &&
	    record->generation != p->identity.generation))
		goto invalid;
	if (record->kind == FILE_TRANSFER_GENERATION)
	{
		/* Cancellation feedback belongs to the newly validated peer view.
		 * Publish its attachment only after older work has been retired. */
		p->identity = *record;
		p->identity.kind = FILE_TRANSFER_ATTACH;
		source_cancel(r, "cancelled");
		target_finish(r, false, "cancelled");
		incoming_fail(r, "cancelled");
		publish_attachment(r, record);
		peer_feedback(r, FILE_TRANSFER_IDLE);
		return;
	}
	if (record->kind == FILE_TRANSFER_DETACH)
	{
		peer_close(r, index);
		return;
	}
	if (record->kind == FILE_TRANSFER_CANCEL)
	{
		source_cancel(r, "cancelled");
		return;
	}
	if (!context_current(r, record->epoch, record->generation))
	{
		peer_feedback(r, FILE_TRANSFER_REJECTED);
		return;
	}
	if (record->kind == FILE_TRANSFER_ACTION ||
	    record->kind == FILE_TRANSFER_BEGIN)
	{
		if (!record->timestamp)
			return;
		native_action(r, record, record->kind == FILE_TRANSFER_BEGIN);
	}
	else if (record->kind == FILE_TRANSFER_MOTION ||
	    record->kind == FILE_TRANSFER_RELEASE)
	{
		struct ft_drag *d = &r->drag;
		if (!d->active || d->released ||
		    !ft_time_valid(record->timestamp, d->time))
			return;
		d->x = record->root_x;
		d->y = record->root_y;
		d->state = record->state;
		d->requested_move = (record->state & ShiftMask) != 0;
		d->time = record->timestamp;
		d->dirty = true;
		d->released = record->kind == FILE_TRANSFER_RELEASE;
		if (d->ready && !d->waiting)
			source_position(r);
	}
	else if (record->kind == FILE_TRANSFER_CLIENT_MESSAGE)
		client_message(r, record);
	return;
invalid:
	unavailable(r, "invalid-form");
	peer_close(r, index);
}

static char *
canonical_uris(struct ft_runtime *r, const char *text, bool local)
{
	/* Rewrite only authority bytes; percent-encoded path bytes stay intact. */
	size_t capacity = FILE_TRANSFER_MAX_URI_BYTES + 1, used = 0;
	char *out = fxmalloc(capacity);
	const char *p = text;
	while (*p)
	{
		const char *end = strchr(p, '\n');
		size_t length = end ? (size_t)(end - p) : strlen(p);
		if (length && p[length - 1] == '\r')
			length--;
		if (length && p[0] != '#')
		{
			const char *path = memchr(p + 7, '/', length - 7);
			size_t path_length = (size_t)(p + length - path);
			size_t host_length = local ? strlen(r->hostname) :
				(size_t)(path - p - 7);
			size_t need = 7 + host_length + path_length + 2;
			if (need > capacity - used - 1)
			{
				free(out);
				return NULL;
			}
			memcpy(out + used, "file://", 7);
			used += 7;
			memcpy(out + used, local ? r->hostname : p + 7, host_length);
			used += host_length;
			memcpy(out + used, path, path_length);
			used += path_length;
			out[used++] = '\r';
			out[used++] = '\n';
		}
		p = end ? end + 1 : p + strlen(p);
	}
	out[used] = 0;
	return out;
}

static void
execute_command(struct ft_runtime *r, struct ft_command *c)
{
	bool ok = false;
	const char *code = "invalid-state";
	if ((c->kind == FT_OFFER || c->kind == FT_READ) && !c->has_time)
	{
		if (r->timed_command)
		{
			reply(r, c->id, false, "busy", 0);
			return;
		}
		if (strlen(c->uris) > FT_RETAINED_MAX - r->retained -
		    r->output_bytes - r->input_used)
		{
			reply(r, c->id, false, "limit", 0);
			return;
		}
		r->retained += strlen(c->uris);
		r->timed_command = fxmalloc(sizeof(*c));
		*r->timed_command = *c;
		r->clock_deadline = now_ms() + 5000;
		r->timed_epoch = r->context_epoch;
		r->timed_generation = r->generation;
		unsigned char value = 0;
		change_property(r, r->owner, r->clock_property,
			XA_INTEGER, 8, PropModeReplace, &value, 1);
		return;
	}
	switch (c->kind)
	{
	case FT_EXPECT:
		if (c->epoch <= r->expected_epoch)
			break;
		source_cancel(r, "cancelled");
		target_finish(r, false, "cancelled");
		incoming_fail(r, "cancelled");
		for (size_t i = 0; i < FT_PEERS; i++)
			if (r->peers[i].attached)
				peer_close(r, i);
		r->expected_epoch = c->epoch;
		r->enabled = false;
		ok = true;
		break;
	case FT_CONTEXT:
		if (c->epoch != r->expected_epoch)
			break;
		if (r->context_epoch != c->epoch ||
		    r->generation != c->generation || !c->enabled)
		{
			source_cancel(r, "cancelled");
			target_finish(r, false, "cancelled");
			incoming_fail(r, "cancelled");
		}
		r->context_epoch = c->epoch;
		r->generation = c->generation;
		r->enabled = c->enabled;
		ok = true;
		break;
	case FT_OFFER:
		if (!ft_uris_valid(c->uris, r->hostname))
		{
			code = "invalid-uris";
			break;
		}
		if (!ft_time_valid(c->time, r->clipboard_time))
			break;
		char *canonical = canonical_uris(r, c->uris, false);
		if (!canonical)
			break;
		struct ft_bytes *offered = bytes_new(r, canonical);
		free(canonical);
		if (!offered)
		{
			code = "limit";
			break;
		}
		XSetSelectionOwner(r->display, r->clipboard, r->owner, c->time);
		if (XGetSelectionOwner(r->display, r->clipboard) != r->owner)
		{
			bytes_release(r, &offered);
			code = "ownership";
			break;
		}
		/* A new local offer supersedes an in-flight read of our own lease. */
		if (r->incoming.active && r->incoming.selection == r->clipboard &&
		    r->incoming.owner == r->owner)
			incoming_fail(r, "cancelled");
		bytes_release(r, &r->clipboard_bytes);
		r->clipboard_bytes = offered;
		r->clipboard_time = c->time;
		r->clipboard_move = c->move;
		r->lease = ++r->next_id;
		reply(r, c->id, true, "ok", r->lease);
		return;
	case FT_READ:
		if (r->incoming_drop.active)
			break;
		ok = incoming_begin(r, false, c->time, c->id);
		if (ok)
		{
			cJSON *o = object(NULL);
			add_id(o, "id", c->id);
			cJSON_AddStringToObject(o, "status", "ok");
			cJSON_AddStringToObject(o, "code", "ok");
			add_id(o, "transfer", r->incoming.transfer);
			output(r, o);
			return;
		}
		break;
	case FT_DRAG_DATA:
		if (!r->drag.active || r->drag.ready ||
		    c->transfer != r->drag.transfer ||
		    !context_current(r, r->drag.epoch, r->drag.generation))
			break;
		if (!ft_uris_valid(c->uris, r->hostname))
		{
			code = "invalid-uris";
			break;
		}
		char *encoded = canonical_uris(r, c->uris, true);
		if (!encoded)
			break;
		r->drag_bytes = bytes_new(r, encoded);
		free(encoded);
		if (!r->drag_bytes)
			break;
		r->drag_time = r->drag.time;
		XSetSelectionOwner(r->display, r->xdnd_selection,
			r->owner, r->drag_time);
		if (XGetSelectionOwner(r->display, r->xdnd_selection) != r->owner)
		{
			bytes_release(r, &r->drag_bytes);
			break;
		}
		unsigned long actions[] = {r->action_copy, r->action_move};
		change_property(r, r->drag.frame, r->action_list,
			XA_ATOM, 32, PropModeReplace, (unsigned char *)actions, 2);
		r->drag.ready = true;
		/* Native gesture metadata owns the latest modifier/action intent. */
		r->drag.move = r->drag.requested_move;
		source_position(r);
		ok = true;
		break;
	case FT_ADMIT:
		if (!r->incoming_drop.active || !r->incoming_drop.proposed ||
		    r->incoming_drop.admitted ||
		    c->transfer != r->incoming_drop.transfer ||
		    !context_current(r, r->incoming_drop.epoch,
		    r->incoming_drop.generation))
			break;
		/* The acknowledgment is enqueued before the parent can start a job. */
		if (!reply(r, c->id, true, "ok", 0))
			return;
		r->incoming_drop.admitted = true;
		r->incoming_drop.deadline = now_ms() + 300000;
		return;
	case FT_COMPLETE:
		if (!r->incoming_drop.active || !r->incoming_drop.proposed ||
		    c->transfer != r->incoming_drop.transfer ||
		    (c->success && (!r->incoming_drop.admitted ||
		    c->move != r->incoming_drop.move)))
			break;
		target_finish(r, c->success, c->success ? "success" : "failed");
		ok = true;
		break;
	case FT_CLEAR:
		if (c->lease == r->lease && r->clipboard_bytes &&
		    XGetSelectionOwner(r->display, r->clipboard) == r->owner)
		{
			XSetSelectionOwner(r->display, r->clipboard, None, r->clipboard_time);
			bytes_release(r, &r->clipboard_bytes);
			r->lease = 0;
		}
		ok = true;
		break;
	case FT_CANCEL:
		if (r->drag.active && c->transfer == r->drag.transfer)
			source_cancel(r, "cancelled");
		if (r->incoming_drop.active &&
		    c->transfer == r->incoming_drop.transfer)
			target_finish(r, false, "cancelled");
		if (r->incoming.active && c->transfer == r->incoming.transfer)
			incoming_fail(r, "cancelled");
		ok = true;
		break;
	case FT_SHUTDOWN:
		r->stopping = true;
		ok = true;
		break;
	}
	reply(r, c->id, ok, ok ? "ok" : code, 0);
}

static void
x_event(struct ft_runtime *r, const XEvent *event)
{
	switch (event->type)
	{
	case SelectionRequest:
		selection_request(r, &event->xselectionrequest);
		break;
	case SelectionNotify:
		incoming_notify(r, &event->xselection);
		break;
	case SelectionClear:
		if (event->xselectionclear.selection == r->clipboard && r->lease &&
		    XGetSelectionOwner(r->display, r->clipboard) != r->owner &&
		    ft_time_valid(event->xselectionclear.time, r->clipboard_time))
		{
			cJSON *o = object("lost");
			add_id(o, "lease", r->lease);
			output(r, o);
			r->lease = 0;
			bytes_release(r, &r->clipboard_bytes);
		}
		else if (event->xselectionclear.selection == r->xdnd_selection &&
		    XGetSelectionOwner(r->display, r->xdnd_selection) != r->owner)
			source_cancel(r, "failed");
		break;
	case PropertyNotify:
		if (r->timed_command && event->xproperty.window == r->owner &&
		    event->xproperty.atom == r->clock_property &&
		    event->xproperty.state == PropertyNewValue)
		{
			struct ft_command *c = r->timed_command;
			r->timed_command = NULL;
			r->retained -= strlen(c->uris);
			c->time = event->xproperty.time;
			c->has_time = true;
			if (c->kind == FT_READ &&
			    (r->timed_epoch != r->context_epoch ||
			    r->timed_generation != r->generation))
				reply(r, c->id, false, "stale-context", 0);
			else
				execute_command(r, c);
			free(c);
		}
		else
			property_event(r, &event->xproperty);
		break;
	case DestroyNotify:
		if (r->incoming.active &&
		    r->incoming.owner == event->xdestroywindow.window)
			incoming_fail(r, "failed");
		else if (r->incoming.active &&
		    r->incoming.window == event->xdestroywindow.window)
			incoming_fail(r, "cancelled");
		for (size_t i = 0; i < 4; i++)
			if (r->outgoing[i].window == event->xdestroywindow.window)
				outgoing_reset(r, &r->outgoing[i]);
		if (r->drag.active &&
		    r->drag.target == event->xdestroywindow.window)
			source_cancel(r, "failed");
		else if (r->drag.active &&
		    r->drag.frame == event->xdestroywindow.window)
			source_cancel(r, "cancelled");
		if (r->incoming_drop.active &&
		    r->incoming_drop.source == event->xdestroywindow.window)
			target_finish(r, false, "failed");
		else if (r->incoming_drop.active &&
		    r->incoming_drop.target == event->xdestroywindow.window)
			target_finish(r, false, "cancelled");
		for (size_t i = 0; i < FT_PEERS; i++)
			if (r->peers[i].attached &&
			    (r->peers[i].identity.frame == event->xdestroywindow.window ||
			    r->peers[i].identity.list == event->xdestroywindow.window))
				peer_close(r, i);
		break;
	}
}

static bool
peer_same_user(int fd)
{
#if defined(__linux__) && defined(SO_PEERCRED)
	struct {
		pid_t pid;
		uid_t uid;
		gid_t gid;
	} credentials;
	socklen_t length = sizeof(credentials);
	return getsockopt(fd, SOL_SOCKET, SO_PEERCRED, &credentials, &length) == 0 &&
		length == sizeof(credentials) && credentials.uid == getuid();
#elif defined(__FreeBSD__) || defined(__OpenBSD__) || defined(__APPLE__)
	uid_t uid;
	gid_t gid;
	return getpeereid(fd, &uid, &gid) == 0 && uid == getuid();
#else
	(void)fd;
	/* Other platforms still require the private directory and nonce. */
	return true;
#endif
}

static bool
listener_open(struct ft_runtime *r, const char *path)
{
	struct sockaddr_un address = {0};
	struct stat st;
	char parent[sizeof(address.sun_path)];
	if (!path || path[0] != '/' || strlen(path) >= sizeof(address.sun_path))
		return false;
	strcpy(parent, path);
	char *slash = strrchr(parent, '/');
	if (!slash || slash == parent || !slash[1])
		return false;
	*slash = 0;
	if (lstat(parent, &st) || !S_ISDIR(st.st_mode) || st.st_uid != getuid() ||
	    (st.st_mode & 077) || lstat(path, &st) == 0 || errno != ENOENT)
		return false;
	r->listener = socket(AF_UNIX, SOCK_STREAM, 0);
	if (r->listener < 0 || !nonblocking(r->listener))
		return false;
	address.sun_family = AF_UNIX;
#if defined(__FreeBSD__) || defined(__OpenBSD__) || defined(__APPLE__)
	address.sun_len = sizeof(address);
#endif
	strcpy(address.sun_path, path);
	if (bind(r->listener, (struct sockaddr *)&address, sizeof(address)))
		return false;
	r->socket_path = path;
	if (chmod(path, 0600) || listen(r->listener, 2))
		return false;
	return true;
}

static void
peer_accept(struct ft_runtime *r)
{
	int fd = accept(r->listener, NULL, NULL);
	if (fd < 0)
		return;
	if (!peer_same_user(fd) || !nonblocking(fd))
	{
		close(fd);
		return;
	}
	for (size_t i = 0; i < FT_PEERS; i++)
		if (r->peers[i].fd < 0)
		{
			r->peers[i].fd = fd;
			r->peers[i].input_touched = now_ms();
			return;
		}
	close(fd);
}

static void
peer_io(struct ft_runtime *r, size_t index, short events)
{
	struct ft_peer *p = &r->peers[index];
	if (events & (POLLHUP | POLLERR | POLLNVAL))
	{
		peer_close(r, index);
		return;
	}
	if (events & POLLIN)
	{
		/* One record per dispatch preserves fairness and release ordering. */
		ssize_t n = read(p->fd, p->input + p->used,
			FILE_TRANSFER_WIRE_BYTES - p->used);
		if (!n || (n < 0 && errno != EAGAIN && errno != EINTR))
		{
			peer_close(r, index);
			return;
		}
		if (n > 0)
		{
			p->used += n;
			p->input_touched = now_ms();
		}
		if (p->used == FILE_TRANSFER_WIRE_BYTES)
		{
			struct file_transfer_record record;
			p->used = 0;
			if (!file_transfer_decode(p->input, sizeof(p->input), &record))
			{
				unavailable(r, "invalid-form");
				peer_close(r, index);
				return;
			}
			peer_record(r, index, &record);
		}
	}
	if (p->fd >= 0 && (events & POLLOUT) && p->output_count)
	{
		ssize_t n = fvwm_write_without_sigpipe(p->fd,
			p->output[0] + p->sent, FILE_TRANSFER_WIRE_BYTES - p->sent);
		if (n < 0 && errno != EAGAIN && errno != EINTR)
		{
			peer_close(r, index);
			return;
		}
		if (n > 0)
		{
			p->sent += n;
			p->output_touched = now_ms();
		}
		if (p->sent == FILE_TRANSFER_WIRE_BYTES)
		{
			p->sent = 0;
			p->output_count--;
			memmove(p->output, p->output + 1,
				p->output_count * FILE_TRANSFER_WIRE_BYTES);
		}
	}
}

static void
output_flush(struct ft_runtime *r)
{
	if (!r->output_count)
		return;
	struct ft_output *o = &r->output[0];
	ssize_t n = fvwm_write_without_sigpipe(STDOUT_FILENO,
		o->data + o->sent, o->length - o->sent);
	if (n < 0 && errno != EAGAIN && errno != EINTR)
	{
		r->stopping = r->failed = true;
		return;
	}
	if (n > 0)
		o->sent += n;
	if (o->sent == o->length)
	{
		r->output_bytes -= o->length;
		free(o->data);
		r->output_count--;
		memmove(r->output, r->output + 1,
			r->output_count * sizeof(r->output[0]));
	}
}

static void
input_process(struct ft_runtime *r)
{
	size_t consumed = 0, records = 0;
	while (consumed < r->input_used && records < FT_QUEUE &&
	       !r->timed_command)
	{
		char *end = memchr(r->input + consumed, '\n', r->input_used - consumed);
		if (!end)
			break;
		struct ft_command *c = fxmalloc(sizeof(*c));
		size_t length = (size_t)(end - r->input - consumed);
		if (!ft_command_parse(r->input + consumed, length, c))
			unavailable(r, "invalid-command");
		else
			execute_command(r, c);
		free(c);
		consumed += length + 1;
		records++;
		if (r->stopping)
			break;
	}
	if (consumed)
	{
		r->input_used -= consumed;
		memmove(r->input, r->input + consumed, r->input_used);
	}
	size_t queued = 0;
	for (size_t i = 0; i < r->input_used; i++)
		queued += r->input[i] == '\n';
	if (queued > FT_QUEUE || r->input_used == sizeof(r->input))
	{
		unavailable(r, "frame-limit");
		r->stopping = true;
	}
}

static void
deadlines(struct ft_runtime *r)
{
	if (r->peer_overflow)
	{
		r->peer_overflow = false;
		if (r->active_peer >= 0)
			peer_close(r, r->active_peer);
		unavailable(r, "form-queue-limit");
	}
	while (r->lost_count)
	{
		XEvent event = {0};
		event.type = DestroyNotify;
		event.xdestroywindow.window = r->lost_windows[--r->lost_count];
		x_event(r, &event);
	}
	int64_t now = now_ms();
	if (now < 0)
	{
		r->stopping = r->failed = true;
		return;
	}
	for (size_t i = 0; i < FT_PEERS; i++)
	{
		struct ft_peer *p = &r->peers[i];
		if (p->fd >= 0 &&
		    (((!p->attached || p->used) &&
		    now - p->input_touched >= 5000) ||
		    (p->output_count && now - p->output_touched >= 5000)))
			peer_close(r, i);
	}
	if (r->timed_command && now >= r->clock_deadline)
	{
		r->retained -= strlen(r->timed_command->uris);
		reply(r, r->timed_command->id, false, "timestamp-timeout", 0);
		free(r->timed_command);
		r->timed_command = NULL;
	}
	for (size_t i = 0; i < 4; i++)
		if (r->outgoing[i].bytes &&
		    (now - r->outgoing[i].touched >= 5000 ||
		    now - r->outgoing[i].began >= 30000))
			outgoing_reset(r, &r->outgoing[i]);
	/* Owner loss is a failure regardless of which event or probe observes
	 * it first. Only elapsed inactivity/absolute bounds report a timeout. */
	if (r->incoming.active)
	{
		if (XGetSelectionOwner(r->display, r->incoming.selection) !=
		    r->incoming.owner)
			incoming_fail(r, "failed");
		else if (now - r->incoming.touched >= 5000 ||
		    now - r->incoming.began >= 30000)
			incoming_fail(r, "timeout");
	}
	if (r->drag.active)
	{
		if (r->drag.ready && XGetSelectionOwner(r->display,
		    r->xdnd_selection) != r->owner)
			source_cancel(r, "failed");
		else if ((!r->drag.dropped && now - r->drag.touched >= 5000) ||
		    now - r->drag.began >= (r->drag.dropped ? 330000 : 30000))
			source_cancel(r, "timeout");
	}
	if (r->incoming_drop.active)
	{
		if (r->incoming_drop.selection_owner &&
		    XGetSelectionOwner(r->display, r->xdnd_selection) !=
		    r->incoming_drop.selection_owner)
			target_finish(r, false, "failed");
		else if (now >= r->incoming_drop.deadline ||
		    (r->incoming_drop.source && !r->incoming_drop.dropped &&
		    now - r->incoming_drop.touched >= 5000))
			target_finish(r, false, "timeout");
	}
}

static bool
atoms_init(struct ft_runtime *r)
{
	char *names[] = {"CLIPBOARD", "XdndSelection", "text/uri-list",
		"x-special/gnome-copied-files", "TARGETS", "TIMESTAMP", "MULTIPLE",
		"ATOM_PAIR", "INCR", "_FVWM_FILE_TRANSFER_DATA",
		"_FVWM_FILE_TRANSFER_CLOCK", "text/x-xdnd-username", "XdndAware",
		"XdndEnter", "XdndPosition", "XdndStatus", "XdndLeave", "XdndDrop",
		"XdndFinished", "XdndActionCopy", "XdndActionMove", "XdndTypeList",
		"XdndActionList"};
	Atom atoms[23];
	if (!XInternAtoms(r->display, names, 23, False, atoms))
		return false;
	r->clipboard = atoms[0];
	r->xdnd_selection = atoms[1];
	r->uri = atoms[2];
	r->gnome = atoms[3];
	r->targets = atoms[4];
	r->timestamp = atoms[5];
	r->multiple = atoms[6];
	r->atom_pair = atoms[7];
	r->incr = atoms[8];
	r->property = atoms[9];
	r->clock_property = atoms[10];
	r->username = atoms[11];
	r->aware = atoms[12];
	r->enter = atoms[13];
	r->position = atoms[14];
	r->status = atoms[15];
	r->leave = atoms[16];
	r->drop = atoms[17];
	r->finished = atoms[18];
	r->action_copy = atoms[19];
	r->action_move = atoms[20];
	r->type_list = atoms[21];
	r->action_list = atoms[22];
	return true;
}

static bool
token_parse(const char *text, unsigned char nonce[16])
{
	if (strlen(text) != 32)
		return false;
	unsigned int any = 0;
	for (size_t i = 0; i < 16; i++)
	{
		unsigned int value = 0;
		for (size_t j = 0; j < 2; j++)
		{
			unsigned char c = text[i * 2 + j];
			if (c >= '0' && c <= '9')
				value = value * 16 + c - '0';
			else if (c >= 'a' && c <= 'f')
				value = value * 16 + c - 'a' + 10;
			else if (c >= 'A' && c <= 'F')
				value = value * 16 + c - 'A' + 10;
			else
				return false;
		}
		nonce[i] = value;
		any |= value;
	}
	return any != 0;
}

int
main(int argc, char **argv)
{
	struct ft_runtime *r = fxcalloc(1, sizeof(*r));
	int status = 1;
	r->listener = r->active_peer = -1;
	for (size_t i = 0; i < FT_PEERS; i++)
		r->peers[i].fd = -1;
	if (argc != 5 || strcmp(argv[1], "--form-socket") ||
	    strcmp(argv[3], "--token") || !token_parse(argv[4], r->nonce) ||
	    !nonblocking(STDIN_FILENO) || !nonblocking(STDOUT_FILENO) ||
	    gethostname(r->hostname, sizeof(r->hostname)) ||
	    !memchr(r->hostname, 0, sizeof(r->hostname)) || !r->hostname[0])
		goto done;
	/* Hostname authority is literal ASCII, never an arbitrary URI authority. */
	for (const unsigned char *p = (unsigned char *)r->hostname; *p; p++)
		if (!((*p >= 'a' && *p <= 'z') || (*p >= 'A' && *p <= 'Z') ||
		    (*p >= '0' && *p <= '9') || *p == '-' || *p == '.'))
			goto done;
	const char *user = getenv("USER");
	if (user && strlen(user) < sizeof(r->user))
		strcpy(r->user, user);
	if (!listener_open(r, argv[2]))
		goto done;
	r->display = XOpenDisplay(NULL);
	if (!r->display)
	{
		unavailable(r, "display");
		output_flush(r);
		goto done;
	}
	x_runtime = r;
	XSetErrorHandler(x_error);
	XSetIOErrorHandler(x_io_error);
	r->root = DefaultRootWindow(r->display);
	if (!atoms_init(r))
		goto done;
	r->owner = XCreateSimpleWindow(r->display, r->root, 0, 0, 1, 1, 0, 0, 0);
	select_input(r, r->owner, PropertyChangeMask);
	cJSON *ready = object("ready");
	cJSON_AddStringToObject(ready, "hostname", r->hostname);
	output(r, ready);
	while (!r->stopping)
	{
		input_process(r);
		for (size_t i = 0; i < 64 && XPending(r->display); i++)
		{
			XEvent event;
			XNextEvent(r->display, &event);
			x_event(r, &event);
		}
		deadlines(r);
		XFlush(r->display);
		struct pollfd fds[6] = {
			{STDIN_FILENO, POLLIN, 0},
			{STDOUT_FILENO, r->output_count ? POLLOUT : 0, 0},
			{ConnectionNumber(r->display), POLLIN, 0},
			{r->listener, POLLIN, 0},
			{r->peers[0].fd, POLLIN |
				(r->peers[0].output_count ? POLLOUT : 0), 0},
			{r->peers[1].fd, POLLIN |
				(r->peers[1].output_count ? POLLOUT : 0), 0}};
		int timeout = !r->timed_command &&
			memchr(r->input, '\n', r->input_used) ? 0 : 50;
		if (poll(fds, 6, timeout) < 0)
		{
			if (errno == EINTR)
				continue;
			r->failed = true;
			break;
		}
		if (fds[0].revents & POLLIN)
		{
			size_t capacity = sizeof(r->input) - r->input_used;
			size_t retained = r->retained + r->output_bytes + r->input_used;
			if (retained >= FT_RETAINED_MAX)
			{
				r->failed = r->stopping = true;
				continue;
			}
			if (capacity > FT_RETAINED_MAX - retained)
				capacity = FT_RETAINED_MAX - retained;
			if (capacity > 65536)
				capacity = 65536;
			ssize_t n = read(STDIN_FILENO, r->input + r->input_used, capacity);
			if (n > 0)
				r->input_used += n;
			else if (!n || (errno != EAGAIN && errno != EINTR))
				r->stopping = true;
		}
		else if (fds[0].revents & (POLLHUP | POLLERR | POLLNVAL))
			r->stopping = true;
		if (fds[1].revents & POLLOUT)
			output_flush(r);
		if (fds[1].revents & (POLLERR | POLLHUP | POLLNVAL))
			r->failed = r->stopping = true;
		if (fds[3].revents & POLLIN)
			peer_accept(r);
		for (size_t i = 0; i < FT_PEERS; i++)
			if (r->peers[i].fd >= 0)
				peer_io(r, i, fds[i + 4].revents);
	}
	status = r->failed ? 1 : 0;
done:
	if (r->display)
	{
		source_cancel(r, "cancelled");
		target_finish(r, false, "cancelled");
		incoming_reset(r);
		for (size_t i = 0; i < 4; i++)
			outgoing_reset(r, &r->outgoing[i]);
		bytes_release(r, &r->clipboard_bytes);
		XCloseDisplay(r->display);
		x_runtime = NULL;
	}
	/* Shutdown is bounded even when the parent stops draining stdout. */
	int64_t until = now_ms() + 250;
	while (r->output_count && now_ms() < until)
	{
		struct pollfd fd = {STDOUT_FILENO, POLLOUT, 0};
		if (poll(&fd, 1, 10) > 0 && (fd.revents & POLLOUT))
			output_flush(r);
		else if (fd.revents & (POLLERR | POLLHUP | POLLNVAL))
			break;
	}
	for (size_t i = 0; i < FT_PEERS; i++)
		if (r->peers[i].fd >= 0)
			close(r->peers[i].fd);
	if (r->listener >= 0)
		close(r->listener);
	if (r->socket_path)
		unlink(r->socket_path);
	for (size_t i = 0; i < r->output_count; i++)
		free(r->output[i].data);
	free(r->timed_command);
	free(r);
	return status;
}
