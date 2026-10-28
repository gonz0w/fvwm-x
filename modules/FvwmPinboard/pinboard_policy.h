#ifndef FVWM_PINBOARD_POLICY_H
#define FVWM_PINBOARD_POLICY_H

#include "libs/module_workarea.h"
#include <stdbool.h>
#include <stdint.h>

#define PINBOARD_MAX 256
#define PINBOARD_LINE 1000
#define PINBOARD_LABEL 256
#define PINBOARD_PICTURE 384

typedef struct
{
	unsigned int width, height, icon;
} pinboard_layout;

typedef struct
{
	char id[33], monitor[257], label[257], picture[385];
	unsigned int column, row;
	bool missing, labelled, pictured;
} pinboard_entry;

typedef struct
{
	unsigned int generation, count;
	pinboard_layout layout;
	pinboard_entry entries[PINBOARD_MAX];
} pinboard_scene;

typedef struct
{
	char owner[33];
	pinboard_scene current, pending;
	unsigned int received;
	bool staging, poisoned;
} pinboard_model;

typedef enum
{
	PINBOARD_BAD,
	PINBOARD_OWNER,
	PINBOARD_BEGIN,
	PINBOARD_STAGE,
	PINBOARD_COMMIT,
	PINBOARD_ABORT,
	PINBOARD_STATUS,
	PINBOARD_CELLS,
	PINBOARD_RENEW,
	PINBOARD_RETIRE
} pinboard_command;

typedef struct
{
	pinboard_command command;
	char tag[65];
	unsigned int offset, limit;
} pinboard_request;

typedef struct
{
	char monitor[257];
	unsigned int quarters, column, row;
	int x, y, width, height;
	bool visible;
} pinboard_cell;

typedef struct
{
	uint64_t epoch;
	pinboard_cell manage, cells[PINBOARD_MAX];
} pinboard_projection;

typedef struct
{
	int source_x, source_y, destination_x, destination_y, width, height;
} pinboard_blit;

typedef struct
{
	unsigned long output, page[8], desk;
	bool used, has_page, has_desk;
} pinboard_workspace_row;

typedef struct
{
	pinboard_workspace_row rows[MODULE_UI_SCALE_MAX];
} pinboard_workspace;

typedef enum
{
	PINBOARD_CONFIG_NONE,
	PINBOARD_CONFIG_FONT,
	PINBOARD_CONFIG_COLORSET,
	PINBOARD_CONFIG_WIDTH,
	PINBOARD_CONFIG_HEIGHT,
	PINBOARD_CONFIG_ICON
} pinboard_config;

/* First observations seed facts. Only effective changes request config: the
 * core's config reply itself reissues DesktopSize and sends M_NEW_PAGE. */
bool pinboard_workspace_changed(pinboard_workspace *state, bool page,
				const unsigned long *body, size_t words);
/* A new topology retires old output identities. An unsolicited publication
 * needs a baseline config request; with a live view, coalesce a follow-up even
 * if an older request is in flight. Same-epoch replay never requests/reset. */
bool pinboard_workspace_publication(pinboard_workspace *state,
				    module_ui_scale_status status,
				    bool request_pending, bool active_view);
/* The core canonicalizes '*Alias: Font ...' to '*AliasFont ...'. Both forms
 * are admitted only for the five known native configuration options. */
pinboard_config pinboard_config_parse(char *text, const char *alias,
				      char **value);

bool pinboard_hex(const char *text, char *bytes, size_t capacity);
void pinboard_encode(const char *bytes, char *hex);
bool pinboard_number(const char *text, unsigned int maximum,
		     unsigned int *value);
bool pinboard_layout_valid(pinboard_layout layout);
/* Parsing never commits. A malformed owned record poisons pending state;
 * old committed data remains untouched until validated resource admission. */
pinboard_request pinboard_receive(pinboard_model *model, const char *text,
				  size_t alias_length);
bool pinboard_pending_valid(const pinboard_model *model);
void pinboard_publish(pinboard_model *model);
bool pinboard_project(const pinboard_scene *scene,
		      const module_ui_scale_snapshot *snapshot,
		      const module_workarea_state *workareas,
		      const rectangle *drawable,
		      pinboard_projection *projection);
/* Root-pixel drag/key proposals return logical monitor-relative coordinates. */
bool pinboard_move_cell(const pinboard_layout *layout,
			const module_ui_scale_snapshot *snapshot,
			const module_workarea_state *workareas,
			const rectangle *drawable, int x, int y,
			pinboard_cell *cell);
/* The native picture helper can replace the GC clip and use an unclipped
 * XRender destination. Bound its actual source/destination copy explicitly. */
bool pinboard_picture_clip(const pinboard_cell *cell, int picture_width,
			   int picture_height, pinboard_blit *copy);

#endif
