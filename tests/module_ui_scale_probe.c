/* This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */
#include "config.h"
#include "libs/Module.h"
#include "libs/Parse.h"
#include "libs/cJSON.h"
#include "libs/module_monitor.h"
#include "libs/module_packet_reader.h"
#include "libs/module_ui_scale.h"

#include <errno.h>
#include <inttypes.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <time.h>
#include <unistd.h>

/* This non-installed native consumer uses production framing, parsing and
 * ownership policy. Its only fault gate omits one received END, recording the
 * original bytes before the harness explicitly aborts/replays that snapshot. */
struct probe {
	module_ui_scale_receiver receiver;
	Display *display;
	FILE *report;
	const char *report_path;
	int descriptors[2];
	Atom incumbent;
	bool omit_end;
	char request[65];
	size_t bytes;
};

static volatile sig_atomic_t stopping;

static void stop_probe(int signum)
{
	(void)signum;
	stopping = 1;
}

static const char *status_name(module_ui_scale_status status)
{
	switch (status)
	{
	case MODULE_UI_SCALE_IGNORED: return "ignored";
	case MODULE_UI_SCALE_STAGED: return "staged";
	case MODULE_UI_SCALE_PUBLISHED: return "published";
	case MODULE_UI_SCALE_UNCHANGED: return "unchanged";
	case MODULE_UI_SCALE_REFUSED: return "refused";
	}
	return "invalid-status";
}

static cJSON *snapshot_json(const module_ui_scale_snapshot *snapshot)
{
	cJSON *object = cJSON_CreateObject(), *rows;
	char epoch[17];

	snprintf(epoch, sizeof(epoch), "%016" PRIx64, snapshot->epoch);
	if (!object || !cJSON_AddStringToObject(object, "epoch", epoch) ||
	    !cJSON_AddBoolToObject(object, "root_fallback",
		snapshot->root_fallback) ||
	    !(rows = cJSON_AddArrayToObject(object, "rows")))
		goto failed;
	for (unsigned int i = 0; i < snapshot->count; i++)
	{
		const module_ui_scale_row *row = &snapshot->rows[i];
		char name[2 * MODULE_UI_SCALE_NAME_MAX + 1];
		size_t length = strnlen(row->name, sizeof(row->name));
		cJSON *item = cJSON_CreateObject();

		if (length == sizeof(row->name) || !item)
		{
			cJSON_Delete(item);
			goto failed;
		}
		for (size_t j = 0; j < length; j++)
			snprintf(name + 2 * j, 3, "%02x",
				(unsigned char)row->name[j]);
		name[2 * length] = '\0';
		if (!cJSON_AddItemToArray(rows, item))
		{
			cJSON_Delete(item);
			goto failed;
		}
		if (!cJSON_AddNumberToObject(item, "atom", row->name_atom) ||
		    !cJSON_AddNumberToObject(item, "quarters", row->quarters) ||
		    !cJSON_AddBoolToObject(item, "primary", row->primary) ||
		    !cJSON_AddStringToObject(item, "name_hex", name) ||
		    !cJSON_AddNumberToObject(item, "x", row->rect.x) ||
		    !cJSON_AddNumberToObject(item, "y", row->rect.y) ||
		    !cJSON_AddNumberToObject(item, "width", row->rect.width) ||
		    !cJSON_AddNumberToObject(item, "height", row->rect.height))
			goto failed;
	}
	return object;
failed:
	cJSON_Delete(object);
	return NULL;
}

static cJSON *event_json(struct probe *probe, const char *event,
	const char *token)
{
	cJSON *object = cJSON_CreateObject();
	cJSON *snapshot = snapshot_json(&probe->receiver.published);

	if (!object || !snapshot)
		goto failed;
	if (!cJSON_AddItemToObject(object, "snapshot", snapshot))
		goto failed;
	snapshot = NULL;
	if (!cJSON_AddStringToObject(object, "event", event) ||
	    !cJSON_AddStringToObject(object, "token", token) ||
	    !cJSON_AddBoolToObject(object, "pending", probe->receiver.pending) ||
	    !cJSON_AddNumberToObject(object, "received", probe->receiver.received) ||
	    !cJSON_AddNumberToObject(object, "pid", getpid()))
		goto failed;
	return object;
failed:
	cJSON_Delete(snapshot);
	cJSON_Delete(object);
	return NULL;
}

static bool emit(struct probe *probe, cJSON *object)
{
	char *text = object ? cJSON_PrintUnformatted(object) : NULL;
	bool ok = false;

	cJSON_Delete(object);
	if (text)
	{
		size_t length = strlen(text);

		if (length < 8 * 1024 * 1024 - probe->bytes &&
		    fprintf(probe->report, "%s\n", text) >= 0 &&
		    fflush(probe->report) == 0)
		{
			probe->bytes += length + 1;
			ok = true;
		}
		cJSON_free(text);
	}
	return ok;
}

static bool config_record(struct probe *probe, const char *text)
{
	bool omitted = probe->omit_end &&
		strncmp(text, "MonitorUI 1 END ", 16) == 0;
	module_ui_scale_status status = MODULE_UI_SCALE_IGNORED;
	cJSON *object;

	if (omitted)
		probe->omit_end = false;
	else
		status = module_ui_scale_receive(&probe->receiver, text);
	if (strncmp(text, "MonitorUI ", 10) != 0 &&
	    strncmp(text, "Monitor ", 8) != 0)
		return true;
	object = event_json(probe, omitted ? "omitted-end" : "config",
		probe->request);
	if (!object || !cJSON_AddStringToObject(object, "raw", text) ||
	    !cJSON_AddStringToObject(object, "status", status_name(status)))
		goto failed;
	if (strncmp(text, "Monitor ", 8) == 0)
	{
		char *name = NULL;
		const char *tail = GetNextToken(text + 8, &name);
		int values[MODULE_MONITOR_VALUES];
		bool valid = name && tail && module_parse_decimal_integers(
			tail, values, MODULE_MONITOR_VALUES);
		cJSON *list = cJSON_AddArrayToObject(object, "legacy_values");

		free(name);
		if (!list || !cJSON_AddBoolToObject(object, "legacy_valid", valid))
			goto failed;
		for (int i = 0; valid && i < MODULE_MONITOR_VALUES; i++)
			if (!cJSON_AddItemToArray(list,
				cJSON_CreateNumber(values[i])))
				goto failed;
	}
	return emit(probe, object);
failed:
	cJSON_Delete(object);
	return false;
}

static bool control_record(struct probe *probe, const char *text, bool *unlocked)
{
	char operation[17], token[65], extra;
	unsigned long window = 0;
	int fields = sscanf(text, "%16s %64s %lu %c", operation, token,
		&window, &extra);
	cJSON *object;

	if (fields < 2 || strspn(token,
	    "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-") !=
	    strlen(token))
		return false;
	if (strcmp(operation, "request") == 0 && fields == 2)
	{
		strcpy(probe->request, token);
		/* Retire this synchronized string before requesting a new
		 * synchronized config stream. Never lend its ACK to a nested row. */
		SendUnlockNotification(probe->descriptors);
		*unlocked = true;
		SendText(probe->descriptors, "Send_ConfigInfo", None);
	}
	else if (strcmp(operation, "omit-end") == 0 && fields == 2 &&
	    !probe->omit_end)
		probe->omit_end = true;
	else if (strcmp(operation, "abort") == 0 && fields == 2)
		module_ui_scale_abort(&probe->receiver);
	else if (strcmp(operation, "quit") == 0 && fields == 2)
		stopping = 1;
	else if (strcmp(operation, "barrier") == 0 && fields == 2)
	{
		char path[4096];
		struct timespec start, now, pause = {0, 5000000};
		int length = snprintf(path, sizeof(path), "%s.%s.release",
			probe->report_path, token);

		if (length < 0 || (size_t)length >= sizeof(path) ||
		    clock_gettime(CLOCK_MONOTONIC, &start) != 0 ||
		    !emit(probe, event_json(probe, "barrier", token)))
			return false;
		while (access(path, F_OK) != 0 && !stopping)
		{
			if (clock_gettime(CLOCK_MONOTONIC, &now) != 0 ||
			    now.tv_sec - start.tv_sec > 1 ||
			    (now.tv_sec - start.tv_sec) * 1000000000LL +
			    now.tv_nsec - start.tv_nsec >= 1000000000LL)
				return false;
			nanosleep(&pause, NULL);
		}
		return !stopping && emit(probe,
			event_json(probe, "barrier-released", token));
	}
	else if (strcmp(operation, "observe") != 0 || fields != 3 ||
	    window == None || window > UINT32_MAX)
		return false;
	object = event_json(probe, operation, token);
	if (object && strcmp(operation, "observe") == 0)
	{
		XWindowAttributes attributes;
		Window child;
		int root_x, root_y;
		rectangle actual;
		const module_ui_scale_row *row;

		if (!XGetWindowAttributes(probe->display, window, &attributes) ||
		    !XTranslateCoordinates(probe->display, window,
			DefaultRootWindow(probe->display), 0, 0,
			&root_x, &root_y, &child))
			goto failed;
		actual = (rectangle){root_x, root_y,
			attributes.width, attributes.height};
		row = module_ui_scale_select(&probe->receiver.published,
			&actual, probe->incumbent);
		if (!row || !cJSON_AddNumberToObject(object, "window", window) ||
		    !cJSON_AddNumberToObject(object, "x", actual.x) ||
		    !cJSON_AddNumberToObject(object, "y", actual.y) ||
		    !cJSON_AddNumberToObject(object, "width", actual.width) ||
		    !cJSON_AddNumberToObject(object, "height", actual.height) ||
		    !cJSON_AddNumberToObject(object, "owner_atom", row->name_atom) ||
		    !cJSON_AddNumberToObject(object, "quarters", row->quarters))
			goto failed;
		probe->incumbent = row->name_atom;
	}
	return emit(probe, object);
failed:
	cJSON_Delete(object);
	return false;
}

int main(int argc, char **argv)
{
	ModuleArgs *args = ParseModuleArgs(argc, argv, 0);
	module_packet_reader reader;
	struct probe probe = {0};
	int result = 0;
	bool startup_notified = false;
	time_t deadline = time(NULL) + 120;

	if (!args || args->user_argc != 1 || args->from_fvwm < 0 ||
	    args->from_fvwm >= FD_SETSIZE)
		return 2;
	probe.report = fopen(args->user_argv[0], "w");
	probe.report_path = args->user_argv[0];
	probe.display = XOpenDisplay(NULL);
	probe.descriptors[0] = args->to_fvwm;
	probe.descriptors[1] = args->from_fvwm;
	if (!probe.report || !probe.display ||
	    !module_packet_reader_init(&reader, args->from_fvwm))
	{
		result = 3;
		goto done;
	}
	signal(SIGTERM, stop_probe);
	signal(SIGINT, stop_probe);
	SetMessageMask(probe.descriptors,
		M_CONFIG_INFO | M_END_CONFIG_INFO | M_STRING);
	SetSyncMask(probe.descriptors, M_CONFIG_INFO | M_STRING);
	strcpy(probe.request, "startup");
	SendText(probe.descriptors, "Send_ConfigInfo", None);
	while (!stopping && time(NULL) < deadline)
	{
		fd_set read_set;
		struct timeval wait = {1, 0};
		FvwmPacket *packet = NULL;
		module_packet_read_status status;

		FD_ZERO(&read_set);
		FD_SET(args->from_fvwm, &read_set);
		if (select(args->from_fvwm + 1, &read_set, NULL, NULL, &wait) < 0)
		{
			if (errno == EINTR) continue;
			result = 4;
			break;
		}
		if (!FD_ISSET(args->from_fvwm, &read_set)) continue;
		status = module_packet_reader_read(&reader, &packet);
		if (status == MODULE_PACKET_READ_MORE) continue;
		if (status == MODULE_PACKET_READ_ENDED) break;
		if (status != MODULE_PACKET_READ_READY)
		{
			result = 5;
			break;
		}
		if (packet->type == M_CONFIG_INFO || packet->type == M_STRING)
		{
			bool unlocked = false;
			bool valid = module_packet_body_has_text(packet->body,
				FvwmPacketBodySize(*packet), 3);
			const char *text = (const char *)(packet->body + 3);

			if (valid)
				valid = packet->type == M_CONFIG_INFO ?
					config_record(&probe, text) :
					control_record(&probe, text, &unlocked);
			if (!unlocked)
				SendUnlockNotification(probe.descriptors);
			if (!valid) { result = 6; break; }
		}
		else if (packet->type == M_END_CONFIG_INFO)
		{
			module_ui_scale_abort(&probe.receiver);
			if (!emit(&probe, event_json(&probe, "end-config", probe.request)))
			{
				result = 7;
				break;
			}
			probe.request[0] = '\0';
			if (!startup_notified)
			{
				SendFinishedStartupNotification(probe.descriptors);
				startup_notified = true;
			}
		}
	}
	if (!stopping && time(NULL) >= deadline) result = 8;
done:
	module_ui_scale_abort(&probe.receiver);
	if (probe.report && !emit(&probe, event_json(&probe, "closed", "")))
		result = 9;
	if (probe.display) XCloseDisplay(probe.display);
	if (probe.report && fclose(probe.report) != 0) result = 10;
	return result;
}
