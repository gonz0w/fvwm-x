/* This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or any later version. */
#include "config.h"
#include "libs/module_ui_scale.h"

#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>

static module_ui_scale_snapshot example(uint64_t epoch)
{
	module_ui_scale_snapshot snapshot = {.epoch = epoch, .count = 2};

	snapshot.rows[0] = (module_ui_scale_row){
		.name_atom = 11, .quarters = 4, .primary = true,
		.rect = {-1000, 0, 1000, 800}, .name = "left display"
	};
	snapshot.rows[1] = (module_ui_scale_row){
		.name_atom = 22, .quarters = 7,
		.rect = {0, 0, 1400, 900}, .name = "right\\'\"\t\xc3\xa9"
	};
	return snapshot;
}

static module_ui_scale_status replay(module_ui_scale_receiver *receiver,
	const module_ui_scale_snapshot *snapshot)
{
	module_ui_scale_bundle bundle;
	module_ui_scale_status status = MODULE_UI_SCALE_REFUSED;

	assert(module_ui_scale_format(snapshot, &bundle));
	assert(bundle.count == snapshot->count + 2);
	for (unsigned int i = 0; i < bundle.count; i++)
	{
		module_ui_scale_snapshot before = receiver->published;

		status = module_ui_scale_receive(receiver, bundle.lines[i]);
		if (i + 1 < bundle.count)
		{
			assert(status == MODULE_UI_SCALE_STAGED);
			assert(memcmp(&before, &receiver->published,
				sizeof(before)) == 0);
		}
	}
	assert(!receiver->pending && receiver->received == 0);
	return status;
}

static void test_roundtrip_and_selection(void)
{
	module_ui_scale_snapshot snapshot = example(1), changed;
	module_ui_scale_receiver receiver = {0};
	rectangle window = {-900, 20, 100, 80};
	const module_ui_scale_row *row;

	assert(module_ui_scale_select(&receiver.published, &window, None) ==
		NULL);
	assert(replay(&receiver, &snapshot) == MODULE_UI_SCALE_PUBLISHED);
	assert(receiver.published.epoch == 1 && receiver.published.count == 2);
	assert(strcmp(receiver.published.rows[0].name, "left display") == 0);
	assert(strcmp(receiver.published.rows[1].name,
		snapshot.rows[1].name) == 0);
	assert(replay(&receiver, &snapshot) == MODULE_UI_SCALE_UNCHANGED);
	row = module_ui_scale_select(&receiver.published, &window, None);
	assert(row != NULL && row->name_atom == 11 && row->quarters == 4);
	window = (rectangle){100, 10, 200, 200};
	row = module_ui_scale_select(&receiver.published, &window, 11);
	assert(row != NULL && row->name_atom == 22 && row->quarters == 7);
	/* Equal overlap retains the incumbent by the existing policy. */
	window = (rectangle){-2, 10, 4, 1};
	assert(module_ui_scale_select(&receiver.published, &window, 22) ==
		&receiver.published.rows[1]);
	assert(module_ui_scale_select(&receiver.published, &window, None) ==
		&receiver.published.rows[0]);
	window = (rectangle){1500, 10, 1, 1};
	assert(module_ui_scale_select(&receiver.published, &window, 11) ==
		&receiver.published.rows[1]);
	window.width = 0;
	assert(module_ui_scale_select(&receiver.published, &window, None) ==
		NULL);
	window = (rectangle){INT_MAX, 0, 1, 1};
	assert(module_ui_scale_select(&receiver.published, &window, None) ==
		NULL);

	changed = snapshot;
	changed.rows[1].quarters = 8;
	assert(replay(&receiver, &changed) == MODULE_UI_SCALE_REFUSED);
	assert(receiver.published.rows[1].quarters == 7);
	changed.epoch = 9;
	changed.count = 1;
	changed.rows[0] = changed.rows[1];
	changed.rows[0].rect.x = -500;
	assert(replay(&receiver, &changed) == MODULE_UI_SCALE_PUBLISHED);
	assert(receiver.published.epoch == 9 && receiver.published.count == 1);
	window = (rectangle){-900, 20, 10, 10};
	row = module_ui_scale_select(&receiver.published, &window, 11);
	assert(row != NULL && row->name_atom == 22 && row->quarters == 8);
}

static void test_rejection_and_recovery(void)
{
	const char *bad_begin[] = {
		"MonitorUI", "MonitorUI 2 BEGIN 0000000000000002 1 0",
		"MonitorUI 1 begin 0000000000000002 1 0",
		"MonitorUI 1 BEGIN 2 1 0",
		"MonitorUI 1 BEGIN 000000000000000A 1 0",
		"MonitorUI 1 BEGIN 0000000000000000 1 0",
		"MonitorUI 1 BEGIN 0000000000000002 0 0",
		"MonitorUI 1 BEGIN 0000000000000002 65 0",
		"MonitorUI 1 BEGIN 0000000000000002 -1 0",
		"MonitorUI 1 BEGIN 0000000000000002 1 2",
		"MonitorUI 1 BEGIN 0000000000000002 1 0 trailing",
		"MonitorUI 1 BEGIN 0000000000000002 99999999999999999999 0"
	};
	const char *bad_fields[] = {
		"0 0 4 0 0 0 100 100 61",
		"0 4294967296 4 0 0 0 100 100 61",
		"0 1 3 0 0 0 100 100 61", "0 1 17 0 0 0 100 100 61",
		"0 1 4 2 0 0 100 100 61", "0 1 4 0 +1 0 100 100 61",
		"0 1 4 0 2147483648 0 100 100 61",
		"0 1 4 0 -2147483649 0 100 100 61",
		"0 1 4 0 2147483647 0 1 100 61",
		"0 1 4 0 0 2147483647 100 1 61",
		"0 1 4 0 0 0 0 100 61", "0 1 4 0 0 0 100 -1 61",
		"1 1 4 0 0 0 100 100 61", "64 1 4 0 0 0 100 100 61",
		"0 1 4 0 0 0 100 100 00", "0 1 4 0 0 0 100 100 6100",
		"0 1 4 0 0 0 100 100 6", "0 1 4 0 0 0 100 100 zz",
		"0 1 4 0 0 0 100 100 61 extra", "0 1 4 0 0 0 100 100"
	};
	module_ui_scale_receiver receiver = {0};
	module_ui_scale_snapshot snapshot = example(1), before;
	char line[MODULE_UI_SCALE_LINE_MAX + 1];

	assert(replay(&receiver, &snapshot) == MODULE_UI_SCALE_PUBLISHED);
	before = receiver.published;
	assert(module_ui_scale_receive(&receiver, "Monitor x 1 2") ==
	MODULE_UI_SCALE_IGNORED);
	assert(module_ui_scale_receive(&receiver, "MonitorUIOther") ==
	MODULE_UI_SCALE_IGNORED);
	for (size_t i = 0; i < sizeof(bad_begin) / sizeof(bad_begin[0]); i++)
	{
		assert(module_ui_scale_receive(&receiver, bad_begin[i]) ==
		MODULE_UI_SCALE_REFUSED);
		assert(!receiver.pending && memcmp(&before,
			&receiver.published, sizeof(before)) == 0);
	}
	for (size_t i = 0; i < sizeof(bad_fields) / sizeof(bad_fields[0]); i++)
	{
		assert(module_ui_scale_receive(&receiver,
			"MonitorUI 1 BEGIN 0000000000000002 1 0") ==
			MODULE_UI_SCALE_STAGED);
		snprintf(line, sizeof(line), "MonitorUI 1 ROW "
			"0000000000000002 %s", bad_fields[i]);
		assert(module_ui_scale_receive(&receiver, line) ==
		MODULE_UI_SCALE_REFUSED);
		assert(!receiver.pending && memcmp(&before,
			&receiver.published, sizeof(before)) == 0);
	}
	memset(line, 'x', sizeof(line));
	memcpy(line, "MonitorUI ", 10);
	line[sizeof(line) - 1] = '\0';
	assert(module_ui_scale_receive(&receiver, line) ==
	MODULE_UI_SCALE_REFUSED);
	assert(module_ui_scale_receive(&receiver, NULL) ==
	MODULE_UI_SCALE_REFUSED);
	assert(module_ui_scale_receive(NULL, "MonitorUI") ==
	MODULE_UI_SCALE_REFUSED);
	module_ui_scale_abort(NULL);

	assert(module_ui_scale_receive(&receiver,
		"MonitorUI 1 BEGIN 0000000000000002 2 0") ==
		MODULE_UI_SCALE_STAGED);
	assert(module_ui_scale_receive(&receiver,
		"MonitorUI 1 ROW 0000000000000002 0 1 4 0 0 0 10 10 61") ==
		MODULE_UI_SCALE_STAGED);
	assert(module_ui_scale_receive(&receiver,
		"MonitorUI 1 END 0000000000000002 2") ==
		MODULE_UI_SCALE_REFUSED);
	assert(memcmp(&before, &receiver.published, sizeof(before)) == 0);
	/* Newer BEGIN supersedes loss; delayed old traffic leaves it intact. */
	assert(module_ui_scale_receive(&receiver,
		"MonitorUI 1 BEGIN 0000000000000002 1 0") ==
		MODULE_UI_SCALE_STAGED);
	assert(module_ui_scale_receive(&receiver,
		"MonitorUI 1 BEGIN 0000000000000003 1 0") ==
		MODULE_UI_SCALE_STAGED);
	assert(module_ui_scale_receive(&receiver,
		"MonitorUI 1 ROW 0000000000000002 0 1 4 0 0 0 10 10 61") ==
		MODULE_UI_SCALE_REFUSED);
	assert(receiver.pending && receiver.staging.epoch == 3);
	assert(module_ui_scale_receive(&receiver,
		"MonitorUI 1 ROW 0000000000000003 0 1 4 0 0 0 10 10 4A") ==
		MODULE_UI_SCALE_STAGED);
	assert(module_ui_scale_receive(&receiver,
		"MonitorUI 1 END 0000000000000003 1") ==
		MODULE_UI_SCALE_PUBLISHED);
	assert(strcmp(receiver.published.rows[0].name, "J") == 0);
	assert(module_ui_scale_receive(&receiver,
		"MonitorUI 1 BEGIN 0000000000000002 1 0") ==
		MODULE_UI_SCALE_REFUSED);
	assert(module_ui_scale_receive(&receiver,
		"MonitorUI 1 BEGIN 0000000000000004 1 0") ==
		MODULE_UI_SCALE_STAGED);
	module_ui_scale_abort(&receiver);
	assert(!receiver.pending && receiver.published.epoch == 3);
	assert(module_ui_scale_receive(&receiver,
		"MonitorUI 1 END 0000000000000004 1") ==
		MODULE_UI_SCALE_REFUSED);
	/* Repeating BEGIN restarts a lost same-epoch candidate. */
	assert(module_ui_scale_receive(&receiver,
		"MonitorUI 1 BEGIN 0000000000000004 1 0") ==
		MODULE_UI_SCALE_STAGED);
	assert(module_ui_scale_receive(&receiver,
		"MonitorUI 1 ROW 0000000000000004 0 1 4 0 0 0 10 10 61") ==
		MODULE_UI_SCALE_STAGED);
	assert(module_ui_scale_receive(&receiver,
		"MonitorUI 1 BEGIN 0000000000000004 1 0") ==
		MODULE_UI_SCALE_STAGED);
	assert(receiver.pending && receiver.received == 0 &&
		receiver.published.epoch == 3);
	module_ui_scale_abort(&receiver);
	/* A duplicate row index still refuses the entire candidate. */
	assert(module_ui_scale_receive(&receiver,
		"MonitorUI 1 BEGIN 0000000000000004 2 0") ==
		MODULE_UI_SCALE_STAGED);
	assert(module_ui_scale_receive(&receiver,
		"MonitorUI 1 ROW 0000000000000004 0 1 4 0 0 0 10 10 61") ==
		MODULE_UI_SCALE_STAGED);
	assert(module_ui_scale_receive(&receiver,
		"MonitorUI 1 ROW 0000000000000004 0 2 4 0 10 0 10 10 62") ==
		MODULE_UI_SCALE_REFUSED);
	assert(!receiver.pending && receiver.published.epoch == 3);
}

static void test_bounds_and_duplicates(void)
{
	module_ui_scale_snapshot snapshot = example(UINT64_MAX), saved;
	module_ui_scale_bundle bundle;
	module_ui_scale_receiver receiver = {0};

	snapshot.count = MODULE_UI_SCALE_MAX;
	for (unsigned int i = 0; i < snapshot.count; i++)
	{
		snapshot.rows[i] = (module_ui_scale_row){
			.name_atom = i + 1, .quarters = 4 + i % 13,
			.rect = {(int)i * 1000, -1000, 1000, 1000}
		};
		snprintf(snapshot.rows[i].name, sizeof(snapshot.rows[i].name),
			"monitor %u", i);
	}
	memset(snapshot.rows[63].name, 'x', MODULE_UI_SCALE_NAME_MAX);
	snapshot.rows[63].name[MODULE_UI_SCALE_NAME_MAX] = '\0';
	assert(replay(&receiver, &snapshot) == MODULE_UI_SCALE_PUBLISHED);
	assert(strlen(receiver.published.rows[63].name) ==
	MODULE_UI_SCALE_NAME_MAX);
	assert(replay(&receiver, &snapshot) == MODULE_UI_SCALE_UNCHANGED);
	saved = snapshot;
	snapshot.rows[63].name_atom = 1;
	assert(!module_ui_scale_format(&snapshot, &bundle) && bundle.count ==
		0);
	snapshot = saved;
	strcpy(snapshot.rows[63].name, snapshot.rows[0].name);
	assert(!module_ui_scale_validate(&snapshot));
	snapshot = saved;
	memset(snapshot.rows[0].name, 'x', sizeof(snapshot.rows[0].name));
	assert(!module_ui_scale_validate(&snapshot));
	snapshot = saved;
	snapshot.count = MODULE_UI_SCALE_MAX + 1;
	assert(!module_ui_scale_validate(&snapshot));
	snapshot.count = 0;
	assert(!module_ui_scale_validate(&snapshot));
	assert(!module_ui_scale_validate(NULL));
	assert(!module_ui_scale_format(NULL, &bundle) && bundle.count == 0);
	assert(!module_ui_scale_format(&saved, NULL));

	memset(&receiver, 0, sizeof(receiver));
	for (int duplicate_name = 0; duplicate_name <= 1; duplicate_name++)
	{
		assert(module_ui_scale_receive(&receiver,
			"MonitorUI 1 BEGIN 0000000000000001 2 0") ==
			MODULE_UI_SCALE_STAGED);
		assert(module_ui_scale_receive(&receiver,
			"MonitorUI 1 ROW 0000000000000001 "
			"0 1 4 0 0 0 10 10 61") ==
			MODULE_UI_SCALE_STAGED);
		assert(module_ui_scale_receive(&receiver, duplicate_name ?
			"MonitorUI 1 ROW 0000000000000001 "
			"1 2 4 0 10 0 10 10 61" :
			"MonitorUI 1 ROW 0000000000000001 "
			"1 1 4 0 10 0 10 10 62") ==
			MODULE_UI_SCALE_REFUSED);
		assert(!receiver.pending && receiver.published.epoch == 0);
	}
	snapshot = example(1);
	snapshot.count = 1;
	snapshot.root_fallback = true;
	assert(!module_ui_scale_validate(&snapshot));
	snapshot.rows[0].rect.x = 0;
	assert(replay(&receiver, &snapshot) == MODULE_UI_SCALE_PUBLISHED);
	snapshot.rows[0].primary = false;
	assert(!module_ui_scale_validate(&snapshot));
	snapshot.rows[0].primary = true;
	snapshot.rows[0].rect = (rectangle){INT_MIN, INT_MIN, 1, 1};
	snapshot.root_fallback = false;
	snapshot.epoch = 2;
	assert(replay(&receiver, &snapshot) == MODULE_UI_SCALE_PUBLISHED);
}

int main(void)
{
	test_roundtrip_and_selection();
	test_rejection_and_recovery();
	test_bounds_and_duplicates();
	return 0;
}
