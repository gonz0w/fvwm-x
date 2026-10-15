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

int main(void)
{
	picture_set_loader(load_picture, NULL);
	test_same_name_variants();
	test_key_fields();
	test_reload_and_refusal();
	test_svg_stamp_path();
	picture_set_loader(NULL, NULL);
	return 0;
}
