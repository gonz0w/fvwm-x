/* This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */
#include "config.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "fvwm/style_id_filter.h"

static void check_equal(style_id_t a, style_id_t b)
{
	style_id_filter filter = { 0 };

	assert(!style_id_filter_may_repeat(&filter, &a));
	style_id_filter_add(&filter, &a);
	assert(!style_id_filter_may_repeat(&filter, &a));
	style_id_filter_add(&filter, &b);
	assert(style_id_filter_may_repeat(&filter, &a));
	assert(style_id_filter_may_repeat(&filter, &b));
}

int main(void)
{
	style_id_t id = { 0 }, other;
	char name[] = "same name", copied[] = "same name";
	style_id_filter filter = { 0 };

	/* Inactive fields must not distinguish IDs which equality ignores. */
	other = id;
	other.window_id = 17;
	other.name = other.class = other.resource = other.icon = "unused";
	check_equal(id, other);
	for (unsigned fields = 1; fields < 64; fields++)
	{
		id.flags.has_resource = !!(fields & 1);
		id.flags.has_class = !!(fields & 2);
		id.flags.has_name = !!(fields & 4);
		id.flags.has_icon = !!(fields & 8);
		id.flags.has_window_id = !!(fields & 16);
		id.flags.is_compatibility_mode = !!(fields & 32);
		id.name = id.class = id.resource = id.icon = name;
		id.window_id = UINT64_C(0xfedcba9876543210);
		other = id;
		other.name = id.flags.has_name || id.flags.is_compatibility_mode ?
			copied : "ignored name";
		other.class = id.flags.has_class ? copied : "ignored class";
		other.resource = id.flags.has_resource ? copied : "ignored resource";
		other.icon = id.flags.has_icon ? copied : "ignored icon";
		if (!id.flags.has_window_id)
			other.window_id = 0;
		check_equal(id, other);
	}
	puts("all selector combinations and inactive-field equality: passed");

	memset(&id, 0, sizeof(id));
	id.flags.is_compatibility_mode = 1;
	id.name = name;
	/* No borrowed pointers survive insertion or depend on later contents. */
	style_id_filter_add(&filter, &id);
	style_id_filter_add(&filter, &id);
	memset(name, 'x', sizeof(name) - 1);
	other = id;
	other.name = copied;
	assert(style_id_filter_may_repeat(&filter, &other));
	puts("summary owns no selector storage: passed");

	/* Saturation/collisions can only retain work. Once an ID repeats, no
	 * unrelated insertion may turn its answer back into a rejection. */
	memset(&filter, 0, sizeof(filter));
	for (unsigned i = 0; i < 32768; i++)
	{
		char text[32];

		snprintf(text, sizeof(text), "collision-%u", i);
		id.name = text;
		style_id_filter_add(&filter, &id);
		style_id_filter_add(&filter, &id);
	}
	for (unsigned i = 0; i < 32768; i++)
	{
		char text[32];

		snprintf(text, sizeof(text), "collision-%u", i);
		id.name = text;
		assert(style_id_filter_may_repeat(&filter, &id));
	}
	memset(&filter, 0xff, sizeof(filter));
	assert(style_id_filter_may_repeat(&filter, &other));
	puts("collisions and saturated fallback retain repeated IDs: passed");
	return 0;
}
