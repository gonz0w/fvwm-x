/* This program is free software; you can redistribute it and/or modify
 * it under the GNU General Public License, version 2 or any later version.
 *
 * Independent persistent notification peer for the opt-in native-X fixture.
 * Only a private caller-owned bus and fixed notification methods are accepted.
 * The harness retains an external watchdog for authentication/kernel calls.
 */
#include "config.h"
#include <dbus/dbus.h>
#include "libs/cJSON.h"
#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#define NAME "org.freedesktop.Notifications"
#define PATH "/org/freedesktop/Notifications"
#define LINE_MAXIMUM 20000
#define COMMAND_MAXIMUM 512
#define BURST_MAXIMUM 256

struct burst_state
{
	dbus_uint32_t serials[BURST_MAXIMUM];
	unsigned int count;
};

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
	if (!path || path[0] != '/' || strnlen(path, sizeof(parent)) >=
	    sizeof(parent))
		goto out;
	strcpy(parent, path);
	slash = strrchr(parent, '/');
	if (!slash || slash == parent)
		goto out;
	*slash = '\0';
	if (lstat(parent, &st) || !S_ISDIR(st.st_mode) ||
	    st.st_uid != geteuid() || (st.st_mode & 077))
		goto out;
	if (lstat(path, &st) || !S_ISSOCK(st.st_mode) ||
	    st.st_uid != geteuid())
		goto out;
	valid = true;
out:
	if (entries)
		dbus_address_entries_free(entries);
	dbus_error_free(&error);
	return valid;
}

static const char *string_field(cJSON *object, const char *name,
	const char *fallback, size_t maximum)
{
	cJSON *value = cJSON_GetObjectItemCaseSensitive(object, name);
	const char *text = value ? (cJSON_IsString(value) ? value->valuestring :
		NULL) : fallback;

	if (!text || strnlen(text, maximum + 1) > maximum ||
	    !dbus_validate_utf8(text, NULL))
		return NULL;
	return text;
}

static bool number_field(cJSON *object, const char *name, double fallback,
	double minimum, double maximum, double *result)
{
	cJSON *value = cJSON_GetObjectItemCaseSensitive(object, name);
	double number = value ? (cJSON_IsNumber(value) ? value->valuedouble :
		NAN) : fallback;

	if (!isfinite(number) || number < minimum || number > maximum ||
	    (double)(int64_t)number != number)
		return false;
	*result = number;
	return true;
}

static bool append_notify(DBusMessage *message, cJSON *command)
{
	const char *app = string_field(command, "app", "FVWM-X fixture", 256);
	const char *icon = "";
	const char *summary = string_field(command, "summary", "Notice", 1024);
	const char *body = string_field(command, "body", "", 8192);
	cJSON *actions = cJSON_GetObjectItemCaseSensitive(command, "actions");
	double replacement, timeout, urgency;
	DBusMessageIter root, array, hints, entry, variant;
	dbus_uint32_t replaces;
	dbus_int32_t expires;
	unsigned char level;
	const char *hint = "urgency";

	if (!app || !summary || !body ||
	    !number_field(command, "replaces", 0, 0, UINT32_MAX, &replacement) ||
	    !number_field(command, "timeout", 0, INT32_MIN, INT32_MAX, &timeout) ||
	    !number_field(command, "urgency", 1, 0, 255, &urgency) ||
	    (actions && (!cJSON_IsArray(actions) ||
		cJSON_GetArraySize(actions) > 16)))
		return false;
	replaces = (dbus_uint32_t)replacement;
	expires = (dbus_int32_t)timeout;
	level = (unsigned char)urgency;
	dbus_message_iter_init_append(message, &root);
	if (!dbus_message_iter_append_basic(&root, DBUS_TYPE_STRING, &app) ||
	    !dbus_message_iter_append_basic(&root, DBUS_TYPE_UINT32, &replaces) ||
	    !dbus_message_iter_append_basic(&root, DBUS_TYPE_STRING, &icon) ||
	    !dbus_message_iter_append_basic(&root, DBUS_TYPE_STRING, &summary) ||
	    !dbus_message_iter_append_basic(&root, DBUS_TYPE_STRING, &body) ||
	    !dbus_message_iter_open_container(&root, DBUS_TYPE_ARRAY, "s", &array))
		return false;
	for (int i = 0; actions && i < cJSON_GetArraySize(actions); i++)
	{
		cJSON *item = cJSON_GetArrayItem(actions, i);
		const char *text = cJSON_IsString(item) ? item->valuestring : NULL;

		if (!text || strlen(text) > 1024 || !dbus_validate_utf8(text, NULL) ||
		    !dbus_message_iter_append_basic(&array, DBUS_TYPE_STRING, &text))
			return false;
	}
	if (!dbus_message_iter_close_container(&root, &array) ||
	    !dbus_message_iter_open_container(&root, DBUS_TYPE_ARRAY, "{sv}",
		&hints) ||
	    !dbus_message_iter_open_container(&hints, DBUS_TYPE_DICT_ENTRY, NULL,
		&entry) ||
	    !dbus_message_iter_append_basic(&entry, DBUS_TYPE_STRING, &hint) ||
	    !dbus_message_iter_open_container(&entry, DBUS_TYPE_VARIANT, "y",
		&variant) ||
	    !dbus_message_iter_append_basic(&variant, DBUS_TYPE_BYTE, &level) ||
	    !dbus_message_iter_close_container(&entry, &variant) ||
	    !dbus_message_iter_close_container(&hints, &entry) ||
	    !dbus_message_iter_close_container(&root, &hints) ||
	    !dbus_message_iter_append_basic(&root, DBUS_TYPE_INT32, &expires))
		return false;
	return true;
}

static cJSON *collect_signals(DBusConnection *connection)
{
	cJSON *result = cJSON_CreateArray();

	dbus_connection_read_write(connection, 0);
	for (unsigned int i = 0; i < 64; i++)
	{
		DBusMessage *message = dbus_connection_pop_message(connection);
		cJSON *record;
		dbus_uint32_t id, reason;
		const char *key;

		if (!message)
			break;
		if (dbus_message_is_signal(message, NAME, "NotificationClosed") &&
		    dbus_message_get_args(message, NULL, DBUS_TYPE_UINT32, &id,
			DBUS_TYPE_UINT32, &reason, DBUS_TYPE_INVALID))
		{
			record = cJSON_CreateObject();
			cJSON_AddStringToObject(record, "signal", "closed");
			cJSON_AddNumberToObject(record, "id", id);
			cJSON_AddNumberToObject(record, "reason", reason);
			cJSON_AddItemToArray(result, record);
		}
		else if (dbus_message_is_signal(message, NAME, "ActionInvoked") &&
		    dbus_message_get_args(message, NULL, DBUS_TYPE_UINT32, &id,
			DBUS_TYPE_STRING, &key, DBUS_TYPE_INVALID))
		{
			record = cJSON_CreateObject();
			cJSON_AddStringToObject(record, "signal", "action");
			cJSON_AddNumberToObject(record, "id", id);
			cJSON_AddStringToObject(record, "key", key);
			cJSON_AddItemToArray(result, record);
		}
		dbus_message_unref(message);
	}
	return result;
}

/* A fixed short-message producer. The caller intentionally leaves replies
 * unread after this returns and retains the process watchdog around flush:
 * one transport write can still block in the kernel. No reply is popped or
 * dispatched here. The resident stdin wait then performs no bus reads. */
static bool burst(DBusConnection *connection, cJSON *command, cJSON *result,
	struct burst_state *state)
{
	double number;
	unsigned int queued = 0;

	if (state->count || !number_field(command, "count", BURST_MAXIMUM,
	    1, BURST_MAXIMUM, &number))
		return false;
	for (unsigned int i = 0; i < (unsigned int)number; i++)
	{
		DBusMessage *message = dbus_message_new_method_call(NAME, PATH,
			NAME, "Notify");
		bool sent;

		if (!message)
			return false;
		dbus_message_set_auto_start(message, FALSE);
		/* NULL selects the existing fixed short defaults, not a second
		 * caller-controlled payload multiplied by the burst count. */
		sent = append_notify(message, NULL) &&
		    dbus_connection_get_outgoing_size(connection) < 262144 &&
		    dbus_connection_send(connection, message,
			&state->serials[state->count]);
		dbus_message_unref(message);
		if (!sent)
			return false;
		state->count++;
		queued++;
	}
	dbus_connection_flush(connection);
	cJSON_AddNumberToObject(result, "queued", queued);
	cJSON_AddNumberToObject(result, "outgoing_bytes",
		dbus_connection_get_outgoing_size(connection));
	cJSON_AddBoolToObject(result, "replies_popped", false);
	return dbus_connection_get_is_connected(connection);
}

/* Drain exactly the preceding fixed burst, matching captured Notify serials.
 * The read allowance is at most forty 50-ms waits and 512 popped messages;
 * the caller additionally retains its three-second process watchdog. Other
 * notification signals are errors, never counted as discarded replies. */
static bool drain(DBusConnection *connection, cJSON *command, cJSON *result,
	struct burst_state *state)
{
	double number;
	unsigned int replies = 0, returns = 0, errors = 0, controls = 0;
	unsigned int reads = 0, popped = 0, expected = state->count;
	bool valid = true;

	if (!number_field(command, "count", BURST_MAXIMUM, 1, BURST_MAXIMUM,
	    &number) || expected != (unsigned int)number)
		return false;
	while (state->count && popped < 512)
	{
		DBusMessage *message = dbus_connection_pop_message(connection);
		dbus_uint32_t serial;
		unsigned int index;
		int type;

		if (!message)
		{
			if (reads >= 40)
				break;
			reads++;
			if (!dbus_connection_read_write(connection, 50))
				break;
			continue;
		}
		popped++;
		serial = dbus_message_get_reply_serial(message);
		type = dbus_message_get_type(message);
		for (index = 0; index < expected; index++)
			if (serial && state->serials[index] == serial)
				break;
		if (index < expected && (type == DBUS_MESSAGE_TYPE_METHOD_RETURN ||
		    type == DBUS_MESSAGE_TYPE_ERROR))
		{
			dbus_uint32_t id = 0;

			valid = type == DBUS_MESSAGE_TYPE_ERROR ?
			    dbus_message_get_error_name(message) != NULL :
			    dbus_message_get_args(message, NULL,
				DBUS_TYPE_UINT32, &id, DBUS_TYPE_INVALID) && id;
			if (valid)
			{
				state->serials[index] = 0;
				state->count--;
				replies++;
				returns += type == DBUS_MESSAGE_TYPE_METHOD_RETURN;
				errors += type == DBUS_MESSAGE_TYPE_ERROR;
			}
		}
		else
		{
			const char *name = NULL;
			const char *sender = dbus_message_get_sender(message);

			/* Registration may leave this one control signal queued.
			 * It is reported separately, not a Notify reply or action. */
			valid = !controls && sender &&
			    !strcmp(sender, DBUS_SERVICE_DBUS) &&
			    dbus_message_is_signal(message, DBUS_INTERFACE_DBUS,
				"NameAcquired") &&
			    dbus_message_get_args(message, NULL, DBUS_TYPE_STRING,
				&name, DBUS_TYPE_INVALID) &&
			    !strcmp(name, dbus_bus_get_unique_name(connection));
			controls += valid;
		}
		dbus_message_unref(message);
		if (!valid)
			break;
	}
	cJSON_AddNumberToObject(result, "notification_replies_drained", replies);
	cJSON_AddNumberToObject(result, "method_returns", returns);
	cJSON_AddNumberToObject(result, "method_errors", errors);
	cJSON_AddNumberToObject(result, "registration_control_signals", controls);
	cJSON_AddNumberToObject(result, "read_rounds", reads);
	cJSON_AddNumberToObject(result, "remaining", state->count);
	return valid && !state->count && replies == expected;
}

static cJSON *execute(DBusConnection *connection, cJSON *command,
	struct burst_state *state)
{
	const char *verb = string_field(command, "verb", "", 32), *method;
	DBusMessage *message = NULL, *reply = NULL;
	DBusError error;
	cJSON *result = cJSON_CreateObject();
	bool valid = true;

	if (!verb)
		goto invalid;
	if (!strcmp(verb, "identity"))
	{
		const char *name = dbus_bus_get_unique_name(connection);

		if (!name)
			goto invalid;
		cJSON_AddStringToObject(result, "unique", name);
		return result;
	}
	if (!strcmp(verb, "burst"))
	{
		if (!burst(connection, command, result, state))
			goto invalid;
		return result;
	}
	if (!strcmp(verb, "drain"))
	{
		if (!drain(connection, command, result, state))
			goto invalid;
		return result;
	}
	if (state->count)
		goto invalid;
	if (!strcmp(verb, "signals"))
	{
		cJSON_AddItemToObject(result, "signals", collect_signals(connection));
		return result;
	}
	method = !strcmp(verb, "info") ? "GetServerInformation" :
		!strcmp(verb, "capabilities") ? "GetCapabilities" :
		!strcmp(verb, "notify") || !strcmp(verb, "malformed") ? "Notify" :
		!strcmp(verb, "close") ? "CloseNotification" :
		!strcmp(verb, "owner") ? "GetNameOwner" : NULL;
	if (!method)
		goto invalid;
	message = dbus_message_new_method_call(!strcmp(verb, "owner") ?
		"org.freedesktop.DBus" : NAME, !strcmp(verb, "owner") ?
		"/org/freedesktop/DBus" : PATH, !strcmp(verb, "owner") ?
		"org.freedesktop.DBus" : NAME, method);
	if (!message)
		goto invalid;
	dbus_message_set_auto_start(message, FALSE);
	if (!strcmp(verb, "notify"))
		valid = append_notify(message, command);
	else if (!strcmp(verb, "close"))
	{
		double number;
		dbus_uint32_t id;

		valid = number_field(command, "id", 0, 1, UINT32_MAX, &number);
		id = valid ? (dbus_uint32_t)number : 0;
		valid = valid && dbus_message_append_args(message,
			DBUS_TYPE_UINT32, &id, DBUS_TYPE_INVALID);
	}
	else if (!strcmp(verb, "owner") || !strcmp(verb, "malformed"))
	{
		const char *argument = !strcmp(verb, "owner") ? NAME : "wrong type";

		valid = dbus_message_append_args(message, DBUS_TYPE_STRING,
			&argument, DBUS_TYPE_INVALID);
	}
	if (!valid)
		goto invalid;
	dbus_error_init(&error);
	reply = dbus_connection_send_with_reply_and_block(connection, message,
		1500, &error);
	if (!reply)
	{
		cJSON_AddStringToObject(result, "error", error.name ? error.name :
			"missing_reply");
		dbus_error_free(&error);
		goto out;
	}
	dbus_error_free(&error);
	cJSON_AddStringToObject(result, "signature", dbus_message_get_signature(reply));
	if (!strcmp(verb, "notify"))
	{
		dbus_uint32_t id = 0;

		if (!dbus_message_get_args(reply, NULL, DBUS_TYPE_UINT32, &id,
		    DBUS_TYPE_INVALID))
			goto invalid;
		cJSON_AddNumberToObject(result, "id", id);
	}
	else if (!strcmp(verb, "info"))
	{
		const char *name, *vendor, *version, *spec;

		if (!dbus_message_get_args(reply, NULL, DBUS_TYPE_STRING, &name,
		    DBUS_TYPE_STRING, &vendor, DBUS_TYPE_STRING, &version,
		    DBUS_TYPE_STRING, &spec, DBUS_TYPE_INVALID))
			goto invalid;
		cJSON_AddStringToObject(result, "name", name);
		cJSON_AddStringToObject(result, "vendor", vendor);
		cJSON_AddStringToObject(result, "version", version);
		cJSON_AddStringToObject(result, "spec", spec);
	}
	else if (!strcmp(verb, "owner"))
	{
		const char *owner;

		if (!dbus_message_get_args(reply, NULL, DBUS_TYPE_STRING, &owner,
		    DBUS_TYPE_INVALID))
			goto invalid;
		cJSON_AddStringToObject(result, "owner", owner);
	}
	else if (!strcmp(verb, "capabilities"))
	{
		DBusMessageIter iter, array;
		cJSON *caps = cJSON_AddArrayToObject(result, "capabilities");

		if (!dbus_message_has_signature(reply, "as") ||
		    !dbus_message_iter_init(reply, &iter))
			goto invalid;
		dbus_message_iter_recurse(&iter, &array);
		for (unsigned int i = 0; i < 32 &&
		    dbus_message_iter_get_arg_type(&array) == DBUS_TYPE_STRING; i++)
		{
			const char *text;

			dbus_message_iter_get_basic(&array, &text);
			cJSON_AddItemToArray(caps, cJSON_CreateString(text));
			dbus_message_iter_next(&array);
		}
	}
	goto out;
invalid:
	cJSON_AddStringToObject(result, "error", "invalid_fixture_request_or_reply");
out:
	if (message)
		dbus_message_unref(message);
	if (reply)
		dbus_message_unref(reply);
	return result;
}

int main(int argc, char **argv)
{
	DBusConnection *connection;
	DBusError error;
	char line[LINE_MAXIMUM + 2];
	unsigned int count = 0;
	struct burst_state state = {0};

	if (argc != 3 || strcmp(argv[1], "--private-bus") ||
	    !private_address(argv[2]))
		return 2;
	dbus_error_init(&error);
	connection = dbus_connection_open_private(argv[2], &error);
	if (!connection || !dbus_bus_register(connection, &error))
	{
		fprintf(stderr, "private fixture bus unavailable\n");
		dbus_error_free(&error);
		return 1;
	}
	dbus_error_free(&error);
	dbus_connection_set_exit_on_disconnect(connection, FALSE);
	dbus_connection_set_max_message_size(connection, 32768);
	dbus_connection_set_max_received_size(connection, 131072);
	while (count++ < COMMAND_MAXIMUM && fgets(line, sizeof(line), stdin))
	{
		cJSON *command, *result;
		char *output;

		if (!strchr(line, '\n'))
			break;
		command = cJSON_Parse(line);
		result = execute(connection, command, &state);
		output = cJSON_PrintUnformatted(result);
		if (!output || strlen(output) > 32768 || puts(output) < 0 ||
		    fflush(stdout))
		{
			cJSON_free(output);
			cJSON_Delete(result);
			cJSON_Delete(command);
			break;
		}
		cJSON_free(output);
		cJSON_Delete(result);
		cJSON_Delete(command);
	}
	dbus_connection_close(connection);
	dbus_connection_unref(connection);
	return 0;
}
