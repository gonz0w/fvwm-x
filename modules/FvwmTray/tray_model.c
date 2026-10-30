/* This program is free software; you can redistribute it and/or modify it
 * under the GNU General Public License, version 2 or any later version. */
#include "config.h"
#include "tray_model.h"
#include <limits.h>
#include <stddef.h>
#include <string.h>

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

tray_admission tray_model_add(tray_model *model, uint32_t id,
	unsigned int width, unsigned int height, bool mapped)
{
	if (!model || !id || model->count > TRAY_CLIENT_MAX ||
	    !tray_model_size(width, height) || model->generation == UINT64_MAX)
		return TRAY_INVALID;
	if (tray_model_index(model, id) >= 0)
		return TRAY_DUPLICATE;
	if (model->count == TRAY_CLIENT_MAX)
		return TRAY_FULL;
	tray_client *client = &model->clients[model->count++];
	*client = (tray_client){id, ++model->generation, width, height, mapped};
	return TRAY_ADMITTED;
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
	if (index < 0 || !tray_model_size(width, height))
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

bool tray_model_layout(const tray_model *model, int width, int height,
	int padding, int gap, int header_width, int header_height,
	bool vertical,
	bool visible, tray_layout *layout)
{
	if (!layout)
		return false;
	memset(layout, 0, sizeof(*layout));
	if (!model || model->count > TRAY_CLIENT_MAX || width < 1 ||
	    width > 32767 || height < 1 || height > 32767 || padding < 0 ||
	    padding > 4096 || gap < 0 || gap > 4096 || header_width < 0 ||
	    header_width > 4096 || header_height < 0 || header_height > 4096)
		return false;
	layout->width = lesser(width, padding * 2 + header_width);
	layout->height = lesser(height, padding * 2 + header_height);
	if (layout->width < 1)
		layout->width = 1;
	if (layout->height < 1)
		layout->height = 1;
	int available_width = width - padding * 2;
	int available_height = height - padding * 2 - header_height - gap;
	int cursor = 0, breadth = 0;
	for (unsigned int i = 0; i < model->count; i++)
	{
		const tray_client *client = &model->clients[i];
		if (!client->id ||
		    !tray_model_size(client->width, client->height))
			return false;
		if (!visible || !client->mapped)
			continue;
		int cell_width = (int)client->width;
		int cell_height = (int)client->height;
		int offset = cursor + (layout->count ? gap : 0);
		if ((vertical && (cell_width > available_width ||
		    cell_height > available_height - offset)) ||
		    (!vertical && (cell_width > available_width - offset ||
		    cell_height > available_height)))
			break;
		tray_cell *cell = &layout->cells[i];
		*cell = (tray_cell){padding + (vertical ? 0 : offset),
		    padding + header_height + gap + (vertical ? offset : 0),
		    cell_width, cell_height, true};
		cursor = offset + (vertical ? cell_height : cell_width);
		int cross = vertical ? cell_width : cell_height;
		if (cross > breadth)
			breadth = cross;
		layout->count++;
	}
	if (layout->count)
	{
		int needed_width = padding * 2 + (vertical ? breadth : cursor);
		int needed_height = padding * 2 + header_height + gap +
		    (vertical ? cursor : breadth);
		if (needed_width > layout->width)
			layout->width = needed_width;
		if (needed_height > layout->height)
			layout->height = needed_height;
	}
	return layout->width <= width && layout->height <= height;
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
