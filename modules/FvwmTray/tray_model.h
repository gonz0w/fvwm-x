#ifndef FVWM_TRAY_MODEL_H
#define FVWM_TRAY_MODEL_H

#include <stdbool.h>
#include <stdint.h>

#define TRAY_CLIENT_MAX 32U
#define TRAY_DIMENSION_MAX 256U
#define TRAY_XEMBED_MAPPED 1U

typedef enum
{
	TRAY_CLIENT_XEMBED,
	TRAY_CLIENT_NATIVE
} tray_client_kind;

typedef struct
{
	uint32_t id;
	uint64_t generation;
	unsigned int width, height;
	bool mapped;
	tray_client_kind kind;
} tray_client;

typedef struct
{
	tray_client clients[TRAY_CLIENT_MAX];
	unsigned int count;
	uint32_t focused;
	uint64_t generation;
} tray_model;

typedef enum
{
	TRAY_ADMITTED,
	TRAY_DUPLICATE,
	TRAY_FULL,
	TRAY_INVALID
} tray_admission;

typedef struct
{
	int x, y, width, height;
	bool visible;
} tray_cell;

typedef struct
{
	int width, height;
	unsigned int count, page, pages, omitted;
	tray_cell cells[TRAY_CLIENT_MAX];
} tray_layout;

typedef struct
{
	int x, y, width, height;
} tray_rect;

/* Anchor parsing leaves output untouched on failure. All geometry is actual
 * root pixels, representable in signed X coordinates, including far edges. */
bool tray_model_anchor(const char *text, tray_rect *anchor);
bool tray_model_place(const tray_rect *anchor, const tray_rect *area,
	int width, int height, tray_rect *placed);
bool tray_model_page(const tray_model *model, int width, int height,
	int padding, int gap, int header_width, int header_height,
	bool vertical, bool visible, unsigned int page, tray_layout *layout);

bool tray_model_compact(const tray_model *model, int width, int height,
	int padding, int gap, int target, bool vertical, bool visible,
	unsigned int page, tray_layout *layout, tray_cell *previous,
	tray_cell *next);
/* Clip pointer hits to the rows actually painted; clear row on refusal. */
bool tray_model_row_at(const tray_rect *bounds, int row_height,
	unsigned int count, int x, int y, unsigned int *row);

bool tray_model_info(const uint32_t words[2], bool *mapped);
bool tray_model_size(unsigned int width, unsigned int height);
int tray_model_index(const tray_model *model, uint32_t id);
tray_admission tray_model_add(tray_model *model, uint32_t id,
	unsigned int width, unsigned int height, bool mapped);
tray_admission tray_model_add_native(tray_model *model, uint32_t id,
	unsigned int width, unsigned int height, bool mapped);
bool tray_model_resize_native(tray_model *model, uint32_t id,
	unsigned int width, unsigned int height);
bool tray_model_remove(tray_model *model, uint32_t id);
bool tray_model_resize(tray_model *model, uint32_t id,
	unsigned int width, unsigned int height);
bool tray_model_mapped(tray_model *model, uint32_t id, bool mapped);
/* Input geometry is actual pixels. Padding/header/gap have already passed
 * the host's logical-to-pixel conversion; foreign sizes are never scaled. */
bool tray_model_layout(const tray_model *model, int width, int height,
	int padding, int gap, int header_width, int header_height,
	bool vertical,
	bool visible, tray_layout *layout);
uint32_t tray_model_focus(tray_model *model, const tray_layout *layout,
	int direction);

#endif
