/* This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */
#include "config.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <X11/Xatom.h>
#include "libs/wcontext.h"
#include "fvwm/fvwm.h"
#include "fvwm/add_window.h"
#include "fvwm/execcontext.h"
#include "fvwm/externs.h"
#include "fvwm/functions.h"
#include "fvwm/screen.h"
#include "fvwm/style.h"
#include "fvwm/style_id_filter.h"
#include "fvwm/style_match.h"
#include "libs/wild.h"

static Bool measuring, force_reference, force_previous, force_collision;
static Bool force_index_collision;
static unsigned long comparisons, matches, grabbed_matches, grabs;
static unsigned grab_depth, wildcard_call_depth;
static unsigned long overlap_checks, id_visits, lookup_visits;
static uint64_t grabbed_ns, grab_started;

void __real_style_match_prepare(style_match_patterns *, const style_id_t *);
void __wrap_style_match_prepare(style_match_patterns *patterns,
	const style_id_t *id)
{
	__real_style_match_prepare(patterns, id);
	if (force_index_collision && patterns->lookup_bucket)
		patterns->lookup_bucket = 1;
}

unsigned __real_style_match_lookup_subject(const style_match_subject *,
	unsigned [STYLE_LOOKUP_STREAMS]);
unsigned __wrap_style_match_lookup_subject(const style_match_subject *subject,
	unsigned buckets[STYLE_LOOKUP_STREAMS])
{
	if (!force_index_collision)
		return __real_style_match_lookup_subject(subject, buckets);
	buckets[0] = 0;
	buckets[1] = 1;
	return 2;
}

bool __real_style_match_lookup_enabled(size_t, size_t);
bool __wrap_style_match_lookup_enabled(size_t count, size_t indexed)
{
	return !force_reference &&
		__real_style_match_lookup_enabled(count, indexed);
}

uint64_t __real_style_id_hash(const style_id_t *);
uint64_t __wrap_style_id_hash(const style_id_t *id)
{
	return force_collision ? 0 : __real_style_id_hash(id);
}

bool __real_style_id_filter_hash_may_repeat(const style_id_filter *, uint64_t);
bool __wrap_style_id_filter_hash_may_repeat(const style_id_filter *filter,
	uint64_t hash)
{
	return force_reference ||
		__real_style_id_filter_hash_may_repeat(filter, hash);
}

size_t __real_style_id_filter_first(const style_id_filter *, uint64_t);
size_t __wrap_style_id_filter_first(const style_id_filter *filter,
	uint64_t hash)
{
	return force_reference ? 0 : __real_style_id_filter_first(filter, hash);
}

bool __real_style_match_id(const style_id_t *, const style_match_patterns *,
	style_match_subject *);
bool __wrap_style_match_id(const style_id_t *id,
	const style_match_patterns *patterns, style_match_subject *subject)
{
	if (measuring)
		lookup_visits++;
	if (measuring && id->name && strncmp(id->name, "probe-", 6) == 0)
		id_visits++;
	if (force_previous)
	{
		/* Disable precisely this batch's metadata. The existing literal,
		 * prefix, compatibility and numeric checks still execute. */
		style_match_patterns previous = *patterns;

		previous.unescaped = previous.single_star = 0;
		return __real_style_match_id(id, &previous, subject);
	}
	if (!force_reference)
		return __real_style_match_id(id, patterns, subject);
	/* Original application predicate, including its three-field OR and
	 * late numeric comparison, is independent of the prepared metadata. */
	if (id->flags.is_compatibility_mode &&
	    !(matchWildcards(id->name, subject->value[0]) ||
	      matchWildcards(id->name, subject->value[1]) ||
	      matchWildcards(id->name, subject->value[2])))
		return false;
	if (id->flags.has_resource &&
	    !matchWildcards(id->resource, subject->value[0]))
		return false;
	if (id->flags.has_class &&
	    !matchWildcards(id->class, subject->value[1]))
		return false;
	if (id->flags.has_name &&
	    !matchWildcards(id->name, subject->value[2]))
		return false;
	if (id->flags.has_icon &&
	    !matchWildcards(id->icon, subject->value[3]))
		return false;
	return !id->flags.has_window_id || id->window_id == subject->window;
}

bool __real_style_match_ids_may_overlap(const style_id_t *,
	const style_match_patterns *, const style_id_t *,
	const style_match_patterns *);
bool __wrap_style_match_ids_may_overlap(const style_id_t *a,
	const style_match_patterns *ap, const style_id_t *b,
	const style_match_patterns *bp)
{
	if (measuring)
		overlap_checks++;
	if (force_reference)
		return !a->flags.has_window_id || !b->flags.has_window_id ||
			a->window_id == b->window_id;
	return __real_style_match_ids_may_overlap(a, ap, b, bp);
}

static uint64_t now_ns(void)
{
	struct timespec value;

	assert(clock_gettime(CLOCK_MONOTONIC, &value) == 0);
	return (uint64_t)value.tv_sec * 1000000000 + value.tv_nsec;
}

int __real_strcmp(const char *, const char *);
int __wrap_strcmp(const char *a, const char *b)
{
	/* Only count comparisons between the uniquely named workload rules.
	 * Command parsing/setup and unrelated core strings stay out of scope. */
	if (measuring && strncmp(a, "probe-", 6) == 0 &&
	    strncmp(b, "probe-", 6) == 0)
	{
		comparisons++;
	}
	return __real_strcmp(a, b);
}

int __real_matchWildcards(const char *, const char *);
int __wrap_matchWildcards(const char *pattern, const char *value)
{
	int result;

	if (wildcard_call_depth++ == 0 && measuring && pattern &&
	    strncmp(pattern, "probe-", 6) == 0)
	{
		matches++;
		if (grab_depth != 0)
		{
			grabbed_matches++;
		}
	}
	result = __real_matchWildcards(pattern, value);
	wildcard_call_depth--;
	return result;
}

void __real_MyXGrabServer(Display *);
void __wrap_MyXGrabServer(Display *display)
{
	__real_MyXGrabServer(display);
	if (measuring && grab_depth++ == 0)
	{
		grabs++;
		grab_started = now_ns();
	}
}

void __real_MyXUngrabServer(Display *);
void __wrap_MyXUngrabServer(Display *display)
{
	if (measuring)
	{
		assert(grab_depth != 0);
		if (--grab_depth == 0)
		{
			grabbed_ns += now_ns() - grab_started;
		}
	}
	__real_MyXUngrabServer(display);
}

static void command(const exec_context_t *exc, const char *text)
{
	execute_function_override_window(NULL, exc, (char *)text, NULL,
		FUNC_DONT_DEFER, NULL);
}

static FvwmWindow *create_window(const exec_context_t *exc)
{
	XSizeHints hints = { 0 };
	exec_context_changes_t changes = { 0 };
	initial_window_options_t options = { 0 };
	const char *map_command = NULL;
	const exec_context_t *context;
	FvwmWindow *window;
	Window client = XCreateSimpleWindow(dpy, Scr.Root,
		10, 10, 100, 80, 0, 0, 0);

	XStoreName(dpy, client, "style profile control");
	hints.flags = USPosition | USSize;
	hints.x = hints.y = 10;
	hints.width = 100;
	hints.height = 80;
	XSetWMNormalHints(dpy, client, &hints);
	XSync(dpy, False);
	changes.w.w = client;
	changes.w.wcontext = C_ROOT;
	context = exc_clone_context(exc, &changes,
		ECC_W | ECC_FW | ECC_WCONTEXT);
	options.initial_state = NormalState;
	window = AddWindow(&map_command, context, NULL, &options);
	assert(window != NULL && FW_W(window) == client);
	exc_destroy_context(context);
	return window;
}

static void check_border(FvwmWindow *window, unsigned width)
{
	Atom type;
	int format;
	unsigned long count, remaining;
	unsigned char *data = NULL;
	XWindowAttributes frame, client;

	assert(window->boundary_width == width);
	assert(XGetWindowProperty(dpy, FW_W(window),
		XInternAtom(dpy, "_NET_FRAME_EXTENTS", False), 0, 4, False,
		XA_CARDINAL, &type, &format, &count, &remaining, &data) ==
		Success);
	assert(type == XA_CARDINAL && format == 32 && count == 4);
	assert(remaining == 0 && data != NULL);
	for (unsigned i = 0; i < 4; i++)
	{
		assert(((unsigned long *)data)[i] == width);
	}
	XFree(data);
	assert(XGetWindowAttributes(dpy, FW_W_FRAME(window), &frame));
	assert(XGetWindowAttributes(dpy, FW_W(window), &client));
	assert(frame.width == client.width + 2 * (int)width);
	assert(frame.height == client.height + 2 * (int)width);
}

static void lookup_control(FvwmWindow *window, unsigned count)
{
	FvwmWindow sample = *window;
	window_style style;
	char name[64];

	snprintf(name, sizeof(name), "probe-%04u", count - 1);
	/* Borrow the live record's initialized EWMH/monitor state only for this
	 * synchronous lookup. Never publish this temporary identity. */
	sample.class.res_name = name;
	sample.class.res_class = "unmatched class";
	sample.name.name = "unmatched name";
	lookup_style(&sample, &style);
	assert(SGET_BORDER_WIDTH(style) == 7);
}

static void check_precedence(const exec_context_t *exc, FvwmWindow *window)
{
	FvwmWindow sample = *window;
	window_style style;

	sample.class.res_name = "precedence-window";
	sample.class.res_class = "unmatched class";
	sample.name.name = "unmatched name";
	command(exc, "Style precedence-* BorderWidth 4, HandleWidth 4");
	command(exc, "Style precedence-window BorderWidth 7");
	command(exc, "Style precedence-* HandleWidth 9");
	lookup_style(&sample, &style);
	assert(SGET_BORDER_WIDTH(style) == 7 && SGET_HANDLE_WIDTH(style) == 9);
	simplify_style_list();
	lookup_style(&sample, &style);
	assert(SGET_BORDER_WIDTH(style) == 7 && SGET_HANDLE_WIDTH(style) == 9);
	command(exc, "DestroyStyle precedence-window");
	lookup_style(&sample, &style);
	assert(SGET_BORDER_WIDTH(style) == 4 && SGET_HANDLE_WIDTH(style) == 9);
}

static void check_structured_ids(const exec_context_t *exc, FvwmWindow *window)
{
	FvwmWindow sample = *window;
	window_style style;
	char text[128];

	sample.class.res_name = "selector-window";
	sample.class.res_class = "SelectorClass";
	sample.name.name = "selector-name";
	sample.icon_name.name = "selector-icon";
	command(exc, "Style (Resource selector-*, Class SelectorClass) "
		"BorderWidth 5");
	command(exc, "Style (Resource selector-window, Class SelectorClass) "
		"BorderWidth 7");
	command(exc, "Style (Resource selector-*, Class SelectorClass) "
		"HandleWidth 9");
	command(exc, "Style (Name selector-name, Icon selector-icon) "
		"TitleFormat selected %n, BorderWidth 13");
	command(exc, "Style (Name selector-*, Icon selector-icon) HandleWidth 15");
	command(exc, "Style (Name selector-name, Icon selector-icon) "
		"BorderWidth 17");
	/* Several passes exercise both merge directions, subset retirement and
	 * rebuilding a fresh summary after selector strings have been freed. */
	for (unsigned pass = 0; pass < 16; pass++)
	{
		lookup_style(&sample, &style);
		assert(SGET_BORDER_WIDTH(style) == 17);
		assert(SGET_HANDLE_WIDTH(style) == 15);
		assert(strcmp(SGET_TITLE_FORMAT_STRING(style), "selected %n") == 0);
		if (!Scr.flags.do_need_style_list_update)
			break;
		assert(pass < 15);
		simplify_style_list();
	}
	snprintf(text, sizeof(text), "Style (WindowID 0x%lx) BorderWidth 21",
		(unsigned long)FW_W(window));
	command(exc, text);
	snprintf(text, sizeof(text), "Style (WindowID 0x%lx) BorderWidth 25",
		(unsigned long)Scr.NoFocusWin);
	command(exc, text);
	snprintf(text, sizeof(text), "Style (WindowID 0x%lx) HandleWidth 23",
		(unsigned long)FW_W(window));
	command(exc, text);
	for (unsigned pass = 0; pass < 2; pass++)
	{
		lookup_style(&sample, &style);
		assert(SGET_BORDER_WIDTH(style) == 21);
		assert(SGET_HANDLE_WIDTH(style) == 23);
		simplify_style_list();
	}
	snprintf(text, sizeof(text), "DestroyStyle (WindowID 0x%lx)",
		(unsigned long)FW_W(window));
	command(exc, text);
	lookup_style(&sample, &style);
	assert(SGET_BORDER_WIDTH(style) == 17 && SGET_HANDLE_WIDTH(style) == 15);
	sample.icon_name.name = "another icon";
	lookup_style(&sample, &style);
	assert(SGET_BORDER_WIDTH(style) == 7 && SGET_HANDLE_WIDTH(style) == 9);
	command(exc, "DestroyStyle (Resource selector-window, Class SelectorClass)");
	lookup_style(&sample, &style);
	assert(SGET_BORDER_WIDTH(style) == 5 && SGET_HANDLE_WIDTH(style) == 9);
	sample.class.res_class = "another class";
	lookup_style(&sample, &style);
	assert(SGET_BORDER_WIDTH(style) == 3 && SGET_HANDLE_WIDTH(style) == 3);
}

static void matching_work(const exec_context_t *exc, FvwmWindow *window,
	unsigned count)
{
	FvwmWindow sample = *window;
	window_style style;
	char text[160];
	char resource[] = "probe-duplicate";
	char class[] = "probe-duplicate";
	char name[] = "probe-duplicate";

	/* Retire the measured literal rules before isolating each next cost. */
	for (unsigned i = 0; i < count; i++)
	{
		snprintf(text, sizeof(text), "DestroyStyle probe-%04u", i);
		command(exc, text);
	}
	/* Both documented conjunction orders must retain the numeric selector.
	 * Previously Name first caused the parser to discard WindowID. */
	for (unsigned order = 0; order < 2; order++)
	{
		char selector[128];

		snprintf(selector, sizeof(selector), order == 0 ?
			"(Name order-name, WindowID 0x%lx)" :
			"(WindowID 0x%lx, Name order-name)",
			(unsigned long)FW_W(window));
		snprintf(text, sizeof(text), "Style %s BorderWidth 31", selector);
		command(exc, text);
		sample.name.name = "order-name";
		lookup_style(&sample, &style);
		assert(SGET_BORDER_WIDTH(style) == 31);
		FW_W(&sample) = Scr.NoFocusWin;
		lookup_style(&sample, &style);
		assert(SGET_BORDER_WIDTH(style) == 3);
		FW_W(&sample) = FW_W(window);
		sample.name.name = "other name";
		lookup_style(&sample, &style);
		assert(SGET_BORDER_WIDTH(style) == 3);
		snprintf(text, sizeof(text), "DestroyStyle %s", selector);
		command(exc, text);
	}
	command(exc, "Style metadata-template BorderWidth 33");
	command(exc, "Style (Name metadata-owner) UseStyle metadata-template");
	command(exc, "Style (Name metadata-owner) HandleWidth 35");
	command(exc, "DestroyStyle metadata-template");
	simplify_style_list();
	sample.name.name = "metadata-owner";
	lookup_style(&sample, &style);
	assert(SGET_BORDER_WIDTH(style) == 33 && SGET_HANDLE_WIDTH(style) == 35);
	command(exc, "DestroyStyle (Name metadata-owner)");
	lookup_style(&sample, &style);
	assert(SGET_BORDER_WIDTH(style) == 3);
	for (unsigned kind = 0; kind < 3; kind++)
	{
		const char *label = kind == 0 ? "prefix" :
			kind == 1 ? "duplicate" : "window-id";

		sample.class.res_name = kind == 1 ? resource : "other resource";
		sample.class.res_class = kind == 1 ? class : "other class";
		sample.name.name = kind == 1 ? name : "probe-window";
		for (unsigned i = 0; i < count; i++)
		{
			if (kind == 0)
				snprintf(text, sizeof(text),
					"Style probe-prefix-%04u* BorderWidth 5", i);
			else if (kind == 1)
				snprintf(text, sizeof(text),
					"Style probe-?%04u BorderWidth 5", i);
			else
				snprintf(text, sizeof(text), "Style (Name probe-?*, "
					"WindowID 0x%lx) BorderWidth 5",
					(unsigned long)FW_W(window) + 0x10000 + i);
			command(exc, text);
		}
		matches = grabbed_matches = 0;
		measuring = True;
		lookup_style(&sample, &style);
		measuring = False;
		assert(SGET_BORDER_WIDTH(style) == 3 && grabbed_matches == 0);
		assert(matches == count * (force_reference ?
			(kind == 2 ? 1 : 3) : 0));
		printf("STYLE_MATCH_WORK {\"case\":\"%s\",\"reference\":%s,"
			"\"rules\":%u,\"calls\":%lu}\n", label,
			force_reference ? "true" : "false", count, matches);
		for (unsigned i = 0; i < count; i++)
		{
			if (kind == 0)
				snprintf(text, sizeof(text),
					"DestroyStyle probe-prefix-%04u*", i);
			else if (kind == 1)
				snprintf(text, sizeof(text),
					"DestroyStyle probe-?%04u", i);
			else
				snprintf(text, sizeof(text), "DestroyStyle (Name probe-?*, "
					"WindowID 0x%lx)",
					(unsigned long)FW_W(window) + 0x10000 + i);
			command(exc, text);
		}
	}
}

static void wildcard_work(const exec_context_t *exc, FvwmWindow *window)
{
	FvwmWindow sample = *window;
	window_style style;
	char selector[96], text[160], name[96];
	const char *labels[] = { "minimum", "suffix", "single-star" };

	for (unsigned kind = 0; kind < 3; kind++)
	{
		for (unsigned previous = 0; previous < 2; previous++)
		{
			unsigned long total = 0;

			for (unsigned i = 0; i < 64; i++)
			{
				if (kind == 0)
				{
					snprintf(selector, sizeof(selector),
						"probe-?%04u?*", i);
					strcpy(name, "probe-x");
				}
				else if (kind == 1)
				{
					snprintf(selector, sizeof(selector),
						"probe-?*%04u-end", i);
					strcpy(name, "probe-long-enough-title-wrong");
				}
				else
				{
					snprintf(selector, sizeof(selector), "probe-*%04u", i);
					snprintf(name, sizeof(name), "probe-title-%04u", i);
				}
				sample.name.name = name;
				snprintf(text, sizeof(text),
					"Style (Name %s) BorderWidth 7", selector);
				command(exc, text);
				force_previous = previous;
				matches = 0;
				measuring = True;
				lookup_style(&sample, &style);
				measuring = False;
				force_previous = False;
				total += matches;
				assert(SGET_BORDER_WIDTH(style) == (kind == 2 ? 7 : 3));
				snprintf(text, sizeof(text), "DestroyStyle (Name %s)", selector);
				command(exc, text);
				lookup_style(&sample, &style);
				assert(SGET_BORDER_WIDTH(style) == 3);
			}
			assert(total == (previous ? 64 : 0));
			printf("STYLE_WILDCARD_WORK {\"case\":\"%s\","
				"\"previous\":%s,\"rules\":64,\"calls\":%lu}\n",
				labels[kind], previous ? "true" : "false", total);
		}
	}
}

static void repeated_work(const exec_context_t *exc, FvwmWindow *window)
{
	Bool baseline = getenv("FVWM_STYLE_SIMPLIFY_BASELINE") != NULL;
	FvwmWindow sample = *window;
	window_style style;
	char text[160];

	for (unsigned kind = 0; kind < 3; kind++)
	{
		const char *labels[] = {
			"fingerprint", "prefix-cutoff", "disjoint"
		};
		unsigned long scan_comparisons;

		if (kind != 1)
			command(exc, "Style (Name probe-repeat) BorderWidth 5");
		for (unsigned i = 0; i < (kind == 2 ? 1U : 64U); i++)
		{
			snprintf(text, sizeof(text),
				"Style (Name probe-other-%04u) "
				"BorderWidth 9, HandleWidth 11", i);
			command(exc, text);
		}
		if (kind == 1)
		{
			command(exc, "Style (Name probe-repeat) BorderWidth 5");
			command(exc, "Style (Name probe-gap) BorderWidth 9");
		}
		command(exc, kind == 2 ?
			"Style (Name probe-repeat) HandleWidth 7" :
			"Style (Name probe-repeat) BorderWidth 7");
		sample.name.name = "probe-repeat";
		lookup_style(&sample, &style);
		assert(SGET_BORDER_WIDTH(style) == (kind == 2 ? 5 : 7));
		comparisons = overlap_checks = id_visits = 0;
		measuring = True;
		simplify_style_list();
		measuring = False;
		scan_comparisons = comparisons;

		measuring = True;
		lookup_style(&sample, &style);
		measuring = False;
		assert(SGET_BORDER_WIDTH(style) == (kind == 2 ? 5 : 7));
		if (kind == 2)
			assert(SGET_HANDLE_WIDTH(style) == 7);
		if (!baseline)
		{
			if (!force_collision)
				assert(scan_comparisons == 1);
			if (kind == 1 && !force_collision)
				assert(overlap_checks == 1);
			if (kind == 2)
				assert(id_visits >= 1 && id_visits <= 2);
		}
		printf("STYLE_SIMPLIFY_WORK {\"case\":\"%s\","
			"\"baseline\":%s,\"hash_collision\":%s,"
			"\"comparisons\":%lu,"
			"\"overlap_checks\":%lu,\"lookup_rules\":%lu}\n",
			labels[kind], baseline ? "true" : "false",
			force_collision ? "true" : "false", scan_comparisons, overlap_checks, id_visits);
		/* Repeat/deletion must see the same surviving owners. */
		simplify_style_list();
		command(exc, "DestroyStyle (Name probe-repeat)");
		lookup_style(&sample, &style);
		assert(SGET_BORDER_WIDTH(style) == 3);
		for (unsigned i = 0; i < (kind == 2 ? 1U : 64U); i++)
		{
			snprintf(text, sizeof(text),
				"DestroyStyle (Name probe-other-%04u)", i);
			command(exc, text);
		}
		command(exc, "DestroyStyle (Name probe-gap)");
		simplify_style_list();
	}
}

static void index_compare(FvwmWindow *sample)
{
	window_style candidate, original;
	Bool saved = force_reference;

	lookup_style(sample, &candidate);
	force_reference = True;
	lookup_style(sample, &original);
	force_reference = saved;
	/* Both results borrow the same owned fields; compare the complete
	 * initialized result, including fields beyond the workload border. */
	assert(memcmp(&candidate, &original, sizeof(candidate)) == 0);
}

/* Separate numeric, explicit conjunction and legacy OR workloads. Each
 * starts with fresh rules, then retires them with the real command engine. */
static void index_work(const exec_context_t *exc, FvwmWindow *window,
	unsigned count)
{
	const char *fields[] = { "Resource", "Class", "Name", "Icon" };
	const char *labels[] = { "window-id", "explicit", "compatibility" };
	char text[256], key[64];
	FvwmWindow sample = *window;
	window_style style;

	for (unsigned kind = 0; kind < 3; kind++)
	{
		sample.class.res_name = "index-miss-resource";
		sample.class.res_class = "index-miss-class";
		sample.name.name = "index-miss-name";
		sample.icon_name.name = "index-miss-icon";
		for (unsigned i = 0; i < count; i++)
		{
			if (kind == 0)
				snprintf(text, sizeof(text),
					"Style (WindowID 0x%lx) BorderWidth %u",
					(unsigned long)FW_W(window) + i,
					i == 0 ? 7 : 5);
			else if (kind == 1)
				snprintf(text, sizeof(text),
					"Style (%s index-%04u) BorderWidth 7",
					fields[i % 4], i);
			else
				snprintf(text, sizeof(text),
					"Style index-%04u BorderWidth 7", i);
			command(exc, text);
		}
		for (unsigned target = 0; target < 4; target++)
		{
			snprintf(key, sizeof(key), "index-%04u", target);
			if (kind == 1)
			{
				sample.class.res_name =
					target == 0 ? key : "miss";
				sample.class.res_class =
					target == 1 ? key : "miss";
				sample.name.name = target == 2 ? key : "miss";
				sample.icon_name.name =
					target == 3 ? key : "miss";
			}
			else if (kind == 2)
			{
				/* Equal fields must not duplicate a bucket. */
				sample.class.res_name =
					target == 0 || target == 3 ?
					key : "miss";
				sample.class.res_class =
					target == 1 || target == 3 ?
					key : "miss";
				sample.name.name =
					target == 2 || target == 3 ?
					key : "miss";
			}
			lookup_visits = 0;
			measuring = True;
			for (unsigned pass = 0; pass < 32; pass++)
			{
				lookup_style(&sample, &style);
				assert(SGET_BORDER_WIDTH(style) == 7);
			}
			measuring = False;
			printf("STYLE_INDEX_WORK {\"case\":\"%s\","
				"\"collision\":%s,\"reference\":%s,"
				"\"rules\":%u,\"target\":%u,"
				"\"lookups\":32,\"visits\":%lu}\n",
				labels[kind],
				force_index_collision ? "true" : "false",
				force_reference ? "true" : "false",
				count, target,
				lookup_visits);
			index_compare(&sample);
		}
		const char *identities[] = { NULL, "", "miss", "index-0000",
			"index-0001", "index-0002", "index-0003",
			"index-0255" };

		for (unsigned i = 0; i < 128; i++)
		{
			sample.class.res_name = (char *)identities[i % 8];
			sample.class.res_class =
				(char *)identities[(i / 2) % 8];
			sample.name.name = (char *)identities[(i / 4) % 8];
			sample.icon_name.name = (char *)identities[(i / 8) % 8];
			index_compare(&sample);
		}
		/* Exercise live-index tail merges, interference, both move
		 * paths, removal and reinsertion. The original traversal
		 * runs these same assertions. */
		sample.class.res_name = sample.class.res_class = "index-0000";
		sample.name.name = sample.icon_name.name = "index-0000";
		if (kind == 0)
			snprintf(text, sizeof(text),
				"Style (WindowID 0x%lx, Name *) BorderWidth 9",
				(unsigned long)FW_W(window));
		else
			snprintf(text, sizeof(text), kind == 1 ?
				"Style (Resource index-*) BorderWidth 9" :
				"Style index-* BorderWidth 9");
		command(exc, text);
		if (kind == 0)
			snprintf(text, sizeof(text),
				"Style (WindowID 0x%lx) HandleWidth 11, "
				"TitleFormat index-owned %%n",
				(unsigned long)FW_W(window));
		else
			snprintf(text, sizeof(text), kind == 1 ?
				"Style (Resource index-0000) HandleWidth 11, "
				"TitleFormat index-owned %%n" :
				"Style index-0000 HandleWidth 11, "
				"TitleFormat index-owned %%n");
		command(exc, text);
		for (unsigned pass = 0; pass < 4; pass++)
		{
			lookup_style(&sample, &style);
			assert(SGET_BORDER_WIDTH(style) == 9);
			assert(SGET_HANDLE_WIDTH(style) == 11);
			index_compare(&sample);
			assert(strcmp(SGET_TITLE_FORMAT_STRING(style),
				"index-owned %n") == 0);
			simplify_style_list();
		}
		if (kind == 2)
		{
			command(exc, "Style index-copied UseStyle index-0000");
			sample.name.name = "index-copied";
			lookup_style(&sample, &style);
			assert(SGET_BORDER_WIDTH(style) == 7);
			assert(SGET_HANDLE_WIDTH(style) == 11);
			index_compare(&sample);
			command(exc, "DestroyStyle index-copied");
		}
		/* One changed identity must not inherit a cached result. */
		FW_W(&sample) = Scr.NoFocusWin;
		sample.class.res_name = sample.class.res_class = "unmatched";
		sample.name.name = sample.icon_name.name = "unmatched";
		lookup_style(&sample, &style);
		assert(SGET_BORDER_WIDTH(style) == 3);
		sample = *window;
		if (kind == 0)
			snprintf(text, sizeof(text),
				"DestroyStyle (WindowID 0x%lx, Name *)",
				(unsigned long)FW_W(window));
		else
			snprintf(text, sizeof(text), kind == 1 ?
				"DestroyStyle (Resource index-*)" :
				"DestroyStyle index-*");
		command(exc, text);
		for (unsigned i = 0; i < count; i++)
		{
			if (kind == 0)
				snprintf(text, sizeof(text),
					"DestroyStyle (WindowID 0x%lx)",
					(unsigned long)FW_W(window) + i);
			else if (kind == 1)
				snprintf(text, sizeof(text),
					"DestroyStyle (%s index-%04u)",
					fields[i % 4], i);
			else
				snprintf(text, sizeof(text),
					"DestroyStyle index-%04u", i);
			command(exc, text);
		}
		lookup_style(&sample, &style);
		assert(SGET_BORDER_WIDTH(style) == 3);
	}
}

void __wrap_CaptureAllWindows(const exec_context_t *exc)
{
	const char *mode = getenv("FVWM_STYLE_PROFILE_MODE");
	const char *size = getenv("FVWM_STYLE_PROFILE_SIZE");
	const char *cohort = getenv("FVWM_STYLE_PROFILE_WINDOWS");
	unsigned count, windows;
	FvwmWindow *clients[32];
	char text[128];
	uint64_t started, elapsed;

	alarm(30);
	assert(mode && size && cohort);
	force_reference = getenv("FVWM_STYLE_PROFILE_REFERENCE") != NULL;
	force_collision = getenv("FVWM_STYLE_HASH_COLLISION") != NULL;
	force_index_collision = getenv("FVWM_STYLE_INDEX_COLLISION") != NULL;
	count = (unsigned)strtoul(size, NULL, 10);
	assert(count == 64 || count == 128 || count == 256);
	fFvwmInStartup = False;
	command(exc, "Style * NoTitle, BorderWidth 3, HandleWidth 3");
	command(exc, "UpdateStyles");
	windows = (unsigned)strtoul(cohort, NULL, 10);
	assert(windows == 1 || windows == 8 || windows == 16 || windows == 32);
	for (unsigned i = 0; i < windows; i++)
	{
		clients[i] = create_window(exc);
		check_border(clients[i], 3);
	}
	if (strcmp(mode, "index") == 0)
	{
		index_work(exc, clients[0], count);
		fflush(stdout);
		_exit(0);
	}
	if (strcmp(mode, "repeat") == 0)
	{
		repeated_work(exc, clients[0]);
		fflush(stdout);
		_exit(0);
	}
	for (unsigned i = 0; i < count; i++)
	{
		snprintf(text, sizeof(text), "Style probe-%04u BorderWidth %u",
			i, i == count - 1 ? 7 : 5);
		command(exc, text);
	}
	if (strcmp(mode, "update") == 0)
	{
		command(exc, "Style * BorderWidth 7, HandleWidth 7");
		/* Pending rules must not alter existing clients before the barrier. */
		for (unsigned i = 0; i < windows; i++)
		{
			check_border(clients[i], 3);
		}
	}
	measuring = True;
	started = now_ns();
	if (strcmp(mode, "simplify") == 0)
	{
		simplify_style_list();
	}
	else if (strcmp(mode, "lookup") == 0)
	{
		for (unsigned i = 0; i < 32; i++)
		{
			lookup_control(clients[0], count);
		}
	}
	else
	{
		assert(strcmp(mode, "update") == 0);
		command(exc, "UpdateStyles");
	}
	elapsed = now_ns() - started;
	measuring = False;
	assert(grab_depth == 0);
	if (strcmp(mode, "simplify") == 0)
	{
		assert(comparisons == 0);
		assert(overlap_checks == (force_reference ?
			(unsigned long)count * (count - 1) / 2 : 0));
		assert(!Scr.flags.do_need_style_list_update);
		lookup_control(clients[0], count);
		check_precedence(exc, clients[0]);
		check_structured_ids(exc, clients[0]);
	}
	else if (strcmp(mode, "lookup") == 0)
	{
		assert(matches == (force_reference ? 32 * (3 * count - 2) : 0));
	}
	else
	{
		assert(!Scr.flags.do_need_window_update);
		assert(grabs == 1 && grabbed_matches ==
			(force_reference ? 3 * count * windows : 0));
		assert(matches == grabbed_matches);
		for (unsigned i = 0; i < windows; i++)
		{
			check_border(clients[i], 7);
		}
		measuring = True;
		command(exc, "UpdateStyles");
		measuring = False;
		assert(grabs == 1 && matches == grabbed_matches);
	}
	printf("STYLE_PROFILE {\"reference\":%s,\"mode\":\"%s\",\"rules\":%u,"
		"\"windows\":%u,\"comparisons\":%lu,\"matches\":%lu,"
		"\"grabbed_matches\":%lu,\"grabs\":%lu,"
		"\"elapsed_ns\":%llu,\"grabbed_ns\":%llu,"
		"\"overlap_checks\":%lu}\n",
		force_reference ? "true" : "false", mode, count, windows,
		comparisons, matches, grabbed_matches,
		grabs, (unsigned long long)elapsed,
		(unsigned long long)grabbed_ns,
		overlap_checks);
	if (strcmp(mode, "lookup") == 0 && count == 64)
	{
		matching_work(exc, clients[0], count);
		if (!force_reference)
			wildcard_work(exc, clients[0]);
	}
	fflush(stdout);
	_exit(0);
}
