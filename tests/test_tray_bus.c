/* This program is free software; you can redistribute it and/or modify it
 * under the GNU General Public License, version 2 or any later version. */
#include "config.h"
#include "modules/FvwmTray/tray_bus.h"
#include <dbus/dbus.h>
#include <assert.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>

static DBusMessage *reply(void)
{
	DBusMessage *message = dbus_message_new(
	    DBUS_MESSAGE_TYPE_METHOD_RETURN);
	assert(message);
	dbus_message_set_serial(message, 9);
	assert(dbus_message_set_reply_serial(message, 1));
	assert(dbus_message_set_sender(message, ":1.42"));
	return message;
}

static void property_open(DBusMessageIter *dict, const char *name,
	const char *signature, DBusMessageIter *pair, DBusMessageIter *value)
{
	assert(dbus_message_iter_open_container(dict, DBUS_TYPE_DICT_ENTRY,
	    NULL, pair));
	assert(dbus_message_iter_append_basic(pair, DBUS_TYPE_STRING, &name));
	assert(dbus_message_iter_open_container(pair, DBUS_TYPE_VARIANT,
	    signature, value));
}

static void property_close(DBusMessageIter *dict, DBusMessageIter *pair,
	DBusMessageIter *value)
{
	assert(dbus_message_iter_close_container(pair, value));
	assert(dbus_message_iter_close_container(dict, pair));
}

static void text_property(DBusMessageIter *dict, const char *name,
	const char *text)
{
	DBusMessageIter pair, value;
	property_open(dict, name, "s", &pair, &value);
	assert(dbus_message_iter_append_basic(&value, DBUS_TYPE_STRING, &text));
	property_close(dict, &pair, &value);
}

static void scalar_property(DBusMessageIter *dict, const char *name,
	const char *signature, int type, const void *data)
{
	DBusMessageIter pair, value;
	property_open(dict, name, signature, &pair, &value);
	assert(dbus_message_iter_append_basic(&value, type, data));
	property_close(dict, &pair, &value);
}

static void image_property(DBusMessageIter *dict, const char *name,
	unsigned int variants, int dimension, int byte_count)
{
	DBusMessageIter pair, value, array;
	unsigned char data[4] = {0x80, 0x20, 0x30, 0x40};
	const unsigned char *pixels = data;
	dbus_int32_t size = dimension;
	property_open(dict, name, "a(iiay)", &pair, &value);
	assert(dbus_message_iter_open_container(&value, DBUS_TYPE_ARRAY,
	    "(iiay)", &array));
	for (unsigned int i = 0; i < variants; i++)
	{
		DBusMessageIter row, bytes;
		assert(dbus_message_iter_open_container(&array,
		    DBUS_TYPE_STRUCT,
		    NULL, &row));
		assert(dbus_message_iter_append_basic(&row, DBUS_TYPE_INT32,
		    &size));
		assert(dbus_message_iter_append_basic(&row, DBUS_TYPE_INT32,
		    &size));
		assert(dbus_message_iter_open_container(&row, DBUS_TYPE_ARRAY,
		    "y", &bytes));
		assert(dbus_message_iter_append_fixed_array(&bytes,
		    DBUS_TYPE_BYTE, &pixels, byte_count));
		assert(dbus_message_iter_close_container(&row, &bytes));
		assert(dbus_message_iter_close_container(&array, &row));
	}
	assert(dbus_message_iter_close_container(&value, &array));
	property_close(dict, &pair, &value);
}

static DBusMessage *properties_message(unsigned int variation)
{
	DBusMessage *message = reply();
	DBusMessageIter root, dict;
	dbus_message_iter_init_append(message, &root);
	assert(dbus_message_iter_open_container(&root, DBUS_TYPE_ARRAY,
	    "{sv}", &dict));
	text_property(&dict, "Category", "ApplicationStatus");
	text_property(&dict, "Id", "policy-client");
	text_property(&dict, "Title", "Application title");
	if (variation == 1)
	{
		dbus_int32_t wrong = 1;
		scalar_property(&dict, "Status", "i", DBUS_TYPE_INT32, &wrong);
	}
	else
		text_property(&dict, "Status", variation == 2 ? "Unknown" :
		    variation == 3 ? "Passive" : "NeedsAttention");
	text_property(&dict, "IconName", "example-icon");
	text_property(&dict, "AttentionIconName", "attention-icon");
	text_property(&dict, "OverlayIconName", "overlay-icon");
	text_property(&dict, "IconThemePath", "/tmp/item-icons");
	if (variation == 4)
		text_property(&dict, "IconName", "duplicate");
	if (variation == 5)
		image_property(&dict, "IconPixmap", 1, 1, 3);
	else if (variation == 6)
		image_property(&dict, "IconPixmap", 9, 1, 4);
	else if (variation == 7)
		image_property(&dict, "IconPixmap", 1, 257, 4);
	else
		image_property(&dict, "IconPixmap", 1, 1, 4);
	image_property(&dict, "AttentionIconPixmap", 0, 0, 0);
	image_property(&dict, "OverlayIconPixmap", 0, 0, 0);
	dbus_bool_t is_menu = TRUE;
	scalar_property(&dict, "ItemIsMenu", "b", DBUS_TYPE_BOOLEAN, &is_menu);
	const char *path = "/Menu";
	scalar_property(&dict, "Menu", "o", DBUS_TYPE_OBJECT_PATH, &path);
	if (variation == 8)
	{
		dbus_uint32_t window = 123;
		scalar_property(&dict, "WindowId", "u", DBUS_TYPE_UINT32,
		    &window);
	}
	else
	{
		dbus_int32_t window = -1;
		scalar_property(&dict, "WindowId", "i", DBUS_TYPE_INT32,
		    &window);
	}
	if (variation == 9)
	{
		char text[1026];
		memset(text, 'a', sizeof(text) - 1);
		text[sizeof(text) - 1] = 0;
		text_property(&dict, "TooLongIgnored", text);
	}
	DBusMessageIter pair, value, fields, icons;
	property_open(&dict, "ToolTip", "(sa(iiay)ss)", &pair, &value);
	assert(dbus_message_iter_open_container(&value, DBUS_TYPE_STRUCT,
	    NULL, &fields));
	const char *empty = "", *title = "Tooltip title";
	const char *body = "Plain <b>literal</b> text\nsecond line";
	assert(dbus_message_iter_append_basic(&fields, DBUS_TYPE_STRING,
	    &empty));
	assert(dbus_message_iter_open_container(&fields, DBUS_TYPE_ARRAY,
	    "(iiay)", &icons));
	assert(dbus_message_iter_close_container(&fields, &icons));
	assert(dbus_message_iter_append_basic(&fields, DBUS_TYPE_STRING,
	    &title));
	assert(dbus_message_iter_append_basic(&fields, DBUS_TYPE_STRING,
	    &body));
	assert(dbus_message_iter_close_container(&value, &fields));
	property_close(&dict, &pair, &value);
	assert(dbus_message_iter_close_container(&root, &dict));
	return message;
}

static void properties_policy(void)
{
	tray_sni_item absent;
	memset(&absent, 0xa5, sizeof(absent));
	assert(!tray_bus_parse_properties(NULL, &absent));
	assert(!absent.id && !absent.icon.pixels);
	for (unsigned int variation = 0; variation <= 9; variation++)
	{
		DBusMessage *message = properties_message(variation);
		tray_sni_item item;
		memset(&item, 0xa5, sizeof(item));
		bool accepted = variation == 0 || variation == 3 || variation
		    == 8;
		assert(tray_bus_parse_properties(message, &item) == accepted);
		if (accepted)
		{
			assert(item.icon.width == 1 && item.icon.height == 1);
			assert(item.icon.pixels[0] == 0x80203040U);
			assert(item.status == (variation == 3 ?
			    TRAY_SNI_PASSIVE :
			    TRAY_SNI_ATTENTION));
			assert(item.window_id == (variation == 8 ? 123U :
			    UINT32_MAX));
			assert(item.item_is_menu && !strcmp(item.menu_path,
			    "/Menu"));
			assert(!strcmp(item.tooltip_title, "Tooltip title"));
			assert(strstr(item.tooltip_body, "<b>literal</b>"));
			tray_sni_item_clear(&item);
		}
		else
		{
			tray_sni_item cleared = {0};
			assert(!memcmp(&item, &cleared, sizeof(item)));
		}
		dbus_message_unref(message);
	}
}

static void registry_name_policy(void)
{
	const char *invalid[] = {":.1/StatusNotifierItem",
	    ":1./StatusNotifierItem", ":1..2/StatusNotifierItem",
	    ":/StatusNotifierItem", ":1/StatusNotifierItem", "org..example",
	    ":1.2//Invalid", "org.example/Bad-Path"};
	char service[TRAY_SNI_OWNER_MAX + 1], path[TRAY_SNI_PATH_MAX + 1];
	for (unsigned int i = 0; i < sizeof(invalid) / sizeof(invalid[0]); i++)
	{
		memset(service, 'x', sizeof(service));
		memset(path, 'x', sizeof(path));
		assert(!tray_bus_parse_registry_entry(invalid[i],
		    service, path));
		assert(!service[0] && !path[0]);
	}
	assert(tray_bus_parse_registry_entry(":1.2/NamedItem", service, path));
	assert(!strcmp(service, ":1.2") && !strcmp(path, "/NamedItem"));
	assert(tray_bus_parse_registry_entry("org.example.Item",
	    service, path));
	assert(!strcmp(service, "org.example.Item"));
	assert(!strcmp(path, "/StatusNotifierItem"));
	assert(!tray_bus_parse_registry_entry(NULL, service, path));
	assert(!service[0] && !path[0]);
}

static void assert_activate_dialect(const tray_sni_item *item,
	const char *interface)
{
	DBusMessage *action = tray_bus_action_message(item, TRAY_BUS_ACTIVATE,
	    -5, 9, 0, false);
	assert(action && !dbus_message_get_auto_start(action));
	assert(!strcmp(dbus_message_get_interface(action), interface));
	assert(!strcmp(dbus_message_get_member(action), "Activate"));
	assert(!strcmp(dbus_message_get_destination(action), item->owner));
	assert(!strcmp(dbus_message_get_path(action), item->path));
	dbus_int32_t x, y;
	assert(dbus_message_get_args(action, NULL, DBUS_TYPE_INT32, &x,
	    DBUS_TYPE_INT32, &y, DBUS_TYPE_INVALID));
	assert(x == -5 && y == 9);
	dbus_message_unref(action);
}

static void context_fallback_policy(void)
{
	tray_sni_item item = {
	    .id = 1, .generation = 2, .status = TRAY_SNI_ACTIVE,
	    .ready = true, .owner = ":1.42", .path = "/Fallback"};
	const char *interfaces[] = {"org.kde.StatusNotifierItem",
	    "org.freedesktop.StatusNotifierItem"};
	const char *menus[] = {"", "/"};
	/* A missing exported menu still addresses ContextMenu to the item,
	 * in its committed dialect and with the actual root coordinates. */
	for (unsigned int d = 0; d < 2; d++)
		for (unsigned int m = 0; m < 2; m++)
		{
			strcpy(item.interface, interfaces[d]);
			strcpy(item.menu_path, menus[m]);
			DBusMessage *message = tray_bus_action_message(&item,
			    TRAY_BUS_CONTEXT, -123, 456, 0, false);
			assert(message &&
			    !dbus_message_get_auto_start(message));
			assert(!strcmp(dbus_message_get_member(message),
			    "ContextMenu"));
			assert(!strcmp(dbus_message_get_interface(message),
			    interfaces[d]));
			assert(!strcmp(dbus_message_get_destination(message),
			    item.owner));
			assert(!strcmp(dbus_message_get_path(message),
			    item.path));
			dbus_int32_t x = 0, y = 0;
			assert(dbus_message_get_args(message, NULL,
			    DBUS_TYPE_INT32, &x, DBUS_TYPE_INT32, &y,
			    DBUS_TYPE_INVALID));
			assert(x == -123 && y == 456);
			dbus_message_unref(message);
		}
}

static void committed_interface_regression(void)
{
	tray_sni_model *model = calloc(1, sizeof(*model));
	assert(model);
	tray_sni_model_init(model);
	tray_sni_item *item = tray_sni_admit(model, ":1.42", "/Ready",
	    "org.kde.StatusNotifierItem");
	assert(item);
	DBusMessage *valid = properties_message(0);
	DBusMessage *invalid = properties_message(1);
	uint64_t id = item->id;
	assert(tray_bus_apply_properties(model, id, item->generation, 0,
	    valid, TRAY_SNI_ICON_BYTES));
	uint64_t generation = item->generation;
	uint32_t *pixels = item->icon.pixels;
	/* Malformed and over-budget refreshes keep one complete KDE snapshot,
	 * including the dialect used by the production Activate builder. */
	assert(!tray_bus_apply_properties(model, id, generation, 0, invalid,
	    TRAY_SNI_ICON_BYTES));
	assert(!tray_bus_apply_properties(model, id, generation, 0, valid, 0));
	assert(!tray_bus_apply_properties(model, id, generation, 1, valid,
	    TRAY_SNI_ICON_BYTES));
	assert(item->ready && item->generation == generation);
	assert(item->icon.pixels == pixels);
	assert(!strcmp(item->interface, "org.kde.StatusNotifierItem"));
	assert(!strcmp(tray_sni_probe_interface(item, 1), item->interface));
	assert(!strcmp(tray_sni_probe_interface(item, 3), item->interface));
	assert_activate_dialect(item, "org.kde.StatusNotifierItem");

	item = tray_sni_admit(model, ":1.42", "/Discovery",
	    "org.kde.StatusNotifierItem");
	assert(item && !item->ready);
	id = item->id;
	generation = item->generation;
	assert(!tray_bus_apply_properties(model, id, generation, 0, invalid,
	    TRAY_SNI_ICON_BYTES));
	assert(!strcmp(tray_sni_probe_interface(item, 1),
	    "org.freedesktop.StatusNotifierItem"));
	assert(!strcmp(item->interface, "org.kde.StatusNotifierItem"));
	assert(item->generation == generation && !item->ready);
	assert(tray_bus_apply_properties(model, id, generation, 1, valid,
	    TRAY_SNI_ICON_BYTES));
	assert(item->ready && item->generation != generation);
	assert(!strcmp(item->interface, "org.freedesktop.StatusNotifierItem"));
	assert_activate_dialect(item, "org.freedesktop.StatusNotifierItem");
	dbus_message_unref(valid);
	dbus_message_unref(invalid);
	tray_sni_model_clear(model);
	free(model);
}

static void menu_row(DBusMessageIter *container, int id,
	unsigned int depth, unsigned int maximum, unsigned int variation)
{
	DBusMessageIter row, properties, children;
	dbus_int32_t identity = variation == 2 && depth == 2 ? 1 : id;
	assert(dbus_message_iter_open_container(container, DBUS_TYPE_STRUCT,
	    NULL, &row));
	assert(dbus_message_iter_append_basic(&row, DBUS_TYPE_INT32,
	    &identity));
	assert(dbus_message_iter_open_container(&row, DBUS_TYPE_ARRAY,
	    "{sv}", &properties));
	text_property(&properties, "label", depth ?
	    "Application _action \"$()\"" :
	    "Application menu");
	text_property(&properties, "icon-name", "example-icon");
	dbus_bool_t enabled = variation != 3 || depth != 1;
	scalar_property(&properties, "enabled", "b", DBUS_TYPE_BOOLEAN,
	    &enabled);
	text_property(&properties, "type", variation == 4 && depth == 1 ?
	    "separator" : "standard");
	text_property(&properties, "children-display", depth < maximum ?
	    "submenu" : "");
	if (depth == maximum)
	{
		text_property(&properties, "toggle-type", variation == 5 ?
		    "unsupported" : "checkmark");
		dbus_int32_t checked = 1;
		scalar_property(&properties, "toggle-state", "i",
		    DBUS_TYPE_INT32,
		    &checked);
	}
	assert(dbus_message_iter_close_container(&row, &properties));
	assert(dbus_message_iter_open_container(&row, DBUS_TYPE_ARRAY,
	    "v", &children));
	if (depth < maximum)
	{
		DBusMessageIter variant;
		assert(dbus_message_iter_open_container(&children,
		    DBUS_TYPE_VARIANT,
		    "(ia{sv}av)", &variant));
		menu_row(&variant, id + 1, depth + 1, maximum, variation);
		assert(dbus_message_iter_close_container(&children, &variant));
	}
	assert(dbus_message_iter_close_container(&row, &children));
	assert(dbus_message_iter_close_container(container, &row));
}

static DBusMessage *menu_message(unsigned int maximum, unsigned int variation)
{
	DBusMessage *message = reply();
	DBusMessageIter root;
	dbus_uint32_t revision = 19;
	dbus_message_iter_init_append(message, &root);
	assert(dbus_message_iter_append_basic(&root, DBUS_TYPE_UINT32,
	    &revision));
	menu_row(&root, 0, 0, maximum, variation);
	return message;
}

static void menu_policy(void)
{
	for (unsigned int variation = 0; variation <= 5; variation++)
	{
		DBusMessage *message = menu_message(variation == 1 ? 5 : 2,
		    variation);
		tray_sni_menu *menu = calloc(1, sizeof(*menu));
		assert(menu);
		bool accepted = variation == 0 || variation == 3 || variation
		    == 4;
		assert(tray_bus_parse_menu(message, menu) == accepted);
		if (accepted)
		{
			assert(menu->revision == 19 && menu->count == 3);
			assert(menu->nodes[2].parent == 1);
			assert(menu->nodes[2].toggle_type == 1);
			assert(menu->nodes[2].toggle_state == 1);
			assert(strstr(menu->nodes[2].label, "\"$()\""));
			if (variation == 3)
				assert(!menu->nodes[1].enabled);
			if (variation == 4)
				assert(menu->nodes[1].separator);
		}
		else
			assert(!menu->count && !menu->generation &&
			    !menu->ready);
		free(menu);
		dbus_message_unref(message);
	}
}

static void unavailable_and_address(void)
{
	assert(tray_bus_available());
	assert(tray_bus_address_valid("unix:path=/tmp/private-bus"));
	assert(tray_bus_address_valid("unix:abstract=private-bus"));
	assert(!tray_bus_address_valid("autolaunch:"));
	assert(!tray_bus_address_valid("tcp:host=localhost,port=1"));
	assert(!tray_bus_address_valid("unix:path=/tmp/a;unix:path=/tmp/b"));
	assert(!tray_bus_address_valid("unix:path=relative"));
	tray_bus *bus = tray_bus_create();
	assert(bus && !tray_bus_connected(bus) && tray_bus_fd(bus) == -1);
	tray_bus_set_capacity(bus, 0);
	assert(tray_bus_model(bus)->capacity == 0);
	tray_bus_set_capacity(bus, UINT_MAX);
	assert(tray_bus_model(bus)->capacity == TRAY_SNI_MAX);
	tray_bus_set_icon_budget(bus, 0);
	assert(!tray_bus_action(bus, 1, 1, TRAY_BUS_ACTIVATE,
	    0, 0, 0, false, 0));
	assert(tray_bus_menu_open(bus, 1, 1, 0, 0, 0, 0) ==
	    TRAY_BUS_MENU_FAILED);
	assert(!tray_bus_model(bus)->menu.open && tray_bus_take_dirty(bus));
	assert(tray_bus_menu_open(NULL, 1, 1, 0, 0, 0, 0) ==
	    TRAY_BUS_MENU_FAILED);
	assert(!tray_bus_menu_click(bus, 1, 1, 0, 0));
	assert(!tray_bus_open(bus, "autolaunch:", 0));
	tray_bus_close(bus);
	tray_bus_destroy(bus);
}

int main(void)
{
	properties_policy();
	registry_name_policy();
	committed_interface_regression();
	context_fallback_policy();
	menu_policy();
	unavailable_and_address();
	return 0;
}
