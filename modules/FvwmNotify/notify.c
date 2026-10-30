/* This program is free software; you can redistribute it and/or modify it
 * under the GNU General Public License, version 2 or any later version. */
#include "config.h"
#include "notify_bus.h"
#include "libs/Module.h"
#include "libs/module_packet_reader.h"
#include "libs/fd_safety.h"
#include "libs/module_ui_scale.h"
#include "libs/module_workarea.h"
#include "libs/Colorset.h"
#include "libs/Parse.h"
#include "libs/PictureUtils.h"
#include "libs/flocale_font_types.h"
#include "libs/safemalloc.h"
#include "libs/log.h"
#include <X11/keysym.h>
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <limits.h>
#include <poll.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <time.h>
#include <unistd.h>

#define NOTIFY_POPUPS 3
#define NOTIFY_LINES 10
#define NOTIFY_LINE_BYTES 256

typedef struct
{
	Window window;
	uint32_t id;
	uint64_t revision;
	unsigned long serial;
	int x, y, width, height, button_y, line;
	unsigned int actions;
	bool mapped;
} notify_view;

typedef struct
{
	Display *display;
	Window root;
	Atom id_atom, delete_atom;
	ModuleArgs *module;
	int channel[2], wake[2], colorset;
	GC gc;
	FlocaleFont *font;
	char font_name[257];
	unsigned int quarters;
	module_packet_reader reader;
	module_ui_scale_receiver monitors;
	module_workarea_state workareas;
	module_monitor_area area;
	notify_bus bus;
	notify_view popups[NOTIFY_POPUPS], center;
	unsigned int selected, action, history_index;
	uint32_t selected_id;
	bool center_open, history_open, focus_pending;
	bool config_pending, config_refresh, dirty, stopped;
} notify_ui;

static volatile sig_atomic_t notify_stopping;
static volatile sig_atomic_t notify_wake_fd = -1;
static bool notify_x_failed;

static void request_config(notify_ui *ui);

static double seconds_now(void)
{
	struct timespec value;

	if (clock_gettime(CLOCK_MONOTONIC, &value))
		return 0;
	return value.tv_sec + value.tv_nsec / 1000000000.0;
}

static void stop_signal(int number)
{
	int saved_errno = errno;
	char byte = 1;

	(void)number;
	notify_stopping = 1;
	if (notify_wake_fd >= 0)
	{
		ssize_t ignored = write(notify_wake_fd, &byte, 1);
		(void)ignored;
	}
	errno = saved_errno;
}

static int x_error(Display *display, XErrorEvent *event)
{
	(void)display;
	(void)event;
	notify_x_failed = true;
	return 0;
}

static int minimum(int left, int right)
{
	return left < right ? left : right;
}

static int scaled(const notify_ui *ui, int logical)
{
	int result = 0;

	if (monitor_scale_apply(logical,
	    (monitor_scale_factor){ui->quarters}, &result) !=
	    MONITOR_SCALE_CONVERSION_OK)
		return logical;
	return result;
}

/* Only our validated published monitor rows select scale/work-area geometry.
 * The root fallback precedes the first publication; it supplies no epoch. */
static bool update_area(notify_ui *ui)
{
	const module_ui_scale_snapshot *snapshot = &ui->monitors.published;
	const module_ui_scale_row *row = NULL;
	module_monitor_area area;
	unsigned int quarters = 4;
	XWindowAttributes root;

	if (snapshot->count)
	{
		row = &snapshot->rows[0];
		for (unsigned int i = 0; i < snapshot->count; i++)
			if (snapshot->rows[i].primary)
				row = &snapshot->rows[i];
		if (!module_workarea_bounds(&ui->workareas, snapshot, row,
		    &area))
			return false;
		quarters = row->quarters;
	}
	else
	{
		if (!XGetWindowAttributes(ui->display, ui->root, &root))
			return false;
		area = (module_monitor_area){0, 0, root.width, root.height};
	}
	if (area.width < 1 || area.height < 1 || area.width > 32767 ||
	    area.height > 32767 || area.x < -32768 || area.x > 32767 ||
	    area.y < -32768 || area.y > 32767 ||
	    area.x + area.width > 32767 || area.y + area.height > 32767)
		return false;
	if (!ui->font || ui->quarters != quarters)
	{
		FlocaleFont *font = FlocaleLoadFontAtScale(ui->display,
		    ui->font_name, ui->module->name,
		    (monitor_scale_factor){quarters}, 0);
		if (!font)
			font = FlocaleLoadFontAtScale(ui->display, "fixed",
			    ui->module->name,
			    (monitor_scale_factor){quarters}, 0);
		if (!font)
			return false;
		if (ui->font)
			FlocaleUnloadFont(ui->display, ui->font);
		ui->font = font;
		ui->quarters = quarters;
	}
	ui->area = area;
	return true;
}

static void text_draw(notify_ui *ui, Drawable drawable, const char *text,
	int x, int y, int width, int height)
{
	FlocaleWinString string = {0};
	XRectangle clip;

	if (width < 1 || height < 1)
		return;
	clip = (XRectangle){x, y - ui->font->ascent, width, height};
	XSetClipRectangles(ui->display, ui->gc, 0, 0, &clip, 1, Unsorted);
	if (ui->font->font)
		XSetFont(ui->display, ui->gc, ui->font->font->fid);
	string.str = (char *)text;
	string.len = strlen(text);
	string.gc = ui->gc;
	string.win = drawable;
	string.colorset = GetColorset(ui->colorset);
	string.flags.has_colorset = 1;
	string.x = x;
	string.y = y;
	string.clip_region = XCreateRegion();
	string.flags.has_clip_region = 1;
	XUnionRectWithRegion(&clip, string.clip_region, string.clip_region);
	FlocaleDrawString(ui->display, ui->font, &string, FWS_HAVE_LENGTH);
	XDestroyRegion(string.clip_region);
	XSetClipMask(ui->display, ui->gc, None);
}

/* All source text was validated by the model. Keep UTF-8 code points whole;
 * wrapping performs bounded work over at most 2,048 admitted body bytes. */
static unsigned int text_lines(notify_ui *ui, const char *text, int width,
	char lines[NOTIFY_LINES][NOTIFY_LINE_BYTES + 1], unsigned int maximum)
{
	unsigned int count = 0;
	const unsigned char *cursor = (const unsigned char *)text;

	while (*cursor && count < maximum)
	{
		unsigned int used = 0;
		while (*cursor && *cursor != '\n')
		{
			unsigned int bytes = *cursor < 0x80 ? 1 :
			    (*cursor < 0xe0 ? 2 : (*cursor < 0xf0 ? 3 : 4));
			if (used + bytes > NOTIFY_LINE_BYTES)
				break;
			memcpy(lines[count] + used, cursor, bytes);
			if (*cursor == '\t')
				lines[count][used] = ' ';
			lines[count][used + bytes] = 0;
			if (FlocaleTextWidth(ui->font, lines[count],
			    used + bytes) > width && used)
			{
				lines[count][used] = 0;
				break;
			}
			used += bytes;
			cursor += bytes;
		}
		lines[count][used] = 0;
		count++;
		if (*cursor == '\n')
			cursor++;
	}
	if (*cursor && count)
	{
		size_t length = strlen(lines[count - 1]);
		if (length + 3 <= NOTIFY_LINE_BYTES)
			memcpy(lines[count - 1] + length, "...", 4);
	}
	return count;
}

static Window create_window(notify_ui *ui, bool passive)
{
	XSetWindowAttributes attributes = {0};
	XClassHint class_hint;
	XWMHints hints = {.flags = InputHint, .input = passive ? False : True};
	Window window;

	attributes.override_redirect = passive;
	attributes.event_mask = ExposureMask | ButtonPressMask | KeyPressMask |
	    StructureNotifyMask;
	attributes.background_pixel = GetColorset(ui->colorset)->bg;
	window = XCreateWindow(ui->display, ui->root, 0, 0, 1, 1, 0,
	    CopyFromParent, InputOutput, CopyFromParent,
	    CWOverrideRedirect | CWEventMask | CWBackPixel, &attributes);
	class_hint.res_name = passive ? "FvwmNotify" : "FvwmNotifyCenter";
	class_hint.res_class = class_hint.res_name;
	XSetClassHint(ui->display, window, &class_hint);
	XStoreName(ui->display, window,
	    passive ? "FvwmNotify popup" : "FVWM-X notifications");
	XSetWMHints(ui->display, window, &hints);
	if (passive)
	{
		Atom property = XInternAtom(ui->display,
		    "_NET_WM_WINDOW_TYPE", False);
		Atom type = XInternAtom(ui->display,
		    "_NET_WM_WINDOW_TYPE_NOTIFICATION", False);
		XChangeProperty(ui->display, window, property, XA_ATOM, 32,
		    PropModeReplace, (unsigned char *)&type, 1);
	}
	else
		XSetWMProtocols(ui->display, window, &ui->delete_atom, 1);
	return window;
}

static void view_unmap(notify_ui *ui, notify_view *view)
{
	if (view->mapped)
		XUnmapWindow(ui->display, view->window);
	view->mapped = false;
	view->id = 0;
}

static void center_close(notify_ui *ui)
{
	ui->center_open = ui->history_open = ui->focus_pending = false;
	if (ui->center.window)
		XDestroyWindow(ui->display, ui->center.window);
	memset(&ui->center, 0, sizeof(ui->center));
}

/* A view records the model revision and request serial of its pixels. Events
 * queued for replaced content never dispatch a new action by index. */
static void draw_view(notify_ui *ui, notify_view *view,
	const notify_record *record, bool history, unsigned int position,
	unsigned int total, int x, int y, int width, int maximum_height)
{
	char summary[NOTIFY_LINES][NOTIFY_LINE_BYTES + 1] = {{0}};
	char body[NOTIFY_LINES][NOTIFY_LINE_BYTES + 1] = {{0}};
	char header[256];
	colorset_t *colors = GetColorset(ui->colorset);
	int pad = minimum(scaled(ui, 12), width / 4);
	int line = ui->font->height + scaled(ui, 6);
	int text_width = width - pad * 2;
	unsigned int actions = history ? 0 : record->action_count;
	unsigned int summary_count, body_count, body_max;
	int available_lines, height, baseline;
	Pixmap pixmap;
	unsigned long id = history ? 0 : record->id;

	if (line < 1)
		line = 1;
	available_lines = (maximum_height - pad * 2) / line;
	if (available_lines < 1)
		available_lines = 1;
	summary_count = text_lines(ui, record->summary, text_width, summary, 2);
	body_max = available_lines > (int)(summary_count + actions + 3)
	    ? available_lines - summary_count - actions - 3 : 0;
	if (body_max > 8)
		body_max = 8;
	body_count = text_lines(ui, record->body, text_width, body, body_max);
	height = pad * 2 + line * (summary_count + body_count + actions + 3);
	height = minimum(height, maximum_height);
	if (height < 1)
		height = 1;
	bool fresh = !view->window;
	bool moved = view->x != x || view->y != y ||
	    view->width != width || view->height != height;
	if (fresh)
		view->window = create_window(ui, view != &ui->center);
	view->serial = NextRequest(ui->display);
	view->revision = ui->bus.model.revision;
	view->id = id;
	view->actions = actions;
	view->x = x;
	view->y = y;
	view->width = width;
	view->height = height;
	view->line = line;
	if (view == &ui->center)
	{
		XSizeHints sizes = {.flags = USPosition | USSize | PMinSize | PMaxSize,
		    .x = x, .y = y, .width = width, .height = height,
		    .min_width = width, .max_width = width,
		    .min_height = height, .max_height = height};
		XSetWMNormalHints(ui->display, view->window, &sizes);
	}
	if (view != &ui->center || fresh)
		XMoveResizeWindow(ui->display, view->window, x, y, width, height);
	else if (moved)
	{
		char command[160];
		/* A managed center can already be reparented. The core owns its
		 * frame geometry; the leading plus preserves negative coordinates
		 * as literal offsets instead of right/bottom anchoring. */
		snprintf(command, sizeof(command),
		    "WindowId %lu ResizeMove %dp %dp screen g +%dp +%dp ewmhiwa",
		    view->window, width, height, x, y);
		SendText(ui->channel, command, 0);
	}
	XChangeProperty(ui->display, view->window, ui->id_atom, XA_CARDINAL, 32,
	    PropModeReplace, (unsigned char *)&id, 1);
	pixmap = XCreatePixmap(ui->display, view->window, width, height,
	    DefaultDepth(ui->display, DefaultScreen(ui->display)));
	XSetForeground(ui->display, ui->gc, colors->bg);
	XFillRectangle(ui->display, pixmap, ui->gc, 0, 0, width, height);
	XSetForeground(ui->display, ui->gc, colors->fg);
	XDrawRectangle(ui->display, pixmap, ui->gc, 0, 0,
	    width > 1 ? width - 1 : 0, height > 1 ? height - 1 : 0);
	if (view == &ui->center)
		snprintf(header, sizeof(header), "%s %u/%u - Up/Down, Tab, Enter",
		    history ? "History" : "Notifications", position, total);
	else
		snprintf(header, sizeof(header), "%s%s", record->app,
		    record->urgency == 2 ? " [critical]" : "");
	baseline = pad + ui->font->ascent;
	text_draw(ui, pixmap, header, pad, baseline, text_width, line);
	baseline += line;
	for (unsigned int i = 0; i < summary_count; i++, baseline += line)
		text_draw(ui, pixmap, summary[i], pad, baseline, text_width, line);
	for (unsigned int i = 0; i < body_count; i++, baseline += line)
		text_draw(ui, pixmap, body[i], pad, baseline, text_width, line);
	/* Buttons stay above the lower edge even when a tiny monitor truncates
	 * presentation. The managed center remains keyboard operable. */
	view->button_y = minimum(baseline - ui->font->ascent,
	    height - (int)(actions + 1) * line - pad);
	if (view->button_y < 0)
		view->button_y = 0;
	for (unsigned int i = 0; i <= actions; i++)
	{
		const char *label = i == actions
		    ? (history ? "Close history (Escape)" : "Dismiss")
		    : record->actions[i].label;
		int top = view->button_y + i * line;
		XDrawRectangle(ui->display, pixmap, ui->gc, pad, top,
		    text_width > 1 ? text_width - 1 : 0, line > 1 ? line - 1 : 0);
		if (view == &ui->center && !history && ui->action == i &&
		    text_width > 4 && line > 4)
			XDrawRectangle(ui->display, pixmap, ui->gc,
			    pad + 2, top + 2, text_width - 5, line - 5);
		text_draw(ui, pixmap, label, pad + scaled(ui, 4),
		    top + ui->font->ascent + scaled(ui, 2),
		    text_width - scaled(ui, 8), line - scaled(ui, 2));
	}
	XCopyArea(ui->display, pixmap, view->window, ui->gc,
	    0, 0, width, height, 0, 0);
	XFreePixmap(ui->display, pixmap);
	if (!view->mapped)
	{
		XMapRaised(ui->display, view->window);
		view->mapped = true;
	}
}

static void render(notify_ui *ui)
{
	notify_model *model = &ui->bus.model;
	bool used[NOTIFY_POPUPS] = {false};
	unsigned int visible;
	int gap, width, height, y;

	if (!update_area(ui))
	{
		ui->stopped = true;
		return;
	}
	gap = minimum(scaled(ui, 10), ui->area.width / 8);
	width = minimum(scaled(ui, 380), ui->area.width - gap * 2);
	if (width < 1)
		width = 1;
	visible = ui->area.height / (ui->font->height * 8 + gap * 2);
	if (!visible)
		visible = 1;
	if (visible > NOTIFY_POPUPS)
		visible = NOTIFY_POPUPS;
	if (visible > model->count)
		visible = model->count;
	height = (ui->area.height - gap * ((int)visible + 1)) /
	    (visible ? (int)visible : 1);
	if (height < 1)
		height = 1;
	y = ui->area.y + minimum(gap, ui->area.height - 1);
	for (unsigned int i = 0; i < visible; i++)
	{
		unsigned int slot = NOTIFY_POPUPS;
		for (unsigned int j = 0; j < NOTIFY_POPUPS; j++)
			if (!used[j] && ui->popups[j].id == model->active[i].id)
				slot = j;
		if (slot == NOTIFY_POPUPS)
			for (unsigned int j = 0; j < NOTIFY_POPUPS; j++)
				if (!used[j])
				{
					slot = j;
					break;
				}
		used[slot] = true;
		draw_view(ui, &ui->popups[slot], &model->active[i], false,
		    i + 1, model->count,
		    ui->area.x + ui->area.width - width - gap,
		    y, width, height);
		y += ui->popups[slot].height + gap;
	}
	for (unsigned int i = 0; i < NOTIFY_POPUPS; i++)
		if (!used[i])
			view_unmap(ui, &ui->popups[i]);
	if (ui->center_open)
	{
		const notify_record *record = NULL;
		unsigned int count = ui->history_open
		    ? model->history_count : model->count;
		if (ui->history_open && count)
		{
			if (ui->history_index >= count)
				ui->history_index = count - 1;
			record = &model->history[ui->history_index].record;
		}
		else if (count)
		{
			for (unsigned int i = 0; i < count; i++)
				if (model->active[i].id == ui->selected_id)
					ui->selected = i;
			if (ui->selected >= count)
				ui->selected = count - 1;
			record = &model->active[ui->selected];
			ui->selected_id = record->id;
			if (ui->action > record->action_count)
				ui->action = record->action_count;
		}
		if (!record)
			center_close(ui);
		else
		{
			int center_width = minimum(scaled(ui, 500),
			    ui->area.width - gap * 2);
			if (center_width < 1)
				center_width = 1;
			draw_view(ui, &ui->center, record, ui->history_open,
			    (ui->history_open ? ui->history_index : ui->selected) + 1,
			    count, ui->area.x + (ui->area.width - center_width) / 2,
			    ui->area.y + gap, center_width,
			    ui->area.height - gap * 2 > 0
				? ui->area.height - gap * 2 : 1);
		}
	}
	XFlush(ui->display);
	ui->dirty = ui->bus.dirty = false;
}

static void center_open(notify_ui *ui, bool history)
{
	if (!(history ? ui->bus.model.history_count : ui->bus.model.count))
		return;
	ui->center_open = true;
	ui->history_open = history;
	ui->history_index = ui->selected = ui->action = 0;
	ui->selected_id = 0;
	ui->focus_pending = true;
	ui->dirty = true;
	/* A previously mapped center has no second MapNotify. Focus is still
	 * an explicit command through the core, never direct XSetInputFocus. */
	if (ui->center.mapped)
	{
		char command[80];
		snprintf(command, sizeof(command), "WindowId %lu Focus",
		    ui->center.window);
		SendText(ui->channel, command, 0);
		ui->focus_pending = false;
	}
}

static void status_reply(notify_ui *ui)
{
	char text[256];
	unsigned int height = ui->font && ui->font->height > 0
	    ? (unsigned int)ui->font->height : 0;
	int length = snprintf(text, sizeof(text),
	    "Echo FVWM_NOTIFY_STATUS %u %u %u %u %u %u %u %" PRIu64,
	    ui->bus.model.dnd ? 1U : 0U, ui->bus.model.count,
	    ui->bus.model.history_count, ui->bus.model.history_limit,
	    height, ui->quarters,
	    ui->config_pending || ui->config_refresh ? 1U : 0U,
	    ui->monitors.published.epoch);

	/* The record contains only fixed labels and owned numeric state. No
	 * notification text, action key or caller-supplied destination escapes. */
	if (length < 0 || (size_t)length >= sizeof(text))
	{
		ui->stopped = true;
		return;
	}
	SendText(ui->channel, text, 0);
}

static void native_message(notify_ui *ui, const char *text)
{
	if (!strcmp(text, "DND 0") || !strcmp(text, "DND 1"))
		notify_bus_dnd(&ui->bus, text[4] == '1');
	else if (!strcmp(text, "DismissAll"))
		notify_bus_dismiss_all(&ui->bus);
	else if (!strcmp(text, "Focus"))
		center_open(ui, false);
	else if (!strcmp(text, "History"))
		center_open(ui, true);
	else if (!strcmp(text, "Reload"))
		request_config(ui);
	else if (!strcmp(text, "Status"))
		status_reply(ui);
	else
		fvwm_debug(__func__, "FvwmNotify: refused native command");
}

static void request_config(notify_ui *ui)
{
	if (ui->config_pending)
	{
		ui->config_refresh = true;
		return;
	}
	ui->config_pending = true;
	InitGetConfigLine(ui->channel, "*FvwmNotify");
}

static void config_message(notify_ui *ui, char *text)
{
	module_ui_scale_status status =
	    module_ui_scale_receive(&ui->monitors, text);
	char *option, *value;

	if (status == MODULE_UI_SCALE_PUBLISHED)
	{
		module_workarea_sync(&ui->workareas, &ui->monitors.published);
		ui->dirty = true;
		if (!ui->config_pending)
			request_config(ui);
		return;
	}
	if (status != MODULE_UI_SCALE_IGNORED)
		return;
	if (module_workarea_receive(&ui->workareas,
	    &ui->monitors.published, text) == MODULE_WORKAREA_ADMITTED)
	{
		ui->dirty = true;
		return;
	}
	if (!strncasecmp(text, "LoggingFD", 9) &&
	    (text[9] == ' ' || text[9] == '\t'))
	{
		int descriptor = -1;
		if (!module_parse_decimal_integers(text + 9, &descriptor, 1))
			fvwm_debug(__func__,
			    "FvwmNotify: refusing invalid logging descriptor");
		else if (!log_adopt_inherited_fd(descriptor))
			fvwm_debug(__func__,
			    "FvwmNotify: unable to adopt logging descriptor %d",
			    descriptor);
		return;
	}
	if (!strncasecmp(text, "Colorset ", 9))
	{
		if (LoadColorset(text + 8) == ui->colorset)
			ui->dirty = true;
		return;
	}
	if (strncasecmp(text, "*FvwmNotify", 11) ||
	    (text[11] && text[11] != ':' && text[11] != ' ' && text[11] != '\t'))
		return;
	option = text + 11;
	if (*option == ':')
		option++;
	while (*option == ' ' || *option == '\t')
		option++;
	value = option;
	while (*value && *value != ' ' && *value != '\t')
		value++;
	if (!*value)
		return;
	*value++ = 0;
	while (*value == ' ' || *value == '\t')
		value++;
	if (!strcasecmp(option, "Colorset"))
	{
		char *end;
		int colorset;
		if (ParseColorsetId(value, &end, &colorset) && !*end &&
		    colorset >= 0 && AllocColorset(colorset))
		{
			ui->colorset = colorset;
			ui->dirty = true;
		}
	}
	else if (!strcasecmp(option, "Font"))
	{
		char *font = NULL;
		char *end = GetNextToken(value, &font);
		while (end && (*end == ' ' || *end == '\t'))
			end++;
		if (font && *font && strlen(font) <= 256 && (!end || !*end))
		{
			FlocaleFont *candidate = FlocaleLoadFontAtScale(ui->display,
			    font, ui->module->name,
			    (monitor_scale_factor){ui->quarters ? ui->quarters : 4}, 0);
			if (candidate)
			{
				if (ui->font)
					FlocaleUnloadFont(ui->display, ui->font);
				ui->font = candidate;
				strcpy(ui->font_name, font);
				ui->dirty = true;
			}
		}
		free(font);
	}
	else if (!strcasecmp(option, "History"))
	{
		int limit;
		if (module_parse_decimal_integers(value, &limit, 1) &&
		    limit >= 0 && limit <= NOTIFY_HISTORY_MAX &&
		    notify_model_history_limit(&ui->bus.model, limit))
			ui->dirty = true;
	}
}

static void center_key(notify_ui *ui, XKeyEvent *event)
{
	KeySym key = XLookupKeysym(event, 0);
	notify_model *model = &ui->bus.model;
	unsigned int count = ui->history_open
	    ? model->history_count : model->count;

	if (key == XK_Escape)
	{
		center_close(ui);
		return;
	}
	if (!count)
		return;
	if (key == XK_Up || key == XK_Down)
	{
		unsigned int *index = ui->history_open
		    ? &ui->history_index : &ui->selected;
		*index = key == XK_Up ? (*index + count - 1) % count
		    : (*index + 1) % count;
		ui->selected_id = ui->history_open ? 0
		    : model->active[ui->selected].id;
		ui->action = 0;
		ui->dirty = true;
		return;
	}
	if (ui->history_open)
		return;
	/* Keyboard intent is ordered even before the next paint: a queued
	 * Down then Enter targets the newly selected ID, never old pixels. */
	const notify_record *record = notify_model_find(model, ui->selected_id);
	if (!record)
		return;
	if (key == XK_Tab || key == XK_ISO_Left_Tab ||
	    key == XK_Left || key == XK_Right)
	{
		unsigned int total = record->action_count + 1;
		bool backwards = key == XK_Left || key == XK_ISO_Left_Tab ||
		    (key == XK_Tab && (event->state & ShiftMask));
		ui->action = backwards ? (ui->action + total - 1) % total
		    : (ui->action + 1) % total;
		ui->dirty = true;
	}
	if (key == XK_Return || key == XK_KP_Enter || key == XK_space ||
	    key == XK_Delete)
	{
		if (key == XK_Delete || ui->action == record->action_count)
			notify_bus_close(&ui->bus, record->id, 2);
		else
			notify_bus_action(&ui->bus, record->id, ui->action);
		ui->dirty = true;
	}
}

static void x_event(notify_ui *ui, XEvent *event)
{
	notify_view *view = NULL;

	if (event->xany.window == ui->center.window)
		view = &ui->center;
	for (unsigned int i = 0; i < NOTIFY_POPUPS; i++)
		if (event->xany.window == ui->popups[i].window)
			view = &ui->popups[i];
	if (!view)
		return;
	if (event->type == Expose && !event->xexpose.count)
	{
		ui->dirty = true;
		return;
	}
	if (view == &ui->center && event->type == MapNotify &&
	    ui->focus_pending && ui->center_open)
	{
		XWMHints hints = {.flags = InputHint, .input = True};
		char command[80];
		XSetWMHints(ui->display, view->window, &hints);
		XSync(ui->display, False);
		snprintf(command, sizeof(command), "WindowId %lu Focus", view->window);
		SendText(ui->channel, command, 0);
		ui->focus_pending = false;
		return;
	}
	if (view == &ui->center && event->type == ClientMessage &&
	    (Atom)event->xclient.data.l[0] == ui->delete_atom)
	{
		center_close(ui);
		return;
	}
	if (!view->mapped || event->xany.send_event ||
	    event->xany.serial < view->serial ||
	    view->revision != ui->bus.model.revision)
		return;
	if (view == &ui->center && event->type == KeyPress)
	{
		center_key(ui, &event->xkey);
		return;
	}
	if (event->type == ButtonPress)
	{
		const notify_record *record;
		XButtonEvent *button = &event->xbutton;
		if (view == &ui->center && ui->history_open)
		{
			if (button->y >= view->button_y)
				center_close(ui);
			return;
		}
		record = notify_model_find(&ui->bus.model, view->id);
		if (!record)
			return;
		if (button->button == 3)
			notify_bus_close(&ui->bus, record->id, 2);
		else if (button->button == 1 && button->y >= view->button_y)
		{
			unsigned int action =
			    (button->y - view->button_y) / view->line;
			if (action == view->actions)
				notify_bus_close(&ui->bus, record->id, 2);
			else if (action < view->actions)
				notify_bus_action(&ui->bus, record->id, action);
		}
		else if (button->button == 1)
			for (unsigned int i = 0; i < record->action_count; i++)
				if (!strcmp(record->actions[i].key, "default"))
				{
					notify_bus_action(&ui->bus, record->id, i);
					break;
				}
	}
}

int main(int argc, char **argv)
{
	ModuleArgs *module = ParseModuleArgs(argc, argv, 0);
	notify_ui *ui;
	int status = 1;

	if (!module || argc != 6 || strcmp(module->name, "FvwmNotify"))
		return 1;
	ui = fxcalloc(1, sizeof(*ui));
	ui->module = module;
	ui->channel[0] = module->to_fvwm;
	ui->channel[1] = module->from_fvwm;
	ui->wake[0] = ui->wake[1] = -1;
	if (!fvwm_pipe_cloexec(ui->wake))
		goto out;
	for (unsigned int i = 0; i < 2; i++)
	{
		int flags = fcntl(ui->wake[i], F_GETFL);
		if (flags < 0 || fcntl(ui->wake[i], F_SETFL, flags | O_NONBLOCK))
			goto out;
	}
	notify_wake_fd = ui->wake[1];
	strcpy(ui->font_name, "fixed");
	signal(SIGTERM, stop_signal);
	signal(SIGINT, stop_signal);
	signal(SIGHUP, stop_signal);
	signal(SIGPIPE, stop_signal);
	/* Optional bus authentication is isolated here. It must never keep the
	 * core's module startup barrier waiting on another service. */
	SendFinishedStartupNotification(ui->channel);
	FlocaleInit(LC_CTYPE, "", "", module->name);
	ui->display = XOpenDisplay(NULL);
	if (!ui->display)
		goto out;
	XSetErrorHandler(x_error);
	ui->root = DefaultRootWindow(ui->display);
	ui->id_atom = XInternAtom(ui->display, "_FVWM_NOTIFY_ID", False);
	ui->delete_atom = XInternAtom(ui->display, "WM_DELETE_WINDOW", False);
	flib_init_graphics(ui->display);
	if (!AllocColorset(0))
		goto out;
	ui->gc = XCreateGC(ui->display, ui->root, 0, NULL);
	if (!notify_bus_open(&ui->bus, getenv("DBUS_SESSION_BUS_ADDRESS")))
	{
		fvwm_debug(__func__, "FvwmNotify: optional bus unavailable or name owned");
		goto out;
	}
	notify_bus_dnd(&ui->bus, true);
	SetMessageMask(ui->channel,
	    M_STRING | M_CONFIG_INFO | M_END_CONFIG_INFO | M_SENDCONFIG);
	request_config(ui);
	if (!module_packet_reader_init(&ui->reader, ui->channel[1]))
		goto out;
	SendText(ui->channel, "SendToModule FvwmDesktop notification-policy", 0);
	ui->dirty = true;
	while (!notify_stopping && !ui->stopped && !notify_x_failed &&
	    !ui->bus.failed)
	{
		double now = seconds_now();
		if (!now || !notify_bus_service(&ui->bus, now))
			break;
		for (unsigned int i = 0; i < 32 && XPending(ui->display); i++)
		{
			XEvent event;
			XNextEvent(ui->display, &event);
			x_event(ui, &event);
		}
		for (unsigned int i = 0; i < 32; i++)
		{
			FvwmPacket *packet;
			module_packet_read_status read =
			    module_packet_reader_read(&ui->reader, &packet);
			if (read == MODULE_PACKET_READ_MORE)
				break;
			if (read != MODULE_PACKET_READ_READY)
			{
				ui->stopped = true;
				break;
			}
			if (packet->type == M_STRING || packet->type == M_CONFIG_INFO)
			{
				if (!module_packet_body_has_text(packet->body,
				    FvwmPacketBodySize(*packet), 3))
				{
					fvwm_debug(__func__,
					    "FvwmNotify: malformed module record");
					ui->stopped = true;
					break;
				}
				char *text = (char *)&packet->body[3];
				if (packet->type == M_STRING)
					native_message(ui, text);
				else
					config_message(ui, text);
			}
			if (packet->type == M_END_CONFIG_INFO)
			{
				module_ui_scale_abort(&ui->monitors);
				ui->config_pending = false;
				if (ui->config_refresh)
				{
					ui->config_refresh = false;
					request_config(ui);
				}
			}
			if (packet->type == M_SENDCONFIG)
				request_config(ui);
		}
		if (ui->dirty || ui->bus.dirty)
			render(ui);
		struct pollfd descriptors[4] = {
		    {ConnectionNumber(ui->display), POLLIN, 0},
		    {ui->channel[1], POLLIN, 0},
		    {notify_bus_fd(&ui->bus), notify_bus_events(&ui->bus), 0},
		    {ui->wake[0], POLLIN, 0}};
		double deadline = notify_model_deadline(&ui->bus.model);
		int timeout = -1;
		if (deadline)
		{
			double remaining = (deadline - seconds_now()) * 1000;
			timeout = remaining <= 0 ? 0 :
			    (remaining >= INT_MAX ? INT_MAX : (int)remaining + 1);
		}
		if (XPending(ui->display) || ui->bus.pending)
			timeout = 0;
		if (poll(descriptors, 4, timeout) < 0 && errno != EINTR)
			break;
	}
	status = !notify_x_failed && !ui->bus.failed &&
	    (notify_stopping || ui->stopped) ? 0 : 1;
out:
	notify_wake_fd = -1;
	if (ui->wake[1] >= 0)
		close(ui->wake[1]);
	if (ui->wake[0] >= 0)
		close(ui->wake[0]);
	notify_bus_destroy(&ui->bus);
	if (ui->display)
	{
		for (unsigned int i = 0; i < NOTIFY_POPUPS; i++)
			if (ui->popups[i].window)
				XDestroyWindow(ui->display, ui->popups[i].window);
		if (ui->center.window)
			XDestroyWindow(ui->display, ui->center.window);
		if (ui->font)
			FlocaleUnloadFont(ui->display, ui->font);
		if (ui->gc)
			XFreeGC(ui->display, ui->gc);
		XCloseDisplay(ui->display);
	}
	close(ui->channel[0]);
	close(ui->channel[1]);
	free(ui);
	return status;
}
