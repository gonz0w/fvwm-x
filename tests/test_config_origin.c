/* This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or any later version.
 */
#include "config.h"
#include "libs/config_origin.h"
#include "libs/Bindings.h"
#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

static void test_binding_ownership(void)
{
	Binding *list = NULL, *removed = NULL, *specific;
	Bool similar;
	XClassHint window_class = {.res_name = "editor", .res_class = "Editor"};
	fvwm_config_origin *root, *child;
	size_t charge;

	root = fvwm_config_origin_file("/bindings.cfg", 3, 4, NULL);
	child = fvwm_config_origin_file("/keys.cfg", 8, 9, root);
	charge = fvwm_config_origin_storage_bytes(child);
	assert(charge > FVWM_CONFIG_SOURCE_MAX_BYTES * 2);
	assert(AddBinding(NULL, &list, BIND_BUTTONPRESS, 1, 0, NULL,
		0, 1, "global", NULL, NULL) == 1);
	assert(list->source == NULL && list->source_charge == 0);
	assert(!list->replaced_existing);
	binding_set_provenance(list, child, False);
	assert(AddBinding(NULL, &list, BIND_BUTTONPRESS, 1, 0, NULL,
		0, 1, "specific", NULL, "editor*") == 1);
	binding_set_provenance(list, child, True);
	specific = list;
	/* Put another global definition first: a later specific match must win. */
	assert(AddBinding(NULL, &list, BIND_BUTTONPRESS, 1, 0, NULL,
		0, 1, "global", NULL, NULL) == 1);
	binding_set_provenance(list, child, False);
	fvwm_config_origin_release(child);
	fvwm_config_origin_release(root);
	assert(specific->source_charge == charge);
	assert(strstr(fvwm_config_origin_json(specific->source),
		"/bindings.cfg") != NULL);
	assert(strcmp(CheckBinding(list, 1, 0, 0, 1, BIND_BUTTONPRESS,
		&window_class, "editor document"), "specific") == 0);
	/* Moving an existing binding transfers both the snapshot and its charge. */
	CollectBindingList(NULL, &list, &removed, &similar, BIND_BUTTONPRESS,
		1, 0, 0, 1, "editor*");
	assert(removed == specific && removed->NextBinding == NULL);
	assert(removed->source_charge == charge && removed->replaced_existing);
	assert(strcmp(CheckBinding(list, 1, 0, 0, 1, BIND_BUTTONPRESS,
		&window_class, "editor document"), "global") == 0);
	/* A replacement keeps its own source after the removed owner is freed. */
	assert(AddBinding(NULL, &list, BIND_BUTTONPRESS, 1, 0, NULL,
		0, 1, "replacement", NULL, "editor*") == 1);
	binding_set_provenance(list, removed->source, removed != NULL);
	FreeBindingList(removed);
	removed = NULL;
	assert(strstr(fvwm_config_origin_json(list->source), "/keys.cfg"));
	CollectBindingList(NULL, &list, &removed, &similar, BIND_BUTTONPRESS,
		1, 0, 0, 1, "editor*");
	assert(removed && !removed->NextBinding);
	FreeBindingList(removed);
	FreeBindingList(list);
}

static void test_binding_budget(void)
{
	fvwm_config_origin *origin = fvwm_config_origin_file("/budget.cfg",
		1, 1, NULL);
	size_t charge = fvwm_config_origin_storage_bytes(origin);
	size_t expected = (1024U * 1024U) / charge;

	/* Repeat after teardown: every charged owner must return its budget. */
	for (unsigned int pass = 0; pass < 2; pass++)
	{
		Binding *list = NULL, *recovered = NULL;
		size_t count;

		for (count = 0; count <= expected; count++)
		{
			assert(AddBinding(NULL, &list, BIND_BUTTONPRESS, 1, 0,
				NULL, 0, 1, "Nop", NULL, NULL) == 1);
			binding_set_provenance(list, origin, False);
			if (count < expected)
				assert(list->source_charge == charge);
			else
			{
				assert(list->source_charge == 0);
				assert(strstr(fvwm_config_origin_json(list->source),
					"binding source retention budget exhausted"));
			}
		}
		/* The unavailable head remains a usable binding. Free one retained
		 * reference and prove the next definition can retain source again. */
		assert(strcmp(CheckBinding(list, 1, 0, 0, 1, BIND_BUTTONPRESS,
			NULL, NULL), "Nop") == 0);
		RemoveBinding(&list, list->NextBinding, list);
		assert(AddBinding(NULL, &recovered, BIND_BUTTONPRESS, 2, 0,
			NULL, 0, 1, "Nop", NULL, NULL) == 1);
		binding_set_provenance(recovered, origin, False);
		assert(recovered->source_charge == charge);
		/* Replacing metadata on one owner must not double-charge it. */
		binding_set_provenance(recovered, recovered->source, True);
		assert(recovered->source_charge == charge);
		assert(recovered->replaced_existing);
		FreeBindingList(recovered);
		FreeBindingList(list);
	}
	fvwm_config_origin_release(origin);
	assert(fvwm_config_origin_storage_bytes(NULL) == 0);
	origin = fvwm_config_origin_binding_budget_exhausted();
	assert(fvwm_config_origin_storage_bytes(origin) == 0);
	fvwm_config_origin_release(fvwm_config_origin_retain(origin));
}

int main(void)
{
	fvwm_config_origin *root, *child, *held, *derived, *bad;
	char large[FVWM_CONFIG_SOURCE_MAX_BYTES + 2];
	unsigned int i;

	test_binding_ownership();
	test_binding_budget();
	root = fvwm_config_origin_file("/root.cfg", 2, 4, NULL);
	child = fvwm_config_origin_file("/child.cfg", 8, 9, root);
	assert(fvwm_config_origin_storage_bytes(child) >
		fvwm_config_origin_storage_bytes(root) +
		FVWM_CONFIG_SOURCE_MAX_BYTES);
	held = fvwm_config_origin_retain(child);
	fvwm_config_origin_release(root);
	fvwm_config_origin_release(child);
	assert(strstr(fvwm_config_origin_json(held),
		"\"include\":[{\"path\":\"/root.cfg\",\"first\":2,\"last\":4}]") != NULL);
	derived = fvwm_config_origin_derive(held, "invocation", "unknown definition");
	fvwm_config_origin_release(held);
	assert(strstr(fvwm_config_origin_json(derived),
		"\"kind\":\"invocation\"") != NULL);
	assert(strstr(fvwm_config_origin_json(derived),
		"\"path\":\"/child.cfg\"") != NULL);
	child = fvwm_config_origin_file("/from-function", 1, 1, derived);
	assert(strstr(fvwm_config_origin_json(child), "unavailable") != NULL);
	fvwm_config_origin_release(child);
	fvwm_config_origin_release(derived);

	bad = fvwm_config_origin_file("relative", 1, 1, NULL);
	assert(strstr(fvwm_config_origin_json(bad), "unavailable") != NULL);
	child = fvwm_config_origin_file("/child", 1, 1, bad);
	assert(strstr(fvwm_config_origin_json(child), "unavailable") != NULL);
	fvwm_config_origin_release(bad);
	fvwm_config_origin_release(child);
	bad = fvwm_config_origin_file("/bad\xc0\xaf", 1, 1, NULL);
	assert(strstr(fvwm_config_origin_json(bad), "unavailable") != NULL);
	fvwm_config_origin_release(bad);
	bad = fvwm_config_origin_file("/bad\npath", 1, 1, NULL);
	assert(strstr(fvwm_config_origin_json(bad), "unavailable") != NULL);
	fvwm_config_origin_release(bad);
	bad = fvwm_config_origin_file("/bad\xc2\x85path", 1, 1, NULL);
	assert(strstr(fvwm_config_origin_json(bad), "unavailable") != NULL);
	fvwm_config_origin_release(bad);
	bad = fvwm_config_origin_file("/range", 1, (size_t)UINT32_MAX + 1, NULL);
	assert(strstr(fvwm_config_origin_json(bad), "unavailable") != NULL);
	fvwm_config_origin_release(bad);
	memset(large, 'x', sizeof(large));
	large[0] = '/';
	large[sizeof(large) - 1] = 0;
	bad = fvwm_config_origin_file(large, 1, 1, NULL);
	assert(strstr(fvwm_config_origin_json(bad), "unavailable") != NULL);
	fvwm_config_origin_release(bad);
	/* An individually bounded path can still exceed the complete JSON wire
	 * bound; refusal must not publish a truncated exact source. */
	large[FVWM_CONFIG_SOURCE_MAX_BYTES - 32] = 0;
	bad = fvwm_config_origin_file(large, 1, 1, NULL);
	assert(strstr(fvwm_config_origin_json(bad), "unavailable") != NULL);
	fvwm_config_origin_release(bad);

	root = NULL;
	for (i = 0; i < FVWM_CONFIG_SOURCE_MAX_INCLUDE + 1; i++)
	{
		child = fvwm_config_origin_file("/x", 1, 1, root);
		fvwm_config_origin_release(root);
		root = child;
		assert(strstr(fvwm_config_origin_json(root), "\"kind\":\"file\"") != NULL);
	}
	child = fvwm_config_origin_file("/overflow", 1, 1, root);
	assert(strstr(fvwm_config_origin_json(child), "unavailable") != NULL);
	fvwm_config_origin_release(root);
	fvwm_config_origin_release(child);
	root = fvwm_config_origin_file("/quote\"slash\\é", 1, 2, NULL);
	assert(strstr(fvwm_config_origin_json(root), "\\u0022slash\\u005c") != NULL);
	assert(strlen(fvwm_config_origin_json(root)) <= FVWM_CONFIG_SOURCE_MAX_BYTES);
	fvwm_config_origin_release(root);
	return 0;
}
