/* This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or any later version.
 */
#include "config.h"
#include "libs/Picture.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int __real_main(int argc, char **argv);
FvwmPicture *__real_PCacheFvwmPicture(Display *, Window, char *,
	const char *, FvwmPictureAttributes);
void __real_PDestroyFvwmPicture(Display *, FvwmPicture *);
int __real_XMapWindow(Display *, Window);
static FvwmPicture *owners[32];
static unsigned int count, released, excess, refused;
static Bool released_slots[32], baseline, actual_release;
static unsigned int expected_refused, unique_pictures;
static Bool expect_aliases;
static Display *display;

FvwmPicture *__wrap_PCacheFvwmPicture(Display *dpy, Window window,
	char *path, const char *name, FvwmPictureAttributes attributes)
{
	FvwmPicture *picture = __real_PCacheFvwmPicture(dpy, window, path,
		name, attributes);

	if (picture == NULL)
	{
		fprintf(stderr, "owner fixture picture refusal: name=%s path=%s\n",
			name == NULL ? "(null)" : name,
			path == NULL ? "(null)" : path);
		refused++;
		return NULL;
	}
	assert(count < 32);
	Bool seen = False;
	for (unsigned int i = 0; i < count; i++)
		seen |= owners[i] == picture;
	unique_pictures += !seen;
	owners[count++] = picture;
	display = dpy;
	return picture;
}

void __wrap_PDestroyFvwmPicture(Display *dpy, FvwmPicture *picture)
{
	assert(dpy == display);
	/* Observe every acquired reference, including aliases. Old controls defer
	 * release to avoid their wrong-slot UAF; candidate actual mode exercises
	 * the real cache/destructor during production cleanup. */
	for (unsigned int i = 0; i < count; i++)
		if (owners[i] == picture && !released_slots[i])
		{
			released_slots[i] = True;
			released++;
			if (actual_release)
				__real_PDestroyFvwmPicture(dpy, picture);
			return;
		}
	excess++;
	assert(!actual_release);
}

int __wrap_XMapWindow(Display *dpy, Window window)
{
	int result = __real_XMapWindow(dpy, window);
	FILE *file = fopen(getenv("MODULE_OWNER_READY"), "w");

	assert(file != NULL);
	XSync(dpy, False);
	fprintf(file, "1 %lu\n", window);
	assert(fclose(file) == 0);
	return result;
}

static void report(void)
{
	FILE *file = fopen(getenv("MODULE_OWNER_REPORT"), "w");

	assert(file != NULL);
	fprintf(file, "{\"acquired\":%u,\"released\":%u,\"excess\":%u,"
		"\"retained\":%u,\"refused\":%u,\"actual_release\":%u,"
		"\"unique_pictures\":%u}\n", count, released, excess,
		count - released, refused, (unsigned int)(actual_release != False),
		unique_pictures);
	assert(fclose(file) == 0);
	assert(count == 7 - expected_refused && refused == expected_refused);
	assert(unique_pictures == (expect_aliases ? 1 : count));
	if (baseline)
		assert(released < count);
	else
		assert(released == count && excess == 0);
	if (!actual_release)
		for (unsigned int i = 0; i < count; i++)
			__real_PDestroyFvwmPicture(display, owners[i]);
}

int __wrap_main(int argc, char **argv)
{
	baseline = getenv("MODULE_OWNER_BASELINE") != NULL;
	actual_release = getenv("MODULE_OWNER_ACTUAL_RELEASE") != NULL;
	expected_refused = getenv("MODULE_OWNER_EXPECT_REFUSAL") != NULL;
	const char *scenario = getenv("MODULE_OWNER_SCENARIO");
	expect_aliases = scenario != NULL && strncmp(scenario, "aliases", 7) == 0;
	assert(!baseline || (!actual_release && expected_refused == 0));
	assert(atexit(report) == 0);
	return __real_main(argc, argv);
}
