#ifndef FVWM_TRAY_SNI_MODEL_H
#define FVWM_TRAY_SNI_MODEL_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define TRAY_SNI_MAX 32U
#define TRAY_SNI_OWNER_MAX 255U
#define TRAY_SNI_PATH_MAX 1024U
#define TRAY_SNI_TEXT_MAX 1024U
#define TRAY_SNI_ICON_BYTES (4U * 1024U * 1024U)
#define TRAY_SNI_MENU_MAX 128U
#define TRAY_SNI_MENU_DEPTH 4U
#define TRAY_SNI_MENU_BYTES (64U * 1024U)
#define TRAY_SNI_WIRE_MAX (1024U * 1024U)
#define TRAY_SNI_QUEUE_MAX (2U * 1024U * 1024U)
#define TRAY_SNI_UPLOAD_SIDE_MAX 512U

typedef enum
{
	TRAY_SNI_PASSIVE,
	TRAY_SNI_ACTIVE,
	TRAY_SNI_ATTENTION
} tray_sni_status;

typedef struct
{
	/* Owned normalized host-order, straight-alpha 0xAARRGGBB pixels. */
	uint32_t *pixels;
	unsigned int width, height;
} tray_sni_pixmap;

typedef struct
{
	uint64_t id, generation;
	char owner[TRAY_SNI_OWNER_MAX + 1];
	char path[TRAY_SNI_PATH_MAX + 1];
	char interface[64];
	char category[64], identity[256], title[TRAY_SNI_TEXT_MAX + 1];
	char icon_name[TRAY_SNI_TEXT_MAX + 1];
	char attention_name[TRAY_SNI_TEXT_MAX + 1];
	char overlay_name[TRAY_SNI_TEXT_MAX + 1];
	char icon_theme_path[TRAY_SNI_PATH_MAX + 1];
	char tooltip_title[TRAY_SNI_TEXT_MAX + 1];
	char tooltip_body[TRAY_SNI_TEXT_MAX + 1];
	char menu_path[TRAY_SNI_PATH_MAX + 1];
	tray_sni_pixmap icon, attention, overlay;
	tray_sni_status status;
	uint32_t window_id;
	bool item_is_menu, ready;
} tray_sni_item;

typedef struct
{
	int32_t id, parent;
	char label[512], icon_name[512];
	unsigned int depth;
	bool visible, enabled, separator, submenu;
	/* 0 none, 1 checkmark, 2 radio; -1 indeterminate, 0 off, 1 on. */
	int toggle_type, toggle_state;
} tray_sni_menu_node;

typedef struct
{
	uint64_t generation, item_id, item_generation;
	uint32_t revision;
	int32_t root, parent;
	unsigned int count;
	bool open, ready, stale;
	tray_sni_menu_node nodes[TRAY_SNI_MENU_MAX];
} tray_sni_menu;

typedef struct
{
	tray_sni_item items[TRAY_SNI_MAX];
	unsigned int count, capacity;
	uint64_t next_id, next_generation;
	size_t icon_bytes;
	tray_sni_menu menu;
} tray_sni_model;

bool tray_sni_text(const char *text, size_t maximum, bool multiline);
bool tray_sni_owner(const char *owner);
bool tray_sni_path(const char *path);
bool tray_sni_pixmap_valid(unsigned int width, unsigned int height,
	size_t bytes);
/* Nearest straight-ARGB copy into a distinct caller-owned buffer. All
 * validation precedes writes; failure leaves destination unchanged. */
bool tray_sni_resample_argb(const uint32_t *source, size_t source_count,
	unsigned int source_width, unsigned int source_height,
	uint32_t *destination, size_t destination_count,
	unsigned int width, unsigned int height);
void tray_sni_pixmap_clear(tray_sni_pixmap *pixmap);
void tray_sni_item_clear(tray_sni_item *item);
void tray_sni_model_init(tray_sni_model *model);
void tray_sni_model_clear(tray_sni_model *model);
tray_sni_item *tray_sni_find(tray_sni_model *model, uint64_t id);
const tray_sni_item *tray_sni_lookup(const tray_sni_model *model, uint64_t id,
	uint64_t generation);
/* Initial discovery may probe another interface without changing the stored
 * item. Once ready, property and action calls stay on its committed dialect. */
const char *tray_sni_probe_interface(const tray_sni_item *item,
	unsigned int tried);
tray_sni_item *tray_sni_admit(tray_sni_model *model, const char *owner,
	const char *path, const char *interface);
bool tray_sni_remove(tray_sni_model *model, uint64_t id);
bool tray_sni_replace(tray_sni_model *model, uint64_t id,
	tray_sni_item *replacement);
void tray_sni_menu_close(tray_sni_model *model);
bool tray_sni_menu_begin(tray_sni_model *model, uint64_t id,
	uint64_t generation, int32_t parent);
const tray_sni_menu_node *tray_sni_menu_lookup(const tray_sni_menu *menu,
	int32_t id);
bool tray_sni_menu_valid(const tray_sni_menu *menu);

#endif
