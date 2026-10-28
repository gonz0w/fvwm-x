/* This program is free software; you can redistribute it and/or modify it
 * under the GNU General Public License, version 2 or any later version. */
#include "config.h"
#include "pinboard_policy.h"
#include "libs/Module.h"
#include "libs/module_packet_reader.h"
#include "libs/module_x_service.h"
#include "libs/FShape.h"
#include "libs/Picture.h"
#include "libs/picture_types.h"
#include "libs/PictureGraphics.h"
#include "libs/PictureUtils.h"
#include "libs/flocale_font_types.h"
#include "libs/Colorset.h"
#include "libs/Parse.h"
#include "libs/safemalloc.h"
#include "libs/log.h"
#include <X11/keysym.h>
#include <errno.h>
#include <inttypes.h>
#include <limits.h>
#include <poll.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <time.h>
#include <unistd.h>

#define PINBOARD_MANAGE PINBOARD_MAX
#define PINBOARD_CLIENT_LIMIT 512

typedef struct
{
	FlocaleFont *font;
	FvwmPicture *picture;
} pinboard_resource;

typedef struct
{
	Display *display;
	Window root, window;
	Atom selection, type, desktop, clients, timestamp_atom;
	Window watched[PINBOARD_CLIENT_LIMIT];
	unsigned long watched_serial[PINBOARD_CLIENT_LIMIT];
	unsigned int watched_count;
	int channel[2], colorset, width, height;
	ModuleArgs *module;
	module_packet_reader reader;
	pinboard_model model;
	module_ui_scale_receiver monitors;
	module_workarea_state workareas;
	pinboard_workspace workspace;
	bool config_pending, workspace_refresh;
	pinboard_projection projection;
	pinboard_resource resources[PINBOARD_MAX + 1];
	char font_name[257];
	GC gc;
	bool mapped, map_requested, claimed, stopped, unavailable, refused,
	    dirty;
	bool commit_waiting, moving, pointer_down, dragged, layout_notify;
	unsigned int selected, gesture_view;
	unsigned int last_click_index;
	unsigned long input_serial;
	uint64_t gesture_epoch;
	pinboard_cell preview;
	int press_x, press_y;
	Time last_click;
	double started, lease, retiring;
} pinboard;

static volatile sig_atomic_t pinboard_stopping;
static Window pinboard_borrowed_window;
static unsigned char pinboard_borrowed_request;
static bool pinboard_borrowed_gone, pinboard_x_failed;

static void request_config(pinboard *board);

static double seconds_now(void)
{
	struct timespec value;
	if (clock_gettime(CLOCK_MONOTONIC, &value))
		return 0;
	return value.tv_sec + value.tv_nsec / 1000000000.0;
}

static void stopped(int signal_number)
{
	(void)signal_number;
	pinboard_stopping = 1;
}

static int x_error(Display *display, XErrorEvent *event)
{
	(void)display;
	if (event->error_code == BadWindow &&
	    event->request_code == pinboard_borrowed_request &&
	    event->resourceid == pinboard_borrowed_window &&
	    pinboard_borrowed_window)
		pinboard_borrowed_gone = true;
	else
		pinboard_x_failed = true;
	return 0;
}

static void send_command(pinboard *board, const char *format, ...)
{
	char text[PINBOARD_LINE + 1];
	va_list args;
	va_start(args, format);
	int length = vsnprintf(text, sizeof(text), format, args);
	va_end(args);
	if (length < 0 || length > PINBOARD_LINE)
	{
		board->stopped = true;
		return;
	}
	SendText(board->channel, text, 0);
}

static const char *state_name(const pinboard *board)
{
	if (!board->monitors.published.epoch)
		return "unavailable";
	if (board->retiring || board->stopped)
		return "retired";
	if (board->refused || board->model.poisoned)
		return "refused";
	if (board->unavailable)
		return "unavailable";
	if (board->model.staging || !board->mapped)
		return "pending";
	return "ready";
}

static void status_reply(pinboard *board, const char *tag)
{
	send_command(
	    board, "Echo FVWM_PINBOARD_STATUS %s %s %u %u %s %lu %016" PRIx64,
	    board->model.owner, tag, board->model.current.generation,
	    board->model.staging ? board->model.pending.generation : 0,
	    state_name(board), board->window, board->monitors.published.epoch);
}

static void event_reply(pinboard *board, const char *verb, unsigned int index,
			const pinboard_cell *move)
{
	char hex[513] = "-";
	if (!board->model.current.generation || !board->projection.epoch)
		return;
	if (move && move->monitor[0])
		pinboard_encode(move->monitor, hex);
	send_command(board,
		     "Echo FVWM_PINBOARD_EVENT %s %u %016" PRIx64
		     " %s %s %s %u %u",
		     board->model.owner, board->model.current.generation,
		     board->projection.epoch, verb,
		     index < board->model.current.count
			 ? board->model.current.entries[index].id
			 : "-",
		     hex, move ? move->column : 0, move ? move->row : 0);
}

static void release_resources(pinboard *board, pinboard_resource *resources)
{
	for (unsigned int i = 0; i <= PINBOARD_MAX; i++)
	{
		if (resources[i].picture)
			PDestroyFvwmPicture(board->display,
					    resources[i].picture);
		if (resources[i].font)
			FlocaleUnloadFont(board->display, resources[i].font);
		resources[i] = (pinboard_resource){0};
	}
}

static const pinboard_cell *cell_for(const pinboard *board, unsigned int index)
{
	return index == PINBOARD_MANAGE ? &board->projection.manage
	       : index < board->model.current.count
		   ? &board->projection.cells[index]
		   : NULL;
}

static void draw_cell(pinboard *board, unsigned int index,
		      const pinboard_cell *cell)
{
	pinboard_resource *resource = &board->resources[index];
	colorset_t *colors = GetColorset(board->colorset);
	const char *label = index == PINBOARD_MANAGE
				? "Manage pins"
				: board->model.current.entries[index].label;
	FlocaleWinString string = {0};
	XRectangle clip;
	int icon = 0;
	if (!cell || !cell->visible)
		return;
	clip = (XRectangle){cell->x, cell->y, cell->width, cell->height};
	XSetClipRectangles(board->display, board->gc, 0, 0, &clip, 1, Unsorted);
	XSetForeground(board->display, board->gc, colors->bg);
	XFillRectangle(board->display, board->window, board->gc, cell->x,
		       cell->y, cell->width, cell->height);
	XSetForeground(board->display, board->gc, colors->fg);
	if (resource->picture)
	{
		FvwmPicture *picture = resource->picture;
		icon = picture->height;
		pinboard_blit copy;
		if (pinboard_picture_clip(cell, picture->width, picture->height,
					  &copy))
			PGraphicsCopyFvwmPicture(
			    board->display, picture, board->window, board->gc,
			    copy.source_x, copy.source_y, copy.width,
			    copy.height, copy.destination_x,
			    copy.destination_y);
		/* Picture compositing owns its own clip; restore our cell clip.
		 */
		XSetClipRectangles(board->display, board->gc, 0, 0, &clip, 1,
				   Unsorted);
	}
	if (!*label)
		label = "Pin";
	if (resource->font)
	{
		FlocaleFont *font = resource->font;
		if (font->font)
			XSetFont(board->display, board->gc, font->font->fid);
		string.str = (char *)label;
		string.len = strlen(label);
		string.gc = board->gc;
		string.win = board->window;
		string.colorset = colors;
		string.flags.has_colorset = 1;
		string.x = cell->x + 4;
		string.y = cell->y + icon + 8 + font->ascent;
		/* Clip the text, including the Xft/Flocale path, to this cell.
		 */
		string.clip_region = XCreateRegion();
		string.flags.has_clip_region = 1;
		XUnionRectWithRegion(&clip, string.clip_region,
				     string.clip_region);
		FlocaleDrawString(board->display, font, &string,
				  FWS_HAVE_LENGTH);
		XDestroyRegion(string.clip_region);
		if (index < board->model.current.count &&
		    board->model.current.entries[index].missing)
		{
			string.str = "Missing target";
			string.len = strlen(string.str);
			string.y += font->height;
			string.clip_region = XCreateRegion();
			XUnionRectWithRegion(&clip, string.clip_region,
					     string.clip_region);
			FlocaleDrawString(board->display, font, &string,
					  FWS_HAVE_LENGTH);
			XDestroyRegion(string.clip_region);
		}
	}
	if (board->selected == index && cell->width > 4 && cell->height > 4)
		XDrawRectangle(board->display, board->window, board->gc,
			       cell->x + 1, cell->y + 1, cell->width - 3,
			       cell->height - 3);
	XSetClipMask(board->display, board->gc, None);
}

static void render(pinboard *board)
{
	XRectangle shapes[PINBOARD_MAX + 1];
	int count = 0;
	bool preview = board->moving &&
		board->selected < board->model.current.count;
	if (!board->window || !board->projection.epoch)
		return;
	for (unsigned int i = 0; i <= board->model.current.count; i++)
	{
		unsigned int index =
		    i == board->model.current.count ? PINBOARD_MANAGE : i;
		const pinboard_cell *cell =
		    preview && index == board->selected
			? &board->preview
			: cell_for(board, index);
		if (cell && cell->visible)
			shapes[count++] = (XRectangle){
			    cell->x, cell->y, cell->width, cell->height};
	}
	FShapeCombineRectangles(board->display, board->window, FShapeBounding,
				0, 0, shapes, count, FShapeSet, Unsorted);
	for (unsigned int i = 0; i < board->model.current.count; i++)
		if (!preview || i != board->selected)
			draw_cell(board, i, &board->projection.cells[i]);
	/* Preview input uses the same stacking: selected preview above pins,
	 * with the management control always painted and reachable above it. */
	if (preview)
		draw_cell(board, board->selected, &board->preview);
	draw_cell(board, PINBOARD_MANAGE, &board->projection.manage);
	XFlush(board->display);
}

static bool prepare_resources(pinboard *board, const pinboard_scene *scene,
			      const pinboard_projection *projection,
			      pinboard_resource *resources)
{
	for (unsigned int i = 0; i <= scene->count; i++)
	{
		unsigned int index = i == scene->count ? PINBOARD_MANAGE : i;
		const pinboard_cell *cell = index == PINBOARD_MANAGE
						? &projection->manage
						: &projection->cells[i];
		if (!cell->visible)
			continue;
		resources[index].font = FlocaleLoadFontAtScale(
		    board->display, board->font_name, board->module->name,
		    (monitor_scale_factor){cell->quarters}, 0);
		if (!resources[index].font)
			resources[index].font = FlocaleLoadFontAtScale(
			    board->display, "fixed", board->module->name,
			    (monitor_scale_factor){cell->quarters}, 0);
		if (!resources[index].font)
			return false;
		if (index < scene->count && scene->entries[i].picture[0])
		{
			int target;
			if (monitor_scale_apply(
				scene->layout.icon,
				(monitor_scale_factor){cell->quarters},
				&target) != MONITOR_SCALE_CONVERSION_OK)
				return false;
			resources[index].picture = PCacheFvwmPictureAtScale(
			    board->display, board->window, NULL,
			    scene->entries[i].picture,
			    (FvwmPictureAttributes){0},
			    (monitor_scale_factor){cell->quarters}, target,
			    target, scene->generation);
		}
	}
	return true;
}

/* Subscriptions are owned by this X connection, bounded by the current
 * published client list, and never retained as a window-lifetime reference. */
static unsigned int watched_index(const pinboard *board, Window window)
{
	for (unsigned int i = 0; i < board->watched_count; i++)
		if (board->watched[i] == window)
			return i;
	return board->watched_count;
}

static void forget_watched(pinboard *board, Window window)
{
	unsigned int index = watched_index(board, window);
	if (index == board->watched_count)
		return;
	board->watched_count--;
	memmove(&board->watched[index], &board->watched[index + 1],
		(board->watched_count - index) * sizeof(board->watched[0]));
	memmove(
	    &board->watched_serial[index], &board->watched_serial[index + 1],
	    (board->watched_count - index) * sizeof(board->watched_serial[0]));
}

static bool select_borrowed(pinboard *board, Window window, long mask,
			    unsigned long *serial)
{
	XSync(board->display, False);
	pinboard_borrowed_window = window;
	pinboard_borrowed_request = X_ChangeWindowAttributes;
	pinboard_borrowed_gone = false;
	if (serial)
		*serial = NextRequest(board->display);
	XSelectInput(board->display, window, mask);
	XSync(board->display, False);
	pinboard_borrowed_window = None;
	pinboard_borrowed_request = 0;
	return !pinboard_borrowed_gone;
}

static bool borrowed_desktop(pinboard *board, Window window)
{
	unsigned char *types = NULL;
	unsigned long size = 0, after = 0;
	Atom type = None;
	int form = 0;
	bool conflict = false;
	/* Drain earlier errors before admitting exactly this borrowed read. */
	XSync(board->display, False);
	pinboard_borrowed_window = window;
	pinboard_borrowed_request = X_GetProperty;
	pinboard_borrowed_gone = false;
	int status = XGetWindowProperty(board->display, window, board->type, 0,
					16, False, XA_ATOM, &type, &form, &size,
					&after, &types);
	XSync(board->display, False);
	pinboard_borrowed_window = None;
	pinboard_borrowed_request = 0;
	if (pinboard_borrowed_gone)
		forget_watched(board, window);
	else if (status == Success && type == XA_ATOM && form == 32)
	{
		if (after)
			conflict = true;
		for (unsigned long j = 0; j < size; j++)
			if (((unsigned long *)types)[j] == board->desktop)
				conflict = true;
	}
	if (types)
		XFree(types);
	return conflict;
}

static bool other_desktop(pinboard *board)
{
	Atom actual;
	int format;
	unsigned long count, remaining;
	unsigned char *data = NULL;
	Window desired[PINBOARD_CLIENT_LIMIT];
	unsigned int desired_count = 0;
	bool conflict = false;
	if (XGetWindowProperty(board->display, board->root, board->clients, 0,
			       PINBOARD_CLIENT_LIMIT, False, XA_WINDOW, &actual,
			       &format, &count, &remaining, &data) != Success)
		return true;
	if (remaining || count > PINBOARD_CLIENT_LIMIT ||
	    (actual != None && (actual != XA_WINDOW || format != 32)))
	{
		if (data)
			XFree(data);
		return true;
	}
	for (unsigned long i = 0; i < count; i++)
	{
		Window window = ((unsigned long *)data)[i];
		bool duplicate = false;
		if (!window || window == board->root || window == board->window)
			continue;
		for (unsigned int j = 0; j < desired_count; j++)
			if (desired[j] == window)
				duplicate = true;
		if (!duplicate)
			desired[desired_count++] = window;
	}
	if (data)
		XFree(data);
	/* Remove subscriptions before admitting replacements. A removal racing
	 * destruction is tolerated only for this exact ChangeWindowAttributes.
	 */
	for (unsigned int i = 0; i < board->watched_count;)
	{
		Window window = board->watched[i];
		bool retained = false;
		for (unsigned int j = 0; j < desired_count; j++)
			if (desired[j] == window)
				retained = true;
		if (retained)
			i++;
		else
		{
			select_borrowed(board, window, NoEventMask, NULL);
			forget_watched(board, window);
		}
	}
	for (unsigned int i = 0; i < desired_count; i++)
	{
		Window window = desired[i];
		if (watched_index(board, window) == board->watched_count)
		{
			unsigned long serial;
			if (!select_borrowed(board, window,
					     PropertyChangeMask |
						 StructureNotifyMask,
					     &serial))
				continue;
			board->watched_serial[board->watched_count] = serial;
			board->watched[board->watched_count++] = window;
		}
		if (borrowed_desktop(board, window))
			conflict = true;
	}
	return conflict;
}

static void retire(pinboard *board)
{
	board->moving = board->pointer_down = false;
	if (board->window)
	{
		XUnmapWindow(board->display, board->window);
		/* Window destruction releases only our lease, never its
		 * successor. */
		XDestroyWindow(board->display, board->window);
		board->window = None;
		XSync(board->display, False);
	}
	board->mapped = board->map_requested = board->claimed = false;
	if (!board->retiring)
		board->retiring = seconds_now() + .25;
}

static bool claim(pinboard *board)
{
	Time timestamp = CurrentTime;
	XEvent event;
	unsigned char value = 1;
	XChangeProperty(board->display, board->window, board->timestamp_atom,
			XA_INTEGER, 8, PropModeReplace, &value, 1);
	XSync(board->display, False);
	for (unsigned int i = 0;
	     i < 32 && XCheckWindowEvent(board->display, board->window,
					 PropertyChangeMask, &event);
	     i++)
		if (event.type == PropertyNotify &&
		    event.xproperty.atom == board->timestamp_atom)
			timestamp = event.xproperty.time;
	if (timestamp == CurrentTime || other_desktop(board))
		return false;
	/* Fixed X-only check/claim/verify. No filesystem work or event wait. */
	XGrabServer(board->display);
	if (XGetSelectionOwner(board->display, board->selection) == None)
	{
		XSetSelectionOwner(board->display, board->selection,
				   board->window, timestamp);
		board->claimed =
		    XGetSelectionOwner(board->display, board->selection) ==
		    board->window;
	}
	XUngrabServer(board->display);
	XSync(board->display, False);
	if (board->claimed && other_desktop(board))
	{
		retire(board);
		return false;
	}
	if (board->claimed)
	{
		XEvent announcement = {0};
		announcement.xclient.type = ClientMessage;
		announcement.xclient.window = board->root;
		announcement.xclient.message_type =
		    XInternAtom(board->display, "MANAGER", False);
		announcement.xclient.format = 32;
		announcement.xclient.data.l[0] = timestamp;
		announcement.xclient.data.l[1] = board->selection;
		announcement.xclient.data.l[2] = board->window;
		XSendEvent(board->display, board->root, False,
			   StructureNotifyMask, &announcement);
	}
	return board->claimed;
}

static void publish_scene(pinboard *board, bool commit)
{
	pinboard_projection projection;
	pinboard_resource resources[PINBOARD_MAX + 1] = {{0}};
	const pinboard_scene *scene =
	    commit ? &board->model.pending : &board->model.current;
	if (!board->window || board->retiring || board->refused ||
	    board->unavailable || !board->model.owner[0] ||
	    (!commit && !board->model.current.generation) ||
	    (commit && !pinboard_pending_valid(&board->model)))
		return;
	XWindowAttributes root_attributes;
	if (!XGetWindowAttributes(board->display, board->root,
				  &root_attributes) ||
	    root_attributes.width < 1 || root_attributes.width > 32767 ||
	    root_attributes.height < 1 || root_attributes.height > 32767)
		return;
	if (board->width != root_attributes.width ||
	    board->height != root_attributes.height)
	{
		board->width = root_attributes.width;
		board->height = root_attributes.height;
		if (board->mapped)
			send_command(
			    board, "WindowId %lu ResizeMove %dp %dp 0p 0p",
			    board->window, board->width, board->height);
	}
	if (!pinboard_project(
		scene, &board->monitors.published, &board->workareas,
		&(rectangle){0, 0, board->width, board->height}, &projection))
		return;
	if (!prepare_resources(board, scene, &projection, resources))
	{
		release_resources(board, resources);
		if (commit)
			board->model.poisoned = true;
		return;
	}
	if (!board->claimed && !claim(board))
	{
		release_resources(board, resources);
		board->refused = true;
		return;
	}
	/* New X resources are fully owned before retiring old references. */
	pinboard_resource old[PINBOARD_MAX + 1];
	memcpy(old, board->resources, sizeof(old));
	memcpy(board->resources, resources, sizeof(resources));
	board->projection = projection;
	board->moving = board->pointer_down = false;
	board->last_click = 0;
	if (commit)
	{
		pinboard_publish(&board->model);
		board->commit_waiting = false;
	}
	if (board->selected < PINBOARD_MAX &&
	    (board->selected >= board->model.current.count ||
	     !board->projection.cells[board->selected].visible))
		board->selected = PINBOARD_MANAGE;
	render(board);
	if (!board->mapped && !board->map_requested)
	{
		XMapWindow(board->display, board->window);
		XSync(board->display, False);
		board->map_requested = true;
	}
	/* Inputs queued against the retired scene carry an earlier serial.
	 * XSync orders the complete new shape before this publication accepts
	 * input. */
	XSync(board->display, False);
	board->input_serial = LastKnownRequestProcessed(board->display);
	release_resources(board, old);
	if (board->layout_notify && !commit)
		event_reply(board, "layout", PINBOARD_MANAGE, NULL);
	board->layout_notify = false;
}

static void cells_reply(pinboard *board, const pinboard_request *request)
{
	unsigned int count = 0, total = board->model.current.count;
	for (unsigned int i = request->offset;
	     i < total && count < request->limit; i++, count++)
	{
		const pinboard_cell *cell = &board->projection.cells[i];
		char hex[513] = "-";
		if (cell->monitor[0])
			pinboard_encode(cell->monitor, hex);
		send_command(
		    board,
		    "Echo FVWM_PINBOARD_CELL %s %s %u %016" PRIx64
		    " %u %s %s %u %u %u %d %d %d %d %u",
		    board->model.owner, request->tag,
		    board->model.current.generation, board->projection.epoch, i,
		    board->model.current.entries[i].id, hex, cell->quarters,
		    cell->column, cell->row, cell->x, cell->y, cell->width,
		    cell->height, cell->visible ? 1 : 0);
	}
	send_command(
	    board,
	    "Echo FVWM_PINBOARD_CELLS_END %s %s %u %016" PRIx64 " %u %u %u",
	    board->model.owner, request->tag, board->model.current.generation,
	    board->projection.epoch, request->offset, count, total);
}

static void native_message(pinboard *board, const char *text)
{
	pinboard_request request =
	    pinboard_receive(&board->model, text, board->module->namelen);
	if (request.command == PINBOARD_BEGIN)
		board->commit_waiting = false;
	if (board->dirty)
	{
		board->dirty = false;
		publish_scene(board, board->commit_waiting);
	}
	switch (request.command)
	{
	case PINBOARD_OWNER:
	case PINBOARD_RENEW:
		board->lease = seconds_now() + 15;
		break;
	case PINBOARD_COMMIT:
		board->commit_waiting = true;
		publish_scene(board, true);
		break;
	case PINBOARD_ABORT:
		board->commit_waiting = false;
		break;
	case PINBOARD_STATUS:
		status_reply(board, request.tag);
		break;
	case PINBOARD_CELLS:
		cells_reply(board, &request);
		break;
	case PINBOARD_RETIRE:
		retire(board);
		break;
	case PINBOARD_BAD:
		fvwm_debug(__func__, "%s: refused native record",
			   board->module->name);
		break;
	default:
		break;
	}
}

static void config_message(pinboard *board, char *text)
{
	module_ui_scale_status status =
	    module_ui_scale_receive(&board->monitors, text);
	if (status == MODULE_UI_SCALE_PUBLISHED)
	{
		module_workarea_sync(&board->workareas,
				     &board->monitors.published);
		/* Unsolicited MonitorUI contains no desk/page baseline or
		 * legacy reservations. Seed those through one bounded config
		 * request; its same-epoch replay is UNCHANGED and cannot
		 * request again. */
		if (pinboard_workspace_publication(
			&board->workspace, status, board->config_pending,
			board->model.current.generation != 0))
			request_config(board);
		board->moving = board->pointer_down = false;
		board->dirty = true;
		if (board->mapped)
			board->layout_notify = true;
		return;
	}
	if (status != MODULE_UI_SCALE_IGNORED)
		return;
	module_workarea_state previous = board->workareas;
	if (module_workarea_receive(&board->workareas,
				    &board->monitors.published,
				    text) == MODULE_WORKAREA_ADMITTED)
	{
		bool changed = false;
		for (unsigned int i = 0; i < board->monitors.published.count;
		     i++)
		{
			module_monitor_area old_area, new_area;
			const module_ui_scale_row *row =
			    &board->monitors.published.rows[i];
			bool old_valid = module_workarea_bounds(
			    &previous, &board->monitors.published, row,
			    &old_area);
			bool new_valid = module_workarea_bounds(
			    &board->workareas, &board->monitors.published, row,
			    &new_area);
			if (old_valid != new_valid ||
			    (old_valid && (old_area.x != new_area.x ||
					   old_area.y != new_area.y ||
					   old_area.width != new_area.width ||
					   old_area.height != new_area.height)))
				changed = true;
		}
		if (changed)
		{
			board->dirty = true;
			board->moving = board->pointer_down = false;
			if (board->mapped)
				board->layout_notify = true;
		}
		return;
	}
	if (!strncasecmp(text, "Colorset ", 9))
	{
		if (LoadColorset(text + 8) == board->colorset)
			render(board);
		return;
	}
	char *value;
	pinboard_config option =
	    pinboard_config_parse(text, board->module->name, &value);
	unsigned int number;
	bool changed = false;
	pinboard_layout layout = board->model.current.layout;
	if (option == PINBOARD_CONFIG_FONT)
	{
		char *font = NULL;
		char *tail = GetNextToken(value, &font);
		while (tail && (*tail == ' ' || *tail == '\t'))
			tail++;
		if (font && *font && strlen(font) <= 256 && (!tail || !*tail) &&
		    strcmp(board->font_name, font))
		{
			strcpy(board->font_name, font);
			changed = true;
		}
		free(font);
	}
	else if (option == PINBOARD_CONFIG_COLORSET)
	{
		char *end;
		int colorset;
		if (ParseColorsetId(value, &end, &colorset) && !*end &&
		    colorset != board->colorset && AllocColorset(colorset))
		{
			board->colorset = colorset;
			changed = true;
		}
	}
	else if (option != PINBOARD_CONFIG_NONE &&
		 pinboard_number(value, 512, &number))
	{
		if (option == PINBOARD_CONFIG_WIDTH)
			layout.width = number;
		else if (option == PINBOARD_CONFIG_HEIGHT)
			layout.height = number;
		else if (option == PINBOARD_CONFIG_ICON)
			layout.icon = number;
		if (pinboard_layout_valid(layout) &&
		    !board->model.current.generation &&
		    (layout.width != board->model.current.layout.width ||
		     layout.height != board->model.current.layout.height ||
		     layout.icon != board->model.current.layout.icon))
		{
			board->model.current.layout = layout;
			changed = true;
		}
	}
	if (changed)
		board->dirty = true;
}

static unsigned int hit_cell(const pinboard *board, int x, int y)
{
	const pinboard_cell *cell = &board->projection.manage;
	bool preview = board->moving &&
		board->selected < board->model.current.count;
	if (cell->visible && x >= cell->x && y >= cell->y &&
		x < cell->x + cell->width && y < cell->y + cell->height)
		return PINBOARD_MANAGE;
	if (preview)
	{
		cell = &board->preview;
		if (cell->visible && x >= cell->x && y >= cell->y &&
		    x < cell->x + cell->width && y < cell->y + cell->height)
			return board->selected;
	}
	for (unsigned int i = 0; i < board->model.current.count; i++)
	{
		if (preview && i == board->selected)
			continue;
		cell = &board->projection.cells[i];
		if (cell->visible && x >= cell->x && y >= cell->y &&
		    x < cell->x + cell->width && y < cell->y + cell->height)
			return i;
	}
	return PINBOARD_MAX + 1;
}

static void focus_canvas(pinboard *board)
{
	/* Core owns managed focus. The initial non-input hint prevents map
	 * focus. */
	send_command(board, "WindowId %lu Focus", board->window);
}

static void propose_move(pinboard *board)
{
	if (board->moving &&
	    board->gesture_view == board->model.current.generation &&
	    board->gesture_epoch == board->projection.epoch)
		event_reply(board, "move", board->selected, &board->preview);
	board->moving = board->pointer_down = false;
	render(board);
}

static void select_cell(pinboard *board, unsigned int index)
{
	if (index > PINBOARD_MANAGE)
		return;
	board->selected = index;
	event_reply(board, "focus", index, NULL);
	render(board);
}

static void navigate(pinboard *board, KeySym key, bool reverse)
{
	unsigned int indices[PINBOARD_MAX + 1], count = 0, position = 0;
	indices[count++] = PINBOARD_MANAGE;
	for (unsigned int i = 0; i < board->model.current.count; i++)
		if (board->projection.cells[i].visible)
			indices[count++] = i;
	for (unsigned int i = 0; i < count; i++)
		if (indices[i] == board->selected)
			position = i;
	if (key == XK_Home)
		position = 0;
	else if (key == XK_End)
		position = count - 1;
	else if (key == XK_Tab || key == XK_ISO_Left_Tab)
		position = reverse ? (position + count - 1) % count
				   : (position + 1) % count;
	else
	{
		const pinboard_cell *from = cell_for(board, board->selected);
		int64_t best = INT64_MAX;
		for (unsigned int i = 0; i < count; i++)
		{
			const pinboard_cell *to = cell_for(board, indices[i]);
			int64_t dx = to->x - from->x, dy = to->y - from->y;
			if ((key == XK_Left && dx >= 0) ||
			    (key == XK_Right && dx <= 0) ||
			    (key == XK_Up && dy >= 0) ||
			    (key == XK_Down && dy <= 0))
				continue;
			int64_t distance = dx * dx + dy * dy;
			if (distance < best)
				best = distance, position = i;
		}
	}
	select_cell(board, indices[position]);
}

static void key_event(pinboard *board, XKeyEvent *event)
{
	KeySym key = XLookupKeysym(event, 0);
	if (key == XK_Escape)
	{
		board->moving = board->pointer_down = false;
		render(board);
		return;
	}
	if (board->moving)
	{
		if (key == XK_Return || key == XK_KP_Enter)
			propose_move(board);
		else
		{
			int x = board->preview.x, y = board->preview.y;
			if (key == XK_Left)
				x -= board->preview.width;
			if (key == XK_Right)
				x += board->preview.width;
			if (key == XK_Up)
				y -= board->preview.height;
			if (key == XK_Down)
				y += board->preview.height;
			pinboard_cell cell;
			if (pinboard_move_cell(
				&board->model.current.layout,
				&board->monitors.published, &board->workareas,
				&(rectangle){0, 0, board->width, board->height},
				x, y, &cell))
				board->preview = cell;
			render(board);
		}
		return;
	}
	if (key == XK_m && board->selected < board->model.current.count)
	{
		board->moving = true;
		board->gesture_view = board->model.current.generation;
		board->gesture_epoch = board->projection.epoch;
		board->preview = board->projection.cells[board->selected];
	}
	else if (key == XK_Return || key == XK_KP_Enter)
		event_reply(board,
			    board->selected == PINBOARD_MANAGE ? "manage"
							       : "open",
			    board->selected, NULL);
	else if (key == XK_Menu ||
		 (key == XK_F10 && (event->state & ShiftMask)))
		event_reply(board,
			    board->selected == PINBOARD_MANAGE ? "manage"
							       : "context",
			    board->selected, NULL);
	else if (key == XK_r || key == XK_F5)
		event_reply(board, "refresh", PINBOARD_MANAGE, NULL);
	else if (key == XK_q)
		event_reply(board, "close", PINBOARD_MANAGE, NULL);
	else if (key == XK_Tab || key == XK_ISO_Left_Tab || key == XK_Home ||
		 key == XK_End || key == XK_Left || key == XK_Right ||
		 key == XK_Up || key == XK_Down)
		navigate(board, key,
			 (event->state & ShiftMask) || key == XK_ISO_Left_Tab);
}

static void x_event(pinboard *board, XEvent *event)
{
	if (event->type == SelectionClear &&
	    event->xselectionclear.selection == board->selection)
	{
		retire(board);
		return;
	}
	if (event->type == PropertyNotify &&
	    event->xproperty.window == board->root &&
	    event->xproperty.atom == board->clients)
	{
		if ((board->mapped || board->map_requested) &&
		    other_desktop(board))
			retire(board);
		return;
	}
	if (event->type == DestroyNotify && !event->xdestroywindow.send_event &&
	    watched_index(board, event->xdestroywindow.window) <
		board->watched_count)
	{
		/* DestroyNotify already retired this server resource and its
		 * event selection. Never send a request through that destroyed
		 * identity. */
		unsigned int index =
		    watched_index(board, event->xdestroywindow.window);
		/* A queued event from a retired lifetime must not remove a new
		 * subscription if the server has already reused that XID. */
		if (event->xdestroywindow.serial >=
		    board->watched_serial[index])
			forget_watched(board, event->xdestroywindow.window);
		return;
	}
	if (event->type == PropertyNotify &&
	    event->xproperty.atom == board->type &&
	    watched_index(board, event->xproperty.window) <
		board->watched_count)
	{
		unsigned int index =
		    watched_index(board, event->xproperty.window);
		if ((board->mapped || board->map_requested) &&
		    event->xproperty.serial >= board->watched_serial[index] &&
		    borrowed_desktop(board, event->xproperty.window))
			retire(board);
		return;
	}
	if (event->type == ConfigureNotify &&
	    event->xconfigure.window == board->root)
	{
		board->dirty = true;
		return;
	}
	if (!board->window || event->xany.window != board->window)
		return;
	if ((event->type == KeyPress || event->type == ButtonPress ||
	     event->type == ButtonRelease || event->type == MotionNotify) &&
	    (event->xany.send_event ||
	     event->xany.serial < board->input_serial))
		return;
	if (event->type == MapNotify)
	{
		XWMHints hints = {.flags = InputHint, .input = True};
		XSetWMHints(board->display, board->window, &hints);
		XSync(board->display, False);
		board->mapped = true;
		board->map_requested = false;
	}
	if (event->type == Expose && !event->xexpose.count)
		render(board);
	if (event->type == KeyPress && board->mapped)
		key_event(board, &event->xkey);
	if (event->type == ButtonPress && board->mapped)
	{
		XButtonEvent *button = &event->xbutton;
		unsigned int index =
		    hit_cell(board, button->x_root, button->y_root);
		if (index > PINBOARD_MANAGE || button->send_event)
			return;
		focus_canvas(board);
		select_cell(board, index);
		if (index == PINBOARD_MANAGE)
			event_reply(board, "manage", index, NULL);
		else if (button->button == 3)
		{
			/* Menu ownership can consume the remaining button releases;
			 * discard any preview before handing this gesture away. */
			board->moving = board->pointer_down = board->dragged = false;
			render(board);
			/* This real press created our connection's implicit grab.
			 * Release it before the core menu needs its own pointer grab;
			 * XUngrabPointer cannot release another client's grab. */
			XUngrabPointer(board->display, button->time);
			XSync(board->display, False);
			event_reply(board, "context", index, NULL);
		}
		else if (button->button == 1)
		{
			if (board->last_click &&
			    board->last_click_index == index &&
			    button->time - board->last_click <= 400)
				event_reply(board, "open", index, NULL);
			board->last_click = button->time;
			board->last_click_index = index;
			board->pointer_down = true;
			board->dragged = false;
			board->press_x = button->x_root;
			board->press_y = button->y_root;
			board->gesture_view = board->model.current.generation;
			board->gesture_epoch = board->projection.epoch;
			board->preview = board->projection.cells[index];
		}
	}
	if (event->type == MotionNotify && board->pointer_down)
	{
		int x = event->xmotion.x_root, y = event->xmotion.y_root;
		if (abs(x - board->press_x) + abs(y - board->press_y) < 5)
			return;
		pinboard_cell cell;
		if (pinboard_move_cell(
			&board->model.current.layout,
			&board->monitors.published, &board->workareas,
			&(rectangle){0, 0, board->width, board->height}, x, y,
			&cell))
		{
			board->preview = cell;
			board->moving = board->dragged = true;
			render(board);
		}
	}
	if (event->type == ButtonRelease && event->xbutton.button == 1 &&
	    board->pointer_down)
	{
		if (board->dragged)
			propose_move(board);
		board->pointer_down = false;
	}
}

static bool alias_valid(const char *alias)
{
	if (!*alias || strlen(alias) > 64)
		return false;
	for (; *alias; alias++)
		if (!((*alias >= 'a' && *alias <= 'z') ||
		      (*alias >= 'A' && *alias <= 'Z') ||
		      (*alias >= '0' && *alias <= '9') || *alias == '_' ||
		      *alias == '-'))
			return false;
	return true;
}

static void request_config(pinboard *board)
{
	char match[66];

	if (board->config_pending)
	{
		board->workspace_refresh = true;
		return;
	}
	board->config_pending = true;
	/* Common monitor/work-area records and END remain unconditional;
	 * stored module configuration is needed only for this instance. */
	snprintf(match, sizeof(match), "*%s", board->module->name);
	InitGetConfigLine(board->channel, match);
}

int main(int argc, char **argv)
{
	ModuleArgs *module = ParseModuleArgs(argc, argv, 1);
	if (!module || argc != 7 || module->from_fvwm < 0 ||
	    !alias_valid(module->name))
		return 1;
	pinboard *board = fxcalloc(1, sizeof(*board));
	board->module = module;
	board->channel[0] = module->to_fvwm;
	board->channel[1] = module->from_fvwm;
	board->selected = PINBOARD_MANAGE;
	board->model.current.layout = (pinboard_layout){96, 96, 32};
	strcpy(board->font_name, "fixed");
	board->started = seconds_now();
	if (!board->started)
		return 1;
	signal(SIGTERM, stopped);
	signal(SIGINT, stopped);
	signal(SIGHUP, stopped);
	signal(SIGPIPE, stopped);
	FlocaleInit(LC_CTYPE, "", "", module->name);
	board->display = XOpenDisplay(NULL);
	if (!board->display)
		return 1;
	XSetErrorHandler(x_error);
	board->root = DefaultRootWindow(board->display);
	flib_init_graphics(board->display);
	AllocColorset(0);
	FShapeInit(board->display);
	board->unavailable = !FShapesSupported;
	board->clients = XInternAtom(board->display, "_NET_CLIENT_LIST", False);
	board->type = XInternAtom(board->display, "_NET_WM_WINDOW_TYPE", False);
	board->desktop =
	    XInternAtom(board->display, "_NET_WM_WINDOW_TYPE_DESKTOP", False);
	board->timestamp_atom =
	    XInternAtom(board->display, "_FVWM_PINBOARD_TIMESTAMP", False);
	char selection[80];
	snprintf(selection, sizeof(selection), "_NET_DESKTOP_MANAGER_S%d",
		 DefaultScreen(board->display));
	board->selection = XInternAtom(board->display, selection, False);
	XSelectInput(board->display, board->root,
		     PropertyChangeMask | StructureNotifyMask);
	XWindowAttributes root_attributes;
	if (!XGetWindowAttributes(board->display, board->root,
				  &root_attributes) ||
	    root_attributes.width < 1 || root_attributes.width > 32767 ||
	    root_attributes.height < 1 || root_attributes.height > 32767)
		board->unavailable = true;
	if (!board->unavailable)
	{
		board->width = root_attributes.width;
		board->height = root_attributes.height;
		board->window =
		    XCreateSimpleWindow(board->display, board->root, 0, 0,
					board->width, board->height, 0, 0, 0);
		XStoreName(board->display, board->window, module->name);
		XClassHint class_hint = {module->name, "FvwmPinboard"};
		XSetClassHint(board->display, board->window, &class_hint);
		XWMHints hints = {.flags = InputHint, .input = False};
		XSetWMHints(board->display, board->window, &hints);
		XSizeHints sizes = {.flags = USPosition | USSize,
				    .x = 0,
				    .y = 0,
				    .width = board->width,
				    .height = board->height};
		XSetWMNormalHints(board->display, board->window, &sizes);
		XChangeProperty(board->display, board->window, board->type,
				XA_ATOM, 32, PropModeReplace,
				(unsigned char *)&board->desktop, 1);
		XSelectInput(board->display, board->window,
			     ExposureMask | KeyPressMask | ButtonPressMask |
				 ButtonReleaseMask | PointerMotionMask |
				 StructureNotifyMask | PropertyChangeMask);
		board->gc = XCreateGC(board->display, board->window, 0, NULL);
	}
	SetMessageMask(board->channel, M_STRING | M_CONFIG_INFO |
					   M_END_CONFIG_INFO | M_SENDCONFIG |
					   M_NEW_DESK | M_NEW_PAGE);
	request_config(board);
	if (!module_packet_reader_init(&board->reader, board->channel[1]))
		board->stopped = true;
	SendFinishedStartupNotification(board->channel);
	while (!pinboard_stopping && !board->stopped && !pinboard_x_failed)
	{
		double now = seconds_now();
		/* Retirement keeps a short reply window after unmapping.  The
		 * initial-map deadline must not cut that window short. */
		if (!now || (!board->mapped && !board->retiring &&
			     now - board->started >= 5) ||
		    (board->lease && now >= board->lease) ||
		    (board->retiring && now >= board->retiring))
			break;
		for (unsigned int i = 0; i < 32 && XPending(board->display);
		     i++)
		{
			XEvent event;
			XNextEvent(board->display, &event);
			x_event(board, &event);
		}
		for (unsigned int i = 0; i < 32; i++)
		{
			FvwmPacket *packet;
			module_packet_read_status status =
			    module_packet_reader_read(&board->reader, &packet);
			if (status == MODULE_PACKET_READ_MORE)
				break;
			if (status != MODULE_PACKET_READ_READY)
			{
				board->stopped = true;
				break;
			}
			if ((packet->type == M_STRING ||
			     packet->type == M_CONFIG_INFO) &&
			    module_packet_body_has_text(
				packet->body, FvwmPacketBodySize(*packet), 3))
			{
				char *text = (char *)&packet->body[3];
				if (packet->type == M_STRING)
					native_message(board, text);
				else
					config_message(board, text);
			}
			if (packet->type == M_END_CONFIG_INFO)
			{
				module_ui_scale_abort(&board->monitors);
				board->config_pending = false;
				if (board->workspace_refresh)
				{
					board->workspace_refresh = false;
					request_config(board);
				}
			}
			if ((packet->type == M_NEW_DESK ||
			     packet->type == M_NEW_PAGE) &&
			    pinboard_workspace_changed(
				&board->workspace, packet->type == M_NEW_PAGE,
				packet->body, FvwmPacketBodySize(*packet)))
				request_config(board);
		}
		if (board->dirty)
		{
			board->dirty = false;
			publish_scene(board, board->commit_waiting);
		}
		struct pollfd descriptors[2] = {
		    {ConnectionNumber(board->display), POLLIN, 0},
		    {board->channel[1], POLLIN, 0}};
		if (poll(descriptors, 2, XPending(board->display) ? 0 : 20) <
			0 &&
		    errno != EINTR)
			break;
	}
	retire(board);
	release_resources(board, board->resources);
	if (board->gc)
		XFreeGC(board->display, board->gc);
	XCloseDisplay(board->display);
	close(board->channel[0]);
	close(board->channel[1]);
	free(board);
	return pinboard_x_failed ? 1 : 0;
}
