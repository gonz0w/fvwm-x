/* This program is free software; you can redistribute it and/or modify
 * it under the GNU General Public License, version 2 or any later version.
 * Independent bounded StatusNotifier/dbusmenu fixture producer and watcher.
 * Only a caller-owned private Unix bus is admitted; no product code is linked. */
#include "config.h"
#include <dbus/dbus.h>
#include "libs/cJSON.h"
#include <poll.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#define ITEM_MAX 40U
#define EVENT_MAX 128U
#define HELD_MAX 8U
#define LINE_MAXIMUM 16384U

static DBusConnection *connection;
static const char *watch_names[] = {
	"org.kde.StatusNotifierWatcher",
	"org.freedesktop.StatusNotifierWatcher"};
static const char *watch_interfaces[] = {
	"org.kde.StatusNotifierWatcher",
	"org.freedesktop.StatusNotifierWatcher"};
static bool watching[2];
static char registry[32][1280];
static unsigned int registry_count, host_count;
static struct
{
	char owner[256], service[256];
	unsigned int dialects;
} hosts[16];
static cJSON *events;
static DBusMessage *held[HELD_MAX];
static unsigned int held_count;

struct item
{
	bool used, passive, attention, named, invalid, hold, checked;
	bool no_pixmaps;
	unsigned int dialect, revision;
	char path[128], title[1025], icon[1025], theme[1025];
	char menu[128], attention_icon[1025], overlay_icon[1025];
};
static struct item items[ITEM_MAX];

static bool private_address(const char *address)
{
	DBusAddressEntry **entries = NULL;
	DBusError error;
	const char *path;
	char parent[1025], *slash;
	struct stat st;
	int count = 0;
	bool valid = false;

	if (!address || strnlen(address, 2049) > 2048 ||
	    strncmp(address, "unix:path=", 10) || strchr(address, ';'))
		return false;
	dbus_error_init(&error);
	if (!dbus_parse_address(address, &entries, &count, &error) || count != 1)
		goto out;
	path = dbus_address_entry_get_value(entries[0], "path");
	if (!path || path[0] != '/' || strlen(path) >= sizeof(parent))
		goto out;
	strcpy(parent, path);
	slash = strrchr(parent, '/');
	if (!slash || slash == parent)
		goto out;
	*slash = 0;
	if (lstat(parent, &st) || !S_ISDIR(st.st_mode) ||
	    st.st_uid != geteuid() || (st.st_mode & 077) ||
	    lstat(path, &st) || !S_ISSOCK(st.st_mode) ||
	    st.st_uid != geteuid())
		goto out;
	valid = true;
out:
	if (entries)
		dbus_address_entries_free(entries);
	dbus_error_free(&error);
	return valid;
}

static const char *text(cJSON *object, const char *name, const char *fallback,
	size_t maximum)
{
	cJSON *value = cJSON_GetObjectItemCaseSensitive(object, name);
	const char *result = value ? (cJSON_IsString(value) ?
	    value->valuestring : NULL) : fallback;
	return result && strnlen(result, maximum + 1) <= maximum &&
	    dbus_validate_utf8(result, NULL) ? result : NULL;
}

static bool flag(cJSON *object, const char *name, bool fallback)
{
	cJSON *value = cJSON_GetObjectItemCaseSensitive(object, name);
	return value ? cJSON_IsTrue(value) : fallback;
}

static void send_message(DBusMessage *message)
{
	if (!message || !dbus_connection_send(connection, message, NULL))
		exit(3);
	dbus_message_unref(message);
}

static void error_reply(DBusMessage *request, const char *name)
{
	send_message(dbus_message_new_error(request, name,
	    "fixture refused unsupported request"));
}

static void event(const char *kind, const char *path, int64_t id, int x, int y)
{
	cJSON *value = cJSON_CreateObject();
	if (cJSON_GetArraySize(events) >= EVENT_MAX)
		cJSON_DeleteItemFromArray(events, 0);
	cJSON_AddStringToObject(value, "event", kind);
	cJSON_AddStringToObject(value, "path", path ? path : "");
	cJSON_AddNumberToObject(value, "id", id);
	cJSON_AddNumberToObject(value, "x", x);
	cJSON_AddNumberToObject(value, "y", y);
	cJSON_AddItemToArray(events, value);
}

static void property(DBusMessageIter *array, const char *name,
	const char *signature, int type, const void *value)
{
	DBusMessageIter entry, variant;
	dbus_message_iter_open_container(array, DBUS_TYPE_DICT_ENTRY, NULL, &entry);
	dbus_message_iter_append_basic(&entry, DBUS_TYPE_STRING, &name);
	dbus_message_iter_open_container(&entry, DBUS_TYPE_VARIANT, signature,
	    &variant);
	dbus_message_iter_append_basic(&variant, type, value);
	dbus_message_iter_close_container(&entry, &variant);
	dbus_message_iter_close_container(array, &entry);
}

static void string_property(DBusMessageIter *array, const char *name,
	const char *value)
{
	property(array, name, "s", DBUS_TYPE_STRING, &value);
}

static void pixmap_array(DBusMessageIter *parent, bool invalid)
{
	DBusMessageIter array, row, bytes;
	dbus_int32_t width = invalid ? INT32_MAX : 2, height = 2;
	const unsigned char pixels[] = {255, 220, 30, 20, 255, 30, 200, 40,
	    255, 20, 60, 220, 128, 200, 180, 20};
	const unsigned char *data = pixels;
	dbus_message_iter_open_container(parent, DBUS_TYPE_ARRAY, "(iiay)",
	    &array);
	dbus_message_iter_open_container(&array, DBUS_TYPE_STRUCT, NULL, &row);
	dbus_message_iter_append_basic(&row, DBUS_TYPE_INT32, &width);
	dbus_message_iter_append_basic(&row, DBUS_TYPE_INT32, &height);
	dbus_message_iter_open_container(&row, DBUS_TYPE_ARRAY, "y", &bytes);
	dbus_message_iter_append_fixed_array(&bytes, DBUS_TYPE_BYTE, &data,
	    sizeof(pixels));
	dbus_message_iter_close_container(&row, &bytes);
	dbus_message_iter_close_container(&array, &row);
	dbus_message_iter_close_container(parent, &array);
}

static void pixmap_property(DBusMessageIter *array, const char *name,
	bool invalid)
{
	DBusMessageIter entry, variant;
	dbus_message_iter_open_container(array, DBUS_TYPE_DICT_ENTRY, NULL, &entry);
	dbus_message_iter_append_basic(&entry, DBUS_TYPE_STRING, &name);
	dbus_message_iter_open_container(&entry, DBUS_TYPE_VARIANT, "a(iiay)",
	    &variant);
	pixmap_array(&variant, invalid);
	dbus_message_iter_close_container(&entry, &variant);
	dbus_message_iter_close_container(array, &entry);
}

static void tooltip_property(DBusMessageIter *array, const struct item *item)
{
	DBusMessageIter entry, variant, row;
	const char *key = "ToolTip", *icon = "", *body = "Plain text $(data)";
	const char *title = item->title;
	dbus_message_iter_open_container(array, DBUS_TYPE_DICT_ENTRY, NULL, &entry);
	dbus_message_iter_append_basic(&entry, DBUS_TYPE_STRING, &key);
	dbus_message_iter_open_container(&entry, DBUS_TYPE_VARIANT,
	    "(sa(iiay)ss)", &variant);
	dbus_message_iter_open_container(&variant, DBUS_TYPE_STRUCT, NULL, &row);
	dbus_message_iter_append_basic(&row, DBUS_TYPE_STRING, &icon);
	pixmap_array(&row, false);
	dbus_message_iter_append_basic(&row, DBUS_TYPE_STRING, &title);
	dbus_message_iter_append_basic(&row, DBUS_TYPE_STRING, &body);
	dbus_message_iter_close_container(&variant, &row);
	dbus_message_iter_close_container(&entry, &variant);
	dbus_message_iter_close_container(array, &entry);
}

static const char *item_interface(const struct item *item)
{
	return item->dialect ? "org.freedesktop.StatusNotifierItem" :
	    "org.kde.StatusNotifierItem";
}

static void item_properties(DBusMessage *request, struct item *item)
{
	DBusMessage *reply = dbus_message_new_method_return(request);
	DBusMessageIter root, array;
	const char *status = item->passive ? "Passive" :
	    item->attention ? "NeedsAttention" : "Active";
	const char *menu = item->menu, *category = "ApplicationStatus";
	dbus_bool_t is_menu = false;
	dbus_uint32_t window = 0;

	dbus_message_iter_init_append(reply, &root);
	dbus_message_iter_open_container(&root, DBUS_TYPE_ARRAY, "{sv}", &array);
	string_property(&array, "Category", category);
	string_property(&array, "Id", item->path);
	string_property(&array, "Title", item->title);
	string_property(&array, "Status", status);
	string_property(&array, "IconName", item->named ? item->icon : "");
	string_property(&array, "AttentionIconName", item->attention_icon);
	string_property(&array, "OverlayIconName", item->overlay_icon);
	string_property(&array, "IconThemePath", item->theme);
	property(&array, "ItemIsMenu", "b", DBUS_TYPE_BOOLEAN, &is_menu);
	property(&array, "Menu", "o", DBUS_TYPE_OBJECT_PATH, &menu);
	property(&array, "WindowId", item->dialect ? "u" : "i",
	    item->dialect ? DBUS_TYPE_UINT32 : DBUS_TYPE_INT32, &window);
	if (!item->no_pixmaps)
	{
		if (!item->named)
			pixmap_property(&array, "IconPixmap", item->invalid);
		pixmap_property(&array, "AttentionIconPixmap", false);
	}
	tooltip_property(&array, item);
	dbus_message_iter_close_container(&root, &array);
	send_message(reply);
}

static void menu_node(DBusMessageIter *parent, struct item *item, int id,
	unsigned int depth)
{
	DBusMessageIter row, props, children, variant;
	dbus_int32_t identifier = id, state = item->checked ? 1 : 0;
	dbus_bool_t enabled = id != 15, visible = id != 16;
	const char *label = id == 10 ? "Open data $(label)" :
	    id == 11 ? "Checked option" : id == 12 ? "Submenu" :
	    id == 13 ? "Nested action" : id == 15 ? "Disabled" : "Hidden";
	const char *kind = "separator", *toggle = "checkmark", *display = "submenu";

	dbus_message_iter_open_container(parent, DBUS_TYPE_STRUCT, NULL, &row);
	dbus_message_iter_append_basic(&row, DBUS_TYPE_INT32, &identifier);
	dbus_message_iter_open_container(&row, DBUS_TYPE_ARRAY, "{sv}", &props);
	if (id)
	{
		string_property(&props, "label", label);
		if (id == 10)
			string_property(&props, "icon-name", "tray-fixture");
		property(&props, "enabled", "b", DBUS_TYPE_BOOLEAN, &enabled);
		property(&props, "visible", "b", DBUS_TYPE_BOOLEAN, &visible);
	}
	if (id == 14)
		string_property(&props, "type", kind);
	if (id == 11)
	{
		string_property(&props, "toggle-type", toggle);
		property(&props, "toggle-state", "i", DBUS_TYPE_INT32, &state);
	}
	if (id == 0 || id == 12)
		string_property(&props, "children-display", display);
	dbus_message_iter_close_container(&row, &props);
	dbus_message_iter_open_container(&row, DBUS_TYPE_ARRAY, "v", &children);
	if (depth < 4 && (id == 0 || id == 12))
	{
		int first = id == 0 ? 10 : 13, last = id == 0 ? 16 : 13;
		for (int i = first; i <= last; i++)
		{
			/* Nested action is present only under its submenu. */
			if (id == 0 && i == 13)
				continue;
			dbus_message_iter_open_container(&children, DBUS_TYPE_VARIANT,
			    "(ia{sv}av)", &variant);
			menu_node(&variant, item, i, depth + 1);
			dbus_message_iter_close_container(&children, &variant);
		}
	}
	dbus_message_iter_close_container(&row, &children);
	dbus_message_iter_close_container(parent, &row);
}

static void watcher_properties(DBusMessage *request)
{
	DBusMessage *reply = dbus_message_new_method_return(request);
	DBusMessageIter root, props, entry, variant, names;
	const char *key = "RegisteredStatusNotifierItems";
	dbus_bool_t registered = host_count != 0;
	dbus_int32_t version = 0;

	dbus_message_iter_init_append(reply, &root);
	dbus_message_iter_open_container(&root, DBUS_TYPE_ARRAY, "{sv}", &props);
	property(&props, "ProtocolVersion", "i", DBUS_TYPE_INT32, &version);
	property(&props, "IsStatusNotifierHostRegistered", "b",
	    DBUS_TYPE_BOOLEAN, &registered);
	dbus_message_iter_open_container(&props, DBUS_TYPE_DICT_ENTRY, NULL,
	    &entry);
	dbus_message_iter_append_basic(&entry, DBUS_TYPE_STRING, &key);
	dbus_message_iter_open_container(&entry, DBUS_TYPE_VARIANT, "as", &variant);
	dbus_message_iter_open_container(&variant, DBUS_TYPE_ARRAY, "s", &names);
	for (unsigned int i = 0; i < registry_count; i++)
	{
		const char *value = registry[i];
		dbus_message_iter_append_basic(&names, DBUS_TYPE_STRING, &value);
	}
	dbus_message_iter_close_container(&variant, &names);
	dbus_message_iter_close_container(&entry, &variant);
	dbus_message_iter_close_container(&props, &entry);
	dbus_message_iter_close_container(&root, &props);
	send_message(reply);
}

static void watcher_get(DBusMessage *request)
{
	const char *interface, *name;
	if (!dbus_message_get_args(request, NULL,
	    DBUS_TYPE_STRING, &interface, DBUS_TYPE_STRING, &name,
	    DBUS_TYPE_INVALID) || (strcmp(interface, watch_interfaces[0]) &&
	    strcmp(interface, watch_interfaces[1])))
	{
		error_reply(request, DBUS_ERROR_INVALID_ARGS);
		return;
	}
	DBusMessage *reply = dbus_message_new_method_return(request);
	DBusMessageIter root, value, array;
	dbus_message_iter_init_append(reply, &root);
	if (!strcmp(name, "RegisteredStatusNotifierItems"))
	{
		dbus_message_iter_open_container(&root, DBUS_TYPE_VARIANT, "as", &value);
		dbus_message_iter_open_container(&value, DBUS_TYPE_ARRAY, "s", &array);
		for (unsigned int i = 0; i < registry_count; i++)
		{
			const char *entry = registry[i];
			dbus_message_iter_append_basic(&array, DBUS_TYPE_STRING, &entry);
		}
		dbus_message_iter_close_container(&value, &array);
	}
	else if (!strcmp(name, "ProtocolVersion"))
	{
		dbus_int32_t version = 0;
		dbus_message_iter_open_container(&root, DBUS_TYPE_VARIANT, "i", &value);
		dbus_message_iter_append_basic(&value, DBUS_TYPE_INT32, &version);
	}
	else if (!strcmp(name, "IsStatusNotifierHostRegistered"))
	{
		dbus_bool_t registered = host_count != 0;
		dbus_message_iter_open_container(&root, DBUS_TYPE_VARIANT, "b", &value);
		dbus_message_iter_append_basic(&value, DBUS_TYPE_BOOLEAN, &registered);
	}
	else
	{
		dbus_message_unref(reply);
		error_reply(request, DBUS_ERROR_UNKNOWN_PROPERTY);
		return;
	}
	dbus_message_iter_close_container(&root, &value);
	send_message(reply);
}

static void watcher_signal(unsigned int dialect, const char *member,
	const char *identifier)
{
	DBusMessage *message = dbus_message_new_signal("/StatusNotifierWatcher",
	    watch_interfaces[dialect], member);
	if (identifier)
		dbus_message_append_args(message, DBUS_TYPE_STRING, &identifier,
		    DBUS_TYPE_INVALID);
	send_message(message);
}

static bool watcher_host(DBusMessage *message, const char *service,
	unsigned int dialect)
{
	const char *owner = dbus_message_get_sender(message);
	if (!owner || owner[0] != ':' ||
	    !dbus_validate_bus_name(owner, NULL) ||
	    !dbus_validate_bus_name(service, NULL))
		return false;
	unsigned int index = host_count;
	for (unsigned int i = 0; i < host_count; i++)
		if (!strcmp(hosts[i].owner, owner) &&
		    !strcmp(hosts[i].service, service))
			index = i;
	if (index >= sizeof(hosts) / sizeof(hosts[0]))
		return false;
	if (index == host_count)
	{
		strcpy(hosts[index].owner, owner);
		strcpy(hosts[index].service, service);
		host_count++;
	}
	if (!(hosts[index].dialects & (1U << dialect)))
	{
		hosts[index].dialects |= 1U << dialect;
		event("HostRegistered", service, 0, (int)dialect, 0);
		watcher_signal(dialect, "StatusNotifierHostRegistered", NULL);
	}
	return true;
}

static bool watcher_item(DBusMessage *message, const char *name,
	unsigned int dialect)
{
	const char *owner = dbus_message_get_sender(message);
	char value[sizeof(registry[0])];
	if (!owner || owner[0] != ':' ||
	    !dbus_validate_bus_name(owner, NULL) ||
	    (name[0] == '/' ? !dbus_validate_path(name, NULL) :
	     !dbus_validate_bus_name(name, NULL)))
		return false;
	if (snprintf(value, sizeof(value), "%s%s",
	    name[0] == '/' ? owner : "", name) >= (int)sizeof(value))
		return false;
	for (unsigned int i = 0; i < registry_count; i++)
		if (!strcmp(registry[i], value))
			return true;
	if (registry_count >= sizeof(registry) / sizeof(registry[0]))
		return false;
	strcpy(registry[registry_count++], value);
	watcher_signal(dialect, "StatusNotifierItemRegistered", value);
	return true;
}

static void watcher_owner_changed(DBusMessage *message)
{
	const char *sender = dbus_message_get_sender(message);
	const char *path = dbus_message_get_path(message);
	const char *name, *previous, *next;
	if (!sender || strcmp(sender, DBUS_SERVICE_DBUS) ||
	    !path || strcmp(path, DBUS_PATH_DBUS) ||
	    !dbus_message_has_signature(message, "sss") ||
	    !dbus_message_get_args(message, NULL, DBUS_TYPE_STRING, &name,
	    DBUS_TYPE_STRING, &previous, DBUS_TYPE_STRING, &next,
	    DBUS_TYPE_INVALID) || !dbus_validate_bus_name(name, NULL) ||
	    !previous[0] || previous[0] != ':' ||
	    !dbus_validate_bus_name(previous, NULL) ||
	    (next[0] && (next[0] != ':' ||
	     !dbus_validate_bus_name(next, NULL))) || !strcmp(previous, next))
		return;
	/* A registry entry is either a registered service name, or the actual
	 * unique owner followed by its custom object path. Reassignment retires
	 * the former registration; a new owner must register for itself. */
	for (unsigned int i = 0; i < registry_count;)
	{
		const char *slash = strchr(registry[i], '/');
		size_t length = slash ? (size_t)(slash - registry[i]) :
		    strlen(registry[i]);
		if (strlen(name) != length ||
		    strncmp(registry[i], name, length))
		{
			i++;
			continue;
		}
		char retired[sizeof(registry[0])];
		strcpy(retired, registry[i]);
		memmove(&registry[i], &registry[i + 1],
		    (--registry_count - i) * sizeof(registry[0]));
		memset(&registry[registry_count], 0, sizeof(registry[0]));
		for (unsigned int d = 0; d < 2; d++)
			if (watching[d])
				watcher_signal(d,
				    "StatusNotifierItemUnregistered", retired);
		event("ItemUnregistered", retired, 0, 0, 0);
	}
	for (unsigned int i = 0; i < host_count;)
	{
		bool unique_lost = name[0] == ':' &&
		    !strcmp(hosts[i].owner, name);
		bool alias_lost = !strcmp(hosts[i].service, name) &&
		    !strcmp(hosts[i].owner, previous);
		if (!unique_lost && !alias_lost)
		{
			i++;
			continue;
		}
		event("HostUnregistered", hosts[i].service, 0, 0, 0);
		memmove(&hosts[i], &hosts[i + 1],
		    (--host_count - i) * sizeof(hosts[0]));
		memset(&hosts[host_count], 0, sizeof(hosts[0]));
	}
}

static void service_message(DBusMessage *message, bool allow_hold)
{
	const char *path = dbus_message_get_path(message);
	const char *member = dbus_message_get_member(message);
	const char *interface = dbus_message_get_interface(message);
	DBusMessage *reply;
	DBusMessageIter root;

	if (dbus_message_is_signal(message, DBUS_INTERFACE_DBUS,
	    "NameOwnerChanged"))
	{
		watcher_owner_changed(message);
		return;
	}
	if (dbus_message_get_type(message) != DBUS_MESSAGE_TYPE_METHOD_CALL ||
	    !path || !member || !interface)
		return;
	if (!strcmp(path, "/StatusNotifierWatcher"))
	{
		if (!strcmp(interface, "org.freedesktop.DBus.Properties") &&
		    !strcmp(member, "Get"))
		{
			watcher_get(message);
			return;
		}
		if (!strcmp(interface, "org.freedesktop.DBus.Properties") &&
		    !strcmp(member, "GetAll"))
		{
			watcher_properties(message);
			return;
		}
		for (unsigned int d = 0; d < 2; d++)
			if (!strcmp(interface, watch_interfaces[d]) &&
			    dbus_message_has_signature(message, "s"))
			{
				const char *name;
				dbus_message_get_args(message, NULL,
				    DBUS_TYPE_STRING, &name, DBUS_TYPE_INVALID);
				if (!strcmp(member, "RegisterStatusNotifierHost"))
				{
					if (!watcher_host(message, name, d))
						goto unsupported;
				}
				else if (!strcmp(member,
				    "RegisterStatusNotifierItem"))
				{
					if (!watcher_item(message, name, d))
						goto unsupported;
				}
				else
					goto unsupported;
				send_message(dbus_message_new_method_return(message));
				return;
			}
		goto unsupported;
	}
	for (unsigned int i = 0; i < ITEM_MAX; i++)
	{
		struct item *item = &items[i];
		if (!item->used)
			continue;
		if (!strcmp(path, item->path))
		{
			if (!strcmp(interface, "org.freedesktop.DBus.Properties") &&
			    !strcmp(member, "GetAll"))
			{
				const char *requested;
				dbus_uint32_t serial =
				    dbus_message_get_serial(message);
				if (!dbus_message_has_signature(message, "s") ||
				    !dbus_message_get_args(message, NULL,
				        DBUS_TYPE_STRING, &requested,
				        DBUS_TYPE_INVALID))
				{
					error_reply(message,
					    DBUS_ERROR_INVALID_ARGS);
					return;
				}
				if (strcmp(requested, item_interface(item)))
				{
					error_reply(message,
					    DBUS_ERROR_UNKNOWN_INTERFACE);
					return;
				}
				if (allow_hold && item->hold &&
				    held_count < HELD_MAX)
				{
					held[held_count++] =
					    dbus_message_ref(message);
					event("HeldGetAll", path, serial, 0, 0);
				}
				else
					item_properties(message, item);
				return;
			}
			if (strcmp(interface, item_interface(item)))
				goto unsupported;
			int x = 0, y = 0;
			if (!strcmp(member, "Scroll"))
			{
				const char *orientation;
				if (!dbus_message_get_args(message, NULL,
				    DBUS_TYPE_INT32, &x,
				    DBUS_TYPE_STRING, &orientation, DBUS_TYPE_INVALID))
					goto unsupported;
				y = !strcmp(orientation, "vertical");
			}
			else if (strcmp(member, "Activate") &&
			    strcmp(member, "SecondaryActivate") &&
			    strcmp(member, "ContextMenu"))
				goto unsupported;
			else if (!dbus_message_get_args(message, NULL,
			    DBUS_TYPE_INT32, &x, DBUS_TYPE_INT32, &y,
			    DBUS_TYPE_INVALID))
				goto unsupported;
			event(member, path, 0, x, y);
			send_message(dbus_message_new_method_return(message));
			return;
		}
		if (strcmp(path, item->menu) || strcmp(interface, "com.canonical.dbusmenu"))
			continue;
		if (!strcmp(member, "AboutToShow"))
		{
			dbus_bool_t changed = false;
			event(member, path, 0, 0, 0);
			reply = dbus_message_new_method_return(message);
			dbus_message_append_args(reply, DBUS_TYPE_BOOLEAN, &changed,
			    DBUS_TYPE_INVALID);
			send_message(reply);
			return;
		}
		if (!strcmp(member, "GetLayout"))
		{
			dbus_uint32_t revision = item->revision;
			reply = dbus_message_new_method_return(message);
			dbus_message_iter_init_append(reply, &root);
			dbus_message_iter_append_basic(&root, DBUS_TYPE_UINT32, &revision);
			menu_node(&root, item, 0, 0);
			send_message(reply);
			return;
		}
		if (!strcmp(member, "Event") &&
		    dbus_message_iter_init(message, &root) &&
		    dbus_message_iter_get_arg_type(&root) == DBUS_TYPE_INT32)
		{
			dbus_int32_t identifier;
			dbus_message_iter_get_basic(&root, &identifier);
			event("MenuEvent", path, identifier, 0, 0);
			send_message(dbus_message_new_method_return(message));
			return;
		}
	}
unsupported:
	error_reply(message, DBUS_ERROR_UNKNOWN_METHOD);
}

static void service(void)
{
	dbus_connection_read_write(connection, 0);
	for (unsigned int i = 0; i < 32; i++)
	{
		DBusMessage *message = dbus_connection_pop_message(connection);
		if (!message)
			break;
		service_message(message, true);
		dbus_message_unref(message);
	}
}

static cJSON *method(const char *destination, const char *path,
	const char *interface, const char *member, const char *value)
{
	DBusMessage *request = dbus_message_new_method_call(destination,
	    path, interface, member), *reply;
	DBusError error;
	cJSON *result = cJSON_CreateObject();

	dbus_message_set_auto_start(request, false);
	if (value)
		dbus_message_append_args(request, DBUS_TYPE_STRING, &value,
		    DBUS_TYPE_INVALID);
	dbus_error_init(&error);
	reply = dbus_connection_send_with_reply_and_block(connection, request,
	    1000, &error);
	dbus_message_unref(request);
	if (reply)
	{
		cJSON_AddBoolToObject(result, "ok", true);
		dbus_message_unref(reply);
	}
	else
		cJSON_AddStringToObject(result, "error",
		    error.name ? error.name : "no reply");
	dbus_error_free(&error);
	return result;
}

static cJSON *command(cJSON *request)
{
	const char *verb = text(request, "verb", "", 32);
	const char *path = text(request, "path", "/StatusNotifierItem", 127);
	cJSON *result = cJSON_CreateObject();
	struct item *item = NULL;

	if (!verb || !path || !dbus_validate_path(path, NULL))
		goto invalid;
	for (unsigned int i = 0; i < ITEM_MAX; i++)
		if (items[i].used && !strcmp(items[i].path, path))
			item = &items[i];
	if (!strcmp(verb, "hello"))
		cJSON_AddStringToObject(result, "unique",
		    dbus_bus_get_unique_name(connection));
	else if (!strcmp(verb, "has-owner"))
	{
		const char *name = text(request, "name", "", 255);
		if (!name || !dbus_validate_bus_name(name, NULL))
			goto invalid;
		cJSON_AddBoolToObject(result, "owned",
		    dbus_bus_name_has_owner(connection, name, NULL));
	}
	else if (!strcmp(verb, "watch"))
	{
		const char *dialect = text(request, "dialect", "both", 16);
		DBusError error;
		if (!dialect)
			goto invalid;
		for (unsigned int d = 0; d < 2; d++)
			if (!strcmp(dialect, "both") ||
			    !strcmp(dialect, d ? "freedesktop" : "kde"))
			{
				dbus_error_init(&error);
				int status = dbus_bus_request_name(connection, watch_names[d],
				    DBUS_NAME_FLAG_DO_NOT_QUEUE, &error);
				watching[d] = status == DBUS_REQUEST_NAME_REPLY_PRIMARY_OWNER;
				dbus_error_free(&error);
			}
		cJSON_AddBoolToObject(result, "kde", watching[0]);
		cJSON_AddBoolToObject(result, "freedesktop", watching[1]);
	}
	else if (!strcmp(verb, "emit"))
	{
		const char *identifier = text(request, "entry", "", 1279);
		if (!identifier)
			goto invalid;
		for (unsigned int d = 0; d < 2; d++)
			if (watching[d])
				watcher_signal(d, "StatusNotifierItemRegistered", identifier);
		cJSON_AddBoolToObject(result, "ok", true);
	}
	else if (!strcmp(verb, "alias") || !strcmp(verb, "unalias"))
	{
		const char *name = text(request, "name", "", 255);
		DBusError error;
		if (!name || name[0] == ':' || !dbus_validate_bus_name(name, NULL))
			goto invalid;
		dbus_error_init(&error);
		int answer = !strcmp(verb, "alias") ?
		    dbus_bus_request_name(connection, name,
		        DBUS_NAME_FLAG_DO_NOT_QUEUE, &error) :
		    dbus_bus_release_name(connection, name, &error);
		cJSON_AddNumberToObject(result, "result", answer);
		dbus_error_free(&error);
	}
	else if (!strcmp(verb, "unwatch"))
	{
		for (unsigned int d = 0; d < 2; d++)
			if (watching[d])
			{
				dbus_bus_release_name(connection, watch_names[d], NULL);
				watching[d] = false;
			}
		cJSON_AddBoolToObject(result, "ok", true);
	}
	else if (!strcmp(verb, "signals"))
	{
		cJSON_AddItemToObject(result, "events", events);
		events = cJSON_CreateArray();
	}
	else if (!strcmp(verb, "registry"))
	{
		cJSON *array = cJSON_AddArrayToObject(result, "items");
		for (unsigned int i = 0; i < registry_count; i++)
			cJSON_AddItemToArray(array, cJSON_CreateString(registry[i]));
		cJSON_AddNumberToObject(result, "hosts", host_count);
	}
	else if (!strcmp(verb, "reply-held"))
	{
		cJSON *serial_value = cJSON_GetObjectItemCaseSensitive(
		    request, "serial");
		dbus_uint32_t serial = 0;
		unsigned int count = held_count, kept = 0, released = 0;
		if (serial_value)
		{
			if (!cJSON_IsNumber(serial_value) ||
			    !(serial_value->valuedouble >= 1 &&
			      serial_value->valuedouble <= UINT32_MAX))
				goto invalid;
			serial = (dbus_uint32_t)serial_value->valuedouble;
			if ((double)serial != serial_value->valuedouble)
				goto invalid;
		}
		for (unsigned int i = 0; i < count; i++)
		{
			if (serial && (released ||
			    dbus_message_get_serial(held[i]) != serial))
			{
				held[kept++] = held[i];
				continue;
			}
			service_message(held[i], false);
			dbus_message_unref(held[i]);
			released++;
		}
		for (unsigned int i = kept; i < count; i++)
			held[i] = NULL;
		held_count = kept;
		cJSON_AddNumberToObject(result, "released", released);
	}
	else if (!strcmp(verb, "add") || !strcmp(verb, "update"))
	{
		if (!item)
			for (unsigned int i = 0; i < ITEM_MAX; i++)
				if (!items[i].used)
				{
					item = &items[i];
					break;
				}
		const char *title = text(request, "title", "Fixture item", 1024);
		const char *icon = text(request, "icon", "folder", 1024);
		const char *attention = text(request, "attention_icon", "", 1024);
		const char *overlay = text(request, "overlay_icon", "", 1024);
		const char *theme = text(request, "theme_path", "", 1024);
		const char *dialect = text(request, "dialect", "kde", 16);
		if (!item || !title || !icon || !attention || !overlay ||
		    !theme || !dialect)
			goto invalid;
		item->used = true;
		strcpy(item->path, path);
		strcpy(item->title, title);
		strcpy(item->icon, icon);
		strcpy(item->attention_icon, attention);
		strcpy(item->overlay_icon, overlay);
		item->no_pixmaps = flag(request, "no_pixmaps", false);
		strcpy(item->theme, theme);
		if (snprintf(item->menu, sizeof(item->menu), "%sMenu", path) >=
		    (int)sizeof(item->menu))
			goto invalid;
		if (flag(request, "no_menu", false))
			strcpy(item->menu, "/");
		item->dialect = !strcmp(dialect, "freedesktop");
		item->passive = flag(request, "passive", false);
		item->attention = flag(request, "attention", false);
		item->named = flag(request, "named", false);
		item->invalid = flag(request, "invalid", false);
		item->hold = flag(request, "hold", false);
		item->checked = flag(request, "checked", false);
		item->revision++;
		if (!strcmp(verb, "add"))
		{
			if (!flag(request, "register", true))
			{
				cJSON_AddBoolToObject(result, "ok", true);
				return result;
			}
			cJSON_Delete(result);
			return method(watch_names[item->dialect],
			    "/StatusNotifierWatcher", watch_interfaces[item->dialect],
			    "RegisterStatusNotifierItem", path);
		}
		DBusMessage *changed = dbus_message_new_signal(path,
		    item_interface(item), "NewIcon");
		send_message(changed);
		changed = dbus_message_new_signal(path, item_interface(item),
		    "NewStatus");
		const char *status = item->passive ? "Passive" :
		    item->attention ? "NeedsAttention" : "Active";
		dbus_message_append_args(changed, DBUS_TYPE_STRING, &status,
		    DBUS_TYPE_INVALID);
		send_message(changed);
		changed = dbus_message_new_signal(item->menu,
		    "com.canonical.dbusmenu", "LayoutUpdated");
		dbus_uint32_t revision = item->revision;
		dbus_int32_t parent = 0;
		dbus_message_append_args(changed, DBUS_TYPE_UINT32, &revision,
		    DBUS_TYPE_INT32, &parent, DBUS_TYPE_INVALID);
		send_message(changed);
		cJSON_AddBoolToObject(result, "ok", true);
	}
	else if (!strcmp(verb, "register") && item)
	{
		cJSON_Delete(result);
		return method(watch_names[item->dialect], "/StatusNotifierWatcher",
		    watch_interfaces[item->dialect], "RegisterStatusNotifierItem", path);
	}
	else if (!strcmp(verb, "forge"))
	{
		const char *service_name = text(request, "service", "", 255);
		if (!service_name)
			goto invalid;
		cJSON_Delete(result);
		return method(watch_names[0], "/StatusNotifierWatcher",
		    watch_interfaces[0], "RegisterStatusNotifierItem", service_name);
	}
	else
		goto invalid;
	return result;
invalid:
	cJSON_AddStringToObject(result, "error", "invalid fixture command");
	return result;
}

int main(int argc, char **argv)
{
	DBusError error;
	char line[LINE_MAXIMUM + 2];
	int descriptor = -1, status = 0;

	if (argc != 3 || strcmp(argv[1], "--private-bus") ||
	    !private_address(argv[2]))
		return 2;
	dbus_error_init(&error);
	connection = dbus_connection_open_private(argv[2], &error);
	if (!connection || !dbus_bus_register(connection, &error) ||
	    !dbus_connection_get_unix_fd(connection, &descriptor))
		return 2;
	dbus_connection_set_exit_on_disconnect(connection, false);
	dbus_connection_set_max_message_size(connection, 1024 * 1024);
	dbus_bus_add_match(connection,
	    "type='signal',sender='org.freedesktop.DBus',"
	    "interface='org.freedesktop.DBus',member='NameOwnerChanged'", &error);
	if (dbus_error_is_set(&error))
	{
		dbus_error_free(&error);
		dbus_connection_close(connection);
		dbus_connection_unref(connection);
		return 2;
	}
	events = cJSON_CreateArray();
	for (unsigned int turn = 0; turn < 16384; turn++)
	{
		struct pollfd ready[2] = {{STDIN_FILENO, POLLIN, 0},
		    {descriptor, POLLIN, 0}};
		service();
		if (poll(ready, 2, 20) < 0)
		{
			status = 1;
			break;
		}
		if (ready[0].revents & POLLIN)
		{
			char *received = fgets(line, sizeof(line), stdin);
			if (!received)
				break;
			if (strlen(line) > LINE_MAXIMUM || !strchr(line, '\n'))
			{
				status = 1;
				break;
			}
			cJSON *request = cJSON_Parse(line);
			cJSON *reply = request ? command(request) : cJSON_CreateObject();
			char *encoded = cJSON_PrintUnformatted(reply);
			if (!encoded)
				exit(3);
			puts(encoded);
			fflush(stdout);
			free(encoded);
			cJSON_Delete(reply);
			cJSON_Delete(request);
		}
		if (ready[0].revents & (POLLHUP | POLLERR | POLLNVAL) ||
		    !dbus_connection_get_is_connected(connection))
			break;
	}
	for (unsigned int i = 0; i < held_count; i++)
		dbus_message_unref(held[i]);
	cJSON_Delete(events);
	dbus_connection_close(connection);
	dbus_connection_unref(connection);
	dbus_error_free(&error);
	return status;
}
