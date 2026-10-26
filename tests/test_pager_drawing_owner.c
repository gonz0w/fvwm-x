/* This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or any later version.
 */
#include "config.h"
#include "fvwm/fvwm.h"
#include "libs/Flocale.h"
#include "libs/Picture.h"
#include "libs/module_packet_reader.h"
#include "modules/FvwmPager/FvwmPager.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>

int __real_main(int, char **);
void __real_free(void *);
FvwmPicture *__real_PCacheFvwmPicture(Display *, Window, char *,
	const char *, FvwmPictureAttributes);
FvwmPicture *__real_PCloneFvwmPicture(FvwmPicture *);
void __real_PDestroyFvwmPicture(Display *, FvwmPicture *);
int __real_XFreeGC(Display *, GC);
bool __real_module_packet_reader_init(module_packet_reader *, int);
void __real_SendFinishedStartupNotification(int *);
static FvwmPicture *pictures[64];
static Bool released[64], baseline, captured;
static unsigned int acquired, destroyed, excess, clones, gc_count, gc_frees;
static GC gcs[64];
static void *scratch;
static unsigned int scratch_frees;
static const char *scenario;
static char borrowed[] = "borrowed drawing text";

static void watch_picture(FvwmPicture *picture)
{
	if (picture == NULL)
		return;
	assert(acquired < 64);
	pictures[acquired++] = picture;
}

FvwmPicture *__wrap_PCacheFvwmPicture(Display *display, Window window,
	char *path, const char *name, FvwmPictureAttributes attributes)
{
	FvwmPicture *picture = __real_PCacheFvwmPicture(display, window,
		path, name, attributes);
	assert(picture != NULL);
	watch_picture(picture);
	return picture;
}

FvwmPicture *__wrap_PCloneFvwmPicture(FvwmPicture *picture)
{
	FvwmPicture *result = __real_PCloneFvwmPicture(picture);
	if (picture != NULL)
	{
		assert(result == picture);
		watch_picture(result);
		clones++;
	}
	return result;
}

void __wrap_PDestroyFvwmPicture(Display *display, FvwmPicture *picture)
{
	if (picture == NULL)
		return;
	for (unsigned int i = 0; i < acquired; i++)
		if (pictures[i] == picture && !released[i])
		{
			released[i] = True;
			destroyed++;
			if (!baseline)
				__real_PDestroyFvwmPicture(display, picture);
			return;
		}
	excess++;
	assert(baseline);
}

int __wrap_XFreeGC(Display *display, GC gc)
{
	for (unsigned int i = 0; i < gc_count; i++)
		if (gcs[i] == gc)
		{
			gcs[i] = NULL;
			gc_frees++;
			break;
		}
	return __real_XFreeGC(display, gc);
}

void __wrap_free(void *pointer)
{
	assert(pointer != borrowed);
	if (pointer != NULL && pointer == scratch)
	{
		scratch = NULL;
		scratch_frees++;
	}
	__real_free(pointer);
}

static void capture(void)
{
	DeskStyle *style;
	if (captured)
		return;
	captured = True;
	/* A runtime style inherits both already configured picture slots. */
	(void)FindDeskStyle(44);
	TAILQ_FOREACH(style, &desk_style_q, entry)
	{
		GC selected[] = {style->label_gc, style->dashed_gc,
			style->hi_bg_gc, style->hi_fg_gc, style->win_hi_gc,
			style->win_sh_gc, style->focus_hi_gc, style->focus_sh_gc};
		for (unsigned int i = 0; i < 8; i++)
			if (selected[i] != NULL)
				gcs[gc_count++] = selected[i];
	}
	assert(Scr.NormalGC != NULL && Scr.MiniIconGC != NULL &&
		Balloon.gc != NULL);
	gcs[gc_count++] = Scr.NormalGC;
	gcs[gc_count++] = Scr.MiniIconGC;
	gcs[gc_count++] = Balloon.gc;
	assert(gc_count == 43);
	assert(FwinString != NULL);
	scratch = FwinString;
	FwinString->str = borrowed;
	/* Six styles retain two slots each after two replacements. */
	assert(acquired == (baseline ? 8U : 14U));
	assert(clones == (baseline ? 0U : 6U));
	assert(destroyed == 2 && excess == 0);
}

bool __wrap_module_packet_reader_init(module_packet_reader *reader, int fd)
{
	capture();
	if (strcmp(scenario, "partial") == 0)
		return false;
	return __real_module_packet_reader_init(reader, fd);
}

void __wrap_SendFinishedStartupNotification(int *fd)
{
	__real_SendFinishedStartupNotification(fd);
	FILE *file = fopen(getenv("MODULE_LIFECYCLE_READY"), "w");
	assert(file != NULL);
	fprintf(file, "%lu\n", Scr.pager_w);
	assert(fclose(file) == 0);
	if (strcmp(scenario, "exit") == 0)
		ExitPager();
}

static void report(void)
{
	FILE *file = fopen(getenv("MODULE_LIFECYCLE_REPORT"), "w");
	assert(file != NULL && captured);
	fprintf(file, "{\"acquired\":%u,\"destroyed\":%u,\"excess\":%u,"
		"\"clones\":%u,\"gcs\":%u,\"gc_frees\":%u,"
		"\"scratch_frees\":%u}\n", acquired, destroyed, excess,
		clones, gc_count, gc_frees, scratch_frees);
	assert(fclose(file) == 0);
	if (baseline)
	{
		assert(destroyed == acquired && excess == 6);
		assert(gc_frees == 0 && scratch_frees == 0);
		/* Defer old over-release, then drain only acquired references. */
		for (unsigned int i = 0; i < acquired; i++)
			__real_PDestroyFvwmPicture(dpy, pictures[i]);
	}
	else
		assert(destroyed == acquired && excess == 0 &&
			gc_frees == gc_count && scratch_frees == 1);
}

int __wrap_main(int argc, char **argv)
{
	baseline = getenv("MODULE_LIFECYCLE_BASELINE") != NULL;
	scenario = getenv("MODULE_LIFECYCLE_SCENARIO");
	assert(scenario != NULL && atexit(report) == 0);
	int result = __real_main(argc, argv);
	assert(result == (strcmp(scenario, "partial") == 0 ? 1 : 0));
	if (!baseline)
	{
		if (gc_frees != gc_count || scratch_frees != 1 ||
			destroyed != acquired)
		{
			fprintf(stderr, "Pager after main: gcs=%u freed=%u scratch=%u "
				"acquired=%u destroyed=%u excess=%u scratch_live=%p\n",
				gc_count, gc_frees, scratch_frees, acquired, destroyed,
				excess, scratch);
			for (unsigned int i = 0; i < gc_count; i++)
				if (gcs[i] != NULL)
					fprintf(stderr, "Pager retained GC[%u]=%p\n", i,
						(void *)gcs[i]);
			fprintf(stderr, "Pager selected global GCs=%p/%p/%p "
				"scratch=%p\n", (void *)Scr.NormalGC,
				(void *)Scr.MiniIconGC, (void *)Balloon.gc,
				(void *)FwinString);
		}
		assert(gc_frees == gc_count && scratch_frees == 1 &&
			destroyed == acquired);
	}
	/* Repeat selected cleanup through explicit exit; the existing remainder
	 * of teardown and ungrabs still runs once. */
	ExitPager();
	return 0;
}
