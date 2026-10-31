#ifndef FVWM_TRAY_BUS_H
#define FVWM_TRAY_BUS_H

#include "tray_sni_model.h"

typedef struct tray_bus tray_bus;
typedef struct DBusMessage DBusMessage;

typedef enum
{
	TRAY_BUS_ACTIVATE,
	TRAY_BUS_SECONDARY,
	TRAY_BUS_CONTEXT,
	TRAY_BUS_SCROLL
} tray_bus_action_kind;

typedef enum
{
	TRAY_BUS_MENU_FAILED = 0,
	TRAY_BUS_MENU_NATIVE,
	TRAY_BUS_MENU_EXTERNAL
} tray_bus_menu_result;

/* Heap owner; snapshots/pixels are borrowed until the next service/close.
 * Retain numeric identities, never item/menu pointers across those calls. */
tray_bus *tray_bus_create(void);
void tray_bus_destroy(tray_bus *bus);
bool tray_bus_available(void);
bool tray_bus_address_valid(const char *address);
bool tray_bus_open(tray_bus *bus, const char *address, double now);
void tray_bus_close(tray_bus *bus);
bool tray_bus_connected(const tray_bus *bus);
int tray_bus_fd(const tray_bus *bus);
short tray_bus_events(const tray_bus *bus);
int tray_bus_timeout(const tray_bus *bus, double now);
bool tray_bus_service(tray_bus *bus, double now);
bool tray_bus_take_dirty(tray_bus *bus);
const tray_sni_model *tray_bus_model(const tray_bus *bus);
/* Native owner supplies the slots left after foreign XEmbed admission. */
void tray_bus_set_capacity(tray_bus *bus, unsigned int capacity);
/* Raw model pixels share this allowance with native upload/picture leases.
 * Existing pixels survive a lower allowance; new replacements are refused. */
void tray_bus_set_icon_budget(tray_bus *bus, size_t max_bytes);
bool tray_bus_action(tray_bus *bus, uint64_t id, uint64_t generation,
	tray_bus_action_kind action, int x, int y, int delta, bool vertical,
	double now);
/* Every attempt retires the previous local menu. NATIVE starts a new local
 * menu; EXTERNAL submits ContextMenu with local state closed. FAILED starts
 * neither. Only NATIVE lets a caller retain local popup/input ownership. */
tray_bus_menu_result tray_bus_menu_open(tray_bus *bus, uint64_t id,
	uint64_t generation, int32_t parent, int x, int y, double now);
bool tray_bus_menu_click(tray_bus *bus, uint64_t menu_generation,
	int32_t node, uint32_t timestamp, double now);
void tray_bus_menu_close(tray_bus *bus);

/* Side-effect-free bounded protocol parsers for native test coverage. Outputs
 * own pixels on success and stay cleared on failure. */
bool tray_bus_parse_properties(DBusMessage *message, tray_sni_item *item);
bool tray_bus_parse_menu(DBusMessage *message, tray_sni_menu *menu);
bool tray_bus_parse_registry_entry(const char *value,
	char service[TRAY_SNI_OWNER_MAX + 1],
	char path[TRAY_SNI_PATH_MAX + 1]);
/* Quiet protocol operations, shared by dispatch and native regression tests.
 * Applying a snapshot is atomic; the action builder transfers its message. */
bool tray_bus_apply_properties(tray_sni_model *model, uint64_t id,
	uint64_t generation, unsigned int dialect, DBusMessage *message,
	size_t icon_budget);
DBusMessage *tray_bus_action_message(const tray_sni_item *item,
	tray_bus_action_kind action, int x, int y, int delta, bool vertical);

#endif
