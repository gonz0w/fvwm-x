/* This program is free software; you can redistribute it and/or modify
 * it under the GNU General Public License as published by the Free Software
 * Foundation; either version 2, or any later version. */
#include "config.h"
#include "bin/settings_portal_policy.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

struct fixture
{
	unsigned char data[SP_SETTINGS_MAX + 4];
	size_t used;
	uint32_t count;
	bool big;
};

static void
put16(struct fixture *f, size_t at, uint16_t value)
{
	f->data[at] = f->big ? value >> 8 : value;
	f->data[at + 1] = f->big ? value : value >> 8;
}

static void
put32(struct fixture *f, size_t at, uint32_t value)
{
	for (size_t i = 0; i < 4; i++)
		f->data[at + i] = value >> (8 * (f->big ? 3 - i : i));
}

static void
start(struct fixture *f, bool big, uint32_t serial)
{
	memset(f, 0, sizeof(*f));
	f->big = big;
	f->data[0] = big ? 1 : 0;
	f->used = 12;
	put32(f, 4, serial);
}

static size_t
record(struct fixture *f, unsigned char type, const char *name,
	uint32_t value)
{
	size_t name_length = strlen(name), value_at;

	assert(f->used + name_length + 20 <= sizeof(f->data));
	f->data[f->used] = type;
	put16(f, f->used + 2, (uint16_t)name_length);
	memcpy(f->data + f->used + 4, name, name_length);
	f->used = (f->used + 4 + name_length + 3) & ~(size_t)3;
	put32(f, f->used, UINT32_MAX);
	f->used += 4;
	value_at = f->used;
	put32(f, f->used, value);
	f->used += type == 2 ? 8 : 4;
	if (type == 1)
	{
		assert(value <= sizeof(f->data) - f->used);
		memset(f->data + f->used, 'x', value);
		f->used = (f->used + value + 3) & ~(size_t)3;
	}
	put32(f, 8, ++f->count);
	return value_at;
}

static void
rejected(const unsigned char *data, size_t length)
{
	sp_snapshot snapshot = {42, 42};

	assert(!sp_decode(data, length, &snapshot));
	assert(snapshot.serial == 0 && snapshot.reduced_motion == 0);
}

static void
test_decoder(bool big)
{
	struct fixture f;
	sp_snapshot snapshot;
	const char *target = "Gtk/EnableAnimations";

	start(&f, big, UINT32_MAX);
	assert(sp_decode(f.data, f.used, &snapshot));
	assert(snapshot.serial == UINT32_MAX && !snapshot.reduced_motion);
	record(&f, 1, "Gtk/FontName", 7);
	record(&f, 2, "Example/Color", 0x12345678);
	record(&f, 0, "Other/Integer", 0x80000000);
	size_t value_at = record(&f, 0, target, 0);
	assert(sp_decode(f.data, f.used, &snapshot));
	assert(snapshot.reduced_motion == 1 && snapshot.serial == UINT32_MAX);
	put32(&f, value_at, 1);
	put32(&f, 4, 0);
	assert(sp_decode(f.data, f.used, &snapshot));
	assert(snapshot.serial == 0 && !snapshot.reduced_motion);
	/* No prefix of a complete snapshot may be adopted. */
	for (size_t i = 0; i < f.used; i++)
		rejected(f.data, i);
	rejected(f.data, f.used + 1);
	put32(&f, value_at, 2);
	rejected(f.data, f.used);
	put32(&f, value_at, UINT32_MAX);
	rejected(f.data, f.used);
	put32(&f, value_at, 0);
	record(&f, 0, target, 0);
	rejected(f.data, f.used);
	start(&f, big, 1);
	record(&f, 1, target, 0);
	rejected(f.data, f.used);
	start(&f, big, 1);
	record(&f, 2, target, 0);
	rejected(f.data, f.used);
	start(&f, big, 1);
	record(&f, 3, "Other/Unknown", 0);
	rejected(f.data, f.used);
	start(&f, big, 1);
	record(&f, 0, "", 0);
	rejected(f.data, f.used);
	start(&f, big, 1);
	record(&f, 0, "Name", 0);
	f.data[16] = 0;
	rejected(f.data, f.used);
	start(&f, big, 1);
	value_at = record(&f, 1, "Name", 1);
	put32(&f, value_at, UINT32_MAX);
	rejected(f.data, f.used);
	start(&f, big, 1);
	record(&f, 0, "Name", 0);
	put16(&f, 14, UINT16_MAX);
	rejected(f.data, f.used);
	start(&f, big, 1);
	f.data[0] = 2;
	rejected(f.data, f.used);
	start(&f, big, 1);
	put32(&f, 8, UINT32_MAX);
	rejected(f.data, f.used);
	start(&f, big, 1);
	for (size_t i = 0; i < SP_RECORDS_MAX; i++)
		record(&f, 0, "Other", 1);
	assert(sp_decode(f.data, f.used, &snapshot));
	record(&f, 0, "Other", 1);
	rejected(f.data, f.used);
	/* Header12 + record header/name8 + serial4 + string length4. */
	start(&f, big, 1);
	record(&f, 1, "Name", SP_SETTINGS_MAX - 28);
	assert(f.used == SP_SETTINGS_MAX);
	assert(sp_decode(f.data, f.used, &snapshot));
	rejected(f.data, f.used + 4);
	start(&f, big, 1);
	record(&f, 1, "Name", 0);
	assert(sp_decode(f.data, f.used, &snapshot));
	rejected(NULL, 12);
}

static void
test_patterns(void)
{
	const char *exact[] = {SP_NAMESPACE};
	const char *prefix[] = {"org.freedesktop.*"};
	const char *other[] = {"org.example.*"};
	const char *parent[] = {"org.freedesktop.appearance.*"};
	const char *empty[] = {""};
	const char *all[] = {"*"};
	const char *invalid[] = {"org.*.appearance"};
	const char *many[SP_PATTERNS_MAX + 1];
	char limit[SP_PATTERN_MAX + 2];

	assert(sp_namespace_matches(NULL, 0));
	assert(sp_namespace_matches(exact, 1));
	assert(sp_namespace_matches(prefix, 1));
	assert(!sp_namespace_matches(other, 1));
	assert(!sp_namespace_matches(parent, 1));
	assert(sp_namespace_matches(empty, 1));
	assert(sp_namespace_matches(all, 1));
	assert(!sp_namespace_matches(invalid, 1));
	assert(!sp_pattern_valid(NULL));
	assert(!sp_pattern_valid("org.freedesktop*"));
	assert(!sp_pattern_valid("org.**"));
	assert(!sp_pattern_valid("org.\nappearance"));
	memset(limit, 'a', sizeof(limit));
	limit[SP_PATTERN_MAX] = 0;
	assert(sp_pattern_valid(limit));
	limit[SP_PATTERN_MAX] = 'a';
	limit[SP_PATTERN_MAX + 1] = 0;
	assert(!sp_pattern_valid(limit));
	for (size_t i = 0; i < SP_PATTERNS_MAX + 1; i++)
		many[i] = SP_NAMESPACE;
	assert(sp_namespace_matches(many, SP_PATTERNS_MAX));
	assert(!sp_namespace_matches(many, SP_PATTERNS_MAX + 1));
	assert(!sp_namespace_matches(NULL, 1));
}

static void
test_commit(void)
{
	uint32_t value = 0;
	sp_snapshot snapshot = {UINT32_MAX, 1};

	assert(sp_commit(&value, &snapshot) && value == 1);
	assert(!sp_commit(&value, &snapshot));
	snapshot.serial = 0;
	assert(!sp_commit(&value, &snapshot));
	/* A new owner's lower serial and unavailable source are not stale. */
	snapshot.serial = 1;
	snapshot.reduced_motion = 0;
	assert(sp_commit(&value, &snapshot) && value == 0);
	assert(!sp_commit(&value, NULL));
	snapshot.reduced_motion = 1;
	assert(sp_commit(&value, &snapshot));
	assert(sp_commit(&value, NULL) && value == 0);
	assert(!sp_commit(&value, NULL));
}

int
main(void)
{
	test_decoder(false);
	test_decoder(true);
	test_patterns();
	test_commit();
	puts("settings portal decoder, filters and mapping passed");
	return 0;
}
