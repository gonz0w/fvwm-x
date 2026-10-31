/* This program is free software; you can redistribute it and/or modify it
 * under the GNU General Public License, version 2 or any later version. */
#include "config.h"
#include "tray_model.h"
#include <limits.h>
#include <errno.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>

static bool rect_valid(const tray_rect *rect)
{
	return rect && rect->x >= -32768 && rect->x <= 32767 &&
	    rect->y >= -32768 && rect->y <= 32767 &&
	    rect->width > 0 && rect->width <= 32767 &&
	    rect->height > 0 && rect->height <= 32767 &&
	    (int64_t)rect->x + rect->width <= 32767 &&
	    (int64_t)rect->y + rect->height <= 32767;
}

bool tray_model_anchor(const char *text, tray_rect *anchor)
{
	int values[4];

	if (!text || !anchor)
		return false;
	for (unsigned int i = 0; i < 4; i++)
	{
		char *end;
		while (*text == ' ' || *text == '\t')
			text++;
		const char *digits = text;
		if (*digits == '-' || *digits == '+')
			digits++;
		if (*digits < '0' || *digits > '9')
			return false;
		errno = 0;
		long value = strtol(text, &end, 10);
		if (errno == ERANGE || value < -32768 || value > 32767 ||
		    (*end && *end != ' ' && *end != '\t'))
			return false;
		values[i] = (int)value;
		text = end;
	}
	while (*text == ' ' || *text == '\t')
		text++;
	tray_rect candidate = {values[0], values[1], values[2], values[3]};
	if (*text || !rect_valid(&candidate))
		return false;
	*anchor = candidate;
	return true;
}

bool tray_model_place(const tray_rect *anchor, const tray_rect *area,
	int width, int height, tray_rect *placed)
{
	if (placed)
		*placed = (tray_rect){0};
	if (!placed || !rect_valid(anchor) || !rect_valid(area) || width < 1 ||
	    height < 1 || width > area->width || height > area->height)
		return false;
	int64_t right = (int64_t)area->x + area->width;
	int64_t bottom = (int64_t)area->y + area->height;
	int64_t x = anchor->x, y = (int64_t)anchor->y + anchor->height;
	if (y + height > bottom)
		y = (int64_t)anchor->y - height;
	if (x + width > right)
		x = right - width;
	if (x < area->x)
		x = area->x;
	if (y + height > bottom)
		y = bottom - height;
	if (y < area->y)
		y = area->y;
	*placed = (tray_rect){(int)x, (int)y, width, height};
	return true;
}

bool tray_model_info(const uint32_t words[2], bool *mapped)
{
	if (mapped)
		*mapped = false;
	if (!words || !mapped || (words[1] & ~TRAY_XEMBED_MAPPED))
		return false;
	/* Negotiate zero even when a client supports a newer version.
	 * The only currently defined flag is MAPPED. */
	*mapped = (words[1] & TRAY_XEMBED_MAPPED) != 0;
	return true;
}

bool tray_model_size(unsigned int width, unsigned int height)
{
	return width >= 1 && width <= TRAY_DIMENSION_MAX && height >= 1 &&
	    height <= TRAY_DIMENSION_MAX;
}

int tray_model_index(const tray_model *model, uint32_t id)
{
	if (!model || !id || model->count > TRAY_CLIENT_MAX)
		return -1;
	for (unsigned int i = 0; i < model->count; i++)
		if (model->clients[i].id == id)
			return (int)i;
	return -1;
}

static tray_admission add(tray_model *model, uint32_t id,
	unsigned int width, unsigned int height, bool mapped,
	tray_client_kind kind)
{
	if (!model || !id || model->count > TRAY_CLIENT_MAX ||
	    ((kind == TRAY_CLIENT_XEMBED && !tray_model_size(width, height)) ||
	     (kind == TRAY_CLIENT_NATIVE && (!width || !height ||
	      width > 4096 || height > 4096))) ||
	    model->generation == UINT64_MAX)
		return TRAY_INVALID;
	if (tray_model_index(model, id) >= 0)
		return TRAY_DUPLICATE;
	if (model->count == TRAY_CLIENT_MAX)
		return TRAY_FULL;
	tray_client *client = &model->clients[model->count++];
	*client = (tray_client){id, ++model->generation, width, height, mapped,
	    kind};
	return TRAY_ADMITTED;
}

tray_admission tray_model_add(tray_model *model, uint32_t id,
	unsigned int width, unsigned int height, bool mapped)
{
	return add(model, id, width, height, mapped, TRAY_CLIENT_XEMBED);
}

tray_admission tray_model_add_native(tray_model *model, uint32_t id,
	unsigned int width, unsigned int height, bool mapped)
{
	return add(model, id, width, height, mapped, TRAY_CLIENT_NATIVE);
}

bool tray_model_resize_native(tray_model *model, uint32_t id,
	unsigned int width, unsigned int height)
{
	int index = tray_model_index(model, id);
	if (index < 0 || model->clients[index].kind != TRAY_CLIENT_NATIVE ||
	    !width || !height || width > 4096 || height > 4096)
		return false;
	model->clients[index].width = width;
	model->clients[index].height = height;
	return true;
}

bool tray_model_remove(tray_model *model, uint32_t id)
{
	int index = tray_model_index(model, id);
	if (index < 0)
		return false;
	if (model->focused == id)
		model->focused = 0;
	model->count--;
	memmove(&model->clients[index], &model->clients[index + 1],
	    (model->count - (unsigned int)index) * sizeof(model->clients[0]));
	memset(&model->clients[model->count], 0, sizeof(model->clients[0]));
	return true;
}

bool tray_model_resize(tray_model *model, uint32_t id,
	unsigned int width, unsigned int height)
{
	int index = tray_model_index(model, id);
	if (index < 0 || model->clients[index].kind != TRAY_CLIENT_XEMBED ||
	    !tray_model_size(width, height))
		return false;
	model->clients[index].width = width;
	model->clients[index].height = height;
	return true;
}

bool tray_model_mapped(tray_model *model, uint32_t id, bool mapped)
{
	int index = tray_model_index(model, id);
	if (index < 0)
		return false;
	model->clients[index].mapped = mapped;
	if (!mapped && model->focused == id)
		model->focused = 0;
	return true;
}

static int lesser(int left, int right)
{
	return left < right ? left : right;
}

bool tray_model_page(const tray_model *model, int width, int height,
	int padding, int gap, int header_width, int header_height,
	bool vertical, bool visible, unsigned int page, tray_layout *layout)
{
	unsigned int membership[TRAY_CLIENT_MAX] = {0};
	int cursor = 0;
	unsigned int on_page = 0, current = 0;

	if (!layout)
		return false;
	memset(layout, 0, sizeof(*layout));
	if (!model || model->count > TRAY_CLIENT_MAX || width < 1 ||
	    width > 32767 || height < 1 || height > 32767 || padding < 0 ||
	    padding > 4096 || gap < 0 || gap > 4096 || header_width < 0 ||
	    header_width > 4096 || header_height < 0 || header_height > 4096)
		return false;
	int available_width = width - padding * 2;
	int header_gap = header_height ? gap : 0;
	int available_height =
	    height - padding * 2 - header_height - header_gap;
	for (unsigned int i = 0; i < model->count; i++)
	{
		const tray_client *client = &model->clients[i];
		if (!client->id ||
		    ((client->kind == TRAY_CLIENT_XEMBED &&
		     !tray_model_size(client->width, client->height)) ||
		     (client->kind == TRAY_CLIENT_NATIVE &&
		      (!client->width || !client->height ||
		       client->width > 4096 || client->height > 4096)) ||
		     client->kind > TRAY_CLIENT_NATIVE))
			return false;
		if (!client->mapped)
			continue;
		int w = (int)client->width, h = (int)client->height;
		/* An unplaceable icon cannot strand later icons. */
		if (w > available_width || h > available_height)
		{
			layout->omitted++;
			continue;
		}
		int length = vertical ? h : w;
		int available = vertical ? available_height : available_width;
		int offset = cursor + (on_page ? gap : 0);
		if (length > available - offset)
		{
			current++;
			cursor = offset = 0;
			on_page = 0;
		}
		membership[i] = current + 1;
		layout->cells[i] = (tray_cell){
		    padding + (vertical ? 0 : offset),
		    padding + header_height + header_gap +
		    (vertical ? offset : 0),
		    w, h, false};
		cursor = offset + length;
		on_page++;
	}
	layout->pages = current + 1;
	layout->page = page < layout->pages ? page : layout->pages - 1;
	layout->width = lesser(width, padding * 2 + header_width);
	layout->height = lesser(height, padding * 2 + header_height);
	if (layout->width < 1)
		layout->width = 1;
	if (layout->height < 1)
		layout->height = 1;
	for (unsigned int i = 0; i < model->count; i++)
	{
		tray_cell *cell = &layout->cells[i];
		if (membership[i] != layout->page + 1 || !visible)
		{
			*cell = (tray_cell){0};
			continue;
		}
		cell->visible = true;
		layout->count++;
		int right = cell->x + cell->width + padding;
		int bottom = cell->y + cell->height + padding;
		if (right > layout->width)
			layout->width = right;
		if (bottom > layout->height)
			layout->height = bottom;
	}
	return layout->width <= width && layout->height <= height;
}

bool tray_model_compact(const tray_model *model, int width, int height,
	int padding, int gap, int target, bool vertical, bool visible,
	unsigned int page, tray_layout *layout, tray_cell *previous,
	tray_cell *next)
{
	if (previous)
		*previous = (tray_cell){0};
	if (next)
		*next = (tray_cell){0};
	if (!layout || !previous || !next || target < 1 || target > 4096)
		return false;
	if (!tray_model_page(model, width, height, padding, gap, 0, 0,
	    vertical, visible, page, layout))
		return false;
	/* The slot owns the full allocation, independently of child content. */
	if (layout->pages > 1 && width >= target * 2 + padding * 2 + 1 &&
	    height >= target + padding * 2)
	{
		tray_layout reserved;
		if (!tray_model_page(model, width - target * 2, height, padding,
		    gap, 0, 0, vertical, visible, page, &reserved))
			return false;
		/* Controls must not strand an otherwise fitting icon. */
		if (reserved.omitted != layout->omitted)
			goto allocation;
		*layout = reserved;
		for (unsigned int i = 0; i < TRAY_CLIENT_MAX; i++)
			if (layout->cells[i].visible)
				layout->cells[i].x += target;
		if (visible)
		{
			*previous = (tray_cell){padding, (height - target) / 2,
			    target, target, true};
			*next = (tray_cell){width - padding - target,
			    (height - target) / 2, target, target, true};
		}
	}
allocation:
	layout->width = width;
	layout->height = height;
	return true;
}

bool tray_model_row_at(const tray_rect *bounds, int row_height,
	unsigned int count, int x, int y, unsigned int *row)
{
	if (row)
		*row = 0;
	if (!bounds || !row || row_height < 1 || !count ||
	    bounds->width < 1 || bounds->height < 1 ||
	    x < bounds->x || y < bounds->y ||
	    (int64_t)x - bounds->x >= bounds->width ||
	    (int64_t)y - bounds->y >= bounds->height)
		return false;
	uint64_t index = ((int64_t)y - bounds->y) / row_height;
	if (index >= count)
		return false;
	*row = (unsigned int)index;
	return true;
}

bool tray_model_layout(const tray_model *model, int width, int height,
	int padding, int gap, int header_width, int header_height,
	bool vertical, bool visible, tray_layout *layout)
{
	return tray_model_page(model, width, height, padding, gap, header_width,
	    header_height, vertical, visible, 0, layout);
}

uint32_t tray_model_focus(tray_model *model, const tray_layout *layout,
	int direction)
{
	if (!model || !layout || !model->count || !layout->count ||
	    model->count > TRAY_CLIENT_MAX || direction < -1 || direction > 1)
		return 0;
	int current = tray_model_index(model, model->focused);
	if (!direction && current >= 0 && layout->cells[current].visible)
		return model->focused;
	if (current < 0)
		current = direction < 0 ? 0 : (int)model->count - 1;
	int step = direction < 0 ? -1 : 1;
	for (unsigned int i = 0; i < model->count; i++)
	{
		current = (current + step + (int)model->count) %
		    (int)model->count;
		if (layout->cells[current].visible)
		{
			model->focused = model->clients[current].id;
			return model->focused;
		}
	}
	return 0;
}
