/* This program is free software; you can redistribute it and/or modify it
 * under the GNU General Public License, version 2 or any later version. */
#include "config.h"
#include "modules/FvwmNotify/notify_bus.h"
#include <dbus/dbus.h>
#include <assert.h>
#include <stdlib.h>
#include <string.h>

static const char *hint_text = "not interpreted";

static void add_text(DBusMessageIter *iter, const char *text)
{
	assert(dbus_message_iter_append_basic(iter, DBUS_TYPE_STRING, &text));
}

static void add_hint(DBusMessageIter *array, const char *name, int type,
	const void *value)
{
	DBusMessageIter entry, variant;
	char signature[2] = {(char)type, 0};
	assert(dbus_message_iter_open_container(array, DBUS_TYPE_DICT_ENTRY,
		NULL, &entry));
	add_text(&entry, name);
	assert(dbus_message_iter_open_container(&entry, DBUS_TYPE_VARIANT,
		signature, &variant));
	assert(dbus_message_iter_append_basic(&variant, type, value));
	assert(dbus_message_iter_close_container(&entry, &variant));
	assert(dbus_message_iter_close_container(array, &entry));
}

/* Only constructs messages for pure decoding. No bus, display or daemon. */
static DBusMessage *message(const char *summary, const char *body,
	unsigned int action_strings, unsigned int hint_count,
	int urgency_type, unsigned char urgency, int32_t timeout)
{
	DBusMessage *call = dbus_message_new_method_call(
		"org.freedesktop.Notifications", "/org/freedesktop/Notifications",
		"org.freedesktop.Notifications", "Notify");
	DBusMessageIter root, actions, hints;
	uint32_t replaces = 0;
	assert(call && dbus_message_set_sender(call, ":1.2"));
	dbus_message_set_serial(call, 1);
	dbus_message_iter_init_append(call, &root);
	add_text(&root, "Test application");
	assert(dbus_message_iter_append_basic(&root, DBUS_TYPE_UINT32, &replaces));
	add_text(&root, "ignored-icon");
	add_text(&root, summary);
	add_text(&root, body);
	assert(dbus_message_iter_open_container(&root, DBUS_TYPE_ARRAY,
		"s", &actions));
	for (unsigned int i = 0; i < action_strings; i++)
		add_text(&actions, i % 2 ? "Open" : "default");
	assert(dbus_message_iter_close_container(&root, &actions));
	assert(dbus_message_iter_open_container(&root, DBUS_TYPE_ARRAY,
		"{sv}", &hints));
	for (unsigned int i = 0; i < hint_count; i++)
	{
		if (!i && urgency_type)
		{
			if (urgency_type == DBUS_TYPE_BYTE)
				add_hint(&hints, "urgency", DBUS_TYPE_BYTE, &urgency);
			else
			{
				const char *wrong = "critical";
				add_hint(&hints, "urgency", DBUS_TYPE_STRING, &wrong);
			}
		}
		else
		{
			const char *ignored = hint_text;
			add_hint(&hints, "x-test-extension", DBUS_TYPE_STRING,
				&ignored);
		}
	}
	assert(dbus_message_iter_close_container(&root, &hints));
	assert(dbus_message_iter_append_basic(&root, DBUS_TYPE_INT32, &timeout));
	return call;
}

static void decoding(void)
{
	notify_request request;
	DBusMessage *call = message("Summary", "Body\nnext", 2, 1,
		DBUS_TYPE_BYTE, 2, -1);
	assert(notify_bus_parse_notify(call, &request));
	assert(!strcmp(request.record.owner, ":1.2"));
	assert(!strcmp(request.record.summary, "Summary"));
	assert(!strcmp(request.record.body, "Body\nnext"));
	assert(request.record.action_count == 1 && request.record.urgency == 2);
	assert(!strcmp(request.record.actions[0].key, "default"));
	assert(request.timeout == -1);
	dbus_message_unref(call);
	call = message("", "", 0, 0, 0, 0, 0);
	assert(notify_bus_parse_notify(call, &request));
	assert(request.record.urgency == 1 && !request.record.action_count);
	dbus_message_unref(call);
	call = message("data: $(touch nothing)", "<b>literal</b>", 0, 1,
		0, 0, 86400000);
	assert(notify_bus_parse_notify(call, &request));
	assert(!strcmp(request.record.body, "<b>literal</b>"));
	dbus_message_unref(call);
}

static void refusals(void)
{
	notify_request request, empty = {0};
	DBusMessage *call;
	for (unsigned int i = 0; i < 8; i++)
	{
		switch (i)
		{
		case 0: call = message("Summary", "", 1, 0, 0, 0, -1); break;
		case 1: call = message("Summary", "", 10, 0, 0, 0, -1); break;
		case 2: call = message("Summary", "", 0, 1, DBUS_TYPE_BYTE, 3, -1); break;
		case 3: call = message("Summary", "", 0, 1, DBUS_TYPE_STRING, 0, -1); break;
		case 4: call = message("Summary", "", 0, 33, 0, 0, -1); break;
		case 5: call = message("Summary", "", 0, 0, 0, 0, -2); break;
		case 6: call = message("Summary", "", 0, 0, 0, 0, 86400001); break;
		default: call = message("Bad\nsummary", "", 0, 0, 0, 0, -1); break;
		}
		memset(&request, 0x7f, sizeof(request));
		assert(!notify_bus_parse_notify(call, &request));
		assert(!memcmp(&request, &empty, sizeof(request)));
		dbus_message_unref(call);
	}
	call = dbus_message_new_method_call("org.freedesktop.Notifications",
		"/org/freedesktop/Notifications", "org.freedesktop.Notifications",
		"Notify");
	assert(call && dbus_message_set_sender(call, ":1.2"));
	dbus_message_set_serial(call, 1);
	assert(!notify_bus_parse_notify(call, &request));
	dbus_message_unref(call);
	call = message("Summary", "", 0, 0, 0, 0, -1);
	assert(dbus_message_set_sender(call, NULL));
	assert(!notify_bus_parse_notify(call, &request));
	dbus_message_unref(call);
}

static void lengths(void)
{
	char body[NOTIFY_BODY_MAX + 2];
	char summary[NOTIFY_SUMMARY_MAX + 2];
	notify_request request;
	DBusMessage *call;
	memset(body, 'x', sizeof(body));
	body[NOTIFY_BODY_MAX] = 0;
	memset(summary, 'x', sizeof(summary));
	summary[NOTIFY_SUMMARY_MAX] = 0;
	call = message(summary, body, 0, 0, 0, 0, -1);
	assert(notify_bus_parse_notify(call, &request));
	dbus_message_unref(call);
	body[NOTIFY_BODY_MAX] = 'x';
	body[NOTIFY_BODY_MAX + 1] = 0;
	call = message("Summary", body, 0, 0, 0, 0, -1);
	assert(!notify_bus_parse_notify(call, &request));
	dbus_message_unref(call);
	summary[NOTIFY_SUMMARY_MAX] = 'x';
	summary[NOTIFY_SUMMARY_MAX + 1] = 0;
	call = message(summary, "", 0, 0, 0, 0, -1);
	assert(!notify_bus_parse_notify(call, &request));
	dbus_message_unref(call);
	/* Individual fields can fit while the complete wire message does not. */
	body[NOTIFY_BODY_MAX] = 0;
	hint_text = body;
	call = message("Summary", "", 0, 16, 0, 0, -1);
	assert(!notify_bus_parse_notify(call, &request));
	dbus_message_unref(call);
	hint_text = "not interpreted";
}

static void local_addresses(void)
{
	assert(notify_bus_address_valid("unix:path=/tmp/test-bus"));
	assert(notify_bus_address_valid("unix:abstract=test-bus"));
	assert(notify_bus_address_valid("unix:path=/tmp/test-bus,"
		"guid=0123456789abcdef0123456789abcdef"));
	assert(!notify_bus_address_valid(NULL));
	assert(!notify_bus_address_valid(""));
	assert(!notify_bus_address_valid("autolaunch:"));
	assert(!notify_bus_address_valid("tcp:host=localhost,port=1234"));
	assert(!notify_bus_address_valid("unix:path=relative"));
	assert(!notify_bus_address_valid("unix:path=/tmp/one;unix:path=/tmp/two"));
	assert(!notify_bus_address_valid("unix:path=/tmp/one,abstract=two"));
	assert(!notify_bus_address_valid("unix:path=/tmp/one,key=value"));
}

int main(void)
{
	decoding();
	refusals();
	lengths();
	local_addresses();
	return 0;
}
