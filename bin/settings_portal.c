/* This program is free software; you can redistribute it and/or modify
 * it under the GNU General Public License as published by the Free Software
 * Foundation; either version 2, or any later version.
 *
 * Isolated, read-only XSETTINGS consumer. Xlib/libdbus calls may block;
 * the cooperative turn target cannot preempt them. The activation owner
 * supplies DISPLAY/XAUTHORITY and owns this process's overall lifetime.
 */
#include "config.h"
#include "settings_portal_policy.h"
#include "libs/FProperty.h"
#include "libs/XError.h"
#include "libs/module_startup.h"
#include <dbus/dbus.h>
#include <errno.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#define SP_BUS_NAME "org.freedesktop.impl.portal.desktop.fvwm"
#define SP_PATH "/org/freedesktop/portal/desktop"
#define SP_INTERFACE "org.freedesktop.impl.portal.Settings"
#define SP_NOT_FOUND "org.freedesktop.portal.Error.NotFound"
#define SP_MESSAGE_MAX 8192L
#define SP_RECEIVE_MAX 32768L
#define SP_OUTPUT_MAX 65536L
#define SP_TURN_MESSAGES 32U
#define SP_TURN_NS 2000000LL
#define SP_SOURCE_POLL_NS 250000000LL

struct portal
{
	Display *display;
	DBusConnection *bus;
	Window root, owner;
	Atom selection, settings, manager;
	uint32_t value, announced;
	bool dirty, failed, malformed;
	int64_t next_source;
};

/* One X thread; fence previous requests before accepting BadWindow only
 * for this foreign XID and the active operation's bounded request span. */
static struct
{
	Display *display;
	Window window;
	unsigned long first;
	bool active, vanished;
} sp_xtrap;
static bool sp_xfailed;

static const char introspection[] =
	"<node><interface name='" SP_INTERFACE "'>"
	"<method name='ReadAll'><arg name='namespaces' type='as' direction='in'/>"
	"<arg name='settings' type='a{sa{sv}}' direction='out'/></method>"
	"<method name='Read'><arg name='namespace' type='s' direction='in'/>"
	"<arg name='key' type='s' direction='in'/>"
	"<arg name='value' type='v' direction='out'/></method>"
	"<signal name='SettingChanged'><arg name='namespace' type='s'/>"
	"<arg name='key' type='s'/><arg name='value' type='v'/></signal>"
	"<property name='version' type='u' access='read'>"
	"<annotation name='org.freedesktop.DBus.Property.EmitsChangedSignal'"
	" value='const'/></property></interface>"
	"<interface name='org.freedesktop.DBus.Introspectable'>"
	"<method name='Introspect'><arg type='s' direction='out'/></method>"
	"</interface><interface name='org.freedesktop.DBus.Properties'>"
	"<method name='Get'><arg type='s' direction='in'/>"
	"<arg type='s' direction='in'/><arg type='v' direction='out'/></method>"
	"<method name='GetAll'><arg type='s' direction='in'/>"
	"<arg type='a{sv}' direction='out'/></method>"
	"<method name='Set'><arg type='s' direction='in'/>"
	"<arg type='s' direction='in'/><arg type='v' direction='in'/></method>"
	"</interface></node>";

static int64_t
monotonic_ns(void)
{
	struct timespec now;

	if (clock_gettime(CLOCK_MONOTONIC, &now))
		return -1;
	return (int64_t)now.tv_sec * 1000000000LL + now.tv_nsec;
}

static int
portal_x_error(Display *display, XErrorEvent *event)
{
	(void)display;
	(void)event;
	sp_xfailed = true;
	return 0;
}

static int
portal_x_io_error(Display *display)
{
	(void)display;
	/* No further X request, including XCloseDisplay, is safe here.
	 * Process exit also closes our private bus connection/name. */
	fputs("fvwm-settings-portal: X connection lost\n", stderr);
	_exit(1);
}

static int
foreign_x_error(Display *display, XErrorEvent *event)
{
	if (sp_xtrap.active && display == sp_xtrap.display &&
	    event->error_code == BadWindow &&
	    event->resourceid == sp_xtrap.window &&
	    event->serial >= sp_xtrap.first &&
	    event->serial < NextRequest(display))
	{
		sp_xtrap.vanished = true;
		return 0;
	}
	return ferror_call_next_error_handler(display, event);
}

static void
foreign_begin(Display *display, Window window)
{
	XSync(display, False);
	sp_xtrap.display = display;
	sp_xtrap.window = window;
	sp_xtrap.first = NextRequest(display);
	sp_xtrap.vanished = false;
	sp_xtrap.active = true;
	ferror_set_temp_error_handler(foreign_x_error);
}

static bool
foreign_end(Display *display)
{
	XSync(display, False);
	ferror_reset_temp_error_handler();
	sp_xtrap.active = false;
	return !sp_xtrap.vanished && !sp_xfailed;
}

static bool
select_owner(struct portal *portal, Window owner, long mask)
{
	foreign_begin(portal->display, owner);
	XSelectInput(portal->display, owner, mask);
	return foreign_end(portal->display);
}

typedef enum
{
	SP_SOURCE_VALID, SP_SOURCE_ABSENT, SP_SOURCE_INVALID, SP_SOURCE_RETRY
} sp_source_result;

static sp_source_result
read_source(struct portal *portal, sp_snapshot *snapshot)
{
	unsigned char *data = NULL;
	size_t length = 0;
	sp_source_result source = SP_SOURCE_ABSENT;
	Display *display = portal->display;
	Window owner = XGetSelectionOwner(display, portal->selection);

	if (owner != portal->owner)
	{
		if (portal->owner != None)
			(void)select_owner(portal, portal->owner,
			    portal->owner == portal->root ? StructureNotifyMask : 0);
		portal->owner = None;
		if (owner != None && !select_owner(portal, owner,
		    PropertyChangeMask | StructureNotifyMask))
			return SP_SOURCE_RETRY;
		portal->owner = owner;
	}
	if (owner == None)
		return SP_SOURCE_ABSENT;
	/* Recheck after subscription and on both sides of the property read.
	 * MANAGER hints are never accepted as proof of selection authority. */
	if (XGetSelectionOwner(display, portal->selection) != owner)
		return SP_SOURCE_RETRY;
	foreign_begin(display, owner);
	FPropertyLimits limits = {12, SP_SETTINGS_MAX, 8};
	FPropertyResult result = FPropertyRead(display, owner, portal->settings,
	    portal->settings, limits, &data, &length);
	bool alive = foreign_end(display);
	if (XGetSelectionOwner(display, portal->selection) != owner || !alive)
		source = SP_SOURCE_RETRY;
	else if (result == FPROPERTY_OK)
		source = sp_decode(data, length, snapshot) ?
		    SP_SOURCE_VALID : SP_SOURCE_INVALID;
	else if (result != FPROPERTY_MISSING)
		source = SP_SOURCE_INVALID;
	free(data);
	return source;
}

static void
refresh_source(struct portal *portal)
{
	sp_snapshot snapshot;
	sp_source_result source = SP_SOURCE_RETRY;

	/* One racing replacement gets another bounded complete read, avoiding
	 * a spurious zero/change pair for a stable replacement with equal data.
	 * Continued churn is ambiguous and therefore yields no preference. */
	for (unsigned int i = 0; i < 2 && source == SP_SOURCE_RETRY &&
	    !sp_xfailed; i++)
		source = read_source(portal, &snapshot);
	portal->dirty = source == SP_SOURCE_RETRY;
	(void)sp_commit(&portal->value,
	    source == SP_SOURCE_VALID ? &snapshot : NULL);
	bool malformed = source == SP_SOURCE_INVALID;
	/* Diagnose one malformed run; repeated reads do not flood stderr. */
	if (malformed && !portal->malformed)
		fputs("fvwm-settings-portal: invalid XSETTINGS source\n", stderr);
	portal->malformed = malformed;
	if (sp_xfailed)
		portal->failed = true;
}

static bool
text_valid(const char *text, size_t maximum)
{
	size_t length = text ? strnlen(text, maximum + 1) : maximum + 1;

	if (length > maximum || !dbus_validate_utf8(text, NULL))
		return false;
	for (size_t i = 0; i < length; i++)
		if ((unsigned char)text[i] < 0x20 || text[i] == 0x7f)
			return false;
	return true;
}

/* Keep the same narrow inherited local-address admission as session_bus.c.
 * No autolaunch, fallback address, bus startup or activation-env mutation. */
static bool
local_address(const char *address)
{
	DBusAddressEntry **entries = NULL;
	DBusError error;
	const char *field, *comma, *path, *abstract;
	int count = 0;
	bool valid = false;

	if (!address || strnlen(address, 2049) > 2048 ||
	    strncmp(address, "unix:", 5) != 0 || strchr(address, ';'))
		return false;
	field = address + 5;
	if (strncmp(field, "path=", 5) != 0 &&
	    strncmp(field, "abstract=", 9) != 0)
		return false;
	comma = strchr(field, ',');
	if (comma && (strncmp(comma, ",guid=", 6) != 0 ||
	    strchr(comma + 1, ',')))
		return false;
	dbus_error_init(&error);
	if (dbus_parse_address(address, &entries, &count, &error) && count == 1 &&
	    strcmp(dbus_address_entry_get_method(entries[0]), "unix") == 0)
	{
		path = dbus_address_entry_get_value(entries[0], "path");
		abstract = dbus_address_entry_get_value(entries[0], "abstract");
		valid = (path && !abstract && path[0] == '/' &&
		    text_valid(path, 1024)) ||
		    (abstract && !path && abstract[0] && text_valid(abstract, 1024));
	}
	if (entries)
		dbus_address_entries_free(entries);
	dbus_error_free(&error);
	return valid;
}

static void
send_message(struct portal *portal, DBusMessage *message)
{
	char *wire = NULL;
	int length = 0;
	long queued = dbus_connection_get_outgoing_size(portal->bus);

	/* pop_message bypasses automatic Peer replies. Every outgoing message
	 * passes the same finite budget; no second unbounded reply queue. */
	if (!message || queued < 0 ||
	    !dbus_message_marshal(message, &wire, &length) || length <= 0 ||
	    length > SP_OUTPUT_MAX || queued > SP_OUTPUT_MAX - length ||
	    !dbus_connection_send(portal->bus, message, NULL))
		portal->failed = true;
	if (wire)
		dbus_free(wire);
	if (message)
		dbus_message_unref(message);
}

static void
send_reply(struct portal *portal, DBusMessage *request, DBusMessage *reply)
{
	if (dbus_message_get_no_reply(request))
	{
		if (reply)
			dbus_message_unref(reply);
		return;
	}
	send_message(portal, reply);
}

static void
send_error(struct portal *portal, DBusMessage *request,
	const char *name, const char *detail)
{
	if (!dbus_message_get_no_reply(request))
		send_message(portal, dbus_message_new_error(request, name, detail));
}

static bool
append_variant(DBusMessageIter *iter, uint32_t value)
{
	DBusMessageIter variant;
	dbus_uint32_t number = value;

	if (!dbus_message_iter_open_container(iter, DBUS_TYPE_VARIANT, "u",
	    &variant))
		return false;
	if (dbus_message_iter_append_basic(&variant, DBUS_TYPE_UINT32, &number) &&
	    dbus_message_iter_close_container(iter, &variant))
		return true;
	dbus_message_iter_abandon_container(iter, &variant);
	return false;
}

static bool
append_setting(DBusMessageIter *iter, const char *key, uint32_t value)
{
	DBusMessageIter entry;

	if (!dbus_message_iter_open_container(iter, DBUS_TYPE_DICT_ENTRY, NULL,
	    &entry))
		return false;
	if (dbus_message_iter_append_basic(&entry, DBUS_TYPE_STRING, &key) &&
	    append_variant(&entry, value) &&
	    dbus_message_iter_close_container(iter, &entry))
		return true;
	dbus_message_iter_abandon_container(iter, &entry);
	return false;
}

static bool
append_namespace(DBusMessageIter *iter, uint32_t value)
{
	DBusMessageIter entry, settings;
	const char *name = SP_NAMESPACE;

	if (!dbus_message_iter_open_container(iter, DBUS_TYPE_DICT_ENTRY, NULL,
	    &entry))
		return false;
	if (!dbus_message_iter_append_basic(&entry, DBUS_TYPE_STRING, &name) ||
	    !dbus_message_iter_open_container(&entry, DBUS_TYPE_ARRAY, "{sv}",
	    &settings))
		goto fail_entry;
	if (!append_setting(&settings, SP_KEY, value) ||
	    !dbus_message_iter_close_container(&entry, &settings))
	{
		dbus_message_iter_abandon_container(&entry, &settings);
		goto fail_entry;
	}
	if (dbus_message_iter_close_container(iter, &entry))
		return true;
fail_entry:
	dbus_message_iter_abandon_container(iter, &entry);
	return false;
}

static void
reply_variant(struct portal *portal, DBusMessage *request, uint32_t value)
{
	DBusMessage *reply = dbus_message_new_method_return(request);
	DBusMessageIter iter;

	if (reply)
	{
		dbus_message_iter_init_append(reply, &iter);
		if (!append_variant(&iter, value))
		{
			dbus_message_unref(reply);
			reply = NULL;
		}
	}
	send_reply(portal, request, reply);
}

static void
read_all(struct portal *portal, DBusMessage *request)
{
	DBusMessageIter iter, array;
	const char *patterns[SP_PATTERNS_MAX];
	size_t count = 0;

	if (!dbus_message_has_signature(request, "as") ||
	    !dbus_message_iter_init(request, &iter))
		goto invalid;
	dbus_message_iter_recurse(&iter, &array);
	while (dbus_message_iter_get_arg_type(&array) != DBUS_TYPE_INVALID)
	{
		const char *pattern;

		dbus_message_iter_get_basic(&array, &pattern);
		if (count == SP_PATTERNS_MAX || !sp_pattern_valid(pattern))
			goto invalid;
		patterns[count++] = pattern;
		dbus_message_iter_next(&array);
	}
	bool selected = sp_namespace_matches(patterns, count);
	DBusMessage *reply = dbus_message_new_method_return(request);
	if (reply)
	{
		dbus_message_iter_init_append(reply, &iter);
		if (!dbus_message_iter_open_container(&iter, DBUS_TYPE_ARRAY,
		    "{sa{sv}}", &array))
			goto failed;
		if ((selected && !append_namespace(&array, portal->value)) ||
		    !dbus_message_iter_close_container(&iter, &array))
		{
			dbus_message_iter_abandon_container(&iter, &array);
			goto failed;
		}
	}
	send_reply(portal, request, reply);
	return;
failed:
	dbus_message_unref(reply);
	portal->failed = true;
	return;
invalid:
	send_error(portal, request, DBUS_ERROR_INVALID_ARGS,
	    "ReadAll requires at most 16 namespace patterns of 128 bytes");
}

static void
read_setting(struct portal *portal, DBusMessage *request)
{
	const char *name, *key;

	if (!dbus_message_has_signature(request, "ss") ||
	    !dbus_message_get_args(request, NULL, DBUS_TYPE_STRING, &name,
	    DBUS_TYPE_STRING, &key, DBUS_TYPE_INVALID))
	{
		send_error(portal, request, DBUS_ERROR_INVALID_ARGS,
		    "Read requires namespace and key strings");
		return;
	}
	if (strcmp(name, SP_NAMESPACE) || strcmp(key, SP_KEY))
		send_error(portal, request, SP_NOT_FOUND, "Setting not found");
	else
		reply_variant(portal, request, portal->value);
}

static void
properties(struct portal *portal, DBusMessage *request, const char *member)
{
	const char *interface, *property = NULL;
	bool get = strcmp(member, "Get") == 0;
	bool all = strcmp(member, "GetAll") == 0;
	bool set = strcmp(member, "Set") == 0;
	DBusMessageIter iter, array;
	DBusMessage *reply;

	if (!get && !all && !set)
	{
		send_error(portal, request, DBUS_ERROR_UNKNOWN_METHOD,
		    "Unknown Properties method");
		return;
	}
	if (!dbus_message_has_signature(request, all ? "s" : set ? "ssv" :
	    "ss") || !dbus_message_iter_init(request, &iter))
		goto invalid;
	dbus_message_iter_get_basic(&iter, &interface);
	if (!all)
	{
		dbus_message_iter_next(&iter);
		dbus_message_iter_get_basic(&iter, &property);
	}
	bool settings_interface = strcmp(interface, SP_INTERFACE) == 0;
	if (!settings_interface &&
	    strcmp(interface, DBUS_INTERFACE_PROPERTIES) != 0 &&
	    strcmp(interface, DBUS_INTERFACE_INTROSPECTABLE) != 0)
	{
		send_error(portal, request, DBUS_ERROR_UNKNOWN_INTERFACE,
		    "Unknown property interface");
		return;
	}
	if (!all && (!settings_interface || strcmp(property, "version")))
	{
		send_error(portal, request, DBUS_ERROR_UNKNOWN_PROPERTY,
		    "Unknown property");
		return;
	}
	if (set)
	{
		send_error(portal, request, DBUS_ERROR_PROPERTY_READ_ONLY,
		    "version is read-only");
		return;
	}
	if (get)
	{
		reply_variant(portal, request, 1);
		return;
	}
	reply = dbus_message_new_method_return(request);
	if (reply)
	{
		dbus_message_iter_init_append(reply, &iter);
		if (!dbus_message_iter_open_container(&iter, DBUS_TYPE_ARRAY,
		    "{sv}", &array))
			goto failed;
		if ((settings_interface && !append_setting(&array, "version", 1)) ||
		    !dbus_message_iter_close_container(&iter, &array))
		{
			dbus_message_iter_abandon_container(&iter, &array);
			goto failed;
		}
	}
	send_reply(portal, request, reply);
	return;
failed:
	dbus_message_unref(reply);
	portal->failed = true;
	return;
invalid:
	send_error(portal, request, DBUS_ERROR_INVALID_ARGS,
	    "Invalid Properties signature");
}

static void
dispatch_message(struct portal *portal, DBusMessage *message)
{
	if (dbus_message_is_signal(message, DBUS_INTERFACE_LOCAL, "Disconnected"))
	{
		portal->failed = true;
		return;
	}
	if (dbus_message_is_signal(message, DBUS_INTERFACE_DBUS, "NameLost") &&
	    dbus_message_has_sender(message, DBUS_SERVICE_DBUS) &&
	    dbus_message_has_path(message, DBUS_PATH_DBUS))
	{
		const char *name = NULL;

		if (!dbus_message_get_args(message, NULL, DBUS_TYPE_STRING, &name,
		    DBUS_TYPE_INVALID) || strcmp(name, SP_BUS_NAME) == 0)
			portal->failed = true;
		return;
	}
	if (dbus_message_get_type(message) != DBUS_MESSAGE_TYPE_METHOD_CALL)
		return;
	if (dbus_message_contains_unix_fds(message))
	{
		send_error(portal, message, DBUS_ERROR_INVALID_ARGS,
		    "File descriptors are not supported");
		return;
	}
	const char *path = dbus_message_get_path(message);
	const char *interface = dbus_message_get_interface(message);
	const char *member = dbus_message_get_member(message);
	if (!path || strcmp(path, SP_PATH))
	{
		send_error(portal, message, DBUS_ERROR_UNKNOWN_OBJECT,
		    "Unknown object");
		return;
	}
	/* Commit the complete latest source before any method reply. */
	refresh_source(portal);
	if (portal->failed)
		return;
	if (interface && member &&
	    strcmp(interface, DBUS_INTERFACE_INTROSPECTABLE) == 0 &&
	    strcmp(member, "Introspect") == 0)
	{
		if (!dbus_message_has_signature(message, ""))
		{
			send_error(portal, message, DBUS_ERROR_INVALID_ARGS,
			    "Introspect requires no arguments");
			return;
		}
		DBusMessage *reply = dbus_message_new_method_return(message);
		const char *xml = introspection;
		if (reply && !dbus_message_append_args(reply, DBUS_TYPE_STRING,
		    &xml, DBUS_TYPE_INVALID))
		{
			dbus_message_unref(reply);
			reply = NULL;
		}
		send_reply(portal, message, reply);
		return;
	}
	if (interface && member &&
	    strcmp(interface, DBUS_INTERFACE_PROPERTIES) == 0)
	{
		properties(portal, message, member);
		return;
	}
	if (interface && member && strcmp(interface, SP_INTERFACE) == 0)
	{
		if (strcmp(member, "ReadAll") == 0)
		{
			read_all(portal, message);
			return;
		}
		if (strcmp(member, "Read") == 0)
		{
			read_setting(portal, message);
			return;
		}
	}
	send_error(portal, message, DBUS_ERROR_UNKNOWN_METHOD, "Unknown method");
}

static void
signal_changed(struct portal *portal)
{
	DBusMessageIter iter;
	const char *name = SP_NAMESPACE, *key = SP_KEY;

	/* Retain only the newest public value while replies are pending. */
	if (portal->value == portal->announced ||
	    dbus_connection_get_outgoing_size(portal->bus) != 0)
		return;
	DBusMessage *message = dbus_message_new_signal(SP_PATH, SP_INTERFACE,
	    "SettingChanged");
	if (message)
	{
		dbus_message_iter_init_append(message, &iter);
		if (!dbus_message_iter_append_basic(&iter, DBUS_TYPE_STRING, &name) ||
		    !dbus_message_iter_append_basic(&iter, DBUS_TYPE_STRING, &key) ||
		    !append_variant(&iter, portal->value))
		{
			dbus_message_unref(message);
			message = NULL;
		}
	}
	send_message(portal, message);
	if (!portal->failed)
		portal->announced = portal->value;
}

static void
process_x_event(struct portal *portal)
{
	XEvent event;

	XNextEvent(portal->display, &event);
	if ((event.type == PropertyNotify &&
	    event.xproperty.window == portal->owner &&
	    event.xproperty.atom == portal->settings) ||
	    (event.type == DestroyNotify &&
	    event.xdestroywindow.window == portal->owner) ||
	    (event.type == ClientMessage && event.xclient.window == portal->root &&
	    event.xclient.message_type == portal->manager &&
	    event.xclient.format == 32 &&
	    (Atom)event.xclient.data.l[1] == portal->selection))
		portal->dirty = true;
}

static void
run_portal(struct portal *portal)
{
	int bus_fd = -1;

	if (!dbus_connection_get_unix_fd(portal->bus, &bus_fd) || bus_fd < 0)
	{
		portal->failed = true;
		return;
	}
	while (!portal->failed)
	{
		int64_t turn = monotonic_ns(), now = turn;
		unsigned int count = 0;
		bool prefer_x = true;

		if (turn < 0 || !dbus_connection_read_write(portal->bus, 0))
			break;
		/* Alternate surfaces so an X flood cannot starve bus calls.
		 * The combined cap counts both X events and bus messages. */
		while (count < SP_TURN_MESSAGES && !portal->failed)
		{
			DBusMessage *message = NULL;
			bool have_x = XPending(portal->display) != 0;

			now = monotonic_ns();
			if (now < 0 || now - turn >= SP_TURN_NS)
				break;
			if (!have_x || !prefer_x)
				message = dbus_connection_pop_message(portal->bus);
			if (message)
			{
				dispatch_message(portal, message);
				dbus_message_unref(message);
				prefer_x = true;
			}
			else if (have_x)
			{
				process_x_event(portal);
				prefer_x = false;
			}
			else
				break;
			count++;
		}
		now = monotonic_ns();
		if (now < 0 || sp_xfailed)
			break;
		if (portal->dirty || now >= portal->next_source)
		{
			refresh_source(portal);
			portal->next_source = now + SP_SOURCE_POLL_NS;
		}
		if (portal->failed)
			break;
		signal_changed(portal);
		if (portal->failed ||
		    !dbus_connection_read_write(portal->bus, 0))
			break;
		struct pollfd descriptors[2] = {
			{ConnectionNumber(portal->display), POLLIN, 0},
			{bus_fd, POLLIN, 0}
		};
		if (dbus_connection_get_outgoing_size(portal->bus))
			descriptors[1].events |= POLLOUT;
		int timeout = portal->dirty || XPending(portal->display) ||
		    dbus_connection_get_dispatch_status(portal->bus) ==
		    DBUS_DISPATCH_DATA_REMAINS ? 0 : 50;
		if (poll(descriptors, 2, timeout) < 0 && errno != EINTR)
			break;
		for (size_t i = 0; i < 2; i++)
			if (descriptors[i].revents & (POLLERR | POLLHUP | POLLNVAL))
				portal->failed = true;
	}
	fputs("fvwm-settings-portal: connection retired\n", stderr);
}

int
main(int argc, char **argv)
{
	struct portal portal = {0};
	DBusError error;
	const char *address = getenv("DBUS_SESSION_BUS_ADDRESS");
	const char *detail = "invalid arguments";
	char selection_name[64];
	(void)argv;

	dbus_error_init(&error);
	if (argc != 1)
		goto out;
	detail = "explicit local session bus address required";
	if (!local_address(address))
		goto out;
	detail = "startup guard failed";
	if (!module_startup_begin(MODULE_STARTUP_SECONDS))
		goto out;
	detail = "X display unavailable";
	portal.display = XOpenDisplay(NULL);
	if (!portal.display)
		goto out;
	XSetErrorHandler(portal_x_error);
	XSetIOErrorHandler(portal_x_io_error);
	portal.root = DefaultRootWindow(portal.display);
	snprintf(selection_name, sizeof(selection_name), "_XSETTINGS_S%d",
	    DefaultScreen(portal.display));
	portal.selection = XInternAtom(portal.display, selection_name, False);
	portal.settings = XInternAtom(portal.display, "_XSETTINGS_SETTINGS", False);
	portal.manager = XInternAtom(portal.display, "MANAGER", False);
	XSelectInput(portal.display, portal.root, StructureNotifyMask);
	refresh_source(&portal);
	portal.announced = portal.value;
	detail = "X initialization failed";
	if (portal.failed || sp_xfailed)
		goto out;
	detail = "session bus connection failed";
	portal.bus = dbus_connection_open_private(address, &error);
	if (!portal.bus)
		goto out;
	dbus_connection_set_exit_on_disconnect(portal.bus, FALSE);
	dbus_connection_set_max_message_size(portal.bus, SP_MESSAGE_MAX);
	dbus_connection_set_max_received_size(portal.bus, SP_RECEIVE_MAX);
	/* Positive limits keep reads enabled; every admitted FD is rejected. */
	dbus_connection_set_max_message_unix_fds(portal.bus, 1);
	dbus_connection_set_max_received_unix_fds(portal.bus, 1);
	detail = "session bus registration failed";
	if (!dbus_bus_register(portal.bus, &error))
		goto out;
	detail = "backend name already owned or unavailable";
	if (dbus_bus_request_name(portal.bus, SP_BUS_NAME,
	    DBUS_NAME_FLAG_DO_NOT_QUEUE, &error) !=
	    DBUS_REQUEST_NAME_REPLY_PRIMARY_OWNER)
		goto out;
	module_startup_finish();
	run_portal(&portal);
	detail = NULL;
out:
	module_startup_finish();
	if (detail)
		fprintf(stderr, "fvwm-settings-portal: %s\n", detail);
	if (portal.bus)
	{
		/* The private connection releases only our own backend name. */
		dbus_connection_close(portal.bus);
		dbus_connection_unref(portal.bus);
	}
	if (portal.display)
		XCloseDisplay(portal.display);
	dbus_error_free(&error);
	return argc == 1 ? 1 : 2;
}
