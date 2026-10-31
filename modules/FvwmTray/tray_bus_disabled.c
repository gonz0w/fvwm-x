/* This program is free software; you can redistribute it and/or modify it
 * under the GNU General Public License, version 2 or any later version. */
#include "config.h"
#include "tray_bus.h"
#include "libs/safemalloc.h"
#include <stdlib.h>
#include <string.h>

struct tray_bus
{
	tray_sni_model model;
	bool dirty;
};

tray_bus *tray_bus_create(void)
{
	tray_bus *bus = fxcalloc(1, sizeof(*bus));
	tray_sni_model_init(&bus->model);
	return bus;
}

void tray_bus_destroy(tray_bus *bus)
{
	if (bus)
	{
		tray_sni_model_clear(&bus->model);
		free(bus);
	}
}

bool tray_bus_available(void) { return false; }
bool tray_bus_address_valid(const char *address)
{
	(void)address;
	return false;
}
bool tray_bus_open(tray_bus *bus, const char *address, double now)
{
	(void)bus;
	(void)address;
	(void)now;
	return false;
}
void tray_bus_close(tray_bus *bus)
{
	if (bus)
	{
		tray_sni_model_clear(&bus->model);
		bus->dirty = true;
	}
}
bool tray_bus_connected(const tray_bus *bus) { (void)bus; return false; }
int tray_bus_fd(const tray_bus *bus) { (void)bus; return -1; }
short tray_bus_events(const tray_bus *bus) { (void)bus; return 0; }
int tray_bus_timeout(const tray_bus *bus, double now)
{
	(void)bus;
	(void)now;
	return -1;
}
bool tray_bus_service(tray_bus *bus, double now)
{
	(void)bus;
	(void)now;
	return false;
}
bool tray_bus_take_dirty(tray_bus *bus)
{
	bool dirty = bus && bus->dirty;
	if (bus)
		bus->dirty = false;
	return dirty;
}
const tray_sni_model *tray_bus_model(const tray_bus *bus)
{
	return bus ? &bus->model : NULL;
}
void tray_bus_set_capacity(tray_bus *bus, unsigned int capacity)
{
	if (bus)
		bus->model.capacity = capacity > TRAY_SNI_MAX ?
		    TRAY_SNI_MAX : capacity;
}
void tray_bus_set_icon_budget(tray_bus *bus, size_t max_bytes)
{
	(void)bus; (void)max_bytes;
}
bool tray_bus_action(tray_bus *bus, uint64_t id, uint64_t generation,
	tray_bus_action_kind action, int x, int y, int delta, bool vertical,
	double now)
{
	(void)bus; (void)id; (void)generation; (void)action;
	(void)x; (void)y; (void)delta; (void)vertical; (void)now;
	return false;
}
tray_bus_menu_result tray_bus_menu_open(tray_bus *bus, uint64_t id,
	uint64_t generation, int32_t parent, int x, int y, double now)
{
	(void)id; (void)generation; (void)parent;
	(void)x; (void)y; (void)now;
	tray_bus_menu_close(bus);
	return TRAY_BUS_MENU_FAILED;
}
bool tray_bus_menu_click(tray_bus *bus, uint64_t generation, int32_t node,
	uint32_t timestamp, double now)
{
	(void)bus; (void)generation; (void)node; (void)timestamp; (void)now;
	return false;
}
void tray_bus_menu_close(tray_bus *bus)
{
	if (bus)
		tray_sni_menu_close(&bus->model);
}
bool tray_bus_parse_properties(DBusMessage *message, tray_sni_item *item)
{
	(void)message;
	if (item)
		memset(item, 0, sizeof(*item));
	return false;
}
bool tray_bus_parse_menu(DBusMessage *message, tray_sni_menu *menu)
{
	(void)message;
	if (menu)
		memset(menu, 0, sizeof(*menu));
	return false;
}

bool tray_bus_parse_registry_entry(const char *value,
	char service[TRAY_SNI_OWNER_MAX + 1],
	char path[TRAY_SNI_PATH_MAX + 1])
{
	(void)value;
	if (service)
		service[0] = 0;
	if (path)
		path[0] = 0;
	return false;
}
bool tray_bus_apply_properties(tray_sni_model *model, uint64_t id,
	uint64_t generation, unsigned int dialect, DBusMessage *message,
	size_t icon_budget)
{
	(void)model; (void)id; (void)generation; (void)dialect;
	(void)message; (void)icon_budget;
	return false;
}
DBusMessage *tray_bus_action_message(const tray_sni_item *item,
	tray_bus_action_kind action, int x, int y, int delta, bool vertical)
{
	(void)item; (void)action; (void)x; (void)y;
	(void)delta; (void)vertical;
	return NULL;
}
