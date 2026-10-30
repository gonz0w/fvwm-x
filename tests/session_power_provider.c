/* This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or any later version.
 *
 * Opt-in protocol fixture, never an operating-system power provider.
 * Usage: session_power_provider --private-bus ADDRESS SCENARIO LOG [--replace]
 * ADDRESS must name a Unix socket inside a caller-owned private directory.
 * SCENARIO is a <=256-byte regular file with four whitespace-separated words:
 * poweroff-permission reboot-permission suspend-permission mode
 * Modes: normal, deny, challenge, inhibited, unknown, stall, stall-request,
 * loss, release.
 * Read it afresh per call; replace atomically to change permissions in flight.
 * --replace takes the name from an existing fixture (which allows replacement).
 * release drops the name after replying to the next capability query.
 * stall omits replies; stall-request omits only power request replies.
 * loss disconnects after observing a valid power request.
 * LOG is created exclusively with mode 0600; it records fixed method names and
 * interactive=false witnesses. This process never invokes a native power tool,
 * authentication agent, Suspend, arbitrary destination or arbitrary method.
 * The owner must also retain a finite external process watchdog for blocking
 * connect/authentication/kernel I/O. Dispatch has a 30-second lifetime.
 */
#include "config.h"
#include <dbus/dbus.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#define FIXTURE_NAME "org.freedesktop.login1"
#define FIXTURE_PATH "/org/freedesktop/login1"
#define FIXTURE_INTERFACE "org.freedesktop.login1.Manager"
#define FIXTURE_MAX_MESSAGES 256

struct scenario {
	char permission[3][65];
	char mode[32];
};

static bool private_address(const char *address)
{
	DBusAddressEntry **entries = NULL;
	DBusError error;
	const char *path, *comma;
	char parent[1025], *slash;
	struct stat st;
	int count = 0;
	bool valid = false;

	if (!address || strnlen(address, 2049) > 2048 ||
	    strncmp(address, "unix:path=", 10) != 0 || strchr(address, ';'))
		return false;
	comma = strchr(address, ',');
	if (comma && (strncmp(comma, ",guid=", 6) != 0 ||
	    strchr(comma + 1, ',')))
		return false;
	dbus_error_init(&error);
	if (!dbus_parse_address(address, &entries, &count, &error) || count != 1)
		goto out;
	path = dbus_address_entry_get_value(entries[0], "path");
	if (!path || path[0] != '/' || strnlen(path, 1025) > 1024)
		goto out;
	strcpy(parent, path);
	slash = strrchr(parent, '/');
	if (!slash || slash == parent)
		goto out;
	*slash = '\0';
	/* Refuse the conventional system socket and all shared parent directories.
	 * A private fixture bus is an explicit test prerequisite, not a fallback.
	 */
	if (lstat(parent, &st) != 0 || !S_ISDIR(st.st_mode) ||
	    st.st_uid != geteuid() || (st.st_mode & 077) != 0)
		goto out;
	if (lstat(path, &st) != 0 || !S_ISSOCK(st.st_mode) ||
	    st.st_uid != geteuid())
		goto out;
	valid = true;
out:
	if (entries)
		dbus_address_entries_free(entries);
	dbus_error_free(&error);
	return valid;
}

static bool scenario_read(const char *path, struct scenario *scenario)
{
	static const char *const modes[] = {
		"normal", "deny", "challenge", "inhibited", "unknown", "stall",
		"stall-request", "loss", "release"
	};
	char data[258], extra;
	struct stat st;
	ssize_t count;
	int fd;
	bool valid = false;

	fd = open(path, O_RDONLY | O_NOFOLLOW | O_NONBLOCK);
	if (fd < 0)
		return false;
	if (fstat(fd, &st) != 0 || !S_ISREG(st.st_mode) ||
	    st.st_uid != geteuid() || st.st_size < 1 || st.st_size > 256)
		goto out;
	count = read(fd, data, sizeof(data) - 1);
	if (count < 1 || count > 256 || memchr(data, '\0', (size_t)count))
		goto out;
	data[count] = '\0';
	if (sscanf(data, "%64s %64s %64s %31s %c", scenario->permission[0],
	    scenario->permission[1], scenario->permission[2], scenario->mode,
	    &extra) != 4)
		goto out;
	for (size_t i = 0; i < 3; i++)
		for (const char *text = scenario->permission[i]; *text; text++)
			if ((*text < 'a' || *text > 'z') && *text != '-')
				goto out;
	for (size_t i = 0; i < sizeof(modes) / sizeof(modes[0]); i++)
		if (strcmp(scenario->mode, modes[i]) == 0)
		{
			valid = true;
			break;
		}
out:
	close(fd);
	return valid;
}

static bool send_reply(DBusConnection *connection, DBusMessage *reply)
{
	bool valid;

	if (!reply)
		return false;
	valid = dbus_connection_send(connection, reply, NULL);
	dbus_message_unref(reply);
	dbus_connection_flush(connection);
	return valid;
}

static bool reject(DBusConnection *connection, DBusMessage *message,
	const char *name)
{
	return send_reply(connection, dbus_message_new_error(message, name,
		"Fixed private test provider refused request"));
}

static void log_rejected(FILE *log, const char *member)
{
	/* libdbus validates members as bounded protocol identifiers. Preserve the
	 * rejected method so negative Suspend evidence cannot hide in a generic log.
	 */
	const char *name = member && dbus_validate_member(member, NULL) ?
		member : "invalid";

	fprintf(log, "rejected method=%.255s\n", name);
	fflush(log);
}

static bool dispatch(DBusConnection *connection, DBusMessage *message,
	const char *scenario_path, FILE *log)
{
	static const char *const queries[3] = {
		"CanPowerOff", "CanReboot", "CanSuspend"
	};
	const char *member = dbus_message_get_member(message);
	const char *path = dbus_message_get_path(message);
	const char *interface = dbus_message_get_interface(message);
	struct scenario scenario;
	DBusMessage *reply;

	if (dbus_message_get_type(message) != DBUS_MESSAGE_TYPE_METHOD_CALL)
		return true;
	if (!member || !path || !interface ||
	    strcmp(path, FIXTURE_PATH) != 0 ||
	    strcmp(interface, FIXTURE_INTERFACE) != 0 ||
	    dbus_message_contains_unix_fds(message))
	{
		log_rejected(log, member);
		return reject(connection, message, DBUS_ERROR_UNKNOWN_METHOD);
	}
	if (!scenario_read(scenario_path, &scenario))
	{
		log_rejected(log, member);
		return reject(connection, message, DBUS_ERROR_FAILED);
	}
	for (size_t i = 0; i < 3; i++)
		if (strcmp(member, queries[i]) == 0)
		{
			const char *permission = scenario.permission[i];

			if (!dbus_message_has_signature(message, ""))
				return reject(connection, message, DBUS_ERROR_INVALID_ARGS);
			fprintf(log, "%s mode=%s permission=%s\n", queries[i],
				scenario.mode, permission);
			fflush(log);
			if (strcmp(scenario.mode, "stall") == 0)
				return true;
			if (strcmp(scenario.mode, "unknown") == 0)
				permission = "future-permission";
			reply = dbus_message_new_method_return(message);
			if (!reply)
				return false;
			if (!dbus_message_append_args(reply, DBUS_TYPE_STRING,
			    &permission, DBUS_TYPE_INVALID))
			{
				dbus_message_unref(reply);
				return false;
			}
			if (!send_reply(connection, reply))
				return false;
			if (strcmp(scenario.mode, "release") == 0)
			{
				DBusError error;

				dbus_error_init(&error);
				dbus_bus_release_name(connection, FIXTURE_NAME, &error);
				dbus_error_free(&error);
			}
			return true;
		}
	if (strcmp(member, "PowerOff") == 0 || strcmp(member, "Reboot") == 0)
	{
		dbus_bool_t interactive = TRUE;

		if (!dbus_message_has_signature(message, "b") ||
		    !dbus_message_get_args(message, NULL, DBUS_TYPE_BOOLEAN,
			&interactive, DBUS_TYPE_INVALID) || interactive)
			return reject(connection, message, DBUS_ERROR_INVALID_ARGS);
		fprintf(log, "%s interactive=false mode=%s\n", member,
			scenario.mode);
		fflush(log);
		if (strcmp(scenario.mode, "stall") == 0 ||
		    strcmp(scenario.mode, "stall-request") == 0)
			return true;
		if (strcmp(scenario.mode, "loss") == 0)
			return false;
		if (strcmp(scenario.mode, "deny") == 0)
			return reject(connection, message, DBUS_ERROR_ACCESS_DENIED);
		if (strcmp(scenario.mode, "challenge") == 0)
			return reject(connection, message,
				"org.freedesktop.DBus.Error."
				"InteractiveAuthorizationRequired");
		if (strcmp(scenario.mode, "inhibited") == 0)
			return reject(connection, message,
				"org.freedesktop.login1.OperationInProgress");
		return send_reply(connection, dbus_message_new_method_return(message));
	}
	log_rejected(log, member);
	return reject(connection, message, DBUS_ERROR_UNKNOWN_METHOD);
}

int main(int argc, char **argv)
{
	DBusConnection *connection = NULL;
	DBusError error;
	struct timespec start, now;
	struct scenario scenario;
	FILE *log = NULL;
	int fd, result = 1;
	bool replace = argc == 6 && strcmp(argv[5], "--replace") == 0;

	dbus_error_init(&error);
	if ((argc != 5 && !replace) || strcmp(argv[1], "--private-bus") != 0 ||
	    !private_address(argv[2]) || !scenario_read(argv[3], &scenario))
		goto out;
	fd = open(argv[4], O_WRONLY | O_CREAT | O_EXCL | O_NOFOLLOW, 0600);
	if (fd < 0)
		goto out;
	log = fdopen(fd, "w");
	if (!log)
	{
		close(fd);
		goto out;
	}
	connection = dbus_connection_open_private(argv[2], &error);
	if (!connection)
		goto out;
	dbus_connection_set_exit_on_disconnect(connection, FALSE);
	dbus_connection_set_max_message_size(connection, 16384);
	dbus_connection_set_max_received_size(connection, 32768);
	dbus_connection_set_max_message_unix_fds(connection, 0);
	dbus_connection_set_max_received_unix_fds(connection, 1);
	if (!dbus_bus_register(connection, &error) ||
	    dbus_bus_request_name(connection, FIXTURE_NAME,
		DBUS_NAME_FLAG_ALLOW_REPLACEMENT | DBUS_NAME_FLAG_DO_NOT_QUEUE |
		(replace ? DBUS_NAME_FLAG_REPLACE_EXISTING : 0), &error) !=
		DBUS_REQUEST_NAME_REPLY_PRIMARY_OWNER ||
	    clock_gettime(CLOCK_MONOTONIC, &start) != 0)
		goto out;
	fputs("ready\n", log);
	fflush(log);
	result = 0;
	for (size_t count = 0; count < FIXTURE_MAX_MESSAGES;)
	{
		DBusMessage *message;
		bool keep;

		if (clock_gettime(CLOCK_MONOTONIC, &now) != 0 ||
		    now.tv_sec - start.tv_sec >= 30 ||
		    !dbus_connection_read_write(connection, 100))
			break;
		message = dbus_connection_pop_message(connection);
		if (!message)
			continue;
		count++;
		keep = dispatch(connection, message, argv[3], log);
		dbus_message_unref(message);
		if (!keep)
			break;
	}
out:
	if (connection)
	{
		dbus_connection_close(connection);
		dbus_connection_unref(connection);
	}
	if (log)
		fclose(log);
	dbus_error_free(&error);
	return result;
}
