/* This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */
#include "config.h"
#include "libs/Picture.h"
#include "libs/System.h"
#include "libs/Fsvg.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "libs/picture_types.h"
#include "libs/picture_cache_name.h"
#include "libs/picture_view.h"
static unsigned stamp_checks, loads;
static FileStamp current_stamp = 1;
static Bool file_missing, load_failed;
static const char *stamp_path;

char *__wrap_PictureFindImageFile(const char *name, const char *path, int type)
{
	char *copy;

	if (file_missing)
	{
		return NULL;
	}
	copy = malloc(strlen(name) + 1);
	assert(copy != NULL);
	memcpy(copy, name, strlen(name) + 1);
	return copy;
}

Bool __wrap_isFileStampChanged(const FileStamp *stamp, const char *name)
{
	stamp_checks++;
	assert(strcmp(name, stamp_path) == 0);
	return *stamp != current_stamp;
}

static FvwmPicture *load_picture(
	Display *display, Window window, char *path,
	FvwmPictureAttributes attributes, monitor_scale_factor factor,
	unsigned width, unsigned height, unsigned generation, Bool cache,
	void *context)
{
	FvwmPicture *picture;

	loads++;
	assert(cache);
	if (load_failed)
	{
		return NULL;
	}
	picture = calloc(1, sizeof(*picture));
	assert(picture != NULL);
	picture->name = path;
	picture->count = 1;
	picture->stamp = current_stamp;
	picture->fpa_mask = attributes.mask;
	return picture;
}

static FvwmPicture *cached_picture(
	const char *name, unsigned mask, unsigned scale, unsigned width,
	unsigned height, unsigned generation)
{
	FvwmPictureAttributes attributes = { .mask = mask };
	monitor_scale_factor factor = { scale };

	return PCacheFvwmPictureAtScale(NULL, None, NULL, name, attributes,
		factor, width, height, generation);
}

static void test_same_name_variants(void)
{
	FvwmPicture *pictures[65];
	FvwmPicture *hit;
	const char *name = "/virtual/picture.png";

	stamp_path = name;
	for (unsigned i = 0; i < 65; i++)
	{
		pictures[i] = cached_picture(name, 0, 4, 0, 0, 7 + i);
		assert(pictures[i] != NULL && pictures[i]->count == 1);
	}
	stamp_checks = 0;
	hit = cached_picture(name, 0, 4, 0, 0, 7);
	assert(hit == pictures[0] && hit->count == 2);
	printf("65 same-name variants: %u stamp checks for one hit\n",
		stamp_checks);
	fflush(stdout);
	assert(stamp_checks == 1);
	PDestroyFvwmPicture(NULL, hit);
	for (unsigned i = 0; i < 65; i++)
	{
		assert(pictures[i]->count == 1);
		PDestroyFvwmPicture(NULL, pictures[i]);
	}
}

static void test_key_fields(void)
{
	const char *name = "/virtual/picture.png";
	const unsigned keys[][5] = {
		{0, 4, 16, 12, 7}, {FPAM_NO_ALPHA, 4, 16, 12, 7},
		{0, 8, 16, 12, 7}, {0, 4, 18, 12, 7},
		{0, 4, 16, 14, 7}, {0, 4, 16, 12, 8}
	};
	FvwmPicture *pictures[6];

	stamp_path = name;
	for (unsigned i = 0; i < 6; i++)
	{
		stamp_checks = 0;
		pictures[i] = cached_picture(name, keys[i][0], keys[i][1],
			keys[i][2], keys[i][3], keys[i][4]);
		assert(pictures[i] != NULL && stamp_checks == 0);
	}
	for (unsigned i = 0; i < 6; i++)
	{
		FvwmPicture *hit;
		unsigned before = loads;

		stamp_checks = 0;
		hit = cached_picture(name, keys[i][0], keys[i][1],
			keys[i][2], keys[i][3], keys[i][4]);
		assert(hit == pictures[i] && stamp_checks == 1);
		assert(loads == before && hit->count == 2);
		PDestroyFvwmPicture(NULL, hit);
	}
	stamp_path = "/virtual/other.png";
	stamp_checks = 0;
	FvwmPicture *other = cached_picture(stamp_path, 0, 4, 16, 12, 7);

	assert(other != NULL && stamp_checks == 0);
	PDestroyFvwmPicture(NULL, other);
	for (unsigned i = 0; i < 6; i++)
	{
		PDestroyFvwmPicture(NULL, pictures[i]);
	}
}

static void test_reload_and_refusal(void)
{
	const char *name = "/virtual/picture.png";
	FvwmPicture *old, *replacement, *hit;
	unsigned before;

	stamp_path = name;
	old = cached_picture(name, 0, 4, 0, 0, 7);
	assert(old != NULL);
	current_stamp++;
	load_failed = True;
	stamp_checks = 0;
	assert(cached_picture(name, 0, 4, 0, 0, 7) == NULL);
	assert(stamp_checks == 1 && old->count == 1);
	load_failed = False;
	stamp_checks = 0;
	replacement = cached_picture(name, 0, 4, 0, 0, 7);
	assert(replacement && replacement != old && stamp_checks == 1);
	assert(old->count == 1 && old->stamp != replacement->stamp);
	file_missing = True;
	stamp_checks = 0;
	before = loads;
	assert(cached_picture(name, 0, 4, 0, 0, 7) == NULL);
	assert(stamp_checks == 0 && loads == before);
	file_missing = False;
	hit = cached_picture(name, 0, 4, 0, 0, 7);
	assert(hit == replacement && stamp_checks == 1);
	PDestroyFvwmPicture(NULL, hit);
	PDestroyFvwmPicture(NULL, replacement);
	PDestroyFvwmPicture(NULL, old);
}

static void test_svg_stamp_path(void)
{
#if USE_SVG
	const char *name = ":16x16:/virtual/picture.svg";
	FvwmPicture *picture, *hit;

	stamp_path = "/virtual/picture.svg";
	picture = cached_picture(name, 0, 4, 0, 0, 7);
	assert(picture != NULL);
	stamp_checks = 0;
	hit = cached_picture(name, 0, 4, 0, 0, 7);
	assert(hit == picture && stamp_checks == 1);
	PDestroyFvwmPicture(NULL, hit);
	PDestroyFvwmPicture(NULL, picture);
#endif
}

static void check_membership(FvwmPicture *picture)
{
	assert(picture->cache_link != NULL);
	assert(*picture->cache_link == picture);
	if (picture->next != NULL)
	{
		assert(picture->next->cache_link == &picture->next);
	}
}

static void test_name_collisions(void)
{
	const char *names[] = { "/virtual/target", "/virtual/collision",
		"/virtual/head", "", "/virtual/\200\377" };
	FvwmPicture *pictures[5], *hit;

	for (unsigned i = 0; i < 5; i++)
	{
		stamp_path = names[i];
		pictures[i] = cached_picture(names[i], 0, 4, 0, 0, 7);
		assert(pictures[i] != NULL);
		check_membership(pictures[i]);
	}
	/* A rejection fingerprint must never replace full name equality. */
	pictures[1]->cache_name_hash = picture_cache_name_hash(names[0]);
	stamp_path = names[0];
	stamp_checks = 0;
	hit = cached_picture(names[0], 0, 4, 0, 0, 7);
	assert(hit == pictures[0] && stamp_checks == 1);
	PDestroyFvwmPicture(NULL, hit);
	for (unsigned i = 3; i < 5; i++)
	{
		stamp_path = names[i];
		hit = cached_picture(names[i], 0, 4, 0, 0, 7);
		assert(hit == pictures[i]);
		PDestroyFvwmPicture(NULL, hit);
	}
	for (unsigned i = 0; i < 5; i++)
	{
		check_membership(pictures[i]);
		PDestroyFvwmPicture(NULL, pictures[i]);
	}
}

static void test_retirement_orders(void)
{
	FvwmPicture *pictures[129];
	unsigned state = 0x43524355;

	stamp_path = "/virtual/retirement";
	for (unsigned mode = 0; mode < 4; mode++)
	{
		for (unsigned i = 0; i < 129; i++)
		{
			pictures[i] = cached_picture(stamp_path, 0, 4,
				0, 0, 7 + i);
			check_membership(pictures[i]);
		}
		for (unsigned remaining = 129; remaining != 0; remaining--)
		{
			unsigned slot = mode == 0 ? 0 : remaining - 1;
			FvwmPicture *hit;

			if (mode >= 2)
			{
				state = state * 1664525u + 1013904223u;
				slot = mode == 2 ? remaining / 2 : state % remaining;
			}
			FvwmPicture *picture = pictures[slot];

			/* A retained reference keeps its cache membership. */
			hit = PCloneFvwmPicture(picture);
			PDestroyFvwmPicture(NULL, hit);
			assert(picture->count == 1);
			check_membership(picture);
			PDestroyFvwmPicture(NULL, picture);
			pictures[slot] = pictures[remaining - 1];
			for (unsigned i = 0; i + 1 < remaining; i++)
			{
				check_membership(pictures[i]);
				hit = cached_picture(stamp_path, 0, 4, 0, 0,
					pictures[i]->generation);
				assert(hit == pictures[i] && hit->count == 2);
				PDestroyFvwmPicture(NULL, hit);
			}
		}
	}
}

static void retire_neighbour(Display *display, FvwmPicture *view, void *data)
{
	FvwmPicture **picture = data;

	(void)view;
	PDestroyFvwmPicture(display, *picture);
	*picture = NULL;
}

static void test_unpublished_and_recursive_release(void)
{
	FvwmPicture *cached, *uncached, *view, *pixmap, *hit;

	stamp_path = "/virtual/retained";
	cached = cached_picture(stamp_path, 0, 4, 0, 0, 7);
	uncached = PLoadFvwmPictureFromPixmap(NULL, None, strdup("uncached"),
		None, None, None, 1, 1, 0, NULL, 0);
	assert(uncached->cache_link == NULL);
	PDestroyFvwmPicture(NULL, uncached);
	check_membership(cached);
	pixmap = PCacheFvwmPictureFromPixmap(NULL, None, strdup("pixmap"),
		None, None, None, 1, 1, 0, NULL, 0);
	check_membership(pixmap);
	/* Raw-record cleanup also withdraws a published data-only record. */
	PFreeFvwmPictureData(pixmap);
	check_membership(cached);
	view = picture_view_create(strdup("view"),
		(FvwmPictureAttributes){0}, (monitor_scale_factor){4},
		0, 0, 7, retire_neighbour, &cached);
	assert(view->cache_link == NULL);
	PDestroyFvwmPicture(NULL, view);
	assert(cached == NULL);
	hit = cached_picture(stamp_path, 0, 4, 0, 0, 7);
	assert(hit != NULL && hit->count == 1);
	PDestroyFvwmPicture(NULL, hit);
}

int main(void)
{
	picture_set_loader(load_picture, NULL);
	test_same_name_variants();
	test_key_fields();
	test_reload_and_refusal();
	test_svg_stamp_path();
	test_name_collisions();
	test_retirement_orders();
	test_unpublished_and_recursive_release();
	picture_set_loader(NULL, NULL);
	return 0;
}
