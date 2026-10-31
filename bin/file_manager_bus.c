/* This program is free software; you can redistribute it and/or modify it
 * under the GNU General Public License, version 2 or any later version. */
#include "config.h"
#include "file_manager_bus.h"
#include "file_manager_policy.h"
#include "libs/cJSON.h"
#include "libs/module_startup.h"
#include <dbus/dbus.h>
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <poll.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#define FILE_MANAGER_NAME "org.freedesktop.FileManager1"
#define FILE_MANAGER_PATH "/org/freedesktop/FileManager1"
#define FILE_MANAGER_RECEIVE_BYTES 131072L
#define FILE_MANAGER_OUTPUT_BYTES 65536L
#define FILE_MANAGER_ADMISSION_MS 2000
#define FILE_MANAGER_TURN_MESSAGES 32U
#define FILE_MANAGER_TURN_NS 2000000LL

struct file_manager_bus {
	DBusConnection *connection;
	/* Only this reference may outlive one dispatch turn. */
	DBusMessage *pending;
	uint64_t next_id;
	uint64_t pending_id;
	int64_t deadline;
	char output[FILE_MANAGER_FRAME_BYTES];
	size_t output_length;
	size_t output_offset;
	char input[FILE_MANAGER_REPLY_BYTES + 1];
	size_t input_length;
	bool failed;
};

static const char introspection[] =
	"<node><interface name='org.freedesktop.FileManager1'>"
	"<method name='ShowFolders'><arg type='as' direction='in'/>"
	"<arg type='s' direction='in'/></method>"
	"<method name='ShowItems'><arg type='as' direction='in'/>"
	"<arg type='s' direction='in'/></method>"
	"<method name='ShowItemProperties'><arg type='as' direction='in'/>"
	"<arg type='s' direction='in'/></method></interface>"
	"<interface name='org.freedesktop.DBus.Introspectable'>"
	"<method name='Introspect'><arg type='s' direction='out'/></method>"
	"</interface><interface name='org.freedesktop.DBus.Peer'>"
	"<method name='Ping'/></interface></node>";

static int64_t monotonic_ns(void)
{
	struct timespec now;

	if (clock_gettime(CLOCK_MONOTONIC, &now))
		return -1;
	return (int64_t)now.tv_sec * 1000000000LL + now.tv_nsec;
}

static bool nonblocking(int fd)
{
	int flags = fcntl(fd, F_GETFL);

	return flags >= 0 && fcntl(fd, F_SETFL, flags | O_NONBLOCK) == 0;
}

static bool ignore_sigpipe(void)
{
	struct sigaction action = {0};

	action.sa_handler = SIG_IGN;
	return sigemptyset(&action.sa_mask) == 0 &&
		sigaction(SIGPIPE, &action, NULL) == 0;
}

int file_manager_bus_unavailable(const char *detail)
{
	char line[256];
	int length;

	/* Callers supply fixed native reasons, never external text. */
	length = snprintf(line, sizeof(line),
		"{\"version\":1,\"kind\":\"unavailable\",\"detail\":\"%s\"}\n",
		detail);
	if (length > 0 && (size_t)length < sizeof(line) &&
	    nonblocking(STDOUT_FILENO) && ignore_sigpipe())
		(void)write(STDOUT_FILENO, line, (size_t)length);
	return 1;
}

static bool output_frame(struct file_manager_bus *bus, cJSON *root)
{
	size_t length;

	if (bus->output_length || !root ||
	    !cJSON_PrintPreallocated(root, bus->output,
		sizeof(bus->output) - 1, false))
		return false;
	length = strlen(bus->output);
	bus->output[length++] = '\n';
	bus->output_length = length;
	bus->output_offset = 0;
	return true;
}

static bool output_ready(struct file_manager_bus *bus, const char *owner)
{
	cJSON *root = cJSON_CreateObject();
	bool valid = root && cJSON_AddNumberToObject(root, "version", 1) &&
		cJSON_AddStringToObject(root, "kind", "ready") &&
		cJSON_AddStringToObject(root, "owner", owner) &&
		output_frame(bus, root);

	cJSON_Delete(root);
	return valid;
}

static void output_write(struct file_manager_bus *bus)
{
	ssize_t count;

	if (!bus->output_length)
		return;
	count = write(STDOUT_FILENO, bus->output + bus->output_offset,
		bus->output_length - bus->output_offset);
	if (count > 0)
	{
		bus->output_offset += (size_t)count;
		if (bus->output_offset == bus->output_length)
			bus->output_offset = bus->output_length = 0;
	}
	else if (count == 0 || (errno != EAGAIN && errno != EWOULDBLOCK &&
	    errno != EINTR))
		bus->failed = true;
}

static void native_reply(struct file_manager_bus *bus, DBusMessage *reply)
{
	char *wire = NULL;
	int length = 0;
	long queued = dbus_connection_get_outgoing_size(bus->connection);

	/* pop_message never invokes libdbus' automatic Peer reply handling.
	 * Every reply is admitted here; a flooded writer closes the service
	 * instead of growing a second native reply queue. */
	if (!reply || queued < 0 ||
	    !dbus_message_marshal(reply, &wire, &length) || length <= 0 ||
	    length > FILE_MANAGER_OUTPUT_BYTES ||
	    queued > FILE_MANAGER_OUTPUT_BYTES - length ||
	    !dbus_connection_send(bus->connection, reply, NULL))
		bus->failed = true;
	if (wire)
		dbus_free(wire);
	if (reply)
		dbus_message_unref(reply);
}

static void native_error(struct file_manager_bus *bus, DBusMessage *message,
	const char *name, const char *detail)
{
	if (dbus_message_get_type(message) == DBUS_MESSAGE_TYPE_METHOD_CALL &&
	    !dbus_message_get_no_reply(message))
		native_reply(bus,
			dbus_message_new_error(message, name, detail));
}

static void pending_clear(struct file_manager_bus *bus)
{
	if (bus->pending)
		dbus_message_unref(bus->pending);
	bus->pending = NULL;
	bus->pending_id = 0;
	bus->deadline = 0;
}

static void parent_reply(struct file_manager_bus *bus, const char *line,
	size_t length)
{
	struct file_manager_reply reply;
	int64_t now;

	if (!file_manager_reply_parse(line, length, &reply))
	{
		bus->failed = true;
		return;
	}
	/* A previous operation's late receipt cannot acknowledge newer work. */
	if (!bus->pending || reply.id != bus->pending_id)
		return;
	now = monotonic_ns();

	if (now < 0)
	{
		bus->failed = true;
		return;
	}
	if (now >= bus->deadline)
		return;
	if (!dbus_message_get_no_reply(bus->pending))
	{
		if (reply.status == FILE_MANAGER_ACCEPTED)
			native_reply(bus,
				dbus_message_new_method_return(bus->pending));
		else
			native_error(bus, bus->pending,
				reply.status == FILE_MANAGER_INVALID ?
				DBUS_ERROR_INVALID_ARGS :
				reply.status == FILE_MANAGER_REFUSED ?
				DBUS_ERROR_LIMITS_EXCEEDED : DBUS_ERROR_FAILED,
				reply.detail);
	}
	pending_clear(bus);
}

static void input_read(struct file_manager_bus *bus)
{
	char bytes[FILE_MANAGER_REPLY_BYTES + 1];
	ssize_t count = read(STDIN_FILENO, bytes, sizeof(bytes));

	if (count <= 0)
	{
		if (!count || (errno != EAGAIN && errno != EWOULDBLOCK &&
		    errno != EINTR))
			bus->failed = true;
		return;
	}
	for (ssize_t i = 0; i < count && !bus->failed; i++)
	{
		if (bytes[i] == '\n')
		{
			parent_reply(bus, bus->input, bus->input_length);
			bus->input_length = 0;
		}
		else if (bus->input_length == FILE_MANAGER_REPLY_BYTES)
			bus->failed = true;
		else
			bus->input[bus->input_length++] = bytes[i];
	}
}

static bool request_frame(struct file_manager_bus *bus, DBusMessage *message)
{
	DBusMessageIter iter, array;
	cJSON *root = NULL, *uris = NULL;
	const char *text, *method = dbus_message_get_member(message);
	char id[32];
	size_t count = 0, total = 0;
	bool valid = false;

	if (!dbus_message_has_signature(message, "ass") ||
	    !dbus_message_iter_init(message, &iter))
		return false;
	/* Validate every envelope byte before allocating any JSON strings. URI
	 * schemes, decoded bytes and lexical grouping belong solely to Go. */
	dbus_message_iter_recurse(&iter, &array);
	while (dbus_message_iter_get_arg_type(&array) != DBUS_TYPE_INVALID)
	{
		dbus_message_iter_get_basic(&array, &text);
		if (++count > FILE_MANAGER_URI_COUNT ||
		    !file_manager_text_valid(text,
			FILE_MANAGER_URI_BYTES - total))
			return false;
		total += strlen(text);
		dbus_message_iter_next(&array);
	}
	if (!count || !dbus_message_iter_next(&iter))
		return false;
	dbus_message_iter_get_basic(&iter, &text);
	if (!file_manager_text_valid(text, FILE_MANAGER_STARTUP_BYTES))
		return false;
	root = cJSON_CreateObject();
	if (!root || !(uris = cJSON_AddArrayToObject(root, "uris")))
		goto out;
	dbus_message_iter_init(message, &iter);
	dbus_message_iter_recurse(&iter, &array);
	while (dbus_message_iter_get_arg_type(&array) != DBUS_TYPE_INVALID)
	{
		cJSON *uri;

		dbus_message_iter_get_basic(&array, &text);
		uri = cJSON_CreateString(text);
		if (!uri || !cJSON_AddItemToArray(uris, uri))
		{
			cJSON_Delete(uri);
			goto out;
		}
		dbus_message_iter_next(&array);
	}
	dbus_message_iter_next(&iter);
	dbus_message_iter_get_basic(&iter, &text);
	(void)snprintf(id, sizeof(id), "%" PRIu64, bus->next_id);
	valid = cJSON_AddNumberToObject(root, "version", 1) &&
		cJSON_AddStringToObject(root, "kind", "request") &&
		cJSON_AddStringToObject(root, "id", id) &&
		cJSON_AddStringToObject(root, "method", method) &&
		cJSON_AddStringToObject(root, "startup_id", text) &&
		output_frame(bus, root);
out:
	cJSON_Delete(root);
	if (!valid)
		bus->failed = true;
	return valid;
}

static void dispatch_message(struct file_manager_bus *bus, DBusMessage *message)
{
	const char *path, *interface, *member;

	if (dbus_message_contains_unix_fds(message))
	{
		native_error(bus, message, DBUS_ERROR_INVALID_ARGS,
			"unix_fds_not_supported");
		return;
	}
	if (dbus_message_is_signal(message, DBUS_INTERFACE_LOCAL,
	    "Disconnected"))
	{
		bus->failed = true;
		return;
	}
	if (dbus_message_is_signal(message, DBUS_INTERFACE_DBUS, "NameLost") &&
	    dbus_message_has_sender(message, DBUS_SERVICE_DBUS) &&
	    dbus_message_has_path(message, DBUS_PATH_DBUS))
	{
		const char *name = NULL;

		if (!dbus_message_get_args(message, NULL,
		    DBUS_TYPE_STRING, &name,
		    DBUS_TYPE_INVALID) || strcmp(name, FILE_MANAGER_NAME) == 0)
			bus->failed = true;
		return;
	}
	if (dbus_message_get_type(message) != DBUS_MESSAGE_TYPE_METHOD_CALL)
		return;
	path = dbus_message_get_path(message);
	interface = dbus_message_get_interface(message);
	member = dbus_message_get_member(message);
	if (!path || strcmp(path, FILE_MANAGER_PATH) != 0)
	{
		native_error(bus, message, DBUS_ERROR_UNKNOWN_OBJECT,
			"unknown_object");
		return;
	}
	if (interface &&
	    strcmp(interface, DBUS_INTERFACE_INTROSPECTABLE) == 0 &&
	    member && strcmp(member, "Introspect") == 0)
	{
		DBusMessage *reply;
		const char *xml = introspection;

		if (!dbus_message_has_signature(message, ""))
		{
			native_error(bus, message, DBUS_ERROR_INVALID_ARGS,
				"empty_signature_required");
			return;
		}
		if (dbus_message_get_no_reply(message))
			return;
		reply = dbus_message_new_method_return(message);
		if (reply && !dbus_message_append_args(reply, DBUS_TYPE_STRING,
		    &xml, DBUS_TYPE_INVALID))
		{
			dbus_message_unref(reply);
			reply = NULL;
		}
		native_reply(bus, reply);
		return;
	}
	if (interface && strcmp(interface, "org.freedesktop.DBus.Peer") == 0 &&
	    member && strcmp(member, "Ping") == 0)
	{
		if (!dbus_message_has_signature(message, ""))
			native_error(bus, message, DBUS_ERROR_INVALID_ARGS,
				"empty_signature_required");
		else if (!dbus_message_get_no_reply(message))
			native_reply(bus,
				dbus_message_new_method_return(message));
		return;
	}
	if (!interface || strcmp(interface, FILE_MANAGER_NAME) != 0 ||
	    !member || (strcmp(member, "ShowFolders") != 0 &&
	    strcmp(member, "ShowItems") != 0 &&
	    strcmp(member, "ShowItemProperties") != 0))
	{
		native_error(bus, message, DBUS_ERROR_UNKNOWN_METHOD,
			"unknown_method");
		return;
	}
	if (bus->pending || bus->output_length)
	{
		native_error(bus, message, DBUS_ERROR_LIMITS_EXCEEDED,
			"admission_busy");
		return;
	}
	if (!bus->next_id)
	{
		bus->failed = true;
		return;
	}
	if (!request_frame(bus, message))
	{
		if (bus->failed)
			return;
		native_error(bus, message, DBUS_ERROR_INVALID_ARGS,
			"invalid_or_overlimit_envelope");
		return;
	}
	bus->pending = dbus_message_ref(message);
	bus->pending_id = bus->next_id++;
	bus->deadline = monotonic_ns();
	if (bus->deadline < 0)
		bus->failed = true;
	else
		bus->deadline += FILE_MANAGER_ADMISSION_MS * 1000000LL;
}

static void run_bus(struct file_manager_bus *bus)
{
	int fd = -1;

	if (!dbus_connection_get_unix_fd(bus->connection, &fd) || fd < 0)
	{
		bus->failed = true;
		return;
	}
	while (!bus->failed)
	{
		int64_t turn = monotonic_ns(), now;
		unsigned int count;
		struct pollfd descriptors[3] = {
			{STDIN_FILENO, POLLIN, 0},
			{STDOUT_FILENO, bus->output_length ? POLLOUT : 0, 0},
			{fd, POLLIN, 0}
		};
		int timeout = 50;

		if (turn < 0 || !dbus_connection_read_write(bus->connection, 0))
			break;
		for (count = 0; count < FILE_MANAGER_TURN_MESSAGES; count++)
		{
			DBusMessage *message;

			now = monotonic_ns();
			if (now < 0)
			{
				bus->failed = true;
				break;
			}
			if (now - turn >= FILE_MANAGER_TURN_NS || bus->failed)
				break;
			message = dbus_connection_pop_message(bus->connection);
			if (!message)
				break;
			dispatch_message(bus, message);
			dbus_message_unref(message);
		}
		input_read(bus);
		output_write(bus);
		now = monotonic_ns();
		if (now < 0)
			bus->failed = true;
		else if (bus->pending && now >= bus->deadline)
		{
			native_error(bus, bus->pending, DBUS_ERROR_TIMEOUT,
				"admission_outcome_unknown");
			pending_clear(bus);
			/* Close the producer pipe to retire partial frames
			 * and any late receipt in the parent. */
			bus->failed = true;
		}
		if (bus->failed ||
		    !dbus_connection_read_write(bus->connection, 0))
			break;
		descriptors[1].events = bus->output_length ? POLLOUT : 0;
		if (dbus_connection_get_outgoing_size(bus->connection))
			descriptors[2].events |= POLLOUT;
		if (dbus_connection_get_dispatch_status(bus->connection) ==
		    DBUS_DISPATCH_DATA_REMAINS)
			timeout = 0;
		if (bus->pending && bus->deadline - now < timeout * 1000000LL)
			timeout = (int)((bus->deadline - now) / 1000000LL);
		if (timeout < 0)
			timeout = 0;
		if (poll(descriptors, 3, timeout) < 0 && errno != EINTR)
			break;
		for (size_t i = 0; i < 3; i++)
			if (descriptors[i].revents &
			    (POLLERR | POLLHUP | POLLNVAL))
				bus->failed = true;
	}
}

int file_manager_bus_main(const char *address)
{
	struct file_manager_bus bus = {.next_id = 1};
	DBusError error;
	const char *detail = "startup_guard_failed", *owner;
	bool ready = false;
	int result;

	dbus_error_init(&error);
	if (!nonblocking(STDIN_FILENO) || !nonblocking(STDOUT_FILENO) ||
	    !ignore_sigpipe() || !module_startup_begin(MODULE_STARTUP_SECONDS))
		goto out;
	detail = "session_bus_connection_failed";
	bus.connection = dbus_connection_open_private(address, &error);
	if (!bus.connection)
		goto out;
	dbus_connection_set_exit_on_disconnect(bus.connection, FALSE);
	dbus_connection_set_max_message_size(bus.connection,
		FILE_MANAGER_FRAME_BYTES);
	dbus_connection_set_max_received_size(bus.connection,
		FILE_MANAGER_RECEIVE_BYTES);
	/* Positive limits avoid depending on a zero-limit interpretation.
	 * One admitted descriptor is bounded here and always rejected above. */
	dbus_connection_set_max_message_unix_fds(bus.connection, 1);
	dbus_connection_set_max_received_unix_fds(bus.connection, 1);
	detail = "session_bus_registration_failed";
	if (!dbus_bus_register(bus.connection, &error))
		goto out;
	detail = "filemanager_name_unavailable";
	result = dbus_bus_request_name(bus.connection, FILE_MANAGER_NAME,
		DBUS_NAME_FLAG_DO_NOT_QUEUE, &error);
	if (result != DBUS_REQUEST_NAME_REPLY_PRIMARY_OWNER)
		goto out;
	owner = dbus_bus_get_unique_name(bus.connection);
	detail = "filemanager_owner_unconfirmed";
	if (!owner || owner[0] != ':' ||
	    !file_manager_text_valid(owner, 255) ||
	    !dbus_validate_bus_name(owner, NULL))
		goto out;
	detail = "ready_frame_failed";
	if (!output_ready(&bus, owner))
		goto out;
	module_startup_finish();
	ready = true;
	run_bus(&bus);
out:
	module_startup_finish();
	pending_clear(&bus);
	if (bus.connection)
	{
		/* Closing the private connection releases our own name. */
		dbus_connection_close(bus.connection);
		dbus_connection_unref(bus.connection);
	}
	dbus_error_free(&error);
	return ready ? 1 : file_manager_bus_unavailable(detail);
}
