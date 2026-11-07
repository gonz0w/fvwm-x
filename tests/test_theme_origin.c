/* This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or any later version.
 */
#include "config.h"
#include "libs/theme_origin.h"
#include <assert.h>
#include <stdint.h>
#include <string.h>

static bool override_charge;
static size_t injected_charge;
size_t __real_fvwm_config_origin_storage_bytes(
	const fvwm_config_origin *source);
size_t __wrap_fvwm_config_origin_storage_bytes(const fvwm_config_origin *source)
{
	return override_charge ? injected_charge :
		__real_fvwm_config_origin_storage_bytes(source);
}

static void test_owned_operations(void)
{
	fvwm_theme_origin first = FVWM_THEME_ORIGIN_INIT;
	fvwm_theme_origin copy = FVWM_THEME_ORIGIN_INIT;
	fvwm_theme_origin moved = FVWM_THEME_ORIGIN_INIT;
	fvwm_config_origin *root = fvwm_config_origin_file("/root.cfg", 2, 3,
		NULL);
	fvwm_config_origin *child = fvwm_config_origin_file("/child.cfg", 8, 9,
		root);
	size_t charge = fvwm_config_origin_storage_bytes(child);

	assert(first.operation == FVWM_THEME_BUILTIN_DEFAULT);
	fvwm_theme_origin_definition(&first, child, FVWM_THEME_MERGE, NULL);
	assert(!first.replaces_definition && first.charge == charge);
	fvwm_theme_origin_clone(&copy, &first);
	assert(fvwm_theme_origin_retained_bytes() == 2 * charge);
	fvwm_config_origin_release(child);
	fvwm_config_origin_release(root);
	assert(strstr(fvwm_config_origin_json(copy.source), "/root.cfg"));
	/* Replacement can borrow its source and donor from the old owner. */
	fvwm_theme_origin_definition(&first, first.source,
		FVWM_THEME_COPY, "*");
	assert(first.replaces_definition && !strcmp(first.copied_from, "*"));
	fvwm_theme_origin_definition(&first, first.source, FVWM_THEME_COPY,
		first.copied_from);
	assert(first.retention == FVWM_THEME_RETAINED);
	fvwm_theme_origin_move(&moved, &first);
	assert(!first.source && !first.charge && !first.copied_from);
	assert(!strcmp(moved.copied_from, "*"));
	fvwm_theme_origin_preview(&copy);
	assert(!copy.source && !copy.charge && !copy.copied_from);
	assert(copy.operation == FVWM_THEME_PREVIEW);
	assert(copy.retention == FVWM_THEME_NOT_APPLICABLE);
	fvwm_theme_origin_clear(&copy);
	fvwm_theme_origin_clear(&moved);
	fvwm_theme_origin_clear(&first);
	assert(fvwm_theme_origin_retained_bytes() == 0);
}

static void test_shared_budget(void)
{
	fvwm_theme_origin slots[3] = {FVWM_THEME_ORIGIN_INIT,
		FVWM_THEME_ORIGIN_INIT, FVWM_THEME_ORIGIN_INIT};
	fvwm_config_origin *source = fvwm_config_origin_file("/budget.cfg", 1,
		1, NULL);

	override_charge = true;
	injected_charge = FVWM_THEME_ORIGIN_MAX_BYTES / 2;
	fvwm_theme_origin_definition(&slots[0], source, FVWM_THEME_MERGE, NULL);
	fvwm_theme_origin_clone(&slots[1], &slots[0]);
	assert(fvwm_theme_origin_retained_bytes() ==
		FVWM_THEME_ORIGIN_MAX_BYTES);
	fvwm_theme_origin_clone(&slots[2], &slots[1]);
	assert(slots[2].retention == FVWM_THEME_BUDGET_EXHAUSTED);
	assert(!slots[2].source && !slots[2].charge);
	/* Retain before release refuses a replacement at the shared cap. */
	fvwm_theme_origin_definition(&slots[0], slots[0].source,
		FVWM_THEME_RESET, NULL);
	assert(slots[0].retention == FVWM_THEME_BUDGET_EXHAUSTED);
	assert(slots[0].replaces_definition);
	fvwm_theme_origin_clear(&slots[1]);
	injected_charge = FVWM_THEME_ORIGIN_MAX_BYTES - 2;
	fvwm_theme_origin_definition(&slots[0], source, FVWM_THEME_COPY, "*");
	assert(slots[0].charge == FVWM_THEME_ORIGIN_MAX_BYTES);
	fvwm_theme_origin_clone(&slots[1], &slots[0]);
	assert(slots[1].retention == FVWM_THEME_BUDGET_EXHAUSTED);
	assert(!slots[1].copied_from && !slots[1].source);
	fvwm_theme_origin_clear(&slots[0]);
	injected_charge = SIZE_MAX;
	fvwm_theme_origin_definition(&slots[0], source, FVWM_THEME_MERGE, NULL);
	assert(slots[0].retention == FVWM_THEME_BUDGET_EXHAUSTED);
	override_charge = false;
	for (unsigned int i = 0; i < 3; i++)
		fvwm_theme_origin_clear(&slots[i]);
	fvwm_config_origin_release(source);
	assert(fvwm_theme_origin_retained_bytes() == 0);
}

static void test_unavailable_donors(void)
{
	fvwm_theme_origin slot = FVWM_THEME_ORIGIN_INIT;
	char donor[FVWM_THEME_DONOR_MAX_BYTES + 2];
	const char *bad[] = {NULL, "", "bad\nname", "bad\177name", "\xc0\xaf",
		"\xc2\x85", donor};
	fvwm_config_origin *source = fvwm_config_origin_file("/copy.cfg", 1, 1,
		NULL);

	memset(donor, 'a', sizeof(donor) - 1);
	donor[sizeof(donor) - 1] = '\0';
	for (size_t i = 0; i < sizeof(bad) / sizeof(bad[0]); i++)
	{
		fvwm_theme_origin_definition(&slot, source, FVWM_THEME_COPY,
			bad[i]);
		assert(slot.operation == FVWM_THEME_COPY);
		assert(slot.retention == FVWM_THEME_UNAVAILABLE);
		assert(!slot.source && !slot.copied_from && !slot.charge);
	}
	donor[FVWM_THEME_DONOR_MAX_BYTES] = '\0';
	fvwm_theme_origin_definition(&slot, source, FVWM_THEME_COPY, donor);
	assert(slot.retention == FVWM_THEME_RETAINED);
	assert(strlen(slot.copied_from) == FVWM_THEME_DONOR_MAX_BYTES);
	fvwm_theme_origin_definition(&slot, NULL, FVWM_THEME_MERGE, NULL);
	assert(slot.retention == FVWM_THEME_UNAVAILABLE && !slot.source);
	fvwm_theme_origin_clear(&slot);
	fvwm_config_origin_release(source);
	assert(fvwm_theme_origin_retained_bytes() == 0);
}

int main(void)
{
	test_owned_operations();
	test_shared_budget();
	test_unavailable_donors();
	return 0;
}
