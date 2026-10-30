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
	    false, true, &layout) && !layout.count);
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
	model.generation = UINT64_MAX;
	assert(tray_model_remove(&model, 3));
	assert(tray_model_add(&model, 3, 20, 20, true) == TRAY_INVALID);
	return 0;
}
