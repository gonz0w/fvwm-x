/* This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or any later version.
 */
#include "config.h"
#include "root_background.h"
#include "image_async.h"
#include "colorset.h"
#include "fvwm.h"
#include "execcontext.h"
#include "externs.h"
#include "libs/picture_types.h"
#include "screen.h"
#include "query_json.h"
#include "events.h"
#include "preview_policy.h"
#include "topology_event_adapter.h"
#include "libs/PictureUtils.h"
#include "libs/safemalloc.h"
#include "libs/log.h"
#include <string.h>
#include <stdlib.h>
#include <X11/Xatom.h>

struct root_background
{
	FvwmPicture *picture;
	struct background_plan source;
	unsigned width, height;
	uint64_t topology, source_revision;
	bool ready, failed;
};

static struct root_background *current_root, *pending_root;
static bool selection_pending, took_over, current_valid;
static Atom root_atom, esetroot_atom, xsetroot_atom;
static bool publication_grabbed;
static struct root_background *staged_root, *retired_root;
static uint64_t notification_ordinal;
/* Publication and immediate recovery can precede event dispatch. This finite
 * FIFO attributes each own notification once, even if external events carry
 * the same last-processed request serial. Never suppress a value-only match. */
static struct {
	unsigned long serial;
	uint64_t ordinal;
} expected[4];
static unsigned expected_count;

static uint64_t root_topology(void)
{
	const monitor_topology_snapshot *snapshot = FScreenGetTopologySnapshot();

	return snapshot ? snapshot->epoch : 0;
}

static void root_atoms(void)
{
	if (!root_atom)
	{
		root_atom = XInternAtom(dpy, "_XROOTPMAP_ID", False);
		esetroot_atom = XInternAtom(dpy, "ESETROOT_PMAP_ID", False);
		xsetroot_atom = XInternAtom(dpy, "_XSETROOT_ID", False);
	}
}

static bool root_property(Pixmap pixmap)
{
	Atom type;
	int format;
	unsigned long count, after;
	unsigned char *bytes = NULL;
	bool matches;

	root_atoms();
	matches = XGetWindowProperty(dpy, Scr.Root, root_atom, 0, 1, False,
		XA_PIXMAP, &type, &format, &count, &after, &bytes) == Success &&
		type == XA_PIXMAP && format == 32 && count == 1 && after == 0 &&
		bytes && *(unsigned long *)bytes == pixmap;
	if (bytes)
		XFree(bytes);
	return matches;
}

static bool root_size(const struct root_background *root)
{
	return root && !events_topology_pending() &&
		root->width == (unsigned)monitor_get_all_widths() &&
		root->height == (unsigned)monitor_get_all_heights() &&
		root->topology == root_topology();
}

bool root_background_owned(void)
{
	return current_valid && root_background_ready(current_root) &&
		root_size(current_root);
}

bool root_background_validate(void)
{
	if (!root_background_owned() ||
	    !root_property(current_root->picture->picture))
	{
		current_valid = false;
		return false;
	}
	return true;
}

bool root_background_current(const struct root_background *root)
{
	return root && current_root && root_size(root) &&
		root_background_owned() &&
		root->picture->picture == current_root->picture->picture;
}

FvwmPicture *root_background_picture(const struct root_background *root)
{
	return root_background_ready(root) ? root->picture : NULL;
}

bool root_background_ready(const struct root_background *root)
{
	return root && root->ready && !root->failed &&
		picture_is_usable(root->picture) && !root->picture->pending;
}

bool root_background_failed(const struct root_background *root)
{
	return !root || root->failed || !root_size(root);
}

void root_background_release(struct root_background *root)
{
	if (!root)
		return;
	PDestroyFvwmPicture(dpy, root->picture);
	free(root);
}

static struct root_background *root_clone(const struct root_background *root)
{
	struct root_background *copy = fxmalloc(sizeof(*copy));

	*copy = *root;
	copy->picture = picture_snapshot(root->picture);
	return copy;
}

struct root_background *root_background_capture(void)
{
	return root_background_validate() ? root_clone(current_root) : NULL;
}

static picture_async_commit root_policy(FvwmPicture *view, void *context)
{
	struct root_background *root = context;

	return root_size(root) && root->picture == view ?
		PICTURE_ASYNC_APPLY : PICTURE_ASYNC_DISCARD;
}

static void root_settled(FvwmPicture *view, Bool applied, void *context)
{
	struct root_background *root = context;

	(void)view;
	/* Private settlement only. Publication belongs to the outer owner. */
	root->ready = applied;
	root->failed = !applied;
}

struct root_background *root_background_prepare(
	const struct background_plan *plan)
{
	struct root_background *root;

	if (Pdepth != (unsigned)DefaultDepth(dpy, DefaultScreen(dpy)))
		return NULL;
	if ((plan->flags & BACKGROUND_MODE_MASK) == 0)
		return root_background_capture();
	root = fxcalloc(1, sizeof(*root));
	root->source = *plan;
	root->source.flags &= BACKGROUND_MODE_MASK;
	root->source.menu_opacity = root->source.launcher_opacity = 0;
	root->width = monitor_get_all_widths();
	root->height = monitor_get_all_heights();
	root->topology = root_topology();
	root->source_revision = colorset_root_revision();
	root->picture = image_async_background(&root->source,
		root->width, root->height);
	if (!root->picture || !picture_async_bind(root->picture,
		root_policy, root_settled, root, NULL))
	{
		root_background_release(root);
		return NULL;
	}
	return root;
}

bool root_background_publishable(const struct root_background *root)
{
	return root_background_ready(root) && root_size(root) &&
		notification_ordinal < UINT64_MAX - expected_count - 1;
}

static bool root_expected(const XPropertyEvent *event, unsigned index,
	uint64_t ordinal)
{
	return index < expected_count && preview_root_notice_matches(
		event->serial, ordinal, expected[index].serial,
		expected[index].ordinal, event->send_event,
		event->state == PropertyNewValue);
}

static enum preview_publication_result root_pending_clean(bool initial,
	unsigned long fence)
{
	XEvent queued[PREVIEW_PUBLICATION_QUEUE_MAX];
	bool drop[PREVIEW_PUBLICATION_QUEUE_MAX] = {false};
	int count = XEventsQueued(dpy, QueuedAlready);
	unsigned own = 0;
	uint64_t ordinal = notification_ordinal;
	bool clean = true;

	if (count < 0)
		return PREVIEW_PUBLICATION_CONFLICT;
	enum preview_publication_result result =
		preview_publication_queue_check((size_t)count);

	if (result != PREVIEW_PUBLICATION_READY)
		return result;
	/* The synchronous bounded property/geometry replies precede this
	 * snapshot. XNextEvent cannot wait: only these already queued events
	 * are inspected, and no callback or dispatcher runs under the grab.
	 * Put every retained event back in its original order. */
	for (int i = 0; i < count; i++)
	{
		XNextEvent(dpy, &queued[i]);
		fvwm_topology_event_input input = {.event_type = queued[i].type,
			.randr_event_base = randr_event,
			.randr_subtype = queued[i].type == randr_event + RRNotify ?
				((XRRNotifyEvent *)&queued[i])->subtype : 0,
			.event_window = queued[i].type == ConfigureNotify ?
				queued[i].xconfigure.window : queued[i].xany.window,
			.root_window = Scr.Root};

		if (fvwm_topology_event_classify(&input) !=
		    FVWM_TOPOLOGY_EVENT_REJECTED)
			clean = false;
		if (queued[i].type != PropertyNotify ||
		    queued[i].xproperty.window != Scr.Root ||
		    queued[i].xproperty.atom != root_atom)
			continue;
		if (ordinal == UINT64_MAX)
		{
			clean = false;
			continue;
		}
		ordinal++;
		if (initial && !queued[i].xproperty.send_event &&
		    queued[i].xproperty.serial <= fence)
			drop[i] = true;
		else if (root_expected(&queued[i].xproperty, own, ordinal))
			own++;
		else
			clean = false;
	}
	if ((!initial && own != expected_count) ||
	    ordinal >= UINT64_MAX - expected_count - 1)
		clean = false;
	for (int i = count; i-- > 0;)
		if (!clean || !drop[i])
			XPutBackEvent(dpy, &queued[i]);
	/* Only the initial explicit takeover can supersede older genuine root
	 * events. Ongoing transactions refuse even an identical-value write.
	 * Expected own events remain queued for normal redraw/broadcast work. */
	if (clean && initial)
		notification_ordinal = ordinal;
	return clean ? PREVIEW_PUBLICATION_READY : PREVIEW_PUBLICATION_CONFLICT;
}

void root_background_end_move(void)
{
	if (publication_grabbed)
	{
		XUngrabServer(dpy);
		publication_grabbed = false;
	}
	/* Neither retained pictures nor canceled reader jobs are retired under
	 * the server grab. Their independent owners keep worker inputs alive. */
	root_background_release(staged_root);
	root_background_release(retired_root);
	staged_root = retired_root = NULL;
}

static enum preview_publication_result root_begin_move(
	struct root_background *target, bool ordinary)
{
	bool initial = ordinary && !took_over;
	Window root;
	int x, y;
	unsigned width, height, border, depth;
	bool matches;

	if (publication_grabbed || (target && !root_background_publishable(target)) ||
	    (!ordinary && !root_background_owned()))
		return PREVIEW_PUBLICATION_CONFLICT;
	root_atoms();
	if (target)
		staged_root = root_clone(target);
	XGrabServer(dpy);
	publication_grabbed = true;
	unsigned long fence = NextRequest(dpy);

	matches = root_property(current_root ? current_root->picture->picture : None);
	/* Check actual root dimensions as well as the captured topology epoch.
	 * A queued RandR/root-configure event also refuses publication below. */
	struct root_background *dimensions = target ? target : current_root;

	if ((!initial && (!current_valid || !current_root || !matches)) ||
	    !dimensions || !XGetGeometry(dpy, Scr.Root, &root, &x, &y,
		&width, &height, &border, &depth) ||
	    dimensions->width != width || dimensions->height != height ||
	    dimensions->picture->depth != depth || !root_size(dimensions))
	{
		root_background_end_move();
		return PREVIEW_PUBLICATION_CONFLICT;
	}
	enum preview_publication_result result = root_pending_clean(initial, fence);

	if (result == PREVIEW_PUBLICATION_READY && target &&
	    expected_count == sizeof(expected) / sizeof(*expected))
		result = PREVIEW_PUBLICATION_RETRY;
	if (result != PREVIEW_PUBLICATION_READY)
		root_background_end_move();
	return result;
}

enum preview_publication_result root_background_begin_move(
	struct root_background *target)
{
	return root_begin_move(target, false);
}

enum preview_publication_result root_background_publish(
	struct root_background *root, bool dependents)
{
	bool local_grab = !publication_grabbed;
	enum preview_publication_result result = PREVIEW_PUBLICATION_READY;

	if (local_grab)
		result = root_begin_move(root, true);
	if (result != PREVIEW_PUBLICATION_READY)
		return result;
	/* begin validated and staged this exact immutable picture. From here
	 * through end there are only fixed record moves and root requests. */
	if (!staged_root || staged_root->picture->picture != root->picture->picture)
	{
		if (local_grab)
			root_background_end_move();
		return PREVIEW_PUBLICATION_CONFLICT;
	}
	if (!took_over)
	{
		/* Remove predecessor kill markers without reclaiming its client.
		 * A core-owned XID is published only in _XROOTPMAP_ID. */
		XDeleteProperty(dpy, Scr.Root, esetroot_atom);
		XDeleteProperty(dpy, Scr.Root, xsetroot_atom);
		took_over = true;
	}
	Pixmap pixmap = root->picture->picture;

	retired_root = current_root;
	current_root = staged_root;
	staged_root = NULL;
	current_valid = true;
	XSetWindowBackgroundPixmap(dpy, Scr.Root, pixmap);
	expected[expected_count].serial = NextRequest(dpy);
	expected[expected_count].ordinal = notification_ordinal + expected_count + 1;
	expected_count++;
	XChangeProperty(dpy, Scr.Root, root_atom, XA_PIXMAP, 32,
		PropModeReplace, (unsigned char *)&pixmap, 1);
	XClearWindow(dpy, Scr.Root);
	colorset_root_owned_publish(current_root->picture, false);
	if (local_grab)
		root_background_end_move();
	if (dependents)
		colorset_root_owned_refresh();
	return PREVIEW_PUBLICATION_READY;
}

bool root_background_notification(const XPropertyEvent *event)
{
	if (event->window != Scr.Root || event->atom != root_atom)
		return false;
	if (notification_ordinal == UINT64_MAX)
	{
		current_valid = false;
		fvwm_query_appearance_changed();
		return false;
	}
	notification_ordinal++;
	if (root_expected(event, 0, notification_ordinal))
	{
		memmove(expected, expected + 1,
			--expected_count * sizeof(*expected));
		return true;
	}
	/* External changes invalidate even a retained transaction-owned root.
	 * Its references stay alive but can no longer authorize restoration. */
	current_valid = false;
	fvwm_query_appearance_changed();
	return false;
}

void root_background_select(void)
{
	selection_pending = true;
}

bool root_background_outer_pending(void)
{
	return selection_pending || (pending_root &&
		(pending_root->ready || pending_root->failed));
}

static bool root_source_equal(const struct background_plan *left,
	const struct background_plan *right)
{
	return (left->flags & BACKGROUND_MODE_MASK) ==
		(right->flags & BACKGROUND_MODE_MASK) && left->rgb == right->rgb &&
		strcmp(left->path, right->path) == 0;
}

void root_background_outer_boundary(void)
{
	struct background_plan plan;

	if (events_topology_pending())
		return;
	if (current_root && !root_size(current_root))
		selection_pending = true;
	if (selection_pending)
	{
		selection_pending = false;
		const colorset_t *background = FindColorset(20);

		if (root_size(current_root) && background &&
		    current_root->source_revision == colorset_root_revision() &&
		    background->pixmap == current_root->picture->picture)
			return;
		if (!colorset_root_plan(&plan))
		{
			fvwm_debug(__func__, "CoreOwnedPixmap colorset20 unsupported");
			return;
		}
		if (root_size(current_root) &&
		    current_root->source_revision == colorset_root_revision() &&
		    root_source_equal(&current_root->source, &plan))
			return;
		if (!pending_root || !root_size(pending_root) ||
		    pending_root->source_revision != colorset_root_revision() ||
		    !root_source_equal(&pending_root->source, &plan))
		{
			root_background_release(pending_root);
			pending_root = root_background_prepare(&plan);
		}
	}
	if (pending_root && root_background_failed(pending_root))
	{
		fvwm_debug(__func__, "CoreOwnedPixmap preparation failed");
		root_background_release(pending_root);
		pending_root = NULL;
	}
	if (root_background_ready(pending_root))
	{
		/* Do not publish a preparation overtaken by another source. */
		if (colorset_root_plan(&plan) &&
		    pending_root->source_revision == colorset_root_revision() &&
		    root_source_equal(&pending_root->source, &plan))
		{
			enum preview_publication_result result =
				root_background_publish(pending_root, true);

			/* Keep the one ready candidate while ordinary dispatch drains
			 * queue pressure. The result belongs only to this attempt. */
			if (result == PREVIEW_PUBLICATION_RETRY)
				return;
			if (result == PREVIEW_PUBLICATION_CONFLICT)
				fvwm_debug(__func__, "CoreOwnedPixmap publication refused");
		}
		root_background_release(pending_root);
		pending_root = NULL;
	}
}

void root_background_shutdown(void)
{
	if (current_root && root_property(current_root->picture->picture))
		XDeleteProperty(dpy, Scr.Root, root_atom);
	root_background_release(pending_root);
	root_background_release(current_root);
	pending_root = current_root = NULL;
}
