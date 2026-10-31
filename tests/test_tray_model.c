/* This program is free software; you can redistribute it and/or modify it
 * under the GNU General Public License, version 2 or any later version. */
#include "config.h"
#include "modules/FvwmTray/tray_model.h"
#include <assert.h>
#include <string.h>

int main(void)
{
	tray_model model = {0};
	tray_layout layout;
	bool mapped = true;
	uint32_t info[2] = {99, 1};
	assert(tray_model_info(info, &mapped) && mapped);
	info[1] = 0;
	assert(tray_model_info(info, &mapped) && !mapped);
	info[1] = 2;
	assert(!tray_model_info(info, &mapped) && !mapped);
	assert(!tray_model_info(NULL, &mapped));
	assert(!tray_model_info(info, NULL));
	assert(!tray_model_size(0, 1));
	assert(!tray_model_size(1, 257));
	assert(tray_model_size(256, 256));
	assert(tray_model_add(&model, 0, 20, 20, true) == TRAY_INVALID);
	for (unsigned int i = 1; i <= TRAY_CLIENT_MAX; i++)
		assert(tray_model_add(&model, i, 20, 30, true) ==
		    TRAY_ADMITTED);
	tray_model before = model;
	assert(tray_model_add(&model, 1, 20, 30, true) == TRAY_DUPLICATE);
	assert(tray_model_add(&model, 33, 20, 30, true) == TRAY_FULL);
	assert(!memcmp(&before, &model, sizeof(model)));
	assert(tray_model_layout(&model, 100, 100, 4, 4, 48, 24,
	    false, true, &layout));
	assert(layout.count == 4 && layout.width == 100);
	assert(layout.cells[0].width == 20 && layout.cells[0].height == 30);
	assert(layout.cells[3].visible && !layout.cells[4].visible);
	assert(tray_model_focus(&model, &layout, 0) == 1);
	assert(tray_model_focus(&model, &layout, -1) == 4);
	assert(tray_model_focus(&model, &layout, 1) == 1);
	assert(tray_model_mapped(&model, 1, false) && !model.focused);
	assert(tray_model_resize(&model, 2, 256, 256));
	assert(!tray_model_resize(&model, 2, 257, 20));
	assert(tray_model_layout(&model, 100, 100, 4, 4, 48, 24,
	    false, true, &layout) && layout.count == 4);
	uint64_t old_generation = model.clients[1].generation;
	assert(tray_model_remove(&model, 2));
	assert(!tray_model_remove(&model, 2));
	assert(tray_model_add(&model, 2, 20, 30, true) == TRAY_ADMITTED);
	assert(model.clients[model.count - 1].generation > old_generation);
	assert(tray_model_layout(&model, 100, 100, 4, 4, 48, 24,
	    true, true, &layout) && layout.count == 2);
	assert(tray_model_layout(&model, 100, 100, 4, 4, 48, 24,
	    true, false, &layout) && !layout.count);
	assert(tray_model_layout(&model, 1, 1, 4096, 4096, 4096, 4096,
	    false, true, &layout) && !layout.count);
	assert(!tray_model_layout(&model, INT32_MAX, 100, 4, 4, 48, 24,
	    false, true, &layout));
	assert(!tray_model_layout(&model, 100, 100, -1, 4, 48, 24,
	    false, true, &layout));
	/* Anchor parsing is strict and publishes only complete valid tuples. */
	tray_rect anchor = {1, 2, 3, 4}, area = {-800, -600, 800, 600};
	tray_rect placed;
	assert(tray_model_anchor("-790 -590 40 20", &anchor));
	tray_rect saved_anchor = anchor;
	assert(!tray_model_anchor("1 2 3 4 junk", &anchor));
	assert(!tray_model_anchor("1 2 3 4 5", &anchor));
	assert(!tray_model_anchor("1 2 0 4", &anchor));
	assert(!tray_model_anchor("32760 0 20 1", &anchor));
	assert(!tray_model_anchor("1+2 3 4", &anchor));
	assert(!memcmp(&saved_anchor, &anchor, sizeof(anchor)));
	assert(tray_model_place(&anchor, &area, 100, 60, &placed));
	assert(placed.x == -790 && placed.y == -570);
	anchor = (tray_rect){-20, -30, 20, 20};
	assert(tray_model_place(&anchor, &area, 100, 60, &placed));
	assert(placed.x == -100 && placed.y == -90);
	assert(!tray_model_place(&anchor, &area, 801, 60, &placed));
	tray_model pages = {0};
	assert(tray_model_add(&pages, 1, 256, 256, true) == TRAY_ADMITTED);
	for (unsigned int i = 2; i < 8; i++)
		assert(tray_model_add(&pages, i, 30, 20, true) ==
		    TRAY_ADMITTED);
	assert(tray_model_page(&pages, 80, 80, 4, 4, 48, 24,
	    false, true, 0, &layout));
	assert(layout.pages == 3 && layout.page == 0 && layout.count == 2);
	assert(layout.omitted == 1 && !layout.cells[0].visible);
	assert(layout.cells[1].visible && layout.cells[2].visible);
	assert(tray_model_page(&pages, 80, 80, 4, 4, 48, 24,
	    false, true, 2, &layout));
	assert(layout.page == 2 && layout.cells[5].visible);
	assert(layout.cells[6].visible && !layout.cells[1].visible);
	assert(tray_model_page(&pages, 80, 80, 4, 4, 48, 24,
	    false, true, 99, &layout) && layout.page == 2);
	assert(tray_model_page(&pages, 80, 80, 4, 4, 48, 24,
	    false, false, 1, &layout) && !layout.count && layout.pages == 3);
	/* Every fitting mapped member appears exactly once in each orientation.
	 * Hidden, oversized and unmapped members do not consume a page slot. */
	assert(tray_model_mapped(&pages, 4, false));
	for (unsigned int vertical = 0; vertical < 2; vertical++)
	{
		unsigned int seen[TRAY_CLIENT_MAX] = {0};
		assert(tray_model_page(&pages, 80, 80, 4, 4, 48, 24,
		    vertical != 0, true, 0, &layout));
		unsigned int count = layout.pages;
		for (unsigned int page = 0; page < count; page++)
		{
			assert(tray_model_page(&pages, 80, 80, 4, 4, 48, 24,
			    vertical != 0, true, page, &layout));
			for (unsigned int j = 0; j < pages.count; j++)
			{
				const tray_cell *cell = &layout.cells[j];
				if (!cell->visible)
					continue;
				seen[j]++;
				assert(cell->width == 30 && cell->height == 20);
				assert(cell->x >= 4 && cell->y >= 32);
				assert(cell->x + cell->width <= 76);
				assert(cell->y + cell->height <= 76);
			}
		}
		for (unsigned int j = 0; j < pages.count; j++)
			assert(seen[j] == (j != 0 && j != 3 ? 1U : 0U));
	}
	assert(tray_model_resize(&pages, 1, 30, 20));
	assert(tray_model_page(&pages, 80, 80, 4, 4, 48, 24,
	    false, true, 0, &layout));
	assert(!layout.omitted && layout.cells[0].visible);
	anchor = (tray_rect){-32768, -32768, 1, 1};
	area = (tray_rect){-32768, -32768, 100, 100};
	assert(tray_model_place(&anchor, &area, 100, 100, &placed));
	assert(placed.x == -32768 && placed.y == -32768);
	assert(!tray_model_anchor("1 2 3 4\n", &anchor));
	assert(!tray_model_anchor("1 2 3 4.0", &anchor));
	assert(!tray_model_anchor("9999999999999999999999 0 1 1", &anchor));
	/* Native-owned cells may scale; the foreign admission/resize contract
	 * remains1..256 actual pixels and cannot be used on an owned cell. */
	tray_model mixed = {0};
	assert(tray_model_add(&mixed, 1, 512, 512, true) == TRAY_INVALID);
	assert(tray_model_add_native(&mixed, 1, 512, 512, true) ==
	    TRAY_ADMITTED);
	assert(mixed.clients[0].kind == TRAY_CLIENT_NATIVE);
	assert(!tray_model_resize(&mixed, 1, 20, 20));
	assert(tray_model_resize_native(&mixed, 1, 24, 24));
	assert(tray_model_add(&mixed, 2, 24, 20, true) == TRAY_ADMITTED);
	assert(!tray_model_resize_native(&mixed, 2, 512, 512));
	assert(!tray_model_resize_native(&mixed, 1, 4097, 1));
	tray_cell previous, next;
	for (unsigned int id = 3; id <= 10; id++)
		assert(tray_model_add(&mixed, id, 24, 20, true) ==
		    TRAY_ADMITTED);
	assert(tray_model_compact(&mixed, 152, 36, 4, 4, 24, false, true,
	    0, &layout, &previous, &next));
	assert(layout.width == 152 && layout.height == 36);
	assert(layout.pages > 1 && previous.visible && next.visible);
	unsigned int compact_pages = layout.pages;
	unsigned int compact_seen[TRAY_CLIENT_MAX] = {0};
	for (unsigned int page = 0; page < compact_pages; page++)
	{
		assert(tray_model_compact(&mixed, 152, 36, 4, 4, 24, false,
		    true, page, &layout, &previous, &next));
		for (unsigned int i = 0; i < mixed.count; i++)
			if (layout.cells[i].visible)
			{
				compact_seen[i]++;
				assert(layout.cells[i].x >=
				    previous.x + previous.width);
				assert(layout.cells[i].x +
				    layout.cells[i].width <= next.x);
				assert(layout.cells[i].y +
				    layout.cells[i].height <= 32);
			}
	}
	for (unsigned int i = 0; i < mixed.count; i++)
		assert(compact_seen[i] == 1);
	assert(tray_model_compact(&mixed, 30, 36, 4, 4, 24, false, true,
	    0, &layout, &previous, &next));
	assert(!previous.visible && !next.visible);
	assert(tray_model_compact(&mixed, 152, 36, 4, 4, 24, false, false,
	    0, &layout, &previous, &next));
	assert(!layout.count && !previous.visible && !next.visible);
	/* Side controls yield to wide members: every fitting icon remains
	 * reachable through keyboard pages, including mixed-width pages. */
	tray_model wide = {0};
	assert(tray_model_add(&wide, 1, 80, 20, true) == TRAY_ADMITTED);
	assert(tray_model_add(&wide, 2, 80, 20, true) == TRAY_ADMITTED);
	for (unsigned int page = 0; page < 2; page++)
	{
		assert(tray_model_compact(&wide, 100, 36, 4, 4, 24, false,
		    true, page, &layout, &previous, &next));
		assert(layout.pages == 2 && layout.count == 1);
		assert(layout.cells[page].visible && !layout.omitted);
		assert(!previous.visible && !next.visible);
	}
	assert(tray_model_resize(&wide, 2, 20, 20));
	assert(tray_model_add(&wide, 3, 20, 20, true) == TRAY_ADMITTED);
	for (unsigned int page = 0; page < 2; page++)
	{
		assert(tray_model_compact(&wide, 100, 36, 4, 4, 24, false,
		    true, page, &layout, &previous, &next));
		assert(layout.pages == 2 && !layout.omitted);
		assert(layout.count == (page ? 2U : 1U));
		assert(!previous.visible && !next.visible);
	}
	/* Pointer hits use the painted viewport, not total menu membership. */
	tray_rect rows = {4, 4, 272, 288};
	unsigned int hit = 99;
	assert(tray_model_row_at(&rows, 24, 12, 4, 291, &hit));
	assert(hit == 11);
	assert(!tray_model_row_at(&rows, 24, 12, 4, 292, &hit));
	assert(!hit);
	assert(!tray_model_row_at(&rows, 24, 12, 3, 4, &hit));
	assert(!tray_model_row_at(&rows, 24, 12, 276, 4, &hit));
	assert(!tray_model_row_at(&rows, 24, 12, 4, 3, &hit));
	assert(!tray_model_row_at(&rows, 24, 0, 4, 4, &hit));
	rows.height = 10;
	assert(tray_model_row_at(&rows, 24, 1, 4, 13, &hit));
	assert(!tray_model_row_at(&rows, 24, 1, 4, 14, &hit));
	model.generation = UINT64_MAX;
	assert(tray_model_remove(&model, 3));
	assert(tray_model_add(&model, 3, 20, 20, true) == TRAY_INVALID);
	return 0;
}
