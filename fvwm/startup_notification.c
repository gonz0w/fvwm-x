/* This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */
#include "config.h"
#include "startup_notification.h"
#include "add_window.h"
#include "eventmask.h"
#include "fvwm.h"
#include "execcontext.h"
#include "externs.h"
#include "screen.h"
#include "virtual.h"
#include "libs/FEvent.h"
#include "libs/FProperty.h"
#include "libs/Grab.h"
#include "libs/atom_batch.h"
#include "libs/startup_notification_policy.h"

#include <stdlib.h>
#include <string.h>
#include <time.h>

static struct startup_notification_state startup_activity;
static Atom startup_atoms[4];

/* Written only during startup, like the existing menu context consumers. */
extern XContext MenuContext;

_Static_assert(sizeof(struct startup_notification_state) +
	STARTUP_GROUP_STATE_MAX < 512 * 1024,
	"combined startup observer exceeds its fixed-storage budget");

enum { STARTUP_BEGIN, STARTUP_CONTINUE, STARTUP_ID, STARTUP_UTF8 };

/* False yields the outer batch until queued lifecycle events can dispatch. */
static bool startup_observe_window(FvwmWindow *window, bool inherit_group);

static uint64_t startup_now(void)
{
	struct timespec now;

	if (clock_gettime(CLOCK_MONOTONIC, &now) != 0)
		return UINT64_MAX;
	return (uint64_t)now.tv_sec * 1000 + (uint64_t)now.tv_nsec / 1000000;
}

void startup_notification_initialize(void)
{
	char *names[] = {"_NET_STARTUP_INFO_BEGIN", "_NET_STARTUP_INFO",
		"_NET_STARTUP_ID", "UTF8_STRING"};

	startup_notification_init(&startup_activity, (unsigned int)Scr.screen);
	eventmask_initialize();
	atom_batch_intern(dpy, names, 4, startup_atoms);
}

void startup_notification_service(void)
{
	startup_notification_expire(&startup_activity, startup_now());
	startup_group_pending(eventmask_startup_groups(),
		startup_notification_pending(&startup_activity), false);
}

int startup_notification_next_ms(void)
{
	return startup_notification_timeout_ms(
		&startup_activity, startup_now());
}

Bool startup_notification_client_message(const XClientMessageEvent *event)
{
	uint64_t generation = startup_activity.generation;

	if (!startup_atoms[STARTUP_BEGIN] ||
	    !startup_atoms[STARTUP_CONTINUE] ||
	    (event->message_type != startup_atoms[STARTUP_BEGIN] &&
	    event->message_type != startup_atoms[STARTUP_CONTINUE]))
		return False;
	if (event->format != 8)
	{
		if (startup_activity.malformed != UINT64_MAX)
			startup_activity.malformed++;
		if (startup_activity.generation)
			startup_activity.generation++;
		return True;
	}
	/* ClientMessage.window is the producer. The same core's root is the
	 * receiving screen context. Sender windows may disappear at once. */
	startup_notification_feed(&startup_activity, event->window, Scr.Root,
		event->message_type == startup_atoms[STARTUP_BEGIN],
		(const unsigned char *)event->data.b, startup_now());
	startup_group_pending(eventmask_startup_groups(),
		startup_notification_pending(&startup_activity),
		generation != startup_activity.generation);
	return True;
}

static void startup_group_dropped(void)
{
	if (startup_activity.dropped != UINT64_MAX)
		startup_activity.dropped++;
	if (startup_activity.generation)
		startup_activity.generation++;
}

static bool startup_group_target(Window window)
{
	FvwmWindow *owner = NULL;
	void *menu = NULL;

	if (window == None || window == Scr.Root || window == Scr.NoFocusWin ||
	    window == Scr.SizeWindow.win || is_pan_frame(window))
		return false;
	if (XFindContext(dpy, window, MenuContext, (caddr_t *)&menu) == 0)
		return false;
	if (XFindContext(dpy, window, FvwmContext, (caddr_t *)&owner) == 0 &&
	    owner != NULL && FW_W(owner) != window)
	{
		/* A client-supplied icon is still an external window. Core
		 * frames, decorations and owned icon windows are not leaders. */
		return FW_W_ICON_PIXMAP(owner) == window && !IS_ICON_OURS(owner);
	}
	return true;
}

void startup_notification_window_registered(FvwmWindow *window)
{
	struct startup_group_state *groups = eventmask_startup_groups();
	Window group = None;

	if (window == NULL || window->palette_identity == 0)
		return;
	startup_group_mode(groups, FW_W(window), true);
	if (window->wmhints != NULL &&
	    (window->wmhints->flags & WindowGroupHint) != 0 &&
	    window->wmhints->window_group != FW_W(window) &&
	    startup_group_target(window->wmhints->window_group))
		group = window->wmhints->window_group;
	if (!startup_group_bind(groups, FW_W(window), window->palette_identity,
		group, NextRequest(dpy)))
		startup_group_dropped();
}

void startup_notification_window_retiring(FvwmWindow *window)
{
	struct startup_group_state *groups = eventmask_startup_groups();

	if (window == NULL)
		return;
	/* Another managed client may still refer to this live leader. Do not
	 * let the legacy NoEventMask/tray transition discard its lifecycle. */
	startup_group_mode(groups, FW_W(window), false);
	startup_group_unbind(groups, FW_W(window), window->palette_identity);
}

void startup_notification_raw_property(const XPropertyEvent *event)
{
	if (event->atom == startup_atoms[STARTUP_ID])
		(void)startup_group_notify(eventmask_startup_groups(), event->window,
			event->serial, false);
}

Bool startup_notification_destroyed(const XDestroyWindowEvent *event)
{
	struct startup_group_state *groups = eventmask_startup_groups();
	struct startup_group_leader *leader =
		startup_group_find_leader(groups, event->window);

	/* A rejected event must not fall through to ordinary managed teardown:
	 * its context may now describe the live lifetime of a reused XID. */
	if (leader != NULL &&
	    (event->send_event || event->serial < leader->serial))
		return False;
	if (!event->send_event)
		(void)startup_group_notify(groups, event->window, event->serial, true);
	return True;
}

static void startup_group_mask_step(struct startup_group_leader *leader)
{
	struct startup_group_state *groups = eventmask_startup_groups();
	Window window = (Window)leader->window;
	FvwmWindow *managed;

	if (leader->references == 0)
	{
		if (leader->acquired)
			XSelectInput(dpy, window, leader->base_mask);
		(void)startup_group_release(groups, window);
		return;
	}
	if (!startup_group_target(window))
	{
		if (leader->acquired)
			XSelectInput(dpy, window, leader->base_mask);
		startup_group_abandon(groups, window);
		startup_group_dropped();
		return;
	}
	managed = find_managed_client(window);
	if (!leader->acquired)
	{
		XWindowAttributes attributes;
		unsigned long serial;
		Status exists;

		/* First import and subscription are one server-owned interval:
		 * otherwise destruction between them can escape lifecycle delivery.
		 * The existing nested grab helpers supply their own barriers. */
		MyXGrabServer(dpy);
		exists = XGetWindowAttributes(dpy, window, &attributes);
		if (exists)
		{
			serial = NextRequest(dpy);
			startup_group_acquired(groups, window,
				attributes.your_event_mask, serial, managed != NULL);
			XSelectInput(dpy, window,
				startup_group_effective(groups, leader));
			leader->mask_dirty = false;
		}
		MyXUngrabServer(dpy);
		if (!exists)
		{
			startup_group_abandon(groups, window);
			startup_group_dropped();
		}
		return;
	}
	startup_group_mode(groups, window, managed != NULL);
	/* Selection precedes the queued dependent read on this display. */
	XSelectInput(dpy, window, startup_group_effective(groups, leader));
	leader->mask_dirty = false;
}

Bool startup_notification_outer_pending(void)
{
	return eventmask_outer_ready() &&
		startup_group_outer_pending(eventmask_startup_groups());
}

void startup_notification_outer_boundary(void)
{
	struct startup_group_state *groups = eventmask_startup_groups();
	struct startup_group_work work;

	if (!eventmask_outer_ready())
		return;
	startup_notification_service();
	for (unsigned int step = 0; step < STARTUP_GROUP_STEP_MAX; step++)
	{
		struct startup_group_binding *binding;
		FvwmWindow *window;

		if (!startup_group_next_work(groups, &work))
			break;
		if (work.kind == STARTUP_GROUP_MASK_WORK)
		{
			startup_group_mask_step(&groups->leaders[work.index]);
			continue;
		}
		binding = &groups->clients[work.index];
		window = find_managed_client((Window)binding->client);
		if (window == NULL || window->palette_identity != binding->identity)
		{
			startup_group_unbind(groups, binding->client, binding->identity);
			continue;
		}
		if (window->wmhints == NULL ||
		    (window->wmhints->flags & WindowGroupHint) == 0 ||
		    window->wmhints->window_group != binding->leader)
		{
			startup_notification_window_registered(window);
			continue;
		}
		if (!startup_observe_window(window, true))
			break;
	}
}

static FPropertyResult startup_id(Window window,
	char id[STARTUP_TEXT_MAX + 1],
	bool *present)
{
	unsigned char *data = NULL;
	size_t bytes = 0;
	FPropertyResult result;

	id[0] = '\0';
	*present = true;
	result = FPropertyRead(dpy, window, startup_atoms[STARTUP_ID],
		startup_atoms[STARTUP_UTF8],
		(FPropertyLimits){1, STARTUP_TEXT_MAX + 1, 8}, &data, &bytes);
	if (result == FPROPERTY_MISSING)
	{
		*present = false;
		return result;
	}
	if (result != FPROPERTY_OK)
		return result;
	/* Accept the common optional final terminator, never embedded or
	 * multiple NULs. The content bound remains 255 UTF-8 bytes. */
	if (bytes && data[bytes - 1] == '\0')
		bytes--;
	data[bytes] = '\0';
	bool valid = bytes != 0 && bytes <= STARTUP_TEXT_MAX &&
		memchr(data, '\0', bytes) == NULL &&
		startup_notification_text_valid((const char *)data, true);

	if (valid)
		memcpy(id, data, bytes + 1);
	free(data);
	return valid ? FPROPERTY_OK : FPROPERTY_INVALID;
}

/* Only the WM owns completion of an unambiguous legacy class match.
 * Use the already interned atoms and asynchronous requests: this adds no
 * blocking display round trip, focus action or retained producer window.
 * The pure match supplies an already bounded ID (255 bytes before quoting). */
static bool startup_remove_legacy(const char *id)
{
	char message[2 * STARTUP_TEXT_MAX + 32] = "remove: ID=\"";
	size_t used = strlen(message);
	Window producer;
	bool sent = true;

	if (!startup_atoms[STARTUP_BEGIN] || !startup_atoms[STARTUP_CONTINUE])
		return false;
	for (; *id; id++)
	{
		if (*id == '\\' || *id == '"')
			message[used++] = '\\';
		message[used++] = *id;
	}
	message[used++] = '"';
	message[used] = '\0';
	producer = XCreateSimpleWindow(dpy, Scr.Root, 0, 0, 1, 1, 0, 0, 0);
	if (!producer)
		return false;
	for (size_t offset = 0; offset <= used; offset += 20)
	{
		XEvent event;
		size_t remaining = used + 1 - offset;

		memset(&event, 0, sizeof(event));
		event.xclient.type = ClientMessage;
		event.xclient.display = dpy;
		event.xclient.window = producer;
		event.xclient.message_type = startup_atoms[offset ?
			STARTUP_CONTINUE : STARTUP_BEGIN];
		event.xclient.format = 8;
		memcpy(event.xclient.data.b, message + offset,
			remaining < 20 ? remaining : 20);
		if (!FSendEvent(dpy, Scr.Root, False,
			PropertyChangeMask, &event))
		{
			sent = false;
			break;
		}
	}
	XDestroyWindow(dpy, producer);
	return sent;
}

static bool startup_observe_window(FvwmWindow *window, bool inherit_group)
{
	char id[STARTUP_TEXT_MAX + 1];
	bool present;

	startup_notification_service();
	if (!window || !startup_atoms[STARTUP_ID] ||
	    !startup_atoms[STARTUP_UTF8] ||
	    !startup_notification_pending(&startup_activity))
		return true;
	(void)startup_id(FW_W(window), id, &present);
	if (!present && window->wmhints &&
	    (window->wmhints->flags & WindowGroupHint) &&
	    window->wmhints->window_group != None &&
	    window->wmhints->window_group != Scr.Root &&
	    window->wmhints->window_group != FW_W(window) &&
	    startup_group_target(window->wmhints->window_group))
	{
		struct startup_group_binding *binding = startup_group_find_binding(
			eventmask_startup_groups(), FW_W(window));
		struct startup_group_leader *leader = startup_group_find_leader(
			eventmask_startup_groups(), window->wmhints->window_group);

		if (!inherit_group)
		{
			/* Borrowed dispatch may read the direct ID. Inheritance and
			 * its class fallback consume outer work only. */
			startup_group_defer(eventmask_startup_groups(), FW_W(window),
				window->palette_identity);
			return true;
		}
		if (binding != NULL && binding->identity == window->palette_identity &&
		    binding->invalidated)
			present = true;
		else if (binding != NULL &&
		    binding->identity == window->palette_identity &&
		    binding->leader == window->wmhints->window_group &&
		    leader != NULL && leader->acquired)
		{
			if (startup_id(window->wmhints->window_group, id, &present) ==
			    FPROPERTY_X_ERROR)
			{
				/* No mask restoration is safe after the server reports that
				 * this inherited-property target is gone. */
				startup_group_abandon(eventmask_startup_groups(),
					window->wmhints->window_group);
				startup_group_dropped();
				return true;
			}
			/* The property reply orders earlier lifecycle events into the
			 * local queue. Inspect a bounded prefix without dispatching it. */
			FQueuedDestroyResult queued = FCheckQueuedWindowDestroy(dpy,
				(Window)leader->window, (unsigned long)leader->serial);

			if (queued == FEV_QUEUED_DESTROY_MATCH)
			{
				startup_group_abandon(eventmask_startup_groups(),
					window->wmhints->window_group);
				return true;
			}
			if (queued == FEV_QUEUED_DESTROY_INCOMPLETE)
			{
				startup_group_defer(eventmask_startup_groups(),
					FW_W(window), window->palette_identity);
				return false;
			}
		}
		else
			present = true;
	}
	/* Inheritance is an effective observation, never a copied property.
	 * Group notifications queue bounded, identity-checked outer work. */
	const char *completed = startup_notification_observe(
		&startup_activity, id, present, window->class.res_name,
		window->class.res_class, FW_W(window), startup_now());

	if (completed && !present && !startup_remove_legacy(completed))
	{
		if (startup_activity.dropped != UINT64_MAX)
			startup_activity.dropped++;
		if (startup_activity.generation)
			startup_activity.generation++;
	}
	startup_group_pending(eventmask_startup_groups(),
		startup_notification_pending(&startup_activity), false);
	return true;
}

void startup_notification_window_observed(FvwmWindow *window)
{
	(void)startup_observe_window(window, false);
}

void startup_notification_property(FvwmWindow *window, Atom property)
{
	if (property == startup_atoms[STARTUP_ID])
		startup_notification_window_observed(window);
}

cJSON *startup_notification_query(const struct fvwm_query_spec *spec,
	const char **error)
{
	char generation[MODULE_QUERY_GENERATION_LEN + 1];
	cJSON *result = NULL, *rows, *counters, *limits;
	unsigned int total = 0, skipped = 0, count = 0;
	unsigned int limit = spec->limit > 8 ? 8 : spec->limit;
	uint64_t now = startup_now();

	startup_notification_expire(&startup_activity, now);
	startup_group_pending(eventmask_startup_groups(),
		startup_notification_pending(&startup_activity), false);
	*error = "invalid_request";
	if (!limit || spec->limit > MODULE_QUERY_PAGE_MAX)
		return NULL;
	*error = "generation_unavailable";
	if (!fvwm_query_token(startup_activity.generation, generation))
		return NULL;
	*error = "stale_generation";
	if ((spec->offset && strcmp(spec->generation, "-") == 0) ||
	    (strcmp(spec->generation, "-") != 0 &&
	    strcmp(spec->generation, generation) != 0))
		return NULL;
	for (unsigned int i = 0; i < STARTUP_RECORD_MAX; i++)
		if (startup_activity.records[i].visible)
			total++;
	*error = "invalid_offset";
	if (spec->offset > total)
		return NULL;
	*error = "allocation_failed";
	result = cJSON_CreateObject();
	if (!result)
		return NULL;
	rows = cJSON_AddArrayToObject(result, "startups");
	counters = cJSON_AddObjectToObject(result, "counters");
	limits = cJSON_AddObjectToObject(result, "limits");
	if (!rows || !counters || !limits ||
	    !cJSON_AddStringToObject(result, "state", "observed") ||
	    !cJSON_AddStringToObject(result, "scope",
		"core-startup-activity") ||
	    !cJSON_AddStringToObject(result, "generation", generation) ||
	    !cJSON_AddNumberToObject(result, "offset", spec->offset) ||
	    !cJSON_AddNumberToObject(result, "total", total) ||
	    !cJSON_AddNumberToObject(result, "limit", limit) ||
	    !cJSON_AddBoolToObject(result, "more",
		spec->offset + limit < total) ||
	    !cJSON_AddNumberToObject(counters, "dropped",
		startup_activity.dropped) ||
	    !cJSON_AddNumberToObject(counters, "malformed",
		startup_activity.malformed) ||
	    !cJSON_AddNumberToObject(counters, "partial_expired",
		startup_activity.partial_expired) ||
	    !cJSON_AddNumberToObject(limits, "records", STARTUP_RECORD_MAX) ||
	    !cJSON_AddNumberToObject(limits, "streams", STARTUP_STREAM_MAX) ||
	    !cJSON_AddNumberToObject(limits, "message_bytes",
		STARTUP_MESSAGE_MAX) ||
	    !cJSON_AddNumberToObject(limits, "partial_ms",
		STARTUP_PARTIAL_MS) ||
	    !cJSON_AddNumberToObject(limits, "pending_ms",
		STARTUP_PENDING_MS) ||
	    !cJSON_AddNumberToObject(limits, "retention_ms",
		STARTUP_RETENTION_MS))
		goto fail;
	for (unsigned int i = 0; i < STARTUP_RECORD_MAX && count < limit; i++)
	{
		const struct startup_record *record =
			startup_activity.records + i;
		cJSON *row;

		if (!record->visible)
			continue;
		if (skipped++ < spec->offset)
			continue;
		row = cJSON_CreateObject();
		if (!row)
			goto fail;
		if (!cJSON_AddItemToArray(rows, row))
		{
			cJSON_Delete(row);
			goto fail;
		}
		if (!cJSON_AddStringToObject(row, "id", record->id) ||
		    !cJSON_AddStringToObject(row, "name", record->name) ||
		    !cJSON_AddStringToObject(row, "icon", record->icon) ||
		    !cJSON_AddStringToObject(row, "application_id",
			record->application_id) ||
		    !cJSON_AddStringToObject(row, "wmclass", record->wmclass) ||
		    !cJSON_AddStringToObject(row, "state",
			startup_notification_state_name(record->state)) ||
		    !cJSON_AddNumberToObject(row, "window", record->window) ||
		    !cJSON_AddNumberToObject(row, "age_ms",
			now >= record->started_ms ?
			now - record->started_ms : 0))
			goto fail;
		count++;
	}
	if (!(spec->offset + count < total ?
	    cJSON_AddNumberToObject(result, "next_offset",
		spec->offset + count) :
	    cJSON_AddNullToObject(result, "next_offset")))
		goto fail;
	*error = NULL;
	return result;
fail:
	cJSON_Delete(result);
	return NULL;
}
