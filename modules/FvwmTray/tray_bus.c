/* This program is free software; you can redistribute it and/or modify it
 * under the GNU General Public License, version 2 or any later version. */
#include "config.h"
#include "tray_bus.h"
#include "libs/log.h"
#include "libs/module_startup.h"
#include "libs/safemalloc.h"
#include <dbus/dbus.h>
#include <limits.h>
#include <math.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define BUS_PENDING_MAX 8U
#define BUS_HOST_MAX 16U
#define BUS_OUTPUT_MAX (64U * 1024U)
#define BUS_PROPERTY_MAX 64U
#define BUS_TURN_MAX 32U
#define BUS_PARSE_NODES 4096U
#define BUS_MENU_INTERFACE "com.canonical.dbusmenu"
#define BUS_WATCHER_PATH "/StatusNotifierWatcher"

static const char *watcher_names[2] = {
    "org.kde.StatusNotifierWatcher",
    "org.freedesktop.StatusNotifierWatcher"};
static const char *item_interfaces[2] = {
    "org.kde.StatusNotifierItem", "org.freedesktop.StatusNotifierItem"};
static const char *matches[] = {
    "type='signal',sender='org.freedesktop.DBus',"
    "interface='org.freedesktop.DBus',member='NameOwnerChanged'",
    "type='signal',interface='org.kde.StatusNotifierWatcher'",
    "type='signal',interface='org.freedesktop.StatusNotifierWatcher'",
    "type='signal',interface='org.kde.StatusNotifierItem'",
    "type='signal',interface='org.freedesktop.StatusNotifierItem'",
    "type='signal',interface='org.freedesktop.DBus.Properties'",
    "type='signal',interface='com.canonical.dbusmenu'"};

typedef enum
{
	PENDING_NONE, PENDING_MATCH, PENDING_NAME, PENDING_WATCHER_OWNER,
	PENDING_HOST, PENDING_REGISTRY, PENDING_REGISTER_ITEM,
	PENDING_REGISTER_HOST, PENDING_ITEM_OWNER, PENDING_PROPERTIES,
	PENDING_ABOUT, PENDING_LAYOUT, PENDING_ACTION
} pending_kind;

typedef struct
{
	pending_kind kind;
	dbus_uint32_t serial;
	double expires;
	unsigned int dialect, registry_index;
	bool registry_seed;
	uint64_t item_id, item_generation, menu_generation;
	char expected[TRAY_SNI_OWNER_MAX + 1];
	char path[TRAY_SNI_PATH_MAX + 1];
	char service[TRAY_SNI_OWNER_MAX + 1];
	/* Only a bounded registration may retain the original incoming call. */
	DBusMessage *registration;
} bus_pending;

typedef struct
{
	uint64_t id;
	bool refresh, pending;
	unsigned int tried, references;
} item_meta;

typedef struct
{
	char owner[TRAY_SNI_OWNER_MAX + 1];
	char service[TRAY_SNI_OWNER_MAX + 1];
	char path[TRAY_SNI_PATH_MAX + 1];
	bool resolved, pending;
} registry_record;

typedef struct
{
	char owner[TRAY_SNI_OWNER_MAX + 1];
	bool owned, requested, host_pending, host_registered, host_attempted;
	bool registry_pending, registry_loaded, registry_refresh,
	    registry_seeding;
	unsigned int registry_count;
	registry_record registry[TRAY_SNI_MAX];
} watcher_state;

typedef struct
{
	char owner[TRAY_SNI_OWNER_MAX + 1];
	unsigned int dialects;
	char service[2][TRAY_SNI_OWNER_MAX + 1];
} host_state;

struct tray_bus
{
	DBusConnection *connection;
	tray_sni_model model;
	bus_pending pending[BUS_PENDING_MAX];
	item_meta meta[TRAY_SNI_MAX];
	watcher_state watchers[2];
	host_state hosts[BUS_HOST_MAX];
	unsigned int match_next;
	size_t icon_budget;
	bool dirty, failed, buffered, menu_refresh;
	int menu_x, menu_y;
};

static void refused(tray_bus *bus, const char *reason)
{
	static unsigned int diagnostics;
	if (bus && diagnostics < 8)
	{
		diagnostics++;
		fvwm_debug(__func__, "FvwmTray: %s", reason);
	}
}

static bool finite_time(double now)
{
	return isfinite(now) && now >= 0;
}

static bool wire_valid(DBusMessage *message)
{
	char *bytes = NULL;
	int length = 0;
	bool valid = dbus_message_marshal(message, &bytes, &length) &&
	    length > 0 && length <= TRAY_SNI_WIRE_MAX &&
	    !dbus_message_contains_unix_fds(message);
	dbus_free(bytes);
	return valid;
}

bool tray_bus_address_valid(const char *address)
{
	DBusAddressEntry **entries = NULL;
	DBusError error;
	const char *field, *comma, *path, *abstract;
	int count = 0;
	bool valid = false;

	if (!address || strnlen(address, 2049) > 2048 ||
	    strncmp(address, "unix:", 5) || strchr(address, ';'))
		return false;
	field = address + 5;
	if (strncmp(field, "path=", 5) && strncmp(field, "abstract=", 9))
		return false;
	comma = strchr(field, ',');
	if (comma && (strncmp(comma, ",guid=", 6) || strchr(comma + 1, ',')))
		return false;
	dbus_error_init(&error);
	if (dbus_parse_address(address, &entries, &count, &error) && count == 1
	    &&
	    !strcmp(dbus_address_entry_get_method(entries[0]), "unix"))
	{
		path = dbus_address_entry_get_value(entries[0], "path");
		abstract = dbus_address_entry_get_value(entries[0], "abstract");
		valid = (path && !abstract && path[0] == '/' &&
		    tray_sni_text(path, 1024, false)) ||
		    (abstract && !path && abstract[0] &&
		    tray_sni_text(abstract, 1024, false));
	}
	if (entries)
		dbus_address_entries_free(entries);
	dbus_error_free(&error);
	return valid;
}

static bool take_string(DBusMessageIter *iter, int type, char *output,
	size_t maximum, bool multiline)
{
	const char *text;
	if (dbus_message_iter_get_arg_type(iter) != type)
		return false;
	dbus_message_iter_get_basic(iter, &text);
	if (!tray_sni_text(text, maximum, multiline))
		return false;
	memcpy(output, text, strlen(text) + 1);
	return true;
}

/* Ignored extensions still have finite nesting/fanout. Fixed primitive arrays
 * count as one bounded node, not one dispatch iteration per payload byte. */
static bool bounded_value(DBusMessageIter *iter, unsigned int depth,
	unsigned int *remaining)
{
	int type = dbus_message_iter_get_arg_type(iter);
	if (!*remaining || depth > 8 || type == DBUS_TYPE_UNIX_FD ||
	    type == DBUS_TYPE_INVALID)
		return false;
	(*remaining)--;
	if (type == DBUS_TYPE_STRING || type == DBUS_TYPE_OBJECT_PATH ||
	    type == DBUS_TYPE_SIGNATURE)
	{
		const char *text;
		dbus_message_iter_get_basic(iter, &text);
		return tray_sni_text(text, TRAY_SNI_TEXT_MAX, true);
	}
	if (type == DBUS_TYPE_ARRAY &&
	    dbus_type_is_fixed(dbus_message_iter_get_element_type(iter)))
	{
		DBusMessageIter array;
		const void *data;
		int length = 0;
		dbus_message_iter_recurse(iter, &array);
		dbus_message_iter_get_fixed_array(&array, &data, &length);
		return length >= 0 && length <= TRAY_SNI_WIRE_MAX;
	}
	if (dbus_type_is_container(type))
	{
		DBusMessageIter nested;
		dbus_message_iter_recurse(iter, &nested);
		while (dbus_message_iter_get_arg_type(&nested) !=
		    DBUS_TYPE_INVALID)
		{
			if (!bounded_value(&nested, depth + 1, remaining))
				return false;
			dbus_message_iter_next(&nested);
		}
	}
	return true;
}

static bool pixmap_array(DBusMessageIter *iter, tray_sni_pixmap *pixmap,
	size_t *budget)
{
	DBusMessageIter array;
	unsigned int variants = 0, selected = 0;
	const unsigned char *selected_data = NULL;
	int selected_width = 0, selected_height = 0;
	if (dbus_message_iter_get_arg_type(iter) != DBUS_TYPE_ARRAY ||
	    dbus_message_iter_get_element_type(iter) != DBUS_TYPE_STRUCT)
		return false;
	dbus_message_iter_recurse(iter, &array);
	while (dbus_message_iter_get_arg_type(&array) != DBUS_TYPE_INVALID)
	{
		DBusMessageIter row, bytes;
		dbus_int32_t width, height;
		const unsigned char *data = NULL;
		int length = 0;
		if (++variants > 8 ||
		    dbus_message_iter_get_arg_type(&array) != DBUS_TYPE_STRUCT)
			return false;
		dbus_message_iter_recurse(&array, &row);
		if (dbus_message_iter_get_arg_type(&row) != DBUS_TYPE_INT32)
			return false;
		dbus_message_iter_get_basic(&row, &width);
		dbus_message_iter_next(&row);
		if (dbus_message_iter_get_arg_type(&row) != DBUS_TYPE_INT32)
			return false;
		dbus_message_iter_get_basic(&row, &height);
		dbus_message_iter_next(&row);
		if (dbus_message_iter_get_arg_type(&row) != DBUS_TYPE_ARRAY ||
		    dbus_message_iter_get_element_type(&row) != DBUS_TYPE_BYTE)
			return false;
		dbus_message_iter_recurse(&row, &bytes);
		dbus_message_iter_get_fixed_array(&bytes, &data, &length);
		if (width <= 0 || height <= 0 || length < 0 ||
		    !tray_sni_pixmap_valid((unsigned int)width,
		    (unsigned int)height, (size_t)length) ||
		    dbus_message_iter_next(&row))
			return false;
		/* Keep one source image per role. Select the largest bounded
		 * source;
		 * native rendering chooses its scale without changing foreign
		  * plugs. */
		if ((unsigned int)length > selected)
		{
			selected = (unsigned int)length;
			selected_data = data;
			selected_width = width;
			selected_height = height;
		}
		dbus_message_iter_next(&array);
	}
	if (!selected)
		return true;
	if (selected > *budget)
		return false;
	uint32_t *pixels = fxmalloc(selected);
	for (unsigned int i = 0; i < selected / 4; i++)
	{
		const unsigned char *p = selected_data + i * 4;
		pixels[i] = ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) |
		    ((uint32_t)p[2] << 8) | p[3];
	}
	*budget -= selected;
	pixmap->pixels = pixels;
	pixmap->width = (unsigned int)selected_width;
	pixmap->height = (unsigned int)selected_height;
	return true;
}

static bool tooltip(DBusMessageIter *iter, tray_sni_item *item,
	size_t *budget)
{
	DBusMessageIter fields;
	char ignored_name[TRAY_SNI_TEXT_MAX + 1];
	tray_sni_pixmap ignored = {0};
	bool valid = false;
	if (dbus_message_iter_get_arg_type(iter) != DBUS_TYPE_STRUCT)
		return false;
	dbus_message_iter_recurse(iter, &fields);
	if (!take_string(&fields, DBUS_TYPE_STRING, ignored_name,
	    TRAY_SNI_TEXT_MAX, false))
		return false;
	dbus_message_iter_next(&fields);
	if (!pixmap_array(&fields, &ignored, budget))
		return false;
	dbus_message_iter_next(&fields);
	if (!take_string(&fields, DBUS_TYPE_STRING, item->tooltip_title,
	    TRAY_SNI_TEXT_MAX, false))
		goto out;
	dbus_message_iter_next(&fields);
	if (!take_string(&fields, DBUS_TYPE_STRING, item->tooltip_body,
	    TRAY_SNI_TEXT_MAX, true) || dbus_message_iter_next(&fields))
		goto out;
	valid = true;
out:
	/* Tooltip images are parsed/bounded but this plain-text tooltip does
	 * not
	 * retain or advertise an additional image surface. */
	if (ignored.pixels)
		*budget += (size_t)ignored.width * ignored.height * 4;
	tray_sni_pixmap_clear(&ignored);
	return valid;
}

static bool properties(DBusMessage *message, tray_sni_item *item,
	size_t budget)
{
	DBusMessageIter root, entries;
	tray_sni_item result = {0};
	unsigned int count = 0, remaining = BUS_PARSE_NODES;
	unsigned int required = 0;
	char seen[BUS_PROPERTY_MAX][64];
	if (!item)
		return false;
	memset(item, 0, sizeof(*item));
	if (!wire_valid(message) ||
	    !dbus_message_has_signature(message, "a{sv}"))
		return false;
	dbus_message_iter_init(message, &root);
	dbus_message_iter_recurse(&root, &entries);
	while (dbus_message_iter_get_arg_type(&entries) != DBUS_TYPE_INVALID)
	{
		DBusMessageIter pair, value;
		char key[64], text[TRAY_SNI_TEXT_MAX + 1];
		bool valid = true;
		if (count >= BUS_PROPERTY_MAX ||
		    dbus_message_iter_get_arg_type(&entries) !=
		        DBUS_TYPE_DICT_ENTRY)
			goto failed;
		dbus_message_iter_recurse(&entries, &pair);
		if (!take_string(&pair, DBUS_TYPE_STRING, key, sizeof(key) - 1,
		    false))
			goto failed;
		for (unsigned int i = 0; i < count; i++)
			if (!strcmp(seen[i], key))
				goto failed;
		strcpy(seen[count++], key);
		dbus_message_iter_next(&pair);
		if (dbus_message_iter_get_arg_type(&pair) != DBUS_TYPE_VARIANT)
			goto failed;
		dbus_message_iter_recurse(&pair, &value);
		if (!bounded_value(&value, 0, &remaining))
			goto failed;
		if (!strcmp(key, "Category"))
		{
			valid = take_string(&value, DBUS_TYPE_STRING,
			    result.category, sizeof(result.category) - 1,
			        false);
			required |= 1;
		}
		else if (!strcmp(key, "Id"))
		{
			valid = take_string(&value, DBUS_TYPE_STRING,
			    result.identity, sizeof(result.identity) - 1,
			        false);
			required |= 2;
		}
		else if (!strcmp(key, "Title"))
			valid = take_string(&value, DBUS_TYPE_STRING,
			    result.title,
			    TRAY_SNI_TEXT_MAX, false);
		else if (!strcmp(key, "Status"))
		{
			valid = take_string(&value, DBUS_TYPE_STRING, text,
			    sizeof(text) - 1, false);
			if (valid && !strcmp(text, "Passive"))
				result.status = TRAY_SNI_PASSIVE;
			else if (valid && !strcmp(text, "Active"))
				result.status = TRAY_SNI_ACTIVE;
			else if (valid && !strcmp(text, "NeedsAttention"))
				result.status = TRAY_SNI_ATTENTION;
			else
				valid = false;
			required |= 4;
		}
		else if (!strcmp(key, "IconName"))
			valid = take_string(&value, DBUS_TYPE_STRING,
			    result.icon_name, TRAY_SNI_TEXT_MAX, false);
		else if (!strcmp(key, "AttentionIconName"))
			valid = take_string(&value, DBUS_TYPE_STRING,
			    result.attention_name, TRAY_SNI_TEXT_MAX, false);
		else if (!strcmp(key, "OverlayIconName"))
			valid = take_string(&value, DBUS_TYPE_STRING,
			    result.overlay_name, TRAY_SNI_TEXT_MAX, false);
		else if (!strcmp(key, "IconThemePath"))
		{
			valid = take_string(&value, DBUS_TYPE_STRING,
			    result.icon_theme_path, TRAY_SNI_PATH_MAX, false);
			if (valid && result.icon_theme_path[0] &&
			    result.icon_theme_path[0] != '/')
				valid = false;
		}
		else if (!strcmp(key, "Menu"))
			valid = take_string(&value, DBUS_TYPE_OBJECT_PATH,
			    result.menu_path, TRAY_SNI_PATH_MAX, false) &&
			    tray_sni_path(result.menu_path);
		else if (!strcmp(key, "ItemIsMenu"))
		{
			dbus_bool_t boolean;
			valid = dbus_message_iter_get_arg_type(&value) ==
			    DBUS_TYPE_BOOLEAN;
			if (valid)
			{
				dbus_message_iter_get_basic(&value, &boolean);
				result.item_is_menu = boolean != FALSE;
			}
		}
		else if (!strcmp(key, "WindowId"))
		{
			int type = dbus_message_iter_get_arg_type(&value);
			valid = type == DBUS_TYPE_UINT32 || type ==
			    DBUS_TYPE_INT32;
			if (valid)
				dbus_message_iter_get_basic(&value,
				    &result.window_id);
		}
		else if (!strcmp(key, "IconPixmap"))
			valid = pixmap_array(&value, &result.icon, &budget);
		else if (!strcmp(key, "AttentionIconPixmap"))
			valid = pixmap_array(&value, &result.attention,
			    &budget);
		else if (!strcmp(key, "OverlayIconPixmap"))
			valid = pixmap_array(&value, &result.overlay, &budget);
		else if (!strcmp(key, "ToolTip"))
			valid = tooltip(&value, &result, &budget);
		if (!valid || dbus_message_iter_next(&pair))
			goto failed;
		dbus_message_iter_next(&entries);
	}
	if (required != 7 || !result.identity[0] || !result.category[0])
		goto failed;
	*item = result;
	return true;
failed:
	tray_sni_item_clear(&result);
	return false;
}

bool tray_bus_parse_properties(DBusMessage *message, tray_sni_item *item)
{
	if (!item)
		return false;
	memset(item, 0, sizeof(*item));
	return message && properties(message, item, TRAY_SNI_ICON_BYTES);
}

bool tray_bus_apply_properties(tray_sni_model *model, uint64_t id,
	uint64_t generation, unsigned int dialect, DBusMessage *message,
	size_t icon_budget)
{
	const tray_sni_item *item = model ?
	    tray_sni_lookup(model, id, generation) : NULL;
	if (!item || dialect > 1 || !message ||
	    dbus_message_get_type(message) != DBUS_MESSAGE_TYPE_METHOD_RETURN ||
	    (item->ready && strcmp(item->interface, item_interfaces[dialect])))
		return false;
	if (icon_budget > TRAY_SNI_ICON_BYTES)
		icon_budget = TRAY_SNI_ICON_BYTES;
	tray_sni_item replacement;
	if (!properties(message, &replacement,
	    model->icon_bytes < icon_budget ?
	    icon_budget - model->icon_bytes : 0))
		return false;
	strcpy(replacement.owner, item->owner);
	strcpy(replacement.path, item->path);
	strcpy(replacement.interface, item_interfaces[dialect]);
	bool applied = tray_sni_replace(model, id, &replacement);
	tray_sni_item_clear(&replacement);
	return applied;
}

static bool menu_property(DBusMessageIter *value, const char *key,
	tray_sni_menu_node *node)
{
	char text[64];
	if (!strcmp(key, "label"))
		return take_string(value, DBUS_TYPE_STRING, node->label,
		    sizeof(node->label) - 1, false);
	if (!strcmp(key, "icon-name"))
		return take_string(value, DBUS_TYPE_STRING, node->icon_name,
		    sizeof(node->icon_name) - 1, false);
	if (!strcmp(key, "enabled") || !strcmp(key, "visible"))
	{
		dbus_bool_t boolean;
		if (dbus_message_iter_get_arg_type(value) != DBUS_TYPE_BOOLEAN)
			return false;
		dbus_message_iter_get_basic(value, &boolean);
		if (!strcmp(key, "enabled"))
			node->enabled = boolean != FALSE;
		else
			node->visible = boolean != FALSE;
		return true;
	}
	if (!strcmp(key, "toggle-state"))
	{
		dbus_int32_t state;
		if (dbus_message_iter_get_arg_type(value) != DBUS_TYPE_INT32)
			return false;
		dbus_message_iter_get_basic(value, &state);
		if (state < -1 || state > 1)
			return false;
		node->toggle_state = state;
		return true;
	}
	if (!strcmp(key, "type") || !strcmp(key, "toggle-type") ||
	    !strcmp(key, "children-display"))
	{
		if (!take_string(value, DBUS_TYPE_STRING, text, sizeof(text) -
		    1,
		    false))
			return false;
		if (!strcmp(key, "type"))
		{
			if (strcmp(text, "standard") && strcmp(text,
			    "separator"))
				return false;
			node->separator = !strcmp(text, "separator");
		}
		else if (!strcmp(key, "toggle-type"))
		{
			if (!text[0])
				node->toggle_type = 0;
			else if (!strcmp(text, "checkmark"))
				node->toggle_type = 1;
			else if (!strcmp(text, "radio"))
				node->toggle_type = 2;
			else
				return false;
		}
		else
		{
			if (text[0] && strcmp(text, "submenu"))
				return false;
			node->submenu = text[0] != '\0';
		}
	}
	return true;
}

static bool menu_node(DBusMessageIter *iter, tray_sni_menu *menu,
	int32_t parent, unsigned int depth, unsigned int *remaining)
{
	DBusMessageIter row, props, children;
	tray_sni_menu_node *node;
	unsigned int fields = 0;
	char seen[32][64];
	if (depth > TRAY_SNI_MENU_DEPTH || menu->count >= TRAY_SNI_MENU_MAX ||
	    dbus_message_iter_get_arg_type(iter) != DBUS_TYPE_STRUCT)
		return false;
	node = &menu->nodes[menu->count++];
	node->parent = parent;
	node->depth = depth;
	node->enabled = node->visible = true;
	dbus_message_iter_recurse(iter, &row);
	if (dbus_message_iter_get_arg_type(&row) != DBUS_TYPE_INT32)
		return false;
	dbus_message_iter_get_basic(&row, &node->id);
	if (node->id < 0)
		return false;
	dbus_message_iter_next(&row);
	if (dbus_message_iter_get_arg_type(&row) != DBUS_TYPE_ARRAY ||
	    dbus_message_iter_get_element_type(&row) != DBUS_TYPE_DICT_ENTRY)
		return false;
	dbus_message_iter_recurse(&row, &props);
	while (dbus_message_iter_get_arg_type(&props) != DBUS_TYPE_INVALID)
	{
		DBusMessageIter pair, value;
		char key[64];
		if (fields >= 32 ||
		    dbus_message_iter_get_arg_type(&props) !=
		        DBUS_TYPE_DICT_ENTRY)
			return false;
		dbus_message_iter_recurse(&props, &pair);
		if (!take_string(&pair, DBUS_TYPE_STRING, key, sizeof(key) - 1,
		    false))
			return false;
		for (unsigned int i = 0; i < fields; i++)
			if (!strcmp(seen[i], key))
				return false;
		strcpy(seen[fields++], key);
		dbus_message_iter_next(&pair);
		if (dbus_message_iter_get_arg_type(&pair) != DBUS_TYPE_VARIANT)
			return false;
		dbus_message_iter_recurse(&pair, &value);
		if (!bounded_value(&value, 0, remaining) ||
		    !menu_property(&value, key, node) ||
		    dbus_message_iter_next(&pair))
			return false;
		dbus_message_iter_next(&props);
	}
	dbus_message_iter_next(&row);
	if (dbus_message_iter_get_arg_type(&row) != DBUS_TYPE_ARRAY ||
	    dbus_message_iter_get_element_type(&row) != DBUS_TYPE_VARIANT)
		return false;
	dbus_message_iter_recurse(&row, &children);
	while (dbus_message_iter_get_arg_type(&children) != DBUS_TYPE_INVALID)
	{
		DBusMessageIter child;
		if (dbus_message_iter_get_arg_type(&children) !=
		    DBUS_TYPE_VARIANT)
			return false;
		dbus_message_iter_recurse(&children, &child);
		if (!menu_node(&child, menu, node->id, depth + 1, remaining) ||
		    dbus_message_iter_next(&child))
			return false;
		dbus_message_iter_next(&children);
	}
	return !dbus_message_iter_next(&row);
}

bool tray_bus_parse_menu(DBusMessage *message, tray_sni_menu *menu)
{
	DBusMessageIter root;
	tray_sni_menu parsed = {0};
	unsigned int remaining = BUS_PARSE_NODES;
	if (!menu)
		return false;
	memset(menu, 0, sizeof(*menu));
	if (!message || !wire_valid(message) ||
	    !dbus_message_has_signature(message, "u(ia{sv}av)"))
		return false;
	dbus_message_iter_init(message, &root);
	dbus_message_iter_get_basic(&root, &parsed.revision);
	dbus_message_iter_next(&root);
	if (!menu_node(&root, &parsed, -1, 0, &remaining))
		return false;
	parsed.root = parsed.nodes[0].id;
	if (!tray_sni_menu_valid(&parsed))
		return false;
	*menu = parsed;
	return true;
}

static bus_pending *pending_slot(tray_bus *bus)
{
	for (unsigned int i = 0; i < BUS_PENDING_MAX; i++)
		if (bus->pending[i].kind == PENDING_NONE)
			return &bus->pending[i];
	return NULL;
}

static void pending_clear(bus_pending *pending)
{
	if (pending->registration)
		dbus_message_unref(pending->registration);
	memset(pending, 0, sizeof(*pending));
}

static bool send_message(tray_bus *bus, DBusMessage *message,
	dbus_uint32_t *serial)
{
	bool sent = message && bus->connection &&
	    dbus_connection_get_outgoing_size(bus->connection) < BUS_OUTPUT_MAX
	        &&
	    dbus_connection_send(bus->connection, message, serial);
	if (message)
		dbus_message_unref(message);
	if (!sent || dbus_connection_get_outgoing_size(bus->connection) >
	    BUS_OUTPUT_MAX)
	{
		refused(bus, "outgoing tray queue unavailable or full");
		bus->failed = true;
		return false;
	}
	return true;
}

static bus_pending *request(tray_bus *bus, DBusMessage *message,
	pending_kind kind, const char *expected, unsigned int dialect,
	uint64_t id, uint64_t generation, double now)
{
	bus_pending *pending = pending_slot(bus);
	if (!message || !pending || !finite_time(now) || !bus->connection ||
	    bus->failed)
	{
		refused(bus, pending ? "tray request unavailable" :
		    "tray request capacity reached");
		if (message)
			dbus_message_unref(message);
		return NULL;
	}
	dbus_message_set_auto_start(message, FALSE);
	dbus_uint32_t serial = 0;
	if (!send_message(bus, message, &serial))
		return NULL;
	pending->kind = kind;
	pending->serial = serial;
	pending->expires = now + 2;
	pending->dialect = dialect;
	pending->item_id = id;
	pending->item_generation = generation;
	strcpy(pending->expected, expected);
	return pending;
}

static DBusMessage *new_call(const char *destination, const char *path,
	const char *interface, const char *member)
{
	return dbus_message_new_method_call(destination, path, interface,
	    member);
}

static DBusMessage *string_call(const char *destination, const char *path,
	const char *interface, const char *member, const char *value)
{
	DBusMessage *message = new_call(destination, path, interface, member);
	if (message && !dbus_message_append_args(message, DBUS_TYPE_STRING,
	    &value, DBUS_TYPE_INVALID))
	{
		dbus_message_unref(message);
		message = NULL;
	}
	return message;
}

static void reply_error(tray_bus *bus, DBusMessage *call, const char *name,
	const char *description)
{
	refused(bus, description);
	if (!dbus_message_get_no_reply(call))
		send_message(bus, dbus_message_new_error(call, name,
		    description), NULL);
}

static void reply_empty(tray_bus *bus, DBusMessage *call)
{
	if (!dbus_message_get_no_reply(call))
		send_message(bus, dbus_message_new_method_return(call), NULL);
}

static item_meta *metadata(tray_bus *bus, uint64_t id, bool create)
{
	item_meta *free_slot = NULL;
	for (unsigned int i = 0; i < TRAY_SNI_MAX; i++)
	{
		if (bus->meta[i].id == id)
			return &bus->meta[i];
		if (!bus->meta[i].id)
			free_slot = &bus->meta[i];
	}
	if (create && free_slot)
	{
		free_slot->id = id;
		return free_slot;
	}
	return NULL;
}

static bool item_identity(char *output, size_t size,
	const tray_sni_item *item, unsigned int dialect)
{
	int length = snprintf(output, size, "%s%s", item->owner,
	    dialect == 1 && !strcmp(item->path, "/StatusNotifierItem") ?
	    "" : item->path);
	return length > 0 && (size_t)length < size;
}

static void item_signal(tray_bus *bus, const char *member,
	const tray_sni_item *item)
{
	for (unsigned int i = 0; i < 2; i++)
		if (bus->watchers[i].owned)
		{
			char identity[TRAY_SNI_OWNER_MAX +
			    TRAY_SNI_PATH_MAX + 2];
			const char *value = identity;
			if (!item_identity(identity, sizeof(identity), item, i))
				continue;
			DBusMessage *message = dbus_message_new_signal(
			    BUS_WATCHER_PATH, watcher_names[i], member);
			if (!message || !dbus_message_append_args(message,
			    DBUS_TYPE_STRING, &value, DBUS_TYPE_INVALID))
			{
				if (message)
					dbus_message_unref(message);
				bus->failed = true;
				return;
			}
			if (!send_message(bus, message, NULL))
				return;
		}
}

static void remove_item(tray_bus *bus, uint64_t id)
{
	tray_sni_item *item = tray_sni_find(&bus->model, id);
	if (!item)
		return;
	item_signal(bus, "StatusNotifierItemUnregistered", item);
	for (unsigned int i = 0; i < BUS_PENDING_MAX; i++)
		if (bus->pending[i].item_id == id)
			pending_clear(&bus->pending[i]);
	item_meta *meta = metadata(bus, id, false);
	if (meta)
		memset(meta, 0, sizeof(*meta));
	tray_sni_remove(&bus->model, id);
	bus->dirty = true;
}

static tray_sni_item *admit_item(tray_bus *bus, const char *owner,
	const char *path, unsigned int dialect, bool external)
{
	unsigned int before = bus->model.count;
	tray_sni_item *item = tray_sni_admit(&bus->model, owner, path,
	    item_interfaces[dialect]);
	if (!item)
	{
		refused(bus, "tray item capacity or identity refused");
		return NULL;
	}
	item_meta *meta = metadata(bus, item->id, true);
	if (!meta)
	{
		remove_item(bus, item->id);
		return NULL;
	}
	meta->refresh = true;
	meta->references |= 1U << (dialect + (external ? 2 : 0));
	if (before != bus->model.count)
	{
		item_signal(bus, "StatusNotifierItemRegistered", item);
		bus->dirty = true;
	}
	return item;
}

static bool register_item(tray_bus *bus, const char *owner,
	const char *path, unsigned int dialect, const char *service)
{
	watcher_state *watcher = &bus->watchers[dialect];
	unsigned int index = watcher->registry_count;
	for (unsigned int i = 0; i < watcher->registry_count; i++)
		if (!strcmp(watcher->registry[i].service, service) &&
		    !strcmp(watcher->registry[i].path, path))
			index = i;
	if (index >= TRAY_SNI_MAX ||
	    !admit_item(bus, owner, path, dialect, false))
		return false;
	registry_record *record = &watcher->registry[index];
	memset(record, 0, sizeof(*record));
	strcpy(record->owner, owner);
	strcpy(record->service, service);
	strcpy(record->path, path);
	record->resolved = true;
	if (index == watcher->registry_count)
		watcher->registry_count++;
	return true;
}

static bool owner_request(tray_bus *bus, const char *service,
	pending_kind kind, unsigned int dialect, const char *path,
	DBusMessage *registration, double now)
{
	if (!dbus_validate_bus_name(service, NULL) ||
	    !tray_sni_text(service, TRAY_SNI_OWNER_MAX, false))
		return false;
	bus_pending *pending = request(bus, string_call(DBUS_SERVICE_DBUS,
	    DBUS_PATH_DBUS, DBUS_INTERFACE_DBUS, "GetNameOwner", service),
	    kind, DBUS_SERVICE_DBUS, dialect, 0, 0, now);
	if (!pending)
		return false;
	strcpy(pending->service, service);
	if (path)
		strcpy(pending->path, path);
	if (registration)
		pending->registration = dbus_message_ref(registration);
	return true;
}

bool tray_bus_parse_registry_entry(const char *value,
	char service[TRAY_SNI_OWNER_MAX + 1],
	char path[TRAY_SNI_PATH_MAX + 1])
{
	if (service)
		service[0] = 0;
	if (path)
		path[0] = 0;
	if (!service || !path || !tray_sni_text(value,
	    TRAY_SNI_OWNER_MAX + TRAY_SNI_PATH_MAX + 1, false))
		return false;
	const char *slash = strchr(value, '/');
	const char *object = slash ? slash : "/StatusNotifierItem";
	size_t length = slash ? (size_t)(slash - value) : strlen(value);
	char name[TRAY_SNI_OWNER_MAX + 1];
	if (!length || length > TRAY_SNI_OWNER_MAX || !tray_sni_path(object))
		return false;
	memcpy(name, value, length);
	name[length] = 0;
	/* Native DBus constructors require a valid destination. Never admit a
	 * superficially unique-looking name from a foreign watcher signal. */
	if (!dbus_validate_bus_name(name, NULL) ||
	    (name[0] == ':' && !tray_sni_owner(name)))
		return false;
	strcpy(service, name);
	strcpy(path, object);
	return true;
}

static bool registry_entry(tray_bus *bus, const char *value,
	unsigned int dialect, double now)
{
	char service[TRAY_SNI_OWNER_MAX + 1], path[TRAY_SNI_PATH_MAX + 1];
	if (!tray_bus_parse_registry_entry(value, service, path))
	{
		refused(bus, "invalid watcher registry entry");
		return false;
	}
	if (tray_sni_owner(service))
		return admit_item(bus, service, path, dialect, true) != NULL;
	return owner_request(bus, service, PENDING_ITEM_OWNER, dialect,
	    path, NULL, now);
}

static bool append_registry(tray_bus *bus, DBusMessageIter *iter,
	unsigned int dialect)
{
	DBusMessageIter array;
	if (!dbus_message_iter_open_container(iter, DBUS_TYPE_ARRAY, "s",
	    &array))
		return false;
	for (unsigned int i = 0; i < bus->model.count; i++)
	{
		char identity[TRAY_SNI_OWNER_MAX + TRAY_SNI_PATH_MAX + 2];
		const char *text = identity;
		if (!item_identity(identity, sizeof(identity),
		    &bus->model.items[i], dialect) ||
		    !dbus_message_iter_append_basic(&array, DBUS_TYPE_STRING,
		        &text))
			return false;
	}
	return dbus_message_iter_close_container(iter, &array);
}

static bool append_property(tray_bus *bus, DBusMessageIter *iter,
	const char *name, unsigned int dialect)
{
	DBusMessageIter variant;
	const char *signature = !strcmp(name, "RegisteredStatusNotifierItems") ?
	    "as" : !strcmp(name, "IsStatusNotifierHostRegistered") ? "b" :
	    !strcmp(name, "ProtocolVersion") ? "i" : NULL;
	if (!signature || !dbus_message_iter_open_container(iter,
	    DBUS_TYPE_VARIANT, signature, &variant))
		return false;
	bool okay;
	if (!strcmp(signature, "as"))
		okay = append_registry(bus, &variant, dialect);
	else if (!strcmp(signature, "b"))
	{
		dbus_bool_t present = TRUE;
		okay = dbus_message_iter_append_basic(&variant,
		    DBUS_TYPE_BOOLEAN,
		    &present);
	}
	else
	{
		dbus_int32_t version = 0;
		okay = dbus_message_iter_append_basic(&variant, DBUS_TYPE_INT32,
		    &version);
	}
	return okay && dbus_message_iter_close_container(iter, &variant);
}

static void watcher_properties(tray_bus *bus, DBusMessage *call)
{
	const char *interface, *name;
	const char *member = dbus_message_get_member(call);
	DBusMessage *reply = NULL;
	DBusMessageIter root, dict;
	bool all = !strcmp(member, "GetAll");
	if ((!all && strcmp(member, "Get")) ||
	    !dbus_message_has_signature(call, all ? "s" : "ss") ||
	    !(all ? dbus_message_get_args(call, NULL, DBUS_TYPE_STRING,
	    &interface, DBUS_TYPE_INVALID) : dbus_message_get_args(call, NULL,
	    DBUS_TYPE_STRING, &interface, DBUS_TYPE_STRING, &name,
	    DBUS_TYPE_INVALID)))
	{
		reply_error(bus, call, DBUS_ERROR_INVALID_ARGS,
		    "Expected watcher property query");
		return;
	}
	int dialect = !strcmp(interface, watcher_names[0]) ? 0 :
	    !strcmp(interface, watcher_names[1]) ? 1 : -1;
	if (dialect < 0 || !bus->watchers[dialect].owned)
	{
		reply_error(bus, call, DBUS_ERROR_UNKNOWN_INTERFACE,
		    "This connection does not own that watcher interface");
		return;
	}
	reply = dbus_message_new_method_return(call);
	if (!reply)
		goto failed;
	dbus_message_iter_init_append(reply, &root);
	if (all)
	{
		const char *names[] = {"RegisteredStatusNotifierItems",
		    "IsStatusNotifierHostRegistered", "ProtocolVersion"};
		if (!dbus_message_iter_open_container(&root, DBUS_TYPE_ARRAY,
		    "{sv}", &dict))
			goto failed;
		for (unsigned int i = 0; i < 3; i++)
		{
			DBusMessageIter pair;
			if (!dbus_message_iter_open_container(&dict,
			    DBUS_TYPE_DICT_ENTRY, NULL, &pair) ||
			    !dbus_message_iter_append_basic(&pair,
			        DBUS_TYPE_STRING,
			    &names[i]) || !append_property(bus, &pair,
			    names[i], (unsigned int)dialect) ||
			    !dbus_message_iter_close_container(&dict, &pair))
				goto failed;
		}
		if (!dbus_message_iter_close_container(&root, &dict))
			goto failed;
	}
	else if (!append_property(bus, &root, name, (unsigned int)dialect))
	{
		dbus_message_unref(reply);
		reply_error(bus, call, DBUS_ERROR_UNKNOWN_PROPERTY,
		    "Unknown watcher property");
		return;
	}
	if (dbus_message_get_no_reply(call))
		dbus_message_unref(reply);
	else
		send_message(bus, reply, NULL);
	return;
failed:
	if (reply)
		dbus_message_unref(reply);
	bus->failed = true;
}

static bool register_host(tray_bus *bus, const char *owner,
	unsigned int dialect, const char *service)
{
	host_state *host = NULL;
	for (unsigned int i = 0; i < BUS_HOST_MAX; i++)
		if (!strcmp(bus->hosts[i].owner, owner))
			host = &bus->hosts[i];
	if (!host)
		for (unsigned int i = 0; i < BUS_HOST_MAX; i++)
			if (!bus->hosts[i].owner[0])
			{
				host = &bus->hosts[i];
				break;
			}
	if (!host)
		return false;
	strcpy(host->service[dialect], service);
	if (host->dialects & (1U << dialect))
		return true;
	strcpy(host->owner, owner);
	host->dialects |= 1U << dialect;
	return send_message(bus, dbus_message_new_signal(BUS_WATCHER_PATH,
	    watcher_names[dialect], "StatusNotifierHostRegistered"), NULL);
}

static void introspect_reply(tray_bus *bus, DBusMessage *call)
{
	static const char *xml =
	    "<node>"
	    "<interface name='org.freedesktop.DBus.Introspectable'>"
	    "<method name='Introspect'><arg direction='out' type='s'/></method>"
	    "</interface>"
	    "<interface name='org.freedesktop.DBus.Properties'>"
	    "<method name='Get'><arg direction='in' type='s'/>"
	    "<arg direction='in' type='s'/><arg direction='out' type='v'/>"
	    "</method>"
	    "<method name='GetAll'><arg direction='in' type='s'/>"
	    "<arg direction='out' type='a{sv}'/></method></interface>"
	    "<interface name='org.kde.StatusNotifierWatcher'>"
	    "<method name='RegisterStatusNotifierItem'>"
	    "<arg direction='in' type='s'/></method>"
	    "<method name='RegisterStatusNotifierHost'>"
	    "<arg direction='in' type='s'/></method>"
	    "<property name='RegisteredStatusNotifierItems' type='as' "
	    "access='read'/>"
	    "<property name='IsStatusNotifierHostRegistered' type='b' "
	    "access='read'/>"
	    "<property name='ProtocolVersion' type='i' access='read'/>"
	    "<signal name='StatusNotifierItemRegistered'><arg type='s'/>"
	    "</signal>"
	    "<signal name='StatusNotifierItemUnregistered'><arg type='s'/>"
	    "</signal>"
	    "<signal name='StatusNotifierHostRegistered'/></interface>"
	    "<interface name='org.freedesktop.StatusNotifierWatcher'>"
	    "<method name='RegisterStatusNotifierItem'>"
	    "<arg direction='in' type='s'/></method>"
	    "<method name='RegisterStatusNotifierHost'>"
	    "<arg direction='in' type='s'/></method>"
	    "<property name='RegisteredStatusNotifierItems' type='as' "
	    "access='read'/>"
	    "<property name='IsStatusNotifierHostRegistered' type='b' "
	    "access='read'/>"
	    "<property name='ProtocolVersion' type='i' access='read'/>"
	    "<signal name='StatusNotifierItemRegistered'><arg type='s'/>"
	    "</signal>"
	    "<signal name='StatusNotifierItemUnregistered'><arg type='s'/>"
	    "</signal>"
	    "<signal name='StatusNotifierHostRegistered'/></interface></node>";
	DBusMessage *reply = dbus_message_new_method_return(call);
	if (!reply || !dbus_message_append_args(reply, DBUS_TYPE_STRING, &xml,
	    DBUS_TYPE_INVALID))
	{
		if (reply)
			dbus_message_unref(reply);
		bus->failed = true;
	}
	else if (dbus_message_get_no_reply(call))
		dbus_message_unref(reply);
	else
		send_message(bus, reply, NULL);
}

static void watcher_method(tray_bus *bus, DBusMessage *call, double now)
{
	const char *path = dbus_message_get_path(call);
	const char *interface = dbus_message_get_interface(call);
	const char *member = dbus_message_get_member(call);
	const char *sender = dbus_message_get_sender(call);
	const char *service;
	if (!path || strcmp(path, BUS_WATCHER_PATH) || !interface || !member ||
	    !tray_sni_owner(sender))
	{
		reply_error(bus, call, DBUS_ERROR_UNKNOWN_METHOD,
		    "Expected a watcher request");
		return;
	}
	if (!strcmp(interface, DBUS_INTERFACE_INTROSPECTABLE) &&
	    !strcmp(member, "Introspect") && dbus_message_has_signature(call,
	        ""))
	{
		introspect_reply(bus, call);
		return;
	}
	if (!strcmp(interface, DBUS_INTERFACE_PROPERTIES))
	{
		watcher_properties(bus, call);
		return;
	}
	if (!strcmp(interface, "org.freedesktop.DBus.Peer") &&
	    !strcmp(member, "Ping") && dbus_message_has_signature(call, ""))
	{
		reply_empty(bus, call);
		return;
	}
	int dialect = !strcmp(interface, watcher_names[0]) ? 0 :
	    !strcmp(interface, watcher_names[1]) ? 1 : -1;
	if (dialect < 0 || !bus->watchers[dialect].owned ||
	    (strcmp(member, "RegisterStatusNotifierItem") &&
	     strcmp(member, "RegisterStatusNotifierHost")))
	{
		reply_error(bus, call, DBUS_ERROR_UNKNOWN_METHOD,
		    "Unknown watcher method");
		return;
	}
	if (!dbus_message_has_signature(call, "s") ||
	    !dbus_message_get_args(call, NULL, DBUS_TYPE_STRING, &service,
	    DBUS_TYPE_INVALID) || !tray_sni_text(service, TRAY_SNI_PATH_MAX,
	    false) || !service[0])
	{
		reply_error(bus, call, DBUS_ERROR_INVALID_ARGS,
		    "Expected a bounded item path or owned bus name");
		return;
	}
	bool host = !strcmp(member, "RegisterStatusNotifierHost");
	if (!host && service[0] == '/' && tray_sni_path(service))
	{
		if (register_item(bus, sender, service,
		    (unsigned int)dialect, sender))
			reply_empty(bus, call);
		else
			reply_error(bus, call, DBUS_ERROR_LIMITS_EXCEEDED,
			    "Tray entry capacity reached");
		return;
	}
	if (!owner_request(bus, service, host ? PENDING_REGISTER_HOST :
	    PENDING_REGISTER_ITEM, (unsigned int)dialect,
	    "/StatusNotifierItem", call, now))
		reply_error(bus, call, DBUS_ERROR_LIMITS_EXCEEDED,
		    "Invalid name or registration work capacity reached");
}

static bool property_request(tray_bus *bus, tray_sni_item *item, double now)
{
	item_meta *meta = metadata(bus, item->id, false);
	const char *interface = meta ?
	    tray_sni_probe_interface(item, meta->tried) : NULL;
	if (!interface)
		return false;
	DBusMessage *call = string_call(item->owner, item->path,
	    DBUS_INTERFACE_PROPERTIES, "GetAll", interface);
	bus_pending *pending = request(bus, call, PENDING_PROPERTIES,
	    item->owner,
	    !strcmp(interface, item_interfaces[1]) ? 1 : 0,
	    item->id, item->generation, now);
	if (!pending)
		return false;
	meta->pending = true;
	meta->refresh = false;
	meta->tried |= 1U << pending->dialect;
	return true;
}

static bool menu_fallback(tray_bus *bus, const tray_sni_item *item,
	double now)
{
	uint64_t id = item ? item->id : 0;
	uint64_t generation = item ? item->generation : 0;
	tray_sni_menu_close(&bus->model);
	bus->dirty = true;
	return item && tray_bus_action(bus, id, generation, TRAY_BUS_CONTEXT,
	    bus->menu_x, bus->menu_y, 0, false, now);
}

static bool layout_request(tray_bus *bus, const tray_sni_item *item,
	double now)
{
	DBusMessage *message = new_call(item->owner, item->menu_path,
	    BUS_MENU_INTERFACE, "GetLayout");
	DBusMessageIter root, array;
	dbus_int32_t parent = 0, depth = TRAY_SNI_MENU_DEPTH;
	const char *properties[] = {"label", "type", "enabled", "visible",
	    "toggle-type", "toggle-state", "children-display", "icon-name"};
	if (!message)
		return false;
	dbus_message_iter_init_append(message, &root);
	if (!dbus_message_iter_append_basic(&root, DBUS_TYPE_INT32, &parent) ||
	    !dbus_message_iter_append_basic(&root, DBUS_TYPE_INT32, &depth) ||
	    !dbus_message_iter_open_container(&root, DBUS_TYPE_ARRAY, "s",
	        &array))
		goto failed;
	for (unsigned int i = 0; i < sizeof(properties) / sizeof(
	    properties[0]); i++)
		if (!dbus_message_iter_append_basic(&array, DBUS_TYPE_STRING,
		    &properties[i]))
			goto failed;
	if (!dbus_message_iter_close_container(&root, &array))
		goto failed;
	bus_pending *pending = request(bus, message, PENDING_LAYOUT,
	    item->owner,
	    0, item->id, item->generation, now);
	if (!pending)
		return false;
	pending->menu_generation = bus->model.menu.generation;
	return true;
failed:
	dbus_message_unref(message);
	return false;
}

static void drop_reference(tray_bus *bus, unsigned int dialect,
	const char *owner, const char *path)
{
	for (unsigned int i = 0; i < bus->model.count;)
	{
		tray_sni_item *item = &bus->model.items[i];
		item_meta *meta = metadata(bus, item->id, false);
		if (meta && (!owner || (!strcmp(owner, item->owner) &&
		    !strcmp(path, item->path))))
		{
			meta->references &= ~(1U << (dialect + 2));
			if (!meta->references)
			{
				remove_item(bus, item->id);
				continue;
			}
		}
		i++;
	}
}

static void watcher_changed(tray_bus *bus, unsigned int dialect,
	const char *owner)
{
	watcher_state *watcher = &bus->watchers[dialect];
	if (!strcmp(watcher->owner, owner))
		return;
	for (unsigned int i = 0; i < TRAY_SNI_MAX; i++)
		bus->meta[i].references &= ~((1U << dialect) |
		    (1U << (dialect + 2)));
	for (unsigned int i = 0; i < bus->model.count;)
	{
		item_meta *meta = metadata(bus, bus->model.items[i].id, false);
		if (!meta || !meta->references)
			remove_item(bus, bus->model.items[i].id);
		else
			i++;
	}
	memset(watcher, 0, sizeof(*watcher));
	strcpy(watcher->owner, owner);
	const char *self = dbus_bus_get_unique_name(bus->connection);
	watcher->owned = self && !strcmp(owner, self);
	watcher->requested = owner[0] != '\0';
	for (unsigned int i = 0; i < BUS_PENDING_MAX; i++)
		if (bus->pending[i].dialect == dialect &&
		    (bus->pending[i].kind == PENDING_HOST ||
		     bus->pending[i].kind == PENDING_REGISTRY ||
		     bus->pending[i].kind == PENDING_WATCHER_OWNER ||
		     bus->pending[i].kind == PENDING_ITEM_OWNER))
			pending_clear(&bus->pending[i]);
	bus->dirty = true;
}

static bool registry_reply(tray_bus *bus, DBusMessage *message,
	unsigned int dialect, double now)
{
	DBusMessageIter root, variant, array;
	unsigned int count = 0;
	if (!dbus_message_has_signature(message, "v"))
		return false;
	dbus_message_iter_init(message, &root);
	dbus_message_iter_recurse(&root, &variant);
	if (dbus_message_iter_get_arg_type(&variant) != DBUS_TYPE_ARRAY ||
	    dbus_message_iter_get_element_type(&variant) != DBUS_TYPE_STRING)
		return false;
	dbus_message_iter_recurse(&variant, &array);
	char values[TRAY_SNI_MAX][TRAY_SNI_OWNER_MAX + TRAY_SNI_PATH_MAX + 2];
	while (dbus_message_iter_get_arg_type(&array) != DBUS_TYPE_INVALID)
	{
		if (count >= TRAY_SNI_MAX || !take_string(&array,
		    DBUS_TYPE_STRING,
		    values[count], sizeof(values[0]) - 1, false))
			return false;
		char service[TRAY_SNI_OWNER_MAX + 1];
		char path[TRAY_SNI_PATH_MAX + 1];
		if (!tray_bus_parse_registry_entry(values[count],
		    service, path))
			return false;
		count++;
		dbus_message_iter_next(&array);
	}
	watcher_state *watcher = &bus->watchers[dialect];
	for (unsigned int i = 0; i < BUS_PENDING_MAX; i++)
		if (bus->pending[i].kind == PENDING_ITEM_OWNER &&
		    bus->pending[i].dialect == dialect)
			pending_clear(&bus->pending[i]);
	memset(watcher->registry, 0, sizeof(watcher->registry));
	watcher->registry_count = count;
	watcher->registry_seeding = true;
	for (unsigned int i = 0; i < count; i++)
	{
		registry_record *record = &watcher->registry[i];
		if (!tray_bus_parse_registry_entry(values[i], record->service,
		    record->path))
			return false;
		if (tray_sni_owner(record->service))
		{
			strcpy(record->owner, record->service);
			record->resolved = true;
		}
	}
	(void)now;
	return true;
}

static void registration_reply(tray_bus *bus, bus_pending *pending,
	DBusMessage *message)
{
	const char *owner;
	DBusMessage *call = pending->registration;
	const char *sender = call ? dbus_message_get_sender(call) : NULL;
	if (dbus_message_get_type(message) != DBUS_MESSAGE_TYPE_METHOD_RETURN ||
	    !dbus_message_has_signature(message, "s") ||
	    !dbus_message_get_args(message, NULL, DBUS_TYPE_STRING, &owner,
	    DBUS_TYPE_INVALID) || !tray_sni_owner(owner) ||
	    !sender || strcmp(owner, sender))
	{
		if (call)
			reply_error(bus, call, DBUS_ERROR_ACCESS_DENIED,
			    "Registration name is not owned by its sender");
		return;
	}
	bool admitted = pending->kind == PENDING_REGISTER_HOST ?
	    register_host(bus, owner, pending->dialect, pending->service) :
	    register_item(bus, owner, pending->path, pending->dialect,
	    pending->service);
	if (admitted)
		reply_empty(bus, call);
	else
		reply_error(bus, call, DBUS_ERROR_LIMITS_EXCEEDED,
		    "Tray registration capacity reached");
}

static void menu_reply(tray_bus *bus, const bus_pending *pending,
	DBusMessage *message, const tray_sni_item *item, bool success, double
	    now)
{
	tray_sni_menu *menu = &bus->model.menu;
	if (!item || !menu->open || menu->generation !=
	    pending->menu_generation)
		return;
	if (pending->kind == PENDING_ABOUT)
	{
		dbus_bool_t updated;
		if (success && dbus_message_has_signature(message, "b") &&
		    dbus_message_get_args(message, NULL, DBUS_TYPE_BOOLEAN,
		        &updated,
		    DBUS_TYPE_INVALID) && layout_request(bus, item, now))
			menu->stale = true;
		else
			menu_fallback(bus, item, now);
		return;
	}
	tray_sni_menu parsed;
	if (!success || !tray_bus_parse_menu(message, &parsed))
	{
		refused(bus, "invalid or unavailable application menu");
		menu_fallback(bus, item, now);
		return;
	}
	parsed.item_id = item->id;
	parsed.item_generation = item->generation;
	parsed.generation = menu->generation;
	parsed.parent = menu->parent;
	if (!tray_sni_menu_lookup(&parsed, parsed.parent))
		parsed.parent = parsed.root;
	parsed.open = parsed.ready = true;
	*menu = parsed;
	bus->dirty = true;
}

static void handle_reply(tray_bus *bus, DBusMessage *message, double now)
{
	dbus_uint32_t serial = dbus_message_get_reply_serial(message);
	const char *sender = dbus_message_get_sender(message);
	bus_pending *slot = NULL;
	for (unsigned int i = 0; i < BUS_PENDING_MAX; i++)
		if (bus->pending[i].kind && bus->pending[i].serial == serial)
		{
			slot = &bus->pending[i];
			break;
		}
	if (!slot)
		return;
	if (!sender || strcmp(sender, slot->expected))
	{
		refused(bus, "tray reply sender does not match request owner");
		return;
	}
	/* Retire before continuing, allowing reuse of one of eight slots.
	 * The copy owns the retained registration reference. */
	bus_pending pending = *slot;
	memset(slot, 0, sizeof(*slot));
	bool success = dbus_message_get_type(message) ==
	    DBUS_MESSAGE_TYPE_METHOD_RETURN && now < pending.expires;
	if (!success)
		refused(bus, "tray request rejected or reply expired");
	tray_sni_item *item = tray_sni_find(&bus->model, pending.item_id);
	bool current_item = item && item->generation == pending.item_generation;
	watcher_state *watcher = &bus->watchers[pending.dialect];
	if (pending.kind == PENDING_MATCH)
	{
		if (!success)
			bus->failed = true;
	}
	else if (pending.kind == PENDING_REGISTER_ITEM ||
	    pending.kind == PENDING_REGISTER_HOST)
	{
		if (success)
			registration_reply(bus, &pending, message);
		else if (pending.registration)
			reply_error(bus, pending.registration,
			    DBUS_ERROR_FAILED,
			    "Registration owner lookup failed");
	}
	else if (pending.kind == PENDING_NAME)
	{
		dbus_uint32_t result = 0;
		if (success && dbus_message_has_signature(message, "u") &&
		    dbus_message_get_args(message, NULL, DBUS_TYPE_UINT32,
		    &result, DBUS_TYPE_INVALID))
		{
			if (result == DBUS_REQUEST_NAME_REPLY_PRIMARY_OWNER ||
			    result == DBUS_REQUEST_NAME_REPLY_ALREADY_OWNER)
			{
				const char *self =
				    dbus_bus_get_unique_name(bus->connection);
				watcher_changed(bus, pending.dialect, self);
				watcher->owned = true;
				watcher->host_registered =
				    register_host(bus, self,
				    pending.dialect, self);
			}
			else if (result == DBUS_REQUEST_NAME_REPLY_EXISTS)
				(void)owner_request(bus,
				    watcher_names[pending.dialect],
				    PENDING_WATCHER_OWNER, pending.dialect,
				    NULL, NULL, now);
		}
	}
	else if (pending.kind == PENDING_WATCHER_OWNER ||
	    pending.kind == PENDING_ITEM_OWNER)
	{
		const char *owner = NULL;
		bool valid = success && dbus_message_has_signature(message,
		    "s") &&
		    dbus_message_get_args(message, NULL, DBUS_TYPE_STRING,
		        &owner,
		    DBUS_TYPE_INVALID) && tray_sni_owner(owner);
		if (pending.kind == PENDING_ITEM_OWNER && pending.registry_seed
		    &&
		    watcher->registry_seeding &&
		    pending.registry_index < watcher->registry_count)
		{
			registry_record *record =
			    &watcher->registry[pending.registry_index];
			record->pending = false;
			record->resolved = true;
			if (valid)
				strcpy(record->owner, owner);
		}
		else if (valid)
		{
			if (pending.kind == PENDING_WATCHER_OWNER)
				watcher_changed(bus, pending.dialect, owner);
			else
				(void)admit_item(bus, owner, pending.path,
				    pending.dialect, true);
		}
	}
	else if (pending.kind == PENDING_HOST)
	{
		watcher->host_pending = false;
		if (!strcmp(watcher->owner, pending.expected))
			watcher->host_registered = success;
	}
	else if (pending.kind == PENDING_REGISTRY)
	{
		watcher->registry_pending = false;
		watcher->registry_loaded = true;
		if (success && !strcmp(watcher->owner, pending.expected) &&
		    !registry_reply(bus, message, pending.dialect, now))
			refused(bus, "invalid watcher registry snapshot");
	}
	else if (pending.kind == PENDING_PROPERTIES)
	{
		item_meta *meta = metadata(bus, pending.item_id, false);
		if (meta)
			meta->pending = false;
		if (current_item)
		{
			bool applied = success && tray_bus_apply_properties(
			    &bus->model, item->id, item->generation,
			    pending.dialect, message, bus->icon_budget);
			if (applied)
				bus->dirty = true;
			else
			{
				refused(bus, "invalid or unavailable item "
				    "snapshot; keeping committed state");
				if (!item->ready && meta && !(meta->tried &
				    (1U << (1 - pending.dialect))))
					meta->refresh = true;
			}
		}
	}
	else if (pending.kind == PENDING_ABOUT ||
	    pending.kind == PENDING_LAYOUT)
		menu_reply(bus, &pending, message, current_item ? item : NULL,
		    success, now);
	pending_clear(&pending);
}

static void invalidate_menu(tray_bus *bus)
{
	if (!bus->model.menu.open)
		return;
	if (!bus->model.next_generation)
	{
		tray_bus_menu_close(bus);
		return;
	}
	bus->model.menu.generation = bus->model.next_generation++;
	bus->model.menu.stale = true;
	bus->menu_refresh = true;
	bus->dirty = true;
}

static void retire_aliases(tray_bus *bus, const char *name,
	const char *previous)
{
	for (unsigned int dialect = 0; dialect < 2; dialect++)
	{
		watcher_state *watcher = &bus->watchers[dialect];
		if (!watcher->owned)
		{
			for (unsigned int i = 0;
			    i < watcher->registry_count; i++)
			{
				registry_record *record = &watcher->registry[i];
				if (!strcmp(record->service, name) &&
				    !strcmp(record->owner, previous))
				{
					drop_reference(bus, dialect, previous,
					    record->path);
					watcher->registry_refresh = true;
				}
			}
			continue;
		}
		for (unsigned int i = 0; i < watcher->registry_count;)
		{
			registry_record *record = &watcher->registry[i];
			if ((!tray_sni_owner(name) &&
			    strcmp(record->service, name)) ||
			    strcmp(record->owner, previous))
			{
				i++;
				continue;
			}
			char path[TRAY_SNI_PATH_MAX + 1];
			strcpy(path, record->path);
			memmove(record, record + 1,
			    (--watcher->registry_count - i) * sizeof(*record));
			memset(&watcher->registry[watcher->registry_count], 0,
			    sizeof(*record));
			bool retained = false;
			for (unsigned int j = 0;
			    j < watcher->registry_count; j++)
				if (!strcmp(watcher->registry[j].owner,
				    previous) &&
				    !strcmp(watcher->registry[j].path, path))
					retained = true;
			if (retained)
				continue;
			for (unsigned int j = 0; j < bus->model.count; j++)
			{
				tray_sni_item *item = &bus->model.items[j];
				if (strcmp(item->owner, previous) ||
				    strcmp(item->path, path))
					continue;
				item_meta *meta =
				    metadata(bus, item->id, false);
				if (meta)
					meta->references &= ~(1U << dialect);
				if (!meta || !meta->references)
					remove_item(bus, item->id);
				break;
			}
		}
	}
}

static void owner_changed(tray_bus *bus, DBusMessage *message)
{
	const char *name, *old, *next;
	if (!dbus_message_has_signature(message, "sss") ||
	    !dbus_message_get_args(message, NULL, DBUS_TYPE_STRING, &name,
	    DBUS_TYPE_STRING, &old, DBUS_TYPE_STRING, &next, DBUS_TYPE_INVALID)
	        ||
	    !tray_sni_text(name, TRAY_SNI_OWNER_MAX, false) ||
	    (old[0] && !tray_sni_owner(old)) ||
	    (next[0] && !tray_sni_owner(next)))
		return;
	for (unsigned int i = 0; i < 2; i++)
		if (!strcmp(name, watcher_names[i]))
			watcher_changed(bus, i, next);
	if (old[0] && strcmp(old, next))
		retire_aliases(bus, name, old);
	for (unsigned int i = 0; i < BUS_HOST_MAX; i++)
	{
		host_state *host = &bus->hosts[i];
		for (unsigned int j = 0; j < 2; j++)
			if (!strcmp(host->service[j], name) &&
			    !strcmp(host->owner, old))
			{
				host->service[j][0] = 0;
				host->dialects &= ~(1U << j);
			}
		if (!host->dialects)
			memset(host, 0, sizeof(*host));
	}
	if (!tray_sni_owner(name) || next[0])
		return;
	for (unsigned int i = 0; i < bus->model.count;)
		if (!strcmp(bus->model.items[i].owner, name))
			remove_item(bus, bus->model.items[i].id);
		else
			i++;
	for (unsigned int i = 0; i < BUS_HOST_MAX; i++)
		if (!strcmp(bus->hosts[i].owner, name))
			memset(&bus->hosts[i], 0, sizeof(bus->hosts[i]));
}

static void watcher_signal(tray_bus *bus, DBusMessage *message,
	unsigned int dialect, double now)
{
	const char *member = dbus_message_get_member(message), *value;
	if ((strcmp(member, "StatusNotifierItemRegistered") &&
	    strcmp(member, "StatusNotifierItemUnregistered")) ||
	    !dbus_message_has_signature(message, "s") ||
	    !dbus_message_get_args(message, NULL, DBUS_TYPE_STRING, &value,
	    DBUS_TYPE_INVALID) || !tray_sni_text(value,
	    TRAY_SNI_OWNER_MAX + TRAY_SNI_PATH_MAX + 1, false))
		return;
	watcher_state *watcher = &bus->watchers[dialect];
	if (watcher->registry_seeding)
	{
		watcher->registry_seeding = false;
		for (unsigned int i = 0; i < BUS_PENDING_MAX; i++)
			if (bus->pending[i].kind == PENDING_ITEM_OWNER &&
			    bus->pending[i].dialect == dialect &&
			    bus->pending[i].registry_seed)
				pending_clear(&bus->pending[i]);
	}
	watcher->registry_refresh = true;
	if (!strcmp(member, "StatusNotifierItemRegistered"))
	{
		(void)registry_entry(bus, value, dialect, now);
		return;
	}
	const char *slash = strchr(value, '/');
	char service[TRAY_SNI_OWNER_MAX + 1];
	size_t length = slash ? (size_t)(slash - value) : strlen(value);
	if (length > TRAY_SNI_OWNER_MAX)
		return;
	memcpy(service, value, length);
	service[length] = 0;
	const char *path = slash ? slash : "/StatusNotifierItem";
	if (tray_sni_owner(service))
		drop_reference(bus, dialect, service, path);
	else
		for (unsigned int i = 0; i < watcher->registry_count; i++)
		{
			registry_record *record = &watcher->registry[i];
			if (!strcmp(record->service, service) &&
			    !strcmp(record->path, path) && record->owner[0])
				drop_reference(bus, dialect, record->owner,
				    path);
		}
}

static void signal_message(tray_bus *bus, DBusMessage *message, double now)
{
	const char *sender = dbus_message_get_sender(message);
	const char *interface = dbus_message_get_interface(message);
	const char *member = dbus_message_get_member(message);
	const char *path = dbus_message_get_path(message);
	if (!sender || !interface || !member || !path)
		return;
	if (!strcmp(sender, DBUS_SERVICE_DBUS) &&
	    !strcmp(interface, DBUS_INTERFACE_DBUS) &&
	    !strcmp(path, DBUS_PATH_DBUS) && !strcmp(member,
	        "NameOwnerChanged"))
	{
		owner_changed(bus, message);
		return;
	}
	for (unsigned int i = 0; i < 2; i++)
		if (!bus->watchers[i].owned &&
		    !strcmp(sender, bus->watchers[i].owner) &&
		    !strcmp(interface, watcher_names[i]) &&
		    !strcmp(path, BUS_WATCHER_PATH))
		{
			watcher_signal(bus, message, i, now);
			return;
		}
	for (unsigned int i = 0; i < bus->model.count; i++)
	{
		tray_sni_item *item = &bus->model.items[i];
		if (strcmp(sender, item->owner))
			continue;
		if (!strcmp(path, item->path))
		{
			bool changed = false;
			if ((!strcmp(interface, item_interfaces[0]) ||
			     !strcmp(interface, item_interfaces[1])) &&
			    ((!strcmp(member, "NewStatus") &&
			      dbus_message_has_signature(message, "s")) ||
			     ((!strcmp(member, "NewTitle") ||
			       !strcmp(member, "NewIcon") ||
			       !strcmp(member, "NewAttentionIcon") ||
			       !strcmp(member, "NewOverlayIcon") ||
			       !strcmp(member, "NewToolTip") ||
			       !strcmp(member, "NewMenu")) &&
			      dbus_message_has_signature(message, ""))))
				changed = true;
			if (!strcmp(interface, DBUS_INTERFACE_PROPERTIES) &&
			    !strcmp(member, "PropertiesChanged") &&
			    dbus_message_has_signature(message, "sa{sv}as"))
			{
				DBusMessageIter first;
				const char *subject;
				dbus_message_iter_init(message, &first);
				dbus_message_iter_get_basic(&first, &subject);
				changed = !strcmp(subject, item->interface);
			}
			if (changed)
			{
				item_meta *meta = metadata(bus, item->id,
				    false);
				if (meta)
					meta->refresh = true, meta->tried = 0;
			}
		}
		if (bus->model.menu.open &&
		    bus->model.menu.item_id == item->id &&
		    !strcmp(path, item->menu_path) &&
		    !strcmp(interface, BUS_MENU_INTERFACE) &&
		    ((!strcmp(member, "LayoutUpdated") &&
		      dbus_message_has_signature(message, "ui")) ||
		     (!strcmp(member, "ItemsPropertiesUpdated") &&
		      dbus_message_has_signature(message, "a(ia{sv})a(ias)"))))
			invalidate_menu(bus);
	}
}

static void expire_pending(tray_bus *bus, double now)
{
	for (unsigned int i = 0; i < BUS_PENDING_MAX; i++)
	{
		bus_pending *pending = &bus->pending[i];
		if (!pending->kind || now < pending->expires)
			continue;
		refused(bus, "bounded tray request expired");
		if (pending->registration)
			reply_error(bus, pending->registration,
			    DBUS_ERROR_TIMEOUT,
			    "Bounded tray request expired");
		if (pending->kind == PENDING_MATCH)
			bus->failed = true;
		if (pending->kind == PENDING_HOST)
			bus->watchers[pending->dialect].host_pending = false;
		if (pending->kind == PENDING_REGISTRY)
		{
			bus->watchers[pending->dialect].registry_pending =
			    false;
			bus->watchers[pending->dialect].registry_loaded = true;
		}
		if (pending->kind == PENDING_ITEM_OWNER &&
		    pending->registry_seed &&
		    pending->registry_index <
		    bus->watchers[pending->dialect].registry_count)
		{
			registry_record *record =
			    &bus->watchers[pending->dialect].
			    registry[pending->registry_index];
			record->pending = false;
			record->resolved = true;
		}
		if (pending->kind == PENDING_PROPERTIES)
		{
			item_meta *meta = metadata(bus, pending->item_id,
			    false);
			if (meta)
				meta->pending = false;
		}
		if ((pending->kind == PENDING_ABOUT ||
		     pending->kind == PENDING_LAYOUT) &&
		    bus->model.menu.open && bus->model.menu.generation ==
		    pending->menu_generation)
		{
			tray_sni_item *item = tray_sni_find(&bus->model,
			    pending->item_id);
			/* Clear first so fallback can acquire this bounded
			 * slot. */
			pending_clear(pending);
			menu_fallback(bus, item, now);
			continue;
		}
		pending_clear(pending);
	}
}

static bool menu_pending(const tray_bus *bus)
{
	for (unsigned int i = 0; i < BUS_PENDING_MAX; i++)
		if (bus->pending[i].kind == PENDING_ABOUT ||
		    bus->pending[i].kind == PENDING_LAYOUT)
			return true;
	return false;
}

static void seed_registry(tray_bus *bus, unsigned int dialect, double now)
{
	watcher_state *watcher = &bus->watchers[dialect];
	if (!watcher->registry_seeding)
		return;
	bool complete = true;
	for (unsigned int i = 0; i < watcher->registry_count; i++)
	{
		registry_record *record = &watcher->registry[i];
		if (record->resolved)
			continue;
		complete = false;
		bus_pending *pending = pending_slot(bus);
		if (!record->pending && pending && owner_request(bus,
		    record->service, PENDING_ITEM_OWNER, dialect, record->path,
		    NULL, now))
		{
			pending->registry_seed = true;
			pending->registry_index = i;
			record->pending = true;
		}
	}
	if (!complete)
		return;
	/* Commit a resolved snapshot, preserving existing item identities.
	 * Retire absent references before admitting replacements. */
	for (unsigned int i = 0; i < TRAY_SNI_MAX; i++)
		bus->meta[i].references &= ~(1U << (dialect + 2));
	for (unsigned int i = 0; i < watcher->registry_count; i++)
	{
		registry_record *record = &watcher->registry[i];
		for (unsigned int j = 0; j < bus->model.count; j++)
		{
			tray_sni_item *item = &bus->model.items[j];
			if (!strcmp(item->owner, record->owner) &&
			    !strcmp(item->path, record->path))
			{
				item_meta *meta = metadata(bus, item->id,
				    false);
				if (meta)
					meta->references |= 1U << (dialect + 2);
			}
		}
	}
	for (unsigned int i = 0; i < bus->model.count;)
	{
		item_meta *meta = metadata(bus, bus->model.items[i].id, false);
		if (!meta || !meta->references)
			remove_item(bus, bus->model.items[i].id);
		else
			i++;
	}
	for (unsigned int i = 0; i < watcher->registry_count; i++)
	{
		registry_record *record = &watcher->registry[i];
		if (record->owner[0])
			(void)admit_item(bus, record->owner, record->path,
			    dialect, true);
	}
	watcher->registry_seeding = false;
}

static void pump(tray_bus *bus, double now)
{
	while (bus->match_next < sizeof(matches) / sizeof(matches[0]) &&
	    pending_slot(bus))
	{
		if (!request(bus, string_call(DBUS_SERVICE_DBUS, DBUS_PATH_DBUS,
		    DBUS_INTERFACE_DBUS, "AddMatch", matches[bus->match_next]),
		    PENDING_MATCH, DBUS_SERVICE_DBUS, 0, 0, 0, now))
			break;
		bus->match_next++;
	}
	if (bus->match_next < sizeof(matches) / sizeof(matches[0]))
		return;
	for (unsigned int i = 0; i < 2 && pending_slot(bus); i++)
	{
		watcher_state *watcher = &bus->watchers[i];
		if (!watcher->requested)
		{
			DBusMessage *message = new_call(DBUS_SERVICE_DBUS,
			    DBUS_PATH_DBUS, DBUS_INTERFACE_DBUS, "RequestName");
			dbus_uint32_t flags = DBUS_NAME_FLAG_DO_NOT_QUEUE;
			const char *name = watcher_names[i];
			if (message && !dbus_message_append_args(message,
			    DBUS_TYPE_STRING, &name, DBUS_TYPE_UINT32, &flags,
			    DBUS_TYPE_INVALID))
			{
				dbus_message_unref(message);
				message = NULL;
			}
			if (request(bus, message, PENDING_NAME,
			    DBUS_SERVICE_DBUS,
			    i, 0, 0, now))
				watcher->requested = true;
			continue;
		}
		if (!watcher->owner[0] || watcher->owned)
			continue;
		if (!watcher->host_attempted)
		{
			const char *self = dbus_bus_get_unique_name(
			    bus->connection);
			if (request(bus, string_call(watcher->owner,
			    BUS_WATCHER_PATH, watcher_names[i],
			    "RegisterStatusNotifierHost", self), PENDING_HOST,
			    watcher->owner, i, 0, 0, now))
				watcher->host_pending = watcher->host_attempted
				    = true;
		}
		if (pending_slot(bus) && watcher->host_registered &&
		    !watcher->registry_pending && !watcher->registry_seeding &&
		    (!watcher->registry_loaded || watcher->registry_refresh))
		{
			DBusMessage *message = new_call(watcher->owner,
			    BUS_WATCHER_PATH, DBUS_INTERFACE_PROPERTIES, "Get");
			const char *name = "RegisteredStatusNotifierItems";
			const char *interface = watcher_names[i];
			if (message && !dbus_message_append_args(message,
			    DBUS_TYPE_STRING, &interface, DBUS_TYPE_STRING,
			        &name,
			    DBUS_TYPE_INVALID))
			{
				dbus_message_unref(message);
				message = NULL;
			}
			if (request(bus, message, PENDING_REGISTRY,
			    watcher->owner,
			    i, 0, 0, now))
			{
				watcher->registry_pending = true;
				watcher->registry_refresh = false;
			}
		}
	}
	for (unsigned int i = 0; i < 2; i++)
		seed_registry(bus, i, now);
	for (unsigned int i = 0; i < TRAY_SNI_MAX && pending_slot(bus); i++)
		if (bus->meta[i].id && bus->meta[i].refresh &&
		    !bus->meta[i].pending)
		{
			tray_sni_item *item = tray_sni_find(&bus->model,
			    bus->meta[i].id);
			if (item)
				(void)property_request(bus, item, now);
		}
	if (bus->menu_refresh && bus->model.menu.open && !menu_pending(bus) &&
	    pending_slot(bus))
	{
		const tray_sni_item *item = tray_sni_lookup(&bus->model,
		    bus->model.menu.item_id, bus->model.menu.item_generation);
		if (item && layout_request(bus, item, now))
			bus->menu_refresh = false;
		else if (!item)
			tray_bus_menu_close(bus);
	}
}

tray_bus *tray_bus_create(void)
{
	tray_bus *bus = fxcalloc(1, sizeof(*bus));
	tray_sni_model_init(&bus->model);
	bus->icon_budget = TRAY_SNI_ICON_BYTES;
	return bus;
}

bool tray_bus_available(void)
{
	return true;
}

bool tray_bus_open(tray_bus *bus, const char *address, double now)
{
	DBusError error;
	bool opened = false;
	if (!bus || bus->connection || !finite_time(now) ||
	    !tray_bus_address_valid(address) || !module_startup_begin(5))
		return false;
	dbus_error_init(&error);
	bus->connection = dbus_connection_open_private(address, &error);
	if (!bus->connection)
		goto out;
	dbus_connection_set_exit_on_disconnect(bus->connection, FALSE);
	dbus_connection_set_max_message_size(bus->connection,
	    TRAY_SNI_WIRE_MAX);
	dbus_connection_set_max_received_size(bus->connection,
	    TRAY_SNI_QUEUE_MAX);
	dbus_connection_set_max_message_unix_fds(bus->connection, 0);
	/* Aggregate zero disables even descriptor-free reads in libdbus. */
	dbus_connection_set_max_received_unix_fds(bus->connection, 1);
	if (!dbus_bus_register(bus->connection, &error))
		goto out;
	bus->failed = false;
	bus->dirty = true;
	struct timespec current;
	if (clock_gettime(CLOCK_MONOTONIC, &current) != 0)
		goto out;
	now = current.tv_sec + current.tv_nsec / 1000000000.0;
	opened = true;
	pump(bus, now);
	if (bus->failed)
		opened = false;
out:
	dbus_error_free(&error);
	module_startup_finish();
	if (!opened)
		tray_bus_close(bus);
	return opened;
}

void tray_bus_close(tray_bus *bus)
{
	if (!bus)
		return;
	for (unsigned int i = 0; i < BUS_PENDING_MAX; i++)
		pending_clear(&bus->pending[i]);
	if (bus->connection)
	{
		dbus_connection_close(bus->connection);
		dbus_connection_unref(bus->connection);
		bus->connection = NULL;
	}
	tray_sni_model_clear(&bus->model);
	memset(bus->meta, 0, sizeof(bus->meta));
	memset(bus->hosts, 0, sizeof(bus->hosts));
	memset(bus->watchers, 0, sizeof(bus->watchers));
	bus->match_next = 0;
	bus->buffered = bus->menu_refresh = false;
	bus->failed = false;
	bus->dirty = true;
}

void tray_bus_destroy(tray_bus *bus)
{
	if (!bus)
		return;
	tray_bus_close(bus);
	free(bus);
}

bool tray_bus_connected(const tray_bus *bus)
{
	return bus && bus->connection && !bus->failed &&
	    dbus_connection_get_is_connected(bus->connection);
}

int tray_bus_fd(const tray_bus *bus)
{
	int descriptor = -1;
	if (bus && bus->connection)
		dbus_connection_get_unix_fd(bus->connection, &descriptor);
	return descriptor;
}

short tray_bus_events(const tray_bus *bus)
{
	return POLLIN | (bus && bus->connection &&
	    dbus_connection_get_outgoing_size(bus->connection) ? POLLOUT : 0);
}

int tray_bus_timeout(const tray_bus *bus, double now)
{
	if (!tray_bus_connected(bus) || !finite_time(now))
		return -1;
	if (bus->buffered)
		return 0;
	int timeout = -1;
	for (unsigned int i = 0; i < BUS_PENDING_MAX; i++)
		if (bus->pending[i].kind)
		{
			double left = (bus->pending[i].expires - now) * 1000;
			int value = left <= 0 ? 0 : left > 2000 ? 2000 :
			    (int)ceil(left);
			if (timeout < 0 || value < timeout)
				timeout = value;
		}
	return timeout;
}

bool tray_bus_service(tray_bus *bus, double now)
{
	if (!tray_bus_connected(bus) || !finite_time(now))
		return false;
	expire_pending(bus, now);
	if (!dbus_connection_read_write(bus->connection, 0))
	{
		tray_bus_close(bus);
		return false;
	}
	struct timespec started, current;
	bool clock_valid = clock_gettime(CLOCK_MONOTONIC, &started) == 0;
	for (unsigned int i = 0; i < BUS_TURN_MAX && !bus->failed; i++)
	{
		DBusMessage *message = dbus_connection_pop_message(
		    bus->connection);
		if (!message)
			break;
		if (wire_valid(message))
		{
			int type = dbus_message_get_type(message);
			if (type == DBUS_MESSAGE_TYPE_METHOD_CALL)
				watcher_method(bus, message, now);
			else if (type == DBUS_MESSAGE_TYPE_SIGNAL)
				signal_message(bus, message, now);
			else if (type == DBUS_MESSAGE_TYPE_METHOD_RETURN ||
			    type == DBUS_MESSAGE_TYPE_ERROR)
				handle_reply(bus, message, now);
		}
		else if (dbus_message_get_type(message) ==
		    DBUS_MESSAGE_TYPE_METHOD_CALL)
			reply_error(bus, message, DBUS_ERROR_LIMITS_EXCEEDED,
			    "Tray message exceeds bounded protocol inputs");
		dbus_message_unref(message);
		if (clock_valid && clock_gettime(CLOCK_MONOTONIC, &current) ==
		    0 &&
		    ((current.tv_sec - started.tv_sec) * 1000000000LL +
		     current.tv_nsec - started.tv_nsec) >= 2000000)
			break;
	}
	if (bus->failed || !dbus_connection_get_is_connected(bus->connection))
	{
		tray_bus_close(bus);
		return false;
	}
	pump(bus, now);
	if (bus->failed || !dbus_connection_read_write(bus->connection, 0))
	{
		tray_bus_close(bus);
		return false;
	}
	bus->buffered = dbus_connection_get_dispatch_status(bus->connection) ==
	    DBUS_DISPATCH_DATA_REMAINS;
	return !bus->failed;
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
	if (!bus)
		return;
	bus->model.capacity = capacity > TRAY_SNI_MAX ? TRAY_SNI_MAX : capacity;
	while (bus->model.count > bus->model.capacity)
		remove_item(bus, bus->model.items[bus->model.count - 1].id);
}

void tray_bus_set_icon_budget(tray_bus *bus, size_t max_bytes)
{
	if (bus)
		bus->icon_budget = max_bytes > TRAY_SNI_ICON_BYTES ?
		    TRAY_SNI_ICON_BYTES : max_bytes;
}

DBusMessage *tray_bus_action_message(const tray_sni_item *item,
	tray_bus_action_kind action, int x, int y, int delta, bool vertical)
{
	if (!item || !item->id || !item->generation || !item->ready ||
	    item->status == TRAY_SNI_PASSIVE || !tray_sni_owner(item->owner) ||
	    !dbus_validate_bus_name(item->owner, NULL) ||
	    !tray_sni_path(item->path) ||
	    !tray_sni_text(item->interface, sizeof(item->interface) - 1,
	    false) ||
	    (strcmp(item->interface, item_interfaces[0]) &&
	     strcmp(item->interface, item_interfaces[1])))
		return NULL;
	const char *member;
	switch (action)
	{
	case TRAY_BUS_ACTIVATE:
		member = "Activate";
		break;
	case TRAY_BUS_SECONDARY:
		member = "SecondaryActivate";
		break;
	case TRAY_BUS_CONTEXT:
		member = "ContextMenu";
		break;
	case TRAY_BUS_SCROLL:
		member = "Scroll";
		break;
	default:
		return NULL;
	}
	DBusMessage *message = new_call(item->owner, item->path,
	    item->interface, member);
	if (!message)
		return NULL;
	dbus_message_set_auto_start(message, FALSE);
	dbus_int32_t first = action == TRAY_BUS_SCROLL ? delta : x;
	dbus_int32_t second = y;
	const char *orientation = vertical ? "vertical" : "horizontal";
	bool okay = action == TRAY_BUS_SCROLL ?
	    dbus_message_append_args(message, DBUS_TYPE_INT32, &first,
	    DBUS_TYPE_STRING, &orientation, DBUS_TYPE_INVALID) :
	    dbus_message_append_args(message, DBUS_TYPE_INT32, &first,
	    DBUS_TYPE_INT32, &second, DBUS_TYPE_INVALID);
	if (!okay)
	{
		dbus_message_unref(message);
		return NULL;
	}
	return message;
}

bool tray_bus_action(tray_bus *bus, uint64_t id, uint64_t generation,
	tray_bus_action_kind action, int x, int y, int delta, bool vertical,
	double now)
{
	const tray_sni_item *item = bus ? tray_sni_lookup(&bus->model, id,
	    generation) : NULL;
	if (!item || !tray_bus_connected(bus) || !finite_time(now))
		return false;
	DBusMessage *message = tray_bus_action_message(item, action, x, y,
	    delta, vertical);
	if (!message)
		return false;
	return request(bus, message, PENDING_ACTION, item->owner, 0,
	    id, generation, now) != NULL;
}

static bool menu_entry_available(const tray_sni_menu *menu,
	const tray_sni_menu_node *entry)
{
	for (unsigned int depth = 0; entry && depth <= TRAY_SNI_MENU_DEPTH;
	    depth++)
	{
		if (!entry->enabled || !entry->visible || entry->separator)
			return false;
		if (entry->parent == -1)
			return true;
		entry = tray_sni_menu_lookup(menu, entry->parent);
	}
	return false;
}

static void cancel_menu_pending(tray_bus *bus)
{
	for (unsigned int i = 0; i < BUS_PENDING_MAX; i++)
		if (bus->pending[i].kind == PENDING_ABOUT ||
		    bus->pending[i].kind == PENDING_LAYOUT)
			pending_clear(&bus->pending[i]);
	bus->menu_refresh = false;
}

tray_bus_menu_result tray_bus_menu_open(tray_bus *bus, uint64_t id,
	uint64_t generation, int32_t parent, int x, int y, double now)
{
	const tray_sni_item *item = bus ? tray_sni_lookup(&bus->model, id,
	    generation) : NULL;
	if (!item || !item->ready || item->status == TRAY_SNI_PASSIVE ||
	    !finite_time(now) || !tray_bus_connected(bus) || parent < 0)
	{
		tray_bus_menu_close(bus);
		return TRAY_BUS_MENU_FAILED;
	}
	if (parent)
	{
		const tray_sni_menu *menu = &bus->model.menu;
		const tray_sni_menu_node *entry =
		    tray_sni_menu_lookup(menu, parent);
		if (!menu->open || !menu->ready || menu->stale ||
		    menu->item_id != id ||
		    menu->item_generation != generation ||
		    !entry || !entry->submenu ||
		    !menu_entry_available(menu, entry))
		{
			tray_bus_menu_close(bus);
			return TRAY_BUS_MENU_FAILED;
		}
	}
	if (!item->menu_path[0] || !strcmp(item->menu_path, "/"))
	{
		tray_bus_menu_close(bus);
		return tray_bus_action(bus, id, generation, TRAY_BUS_CONTEXT,
		    x, y, 0, false, now) ? TRAY_BUS_MENU_EXTERNAL :
		    TRAY_BUS_MENU_FAILED;
	}
	DBusMessage *message = new_call(item->owner, item->menu_path,
	    BUS_MENU_INTERFACE, "AboutToShow");
	dbus_int32_t node = parent;
	if (!message || !dbus_message_append_args(message, DBUS_TYPE_INT32,
	    &node, DBUS_TYPE_INVALID))
	{
		if (message)
			dbus_message_unref(message);
		tray_bus_menu_close(bus);
		return TRAY_BUS_MENU_FAILED;
	}
	if (!bus->model.next_generation)
	{
		dbus_message_unref(message);
		refused(bus, "tray menu generation exhausted");
		tray_bus_menu_close(bus);
		return TRAY_BUS_MENU_FAILED;
	}
	cancel_menu_pending(bus);
	if (!tray_sni_menu_begin(&bus->model, id, generation, parent))
	{
		dbus_message_unref(message);
		tray_bus_menu_close(bus);
		return TRAY_BUS_MENU_FAILED;
	}
	bus->menu_x = x;
	bus->menu_y = y;
	bus_pending *pending = request(bus, message, PENDING_ABOUT, item->owner,
	    0, id, generation, now);
	if (!pending)
	{
		return menu_fallback(bus, item, now) ? TRAY_BUS_MENU_EXTERNAL :
		    TRAY_BUS_MENU_FAILED;
	}
	pending->menu_generation = bus->model.menu.generation;
	bus->dirty = true;
	return TRAY_BUS_MENU_NATIVE;
}

bool tray_bus_menu_click(tray_bus *bus, uint64_t menu_generation,
	int32_t node, uint32_t timestamp, double now)
{
	if (!bus || !tray_bus_connected(bus) || !finite_time(now))
		return false;
	const tray_sni_menu *menu = &bus->model.menu;
	const tray_sni_item *item = tray_sni_lookup(&bus->model, menu->item_id,
	    menu->item_generation);
	const tray_sni_menu_node *entry = tray_sni_menu_lookup(menu, node);
	if (!item || !menu->open || !menu->ready || menu->stale ||
	    !menu_generation || !menu->generation ||
	    menu->generation != menu_generation || !entry || !entry->enabled ||
	    !entry->visible || entry->separator || entry->submenu ||
	    !menu_entry_available(menu, entry))
		return false;
	DBusMessage *message = new_call(item->owner, item->menu_path,
	    BUS_MENU_INTERFACE, "Event");
	DBusMessageIter root, variant;
	dbus_int32_t identity = node, data = 0;
	dbus_uint32_t stamp = timestamp;
	const char *event = "clicked";
	if (!message)
		return false;
	dbus_message_iter_init_append(message, &root);
	if (!dbus_message_iter_append_basic(&root, DBUS_TYPE_INT32, &identity)
	    ||
	    !dbus_message_iter_append_basic(&root, DBUS_TYPE_STRING, &event) ||
	    !dbus_message_iter_open_container(&root, DBUS_TYPE_VARIANT, "i",
	    &variant) || !dbus_message_iter_append_basic(&variant,
	    DBUS_TYPE_INT32, &data) ||
	    !dbus_message_iter_close_container(&root, &variant) ||
	    !dbus_message_iter_append_basic(&root, DBUS_TYPE_UINT32, &stamp))
	{
		dbus_message_unref(message);
		return false;
	}
	return request(bus, message, PENDING_ACTION, item->owner, 0,
	    item->id, item->generation, now) != NULL;
}

void tray_bus_menu_close(tray_bus *bus)
{
	if (!bus)
		return;
	cancel_menu_pending(bus);
	tray_sni_menu_close(&bus->model);
	bus->dirty = true;
}
