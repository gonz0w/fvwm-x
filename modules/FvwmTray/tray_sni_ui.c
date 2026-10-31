/* This program is free software; you can redistribute it and/or modify it
 * under the GNU General Public License, version 2 or any later version. */
#include "config.h"
#include "tray.h"
#include "tray_bus.h"
#include "tray_icons.h"
#include "libs/Colorset.h"
#include "libs/Picture.h"
#include "libs/PictureGraphics.h"
#include "libs/picture_types.h"
#include "libs/picture_argb.h"
#include "libs/wm_ui_metrics.h"
#include "libs/safemalloc.h"
#include "libs/log.h"
#include <X11/Xatom.h>
#include <X11/keysym.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define SNI_UI_IMAGE_MAX (4U * 1024U * 1024U)
#define SNI_MENU_ROWS 12U

typedef struct
{
	Window window;
	uint64_t id, generation;
	FvwmPicture *picture, *overlay;
	bool named_requested, overlay_requested, raw_requested, raw_overlay;
	bool named, named_overlay, attention_missing;
	uint64_t view_generation;
} native_cell;

struct tray_sni_ui
{
	tray_bus *bus;
	tray_icons *icons;
	native_cell cells[TRAY_SNI_MAX];
	picture_argb_builder *builder;
	uint32_t *pixels;
	uint64_t upload_id, upload_generation;
	bool upload_overlay, upload_canceled;
	size_t picture_bytes, upload_bytes;
	uint64_t next_view, next_request;
	struct {
		uint64_t token, id, generation, view;
		unsigned int role, node;
		bool attention;
	} icon_batch[TRAY_ICON_BATCH_MAX];
	unsigned int icon_batch_count;
	FvwmPicture *menu_icons[TRAY_SNI_MENU_MAX];
	bool menu_icon_requested[TRAY_SNI_MENU_MAX];
	uint64_t menu_icon_generation;
	unsigned int scale, size;
	char theme[129];
	Window menu, tooltip;
	uint64_t menu_generation, menu_item, menu_item_generation;
	uint64_t pending_menu_id, pending_menu_generation;
	int pending_menu_x, pending_menu_y;
	int32_t menu_parent, menu_stack[TRAY_SNI_MENU_DEPTH + 1];
	unsigned int menu_depth, menu_selected, menu_top;
	unsigned int menu_painted, menu_painted_top;
	tray_rect menu_rows_area;
	int menu_x, menu_y, menu_width, menu_height, menu_row;
	uint64_t hover_id, hover_generation;
	int hover_x, hover_y;
	double hover_deadline;
	bool requested, retry, stopping, tooltip_visible;
	bool helper_failed;
};

double tray_sni_now(void)
{
	struct timespec stamp;
	if (clock_gettime(CLOCK_MONOTONIC, &stamp) < 0)
		return 0;
	return stamp.tv_sec + stamp.tv_nsec / 1000000000.0;
}

static int scaled(tray_ui *ui, unsigned int value)
{
	int result = 0;
	if (monitor_scale_apply((int)value,
	    (monitor_scale_factor){ui->quarters}, &result) !=
	    MONITOR_SCALE_CONVERSION_OK)
		return (int)value;
	return result;
}

static const tray_sni_item *item(tray_ui *ui, uint64_t id,
	uint64_t generation)
{
	const tray_sni_model *model = tray_bus_model(ui->sni->bus);
	return model ? tray_sni_lookup(model, id, generation) : NULL;
}

static native_cell *cell(tray_ui *ui, uint64_t id)
{
	for (unsigned int i = 0; i < TRAY_SNI_MAX; i++)
		if (ui->sni->cells[i].id == id)
			return &ui->sni->cells[i];
	return NULL;
}

static size_t picture_cost(const FvwmPicture *picture)
{
	return picture ? (size_t)picture->width * picture->height * 6U : 0;
}

static void drop_picture(tray_ui *ui, FvwmPicture **picture)
{
	if (*picture)
	{
		size_t bytes = picture_cost(*picture);
		ui->sni->picture_bytes = bytes <= ui->sni->picture_bytes
		    ? ui->sni->picture_bytes - bytes : 0;
		PDestroyFvwmPicture(ui->display, *picture);
		*picture = NULL;
	}
}

static bool retain_picture(tray_ui *ui, FvwmPicture **slot,
	FvwmPicture *picture)
{
	if (!picture)
		return false;
	size_t cost = picture_cost(picture);
	const tray_sni_model *model = tray_bus_model(ui->sni->bus);
	size_t retained = ui->sni->picture_bytes +
	    (model ? model->icon_bytes : 0) + ui->sni->upload_bytes;
	if (retained > SNI_UI_IMAGE_MAX ||
	    cost > SNI_UI_IMAGE_MAX - retained)
	{
		PDestroyFvwmPicture(ui->display, picture);
		return false;
	}
	drop_picture(ui, slot);
	*slot = picture;
	ui->sni->picture_bytes += cost;
	return true;
}

static void hide_tooltip(tray_ui *ui)
{
	if (ui->sni->tooltip)
		XUnmapWindow(ui->display, ui->sni->tooltip);
	/* A queued Expose cannot revive an intentionally dismissed hover. */
	ui->sni->hover_id = 0;
	ui->sni->hover_generation = 0;
	ui->sni->hover_deadline = 0;
	ui->sni->tooltip_visible = false;
}

static void dismiss_menu(tray_ui *ui, bool restore)
{
	if (ui->sni->menu)
		XUnmapWindow(ui->display, ui->sni->menu);
	bool had_menu = ui->sni->menu_item != 0;
	ui->sni->menu_generation = 0;
	ui->sni->menu_item = 0;
	ui->sni->menu_depth = 0;
	ui->sni->menu_painted = 0;
	ui->sni->pending_menu_id = 0;
	const tray_sni_model *model = tray_bus_model(ui->sni->bus);
	if (model && model->menu.open)
		tray_bus_menu_close(ui->sni->bus);
	if (had_menu && restore)
	{
		if (ui->container.attached)
			tray_container_return(ui, 0);
		else
			tray_x11_unfocus(ui, true);
	}
}

static void close_menu(tray_ui *ui)
{
	dismiss_menu(ui, true);
}

static bool viewable(tray_ui *ui)
{
	XWindowAttributes attributes;
	return ui->visible && XGetWindowAttributes(ui->display, ui->host,
	    &attributes) && attributes.map_state == IsViewable;
}

static void deactivate(tray_ui *ui)
{
	dismiss_menu(ui, false);
	hide_tooltip(ui);
	ui->sni->hover_id = 0;
	ui->container.focus_pending = false;
}

void tray_sni_cancel_pending_focus(tray_ui *ui)
{
	if (ui->sni)
		ui->sni->pending_menu_id = 0;
}

void tray_sni_focus_out(tray_ui *ui)
{
	if (!ui->sni)
		return;
	Window focused = None;
	int revert;
	XGetInputFocus(ui->display, &focused, &revert);
	if (focused == ui->proxy || focused == ui->host ||
	    (ui->sni->menu && focused == ui->sni->menu) ||
	    (ui->sni->tooltip && focused == ui->sni->tooltip))
		return;
	for (unsigned int i = 0; i < ui->model.count; i++)
		if (focused == ui->sites[i].socket ||
		    focused == ui->model.clients[i].id)
			return;
	deactivate(ui);
}

void tray_sni_init(tray_ui *ui)
{
	if (ui->modern && !ui->sni)
		ui->sni = fxcalloc(1, sizeof(*ui->sni));
	/* Modern-off startup allocates no UI, bus/model or icon adapter. */
}

void tray_sni_configure(tray_ui *ui, bool retry)
{
	if (ui->modern && !ui->sni)
		tray_sni_init(ui);
	if (!ui->sni)
		return;
	if (ui->sni->requested != ui->modern || retry)
	{
		/* Appearance reloads preserve live item identities. */
		ui->sni->retry = ui->modern && (!ui->sni->requested ||
		    !tray_bus_connected(ui->sni->bus));
		ui->sni->helper_failed = false;
	}
	ui->sni->requested = ui->modern;
	if (ui->modern && tray_bus_available() && !ui->sni->bus)
	{
		ui->sni->bus = tray_bus_create();
		ui->sni->icons = tray_icons_create(NULL);
	}
	if (!ui->modern)
	{
		close_menu(ui);
		hide_tooltip(ui);
		tray_bus_close(ui->sni->bus);
		tray_icons_cancel(ui->sni->icons);
		tray_sni_sync(ui);
	}
}

void tray_sni_retired(tray_ui *ui, Window window)
{
	if (!ui->sni)
		return;
	for (unsigned int i = 0; i < TRAY_SNI_MAX; i++)
	{
		native_cell *record = &ui->sni->cells[i];
		if (record->window != window)
			continue;
		if (ui->sni->hover_id == record->id)
			hide_tooltip(ui);
		if (ui->sni->builder && ui->sni->upload_id == record->id)
		{
			picture_argb_cancel(ui->sni->builder);
			ui->sni->upload_canceled = true;
		}
		drop_picture(ui, &record->picture);
		drop_picture(ui, &record->overlay);
		*record = (native_cell){0};
		return;
	}
}

static void identify(tray_ui *ui, native_cell *record,
	const tray_sni_item *value)
{
	unsigned long words[5] = {1, (uint32_t)value->id,
	    (uint32_t)(value->id >> 32), (uint32_t)value->generation,
	    (uint32_t)(value->generation >> 32)};
	Atom property = XInternAtom(ui->display, "_FVWM_TRAY_SNI_ITEM", False);
	XChangeProperty(ui->display, record->window, property, XA_CARDINAL,
	    32, PropModeReplace, (unsigned char *)words, 5);
	const char *name = value->title[0] ? value->title : value->identity;
	XStoreName(ui->display, record->window, name);
	Atom utf8 = XInternAtom(ui->display, "UTF8_STRING", False);
	Atom net_name = XInternAtom(ui->display, "_NET_WM_NAME", False);
	XChangeProperty(ui->display, record->window, net_name, utf8, 8,
	    PropModeReplace, (const unsigned char *)name, strlen(name));
}

void tray_sni_sync(tray_ui *ui)
{
	if (!ui->sni)
		return;
	const tray_sni_model *model = tray_bus_model(ui->sni->bus);
	if (ui->sni->hover_id)
	{
		const tray_sni_item *hover = item(ui, ui->sni->hover_id,
		    ui->sni->hover_generation);
		if (!hover || !hover->ready || hover->status == TRAY_SNI_PASSIVE)
			hide_tooltip(ui);
	}
	for (unsigned int i = 0; i < ui->model.count;)
	{
		if (!ui->sites[i].native)
		{
			i++;
			continue;
		}
		if (!model || !tray_sni_lookup(model, ui->sites[i].item_id,
		    ui->sites[i].item_generation))
		{
			/* Same item may have a fresh immutable property
			 * generation. */
			const tray_sni_item *fresh = NULL;
			if (model)
				for (unsigned int j = 0; j < model->count; j++)
					if (model->items[j].id ==
					    ui->sites[i].item_id)
						fresh = &model->items[j];
			if (!fresh)
			{
				tray_x11_retire(ui, i);
				continue;
			}
			ui->sites[i].item_generation = fresh->generation;
		}
		i++;
	}
	if (!model)
		return;
	int extent = scaled(ui, ui->icon_size > 24 ? ui->icon_size : 24);
	if (extent < 1 || extent > 4096)
		return;
	bool changed = ui->sni->scale != ui->quarters ||
	    ui->sni->size != ui->icon_size ||
	    strcmp(ui->sni->theme, ui->icon_theme);
	ui->sni->scale = ui->quarters;
	ui->sni->size = ui->icon_size;
	strcpy(ui->sni->theme, ui->icon_theme);
	if (changed)
		ui->sni->menu_icon_generation = 0;
	for (unsigned int i = 0; i < model->count; i++)
	{
		const tray_sni_item *value = &model->items[i];
		if (!value->ready)
			continue;
		native_cell *record = cell(ui, value->id);
		if (!record)
		{
			if (ui->model.count == TRAY_CLIENT_MAX)
				break;
			for (unsigned int j = 0; j < TRAY_SNI_MAX; j++)
				if (!ui->sni->cells[j].id)
				{
					record = &ui->sni->cells[j];
					break;
				}
			if (!record)
				break;
			record->window = XCreateSimpleWindow(ui->display,
			    ui->host,
			    0, 0, extent, extent, 0, 0, 0);
			record->id = value->id;
			XSelectInput(ui->display, record->window,
			    ExposureMask | ButtonPressMask | PointerMotionMask |
			    EnterWindowMask | LeaveWindowMask);
			unsigned int index = ui->model.count;
			if (tray_model_add_native(&ui->model, record->window,
			    extent, extent,
			        value->status != TRAY_SNI_PASSIVE) !=
			    TRAY_ADMITTED)
			{
				XDestroyWindow(ui->display, record->window);
				*record = (native_cell){0};
				break;
			}
			ui->sites[index] = (tray_site){.socket = record->window,
			    .native = true, .item_id = value->id,
			    .item_generation = value->generation};
			ui->dirty = true;
		}
		if (record->generation != value->generation || changed)
		{
			if (ui->sni->builder && ui->sni->upload_id == value->id)
			{
				picture_argb_cancel(ui->sni->builder);
				ui->sni->upload_canceled = true;
			}
			record->generation = value->generation;
			record->named_requested =
			    record->overlay_requested = false;
			record->raw_requested = record->raw_overlay = false;
			record->named = record->named_overlay = false;
			record->attention_missing = false;
			if (ui->sni->next_view != UINT64_MAX)
				record->view_generation = ++ui->sni->next_view;
			drop_picture(ui, &record->picture);
			drop_picture(ui, &record->overlay);
			identify(ui, record, value);
			tray_model_resize_native(&ui->model, record->window,
			    extent, extent);
			tray_model_mapped(&ui->model, record->window,
			    value->status != TRAY_SNI_PASSIVE);
			ui->dirty = true;
		}
	}
}

static void text(tray_ui *ui, Window window, const char *label,
	int x, int y, int width, int height)
{
	if (!label || !*label || width < 1 || height < 1)
		return;
	XRectangle clip = {x, y, width, height};
	FlocaleWinString string = {0};
	XSetForeground(ui->display, ui->gc, GetColorset(ui->colorset)->fg);
	if (ui->font->font)
		XSetFont(ui->display, ui->gc, ui->font->font->fid);
	string.str = (char *)label;
	string.len = strlen(label);
	string.gc = ui->gc;
	string.win = window;
	string.colorset = GetColorset(ui->colorset);
	string.flags.has_colorset = 1;
	string.x = x;
	string.y = y + ui->font->ascent;
	string.clip_region = XCreateRegion();
	string.flags.has_clip_region = 1;
	XUnionRectWithRegion(&clip, string.clip_region, string.clip_region);
	FlocaleDrawString(ui->display, ui->font, &string, FWS_HAVE_LENGTH);
	XDestroyRegion(string.clip_region);
}

static void background(tray_ui *ui, Window window, int width, int height)
{
	SetWindowBackground(ui->display, window, width, height,
	    GetColorset(ui->colorset), DefaultDepth(ui->display,
	    DefaultScreen(ui->display)), ui->gc, False);
	XClearWindow(ui->display, window);
}

static void paint_picture(tray_ui *ui, Window window, FvwmPicture *picture,
	int x, int y, int size)
{
	if (picture && size > 0)
		PGraphicsRenderPicture(ui->display, window, picture, NULL,
		    window, ui->gc, None, None, 0, 0,
		    picture->width, picture->height, x, y, size, size, False);
}

void tray_sni_paint(tray_ui *ui, unsigned int index)
{
	if (!ui->sni || index >= ui->model.count || !ui->sites[index].native)
		return;
	native_cell *record = cell(ui, ui->sites[index].item_id);
	const tray_sni_item *value = item(ui, ui->sites[index].item_id,
	    ui->sites[index].item_generation);
	if (!record || !value)
		return;
	const tray_client *entry = &ui->model.clients[index];
	background(ui, record->window, entry->width, entry->height);
	int size = scaled(ui, ui->icon_size);
	int x = ((int)entry->width - size) / 2;
	int y = ((int)entry->height - size) / 2;
	if (record->picture)
		paint_picture(ui, record->window, record->picture, x, y, size);
	else
	{
		const char *name = value->title[0] ?
		    value->title : value->identity;
		text(ui, record->window, name, 1,
		    ((int)entry->height - ui->font->height) / 2,
		    entry->width - 2, ui->font->height);
	}
	if (record->overlay)
		paint_picture(ui, record->window, record->overlay,
		    x + size / 2, y + size / 2, size / 2);
	if (ui->model.focused == record->window && entry->width > 2 &&
	    entry->height > 2)
	{
		XSetForeground(ui->display, ui->gc,
		    GetColorset(ui->colorset)->fg);
		XDrawRectangle(ui->display, record->window, ui->gc, 0, 0,
		    entry->width - 1, entry->height - 1);
	}
}

static void start_upload(tray_ui *ui, native_cell *record,
	const tray_sni_pixmap *pixmap, bool overlay)
{
	if (!pixmap->pixels || ui->sni->builder)
		return;
	if (!pixmap->width || !pixmap->height || pixmap->width > 256 ||
	    pixmap->height > 256)
		return;
	int side = scaled(ui, ui->icon_size);
	if (overlay)
		side /= 2;
	if (side < 1 || side > (int)TRAY_SNI_UPLOAD_SIDE_MAX)
		return;
	size_t count = (size_t)side * side;
	size_t source_count = (size_t)pixmap->width * pixmap->height;
	const tray_sni_model *model = tray_bus_model(ui->sni->bus);
	size_t retained = ui->sni->picture_bytes +
	    (model ? model->icon_bytes : 0);
	/* Reserve normalized input plus conservative RGB/mask/alpha server
	 * storage before starting an upload. Replacements retain the old
	 * lease. */
	if (retained > SNI_UI_IMAGE_MAX ||
	    count * 10U > SNI_UI_IMAGE_MAX - retained)
		return;
	ui->sni->upload_bytes = count * 10U;
	ui->sni->pixels = fxmalloc(count * sizeof(uint32_t));
	if (!tray_sni_resample_argb(pixmap->pixels, source_count,
	    pixmap->width, pixmap->height, ui->sni->pixels, count,
	    (unsigned int)side, (unsigned int)side))
	{
		free(ui->sni->pixels);
		ui->sni->pixels = NULL;
		ui->sni->upload_bytes = 0;
		return;
	}
	ui->sni->builder = picture_argb_start(ui->display, ui->root,
	    ui->sni->pixels, count, (unsigned int)side, (unsigned int)side,
	    (FvwmPictureAttributes){0});
	if (!ui->sni->builder)
	{
		free(ui->sni->pixels);
		ui->sni->pixels = NULL;
		ui->sni->upload_bytes = 0;
		return;
	}
	ui->sni->upload_id = record->id;
	ui->sni->upload_generation = record->generation;
	ui->sni->upload_overlay = overlay;
	ui->sni->upload_canceled = false;
}

static void service_upload(tray_ui *ui)
{
	tray_sni_ui *state = ui->sni;
	if (!state->builder || !picture_argb_step(state->builder))
		return;
	FvwmPicture *picture = picture_argb_take(state->builder);
	(void)picture_argb_destroy(state->builder);
	state->builder = NULL;
	free(state->pixels);
	state->pixels = NULL;
	state->upload_bytes = 0;
	native_cell *record = cell(ui, state->upload_id);
	if (picture && !state->upload_canceled && record &&
	    record->generation == state->upload_generation &&
	    !(state->upload_overlay ? record->named_overlay : record->named))
	{
		(void)retain_picture(ui, state->upload_overlay
		    ? &record->overlay : &record->picture, picture);
		ui->dirty = true;
	}
	else if (picture)
		PDestroyFvwmPicture(ui->display, picture);
	state->upload_id = 0;
}

static bool clean_icon_root(const char *path)
{
	if (!path[0])
		return true;
	if (path[0] != '/')
		return false;
	const char *component = path + 1;
	if (!*component)
		return true;
	for (const char *at = component;; at++)
		if (*at == '/' || !*at)
		{
			size_t length = (size_t)(at - component);
			if (!length || (length == 1 && component[0] == '.') ||
			    (length == 2 && component[0] == '.' &&
			     component[1] == '.'))
				return false;
			if (!*at)
				return true;
			component = at + 1;
		}
}

static bool add_icon_request(tray_ui *ui, tray_icon_request *requests,
	unsigned int *count, const char *name, const char *theme_path,
	uint64_t id, uint64_t generation, uint64_t view, unsigned int role,
	unsigned int node)
{
	tray_sni_ui *state = ui->sni;
	if (*count == TRAY_ICON_BATCH_MAX || !name[0] ||
	    strlen(name) > TRAY_ICON_NAME_MAX ||
	        state->next_request == UINT64_MAX)
		return false;
	unsigned int index = (*count)++;
	uint64_t token = ++state->next_request;
	unsigned int size = role == 1 ? ui->icon_size / 2 :
	    role == 2 ? 16U : ui->icon_size;
	if (size < 8)
		size = 8;
	requests[index] = (tray_icon_request){token, view, size,
	    ui->quarters, name, ui->icon_theme,
	    clean_icon_root(theme_path) ? theme_path : ""};
	state->icon_batch[index].token = token;
	state->icon_batch[index].id = id;
	state->icon_batch[index].generation = generation;
	state->icon_batch[index].view = view;
	state->icon_batch[index].role = role;
	state->icon_batch[index].node = node;
	state->icon_batch[index].attention = false;
	return true;
}

static void request_normal_fallback(native_cell *record, bool attention)
{
	/* Call only after matching the retained item and view generations. */
	if (record && attention && !record->attention_missing)
	{
		record->attention_missing = true;
		record->named_requested = false;
	}
}

static void acquire_icons(tray_ui *ui, double stamp)
{
	tray_sni_ui *state = ui->sni;
	if (!state->icons)
		return;
	if (tray_icons_ready(state->icons))
	{
		for (unsigned int i = 0; i < tray_icons_count(state->icons);
		    i++)
		{
			const tray_icon_result *result =
			    tray_icons_result(state->icons, i);
			if (!result)
				continue;
			unsigned int request = 0;
			while (request < state->icon_batch_count &&
			    state->icon_batch[request].token != result->id)
				request++;
			if (request == state->icon_batch_count ||
			    state->icon_batch[request].view !=
			    result->generation)
				continue;
			unsigned int role = state->icon_batch[request].role;
			native_cell *record = role < 2 ?
			    cell(ui, state->icon_batch[request].id) : NULL;
			const tray_sni_model *model =
			    tray_bus_model(state->bus);
			if (role < 2 && (!record || record->generation !=
			    state->icon_batch[request].generation ||
			    record->view_generation != result->generation))
				continue;
			if (role == 2 && (!model || model->menu.generation !=
			    result->generation ||
			        state->icon_batch[request].node >=
			    model->menu.count))
				continue;
			if (!result->path[0])
			{
				/* One fallback per current attention miss. */
				request_normal_fallback(record,
				    state->icon_batch[request].attention);
				continue;
			}
			int desired = scaled(ui,
			    role == 2 ? 16 : ui->icon_size);
			if (role == 1)
				desired /= 2;
			if (desired < 1 ||
			    desired > (int)TRAY_SNI_UPLOAD_SIDE_MAX)
				continue;
			/* Sources stay bounded to256; loading uses the exact
			 * physical paint target. */
			unsigned int estimate_side = desired < 256 ? 256 :
			    (unsigned int)desired;
			size_t estimate =
			    (size_t)estimate_side * estimate_side * 6U;
			size_t retained =
			    state->picture_bytes + state->upload_bytes +
			    (model ? model->icon_bytes : 0);
			if (retained > SNI_UI_IMAGE_MAX ||
			    estimate > SNI_UI_IMAGE_MAX - retained)
				continue;
			FvwmPicture *picture =
			    PCacheFvwmPictureAtScale(ui->display, ui->root,
			    NULL, result->path, (FvwmPictureAttributes){0},
			    (monitor_scale_factor){4},
			    (unsigned int)desired, (unsigned int)desired,
			    WM_UI_RESOURCE_GENERATION_INITIAL);
			if (!picture)
				request_normal_fallback(record,
				    state->icon_batch[request].attention);
			if (picture)
			{
				unsigned int node =
				    state->icon_batch[request].node;
				FvwmPicture **target = role == 2
				    ? &state->menu_icons[node]
				    : role == 1 ?
				        &record->overlay : &record->picture;
				if (retain_picture(ui, target, picture) &&
				    record)
				{
					if (role == 1)
						record->named_overlay = true;
					else
						record->named = true;
				}
				ui->dirty = true;
			}
		}
		tray_icons_release(state->icons);
		state->icon_batch_count = 0;
	}
	if (tray_icons_busy(state->icons) || state->stopping)
		return;
	const char *failure = tray_icons_error(state->icons);
	if (failure && *failure)
	{
		state->helper_failed = true;
		fvwm_debug(__func__, "named tray icons unavailable: %s",
		    failure);
		tray_icons_release(state->icons);
		return;
	}
	if (state->helper_failed)
		return;
	tray_icon_request requests[TRAY_ICON_BATCH_MAX];
	unsigned int count = 0;
	for (unsigned int i = 0; i < TRAY_SNI_MAX &&
	    count < TRAY_ICON_BATCH_MAX; i++)
	{
		native_cell *record = &state->cells[i];
		const tray_sni_item *value = item(ui, record->id,
		    record->generation);
		if (!value)
			continue;
		bool attention = value->status == TRAY_SNI_ATTENTION &&
		    value->attention_name[0] && !record->attention_missing &&
		    strlen(value->attention_name) <= TRAY_ICON_NAME_MAX;
		bool attention_pixels = value->status == TRAY_SNI_ATTENTION &&
		    value->attention.pixels;
		const char *name = attention ? value->attention_name :
		    value->icon_name;
		if (!record->named_requested)
		{
			record->named_requested = true;
			/* Attention pixels precede normal-name fallback. */
			if ((attention || !attention_pixels) &&
			    add_icon_request(ui, requests, &count, name,
			    value->icon_theme_path, record->id,
			        record->generation,
			    record->view_generation, 0, 0))
				state->icon_batch[count - 1].attention =
				    attention;
		}
		if (!record->overlay_requested && count < TRAY_ICON_BATCH_MAX)
		{
			record->overlay_requested = true;
			(void)add_icon_request(ui, requests, &count,
			    value->overlay_name,
			    value->icon_theme_path, record->id,
			        record->generation,
			    record->view_generation, 1, 0);
		}
	}
	const tray_sni_model *model = tray_bus_model(state->bus);
	if (model && model->menu.open && model->menu.ready &&
	    !model->menu.stale)
	{
		if (state->menu_icon_generation != model->menu.generation)
		{
			for (unsigned int i = 0; i < TRAY_SNI_MENU_MAX; i++)
				drop_picture(ui, &state->menu_icons[i]);
			memset(state->menu_icon_requested, 0,
			    sizeof(state->menu_icon_requested));
			state->menu_icon_generation = model->menu.generation;
		}
		for (unsigned int i = 0; i < model->menu.count &&
		    count < TRAY_ICON_BATCH_MAX; i++)
		{
			const tray_sni_menu_node *node = &model->menu.nodes[i];
			if (state->menu_icon_requested[i] ||
			    node->parent != model->menu.parent ||
			        !node->visible)
				continue;
			state->menu_icon_requested[i] = true;
			(void)add_icon_request(ui, requests, &count,
			    node->icon_name,
			    "", model->menu.item_id,
			        model->menu.item_generation,
			    model->menu.generation, 2, i);
		}
	}
	if (count)
	{
		state->icon_batch_count = count;
		(void)tray_icons_begin(state->icons, requests, count, stamp);
	}
}

static Window popup(tray_ui *ui, const char *name)
{
	XSetWindowAttributes attributes = {0};
	attributes.override_redirect = True;
	attributes.event_mask = ExposureMask | ButtonPressMask |
	    KeyPressMask | PointerMotionMask | LeaveWindowMask;
	Window window = XCreateWindow(ui->display, ui->root, 0, 0, 1, 1,
	    0, CopyFromParent, InputOutput, CopyFromParent,
	    CWOverrideRedirect | CWEventMask, &attributes);
	XStoreName(ui->display, window, name);
	return window;
}

static tray_rect popup_area(tray_ui *ui, int x, int y)
{
	const module_ui_scale_row *row;
	rectangle point = {x, y, 1, 1};
	row = module_ui_scale_select(&ui->monitors.published, &point, None);
	module_monitor_area bounds;
	if (row && module_workarea_bounds(&ui->workareas,
	    &ui->monitors.published, row, &bounds))
		return (tray_rect){bounds.x, bounds.y, bounds.width,
		    bounds.height};
	XWindowAttributes root;
	if (XGetWindowAttributes(ui->display, ui->root, &root))
		return (tray_rect){0, 0, root.width, root.height};
	return (tray_rect){0, 0, 1, 1};
}

static void tooltip(tray_ui *ui)
{
	tray_sni_ui *state = ui->sni;
	const tray_sni_item *value = item(ui, state->hover_id,
	    state->hover_generation);
	if (!value || !value->ready || value->status == TRAY_SNI_PASSIVE ||
	    !viewable(ui) || state->menu_item)
	{
		hide_tooltip(ui);
		return;
	}
	if (!state->tooltip)
		state->tooltip = popup(ui, "Tray tooltip");
	const char *title = value->tooltip_title[0] ? value->tooltip_title :
	    value->title[0] ? value->title : value->identity;
	int pad = scaled(ui, 4), line = ui->font->height;
	int width = scaled(ui, 320), height = pad * 2 + line * 3;
	tray_rect area = popup_area(ui, state->hover_x, state->hover_y);
	if (width > area.width)
		width = area.width;
	if (height > area.height)
		height = area.height;
	int x = state->hover_x, y = state->hover_y + scaled(ui, 16);
	if (x + width > area.x + area.width)
		x = area.x + area.width - width;
	if (y + height > area.y + area.height)
		y = state->hover_y - height;
	if (x < area.x)
		x = area.x;
	if (y < area.y)
		y = area.y;
	XMoveResizeWindow(ui->display, state->tooltip, x, y, width, height);
	background(ui, state->tooltip, width, height);
	text(ui, state->tooltip, title, pad, pad, width - pad * 2, line);
	char body[TRAY_SNI_TEXT_MAX + 1];
	strcpy(body, value->tooltip_body);
	char *first = body, *second = strchr(body, '\n');
	if (second)
		*second++ = 0;
	text(ui, state->tooltip, first, pad, pad + line, width - pad * 2, line);
	if (second)
	{
		char *end = strchr(second, '\n');
		if (end)
			*end = 0;
		text(ui, state->tooltip, second, pad, pad + line * 2,
		    width - pad * 2, line);
	}
	XMapRaised(ui->display, state->tooltip);
	state->tooltip_visible = true;
}

static unsigned int menu_rows(tray_ui *ui,
	const tray_sni_menu_node *rows[TRAY_SNI_MENU_MAX])
{
	const tray_sni_model *model = tray_bus_model(ui->sni->bus);
	if (!model || !model->menu.open || !model->menu.ready ||
	    model->menu.stale)
		return 0;
	unsigned int count = 0;
	for (unsigned int i = 0; i < model->menu.count; i++)
	{
		const tray_sni_menu_node *node = &model->menu.nodes[i];
		if (node->id != model->menu.root &&
		    node->parent == model->menu.parent && node->visible)
			rows[count++] = node;
	}
	return count;
}

static void menu_label(const tray_sni_menu_node *node, char *label,
	size_t size)
{
	const char *toggle = node->toggle_type == 2
	    ? (node->toggle_state < 0 ? "(-) " :
	      node->toggle_state ? "(*) " : "( ) ")
	    : node->toggle_type == 1
	    ? (node->toggle_state < 0 ? "[-] " :
	      node->toggle_state ? "[x] " : "[ ] ") : "    ";

	snprintf(label, size, "%s%s%s", toggle,
	    node->enabled ? "" : "- ", node->label);
}

static void paint_menu(tray_ui *ui)
{
	tray_sni_ui *state = ui->sni;
	state->menu_painted = 0;
	const tray_sni_model *model = tray_bus_model(state->bus);
	const tray_sni_menu_node *rows[TRAY_SNI_MENU_MAX];
	unsigned int count = menu_rows(ui, rows);
	if (!state->menu_item || !viewable(ui) || !model || !model->menu.open)
	{
		if (state->menu)
			XUnmapWindow(ui->display, state->menu);
		return;
	}
	if (state->menu_icon_generation != model->menu.generation)
	{
		for (unsigned int i = 0; i < TRAY_SNI_MENU_MAX; i++)
			drop_picture(ui, &state->menu_icons[i]);
		memset(state->menu_icon_requested, 0,
		    sizeof(state->menu_icon_requested));
		state->menu_icon_generation = model->menu.generation;
	}
	if (!state->menu)
		state->menu = popup(ui, "Tray application menu");
	if (model->menu.ready && !model->menu.stale &&
	    state->menu_generation != model->menu.generation)
	{
		state->menu_generation = model->menu.generation;
		state->menu_parent = model->menu.parent;
		state->menu_selected = state->menu_top = 0;
	}
	if (state->menu_selected >= count)
		state->menu_selected = 0;
	int pad = scaled(ui, 4), row_height = ui->font->height + pad * 2;
	if (row_height < scaled(ui, 24))
		row_height = scaled(ui, 24);
	int width = scaled(ui, 280);
	tray_rect area = popup_area(ui, state->menu_x, state->menu_y);
	/* Measure the same bounded labels we paint, including state and icon
	 * columns. A fixed logical width clips ordinary enlarged-font menus. */
	int columns = pad * 2 + scaled(ui, 32);
	for (unsigned int i = 0; i < count; i++)
	{
		char label[544];
		menu_label(rows[i], label, sizeof(label));
		int measured = FlocaleTextWidth(ui->font, label, strlen(label));
		int wanted = measured > area.width - columns
		    ? area.width : measured + columns;
		if (wanted > width)
			width = wanted;
	}
	int fitting = (area.height - pad * 2) / row_height;
	unsigned int viewport = fitting < 1 ? 1U :
	    fitting > (int)SNI_MENU_ROWS ?
	        SNI_MENU_ROWS : (unsigned int)fitting;
	if (state->menu_selected < state->menu_top)
		state->menu_top = state->menu_selected;
	if (state->menu_selected >= state->menu_top + viewport)
		state->menu_top = state->menu_selected - viewport + 1;
	unsigned int visible = count > viewport ? viewport : count;
	if (!visible)
		visible = 1;
	int height = pad * 2 + (int)visible * row_height;
	if (width > area.width)
		width = area.width;
	if (height > area.height)
		height = area.height;
	int x = state->menu_x, y = state->menu_y;
	if (x + width > area.x + area.width)
		x = area.x + area.width - width;
	if (y + height > area.y + area.height)
		y = area.y + area.height - height;
	if (x < area.x)
		x = area.x;
	if (y < area.y)
		y = area.y;
	state->menu_width = width;
	state->menu_height = height;
	state->menu_row = row_height;
	state->menu_painted_top = state->menu_top;
	state->menu_painted = count > state->menu_top ?
	    count - state->menu_top : 0;
	if (state->menu_painted > visible)
		state->menu_painted = visible;
	int rows_height = height - pad * 2;
	if (rows_height > (int)state->menu_painted * row_height)
		rows_height = (int)state->menu_painted * row_height;
	state->menu_rows_area = (tray_rect){pad, pad, width - pad * 2,
	    rows_height};
	XMoveResizeWindow(ui->display, state->menu, x, y, width, height);
	background(ui, state->menu, width, height);
	if (!count)
		text(ui, state->menu, model->menu.stale || !model->menu.ready
		    ? "Updating actions..." : "No actions", pad, pad,
		    width - pad * 2, row_height);
	for (unsigned int row = 0; row < visible &&
	    state->menu_top + row < count; row++)
	{
		unsigned int index = state->menu_top + row;
		const tray_sni_menu_node *node = rows[index];
		int y_position = pad + (int)row * row_height;
		if (node->separator)
		{
			XSetForeground(ui->display, ui->gc,
			    GetColorset(ui->colorset)->fg);
			XDrawLine(ui->display, state->menu, ui->gc, pad,
			    y_position + row_height / 2, width - pad,
			    y_position + row_height / 2);
			continue;
		}
		char label[544];
		menu_label(node, label, sizeof(label));
		unsigned int node_index =
		    (unsigned int)(node - model->menu.nodes);
		int icon_size = scaled(ui, 16);
		if (node_index < TRAY_SNI_MENU_MAX)
			paint_picture(ui, state->menu,
			    state->menu_icons[node_index],
			    pad, y_position + (row_height - icon_size) / 2,
			        icon_size);
		text(ui, state->menu, label, pad + icon_size,
		    y_position + (row_height - ui->font->height) / 2,
		    width - pad * 2 - icon_size - scaled(ui, 16),
		    ui->font->height);
		if (node->submenu)
			text(ui, state->menu, ">", width - pad - scaled(ui, 12),
			    y_position + (row_height - ui->font->height) / 2,
			    scaled(ui, 12), ui->font->height);
		if (index == state->menu_selected)
		{
			XSetForeground(ui->display, ui->gc,
			    GetColorset(ui->colorset)->fg);
			XDrawRectangle(ui->display, state->menu, ui->gc, 0,
			    y_position, width - 1, row_height - 1);
		}
	}
	/* Observe only the current painted view; this property grants no action
	 * authority. A loading/invalidated view publishes zero selectable rows. */
	unsigned long view[7] = {1, (uint32_t)state->menu_generation,
	    (uint32_t)(state->menu_generation >> 32),
	    (uint32_t)state->menu_parent, state->menu_painted,
	    state->menu_selected, model->menu.ready && !model->menu.stale};
	Atom view_atom = XInternAtom(ui->display, "_FVWM_TRAY_MENU_VIEW", False);
	XChangeProperty(ui->display, state->menu, view_atom, XA_CARDINAL,
	    32, PropModeReplace, (unsigned char *)view, 7);
	XMapRaised(ui->display, state->menu);
}

static void open_menu(tray_ui *ui, const tray_sni_item *value,
	int x, int y, double stamp)
{
	hide_tooltip(ui);
	tray_sni_ui *state = ui->sni;
	if (!viewable(ui))
		return;
	native_cell *record = cell(ui, value->id);
	if (tray_container_focus_request(ui, record ? record->window : 0,
	    0, CurrentTime))
	{
		state->pending_menu_id = value->id;
		state->pending_menu_generation = value->generation;
		state->pending_menu_x = x;
		state->pending_menu_y = y;
		return;
	}
	state->menu_item = value->id;
	state->menu_item_generation = value->generation;
	state->menu_x = x;
	state->menu_y = y;
	state->menu_depth = 0;
	state->menu_generation = 0;
	tray_bus_menu_result result = tray_bus_menu_open(state->bus,
	    value->id, value->generation, 0, x, y, stamp);
	if (result != TRAY_BUS_MENU_NATIVE)
	{
		/* External ContextMenu is already submitted. Retire the old local
		 * popup and input scope; it cannot become the new item's menu. */
		close_menu(ui);
		if (result == TRAY_BUS_MENU_FAILED)
			(void)tray_bus_action(state->bus, value->id,
			    value->generation, TRAY_BUS_CONTEXT, x, y,
			    0, true, stamp);
		return;
	}
	tray_x11_focus(ui, record ? record->window : 0, 0, CurrentTime);
	paint_menu(ui);
}

static void activate_menu(tray_ui *ui, unsigned int selected,
	uint32_t timestamp)
{
	const tray_sni_menu_node *rows[TRAY_SNI_MENU_MAX];
	unsigned int count = menu_rows(ui, rows);
	if (selected >= count)
		return;
	const tray_sni_menu_node *node = rows[selected];
	if (!node->enabled || node->separator)
		return;
	tray_sni_ui *state = ui->sni;
	if (node->submenu)
	{
		if (state->menu_depth >= TRAY_SNI_MENU_DEPTH)
			return;
		state->menu_stack[state->menu_depth++] = state->menu_parent;
		if (tray_bus_menu_open(state->bus, state->menu_item,
		    state->menu_item_generation, node->id,
		    state->menu_x, state->menu_y, tray_sni_now()) !=
		    TRAY_BUS_MENU_NATIVE)
			close_menu(ui);
	}
	else if (tray_bus_menu_click(state->bus, state->menu_generation,
	    node->id, timestamp, tray_sni_now()))
		close_menu(ui);
	paint_menu(ui);
}

static void menu_move(tray_ui *ui, int direction)
{
	const tray_sni_menu_node *rows[TRAY_SNI_MENU_MAX];
	unsigned int count = menu_rows(ui, rows);
	if (!count)
		return;
	for (unsigned int i = 0; i < count; i++)
	{
		ui->sni->menu_selected = (ui->sni->menu_selected + count +
		    direction) % count;
		const tray_sni_menu_node *node = rows[ui->sni->menu_selected];
		if (node->enabled && !node->separator)
			break;
	}
	paint_menu(ui);
}

static bool menu_key(tray_ui *ui, KeySym key, Time timestamp)
{
	tray_sni_ui *state = ui->sni;
	if (!state->menu_item)
		return false;
	if (key == XK_Escape || key == XK_Left)
	{
		if (state->menu_depth)
		{
			int32_t parent = state->menu_stack[--state->menu_depth];
			if (tray_bus_menu_open(state->bus, state->menu_item,
			    state->menu_item_generation, parent,
			    state->menu_x, state->menu_y, tray_sni_now()) !=
			    TRAY_BUS_MENU_NATIVE)
				close_menu(ui);
			paint_menu(ui);
		}
		else
			close_menu(ui);
	}
	else if (key == XK_Down || key == XK_Up)
		menu_move(ui, key == XK_Up ? -1 : 1);
	else if (key == XK_Return || key == XK_KP_Enter || key == XK_space ||
	    key == XK_Right)
		activate_menu(ui, state->menu_selected, timestamp);
	return true;
}

bool tray_sni_event(tray_ui *ui, XEvent *event)
{
	if (!ui->sni || event->xany.send_event)
		return false;
	tray_sni_ui *state = ui->sni;
	if ((state->menu_item || state->tooltip_visible ||
	    state->pending_menu_id) && !viewable(ui))
		deactivate(ui);
	if (state->menu_item &&
	    (((event->type == ButtonPress || event->type == MotionNotify ||
	        event->type == KeyPress) &&
	        event->xany.window == state->menu) ||
	        (event->type == KeyPress && event->xany.window == ui->proxy)))
	{
		tray_sni_focus_out(ui);
		if (!state->menu_item)
			return true;
	}
	if (event->type == Expose && event->xexpose.window == state->menu)
	{
		paint_menu(ui);
		return true;
	}
	if (event->type == Expose && event->xexpose.window == state->tooltip)
	{
		if (state->tooltip_visible)
			tooltip(ui);
		return true;
	}
	if (event->type == ButtonPress && event->xbutton.window == state->menu)
	{
		if (event->xbutton.button == 4 || event->xbutton.button == 5)
			menu_move(ui, event->xbutton.button == 4 ? -1 : 1);
		else if (event->xbutton.button == 1)
		{
			unsigned int row;
			if (tray_model_row_at(&state->menu_rows_area,
			    state->menu_row, state->menu_painted,
			    event->xbutton.x, event->xbutton.y, &row))
				activate_menu(ui, state->menu_painted_top + row,
				    event->xbutton.time);
		}
		else if (event->xbutton.button == 3)
			close_menu(ui);
		return true;
	}
	if (event->type == MotionNotify && event->xmotion.window == state->menu)
	{
		unsigned int row;
		if (tray_model_row_at(&state->menu_rows_area,
		    state->menu_row, state->menu_painted,
		    event->xmotion.x, event->xmotion.y, &row))
			state->menu_selected = state->menu_painted_top + row;
		paint_menu(ui);
		return true;
	}
	if (event->type == KeyRelease && event->xkey.window == ui->proxy)
	{
		int focused = tray_model_index(&ui->model, ui->model.focused);
		if (state->menu_item || (focused >= 0 &&
		    ui->sites[focused].native))
			return true;
	}
	if (event->type == KeyPress &&
	    (event->xkey.window == ui->proxy ||
	        event->xkey.window == state->menu))
	{
		Window focused = None;
		int revert;
		XGetInputFocus(ui->display, &focused, &revert);
		if (!viewable(ui) || (focused != ui->proxy &&
		    (!state->menu || focused != state->menu)))
			return true;
		KeySym key = XLookupKeysym(&event->xkey, 0);
		if (menu_key(ui, key, event->xkey.time))
			return true;
		int index = tray_model_index(&ui->model, ui->model.focused);
		if (index >= 0 && ui->sites[index].native)
		{
			const tray_sni_item *value = item(ui,
			    ui->sites[index].item_id,
			    ui->sites[index].item_generation);
			if (!value)
				return true;
			Window child;
			int x = 0, y = 0;
			XTranslateCoordinates(ui->display, ui->model.focused,
			    ui->root, 0, 0, &x, &y, &child);
			if (key == XK_Return || key == XK_KP_Enter)
			{
				if (value->item_is_menu)
					open_menu(ui, value, x, y,
					    tray_sni_now());
				else
					(void)tray_bus_action(state->bus,
					    value->id,
					    value->generation,
					        TRAY_BUS_ACTIVATE,
					    x, y, 0, true, tray_sni_now());
				return true;
			}
			if (key == XK_space || key == XK_Menu)
			{
				open_menu(ui, value, x, y, tray_sni_now());
				return true;
			}
			if (key == XK_Tab || key == XK_ISO_Left_Tab)
			{
				tray_x11_focus(ui, 0,
				    event->xkey.state & ShiftMask ? -1 : 1,
				    event->xkey.time);
				return true;
			}
			if (key == XK_Left || key == XK_Up || key == XK_Right ||
			    key == XK_Down)
			{
				tray_x11_focus(ui, 0,
				    key == XK_Left || key == XK_Up ? -1 : 1,
				    event->xkey.time);
				return true;
			}
		}
	}
	for (unsigned int i = 0; i < ui->model.count; i++)
	{
		if (!ui->sites[i].native ||
		    event->xany.window != ui->model.clients[i].id)
			continue;
		const tray_sni_item *value = item(ui, ui->sites[i].item_id,
		    ui->sites[i].item_generation);
		if (!value)
			return true;
		if (event->type == Expose)
			tray_sni_paint(ui, i);
		else if (event->type == ButtonPress)
		{
			hide_tooltip(ui);
			int button = event->xbutton.button;
			if (button < 1 || button > 7)
				return true;
			if (!viewable(ui))
				return true;
			if (button == 3 || (button == 1 && value->item_is_menu))
				open_menu(ui, value, event->xbutton.x_root,
				    event->xbutton.y_root, tray_sni_now());
			else
			{
				if (ui->container.attached)
					tray_x11_focus(ui,
					    ui->model.clients[i].id, 0,
					    event->xbutton.time);
				(void)tray_bus_action(state->bus, value->id,
				    value->generation, button == 2 ?
				        TRAY_BUS_SECONDARY :
				    button >= 4 ? TRAY_BUS_SCROLL :
				    TRAY_BUS_ACTIVATE, event->xbutton.x_root,
				    event->xbutton.y_root, button == 4 ||
				        button == 6
				    ? 120 : -120, button < 6, tray_sni_now());
			}
		}
		else if (event->type == MotionNotify || event->type == EnterNotify)
		{
			if (state->hover_id != value->id ||
			    state->hover_generation != value->generation ||
			    (!state->tooltip_visible && !state->hover_deadline))
			{
				hide_tooltip(ui);
				state->hover_id = value->id;
				state->hover_generation = value->generation;
				state->hover_deadline = tray_sni_now() + .5;
			}
			state->hover_x = event->type == EnterNotify ?
			    event->xcrossing.x_root : event->xmotion.x_root;
			state->hover_y = event->type == EnterNotify ?
			    event->xcrossing.y_root : event->xmotion.y_root;
		}
		else if (event->type == LeaveNotify)
		{
			hide_tooltip(ui);
			state->hover_id = 0;
		}
		return true;
	}
	return false;
}

void tray_sni_service(tray_ui *ui, double stamp)
{
	tray_sni_ui *state = ui->sni;
	if (!state)
		return;
	if (state->retry && !state->stopping)
	{
		state->retry = false;
		if (!tray_bus_connected(state->bus))
		{
			tray_bus_close(state->bus);
			const char *address =
			    getenv("DBUS_SESSION_BUS_ADDRESS");
			(void)tray_bus_open(state->bus, address, stamp);
		}
	}
	unsigned int foreign = 0;
	for (unsigned int i = 0; i < ui->model.count; i++)
		if (!ui->sites[i].native)
			foreign++;
	tray_bus_set_capacity(state->bus, TRAY_CLIENT_MAX - foreign);
	size_t retained = state->picture_bytes + state->upload_bytes;
	tray_bus_set_icon_budget(state->bus, retained < SNI_UI_IMAGE_MAX
	    ? SNI_UI_IMAGE_MAX - retained : 0);
	(void)tray_bus_service(state->bus, stamp);
	if (tray_bus_take_dirty(state->bus))
	{
		tray_sni_sync(ui);
		const tray_sni_model *model = tray_bus_model(state->bus);
		if (state->menu_item && (!model || !model->menu.open))
			close_menu(ui);
		paint_menu(ui);
	}
	tray_icons_service(state->icons, stamp);
	service_upload(ui);
	if (!ui->visible || state->stopping ||
	    ((state->menu_item || state->tooltip_visible ||
	        state->pending_menu_id) && !viewable(ui)))
		deactivate(ui);
	if (state->pending_menu_id && ui->active)
	{
		const tray_sni_item *pending = item(ui, state->pending_menu_id,
		    state->pending_menu_generation);
		native_cell *record = cell(ui, state->pending_menu_id);
		if (!pending || !ui->container.attached)
			state->pending_menu_id = 0;
		else if (record && ui->model.focused == record->window)
		{
			state->pending_menu_id = 0;
			open_menu(ui, pending, state->pending_menu_x,
			    state->pending_menu_y, stamp);
		}
	}
	if (!state->stopping)
	{
		acquire_icons(ui, stamp);
		for (unsigned int i = 0; !state->builder &&
		    i < TRAY_SNI_MAX; i++)
		{
			native_cell *record = &state->cells[i];
			const tray_sni_item *value = item(ui, record->id,
			    record->generation);
			if (!value)
				continue;
			const tray_sni_pixmap *pixmap = value->status ==
			    TRAY_SNI_ATTENTION && value->attention.pixels
			    ? &value->attention : &value->icon;
			if (!record->raw_requested && !record->picture &&
			    pixmap->pixels)
			{
				record->raw_requested = true;
				start_upload(ui, record, pixmap, false);
			}
			else if (!record->raw_overlay && !record->overlay &&
			    value->overlay.pixels)
			{
				record->raw_overlay = true;
				start_upload(ui, record, &value->overlay, true);
			}
		}
		if (state->hover_deadline && stamp >= state->hover_deadline)
		{
			state->hover_deadline = 0;
			tooltip(ui);
		}
	}
	if (!state->requested && !tray_icons_busy(state->icons) &&
	    !state->builder)
		tray_sni_destroy(ui);
}

unsigned int tray_sni_pollfds(tray_ui *ui, struct pollfd *fds)
{
	if (!ui->sni)
		return 0;
	unsigned int count = 0;
	int descriptor = tray_bus_fd(ui->sni->bus);
	if (descriptor >= 0)
		fds[count++] = (struct pollfd){descriptor,
		    tray_bus_events(ui->sni->bus), 0};
	descriptor = tray_icons_read_fd(ui->sni->icons);
	if (descriptor >= 0)
		fds[count++] = (struct pollfd){descriptor, POLLIN, 0};
	descriptor = tray_icons_write_fd(ui->sni->icons);
	if (descriptor >= 0)
		fds[count++] = (struct pollfd){descriptor, POLLOUT, 0};
	return count;
}

int tray_sni_timeout(tray_ui *ui, double stamp)
{
	if (!ui->sni)
		return -1;
	tray_sni_ui *state = ui->sni;
	int timeout = tray_bus_timeout(state->bus, stamp);
	if (state->builder || state->retry)
		return 0;
	int icon_timeout = tray_icons_timeout_ms(state->icons);
	if (icon_timeout >= 0 && (timeout < 0 || icon_timeout < timeout))
		timeout = icon_timeout;
	if (state->hover_deadline)
	{
		int hover = state->hover_deadline <= stamp ? 0 :
		    (int)((state->hover_deadline - stamp) * 1000.0) + 1;
		if (timeout < 0 || hover < timeout)
			timeout = hover;
	}
	return timeout;
}

void tray_sni_status_reply(tray_ui *ui)
{
	char buffer[256];
	const tray_sni_model *model = ui->sni
	    ? tray_bus_model(ui->sni->bus) : NULL;
	snprintf(buffer, sizeof(buffer),
	    "Echo FVWM_TRAY_SERVICE_STATUS %u %u %u %u %u %u %u %u "
	    "%lu %lu %u %u %u",
	    tray_bus_available() ? 1U : 0U, ui->modern ? 1U : 0U,
	    ui->sni && tray_bus_connected(ui->sni->bus) ? 1U : 0U,
	    model ? model->count : 0U, ui->sni &&
	    tray_icons_available(ui->sni->icons) ? 1U : 0U,
	    ui->sni && tray_icons_busy(ui->sni->icons) ? 1U : 0U,
	    ui->container.capable ? 1U : 0U,
	    ui->container.attached ? 1U : 0U,
	    ui->container.slot, ui->container.offer,
	    model && model->menu.open ? 1U : 0U,
	    ui->container.focus_pending ? 1U : 0U,
	    ui->container.focus_pending ? ui->container.focus_id : 0U);
	SendText(ui->channel, buffer, 0);
}

unsigned int tray_sni_count(const tray_ui *ui)
{
	const tray_sni_model *model = ui->sni
	    ? tray_bus_model(ui->sni->bus) : NULL;
	return model ? model->count : 0;
}

void tray_sni_stop(tray_ui *ui)
{
	if (!ui->sni)
		return;
	ui->sni->stopping = true;
	close_menu(ui);
	hide_tooltip(ui);
	tray_bus_close(ui->sni->bus);
	tray_icons_cancel(ui->sni->icons);
	if (ui->sni->builder)
	{
		picture_argb_cancel(ui->sni->builder);
		ui->sni->upload_canceled = true;
	}
	/* Shutdown belongs to this isolated module. No core work waits for the
	 * child. Poll/reap is bounded; failure is reported, never called
	 * clean. */
	double deadline = tray_sni_now() + 4.0;
	for (unsigned int turn = 0; turn < 800 &&
	    (tray_icons_busy(ui->sni->icons) || ui->sni->builder) &&
	    tray_sni_now() < deadline; turn++)
	{
		tray_icons_service(ui->sni->icons, tray_sni_now());
		service_upload(ui);
		(void)poll(NULL, 0, 5);
	}
	if (tray_icons_busy(ui->sni->icons) || ui->sni->builder)
		ui->failed = true;
}

void tray_sni_destroy(tray_ui *ui)
{
	if (!ui->sni)
		return;
	for (unsigned int i = 0; i < TRAY_SNI_MAX; i++)
	{
		drop_picture(ui, &ui->sni->cells[i].picture);
		drop_picture(ui, &ui->sni->cells[i].overlay);
	}
	for (unsigned int i = 0; i < TRAY_SNI_MENU_MAX; i++)
		drop_picture(ui, &ui->sni->menu_icons[i]);
	if (ui->sni->menu)
		XDestroyWindow(ui->display, ui->sni->menu);
	if (ui->sni->tooltip)
		XDestroyWindow(ui->display, ui->sni->tooltip);
	tray_bus_destroy(ui->sni->bus);
	if (!tray_icons_destroy(ui->sni->icons))
		ui->failed = true;
	free(ui->sni);
	ui->sni = NULL;
}

void tray_sni_creation_error(tray_ui *ui, const XErrorEvent *error)
{
	if (ui->sni && ui->sni->builder)
		picture_argb_creation_error(ui->sni->builder, error);
}

void tray_sni_repaint_popups(tray_ui *ui)
{
	if (!ui->sni)
		return;
	if (ui->sni->menu_item)
		paint_menu(ui);
	if (ui->sni->tooltip_visible)
		tooltip(ui);
}
