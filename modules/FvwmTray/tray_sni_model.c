/* This program is free software; you can redistribute it and/or modify it
 * under the GNU General Public License, version 2 or any later version. */
#include "config.h"
#include "tray_sni_model.h"
#include <limits.h>
#include <stdlib.h>
#include <string.h>

bool tray_sni_text(const char *text, size_t maximum, bool multiline)
{
	const unsigned char *p = (const unsigned char *)text;
	size_t length;

	if (!text || (length = strnlen(text, maximum + 1)) > maximum)
		return false;
	for (size_t i = 0; i < length;)
	{
		unsigned int first = p[i++], count, value, minimum;
		if (first < 0x80)
		{
			if ((first < 0x20 && !(multiline &&
			    (first == '\n' || first == '\t'))) || first == 0x7f)
				return false;
			continue;
		}
		if (first >= 0xc2 && first <= 0xdf)
			count = 1, value = first & 0x1f, minimum = 0x80;
		else if (first >= 0xe0 && first <= 0xef)
			count = 2, value = first & 0xf, minimum = 0x800;
		else if (first >= 0xf0 && first <= 0xf4)
			count = 3, value = first & 7, minimum = 0x10000;
		else
			return false;
		if (count > length - i)
			return false;
		while (count--)
		{
			if ((p[i] & 0xc0) != 0x80)
				return false;
			value = (value << 6) | (p[i++] & 0x3f);
		}
		if (value < minimum || value > 0x10ffff ||
		    (value >= 0xd800 && value <= 0xdfff) ||
		    (value >= 0x80 && value <= 0x9f))
			return false;
	}
	return true;
}

bool tray_sni_owner(const char *owner)
{
	if (!tray_sni_text(owner, TRAY_SNI_OWNER_MAX, false) ||
	    owner[0] != ':' || !owner[1])
		return false;
	bool dot = false;
	for (const char *p = owner + 1; *p; p++)
	{
		if (*p == '.')
		{
			if (p == owner + 1 || p[-1] == '.' || !p[1])
				return false;
			dot = true;
		}
		else if (!((*p >= 'a' && *p <= 'z') ||
		    (*p >= 'A' && *p <= 'Z') || (*p >= '0' && *p <= '9') ||
		    *p == '_' || *p == '-'))
			return false;
	}
	return dot;
}

bool tray_sni_path(const char *path)
{
	if (!tray_sni_text(path, TRAY_SNI_PATH_MAX, false) || path[0] != '/')
		return false;
	if (!path[1])
		return true;
	for (const char *p = path + 1; *p; p++)
	{
		if (*p == '/')
		{
			if (p[-1] == '/' || !p[1])
				return false;
		}
		else if (!((*p >= 'a' && *p <= 'z') ||
		    (*p >= 'A' && *p <= 'Z') || (*p >= '0' && *p <= '9') ||
		    *p == '_'))
			return false;
	}
	return true;
}

bool tray_sni_pixmap_valid(unsigned int width, unsigned int height,
	size_t bytes)
{
	return width && height && width <= 256 && height <= 256 &&
	    bytes == (size_t)width * height * sizeof(uint32_t);
}

bool tray_sni_resample_argb(const uint32_t *source, size_t source_count,
	unsigned int source_width, unsigned int source_height,
	uint32_t *destination, size_t destination_count,
	unsigned int width, unsigned int height)
{
	if (!source || !destination || !source_width || !source_height ||
	    source_width > 256 || source_height > 256 || !width || !height ||
	    width > TRAY_SNI_UPLOAD_SIDE_MAX ||
	    height > TRAY_SNI_UPLOAD_SIDE_MAX)
		return false;
	/* Dimension caps make these products and coordinate products safe on
	 * every supported size_t. Counts describe available pixel slots. */
	size_t input = (size_t)source_width * source_height;
	size_t output = (size_t)width * height;
	if (source_count < input || source_count > 256U * 256U ||
	    destination_count < output || destination_count >
	        TRAY_SNI_UPLOAD_SIDE_MAX * TRAY_SNI_UPLOAD_SIDE_MAX)
		return false;
	size_t source_bytes = input * sizeof(uint32_t);
	size_t destination_bytes = output * sizeof(uint32_t);
	uintptr_t from = (uintptr_t)source, to = (uintptr_t)destination;
	if (from > UINTPTR_MAX - source_bytes ||
	    to > UINTPTR_MAX - destination_bytes ||
	    (from < to + destination_bytes && to < from + source_bytes))
		return false;
	for (unsigned int y = 0; y < height; y++)
	{
		size_t row = (size_t)(y * source_height / height) *
		    source_width;
		for (unsigned int x = 0; x < width; x++)
			destination[(size_t)y * width + x] =
			    source[row + x * source_width / width];
	}
	return true;
}

void tray_sni_pixmap_clear(tray_sni_pixmap *pixmap)
{
	if (!pixmap)
		return;
	free(pixmap->pixels);
	memset(pixmap, 0, sizeof(*pixmap));
}

void tray_sni_item_clear(tray_sni_item *item)
{
	if (!item)
		return;
	tray_sni_pixmap_clear(&item->icon);
	tray_sni_pixmap_clear(&item->attention);
	tray_sni_pixmap_clear(&item->overlay);
	memset(item, 0, sizeof(*item));
}

static size_t item_bytes(const tray_sni_item *item)
{
	return ((size_t)item->icon.width * item->icon.height +
	    (size_t)item->attention.width * item->attention.height +
	    (size_t)item->overlay.width * item->overlay.height) * 4;
}

void tray_sni_model_init(tray_sni_model *model)
{
	memset(model, 0, sizeof(*model));
	model->capacity = TRAY_SNI_MAX;
	model->next_id = model->next_generation = 1;
}

void tray_sni_menu_close(tray_sni_model *model)
{
	memset(&model->menu, 0, sizeof(model->menu));
	if (model->next_generation)
		model->menu.generation = model->next_generation++;
}

bool tray_sni_menu_begin(tray_sni_model *model, uint64_t id,
	uint64_t generation, int32_t parent)
{
	const tray_sni_item *item = model ?
	    tray_sni_lookup(model, id, generation) : NULL;
	if (!item || !item->ready || !model->next_generation || parent < 0)
		return false;
	tray_sni_menu_close(model);
	model->menu.item_id = id;
	model->menu.item_generation = generation;
	model->menu.parent = parent;
	model->menu.open = model->menu.stale = true;
	return true;
}

void tray_sni_model_clear(tray_sni_model *model)
{
	for (unsigned int i = 0; i < model->count; i++)
		tray_sni_item_clear(&model->items[i]);
	model->count = 0;
	model->icon_bytes = 0;
	tray_sni_menu_close(model);
}

tray_sni_item *tray_sni_find(tray_sni_model *model, uint64_t id)
{
	if (id)
		for (unsigned int i = 0; i < model->count; i++)
			if (model->items[i].id == id)
				return &model->items[i];
	return NULL;
}

const tray_sni_item *tray_sni_lookup(const tray_sni_model *model, uint64_t id,
	uint64_t generation)
{
	if (id && generation)
		for (unsigned int i = 0; i < model->count; i++)
			if (model->items[i].id == id &&
			    model->items[i].generation == generation)
				return &model->items[i];
	return NULL;
}

tray_sni_item *tray_sni_admit(tray_sni_model *model, const char *owner,
	const char *path, const char *interface)
{
	if (!tray_sni_owner(owner) || !tray_sni_path(path) ||
	    (!tray_sni_text(interface, 63, false) ||
	     (strcmp(interface, "org.kde.StatusNotifierItem") &&
	     strcmp(interface, "org.freedesktop.StatusNotifierItem"))))
		return NULL;
	for (unsigned int i = 0; i < model->count; i++)
		if (!strcmp(model->items[i].owner, owner) &&
		    !strcmp(model->items[i].path, path))
			return &model->items[i];
	if (model->count >= model->capacity || model->count >= TRAY_SNI_MAX ||
	    !model->next_id || !model->next_generation)
		return NULL;
	tray_sni_item *item = &model->items[model->count++];
	memset(item, 0, sizeof(*item));
	item->id = model->next_id++;
	item->generation = model->next_generation++;
	strcpy(item->owner, owner);
	strcpy(item->path, path);
	strcpy(item->interface, interface);
	return item;
}

const char *tray_sni_probe_interface(const tray_sni_item *item,
	unsigned int tried)
{
	static const char *interfaces[] = {"org.kde.StatusNotifierItem",
	    "org.freedesktop.StatusNotifierItem"};
	if (!item || !tray_sni_text(item->interface,
	    sizeof(item->interface) - 1, false))
		return NULL;
	unsigned int dialect;
	if (!strcmp(item->interface, interfaces[0]))
		dialect = 0;
	else if (!strcmp(item->interface, interfaces[1]))
		dialect = 1;
	else
		return NULL;
	if (!item->ready && (tried & (1U << dialect)) &&
	    !(tried & (1U << (1 - dialect))))
		dialect = 1 - dialect;
	return interfaces[dialect];
}

bool tray_sni_remove(tray_sni_model *model, uint64_t id)
{
	for (unsigned int i = 0; i < model->count; i++)
		if (model->items[i].id == id)
		{
			model->icon_bytes -= item_bytes(&model->items[i]);
			tray_sni_item_clear(&model->items[i]);
			memmove(&model->items[i], &model->items[i + 1],
			    (model->count - i - 1) * sizeof(model->items[0]));
			memset(&model->items[--model->count], 0,
			    sizeof(model->items[0]));
			if (model->menu.item_id == id)
				tray_sni_menu_close(model);
			return true;
		}
	return false;
}

static bool item_valid(const tray_sni_item *item)
{
	if (!item || !tray_sni_owner(item->owner) ||
	    !tray_sni_path(item->path) ||
	    !tray_sni_text(item->interface, sizeof(item->interface) - 1, false)
	        ||
	    !tray_sni_text(item->category, sizeof(item->category) - 1, false) ||
	    !tray_sni_text(item->identity, sizeof(item->identity) - 1, false) ||
	    !item->identity[0] || !item->category[0] ||
	    !tray_sni_text(item->title, TRAY_SNI_TEXT_MAX, false) ||
	    !tray_sni_text(item->icon_name, TRAY_SNI_TEXT_MAX, false) ||
	    !tray_sni_text(item->attention_name, TRAY_SNI_TEXT_MAX, false) ||
	    !tray_sni_text(item->overlay_name, TRAY_SNI_TEXT_MAX, false) ||
	    !tray_sni_text(item->tooltip_title, TRAY_SNI_TEXT_MAX, false) ||
	    !tray_sni_text(item->tooltip_body, TRAY_SNI_TEXT_MAX, true) ||
	    !tray_sni_text(item->icon_theme_path, TRAY_SNI_PATH_MAX, false) ||
	    (item->icon_theme_path[0] && item->icon_theme_path[0] != '/') ||
	    (item->menu_path[0] && !tray_sni_path(item->menu_path)) ||
	    item->status < TRAY_SNI_PASSIVE ||
	    item->status > TRAY_SNI_ATTENTION)
		return false;
	const tray_sni_pixmap *roles[] = {
	    &item->icon, &item->attention, &item->overlay};
	for (unsigned int i = 0; i < 3; i++)
	{
		const tray_sni_pixmap *pixmap = roles[i];
		if (!pixmap->pixels)
		{
			if (pixmap->width || pixmap->height)
				return false;
		}
		else if (!tray_sni_pixmap_valid(pixmap->width, pixmap->height,
		    (size_t)pixmap->width * pixmap->height * 4))
			return false;
	}
	return true;
}

bool tray_sni_replace(tray_sni_model *model, uint64_t id,
	tray_sni_item *replacement)
{
	tray_sni_item *item = tray_sni_find(model, id);
	if (!item || !item_valid(replacement) || !model->next_generation ||
	    strcmp(item->owner, replacement->owner) ||
	    strcmp(item->path, replacement->path) ||
	    (item->ready && strcmp(item->interface, replacement->interface)) ||
	    (strcmp(replacement->interface, "org.kde.StatusNotifierItem") &&
	     strcmp(replacement->interface,
	     "org.freedesktop.StatusNotifierItem")))
		return false;
	size_t bytes = item_bytes(replacement);
	/* Both versions are alive until this transaction commits. */
	if (bytes > TRAY_SNI_ICON_BYTES - model->icon_bytes)
		return false;
	model->icon_bytes -= item_bytes(item);
	uint64_t identity = item->id;
	tray_sni_item_clear(item);
	*item = *replacement;
	memset(replacement, 0, sizeof(*replacement));
	item->id = identity;
	item->generation = model->next_generation++;
	item->ready = true;
	model->icon_bytes += bytes;
	if (model->menu.item_id == id)
		tray_sni_menu_close(model);
	return true;
}

const tray_sni_menu_node *tray_sni_menu_lookup(const tray_sni_menu *menu,
	int32_t id)
{
	if (menu && menu->count <= TRAY_SNI_MENU_MAX)
		for (unsigned int i = 0; i < menu->count; i++)
			if (menu->nodes[i].id == id)
				return &menu->nodes[i];
	return NULL;
}

bool tray_sni_menu_valid(const tray_sni_menu *menu)
{
	size_t bytes = 0;
	if (!menu || !menu->count || menu->count > TRAY_SNI_MENU_MAX)
		return false;
	for (unsigned int i = 0; i < menu->count; i++)
	{
		const tray_sni_menu_node *node = &menu->nodes[i];
		if (node->id < 0 || node->depth > TRAY_SNI_MENU_DEPTH ||
		    !tray_sni_text(node->label, sizeof(node->label) - 1, false)
		        ||
		    !tray_sni_text(node->icon_name,
		    sizeof(node->icon_name) - 1, false) ||
		    node->toggle_type < 0 || node->toggle_type > 2 ||
		    node->toggle_state < -1 || node->toggle_state > 1)
			return false;
		bytes += strlen(node->label) + strlen(node->icon_name) +
		    sizeof(*node) - sizeof(node->label) - sizeof(
		        node->icon_name);
		if (bytes > TRAY_SNI_MENU_BYTES)
			return false;
		for (unsigned int j = 0; j < i; j++)
			if (menu->nodes[j].id == node->id)
				return false;
		if (!i)
		{
			if (node->depth || node->id != menu->root ||
			    node->parent != -1)
				return false;
		}
		else
		{
			const tray_sni_menu_node *parent =
			    tray_sni_menu_lookup(menu, node->parent);
			if (!parent || parent >= node ||
			    node->depth != parent->depth + 1)
				return false;
		}
	}
	return true;
}
