/* This program is free software; you can redistribute it and/or modify it
 * under the GNU General Public License, version 2 or any later version. */
#include "config.h"
#include "modules/FvwmTray/tray_sni_model.h"
#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void text_and_geometry(void)
{
	assert(tray_sni_text("", 0, false));
	assert(!tray_sni_text(NULL, 4, false));
	assert(tray_sni_text("1234", 4, false));
	assert(!tray_sni_text("12345", 4, false));
	assert(tray_sni_text("\xe2\x82\xac", 3, false));
	assert(!tray_sni_text("\xc0\xaf", 2, false));
	assert(!tray_sni_text("\xed\xa0\x80", 3, false));
	assert(!tray_sni_text("\xf4\x90\x80\x80", 4, false));
	assert(!tray_sni_text("\x80", 1, false));
	assert(!tray_sni_text("\xc2\x80", 2, false));
	assert(!tray_sni_text("bad\nline", 8, false));
	assert(tray_sni_text("a\n\tb", 4, true));
	assert(!tray_sni_text("\r", 1, true));
	assert(tray_sni_owner(":1.42"));
	assert(!tray_sni_owner("org.example.Client"));
	assert(!tray_sni_owner(":nodot"));
	assert(!tray_sni_owner(":.1"));
	assert(!tray_sni_owner(":1."));
	assert(!tray_sni_owner(":1..2"));
	assert(!tray_sni_owner(":.."));
	assert(tray_sni_owner(":1.2.component_3-last"));
	assert(tray_sni_path("/"));
	assert(tray_sni_path("/StatusNotifierItem_1"));
	assert(!tray_sni_path("/bad//path"));
	assert(!tray_sni_path("/bad/"));
	assert(!tray_sni_path("/bad-name"));
	assert(tray_sni_pixmap_valid(256, 256, 256U * 256U * 4U));
	assert(!tray_sni_pixmap_valid(0, 24, 0));
	assert(!tray_sni_pixmap_valid(257, 1, 257U * 4U));
	assert(!tray_sni_pixmap_valid(24, 20, 24U * 20U * 4U - 1));
}

static void resample_argb(void)
{
	const uint32_t source[4] = {0x00ff0000U, 0x8000ff00U,
	    0xff0000ffU, 0x40ffffffU};
	uint32_t output[16], saved[16];
	assert(tray_sni_resample_argb(source, 4, 2, 2, output, 16, 4, 4));
	for (unsigned int y = 0; y < 4; y++)
		for (unsigned int x = 0; x < 4; x++)
			assert(output[y * 4 + x] == source[y / 2 * 2 + x / 2]);
	uint32_t down[4];
	assert(tray_sni_resample_argb(output, 16, 4, 4, down, 4, 2, 2));
	assert(!memcmp(down, source, sizeof(source)));
	const uint32_t rectangular[6] = {1, 2, 3, 4, 5, 6};
	const uint32_t expected[6] = {1, 2, 1, 2, 4, 5};
	assert(tray_sni_resample_argb(rectangular, 6, 3, 2,
	    output, 16, 2, 3));
	assert(!memcmp(output, expected, sizeof(expected)));
	for (unsigned int i = 0; i < 16; i++)
		output[i] = 0x12345678U;
	memcpy(saved, output, sizeof(saved));
	const struct {
		unsigned int source_width, source_height, width, height;
		size_t input, output;
	} invalid[] = {
	    {0, 1, 1, 1, 4, 16}, {1, 0, 1, 1, 4, 16},
	    {257, 1, 1, 1, 4, 16}, {1, 257, 1, 1, 4, 16},
	    {UINT_MAX, UINT_MAX, 1, 1, SIZE_MAX, 16},
	    {1, 1, 0, 1, 4, 16}, {1, 1, 1, 0, 4, 16},
	    {1, 1, 513, 1, 4, 16}, {1, 1, 1, 513, 4, 16},
	    {1, 1, UINT_MAX, UINT_MAX, 4, SIZE_MAX},
	    {2, 2, 4, 4, 3, 16}, {2, 2, 4, 4, 4, 15},
	    {1, 1, 1, 1, SIZE_MAX, 16}, {1, 1, 1, 1, 4, SIZE_MAX}
	};
	for (unsigned int i = 0; i < sizeof(invalid) / sizeof(invalid[0]); i++)
	{
		assert(!tray_sni_resample_argb(source, invalid[i].input,
		    invalid[i].source_width, invalid[i].source_height, output,
		    invalid[i].output, invalid[i].width, invalid[i].height));
		assert(!memcmp(output, saved, sizeof(saved)));
	}
	assert(!tray_sni_resample_argb(NULL, 1, 1, 1, output, 16, 1, 1));
	assert(!tray_sni_resample_argb(source, 4, 2, 2, NULL, 16, 4, 4));
	assert(!tray_sni_resample_argb(output, 16, 2, 2, output, 16, 4, 4));
	assert(!tray_sni_resample_argb(output + 1, 15, 1, 1,
	    output, 16, 2, 1));
	assert(!memcmp(output, saved, sizeof(saved)));
	/* The maximum scale-derived destination stays within the pure cap. */
	size_t maximum = TRAY_SNI_UPLOAD_SIDE_MAX * TRAY_SNI_UPLOAD_SIDE_MAX;
	uint32_t *large = calloc(maximum, sizeof(*large));
	uint32_t *maximum_source = calloc(256U * 256U, sizeof(*maximum_source));
	assert(large && maximum_source);
	maximum_source[0] = source[1];
	assert(tray_sni_resample_argb(maximum_source, 256U * 256U, 256, 256,
	    large, maximum, 1, 1));
	assert(large[0] == source[1]);
	assert(tray_sni_resample_argb(source + 1, 1, 1, 1, large, maximum,
	    TRAY_SNI_UPLOAD_SIDE_MAX, TRAY_SNI_UPLOAD_SIDE_MAX));
	assert(large[0] == source[1] && large[maximum - 1] == source[1]);
	free(maximum_source);
	free(large);
}

static tray_sni_item replacement(const tray_sni_item *previous)
{
	tray_sni_item item = {0};
	strcpy(item.owner, previous->owner);
	strcpy(item.path, previous->path);
	strcpy(item.interface, previous->interface);
	strcpy(item.category, "ApplicationStatus");
	strcpy(item.identity, "independent-client");
	strcpy(item.title, "Status item");
	item.status = TRAY_SNI_ACTIVE;
	return item;
}

static void identity_and_atomic_replacement(tray_sni_model *model)
{
	tray_sni_item *first = tray_sni_admit(model, ":1.2", "/One",
	    "org.kde.StatusNotifierItem");
	assert(first && first->id == 1 && model->count == 1);
	uint64_t id = first->id, generation = first->generation;
	assert(tray_sni_admit(model, ":1.2", "/One",
	    "org.freedesktop.StatusNotifierItem") == first);
	assert(model->count == 1);
	tray_sni_item item = replacement(first);
	item.icon.width = item.icon.height = 1;
	item.icon.pixels = calloc(1, sizeof(uint32_t));
	assert(item.icon.pixels);
	item.icon.pixels[0] = 0xff123456U;
	assert(tray_sni_replace(model, id, &item));
	assert(!item.icon.pixels && !item.id);
	first = tray_sni_find(model, id);
	assert(first && first->generation != generation && first->ready);
	assert(!tray_sni_lookup(model, id, generation));
	assert(model->icon_bytes == 4 && first->icon.pixels[0] == 0xff123456U);
	generation = first->generation;
	item = replacement(first);
	strcpy(item.owner, ":1.3");
	assert(!tray_sni_replace(model, id, &item));
	assert(tray_sni_lookup(model, id, generation) == first);
	item = replacement(first);
	memset(item.title, 'x', sizeof(item.title));
	assert(!tray_sni_replace(model, id, &item));
	item = replacement(first);
	item.icon.width = 257;
	assert(!tray_sni_replace(model, id, &item));
	model->menu.open = true;
	model->menu.item_id = id;
	assert(tray_sni_remove(model, id));
	assert(!model->count && !model->icon_bytes && !model->menu.open);
	assert(!tray_sni_find(model, id));
	assert(!tray_sni_remove(model, id));
	first = tray_sni_admit(model, ":1.2", "/One",
	    "org.kde.StatusNotifierItem");
	assert(first && first->id != id);
	tray_sni_model_clear(model);
}

static void capacity_and_exhaustion(tray_sni_model *model)
{
	char path[32];
	model->capacity = 2;
	for (unsigned int i = 0; i < 2; i++)
	{
		snprintf(path, sizeof(path), "/Item%u", i);
		assert(tray_sni_admit(model, ":1.8", path,
		    "org.kde.StatusNotifierItem"));
	}
	assert(!tray_sni_admit(model, ":1.8", "/Third",
	    "org.kde.StatusNotifierItem"));
	assert(model->count == 2);
	tray_sni_model_clear(model);
	model->capacity = TRAY_SNI_MAX;
	model->next_id = UINT64_MAX;
	assert(tray_sni_admit(model, ":1.8", "/Last",
	    "org.kde.StatusNotifierItem"));
	assert(!tray_sni_admit(model, ":1.8", "/Exhausted",
	    "org.kde.StatusNotifierItem"));
	tray_sni_model_clear(model);
}

static void replacement_budget(void)
{
	tray_sni_model *model = calloc(1, sizeof(*model));
	assert(model);
	tray_sni_model_init(model);
	for (unsigned int i = 0; i < 16; i++)
	{
		char path[32];
		snprintf(path, sizeof(path), "/Image%u", i);
		tray_sni_item *item = tray_sni_admit(model, ":1.4", path,
		    "org.kde.StatusNotifierItem");
		assert(item);
		tray_sni_item update = replacement(item);
		update.icon.width = update.icon.height = 256;
		update.icon.pixels = calloc(256U * 256U, sizeof(uint32_t));
		assert(update.icon.pixels);
		assert(tray_sni_replace(model, item->id, &update));
	}
	assert(model->icon_bytes == TRAY_SNI_ICON_BYTES);
	tray_sni_item *item = &model->items[0];
	uint64_t generation = item->generation;
	tray_sni_item update = replacement(item);
	update.icon.width = update.icon.height = 1;
	update.icon.pixels = calloc(1, sizeof(uint32_t));
	assert(update.icon.pixels);
	assert(!tray_sni_replace(model, item->id, &update));
	assert(model->icon_bytes == TRAY_SNI_ICON_BYTES);
	assert(item->generation == generation && item->icon.width == 256);
	tray_sni_item_clear(&update);
	tray_sni_model_clear(model);
	assert(model->icon_bytes == 0);
	free(model);
}

static void menu_generation_exhaustion(void)
{
	tray_sni_model *model = calloc(1, sizeof(*model));
	assert(model);
	tray_sni_model_init(model);
	tray_sni_item *item = tray_sni_admit(model, ":1.2", "/MenuOwner",
	    "org.kde.StatusNotifierItem");
	assert(item);
	tray_sni_item update = replacement(item);
	assert(tray_sni_replace(model, item->id, &update));
	uint64_t id = item->id, generation = item->generation;
	model->next_generation = UINT64_MAX;
	assert(tray_sni_menu_begin(model, id, generation, 0));
	assert(model->menu.generation == UINT64_MAX && model->menu.open);
	assert(!model->next_generation);
	/* Failed reopen cannot assign zero or reuse the last generation. */
	assert(!tray_sni_menu_begin(model, id, generation, 0));
	assert(model->menu.generation == UINT64_MAX && model->menu.open);
	tray_sni_menu_close(model);
	assert(!model->menu.generation && !model->menu.open);
	assert(!tray_sni_menu_begin(model, id, generation, 0));
	assert(!model->menu.generation && !model->menu.open);
	tray_sni_model_clear(model);
	free(model);
}

static void menu_policy(void)
{
	tray_sni_menu *menu = calloc(1, sizeof(*menu));
	assert(menu);
	menu->count = 3;
	menu->root = 0;
	menu->nodes[0] = (tray_sni_menu_node){.id = 0, .parent = -1};
	menu->nodes[1] = (tray_sni_menu_node){.id = 9, .parent = 0,
	    .depth = 1, .enabled = true, .visible = true, .submenu = true};
	menu->nodes[2] = (tray_sni_menu_node){.id = 11, .parent = 9,
	    .depth = 2, .enabled = true, .visible = true,
	    .toggle_type = 1, .toggle_state = -1};
	assert(tray_sni_menu_valid(menu));
	assert(tray_sni_menu_lookup(menu, 11) == &menu->nodes[2]);
	menu->nodes[2].id = 9;
	assert(!tray_sni_menu_valid(menu));
	menu->nodes[2].id = 11;
	menu->nodes[1].parent = 11;
	assert(!tray_sni_menu_valid(menu));
	menu->nodes[1].parent = 0;
	menu->nodes[2].depth = TRAY_SNI_MENU_DEPTH + 1;
	assert(!tray_sni_menu_valid(menu));
	menu->nodes[2].depth = 2;
	menu->nodes[2].toggle_state = 2;
	assert(!tray_sni_menu_valid(menu));
	free(menu);
}

int main(void)
{
	tray_sni_model *model = calloc(1, sizeof(*model));
	assert(model);
	tray_sni_model_init(model);
	text_and_geometry();
	resample_argb();
	identity_and_atomic_replacement(model);
	capacity_and_exhaustion(model);
	menu_policy();
	replacement_budget();
	menu_generation_exhaustion();
	free(model);
	return 0;
}
