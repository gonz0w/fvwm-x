/* This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or any later version. */
#include "config.h"
#include "libs/fvwmlib.h"
#include "libs/Module.h"
#include "libs/flocale_font_types.h"
#include "libs/module_ui_scale.h"
#include "libs/wm_ui_metrics.h"
#include "libs/ui_layout.h"
#include "FvwmForm.h"

#include <inttypes.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>

extern Bool Swallowed;

typedef struct
{
	DrawTable *table;
	char *name;
	int height;
} form_scale_font_source;

typedef struct
{
	Item *item;
	int width, height, font;
} form_scale_item_source;

static struct
{
	bool requested, captured, initialized;
	const char *module;
	const char *state;
	module_ui_scale_receiver receiver;
	unsigned int quarters, desired, resource_generation;
	uint64_t epoch, desired_epoch;
	Atom atom, desired_atom;
	int required_width, required_height, logical_pad;
	Atom request_atom;
	unsigned int request_quarters;
	int request_width, request_height;
	unsigned int count, fonts, lines;
	form_scale_item_source items[FORM_SCALE_ITEMS];
	form_scale_font_source tables[FORM_SCALE_TABLES];
	Line *line[FORM_SCALE_ITEMS];
} scale = {.quarters = 4, .desired = 4, .state = "disabled"};

void form_scale_configure(char *text)
{
	if (strcmp(text, "monitor") == 0)
		scale.requested = true;
	else if (strcmp(text, "off") == 0)
		scale.requested = false;
	else
		fprintf(stderr, "FvwmForm: invalid UIScale monitor|off\n");
}

bool form_scale_message(const char *text)
{
	module_ui_scale_status status = module_ui_scale_receive(&scale.receiver,
		text);

	if (status == MODULE_UI_SCALE_IGNORED)
		return false;
	if (status == MODULE_UI_SCALE_REFUSED)
		fprintf(stderr, "FvwmForm: refused MonitorUI snapshot\n");
	if (status == MODULE_UI_SCALE_PUBLISHED ||
	    status == MODULE_UI_SCALE_UNCHANGED)
		form_scale_retry();
	return true;
}

void form_scale_end_config(void)
{
	module_ui_scale_abort(&scale.receiver);
}

void form_scale_cleanup(void)
{
	for (unsigned int i = 0; i < scale.fonts; i++)
	{
		free(scale.tables[i].name);
		scale.tables[i].name = NULL;
	}
}

void form_scale_capture(const char *module_name)
{
	Line *line = &root_line;

	scale.module = module_name;
	if (!scale.requested)
		return;
	scale.state = "unsupported";
	if (CF.viewport == NULL || CF.last_error != NULL || Swallowed ||
	    CF.padVText < 0)
		goto unsupported;
	scale.logical_pad = CF.padVText;
	for (Item *p = root_item_ptr; p != NULL; p = p->header.next)
	{
		unsigned int font = 0;

		if (scale.count == FORM_SCALE_ITEMS ||
		    (p->type != I_TEXT && p->type != I_INPUT &&
		     p->type != I_BUTTON && p->type != I_TIMEOUT &&
		     (p->type != I_LIST || p != CF.viewport)) ||
		    p->header.dt_ptr == NULL || p->header.size_x < 1 ||
		    p->header.size_y < 1)
			goto unsupported;
		for (; font < scale.fonts; font++)
			if (scale.tables[font].table == p->header.dt_ptr)
				break;
		if (font == scale.fonts)
		{
			FlocaleFont *original = p->header.dt_ptr->dt_Ffont;

			if (scale.fonts == FORM_SCALE_TABLES ||
			    original == NULL ||
			    original->height < 1 || original->name == NULL)
				goto unsupported;
			scale.tables[font] = (form_scale_font_source){
				.table = p->header.dt_ptr,
				.name = fxstrdup(original->name),
				.height = original->height
			};
			scale.fonts++;
		}
		scale.items[scale.count++] = (form_scale_item_source){
			p, p->type == I_TEXT && p->text.viewport ?
				p->text.minimum_width : p->header.size_x,
			p->header.size_y, font
		};
	}
	do
	{
		if (scale.lines == FORM_SCALE_ITEMS)
			goto unsupported;
		scale.line[scale.lines++] = line;
		line = line->next;
	} while (line != &root_line);
	if (!form_scale_metrics_prepare(4, scale.logical_pad, &CF.ui_metrics) ||
	    atexit(form_scale_cleanup) != 0)
		goto unsupported;
	scale.captured = true;
	scale.resource_generation =
		wm_ui_resource_generation_current().generation;
	scale.state = "waiting";
	return;
unsupported:
	form_scale_cleanup();
	fprintf(stderr, "%s: UIScale needs one supported ListViewport Form\n",
		module_name);
}

void form_scale_initialize(const char *module_name)
{
	(void)module_name;
	if (scale.captured && CF.viewport != NULL &&
	    CF.viewport->list.line->n == 1)
	{
		CF.ui_scale_enabled = true;
		scale.initialized = true;
	}
	else if (scale.requested)
		scale.state = "unsupported";
}

bool form_scale_list_geometry(int units, int columns, int *cell, int *height)
{
	Item *list = CF.viewport;
	int logical_font;

	if (!scale.initialized || list == NULL)
		return false;
	logical_font = list->header.dt_ptr->dt_Ffont->height;
	for (unsigned int i = 0; i < scale.fonts; i++)
		if (scale.tables[i].table == list->header.dt_ptr)
			logical_font = scale.tables[i].height;
	return form_scale_cell(scale.quarters, logical_font,
		list->header.dt_ptr->dt_Ffont->height, units, columns > 1,
		&CF.ui_metrics, cell, height);
}

static bool item_dimensions(const form_scale_item_source *source,
	FlocaleFont *font, const form_scale_metrics *metrics,
	unsigned int quarters,
	int *width, int *height)
{
	Item *p = source->item;
	int64_t required_width, required_height;
	int text_width = 0;

	if (font->height < 1 || font->height > 32767)
		return false;
	if (p->type == I_TEXT && !p->text.fixed)
		text_width = FlocaleTextWidth(font, p->text.value, p->text.n);
	else if (p->type == I_BUTTON)
		text_width = FlocaleTextWidth(font, p->button.text,
			p->button.len);
	else if (p->type == I_TIMEOUT)
	{
		int digits = 0;

		text_width = FlocaleTextWidth(font, p->timeout.text,
			p->timeout.len);
		if (strstr(p->timeout.text, "%%") != NULL)
		{
			for (char c = '0'; c <= '9'; c++)
			{
				int measured = FlocaleTextWidth(font, &c, 1);

				if (measured > digits)
					digits = measured;
			}
			if (digits > (32767 - text_width) / 5)
				return false;
			text_width += 5 * digits;
		}
	}
	if (text_width < 0 || text_width > 32767)
		return false;
	required_width = text_width + 2LL * metrics->text;
	{
		int logical_pad = source->height -
			scale.tables[source->font].height;
		int padding;

		if (!form_scale_extent(logical_pad, 0, quarters, &padding))
			return false;
		required_height = font->height + (int64_t)padding;
	}
	if (p->type == I_TEXT && p->text.fixed)
	{
		int base_font = scale.tables[source->font].height;

		required_width = ((int64_t)(source->width - 6) * font->height +
			base_font - 1) / base_font + 2LL * metrics->text;
	}
	else if (p->type == I_INPUT)
	{
		required_width = (int64_t)p->input.width * font->height / 2 +
			2LL * (metrics->text + metrics->box);
		required_height = font->height + 3LL * metrics->text +
			2LL * metrics->box;
	}
	else if (p->type == I_BUTTON)
	{
		required_width += 2LL * metrics->box;
		required_height = font->height +
			2LL * (metrics->text + metrics->box);
	}
	if (required_width < 1 || required_width > 32767 ||
	    required_height < 1 || required_height > 32767)
		return false;
	return form_scale_extent(source->width, required_width,
		quarters, width) && form_scale_extent(source->height,
		required_height, quarters, height);
}

bool form_scale_holds_view(void)
{
	return scale.initialized && scale.desired != scale.quarters;
}

void form_scale_retry(void)
{
	FlocaleFont *fonts[FORM_SCALE_TABLES] = {0};
	FlocaleFont *old[FORM_SCALE_TABLES];
	form_scale_metrics metrics;
	int widths[FORM_SCALE_ITEMS], heights[FORM_SCALE_ITEMS];
	int line_heights[FORM_SCALE_ITEMS] = {0};
	XWindowAttributes attributes;
	Window child;
	rectangle rect;
	const module_ui_scale_row *monitor;
	Item *list = CF.viewport;
	int cell = 0, row_height = 0, minimum, min_height, fixed;
	int list_font = -1;
	ui_layout_grid_result grid;
	unsigned int generation =
		wm_ui_resource_generation_current().generation;

	if (!scale.initialized || CF.frame == None)
		return;
	if (Swallowed || !XGetWindowAttributes(dpy, CF.frame, &attributes) ||
	    !XTranslateCoordinates(dpy, CF.frame, root, 0, 0,
		&rect.x, &rect.y, &child))
		goto refused;
	rect.width = attributes.width;
	rect.height = attributes.height;
	if (rect.width < 1 || rect.width > 32767 ||
	    rect.height < 1 || rect.height > 32767)
		goto refused;
	monitor = module_ui_scale_select(&scale.receiver.published, &rect,
		scale.atom);
	if (monitor == NULL)
	{
		scale.state = "waiting";
		return;
	}
	if (scale.desired != monitor->quarters ||
	    scale.desired_epoch != scale.receiver.published.epoch ||
	    scale.desired_atom != monitor->name_atom)
	{
		scale.required_width = 0;
		scale.required_height = 0;
	}
	scale.desired = monitor->quarters;
	scale.desired_epoch = scale.receiver.published.epoch;
	scale.desired_atom = monitor->name_atom;
	if (list->list.update != NULL)
	{
		scale.state = "deferred";
		return;
	}
	if (rect.width != CF.max_width || rect.height != CF.total_height)
	{
		scale.state = "waiting";
		return;
	}
	if (scale.quarters == monitor->quarters &&
	    scale.resource_generation == generation)
		goto observed;
	if (!form_scale_metrics_prepare(monitor->quarters, scale.logical_pad,
		&metrics))
		goto refused;
	for (unsigned int i = 0; i < scale.fonts; i++)
	{
		char *name = fxstrdup(scale.tables[i].name);

		fonts[i] = FlocaleLoadFontAtScale(dpy, name,
			(char *)scale.module,
			(monitor_scale_factor){monitor->quarters}, generation);
		free(name);
		if (fonts[i] == NULL)
			goto refused;
	}
	for (unsigned int i = 0; i < scale.count; i++)
	{
		const form_scale_item_source *source = &scale.items[i];

		if (source->item == list)
		{
			list_font = source->font;
			continue;
		}
		if (!item_dimensions(source, fonts[source->font], &metrics,
			monitor->quarters, &widths[i], &heights[i]))
			goto refused;
	}
	if (list_font < 0 || !form_scale_cell(monitor->quarters,
		scale.tables[list_font].height, fonts[list_font]->height,
		list->list.viewport_units, list->list.viewport_columns > 1,
		&metrics, &cell, &row_height) || cell > 32700)
		goto refused;
	minimum = cell + 2 * metrics.horizontal + 2 * metrics.box +
		metrics.gutter;
	fixed = metrics.vertical * (scale.lines + 1);
	for (unsigned int l = 0; l < scale.lines; l++)
	{
		Line *line = scale.line[l];
		int64_t width = (int64_t)(line->n + 1) * metrics.horizontal;

		if (line == list->list.line)
			continue;
		for (int j = 0; j < line->n; j++)
			for (unsigned int i = 0; i < scale.count; i++)
				if (scale.items[i].item == line->items[j])
				{
					width += widths[i];
					if (heights[i] > line_heights[l])
						line_heights[l] = heights[i];
				}
		if (width > 32767 || fixed > 32767 - line_heights[l])
			goto refused;
		if (width > minimum)
			minimum = width;
		fixed += line_heights[l];
	}
	min_height = fixed + row_height + 2 * metrics.box;
	if (minimum > 32767 || min_height > 32767)
		goto refused;
	scale.required_width = minimum;
	scale.required_height = min_height;
	if (rect.width < minimum || rect.height < min_height)
	{
		scale.state = "waiting";
		if (scale.request_atom != monitor->name_atom ||
		    scale.request_quarters != monitor->quarters ||
		    scale.request_width != minimum ||
		    scale.request_height != min_height)
		{
			scale.request_atom = monitor->name_atom;
			scale.request_quarters = monitor->quarters;
			scale.request_width = minimum;
			scale.request_height = min_height;
			form_viewport_set_minimum(minimum, min_height);
			XResizeWindow(dpy, CF.frame,
				rect.width > minimum ? rect.width : minimum,
				rect.height > min_height ?
				rect.height : min_height);
		}
		goto release;
	}
	if (!ui_layout_grid(rect.width - 2 * metrics.horizontal -
		2 * metrics.box - metrics.gutter,
		rect.height - fixed - 2 * metrics.box, cell, row_height,
		1, list->list.viewport_columns, 32, &grid))
		goto refused;
	/* Admission is complete. Fonts, metrics and children publish together.
	 * Input bytes/cursors, focus and list generation stay untouched. */
	for (unsigned int i = 0; i < scale.fonts; i++)
	{
		DrawTable *dt = scale.tables[i].table;

		old[i] = dt->dt_Ffont;
		dt->dt_Ffont = fonts[i];
		fonts[i] = NULL;
		if (dt->dt_Ffont->font != NULL)
		{
			if (dt->dt_GC != None)
				XSetFont(dpy, dt->dt_GC,
					dt->dt_Ffont->font->fid);
			if (dt->dt_item_GC != None)
				XSetFont(dpy, dt->dt_item_GC,
					dt->dt_Ffont->font->fid);
		}
	}
	CF.ui_metrics = metrics;
	CF.padVText = metrics.pad_text;
	for (unsigned int i = 0; i < scale.count; i++)
		if (scale.items[i].item != list)
		{
			if (scale.items[i].item->type == I_TEXT &&
			    scale.items[i].item->text.viewport)
				scale.items[i].item->text.minimum_width = widths[i];
			scale.items[i].item->header.size_x = widths[i];
			scale.items[i].item->header.size_y = heights[i];
		}
	for (unsigned int l = 0; l < scale.lines; l++)
		if (scale.line[l] != list->list.line)
			scale.line[l]->size_y = line_heights[l];
	CF.viewport_fixed_height = fixed;
	list->list.viewport_cell = cell;
	list->list.row_height = row_height;
	CF.ui_scale_resize_all = true;
	form_viewport_resize(rect.width, rect.height);
	CF.ui_scale_resize_all = false;
	form_viewport_set_minimum(minimum, min_height);
	UpdateRootTransapency(False, True);
	RedrawFrame(NULL);
	for (unsigned int i = 0; i < scale.count; i++)
		if (scale.items[i].item->header.win != None)
			RedrawItem(scale.items[i].item, 0, NULL);
	for (unsigned int i = 0; i < scale.fonts; i++)
		FlocaleUnloadFont(dpy, old[i]);
	scale.quarters = monitor->quarters;
	scale.resource_generation = generation;
observed:
	if (scale.request_atom != None)
		form_viewport_set_minimum(CF.viewport_min_width,
			CF.viewport_min_height);
	scale.request_atom = None;
	scale.epoch = scale.desired_epoch;
	scale.atom = scale.desired_atom;
	scale.required_width = CF.viewport_min_width;
	scale.required_height = CF.viewport_min_height;
	scale.state = "ready";
	return;
refused:
	if (scale.request_atom != None)
		form_viewport_set_minimum(CF.viewport_min_width,
			CF.viewport_min_height);
	scale.state = "refused";
	fprintf(stderr, "%s: UIScale candidate refused\n", scale.module);
release:
	for (unsigned int i = 0; i < scale.fonts; i++)
		if (fonts[i] != NULL)
			FlocaleUnloadFont(dpy, fonts[i]);
}

void form_scale_status(char *text)
{
	char token[65], reply[512];
	bool selection;
	int length;

	if (!form_list_status_token(text, token, &selection) || selection)
	{
		fprintf(stderr, "FvwmForm: refused UIScaleStatus\n");
		return;
	}
	length = snprintf(reply, sizeof(reply),
		"Echo FVWM_FORM_UI_SCALE %s %u %s %u %u %016" PRIx64
		" %016" PRIx64 " %lu %lu %u %d %d %d %d", token,
		CF.ui_scale_enabled ? 1U : 0U, scale.state, scale.quarters,
		scale.desired, scale.epoch, scale.desired_epoch, scale.atom,
		scale.desired_atom, scale.resource_generation,
		CF.viewport_min_width, CF.viewport_min_height,
		scale.required_width, scale.required_height);
	if (length < 0 || (size_t)length >= sizeof(reply))
		return;
	XFlush(dpy);
	SendText(Channel, reply, 0);
}
