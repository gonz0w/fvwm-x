/* This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or any later version.
 *
 * Optional isolated libdbus client. The invoking parent MUST enforce a
 * finite process deadline: connect/authentication and kernel I/O can block.
 * Fixed session notification and existing system power bus branches only;
 * no activation or exported methods. The private native guardian branch
 * separately owns the optional foreground daemon without making D-Bus calls.
 */
#include "config.h"
#include <dbus/dbus.h>
#include "libs/cJSON.h"
#include "session_bus_guardian.h"
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define BUS_OUTPUT_MAX 16384
#define BUS_MESSAGE_MAX 16384
#define BUS_RECEIVE_MAX 32768
#define BUS_CALL_TIMEOUT_MS 1000
#define BUS_NAME "org.freedesktop.DBus"
#define BUS_PATH "/org/freedesktop/DBus"
#define NOTIFY_NAME "org.freedesktop.Notifications"
#define NOTIFY_PATH "/org/freedesktop/Notifications"
#define POWER_NAME "org.freedesktop.login1"
#define POWER_PATH "/org/freedesktop/login1"
#define POWER_INTERFACE "org.freedesktop.login1.Manager"

struct observation {
	char bus_id[33];
	char owner[256];
	cJSON *capabilities;
	bool available;
};

static bool text_valid(const char *text, size_t maximum)
{
	size_t length = text ? strnlen(text, maximum + 1) : maximum + 1;

	if (length > maximum || !dbus_validate_utf8(text, NULL))
		return false;
	for (size_t i = 0; i < length; i++)
		if ((unsigned char)text[i] < 0x20 || text[i] == 0x7f)
			return false;
	return true;
}

static int publish(const struct observation *observed, const char *state,
	uint32_t id, const char *detail)
{
	cJSON *root = cJSON_CreateObject();
	cJSON *caps = observed->capabilities ?
		cJSON_Duplicate(observed->capabilities, true) : cJSON_CreateArray();
	char *json = NULL;
	int status = 1;

	if (!root || !caps || !cJSON_AddItemToObject(root, "capabilities", caps))
	{
		cJSON_Delete(caps);
		goto out;
	}
	if (!cJSON_AddNumberToObject(root, "version", 1) ||
	    !cJSON_AddBoolToObject(root, "available", observed->available) ||
	    !cJSON_AddStringToObject(root, "state", state) ||
	    !cJSON_AddStringToObject(root, "bus_id", observed->bus_id) ||
	    !cJSON_AddStringToObject(root, "server_owner", observed->owner) ||
	    !cJSON_AddNumberToObject(root, "notification_id", id) ||
	    !cJSON_AddStringToObject(root, "detail", detail))
		goto out;
	json = cJSON_PrintUnformatted(root);
	if (!json || strlen(json) >= BUS_OUTPUT_MAX)
		goto out;
	status = puts(json) < 0 ? 1 : 0;
out:
	cJSON_free(json);
	cJSON_Delete(root);
	if (status)
		fputs("{\"version\":1,\"available\":false,\"state\":\"failed\","
			"\"bus_id\":\"\",\"server_owner\":\"\",\"capabilities\":[],"
			"\"notification_id\":0,\"detail\":\"output_failed\"}\n", stdout);
	return status;
}

static bool local_address(const char *address)
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
			(abstract && !path && abstract[0] &&
			text_valid(abstract, 1024));
	}
	if (entries)
		dbus_address_entries_free(entries);
	dbus_error_free(&error);
	return valid;
}

static DBusMessage *call(DBusConnection *connection, const char *destination,
	const char *path, const char *interface, const char *method,
	const char *argument, DBusError *error)
{
	DBusMessage *message = dbus_message_new_method_call(destination, path,
		interface, method), *reply = NULL;

	if (!message)
		return NULL;
	dbus_message_set_auto_start(message, FALSE);
	if (!argument || dbus_message_append_args(message, DBUS_TYPE_STRING,
	    &argument, DBUS_TYPE_INVALID))
		reply = dbus_connection_send_with_reply_and_block(connection, message,
			BUS_CALL_TIMEOUT_MS, error);
	dbus_message_unref(message);
	return reply;
}

static bool observe(DBusConnection *connection, struct observation *observed,
	const char **detail)
{
	DBusError error;
	DBusMessage *reply;
	const char *text = NULL;
	bool valid = false;

	dbus_error_init(&error);
	*detail = "bus_id_failed";
	reply = call(connection, BUS_NAME, BUS_PATH, BUS_NAME, "GetId", NULL,
		&error);
	if (!reply || !dbus_message_has_signature(reply, "s") ||
	    !dbus_message_get_args(reply, &error, DBUS_TYPE_STRING, &text,
		DBUS_TYPE_INVALID) || strlen(text) != 32 ||
	    strspn(text, "0123456789abcdef") != 32)
		goto out;
	strcpy(observed->bus_id, text);
	observed->available = true;
	dbus_message_unref(reply);
	reply = NULL;
	*detail = "notification_owner_failed";
	reply = call(connection, BUS_NAME, BUS_PATH, BUS_NAME, "GetNameOwner",
		NOTIFY_NAME, &error);
	if (!reply && dbus_error_has_name(&error, DBUS_ERROR_NAME_HAS_NO_OWNER))
	{
		*detail = "notification_server_unavailable";
		valid = true;
		goto out;
	}
	if (!reply || !dbus_message_has_signature(reply, "s") ||
	    !dbus_message_get_args(reply, &error, DBUS_TYPE_STRING, &text,
		DBUS_TYPE_INVALID) || !text_valid(text, 255) || text[0] != ':' ||
	    !dbus_validate_bus_name(text, NULL))
		goto out;
	strcpy(observed->owner, text);
	*detail = "ready";
	valid = true;
out:
	if (reply)
		dbus_message_unref(reply);
	dbus_error_free(&error);
	return valid;
}

static bool capabilities(DBusConnection *connection,
	struct observation *observed)
{
	DBusError error;
	DBusMessage *reply;
	DBusMessageIter iter, array;
	size_t count = 0;
	bool valid = false;

	if (!observed->owner[0])
		return true;
	dbus_error_init(&error);
	reply = call(connection, observed->owner, NOTIFY_PATH, NOTIFY_NAME,
		"GetCapabilities", NULL, &error);
	if (!reply || !dbus_message_has_signature(reply, "as") ||
	    !dbus_message_iter_init(reply, &iter))
		goto out;
	dbus_message_iter_recurse(&iter, &array);
	while (dbus_message_iter_get_arg_type(&array) != DBUS_TYPE_INVALID)
	{
		const char *text;
		cJSON *value;

		if (++count > 32)
			goto out;
		dbus_message_iter_get_basic(&array, &text);
		if (!text_valid(text, 128))
			goto out;
		value = cJSON_CreateString(text);
		if (!value || !cJSON_AddItemToArray(observed->capabilities, value))
		{
			cJSON_Delete(value);
			goto out;
		}
		dbus_message_iter_next(&array);
	}
	valid = true;
out:
	if (reply)
		dbus_message_unref(reply);
	dbus_error_free(&error);
	return valid;
}

static bool id_parse(const char *text, dbus_uint32_t *id)
{
	uint32_t value = 0;

	if (!text || !*text || strlen(text) > 10 ||
	    (text[0] == '0' && text[1]))
		return false;
	for (; *text; text++)
	{
		if (*text < '0' || *text > '9' ||
		    value > (UINT32_MAX - (unsigned)(*text - '0')) / 10)
			return false;
		value = value * 10 + (unsigned)(*text - '0');
	}
	*id = value;
	return value != 0;
}

static void escape_body(char output[6145], const char *input)
{
	size_t used = 0;

	for (; *input; input++)
	{
		const char *replacement = *input == '<' ? "&lt;" :
			*input == '>' ? "&gt;" : *input == '&' ? "&amp;" : NULL;

		if (replacement)
		{
			size_t length = strlen(replacement);

			memcpy(output + used, replacement, length);
			used += length;
		}
		else
			output[used++] = *input;
	}
	output[used] = '\0';
}

static bool notification(DBusConnection *connection,
	const struct observation *observed, bool close, const char *summary,
	const char *body, dbus_uint32_t *id)
{
	DBusError error;
	DBusMessage *message, *reply = NULL;
	DBusMessageIter iter, container;
	const char *app = "FVWM-X", *empty = "";
	char plain[6145];
	const char *escaped = plain;
	dbus_uint32_t replacement = 0;
	dbus_int32_t expiration = -1;
	bool valid = false;

	dbus_error_init(&error);
	message = dbus_message_new_method_call(observed->owner, NOTIFY_PATH,
		NOTIFY_NAME, close ? "CloseNotification" : "Notify");
	if (!message)
		goto out;
	dbus_message_set_auto_start(message, FALSE);
	if (close)
	{
		if (!dbus_message_append_args(message, DBUS_TYPE_UINT32, id,
		    DBUS_TYPE_INVALID))
			goto out;
	}
	else
	{
		const cJSON *capability;
		bool markup = false;

		for (capability = observed->capabilities->child; capability;
		     capability = capability->next)
			if (strcmp(capability->valuestring, "body-markup") == 0)
				markup = true;
		if (markup)
			escape_body(plain, body);
		else
			escaped = body;
		dbus_message_iter_init_append(message, &iter);
		if (!dbus_message_iter_append_basic(&iter, DBUS_TYPE_STRING, &app) ||
		    !dbus_message_iter_append_basic(&iter, DBUS_TYPE_UINT32,
			&replacement) ||
		    !dbus_message_iter_append_basic(&iter, DBUS_TYPE_STRING, &empty) ||
		    !dbus_message_iter_append_basic(&iter, DBUS_TYPE_STRING, &summary) ||
		    !dbus_message_iter_append_basic(&iter, DBUS_TYPE_STRING, &escaped) ||
		    !dbus_message_iter_open_container(&iter, DBUS_TYPE_ARRAY, "s",
			&container) ||
		    !dbus_message_iter_close_container(&iter, &container) ||
		    !dbus_message_iter_open_container(&iter, DBUS_TYPE_ARRAY, "{sv}",
			&container) ||
		    !dbus_message_iter_close_container(&iter, &container) ||
		    !dbus_message_iter_append_basic(&iter, DBUS_TYPE_INT32,
			&expiration))
			goto out;
	}
	reply = dbus_connection_send_with_reply_and_block(connection, message,
		BUS_CALL_TIMEOUT_MS, &error);
	if (reply && close)
		valid = dbus_message_has_signature(reply, "");
	else if (reply && dbus_message_has_signature(reply, "u"))
		valid = dbus_message_get_args(reply, &error, DBUS_TYPE_UINT32, id,
			DBUS_TYPE_INVALID) && *id != 0;
out:
	if (message)
		dbus_message_unref(message);
	if (reply)
		dbus_message_unref(reply);
	dbus_error_free(&error);
	return valid;
}

static int notification_main(int argc, char **argv)
{
	struct observation observed = {.available = true};
	DBusConnection *connection = NULL;
	DBusError error;
	const char *address = getenv("DBUS_SESSION_BUS_ADDRESS");
	const char *state = "failed", *detail = "invalid_arguments";
	const char *owner = NULL, *bus_id = NULL;
	dbus_uint32_t id = 0;
	bool probe = argc == 2 && strcmp(argv[1], "probe") == 0;
	bool notify = argc == 6 && strcmp(argv[1], "notify") == 0;
	bool close = argc == 5 && strcmp(argv[1], "close") == 0;
	int status;

	dbus_error_init(&error);
	observed.capabilities = cJSON_CreateArray();
	if (!observed.capabilities || (!probe && !notify && !close))
		goto out;
	if (notify)
	{
		if (!text_valid(argv[2], 160) || !text_valid(argv[3], 1024))
			goto out;
		owner = argv[4];
		bus_id = argv[5];
	}
	if (close)
	{
		if (!id_parse(argv[2], &id))
			goto out;
		owner = argv[3];
		bus_id = argv[4];
	}
	if (!probe && (!text_valid(owner, 255) || owner[0] != ':' ||
	    !dbus_validate_bus_name(owner, NULL) || strlen(bus_id) != 32 ||
	    strspn(bus_id, "0123456789abcdef") != 32))
		goto out;
	state = "unavailable";
	detail = !address || !*address ? "missing_inherited_session_address" :
		"explicit_local_session_address_required";
	if (!local_address(address))
		goto out;
	state = "failed";
	detail = "session_bus_connection_failed";
	connection = dbus_connection_open_private(address, &error);
	if (!connection)
		goto out;
	dbus_connection_set_exit_on_disconnect(connection, FALSE);
	dbus_connection_set_max_message_size(connection, BUS_MESSAGE_MAX);
	dbus_connection_set_max_received_size(connection, BUS_RECEIVE_MAX);
	dbus_connection_set_max_message_unix_fds(connection, 0);
	/* Zero would disable reads even when no FDs are queued. */
	dbus_connection_set_max_received_unix_fds(connection, 1);
	if (!dbus_bus_register(connection, &error))
		goto out;
	state = "failed";
	if (!observe(connection, &observed, &detail))
		goto out;
	if (probe)
	{
		if (!capabilities(connection, &observed))
		{
			detail = "notification_capabilities_failed";
			goto out;
		}
		state = "ready";
		goto out;
	}
	detail = "stale_server_owner";
	if (!observed.owner[0] || strcmp(owner, observed.owner) != 0 ||
	    strcmp(bus_id, observed.bus_id) != 0)
		goto out;
	if (notify && !capabilities(connection, &observed))
	{
		detail = "notification_capabilities_failed";
		goto out;
	}
	detail = close ? "notification_close_failed" : "notification_send_failed";
	if (!notification(connection, &observed, close,
	    notify ? argv[2] : NULL, notify ? argv[3] : NULL, &id))
		goto out;
	state = close ? "closed" : "sent";
	detail = state;
out:
	status = publish(&observed, state, id, detail);
	if (connection)
	{
		dbus_connection_close(connection);
		dbus_connection_unref(connection);
	}
	cJSON_Delete(observed.capabilities);
	dbus_error_free(&error);
	return status;
}

/* Power observations are independent of the notification branch and its IDs.
 * Availability means compiled client support, even without an address/service.
 * The parent retains the actual Wait lease and a four-second overall watchdog.
 */
struct power_observation {
	char bus_id[33];
	char owner[256];
	const char *permission[3];
	const char *cap_detail[3];
};

static const char *const power_actions[3] = {
	"poweroff", "reboot", "suspend"
};
static const char *const power_queries[3] = {
	"CanPowerOff", "CanReboot", "CanSuspend"
};

static int power_publish(const struct power_observation *observed,
	const char *state, const char *detail)
{
	cJSON *root = cJSON_CreateObject(), *caps = cJSON_CreateArray();
	char *json = NULL;
	int status = 1;

	if (!root || !caps)
	{
		cJSON_Delete(caps);
		goto out;
	}
	if (!cJSON_AddItemToObject(root, "capabilities", caps))
	{
		cJSON_Delete(caps);
		goto out;
	}
	for (size_t i = 0; i < 3; i++)
	{
		cJSON *cap = cJSON_CreateObject();

		if (!cap || !cJSON_AddItemToArray(caps, cap))
		{
			cJSON_Delete(cap);
			goto out;
		}
		if (!cJSON_AddStringToObject(cap, "action", power_actions[i]) ||
		    !cJSON_AddStringToObject(cap, "permission",
			observed->permission[i]) ||
		    !cJSON_AddStringToObject(cap, "detail", observed->cap_detail[i]))
			goto out;
	}
	if (!cJSON_AddNumberToObject(root, "version", 1) ||
	    !cJSON_AddStringToObject(root, "kind", "power") ||
	    !cJSON_AddBoolToObject(root, "available", true) ||
	    !cJSON_AddStringToObject(root, "state", state) ||
	    !cJSON_AddStringToObject(root, "bus_id", observed->bus_id) ||
	    !cJSON_AddStringToObject(root, "server_owner", observed->owner) ||
	    !cJSON_AddStringToObject(root, "detail", detail))
		goto out;
	json = cJSON_PrintUnformatted(root);
	if (json && strlen(json) < BUS_OUTPUT_MAX)
		status = puts(json) < 0 ? 1 : 0;
out:
	cJSON_free(json);
	cJSON_Delete(root);
	if (status)
		fputs("{\"version\":1,\"kind\":\"power\",\"available\":true,"
			"\"state\":\"failed\",\"bus_id\":\"\",\"server_owner\":\"\","
			"\"detail\":\"output_failed\",\"capabilities\":["
			"{\"action\":\"poweroff\",\"permission\":\"unknown\","
			"\"detail\":\"output_failed\"},"
			"{\"action\":\"reboot\",\"permission\":\"unknown\","
			"\"detail\":\"output_failed\"},"
			"{\"action\":\"suspend\",\"permission\":\"unknown\","
			"\"detail\":\"output_failed\"}]}\n", stdout);
	return status;
}

static bool power_identity(DBusConnection *connection,
	struct power_observation *observed, const char **detail)
{
	DBusError error;
	DBusMessage *reply;
	const char *text = NULL;
	bool valid = false;

	dbus_error_init(&error);
	*detail = "system_bus_id_failed";
	reply = call(connection, BUS_NAME, BUS_PATH, BUS_NAME, "GetId", NULL,
		&error);
	if (!reply || !dbus_message_has_signature(reply, "s") ||
	    !dbus_message_get_args(reply, &error, DBUS_TYPE_STRING, &text,
		DBUS_TYPE_INVALID) || strlen(text) != 32 ||
	    strspn(text, "0123456789abcdef") != 32)
		goto out;
	strcpy(observed->bus_id, text);
	dbus_message_unref(reply);
	reply = NULL;
	*detail = "power_owner_failed";
	reply = call(connection, BUS_NAME, BUS_PATH, BUS_NAME, "GetNameOwner",
		POWER_NAME, &error);
	if (!reply && dbus_error_has_name(&error, DBUS_ERROR_NAME_HAS_NO_OWNER))
	{
		*detail = "power_service_unavailable";
		valid = true;
		goto out;
	}
	if (!reply || !dbus_message_has_signature(reply, "s") ||
	    !dbus_message_get_args(reply, &error, DBUS_TYPE_STRING, &text,
		DBUS_TYPE_INVALID) || !text_valid(text, 255) || text[0] != ':' ||
	    !dbus_validate_bus_name(text, NULL))
		goto out;
	strcpy(observed->owner, text);
	*detail = "ready";
	valid = true;
out:
	if (reply)
		dbus_message_unref(reply);
	dbus_error_free(&error);
	return valid;
}

static bool power_permission(DBusConnection *connection,
	struct power_observation *observed, size_t action)
{
	static const char *const permissions[] = {
		"yes", "no", "challenge", "na", "inhibited",
		"inhibitor-blocked", "challenge-inhibitor-blocked"
	};
	DBusError error;
	DBusMessage *reply;
	const char *text = NULL, *sender;
	bool valid = false;

	observed->permission[action] = "unknown";
	observed->cap_detail[action] = "permission_query_failed";
	dbus_error_init(&error);
	reply = call(connection, observed->owner, POWER_PATH, POWER_INTERFACE,
		power_queries[action], NULL, &error);
	if (!reply || !dbus_message_has_signature(reply, "s") ||
	    !dbus_message_get_args(reply, &error, DBUS_TYPE_STRING, &text,
		DBUS_TYPE_INVALID) || !text_valid(text, 64))
		goto out;
	sender = dbus_message_get_sender(reply);
	if (!sender || strcmp(sender, observed->owner) != 0)
		goto out;
	valid = true;
	observed->cap_detail[action] = "unrecognized_permission";
	for (size_t i = 0; i < sizeof(permissions) / sizeof(permissions[0]); i++)
		if (strcmp(text, permissions[i]) == 0)
		{
			observed->permission[action] = permissions[i];
			observed->cap_detail[action] = "permission_observed";
			break;
		}
out:
	if (reply)
		dbus_message_unref(reply);
	dbus_error_free(&error);
	return valid;
}

static const char *power_refusal(const char *permission)
{
	if (strcmp(permission, "yes") == 0)
		return NULL;
	if (strcmp(permission, "no") == 0)
		return "denied";
	if (strcmp(permission, "challenge") == 0)
		return "authentication-required";
	if (strcmp(permission, "inhibited") == 0 ||
	    strcmp(permission, "inhibitor-blocked") == 0 ||
	    strcmp(permission, "challenge-inhibitor-blocked") == 0)
		return "inhibited";
	if (strcmp(permission, "na") == 0)
		return "unavailable";
	return "failed";
}

static const char *power_request(DBusConnection *connection,
	const struct power_observation *observed, size_t action,
	const char **detail)
{
	DBusError error;
	DBusMessage *message, *reply = NULL;
	dbus_bool_t interactive = FALSE;
	const char *state = "failed", *sender;

	dbus_error_init(&error);
	*detail = "power_message_failed";
	message = dbus_message_new_method_call(observed->owner, POWER_PATH,
		POWER_INTERFACE, action == 0 ? "PowerOff" : "Reboot");
	if (!message)
		goto out;
	dbus_message_set_auto_start(message, FALSE);
	if (!dbus_message_append_args(message, DBUS_TYPE_BOOLEAN, &interactive,
	    DBUS_TYPE_INVALID))
		goto out;
	/* Once submitted, loss/timeout/malformed replies cannot prove no action.
	 * No error causes a native fallback or automatic replay.
	 */
	state = "outcome-unconfirmed";
	*detail = "power_request_outcome_unconfirmed";
	reply = dbus_connection_send_with_reply_and_block(connection, message,
		BUS_CALL_TIMEOUT_MS, &error);
	if (reply)
	{
		sender = dbus_message_get_sender(reply);
		if (sender && strcmp(sender, observed->owner) == 0 &&
		    dbus_message_has_signature(reply, ""))
		{
			state = "requested";
			*detail = "power_request_acknowledged";
		}
	}
	else if (dbus_error_has_name(&error, DBUS_ERROR_ACCESS_DENIED))
	{
		state = "denied";
		*detail = "power_request_access_denied";
	}
	else if (dbus_error_has_name(&error,
	    "org.freedesktop.DBus.Error.InteractiveAuthorizationRequired"))
	{
		state = "authentication-required";
		*detail = "power_request_authentication_required";
	}
out:
	if (message)
		dbus_message_unref(message);
	if (reply)
		dbus_message_unref(reply);
	dbus_error_free(&error);
	return state;
}

static int power_main(int argc, char **argv)
{
	struct power_observation observed = {
		.permission = {"unknown", "unknown", "unknown"},
		.cap_detail = {"not_observed", "not_observed", "not_observed"}
	};
	struct power_observation current = {0};
	DBusConnection *connection = NULL;
	DBusError error;
	const char *address = getenv("DBUS_SYSTEM_BUS_ADDRESS");
	const char *state = "failed", *detail = "invalid_arguments";
	bool probe = argc == 2 && strcmp(argv[1], "power-probe") == 0;
	bool request = argc == 5 && strcmp(argv[1], "power-request") == 0;
	bool complete = true;
	size_t action = 0;
	int status;

	dbus_error_init(&error);
	if (!probe && !request)
		goto out;
	if (request)
	{
		if (strcmp(argv[2], "poweroff") != 0 &&
		    strcmp(argv[2], "reboot") != 0)
			goto out;
		action = strcmp(argv[2], "reboot") == 0 ? 1 : 0;
		if (!text_valid(argv[3], 255) || argv[3][0] != ':' ||
		    !dbus_validate_bus_name(argv[3], NULL) ||
		    strlen(argv[4]) != 32 ||
		    strspn(argv[4], "0123456789abcdef") != 32)
			goto out;
	}
	/* Explicit overrides never fall back; absent overrides use the conventional
	 * existing local socket. Custom locations require the inherited override.
	 */
	if (!address)
		address = "unix:path=/var/run/dbus/system_bus_socket";
	state = "unavailable";
	detail = "explicit_local_system_address_required";
	if (!local_address(address))
		goto out;
	detail = "system_bus_connection_failed";
	connection = dbus_connection_open_private(address, &error);
	if (!connection)
		goto out;
	dbus_connection_set_exit_on_disconnect(connection, FALSE);
	dbus_connection_set_max_message_size(connection, BUS_MESSAGE_MAX);
	dbus_connection_set_max_received_size(connection, BUS_RECEIVE_MAX);
	dbus_connection_set_max_message_unix_fds(connection, 0);
	dbus_connection_set_max_received_unix_fds(connection, 1);
	state = "failed";
	detail = "system_bus_registration_failed";
	if (!dbus_bus_register(connection, &error) ||
	    !power_identity(connection, &observed, &detail))
		goto out;
	if (!observed.owner[0])
	{
		state = request ? "stale" : "unavailable";
		goto out;
	}
	if (request && (strcmp(argv[3], observed.owner) != 0 ||
	    strcmp(argv[4], observed.bus_id) != 0))
	{
		state = "stale";
		detail = "stale_power_provider";
		goto out;
	}
	for (size_t i = 0; i < 3; i++)
		if (!power_permission(connection, &observed, i))
			complete = false;
	state = "ready";
	detail = complete ? "power_capabilities_observed" :
		"power_capabilities_incomplete";
	if (probe)
		goto out;
	/* Check permission again immediately before mutation and then ensure its
	 * unique owner and bus still match the presented observation.
	 */
	state = "failed";
	detail = "power_permission_recheck_failed";
	if (!power_permission(connection, &observed, action))
		goto out;
	state = power_refusal(observed.permission[action]);
	if (state)
	{
		detail = observed.cap_detail[action];
		goto out;
	}
	state = "stale";
	if (!power_identity(connection, &current, &detail) ||
	    strcmp(current.owner, observed.owner) != 0 ||
	    strcmp(current.bus_id, observed.bus_id) != 0)
	{
		detail = "power_provider_changed_before_request";
		goto out;
	}
	state = power_request(connection, &observed, action, &detail);
out:
	status = power_publish(&observed, state, detail);
	if (connection)
	{
		dbus_connection_close(connection);
		dbus_connection_unref(connection);
	}
	dbus_error_free(&error);
	return status;
}

int main(int argc, char **argv)
{
	if (argc > 1 && strcmp(argv[1], "bus-guardian") == 0)
		return session_bus_guardian_main(argc, argv);
	if (argc > 1 && strncmp(argv[1], "power-", 6) == 0)
		return power_main(argc, argv);
	return notification_main(argc, argv);
}
