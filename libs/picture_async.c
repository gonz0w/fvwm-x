/* This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */
#include "config.h"
#include "picture_async.h"
#include "picture_argb.h"
#include "image_svg_client.h"
#include "image_effect_client.h"
#include "image_background.h"
#include "ImageSafety.h"
#include "Fsvg.h"
#include "FRenderInit.h"
#include "FRenderInterface.h"
#include "Fcursor.h"
#include "wm_ui_metrics.h"
#include "System.h"
#include "Strings.h"
#include "log.h"
#include "safemalloc.h"

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include "libs/picture_types.h"
#define PICTURE_ASYNC_COMMIT_VIEWS 32U

typedef struct
{
	picture_async_policy_fn policy;
	picture_async_settled_fn settled;
	void *context;
	void (*release)(void *);
} picture_async_binding;

static void picture_async_binding_release(void *context)
{
	picture_async_binding *binding = context;

	if (binding->release != NULL)
	{
		binding->release(binding->context);
	}
	free(binding);
}

Bool picture_async_bind(FvwmPicture *view, picture_async_policy_fn policy,
	picture_async_settled_fn settled, void *context,
	void (*release)(void *))
{
	if (view == NULL || !view->is_view || !view->pending ||
	    view->view_owner != NULL || policy == NULL || settled == NULL)
	{
		errno = EINVAL;
		return False;
	}
	picture_async_binding *binding = fxmalloc(sizeof(*binding));

	*binding = (picture_async_binding){policy, settled, context, release};
	view->view_owner = binding;
	view->view_owner_release = picture_async_binding_release;
	return True;
}

void *picture_async_context(FvwmPicture *view,
	picture_async_settled_fn settled)
{
	picture_async_binding *binding = view != NULL && view->is_view ?
	    view->view_owner : NULL;

	return binding != NULL && binding->settled == settled ?
	    binding->context : NULL;
}

typedef enum
{
	PICTURE_JOB_QUEUED,
	PICTURE_JOB_DECODING,
	PICTURE_JOB_XPM,
	PICTURE_JOB_EXPANDING,
	PICTURE_JOB_START_SVG,
	PICTURE_JOB_START_EFFECT,
	PICTURE_JOB_DECODED,
	PICTURE_JOB_FALLBACK,
	PICTURE_JOB_CONVERTING,
	PICTURE_JOB_ACKING,
	PICTURE_JOB_READY,
	PICTURE_JOB_FAILED
} picture_job_phase;

typedef struct picture_async_job picture_async_job;
typedef struct picture_async_view picture_async_view;

struct picture_async_view
{
	picture_async_view *next;
	picture_async_view **previous;
	picture_async_view *job_next;
	picture_async_view **job_previous;
	picture_async_job *job;
	FvwmPicture *view;
	Bool deferred;
};

struct picture_async_job
{
	picture_async_job *next;
	picture_async_job **previous;
	picture_async_job *queue_next;
	picture_async_job **queue_previous;
	picture_async *manager;
	picture_async_view *views;
	size_t subscribers;
	char *path;
	struct stat identity;
	struct stat opened;
	FvwmPictureAttributes attributes;
	monitor_scale_factor factor;
	unsigned target_width;
	unsigned target_height;
	unsigned generation;
	Visual *visual;
	Colormap cmap;
	unsigned depth;
	Bool default_visual;
	Bool cache;
	Bool cursor;
	Bool effect;
	Bool background;
	struct background_plan background_plan;
	unsigned long effect_pixmap_request;
	FvwmPicture *effect_source;
	picture_color *effect_background;
	picture_color *effect_tint;
	image_effect_request effect_request;
	image_effect_result effect_result;
	int hotspot[2];
	int source_hotspot[2];
	unsigned long cursor_request;
	picture_job_phase phase;
	image_input source;
	image_svg_result result;
	image_decompress_result expanded;
	Bool svg_first;
	Bool svg_tried;
	Bool xpm_tried;
	Bool expansion_tried;
	Bool expansion_ok;
	image_process *process;
	picture_argb_builder *builder;
	FvwmPicture *picture;
	unsigned long first_request;
	unsigned long end_request;
	unsigned long barrier_request;
	Bool request_scope_open;
	Bool waiting_ack;
	Bool conversion_done;
	Bool cleaning;
	Bool x_error;
	XErrorEvent creation_errors[6];
	unsigned creation_error_count;
	int decode_error;
};

struct picture_async
{
	Display *dpy;
	Window drawable;
#if defined(HAVE_XCURSOR) && XRenderSupport
	XRenderPictFormat *cursor_format;
#endif
	picture_async_job *jobs;
	picture_async_job *queue;
	picture_async_job **queue_tail;
	picture_async_job *active;
	picture_async_view *views;
	picture_async_view **views_tail;
	size_t view_count;
	size_t ready_work;
	Window ack_window;
	unsigned long window_request;
	Bool window_refused;
};

/* Shutdown cannot wait for outstanding X errors. Keep three owned
 * request ranges, with no manager/window callback or allocated state. */
static struct
{
	Display *dpy;
	unsigned long first[3];
	unsigned long end[3];
} picture_async_shutdown_errors;
static picture_async *picture_async_owner;

static const char *picture_async_real_path(const char *path)
{
	const char *end;

	if (path[0] == ':' && (end = strchr(path + 1, ':')) != NULL)
	{
		return end + 1;
	}
	return path;
}

/* Match the legacy last-three-character preference, including SVG options. */
static Bool picture_async_svg_first(const char *path)
{
	size_t length = strlen(path);

	return picture_async_real_path(path) != path ||
	    (length > 3 && StrEquals(path + length - 3, "svg"));
}

static Bool picture_async_compressed_xpm(const char *path)
{
	const char *extension = strrchr(picture_async_real_path(path), '.');

	return extension != NULL && (strcmp(extension, ".Z") == 0 ||
	    strcmp(extension, ".gz") == 0);
}

static Bool picture_async_identity(const struct stat *left,
	const struct stat *right)
{
#if defined(__APPLE__)
	const struct timespec lm = left->st_mtimespec, rm = right->st_mtimespec;
	const struct timespec lc = left->st_ctimespec, rc = right->st_ctimespec;
#else
	const struct timespec lm = left->st_mtim, rm = right->st_mtim;
	const struct timespec lc = left->st_ctim, rc = right->st_ctim;
#endif
	return left->st_dev == right->st_dev && left->st_ino == right->st_ino &&
	    left->st_size == right->st_size &&
	    left->st_mode == right->st_mode &&
	    lm.tv_sec == rm.tv_sec && lm.tv_nsec == rm.tv_nsec &&
	    lc.tv_sec == rc.tv_sec && lc.tv_nsec == rc.tv_nsec;
}

static Bool picture_async_current(const picture_async_job *job,
	const struct stat *identity)
{
	struct stat current;

	return stat(picture_async_real_path(job->path), &current) == 0 &&
	    picture_async_identity(identity, &current);
}

static void picture_async_queue_remove(picture_async_job *job)
{
	picture_async *manager = job->manager;

	if (job->queue_previous == NULL)
	{
		return;
	}
	*job->queue_previous = job->queue_next;
	if (job->queue_next != NULL)
	{
		job->queue_next->queue_previous = job->queue_previous;
	}
	else
	{
		manager->queue_tail = job->queue_previous;
	}
	job->queue_previous = NULL;
	job->queue_next = NULL;
}

static void picture_async_view_remove(picture_async_view *entry)
{
	picture_async *manager = entry->job->manager;

	*entry->previous = entry->next;
	if (entry->next != NULL)
	{
		entry->next->previous = entry->previous;
	}
	else
	{
		manager->views_tail = entry->previous;
	}
}

static void picture_async_view_append(picture_async *manager,
	picture_async_view *entry)
{
	entry->next = NULL;
	entry->previous = manager->views_tail;
	*manager->views_tail = entry;
	manager->views_tail = &entry->next;
}

static void picture_async_job_free(picture_async_job *job)
{
	*job->previous = job->next;
	if (job->next != NULL)
	{
		job->next->previous = job->previous;
	}
	image_input_destroy(&job->source);
	image_decompress_result_destroy(&job->expanded);
	image_svg_result_destroy(&job->result);
	image_effect_result_destroy(&job->effect_result);
	PDestroyFvwmPicture(job->manager->dpy, job->effect_source);
	picture_color_release(job->manager->dpy, job->effect_background);
	picture_color_release(job->manager->dpy, job->effect_tint);
	PDestroyFvwmPicture(job->manager->dpy, job->picture);
	free(job->path);
	free(job);
}

static void picture_async_release(Display *dpy, FvwmPicture *view,
	void *context)
{
	(void)dpy;
	picture_async_view *entry = context;
	picture_async_job *job = entry->job;
	picture_async *manager = job->manager;

	if (view->pending && !entry->deferred &&
	    (job->phase == PICTURE_JOB_READY ||
	    job->phase == PICTURE_JOB_FAILED))
	{
		manager->ready_work--;
	}
	picture_async_view_remove(entry);
	*entry->job_previous = entry->job_next;
	if (entry->job_next != NULL)
	{
		entry->job_next->job_previous = entry->job_previous;
	}
	free(entry);
	manager->view_count--;
	if (--job->subscribers != 0)
	{
		return;
	}
	if (manager->active == job)
	{
		/* Child/output storage survives cancellation until its reap.
		 * No callback points at the departed owner. */
		image_process_cancel(job->process);
		if (job->builder != NULL)
		{
			picture_argb_cancel(job->builder);
		}
		return;
	}
	picture_async_queue_remove(job);
	picture_async_job_free(job);
}

static FvwmPicture *picture_async_subscribe(picture_async_job *job,
	char *path)
{
	picture_async_view *entry = fxcalloc(1, sizeof(*entry));
	FvwmPicture *view = picture_view_create(path, job->attributes,
	    job->factor, job->target_width, job->target_height, job->generation,
	    picture_async_release, entry);

	entry->job = job;
	entry->view = view;
	entry->job_previous = &job->views;
	entry->job_next = job->views;
	if (entry->job_next != NULL)
	{
		entry->job_next->job_previous = &entry->job_next;
	}
	job->views = entry;
	job->subscribers++;
	job->manager->view_count++;
	picture_async_view_append(job->manager, entry);
	return view;
}

static FvwmPicture *picture_async_request(Display *dpy, Window drawable,
	char *path, FvwmPictureAttributes attributes,
	monitor_scale_factor factor,
	unsigned target_width, unsigned target_height, unsigned generation,
	Bool cache, void *context, Bool cursor, const int hotspot[2])
{
	picture_async *manager = context;
	struct stat identity;
	image_input_prefix prefix;

	if (!USE_SVG)
	{
		return PImageLoadFvwmPictureFromFile(dpy, drawable, path,
		    attributes, factor, target_width, target_height);
	}
	if (!image_input_probe(picture_async_real_path(path),
	    FVWM_IMAGE_INPUT_LIMIT, &prefix, &identity))
	{
		return NULL;
	}
	/* Definite PNG stays on the ordinary raster path, even when misnamed.
	 * Ambiguous inputs retain only this small prefix while queued. */
	if (!cursor && prefix.length >= 4 &&
	    memcmp(prefix.bytes, "\211PNG", 4) == 0)
	{
		return PImageLoadFvwmPictureFromFile(dpy, drawable, path,
		    attributes, factor, target_width, target_height);
	}
	if (!cursor && !picture_async_svg_first(path) &&
	    !picture_async_compressed_xpm(path) &&
	    ((prefix.length >= 9 &&
	      memcmp(prefix.bytes, "/* XPM */", 9) == 0) ||
	     (prefix.length >= 6 && memcmp(prefix.bytes, "! XPM2", 6) == 0) ||
	     (prefix.length >= 7 && memcmp(prefix.bytes, "#define", 7) == 0)))
	{
		Bool deferred = False;
		FvwmPicture *picture = picture_load_non_svg(dpy, drawable,
		    path, attributes, factor, target_width, target_height,
		    &deferred);

		if (picture != NULL || !deferred)
		{
			return picture;
		}
	}
	picture_async_job *job;

	for (job = manager->jobs; cache && job != NULL; job = job->next)
	{
		if (job->cache && !job->x_error &&
		    job->phase < PICTURE_JOB_READY &&
		    job->subscribers != 0 && strcmp(path, job->path) == 0 &&
		    picture_async_identity(&identity, &job->identity) &&
		    job->attributes.mask == attributes.mask &&
		    job->factor.quarters == factor.quarters &&
		    job->target_width == target_width &&
		    job->target_height == target_height &&
		    job->generation == generation && job->visual == Pvisual &&
		    job->cmap == Pcmap && job->depth == Pdepth)
		{
			break;
		}
	}
	if (!cache)
	{
		job = NULL;
	}
	if (job == NULL)
	{
		job = fxcalloc(1, sizeof(*job));
		job->manager = manager;
		job->path = fxstrdup(path);
		job->identity = identity;
		job->attributes = attributes;
		job->factor = factor;
		job->target_width = target_width;
		job->target_height = target_height;
		job->generation = generation;
		job->cache = cache;
		job->cursor = cursor;
		job->hotspot[0] = cursor ? hotspot[0] : -1;
		job->hotspot[1] = cursor ? hotspot[1] : -1;
		job->source_hotspot[0] = job->source_hotspot[1] = -1;
		job->svg_first = picture_async_svg_first(path);
		job->visual = Pvisual;
		job->cmap = Pcmap;
		job->depth = Pdepth;
		job->default_visual = Pvisual == DefaultVisual(dpy,
		    DefaultScreen(dpy)) && Pcmap == DefaultColormap(dpy,
		    DefaultScreen(dpy));
		job->previous = &manager->jobs;
		job->next = manager->jobs;
		if (job->next != NULL)
		{
			job->next->previous = &job->next;
		}
		manager->jobs = job;
		job->queue_previous = manager->queue_tail;
		*manager->queue_tail = job;
		manager->queue_tail = &job->queue_next;
	}
	FvwmPicture *view = picture_async_subscribe(job, path);

	view->stamp = ((FileStamp)identity.st_mtime << 13) + identity.st_size;
	return view;
}

static FvwmPicture *picture_async_load(Display *dpy, Window drawable,
	char *path, FvwmPictureAttributes attributes,
	monitor_scale_factor factor,
	unsigned target_width, unsigned target_height, unsigned generation,
	Bool cache, void *context)
{
	return picture_async_request(dpy, drawable, path, attributes, factor,
	    target_width, target_height, generation, cache, context, False,
	    NULL);
}

Bool picture_async_cursor(picture_async *manager, const char *path,
	monitor_scale_factor factor, const int hotspot[2], FvwmPicture **view)
{
	if (view == NULL)
	{
		errno = EINVAL;
		return True;
	}
	*view = NULL;
	if (path == NULL || hotspot == NULL)
	{
		errno = EINVAL;
		return True;
	}
	if (manager == NULL || !USE_SVG)
	{
		return False;
	}
	image_input_prefix prefix;
	struct stat identity;

	if (!image_input_probe(picture_async_real_path(path),
	    FVWM_IMAGE_INPUT_LIMIT, &prefix, &identity))
	{
		return True;
	}
	/* Native animation keeps its loader/nominal sizing. Detect its magic
	 * even when the file name suggests another format. */
	if ((prefix.length >= 4 &&
	     (memcmp(prefix.bytes, "Xcur", 4) == 0 ||
	      memcmp(prefix.bytes, "\211PNG", 4) == 0)) ||
	    (!picture_async_svg_first(path) &&
	     !picture_async_compressed_xpm(path) &&
	     ((prefix.length >= 9 &&
	       memcmp(prefix.bytes, "/* XPM */", 9) == 0) ||
	      (prefix.length >= 6 && memcmp(prefix.bytes, "! XPM2", 6) == 0) ||
	      (prefix.length >= 7 && memcmp(prefix.bytes, "#define", 7) == 0))))
	{
		return False;
	}
	char *owned_path = fxstrdup(path);

	*view = picture_async_request(manager->dpy, manager->drawable,
	    owned_path, (FvwmPictureAttributes){0}, factor, 0, 0,
	    WM_UI_RESOURCE_GENERATION_INITIAL, False, manager, True, hotspot);
	if (*view == NULL)
	{
		free(owned_path);
	}
	return True;
}

FvwmPicture *picture_async_effect(picture_async *manager, FvwmPicture *source,
	picture_color *background, picture_color *tint,
	unsigned tint_percent, unsigned alpha_percent)
{
	if (manager == NULL || !picture_is_usable(source) ||
	    source->depth != Pdepth || source->depth <= 1 || background == NULL ||
	    tint == NULL || source->picture > UINT32_MAX ||
	    source->mask > UINT32_MAX || source->alpha > UINT32_MAX ||
	    Pcmap > UINT32_MAX || picture_color_pixel(background) > UINT32_MAX ||
	    picture_color_pixel(tint) > UINT32_MAX)
	{
		errno = EINVAL;
		return NULL;
	}
	image_effect_request request = {.width = source->width,
	    .height = source->height, .pixmap = source->picture,
	    .mask = source->mask, .alpha = source->alpha,
	    .visual = XVisualIDFromVisual(Pvisual), .depth = Pdepth,
	    .alpha_depth = FRenderGetAlphaDepth(), .colormap = Pcmap,
	    .background = picture_color_pixel(background),
	    .tint = picture_color_pixel(tint), .tint_percent = tint_percent,
	    .alpha_percent = alpha_percent};
	unsigned char checked[IMAGE_EFFECT_REQUEST_SIZE];

	if (!image_effect_request_encode(checked, &request))
	{
		return NULL;
	}
	picture_async_job *job = fxcalloc(1, sizeof(*job));

	job->manager = manager;
	job->path = fxstrdup(source->name ? source->name : "colorset effect");
	job->effect = True;
	job->effect_request = request;
	job->effect_source = picture_snapshot(source);
	job->effect_background = picture_color_retain(background);
	job->effect_tint = picture_color_retain(tint);
	job->visual = Pvisual;
	job->cmap = Pcmap;
	job->depth = Pdepth;
	job->factor.quarters = source->scale_quarters;
	job->generation = source->generation;
	job->default_visual = Pvisual == DefaultVisual(manager->dpy,
	    DefaultScreen(manager->dpy)) && Pcmap == DefaultColormap(manager->dpy,
	    DefaultScreen(manager->dpy));
	job->previous = &manager->jobs;
	job->next = manager->jobs;
	if (job->next != NULL)
	{
		job->next->previous = &job->next;
	}
	manager->jobs = job;
	job->queue_previous = manager->queue_tail;
	*manager->queue_tail = job;
	manager->queue_tail = &job->queue_next;
	return picture_async_subscribe(job, fxstrdup(job->path));
}

FvwmPicture *picture_async_background(picture_async *manager,
	const struct background_plan *plan, unsigned width, unsigned height)
{
	char checked[BACKGROUND_SUFFIX_MAX + 1];
	size_t pixels;

	if (!manager || !background_plan_format(checked, sizeof(checked), plan) ||
	    !FImageSizeIsSafe(width, height, IMAGE_SVG_MAX_DIMENSION,
		IMAGE_SVG_MAX_PIXELS, &pixels) || manager->view_count >= 1024)
		return NULL;
	picture_async_job *job = fxcalloc(1, sizeof(*job));

	job->manager = manager;
	job->path = fxstrdup("private background");
	job->background = True;
	job->background_plan = *plan;
	job->target_width = width;
	job->target_height = height;
	job->visual = Pvisual;
	job->cmap = Pcmap;
	job->depth = Pdepth;
	job->factor.quarters = 4;
	job->default_visual = Pvisual == DefaultVisual(manager->dpy,
		DefaultScreen(manager->dpy)) && Pcmap == DefaultColormap(manager->dpy,
		DefaultScreen(manager->dpy));
	job->previous = &manager->jobs;
	job->next = manager->jobs;
	if (job->next)
		job->next->previous = &job->next;
	manager->jobs = job;
	job->queue_previous = manager->queue_tail;
	*manager->queue_tail = job;
	manager->queue_tail = &job->queue_next;
	return picture_async_subscribe(job, fxstrdup(job->path));
}

Bool picture_async_shutdown_ready(picture_async *manager)
{
	picture_async_job *job = manager != NULL ? manager->active : NULL;

	if (job == NULL || (!job->effect && !job->background) ||
	    job->process == NULL)
	{
		return True;
	}
	image_process_cancel(job->process);
	if (image_process_step(job->process) == IMAGE_PROCESS_RUNNING)
	{
		return False;
	}
	image_process_destroy(job->process);
	job->process = NULL;
	return True;
}

Cursor picture_async_take_cursor(FvwmPicture *view)
{
	picture_async_view *entry = view != NULL && view->is_view ?
	    view->view_context : NULL;

	if (entry == NULL || entry->job == NULL || view->pending ||
	    !entry->job->cursor ||
	    entry->job->cache || entry->job->subscribers != 1 ||
	    view->backing != entry->job->picture)
	{
		return None;
	}
	Cursor cursor = view->backing->prepared_cursor;

	view->backing->prepared_cursor = None;
	return cursor;
}

picture_async *picture_async_create(Display *dpy, Window drawable)
{
	if (picture_async_owner != NULL)
	{
		errno = EBUSY;
		return NULL;
	}
	picture_async *manager = fxcalloc(1, sizeof(*manager));

	manager->dpy = dpy;
#if defined(HAVE_XCURSOR) && XRenderSupport
	if (FRenderGetExtensionSupported())
	{
		/* Capability discovery is startup work. Preserve Xcursor's
		 * server checks and user preference for core cursors. */
		if (XcursorSupportsARGB(dpy))
		{
			manager->cursor_format = XRenderFindStandardFormat(dpy,
			    PictStandardARGB32);
		}
	}
#endif
	if (picture_async_shutdown_errors.dpy == dpy &&
	    NextRequest(dpy) < picture_async_shutdown_errors.end[2])
	{
		memset(&picture_async_shutdown_errors, 0,
		    sizeof(picture_async_shutdown_errors));
	}
	XSetWindowAttributes attributes = {.event_mask = PropertyChangeMask};

	manager->window_request = NextRequest(dpy);
	manager->ack_window = XCreateWindow(dpy, drawable, 0, 0, 1, 1, 0, 0,
	    InputOnly, CopyFromParent, CWEventMask, &attributes);
	manager->drawable = drawable;
	manager->queue_tail = &manager->queue;
	manager->views_tail = &manager->views;
	picture_set_loader(picture_async_load, manager);
	picture_async_owner = manager;
	return manager;
}

void picture_async_descriptors(picture_async *manager,
	struct pollfd descriptors[2])
{
	descriptors[0] = (struct pollfd){-1, 0, 0};
	descriptors[1] = descriptors[0];
	if (manager != NULL && manager->active != NULL &&
	    manager->active->process != NULL)
	{
		image_process_descriptors(manager->active->process,
		    descriptors);
	}
}

int picture_async_timeout_ms(const picture_async *manager, Bool outer_boundary)
{
	if (manager == NULL)
	{
		return -1;
	}
	if (outer_boundary && (manager->ready_work != 0 ||
	    (manager->queue != NULL && manager->active == NULL)))
	{
		return 0;
	}
	if (manager->active == NULL)
	{
		return -1;
	}
	if (manager->active->waiting_ack && !manager->window_refused)
	{
		return -1;
	}
	return manager->active->process != NULL ?
	    image_process_timeout_ms(manager->active->process) :
	    outer_boundary ? 0 : -1;
}

Bool picture_async_busy(const picture_async *manager)
{
	return manager != NULL && (manager->active != NULL ||
	    manager->queue != NULL || manager->ready_work != 0);
}

Bool picture_async_handle_error(picture_async *manager,
	const XErrorEvent *error)
{
	if (error->display == picture_async_shutdown_errors.dpy)
	{
		for (unsigned i = 0; i < 3; i++)
		{
			unsigned long first =
			    picture_async_shutdown_errors.first[i];
			unsigned long end =
			    picture_async_shutdown_errors.end[i];

			if (error->serial >= first && error->serial < end)
			{
				return True;
			}
		}
	}
	if (manager == NULL || error->display != manager->dpy)
	{
		return False;
	}
	if (error->serial == manager->window_request)
	{
		manager->window_refused = True;
		return True;
	}
	picture_async_job *job = manager->active;

	if (job == NULL)
	{
		return False;
	}
	unsigned long end = job->request_scope_open ?
	    NextRequest(manager->dpy) : job->end_request;

	if (error->serial >= job->first_request && error->serial < end)
	{
		job->x_error = True;
		if (job->effect && job->picture != NULL &&
		    error->request_code == X_CreatePixmap &&
		    error->serial == job->effect_pixmap_request)
		{
			job->picture->picture = None;
		}
		if (job->cursor && job->picture != NULL &&
		    error->serial == job->cursor_request)
		{
			job->picture->prepared_cursor = None;
		}
		if (!job->cleaning && (error->request_code == X_CreatePixmap ||
		    error->request_code == X_CreateGC) &&
		    job->creation_error_count < 6)
		{
			job->creation_errors[job->creation_error_count] =
			    *error;
			job->creation_error_count++;
		}
		return True;
	}
	if (job->barrier_request != 0 && error->serial == job->barrier_request)
	{
		manager->window_refused = job->x_error = True;
		return True;
	}
	return False;
}

Bool picture_async_handle_event(picture_async *manager, const XEvent *event)
{
	if (manager == NULL || event->type != PropertyNotify ||
	    event->xproperty.display != manager->dpy ||
	    event->xproperty.window != manager->ack_window ||
	    event->xproperty.atom != XA_CARDINAL ||
	    event->xproperty.send_event ||
	    event->xproperty.state != PropertyNewValue)
	{
		return False;
	}
	picture_async_job *job = manager->active;

	if (job != NULL && job->waiting_ack &&
	    event->xproperty.serial == job->barrier_request)
	{
		/* Earlier errors on this connection have already reached its
		 * handler. Conversion and publication wait for outer service. */
		job->waiting_ack = False;
	}
	return True;
}

static void picture_async_scope_begin(picture_async_job *job)
{
	job->first_request = NextRequest(job->manager->dpy);
	job->request_scope_open = True;
}

static void picture_async_scope_ack(picture_async_job *job)
{
	Display *dpy = job->manager->dpy;

	job->end_request = NextRequest(dpy);
	job->request_scope_open = False;
	if (!job->manager->window_refused)
	{
		unsigned long marker = job->end_request;

		job->barrier_request = NextRequest(dpy);
		job->waiting_ack = True;
		XChangeProperty(dpy, job->manager->ack_window,
		    XA_CARDINAL, XA_CARDINAL, 32, PropModeReplace,
		    (const unsigned char *)&marker, 1);
	}
}

static void picture_async_failed(picture_async_job *job, int error)
{
	job->phase = PICTURE_JOB_FAILED;
	job->manager->active = NULL;
	job->manager->ready_work += job->subscribers;
	fvwm_debug(__func__, "%s refused: %s (%s)",
	    job->effect ? "Image effect" : "SVG", job->path,
	    strerror(error ? error : EIO));
	image_input_destroy(&job->source);
	image_decompress_result_destroy(&job->expanded);
	image_svg_result_destroy(&job->result);
	image_effect_result_destroy(&job->effect_result);
	PDestroyFvwmPicture(job->manager->dpy, job->picture);
	job->picture = NULL;
	if (job->subscribers == 0)
	{
		picture_async_job_free(job);
	}
}

static void picture_async_visual(const picture_async_job *job)
{
	if (job->default_visual)
	{
		PictureUseDefaultVisual();
	}
	else
	{
		PictureUseFvwmVisual();
	}
}

static void picture_async_prepare_cursor(picture_async_job *job)
{
	FvwmPicture *picture = job->picture;
	Display *display = job->manager->dpy;
	int hotspot[2];
	unsigned extent[2] = {picture->width, picture->height};

	for (unsigned i = 0; i < 2; i++)
	{
		int source = job->hotspot[i] >= 0 ? job->hotspot[i] :
		    job->source_hotspot[i];

		hotspot[i] = extent[i] / 2;
		if (source >= 0)
		{
			if (wm_ui_scale_hotspot(source, extent[i], job->factor,
			    &hotspot[i]) != MONITOR_SCALE_CONVERSION_OK)
			{
				hotspot[i] = source < (int)extent[i] ? source :
				    (int)extent[i] - 1;
			}
		}
	}
#if defined(HAVE_XCURSOR) && XRenderSupport
	if (job->manager->cursor_format != NULL)
	{
		Picture render = XRenderCreatePicture(display, picture->picture,
		    job->manager->cursor_format, 0, NULL);

		job->cursor_request = NextRequest(display);
		picture->prepared_cursor = XRenderCreateCursor(display, render,
		    hotspot[0], hotspot[1]);
		XRenderFreePicture(display, render);
		return;
	}
#endif
	XColor fore = {0};
	XColor back = {.red = 65535, .green = 65535, .blue = 65535};

	job->cursor_request = NextRequest(display);
	picture->prepared_cursor = XCreatePixmapCursor(display,
	    picture->picture, picture->mask, &fore, &back,
	    hotspot[0], hotspot[1]);
}

static void picture_async_prepare_effect(picture_async_job *job)
{
	picture_async *manager = job->manager;
	FvwmPicture *source = job->effect_source;
	image_effect_request *request = &job->effect_request;
	Bool copy = request->tint_percent == 0 &&
	    request->alpha_percent == 100 && source->alpha == None;

	if (!copy && !FRenderGetExtensionSupported())
	{
		/* Source/color requests on our connection must be processed
		 * before a separate X connection can read those resources.
		 * Use the same asynchronous acknowledgement, never XSync. */
		picture_async_scope_begin(job);
		job->phase = PICTURE_JOB_START_EFFECT;
		picture_async_scope_ack(job);
		return;
	}
	Bool restore_default = Pvisual == DefaultVisual(manager->dpy,
	    DefaultScreen(manager->dpy)) && Pcmap == DefaultColormap(manager->dpy,
	    DefaultScreen(manager->dpy));

	picture_async_visual(job);
	picture_async_scope_begin(job);
	FvwmPicture *picture = fxcalloc(1, sizeof(*picture));

	picture->count = 1;
	picture->width = source->width;
	picture->height = source->height;
	picture->depth = source->depth;
	picture->color_cmap = job->cmap;
	picture->effect_background = picture_color_retain(job->effect_background);
	job->picture = picture;
	job->effect_pixmap_request = NextRequest(manager->dpy);
	picture->picture = XCreatePixmap(manager->dpy, manager->drawable,
	    picture->width, picture->height, picture->depth);
	XGCValues values = {.foreground = request->background};
	GC gc = XCreateGC(manager->dpy, picture->picture, GCForeground, &values);

	XFillRectangle(manager->dpy, picture->picture, gc, 0, 0,
	    picture->width, picture->height);
	Bool rendered;

	if (copy)
	{
		XSetClipMask(manager->dpy, gc, source->mask);
		XCopyArea(manager->dpy, source->picture, picture->picture, gc,
		    0, 0, picture->width, picture->height, 0, 0);
		rendered = True;
	}
	else
	{
		rendered = FRenderRender(manager->dpy, manager->drawable,
		    source->picture, source->mask, source->alpha, source->depth,
		    request->alpha_percent, request->tint, request->tint_percent,
		    picture->picture, gc, None, 0, 0, picture->width,
		    picture->height, 0, 0, picture->width, picture->height, False);
	}
	XFreeGC(manager->dpy, gc);
	if (rendered)
	{
		job->cleaning = True;
		job->phase = PICTURE_JOB_ACKING;
	}
	else
	{
		PDestroyFvwmPicture(manager->dpy, job->picture);
		job->picture = NULL;
		job->phase = PICTURE_JOB_START_EFFECT;
	}
	picture_async_scope_ack(job);
	if (restore_default)
	{
		PictureUseDefaultVisual();
	}
	else
	{
		PictureUseFvwmVisual();
	}
}

void picture_async_service(picture_async *manager, Bool outer_boundary)
{
	if (manager == NULL)
	{
		return;
	}
	picture_async_job *job = manager->active;

	if (job == NULL)
	{
		if (!outer_boundary || manager->queue == NULL)
		{
			return;
		}
		job = manager->active = manager->queue;
		picture_async_queue_remove(job);
		if (manager->window_refused)
		{
			picture_async_failed(job, EIO);
			return;
		}
		if (job->effect)
		{
			picture_async_prepare_effect(job);
			return;
		}
		if (job->background)
		{
			job->process = image_background_start(&job->background_plan,
				job->target_width, job->target_height,
				&job->source, &job->result);
			if (!job->process)
				picture_async_failed(job, errno);
			else
				job->phase = PICTURE_JOB_DECODING;
			return;
		}
		if (!image_input_load(picture_async_real_path(job->path),
		    FVWM_IMAGE_INPUT_LIMIT, &job->source, &job->opened))
		{
			picture_async_failed(job, errno);
			return;
		}
		if (!picture_async_identity(&job->identity, &job->opened))
		{
			picture_async_failed(job, EAGAIN);
			return;
		}
		job->phase = job->svg_first ?
		    PICTURE_JOB_START_SVG : PICTURE_JOB_XPM;
	}
	if (job->process != NULL)
	{
		image_process_status status = image_process_step(job->process);

		if (status == IMAGE_PROCESS_RUNNING)
		{
			return;
		}
		int error = image_process_error(job->process);

		image_process_destroy(job->process);
		job->process = NULL;
		if (job->subscribers == 0 || error == ECANCELED)
		{
			picture_async_failed(job, error ? error : ECANCELED);
			return;
		}
		if (job->effect)
		{
			if (status != IMAGE_PROCESS_SUCCEEDED ||
			    !image_effect_result_complete(&job->effect_result))
			{
				picture_async_failed(job, error ? error : EPROTO);
				return;
			}
			image_input_destroy(&job->source);
			job->phase = PICTURE_JOB_DECODED;
			return;
		}
		if (job->phase == PICTURE_JOB_EXPANDING)
		{
			job->expansion_ok = status == IMAGE_PROCESS_SUCCEEDED;
			if (!job->expansion_ok)
			{
				image_decompress_result_destroy(&job->expanded);
			}
			job->phase = PICTURE_JOB_XPM;
			return;
		}
		if (status != IMAGE_PROCESS_SUCCEEDED ||
		    !image_svg_result_complete(&job->result))
		{
			if (job->background)
			{
				picture_async_failed(job, error ? error : EPROTO);
				return;
			}
			job->decode_error = error ? error : EPROTO;
			image_svg_result_destroy(&job->result);
			job->phase = job->xpm_tried ?
			    PICTURE_JOB_FALLBACK : PICTURE_JOB_XPM;
			return;
		}
		image_input_destroy(&job->source);
		image_decompress_result_destroy(&job->expanded);
		job->phase = PICTURE_JOB_DECODED;
		return;
	}
	if (!outer_boundary || (job->waiting_ack && !manager->window_refused))
	{
		return;
	}
	if (job->subscribers == 0 && job->builder == NULL &&
	    job->picture == NULL)
	{
		manager->active = NULL;
		picture_async_job_free(job);
		return;
	}
	if (job->phase == PICTURE_JOB_START_EFFECT)
	{
		if (job->x_error)
		{
			picture_async_failed(job, EIO);
			return;
		}
		job->process = image_effect_start(DisplayString(manager->dpy),
		    &job->effect_request, &job->source, &job->effect_result);
		if (job->process == NULL)
		{
			picture_async_failed(job, errno);
		}
		else
		{
			job->phase = PICTURE_JOB_DECODING;
		}
		return;
	}
	if (job->phase == PICTURE_JOB_START_SVG)
	{
		job->svg_tried = True;
		job->process = image_svg_start(&job->source, job->path,
		    job->factor, job->target_width, job->target_height,
		    &job->result);
		if (job->process == NULL)
		{
			job->decode_error = errno;
			job->phase = job->xpm_tried ?
			    PICTURE_JOB_FALLBACK : PICTURE_JOB_XPM;
		}
		else
		{
			job->phase = PICTURE_JOB_DECODING;
		}
		return;
	}
	if (job->phase == PICTURE_JOB_XPM)
	{
		Bool compressed = picture_async_compressed_xpm(job->path);

		if (compressed && !job->expansion_tried && XpmSupport &&
		    picture_async_real_path(job->path) == job->path)
		{
			char *args[] = {FVWM_IMAGE_GZIP, "-dqc", NULL};

			job->expansion_tried = True;
			job->process = image_decompress_start(&job->source,
			    FVWM_IMAGE_GZIP, args,
			    FVWM_IMAGE_XPM_EXPANDED_LIMIT,
			    FVWM_IMAGE_DECOMPRESS_TIMEOUT_MS, &job->expanded);
			if (job->process != NULL)
			{
				job->phase = PICTURE_JOB_EXPANDING;
			}
			return;
		}
		const image_input *xpm_input = !compressed ? &job->source :
		    job->expansion_ok ? &job->expanded.output : NULL;
		CARD32 *pixels = NULL;
		int width = 0, height = 0;
		Bool restore_default = Pvisual == DefaultVisual(manager->dpy,
		    DefaultScreen(manager->dpy)) && Pcmap ==
		    DefaultColormap(manager->dpy, DefaultScreen(manager->dpy));

		picture_async_visual(job);

		job->xpm_tried = True;
		if (picture_decode_xpm_cursor_input(manager->dpy, job->path,
		    &job->source, xpm_input, job->factor, job->target_width,
		    job->target_height, &pixels, &width, &height,
		    job->source_hotspot))
		{
			job->result.pixels = pixels;
			job->result.width = width;
			job->result.height = height;
			job->result.pixel_bytes = (size_t)width * height *
			    sizeof(*pixels);
			image_input_destroy(&job->source);
			image_decompress_result_destroy(&job->expanded);
			job->phase = PICTURE_JOB_DECODED;
		}
		else
		{
			job->phase = job->svg_tried ?
			    PICTURE_JOB_FALLBACK : PICTURE_JOB_START_SVG;
		}
		if (restore_default)
		{
			PictureUseDefaultVisual();
		}
		else
		{
			PictureUseFvwmVisual();
		}
		return;
	}
	if (job->phase == PICTURE_JOB_FALLBACK)
	{
		if (job->cursor)
		{
			picture_async_failed(job,
			    job->decode_error ? job->decode_error : EINVAL);
			return;
		}
		if (job->subscribers == 0)
		{
			manager->active = NULL;
			picture_async_job_free(job);
			return;
		}
		/* Preserve non-SVG fallbacks from the active snapshot.
		 * XPM has already been attempted and cannot start another wait.
		 * Nested service never invokes these synchronous decoders. */
		Bool restore_default = Pvisual == DefaultVisual(manager->dpy,
		    DefaultScreen(manager->dpy)) &&
		    Pcmap == DefaultColormap(manager->dpy,
		    DefaultScreen(manager->dpy));
		char *path = fxstrdup(job->path);

		picture_async_visual(job);
		picture_async_scope_begin(job);
		job->picture = picture_load_non_svg_input(manager->dpy,
		    manager->drawable, path, &job->source, &job->opened, NULL,
		    job->attributes, job->factor, job->target_width,
		    job->target_height);
		if (job->picture == NULL)
		{
			free(path);
			job->x_error = True;
		}
		else
		{
			/* Assign the name after acknowledgement. */
			free(job->picture->name);
			job->picture->name = NULL;
		}
		job->cleaning = True;
		job->phase = PICTURE_JOB_ACKING;
		image_decompress_result_destroy(&job->expanded);
		image_input_destroy(&job->source);
		image_svg_result_destroy(&job->result);
		picture_async_scope_ack(job);
		if (restore_default)
		{
			PictureUseDefaultVisual();
		}
		else
		{
			PictureUseFvwmVisual();
		}
		return;
	}
	if (job->cleaning)
	{
		if (job->x_error || job->picture == NULL ||
		    job->subscribers == 0 ||
		    (!job->effect && !job->background &&
		     !picture_async_current(job, &job->opened)))
		{
			if (job->picture != NULL)
			{
				picture_async_scope_begin(job);
				PDestroyFvwmPicture(manager->dpy, job->picture);
				job->picture = NULL;
				picture_async_scope_ack(job);
				return;
			}
			picture_async_failed(job, job->x_error ? EIO : EAGAIN);
			return;
		}
		FvwmPicture *picture = job->picture;

		picture->name = fxstrdup(job->path);
		picture->stamp = ((FileStamp)job->opened.st_mtime << 13) +
		    job->opened.st_size;
		picture->scale_quarters = job->factor.quarters;
		picture->target_width = job->target_width;
		picture->target_height = job->target_height;
		picture->generation = job->generation;
		if (job->cache)
		{
			picture_cache_publish(picture);
		}
		job->phase = PICTURE_JOB_READY;
		manager->active = NULL;
		manager->ready_work += job->subscribers;
		return;
	}
	if (job->subscribers == 0 && job->builder == NULL)
	{
		manager->active = NULL;
		picture_async_job_free(job);
		return;
	}
	Bool restore_default = Pvisual == DefaultVisual(manager->dpy,
	    DefaultScreen(manager->dpy)) &&
	    Pcmap == DefaultColormap(manager->dpy,
		    DefaultScreen(manager->dpy));

	picture_async_visual(job);
	if (job->builder != NULL)
	{
		for (unsigned i = 0; i < job->creation_error_count; i++)
		{
			picture_argb_creation_error(job->builder,
			    &job->creation_errors[i]);
		}
		job->creation_error_count = 0;
	}
	if (job->x_error || manager->window_refused || job->subscribers == 0)
	{
		if (job->builder != NULL)
		{
			picture_argb_cancel(job->builder);
		}
		job->x_error = job->x_error || manager->window_refused;
		job->conversion_done = True;
	}
	picture_async_scope_begin(job);
	if (job->conversion_done)
	{
		if (job->builder != NULL)
		{
			if (!job->x_error && job->subscribers != 0)
			{
				job->picture = picture_argb_take(job->builder);
				if (job->effect && job->picture != NULL)
				{
					job->picture->effect_background =
					    picture_color_retain(job->effect_background);
				}
				if (job->cursor && job->picture != NULL)
				{
					picture_async_prepare_cursor(job);
				}
			}
			else
			{
				picture_argb_step(job->builder);
			}
			picture_argb_destroy(job->builder);
			job->builder = NULL;
		}
		job->cleaning = True;
		job->phase = PICTURE_JOB_ACKING;
		image_svg_result_destroy(&job->result);
		image_effect_result_destroy(&job->effect_result);
	}
	else
	{
		if (job->builder == NULL)
		{
			if (job->effect)
			{
				job->builder = picture_argb_start_effects(manager->dpy,
				    manager->drawable, job->effect_result.pixels,
				    job->effect_result.count, job->effect_result.width,
				    job->effect_result.height,
				    picture_color_pixel(job->effect_background));
			}
			else
#if defined(HAVE_XCURSOR) && XRenderSupport
			if (job->cursor && manager->cursor_format != NULL)
			{
				job->builder = picture_argb_start_argb32(
				    manager->dpy,
				    manager->drawable, job->result.pixels,
				    job->result.pixel_bytes / sizeof(uint32_t),
				    job->result.width, job->result.height);
			}
			else
#endif
			{
				job->builder = picture_argb_start(manager->dpy,
				    manager->drawable, job->result.pixels,
				    job->result.pixel_bytes / sizeof(uint32_t),
				    job->result.width, job->result.height,
				    job->cursor ?
				    (FvwmPictureAttributes){.mask = FPAM_NO_ALPHA |
				        FPAM_MONOCHROME} :
				    job->attributes);
			}
			job->phase = PICTURE_JOB_CONVERTING;
			if (job->builder == NULL)
			{
				job->x_error = job->cleaning = True;
				image_svg_result_destroy(&job->result);
			}
		}
		if (job->builder != NULL)
		{
			job->conversion_done = picture_argb_step(job->builder);
		}
	}
	picture_async_scope_ack(job);
	if (restore_default)
	{
		PictureUseDefaultVisual();
	}
	else
	{
		PictureUseFvwmVisual();
	}
}

void picture_async_commit_ready(picture_async *manager,
	picture_async_policy_fn policy, picture_async_changed_fn changed,
	void *context)
{
	if (manager == NULL)
	{
		return;
	}
	FvwmPicture *batch[PICTURE_ASYNC_COMMIT_VIEWS];
	size_t count = manager->view_count;

	if (count > PICTURE_ASYNC_COMMIT_VIEWS)
	{
		count = PICTURE_ASYNC_COMMIT_VIEWS;
	}
	for (size_t i = 0; i < count; i++)
	{
		picture_async_view *entry = manager->views;

		batch[i] = PCloneFvwmPicture(entry->view);
		picture_async_view_remove(entry);
		picture_async_view_append(manager, entry);
	}
	for (size_t i = 0; i < count; i++)
	{
		FvwmPicture *view = batch[i];
		picture_async_view *entry = view->view_context;
		picture_async_job *job = entry->job;
		picture_async_binding *binding = view->view_owner;

		if (view->pending && (job->phase == PICTURE_JOB_READY ||
		    job->phase == PICTURE_JOB_FAILED))
		{
			if (!entry->deferred)
			{
				manager->ready_work--;
			}
			entry->deferred = True;
			picture_async_commit action =
			    job->phase == PICTURE_JOB_FAILED ?
			    PICTURE_ASYNC_DISCARD : binding != NULL ?
			    binding->policy(view, binding->context) :
			    policy != NULL ?
			    policy(view, context) : PICTURE_ASYNC_APPLY;

			if (action != PICTURE_ASYNC_DEFER)
			{
				view->pending = False;
				if (action == PICTURE_ASYNC_APPLY)
				{
					picture_view_set_backing(manager->dpy,
					    view,
					    job->picture);
					if (binding == NULL && changed != NULL)
					{
						changed(view, context);
					}
				}
				if (binding != NULL)
				{
					binding->settled(view,
					    action == PICTURE_ASYNC_APPLY,
					    binding->context);
				}
			}
		}
		PDestroyFvwmPicture(manager->dpy, view);
	}
}

void picture_async_cancel_worker(picture_async *manager)
{
	if (manager != NULL && manager->active != NULL)
	{
		image_process_cancel(manager->active->process);
	}
}

Bool picture_async_destroy(picture_async *manager)
{
	if (manager == NULL)
	{
		return True;
	}
	if (!picture_async_shutdown_ready(manager))
	{
		errno = EBUSY;
		return False;
	}
	picture_set_loader(NULL, NULL);
	picture_async_job *active = manager->active;

	picture_async_shutdown_errors.dpy = manager->dpy;
	picture_async_shutdown_errors.first[0] = manager->window_request;
	picture_async_shutdown_errors.end[0] = manager->window_request + 1;
	picture_async_shutdown_errors.first[1] = active != NULL ?
	    active->first_request : 0;
	picture_async_shutdown_errors.end[1] = active != NULL &&
	    active->first_request != 0 ? active->end_request : 0;
	if (active != NULL && active->first_request != 0 &&
	    active->barrier_request >= active->end_request)
	{
		picture_async_shutdown_errors.end[1] =
		    active->barrier_request + 1;
	}
	picture_async_shutdown_errors.first[2] = NextRequest(manager->dpy);

	if (active != NULL && active->process != NULL)
	{
		if (!image_process_handoff(active->process))
		{
			return False;
		}
		active->process = NULL;
	}
	if (active != NULL && active->builder != NULL)
	{
		for (unsigned i = 0; i < active->creation_error_count; i++)
		{
			picture_argb_creation_error(active->builder,
			    &active->creation_errors[i]);
		}
		picture_argb_cancel(active->builder);
		picture_async_visual(active);
		Bool done = picture_argb_step(active->builder);

		PictureUseFvwmVisual();
		if (!done || !picture_argb_destroy(active->builder))
		{
			return False;
		}
		active->builder = NULL;
	}
	while (manager->views != NULL)
	{
		picture_async_view *entry = manager->views;

		manager->views = entry->next;
		entry->view->view_context = NULL;
		entry->view->view_release = NULL;
		entry->view->pending = False;
		free(entry);
	}
	while (manager->jobs != NULL)
	{
		picture_async_job_free(manager->jobs);
	}
	if (!manager->window_refused)
	{
		XDestroyWindow(manager->dpy, manager->ack_window);
	}
	picture_async_shutdown_errors.end[2] = NextRequest(manager->dpy);
	picture_async_owner = NULL;
	free(manager);
	return True;
}
