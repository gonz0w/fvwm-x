/* This program is free software; you can redistribute it and/or modify it
 * under the GNU General Public License, version 2 or any later version. */
#include "config.h"
#include "modules/FvwmPinboard/pinboard_policy.h"
#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define OWNER "0123456789abcdef0123456789abcdef"
#define ID "fedcba9876543210fedcba9876543210"

static pinboard_request receive(pinboard_model *model, const char *message)
{
	return pinboard_receive(model, message, 20);
}

static void configuration_replay(void)
{
	pinboard_workspace workspace = {0};
	unsigned long page[8] = {0, 0, 0, 2560, 2160, 2, 1, 17};
	unsigned long desk[2] = {1, 17};
	char *value;
	char canonical[] = "*FvwmPinboard-testFont Sans:size=10:antialias=True";
	char ordinary[] =
	    "*FvwmPinboard-test: Font Sans:size=10:antialias=True\n";
	char colorset[] = "*FvwmPinboard-testColorset 3";
	char unknown[] = "*FvwmPinboard-testFontExtra ignored";

	assert(pinboard_config_parse(canonical, "FvwmPinboard-test", &value) ==
	       PINBOARD_CONFIG_FONT);
	assert(!strcmp(value, "Sans:size=10:antialias=True"));
	assert(pinboard_config_parse(ordinary, "FvwmPinboard-test", &value) ==
	       PINBOARD_CONFIG_FONT);
	assert(!strcmp(value, "Sans:size=10:antialias=True"));
	assert(pinboard_config_parse(colorset, "FvwmPinboard-test", &value) ==
		   PINBOARD_CONFIG_COLORSET &&
	       !strcmp(value, "3"));
	assert(pinboard_config_parse(unknown, "FvwmPinboard-test", &value) ==
		   PINBOARD_CONFIG_NONE &&
	       value == NULL);
	assert(!pinboard_workspace_changed(&workspace, true, page, 7));
	assert(!pinboard_workspace_changed(&workspace, true, page, 8));
	/* Send_ConfigInfo reissues DesktopSize. Identical replay never starts
	 * another request, regardless of how many replies are observed. */
	for (unsigned int i = 0; i < 1000; i++)
		assert(!pinboard_workspace_changed(&workspace, true, page, 8));
	assert(pinboard_workspace_changed(&workspace, false, desk, 2));
	page[2] = 1;
	assert(!pinboard_workspace_changed(&workspace, true, page, 8));
	assert(!pinboard_workspace_changed(&workspace, false, desk, 2));
	page[0] = 2560;
	assert(pinboard_workspace_changed(&workspace, true, page, 8));
	assert(!pinboard_workspace_changed(&workspace, true, page, 8));
	page[5] = 3;
	assert(pinboard_workspace_changed(&workspace, true, page, 8));
	assert(!pinboard_workspace_changed(&workspace, true, page, 8));
	/* The initial directed config already supplies its own baseline. */
	assert(!pinboard_workspace_publication(
	    &workspace, MODULE_UI_SCALE_PUBLISHED, true, false));
	assert(!pinboard_workspace_changed(&workspace, true, page, 8));
	/* An unsolicited topology publication resets known facts but must
	 * request a baseline even when the first changed desk is only seeded.
	 */
	assert(pinboard_workspace_publication(
	    &workspace, MODULE_UI_SCALE_PUBLISHED, false, true));
	desk[0] = 2;
	assert(!pinboard_workspace_changed(&workspace, false, desk, 2));
	page[2] = 2;
	assert(!pinboard_workspace_changed(&workspace, true, page, 8));
	/* The first newly introduced output is also covered by that baseline.
	 */
	page[7] = 99;
	assert(!pinboard_workspace_changed(&workspace, true, page, 8));
	assert(!pinboard_workspace_publication(
	    &workspace, MODULE_UI_SCALE_UNCHANGED, false, true));
	assert(!pinboard_workspace_changed(&workspace, true, page, 8));
	/* A live-view publication during an older request coalesces exactly
	 * one follow-up through the runtime's existing request_config path. */
	assert(pinboard_workspace_publication(
	    &workspace, MODULE_UI_SCALE_PUBLISHED, true, true));
	assert(!pinboard_workspace_publication(
	    &workspace, MODULE_UI_SCALE_UNCHANGED, true, true));
	assert(pinboard_workspace_publication(
	    &workspace, MODULE_UI_SCALE_PUBLISHED, false, false));
}

static void drawable_geometry(void)
{
	pinboard_scene *scene = calloc(1, sizeof(*scene));
	module_workarea_state workareas = {0};
	module_ui_scale_snapshot snapshot = {.epoch = 1, .count = 2};
	pinboard_projection projection;
	pinboard_cell move;
	rectangle drawable = {0, 0, 500, 300};
	assert(scene);
	scene->layout = (pinboard_layout){96, 96, 32};
	scene->count = 1;
	strcpy(scene->entries[0].monitor, "offroot");
	snapshot.rows[0] = (module_ui_scale_row){.name_atom = 1,
						 .quarters = 4,
						 .primary = true,
						 .rect = {-300, 0, 192, 192}};
	snapshot.rows[1] = (module_ui_scale_row){
	    .name_atom = 2, .quarters = 4, .rect = {0, 0, 500, 300}};
	strcpy(snapshot.rows[0].name, "offroot");
	strcpy(snapshot.rows[1].name, "visible");
	assert(pinboard_project(scene, &snapshot, &workareas, &drawable,
				&projection));
	assert(projection.manage.x == 0 && projection.manage.y == 0);
	assert(!strcmp(projection.manage.monitor, "visible"));
	assert(projection.cells[0].visible && projection.cells[0].x >= 0);
	assert(!strcmp(projection.cells[0].monitor, "visible"));
	assert(!strcmp(scene->entries[0].monitor, "offroot"));
	assert(scene->entries[0].column == 0 && scene->entries[0].row == 0);
	snapshot.count = 1;
	assert(pinboard_project(scene, &snapshot, &workareas, &drawable,
				&projection));
	assert(projection.manage.visible && projection.manage.x == 0 &&
	       projection.manage.y == 0);
	assert(!projection.cells[0].visible);
	assert(!pinboard_move_cell(&scene->layout, &snapshot, &workareas,
				   &drawable, -250, 40, &move));
	assert(!pinboard_move_cell(&scene->layout, &snapshot, &workareas,
				   &drawable, 5, 5, &move));
	/* Grid origin remains -50,-20. Cell 0,0 cannot be admitted, but the
	 * fallback scan begins at original grid indices 1,1, not at 0,0. */
	snapshot.rows[0].rect = (rectangle){-50, -20, 400, 300};
	assert(pinboard_project(scene, &snapshot, &workareas, &drawable,
				&projection));
	assert(projection.cells[0].visible && projection.cells[0].column == 1 &&
	       projection.cells[0].row == 2);
	assert(projection.cells[0].x == 46 && projection.cells[0].y == 172);
	assert(!pinboard_move_cell(&scene->layout, &snapshot, &workareas,
				   &drawable, 1, 1, &move));
	assert(pinboard_move_cell(&scene->layout, &snapshot, &workareas,
				  &drawable, 50, 180, &move));
	assert(move.column == 1 && move.row == 2 && move.x == 46 &&
	       move.y == 172);
	assert(!pinboard_move_cell(&scene->layout, &snapshot, &workareas,
				   &drawable, 349, 279, &move));
	snapshot.rows[0].rect = (rectangle){-50, 0, 110, 110};
	assert(pinboard_project(scene, &snapshot, &workareas, &drawable,
				&projection));
	assert(projection.manage.visible && projection.manage.width == 60);
	assert(!projection.cells[0].visible);
	snapshot.rows[0].rect = (rectangle){600, 0, 192, 192};
	assert(pinboard_project(scene, &snapshot, &workareas, &drawable,
				&projection));
	assert(projection.manage.visible && projection.manage.x == 0);
	assert(!projection.cells[0].visible);
	free(scene);
}

static void picture_geometry(void)
{
	pinboard_cell cell = {.x = 100, .y = 200, .width = 64, .height = 64};
	pinboard_blit copy;
	/* This exact copy geometry reaches both opaque and alpha paths of the
	 * shared helper; it does not depend on either path honoring a GC clip.
	 */
	assert(pinboard_picture_clip(&cell, 128, 128, &copy));
	assert(copy.source_x == 32 && copy.source_y == 0);
	assert(copy.destination_x == 100 && copy.destination_y == 204);
	assert(copy.width == 64 && copy.height == 60);
	assert(copy.source_x + copy.width <= 128 &&
	       copy.source_y + copy.height <= 128);
	assert(copy.destination_x >= cell.x && copy.destination_y >= cell.y);
	assert(copy.destination_x + copy.width <= cell.x + cell.width);
	assert(copy.destination_y + copy.height <= cell.y + cell.height);
	assert(pinboard_picture_clip(&cell, 32, 32, &copy));
	assert(copy.source_x == 0 && copy.source_y == 0 && copy.width == 32 &&
	       copy.height == 32);
	assert(copy.destination_x == 116 && copy.destination_y == 204);
	cell.height = 1;
	assert(!pinboard_picture_clip(&cell, 128, 128, &copy));
	assert(copy.width == 0 && copy.height == 0);
	assert(!pinboard_picture_clip(&cell, 0, 128, &copy));
	assert(!pinboard_picture_clip(&cell, INT_MAX, 128, &copy));
}

int main(void)
{
	pinboard_model *model = calloc(1, sizeof(*model));
	pinboard_projection projection;
	module_workarea_state workareas = {0};
	rectangle drawable = {-800, 0, 1600, 1000};
	module_ui_scale_snapshot snapshot = {.epoch = 1, .count = 1};
	char bytes[8], line[1100];
	unsigned int number;

	assert(model);
	configuration_replay();
	drawable_geometry();
	picture_geometry();
	model->current.layout = (pinboard_layout){96, 96, 32};
	assert(!pinboard_number("01", 100, &number));
	assert(!pinboard_number("+1", 100, &number));
	assert(!pinboard_number("42949672960", 100, &number));
	assert(pinboard_number("100", 100, &number) && number == 100);
	assert(!pinboard_hex("00", bytes, sizeof(bytes)));
	assert(!pinboard_hex("FF", bytes, sizeof(bytes)));
	assert(pinboard_hex("61ff", bytes, sizeof(bytes)));
	assert((unsigned char)bytes[1] == 255);
	assert(receive(model, "Begin " OWNER " 0 1 1").command == PINBOARD_BAD);
	assert(receive(model, "Owner " OWNER).command == PINBOARD_OWNER);
	assert(receive(model, "Begin " OWNER " 0 1 1").command ==
	       PINBOARD_BEGIN);
	assert(receive(model, "Entry " OWNER " 1 " ID " 41 0 0 1").command ==
	       PINBOARD_STAGE);
	assert(receive(model, "Commit " OWNER " 1").command == PINBOARD_BAD);
	assert(model->current.generation == 0);
	assert(receive(model, "Abort " OWNER " 1").command == PINBOARD_ABORT);
	assert(receive(model, "Begin " OWNER " 0 1 1").command ==
	       PINBOARD_BEGIN);
	assert(receive(model, "Layout " OWNER " 1 128 96 48").command ==
	       PINBOARD_STAGE);
	assert(model->current.layout.width == 96);
	assert(receive(model, "Entry " OWNER " 1 " ID " 41 0 0 1").command ==
	       PINBOARD_STAGE);
	assert(receive(model, "Label " OWNER " 1 " ID " 50696e").command ==
	       PINBOARD_STAGE);
	assert(receive(model,
		       "Picture " OWNER " 1 " ID " 2f746d702f70696e2e706e67")
		   .command == PINBOARD_STAGE);
	assert(receive(model, "Commit " OWNER " 1").command == PINBOARD_COMMIT);
	assert(model->current.generation == 0 && pinboard_pending_valid(model));
	pinboard_publish(model);
	assert(model->current.generation == 1 &&
	       model->current.layout.width == 128);
	assert(strcmp(model->current.entries[0].label, "Pin") == 0);
	assert(receive(model, "Begin " OWNER " 0 2 0").command == PINBOARD_BAD);
	assert(receive(model, "Begin " OWNER " 1 2 257").command ==
	       PINBOARD_BAD);
	assert(receive(model, "Cells " OWNER " c1 0 9").command ==
	       PINBOARD_BAD);
	assert(receive(model, "Cells " OWNER " c1 0 8").command ==
	       PINBOARD_CELLS);
	assert(receive(model, "Status " OWNER " s1").command ==
	       PINBOARD_STATUS);
	assert(receive(model, "Status " OWNER " bad;tag").command ==
	       PINBOARD_BAD);
	assert(receive(model, "Begin " OWNER " 1 2 1").command ==
	       PINBOARD_BEGIN);
	assert(receive(model, "Entry " OWNER " 2 " ID " 41 1 1 0").command ==
	       PINBOARD_STAGE);
	assert(receive(model, "Entry " OWNER " 2 " ID " 41 1 1 0").command ==
	       PINBOARD_BAD);
	assert(receive(model, "Label " OWNER " 2 " ID " 50696e").command ==
	       PINBOARD_BAD);
	assert(!pinboard_pending_valid(model) &&
	       model->current.generation == 1);
	assert(receive(model, "Abort " OWNER " 2").command == PINBOARD_ABORT);
	assert(receive(model, "Begin " OWNER " 1 2 1").command ==
	       PINBOARD_BEGIN);
	assert(receive(model, "Entry " OWNER " 2 " ID " 41 1 1 0").command ==
	       PINBOARD_STAGE);
	assert(receive(model, "Label " OWNER " 2 " ID " c080").command ==
	       PINBOARD_BAD);
	assert(model->current.generation == 1);
	/* A queued Commit may be waiting for MonitorUI. Every accepted Begin,
	 * including whitespace variants, explicitly cancels that old intent. */
	assert(receive(model, "Abort " OWNER " 2").command == PINBOARD_ABORT);
	assert(receive(model, "Begin " OWNER " 1 2 0").command ==
	       PINBOARD_BEGIN);
	assert(receive(model, "Commit " OWNER " 2").command == PINBOARD_COMMIT);
	pinboard_request replacement =
	    receive(model, " \tBegin\t" OWNER "\t1\t3\t0");
	assert(replacement.command == PINBOARD_BEGIN);
	assert(model->current.generation == 1 &&
	       model->pending.generation == 3);
	assert(model->staging && pinboard_pending_valid(model));
	assert(receive(model, "Status " OWNER " replacement").command ==
	       PINBOARD_STATUS);
	assert(model->current.generation == 1);
	memset(line, 'x', sizeof(line));
	line[sizeof(line) - 1] = 0;
	assert(receive(model, line).command == PINBOARD_BAD);

	snapshot.rows[0] = (module_ui_scale_row){.name_atom = 1,
						 .quarters = 4,
						 .primary = true,
						 .rect = {-500, 100, 384, 288}};
	strcpy(snapshot.rows[0].name, "A");
	assert(pinboard_project(&model->current, &snapshot, &workareas,
				&drawable, &projection));
	assert(projection.manage.visible && projection.manage.x == -500);
	assert(projection.cells[0].visible && projection.cells[0].row == 1);
	assert(projection.cells[0].x == -500 && projection.cells[0].y == 196);
	strcpy(model->current.entries[0].monitor, "absent");
	model->current.entries[0].column = 2147483647U;
	model->current.entries[0].row = 2147483647U;
	assert(pinboard_project(&model->current, &snapshot, &workareas,
				&drawable, &projection));
	assert(projection.cells[0].visible);
	assert(strcmp(model->current.entries[0].monitor, "absent") == 0);
	snapshot.rows[0].rect.width = 1;
	snapshot.rows[0].rect.height = 1;
	assert(pinboard_project(&model->current, &snapshot, &workareas,
				&drawable, &projection));
	assert(projection.manage.visible && projection.manage.width == 1);
	assert(!projection.cells[0].visible);
	snapshot.rows[0].rect = (rectangle){-500, 100, 768, 576};
	snapshot.rows[0].quarters = 8;
	assert(pinboard_project(&model->current, &snapshot, &workareas,
				&drawable, &projection));
	assert(projection.cells[0].width == 256 &&
	       projection.cells[0].height == 192);
	assert(pinboard_move_cell(&model->current.layout, &snapshot, &workareas,
				  &drawable, -200, 310, &projection.cells[0]));
	assert(projection.cells[0].column == 1 && projection.cells[0].row == 1);
	snapshot.epoch = 0;
	assert(!pinboard_project(&model->current, &snapshot, &workareas,
				 &drawable, &projection));
	free(model);
	puts("pinboard protocol and placement policy passed");
	return 0;
}
