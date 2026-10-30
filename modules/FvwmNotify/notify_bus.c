/* This program is free software; you can redistribute it and/or modify it
 * under the GNU General Public License, version 2 or any later version. */
#include "config.h"
#include "notify_bus.h"
#include <dbus/dbus.h>
#include <poll.h>
#include <stdio.h>
#include <string.h>

#define NOTIFY_NAME "org.freedesktop.Notifications"
#define NOTIFY_PATH "/org/freedesktop/Notifications"
#define NOTIFY_TURN_MAX 32
#define NOTIFY_HINT_MAX 32
#define NOTIFY_HINT_DEPTH_MAX 8

bool notify_bus_address_valid(const char *address)
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
	if (dbus_parse_address(address, &entries, &count, &error) && count == 1 &&
	    !strcmp(dbus_address_entry_get_method(entries[0]), "unix"))
	{
		path = dbus_address_entry_get_value(entries[0], "path");
		abstract = dbus_address_entry_get_value(entries[0], "abstract");
		valid = (path && !abstract && path[0] == '/' &&
			notify_model_text(path, 1024, false)) ||
		    (abstract && !path && abstract[0] &&
			notify_model_text(abstract, 1024, false));
	}
	if (entries)
		dbus_address_entries_free(entries);
	dbus_error_free(&error);
	return valid;
}

/* This bounded walk also covers ignored hints: no caller-controlled nesting or
 * container fan-out turns an ignored extension into unbounded dispatch work. */
static bool hint_value_valid(DBusMessageIter *value, unsigned int depth,
	unsigned int *remaining)
{
	if (depth > NOTIFY_HINT_DEPTH_MAX || !*remaining)
		return false;
	while (dbus_message_iter_get_arg_type(value) != DBUS_TYPE_INVALID)
	{
		int type = dbus_message_iter_get_arg_type(value);
		if (!*remaining)
			return false;
		(*remaining)--;
		if (type == DBUS_TYPE_UNIX_FD)
			return false;
		if (dbus_type_is_container(type))
		{
			DBusMessageIter nested;
			dbus_message_iter_recurse(value, &nested);
			if (!hint_value_valid(&nested, depth + 1, remaining))
				return false;
		}
		else if (type == DBUS_TYPE_STRING || type == DBUS_TYPE_OBJECT_PATH ||
		    type == DBUS_TYPE_SIGNATURE)
		{
			const char *text;
			dbus_message_iter_get_basic(value, &text);
			if (!notify_model_text(text, NOTIFY_BODY_MAX, true))
				return false;
		}
		dbus_message_iter_next(value);
	}
	return true;
}

static bool take_text(DBusMessageIter *iter, char *output, unsigned int maximum,
	bool multiline)
{
	const char *text;
	if (dbus_message_iter_get_arg_type(iter) != DBUS_TYPE_STRING)
		return false;
	dbus_message_iter_get_basic(iter, &text);
	if (!notify_model_text(text, maximum, multiline))
		return false;
	memcpy(output, text, strlen(text) + 1);
	dbus_message_iter_next(iter);
	return true;
}

bool notify_bus_parse_notify(DBusMessage *message, notify_request *request)
{
	DBusMessageIter iter, array;
	notify_request parsed = {0};
	const char *sender = dbus_message_get_sender(message);
	char ignored_icon[1025];
	unsigned int pairs = 0, hints = 0, remaining = 1024;
	char *encoded = NULL;
	int wire_length = 0;
	bool wire_valid;

	wire_valid = dbus_message_marshal(message, &encoded, &wire_length) &&
	    wire_length > 0 && wire_length <= NOTIFY_WIRE_MAX;
	dbus_free(encoded);

	memset(request, 0, sizeof(*request));
	if (!wire_valid ||
	    !dbus_message_has_signature(message, "susssasa{sv}i") ||
	    !sender || sender[0] != ':' ||
	    !notify_model_text(sender, NOTIFY_OWNER_MAX, false) ||
	    !dbus_validate_bus_name(sender, NULL) ||
	    !dbus_message_iter_init(message, &iter))
		return false;
	strcpy(parsed.record.owner, sender);
	parsed.record.urgency = 1;
	if (!take_text(&iter, parsed.record.app, NOTIFY_APP_MAX, false))
		return false;
	dbus_message_iter_get_basic(&iter, &parsed.replaces);
	dbus_message_iter_next(&iter);
	if (!take_text(&iter, ignored_icon, sizeof(ignored_icon) - 1, false) ||
	    !take_text(&iter, parsed.record.summary, NOTIFY_SUMMARY_MAX, false) ||
	    !take_text(&iter, parsed.record.body, NOTIFY_BODY_MAX, true))
		return false;
	dbus_message_iter_recurse(&iter, &array);
	while (dbus_message_iter_get_arg_type(&array) != DBUS_TYPE_INVALID)
	{
		if (pairs == NOTIFY_ACTION_MAX ||
		    !take_text(&array, parsed.record.actions[pairs].key,
			NOTIFY_ACTION_KEY_MAX, false) ||
		    !take_text(&array, parsed.record.actions[pairs].label,
			NOTIFY_ACTION_LABEL_MAX, false))
			return false;
		pairs++;
	}
	parsed.record.action_count = pairs;
	dbus_message_iter_next(&iter);
	dbus_message_iter_recurse(&iter, &array);
	while (dbus_message_iter_get_arg_type(&array) != DBUS_TYPE_INVALID)
	{
		DBusMessageIter entry, value, checked;
		const char *key;
		if (++hints > NOTIFY_HINT_MAX)
			return false;
		dbus_message_iter_recurse(&array, &entry);
		dbus_message_iter_get_basic(&entry, &key);
		if (!notify_model_text(key, 128, false))
			return false;
		dbus_message_iter_next(&entry);
		dbus_message_iter_recurse(&entry, &value);
		checked = value;
		if (!hint_value_valid(&checked, 0, &remaining))
			return false;
		if (!strcmp(key, "urgency"))
		{
			unsigned char urgency;
			if (dbus_message_iter_get_arg_type(&value) != DBUS_TYPE_BYTE)
				return false;
			dbus_message_iter_get_basic(&value, &urgency);
			if (urgency > 2)
				return false;
			parsed.record.urgency = urgency;
		}
		dbus_message_iter_next(&array);
	}
	dbus_message_iter_next(&iter);
	dbus_message_iter_get_basic(&iter, &parsed.timeout);
	if (parsed.timeout < -1 || parsed.timeout > 86400000)
		return false;
	*request = parsed;
	return true;
}

static bool queue_message(notify_bus *bus, DBusMessage *message)
{
	char *encoded = NULL;
	int length = 0;
	bool sent = false;

	if (!message || bus->failed || !bus->connection)
		goto out;
	if (!dbus_message_marshal(message, &encoded, &length) ||
	    length < 0 || length > NOTIFY_WIRE_MAX ||
	    dbus_connection_get_outgoing_size(bus->connection) >
		NOTIFY_QUEUE_MAX - length)
		goto out;
	sent = dbus_connection_send(bus->connection, message, NULL);
out:
	dbus_free(encoded);
	if (message)
		dbus_message_unref(message);
	if (!sent)
		bus->failed = true;
	return sent;
}

static bool reply_message(notify_bus *bus, DBusMessage *call,
	DBusMessage *reply)
{
	if (dbus_message_get_no_reply(call))
	{
		if (reply)
			dbus_message_unref(reply);
		return true;
	}
	return queue_message(bus, reply);
}

static void reply_error(notify_bus *bus, DBusMessage *call, const char *name,
	const char *detail)
{
	reply_message(bus, call, dbus_message_new_error(call, name, detail));
}

bool notify_bus_close(notify_bus *bus, uint32_t id, uint32_t reason)
{
	notify_record retired;
	DBusMessage *signal;
	if (bus->failed || !notify_model_close(&bus->model, id, reason, &retired))
		return false;
	bus->dirty = true;
	signal = dbus_message_new_signal(NOTIFY_PATH, NOTIFY_NAME,
		"NotificationClosed");
	if (!signal || !dbus_message_set_destination(signal, retired.owner) ||
	    !dbus_message_append_args(signal, DBUS_TYPE_UINT32, &id,
		DBUS_TYPE_UINT32, &reason, DBUS_TYPE_INVALID))
	{
		if (signal)
			dbus_message_unref(signal);
		bus->failed = true;
		return false;
	}
	return queue_message(bus, signal);
}

bool notify_bus_action(notify_bus *bus, uint32_t id, unsigned int action)
{
	const notify_record *record = notify_model_find(&bus->model, id);
	DBusMessage *signal;
	const char *key;
	if (bus->failed || !record || action >= record->action_count)
		return false;
	key = record->actions[action].key;
	signal = dbus_message_new_signal(NOTIFY_PATH, NOTIFY_NAME,
		"ActionInvoked");
	if (!signal || !dbus_message_set_destination(signal, record->owner) ||
	    !dbus_message_append_args(signal, DBUS_TYPE_UINT32, &id,
		DBUS_TYPE_STRING, &key, DBUS_TYPE_INVALID))
	{
		if (signal)
			dbus_message_unref(signal);
		bus->failed = true;
		return false;
	}
	if (!queue_message(bus, signal))
		return false;
	return notify_bus_close(bus, id, 2);
}

void notify_bus_dismiss_all(notify_bus *bus)
{
	for (unsigned int i = 0; i < NOTIFY_ACTIVE_MAX &&
	    bus->model.count && !bus->failed; i++)
		notify_bus_close(bus, bus->model.active[0].id, 2);
}

void notify_bus_dnd(notify_bus *bus, bool enabled)
{
	if (bus->model.dnd == enabled)
		return;
	bus->model.dnd = enabled;
	bus->model.revision++;
	bus->dirty = true;
	if (enabled)
		for (unsigned int i = 0; i < NOTIFY_ACTIVE_MAX &&
		    bus->model.count && !bus->failed; i++)
			notify_bus_close(bus, bus->model.active[0].id, 4);
}

static void method(notify_bus *bus, DBusMessage *call, double now)
{
	DBusMessage *reply;
	const char *member = dbus_message_get_member(call);
	if (!dbus_message_has_path(call, NOTIFY_PATH))
	{
		reply_error(bus, call, DBUS_ERROR_UNKNOWN_OBJECT,
			"Unknown notification object");
		return;
	}
	if (!dbus_message_has_interface(call, NOTIFY_NAME) || !member)
	{
		reply_error(bus, call, DBUS_ERROR_UNKNOWN_METHOD,
			"Unknown notification interface or method");
		return;
	}
	if (!strcmp(member, "Notify"))
	{
		notify_request request;
		uint32_t id;
		notify_result result;
		if (!notify_bus_parse_notify(call, &request))
		{
			reply_error(bus, call, DBUS_ERROR_INVALID_ARGS,
				"Invalid or oversized notification");
			return;
		}
		result = notify_model_put(&bus->model, &request, now, &id);
		if (result != NOTIFY_ACCEPTED)
		{
			const char *error = result == NOTIFY_UNKNOWN_ID ?
			    DBUS_ERROR_INVALID_ARGS : result == NOTIFY_INVALID ?
			    DBUS_ERROR_INVALID_ARGS : DBUS_ERROR_LIMITS_EXCEEDED;
			reply_error(bus, call, error,
				"Notification refused by bounded ID policy");
			return;
		}
		bus->dirty = true;
		reply = dbus_message_new_method_return(call);
		if (!reply || !dbus_message_append_args(reply, DBUS_TYPE_UINT32,
		    &id, DBUS_TYPE_INVALID))
		{
			if (reply)
				dbus_message_unref(reply);
			bus->failed = true;
			return;
		}
		if (reply_message(bus, call, reply) && bus->model.dnd)
			notify_bus_close(bus, id, 4);
		return;
	}
	if (!strcmp(member, "CloseNotification"))
	{
		uint32_t id;
		const notify_record *record;
		if (!dbus_message_has_signature(call, "u") ||
		    !dbus_message_get_args(call, NULL, DBUS_TYPE_UINT32, &id,
			DBUS_TYPE_INVALID))
		{
			reply_error(bus, call, DBUS_ERROR_INVALID_ARGS,
				"CloseNotification requires one uint32 ID");
			return;
		}
		record = notify_model_find(&bus->model, id);
		if (!record)
		{
			reply_error(bus, call, DBUS_ERROR_INVALID_ARGS,
				"Notification ID is absent");
			return;
		}
		if (notify_bus_close(bus, id, 3))
			reply_message(bus, call, dbus_message_new_method_return(call));
		return;
	}
	if ((!strcmp(member, "GetCapabilities") ||
	     !strcmp(member, "GetServerInformation")) &&
	    !dbus_message_has_signature(call, ""))
	{
		reply_error(bus, call, DBUS_ERROR_INVALID_ARGS,
			"This method takes no arguments");
		return;
	}
	if (!strcmp(member, "GetCapabilities"))
	{
		const char *capabilities[] = {"body", "actions"};
		DBusMessageIter root, array;
		reply = dbus_message_new_method_return(call);
		if (!reply)
			goto no_memory;
		dbus_message_iter_init_append(reply, &root);
		if (!dbus_message_iter_open_container(&root, DBUS_TYPE_ARRAY,
		    DBUS_TYPE_STRING_AS_STRING, &array))
			goto no_memory;
		for (unsigned int i = 0; i < 2; i++)
			if (!dbus_message_iter_append_basic(&array, DBUS_TYPE_STRING,
			    &capabilities[i]))
				goto no_memory;
		if (!dbus_message_iter_close_container(&root, &array))
			goto no_memory;
		reply_message(bus, call, reply);
		return;
	}
	if (!strcmp(member, "GetServerInformation"))
	{
		const char *name = "FvwmNotify", *vendor = "FVWM-X";
		const char *version = VERSION, *spec = "1.3";
		reply = dbus_message_new_method_return(call);
		if (!reply || !dbus_message_append_args(reply,
		    DBUS_TYPE_STRING, &name, DBUS_TYPE_STRING, &vendor,
		    DBUS_TYPE_STRING, &version, DBUS_TYPE_STRING, &spec,
		    DBUS_TYPE_INVALID))
			goto no_memory;
		reply_message(bus, call, reply);
		return;
	}
	reply_error(bus, call, DBUS_ERROR_UNKNOWN_METHOD,
		"Unknown notification method");
	return;
no_memory:
	if (reply)
		dbus_message_unref(reply);
	bus->failed = true;
}

bool notify_bus_open(notify_bus *bus, const char *address)
{
	DBusError error;
	int result;

	if (bus->connection || !notify_bus_address_valid(address))
		return false;
	dbus_error_init(&error);
	bus->connection = dbus_connection_open_private(address, &error);
	if (!bus->connection)
		goto failed;
	dbus_connection_set_exit_on_disconnect(bus->connection, FALSE);
	dbus_connection_set_max_message_size(bus->connection, NOTIFY_WIRE_MAX);
	dbus_connection_set_max_received_size(bus->connection, NOTIFY_QUEUE_MAX);
	dbus_connection_set_max_message_unix_fds(bus->connection, 0);
	/* Aggregate zero disables all reads, including messages with no FDs.
	 * As in the transient client, one permits progress while the per-message
	 * zero limit continues to refuse descriptor-bearing messages. */
	dbus_connection_set_max_received_unix_fds(bus->connection, 1);
	if (!dbus_bus_register(bus->connection, &error))
		goto failed;
	result = dbus_bus_request_name(bus->connection, NOTIFY_NAME,
		DBUS_NAME_FLAG_DO_NOT_QUEUE, &error);
	if (dbus_error_is_set(&error) ||
	    result != DBUS_REQUEST_NAME_REPLY_PRIMARY_OWNER)
		goto failed;
	dbus_error_free(&error);
	bus->failed = false;
	return true;
failed:
	dbus_error_free(&error);
	notify_bus_destroy(bus);
	bus->failed = true;
	return false;
}

void notify_bus_destroy(notify_bus *bus)
{
	if (bus->connection)
	{
		dbus_connection_close(bus->connection);
		dbus_connection_unref(bus->connection);
		bus->connection = NULL;
	}
	memset(&bus->model, 0, sizeof(bus->model));
	bus->dirty = false;
	bus->pending = false;
}

int notify_bus_fd(const notify_bus *bus)
{
	int descriptor = -1;
	if (bus->connection)
		dbus_connection_get_unix_fd(bus->connection, &descriptor);
	return descriptor;
}

short notify_bus_events(const notify_bus *bus)
{
	return POLLIN | (bus->connection &&
	    dbus_connection_get_outgoing_size(bus->connection) ? POLLOUT : 0);
}

bool notify_bus_service(notify_bus *bus, double now)
{
	if (!bus->connection || bus->failed ||
	    !dbus_connection_read_write(bus->connection, 0))
		return false;
	for (unsigned int i = 0; i < NOTIFY_TURN_MAX && !bus->failed; i++)
	{
		DBusMessage *message = dbus_connection_pop_message(bus->connection);
		if (!message)
			break;
		if (dbus_message_get_type(message) == DBUS_MESSAGE_TYPE_METHOD_CALL)
			method(bus, message, now);
		else if (dbus_message_is_signal(message, DBUS_INTERFACE_DBUS,
		    "NameLost"))
		{
			const char *name = NULL;
			if (dbus_message_get_args(message, NULL, DBUS_TYPE_STRING,
			    &name, DBUS_TYPE_INVALID) && !strcmp(name, NOTIFY_NAME))
				bus->failed = true;
		}
		dbus_message_unref(message);
	}
	for (unsigned int i = 0; i < NOTIFY_ACTIVE_MAX && !bus->failed; i++)
	{
		uint32_t id = notify_model_expired(&bus->model, now);
		if (!id)
			break;
		notify_bus_close(bus, id, 1);
	}
	if (!bus->failed)
		dbus_connection_read_write(bus->connection, 0);
	bus->pending = dbus_connection_get_dispatch_status(bus->connection) ==
	    DBUS_DISPATCH_DATA_REMAINS;
	return !bus->failed && dbus_connection_get_is_connected(bus->connection);
}
