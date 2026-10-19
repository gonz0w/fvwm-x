/* This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */
#include "config.h"

#include <assert.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include "fvwm/fvwm.h"
#include "fvwm/execcontext.h"
#include "fvwm/externs.h"
#include "fvwm/screen.h"
#include "fvwm/ewmh.h"
#include "fvwm/ewmh_intern.h"
#include "fvwm/ewmh_client_list.h"
#include "fvwm/ewmh_atom_index.h"
#include "libs/monitor_ownership.h"

extern ewmh_atom_list atom_list[];
static Bool measuring, refuse;
static unsigned appends, allocations, publications, index_calls;
static unsigned reference_visits;
static Atom client_atom, stack_atom;
static Atom published[4];
static FvwmWindow windows[256];

static Bool checking_name, startup_counting;
static unsigned name_comparisons, deleted, startup_allocations, sort_calls;
static Atom deleted_atom;
static unsigned supported_publications;
static void atom_names(Bool reentrant);

int __real_strcmp(const char *, const char *);
int __wrap_strcmp(const char *left, const char *right)
{
	name_comparisons += checking_name;
	return __real_strcmp(left, right);
}

int __real_XDeleteProperty(Display *, Window, Atom);
int __wrap_XDeleteProperty(Display *display, Window window, Atom atom)
{
	if (checking_name)
	{
		deleted++;
		deleted_atom = atom;
		return 0;
	}
	return __real_XDeleteProperty(display, window, atom);
}

void __real_ewmh_atom_index_sort(ewmh_atom_index_entry *, size_t);
void __wrap_ewmh_atom_index_sort(ewmh_atom_index_entry *entries, size_t count)
{
	__real_ewmh_atom_index_sort(entries, count);
	if (++sort_calls % EWMH_ATOM_LIST_END == 0)
	{
		startup_counting = True;
		startup_allocations = 0;
	}
}

static unsigned atom_generation;
static ewmh_atom *reference_atom(Atom atom, ewmh_atom_list_name selected);
Status __real_XInternAtoms(Display *, char **, int, Bool, Atom *);
Atom __real_XInternAtom(Display *, const char *, Bool);

Atom __wrap_XInternAtom(Display *display, const char *name, Bool exists)
{
	if (getenv("FVWM_STATE_ATOM_MISSING") && atom_generation % 2 &&
	    strcmp(name, "_NET_WM_STATE_SHADED") == 0)
		return None;
	return __real_XInternAtom(display, name, exists);
}

Status __wrap_XInternAtoms(Display *display, char **names, int count,
	Bool exists, Atom *values)
{
	Bool ewmh = count && strcmp(names[0], "UTF8_STRING") == 0;
	Status status;

	if (ewmh)
	{
		/* The preceding index is withdrawn before interning. */
		for (unsigned list = 0; list < EWMH_ATOM_LIST_END; list++)
			assert(ewmh_GetEwmhAtomByAtom(None, list) ==
				reference_atom(None, list));
		atom_names(True);
		atom_generation++;
	}
	status = __real_XInternAtoms(display, names, count, exists, values);
	if (ewmh && getenv("FVWM_STATE_ATOM_MISSING") && atom_generation % 2)
		for (int i = 0; i < count; i++)
			if (strcmp(names[i], "_NET_WM_STATE_SHADED") == 0)
			{
				values[i] = None;
				return 0;
			}
	return status;
}

static uint64_t now_ns(void)
{
	struct timespec now;

	assert(clock_gettime(CLOCK_MONOTONIC, &now) == 0);
	return (uint64_t)now.tv_sec * 1000000000 + now.tv_nsec;
}

void *__real_malloc(size_t);
void *__real_realloc(void *, size_t);
void *__wrap_malloc(size_t bytes)
{
	startup_allocations += startup_counting;
	if (measuring)
	{
		allocations++;
		if (refuse)
			return NULL;
	}
	return __real_malloc(bytes);
}
void *__wrap_realloc(void *old, size_t bytes)
{
	startup_allocations += startup_counting;
	if (measuring)
	{
		allocations++;
		if (refuse)
			return NULL;
	}
	return __real_realloc(old, bytes);
}

bool __real_ewmh_client_list_append(ewmh_client_list *, Window);
bool __wrap_ewmh_client_list_append(ewmh_client_list *list, Window window)
{
	appends += measuring;
	return __real_ewmh_client_list_append(list, window);
}
void *__real_ewmh_atom_index_find(const ewmh_atom_index_entry *, size_t, Atom);
void *__wrap_ewmh_atom_index_find(const ewmh_atom_index_entry *entries,
	size_t count, Atom atom)
{
	index_calls += measuring;
	return __real_ewmh_atom_index_find(entries, count, atom);
}

int __real_XChangeProperty(Display *, Window, Atom, Atom, int, int,
	const unsigned char *, int);
int __wrap_XChangeProperty(Display *display, Window window, Atom property,
	Atom type, int format, int mode, const unsigned char *data, int length)
{
	if (startup_counting)
	{
		unsigned count = 0;

		startup_counting = False;
		assert(property == XInternAtom(
			display, "_NET_SUPPORTED", False));
		assert(window == Scr.Root && format == 32 && type == XA_ATOM);
		assert(mode == PropModeReplace);
		for (ewmh_atom_list *list = atom_list; list->list; list++)
			for (ewmh_atom *entry = list->list;
			     entry->name; entry++)
			{
				assert(count < (unsigned)length);
				assert(((const Atom *)data)[count++] ==
					entry->atom);
			}
		assert(count == (unsigned)length);
		assert(startup_allocations == 0);
		supported_publications++;
	}
	if (measuring && (property == client_atom || property == stack_atom))
	{
		assert(publications < 4 && format == 32 && type == XA_WINDOW);
		published[publications++] = property;
	}
	return __real_XChangeProperty(display, window, property, type, format,
		mode, data, length);
}

static void configure_windows(size_t count, size_t stacked,
	struct monitor *monitor)
{
	assert(count <= 256 && stacked <= count);
	memset(windows, 0, sizeof(windows));
	Scr.FvwmRoot.next = count ? windows : NULL;
	Scr.FvwmRoot.stack_next = stacked ? windows : &Scr.FvwmRoot;
	Scr.FvwmRoot.stack_prev =
		stacked ? windows + stacked - 1 : &Scr.FvwmRoot;
	for (size_t i = 0; i < count; i++)
	{
		FvwmWindow *w = windows + i;

		FW_W(w) = 0x100000 + i;
		w->m = monitor;
		w->next = i + 1 == count ? NULL : w + 1;
		w->prev = i ? w - 1 : &Scr.FvwmRoot;
		w->stack_next = i + 1 == stacked ? &Scr.FvwmRoot : w + 1;
		w->stack_prev = i ? w - 1 : &Scr.FvwmRoot;
		SET_STICKY_ACROSS_PAGES(w, 1);
	}
}

static void check_property(Atom property, size_t count, Bool reverse)
{
	Atom type;
	int format;
	unsigned long length, remaining;
	unsigned char *bytes = NULL;

	assert(XGetWindowProperty(dpy, Scr.Root, property, 0, 1024, False,
		XA_WINDOW, &type, &format, &length, &remaining, &bytes)
		== Success);
	assert(type == XA_WINDOW && format == 32 && remaining == 0);
	assert(length == count);
	for (size_t i = 0; i < count; i++)
		assert(((Window *)bytes)[i] ==
			FW_W(windows + (reverse ? count - i - 1 : i)));
	XFree(bytes);
}

static void reference_client_list(Bool stacking)
{
	FvwmWindow *w;
	size_t count = 0, i;
	Window *ids = NULL;

	for (w = stacking ? Scr.FvwmRoot.stack_next : Scr.FvwmRoot.next;
	     stacking ? w != &Scr.FvwmRoot : w != NULL;
	     w = stacking ? w->stack_next : w->next)
	{
		count++;
		reference_visits++;
	}
	if (count)
		ids = malloc(count * sizeof(*ids));
	assert(!count || ids != NULL);
	i = stacking ? count : 0;
	for (w = stacking ? Scr.FvwmRoot.stack_next : Scr.FvwmRoot.next;
	     stacking ? w != &Scr.FvwmRoot : w != NULL;
	     w = stacking ? w->stack_next : w->next)
	{
		ids[stacking ? --i : i++] = FW_W(w);
		reference_visits++;
	}
	ewmh_ChangeProperty(Scr.Root, stacking ? "_NET_CLIENT_LIST_STACKING" :
		"_NET_CLIENT_LIST", EWMH_ATOM_LIST_FVWM_ROOT,
		(unsigned char *)ids, (int)count);
	free(ids);
}

static void client_lists(struct monitor *monitor)
{
	const unsigned counts[] = { 0, 1, 64, 65, 128, 256 };

	for (unsigned n = 0; n < sizeof(counts) / sizeof(*counts); n++)
	{
		unsigned count = counts[n];

		configure_windows(count, count, monitor);
		for (unsigned reference = 0; reference < 2; reference++)
		{
			uint64_t start, elapsed;

			appends = allocations = 0;
			publications = reference_visits = 0;
			measuring = True;
			start = now_ns();
			if (reference)
			{
				reference_client_list(False);
				reference_client_list(True);
			}
			else
			{
				EWMH_SetClientList(monitor);
				EWMH_SetClientListStacking(monitor);
			}
			elapsed = now_ns() - start;
			measuring = False;
			assert(publications == 2 &&
			published[0] == client_atom &&
				published[1] == stack_atom);
			assert(appends == (reference ? 0 : 2 * count));
			assert(reference_visits == (reference ? 4 * count : 0));
			assert(allocations == (reference ? 2 * (count != 0) :
				count <= 64 ? 0 : count <= 128 ? 2 : 4));
			check_property(client_atom, count, False);
			check_property(stack_atom, count, True);
			printf("STATE_CLIENT {\"windows\":%u,\"reference\":%u,"
				"\"appends\":%u,\"reference_visits\":%u,"
				"\"allocations\":%u,\"publications\":%u,"
				"\"elapsed_ns\":%llu}\n",
				count, reference, appends,
				reference_visits, allocations, publications,
				(unsigned long long)elapsed);
		}
	}
	configure_windows(64, 64, monitor);
	EWMH_SetClientList(monitor);
	configure_windows(65, 63, monitor);
	refuse = measuring = True;
	publications = 0;
	EWMH_SetClientList(monitor);
	measuring = refuse = False;
	assert(publications == 0);
	check_property(client_atom, 64, False);
	EWMH_SetClientList(monitor);
	EWMH_SetClientListStacking(monitor);
	check_property(client_atom, 65, False);
	check_property(stack_atom, 63, True);
	/* External replacement and repeated calls must still be repaired. */
	XDeleteProperty(dpy, Scr.Root, client_atom);
	publications = 0;
	measuring = True;
	EWMH_SetClientList(monitor);
	EWMH_SetClientList(monitor);
	measuring = False;
	assert(publications == 2);
	check_property(client_atom, 65, False);
	puts("client-list order/empty/independent membership/refusal/repair: "
		"passed");
}

static void reference_work_area(struct monitor *monitor, Bool dynamic)
{
	ewmh_strut area = { monitor->ewmhc.BaseStrut.left,
		monitor->ewmhc.BaseStrut.right, monitor->ewmhc.BaseStrut.top,
		monitor->ewmhc.BaseStrut.bottom };
	rectangle rect = { monitor->si->x, monitor->si->y,
		monitor->si->w, monitor->si->h }, work;

	for (FvwmWindow *w = Scr.FvwmRoot.next; w != NULL; w = w->next)
	{
		const ewmh_strut *v = dynamic ? &w->dyn_strut : &w->strut;

		reference_visits++;
		if (w->m != monitor || DO_EWMH_IGNORE_STRUT_HINTS(w) ||
		    !IS_STICKY_ACROSS_PAGES(w))
			continue;
		area.left = max(area.left, v->left);
		area.right = max(area.right, v->right);
		area.top = max(area.top, v->top);
		area.bottom = max(area.bottom, v->bottom);
	}
	if (monitor_ownership_work_rect(&rect, area.left, area.right,
		area.top, area.bottom, &work) == MONITOR_OWNERSHIP_OK)
	{
		if (dynamic)
			monitor->Desktops->ewmh_dyn_working_area = work;
		else
			monitor->Desktops->ewmh_working_area = work;
	}
}

static void work_areas(struct monitor *live)
{
	struct monitor monitor = *live, other = *live;
	struct screen_info info = *live->si;
	DesktopsInfo desktop = { 0 };
	unsigned state = 0x5423109U;

	monitor.si = &info;
	monitor.Desktops = &desktop;
	info.x = -400;
	info.y = -200;
	info.w = 800;
	info.h = 600;
	for (unsigned round = 0; round < 1000; round++)
	{
		rectangle expected[2];

		configure_windows(64, 64, &monitor);
		for (unsigned i = 0; i < 64; i++)
		{
			FvwmWindow *w = windows + i;

			state = state * 1664525U + 1013904223U;
			w->m = state & 1 ? &monitor : &other;
			SET_STICKY_ACROSS_PAGES(w, state & 2);
			SET_DO_EWMH_IGNORE_STRUT_HINTS(w, state & 4);
			w->strut = (ewmh_strut){ state % 17, state % 19,
				state % 23, state % 29 };
			w->dyn_strut = (ewmh_strut){ state % 31, state % 37,
				state % 41, state % 43 };
		}
		/* Invalidate either result independently. */
		windows[0].m = &monitor;
		SET_STICKY_ACROSS_PAGES(windows, 1);
		SET_DO_EWMH_IGNORE_STRUT_HINTS(windows, 0);
		if (round % 3 == 0)
			windows[0].strut.left = 801;
		if (round % 3 == 1)
			windows[0].dyn_strut.left = 801;
		desktop.ewmh_working_area = (rectangle){ 11, 22, 33, 44 };
		desktop.ewmh_dyn_working_area = (rectangle){ 55, 66, 77, 88 };
		reference_work_area(&monitor, False);
		reference_work_area(&monitor, True);
		expected[0] = desktop.ewmh_working_area;
		expected[1] = desktop.ewmh_dyn_working_area;
		desktop.ewmh_working_area = (rectangle){ 11, 22, 33, 44 };
		desktop.ewmh_dyn_working_area = (rectangle){ 55, 66, 77, 88 };
		EWMH_UpdateWorkArea(&monitor);
		assert(memcmp(expected, &desktop.ewmh_working_area,
			sizeof(rectangle)) == 0);
		assert(memcmp(expected + 1, &desktop.ewmh_dyn_working_area,
			sizeof(rectangle)) == 0);
		desktop.ewmh_dyn_working_area = (rectangle){ 1, 2, 3, 4 };
		ewmh_ComputeAndSetWorkArea(&monitor);
		assert(desktop.ewmh_dyn_working_area.x == 1 &&
			desktop.ewmh_dyn_working_area.width == 3);
	}
	for (unsigned count = 64; count <= 256; count *= 2)
	{
		configure_windows(count, count, &monitor);
		for (unsigned reference = 0; reference < 2; reference++)
		{
			uint64_t start = now_ns(), elapsed;

			reference_visits = 0;
			for (unsigned i = 0; i < 64; i++)
				if (reference)
				{
					reference_work_area(&monitor, False);
					reference_work_area(&monitor, True);
				}
				else
					EWMH_UpdateWorkArea(&monitor);
			elapsed = now_ns() - start;
			assert(reference_visits ==
				(reference ? 128 * count : 0));
			printf("STATE_WORK {\"windows\":%u,\"reference\":%u,"
				"\"updates\":64,\"reference_visits\":%u,"
				"\"elapsed_ns\":%llu}\n", count, reference,
				reference_visits, (unsigned long long)elapsed);
		}
	}
	configure_windows(1, 1, &monitor);
	windows[0].strut.left = 20;
	windows[0].dyn_strut.left = 40;
	{
		int x = 777, y = 888, width = 1, height = 2;

		EWMH_GetWorkAreaIntersection(&monitor, &x, &y, &width, &height,
			EWMH_IGNORE_WORKING_AREA);
		assert(x == 777 && y == 888 && width == 1 && height == 2);
		assert(desktop.ewmh_working_area.x == info.x + 20);
		assert(desktop.ewmh_dyn_working_area.x == info.x + 40);
	}
	configure_windows(0, 0, &monitor);
	EWMH_UpdateWorkArea(&monitor);
	assert(desktop.ewmh_working_area.width == 800 &&
		desktop.ewmh_dyn_working_area.width == 800);
	puts("work-area paired differential/filter/invalid/single-mode: "
		"passed");
}

static ewmh_atom *reference_atom(Atom atom, ewmh_atom_list_name selected)
{
	for (ewmh_atom_list *list = atom_list; list->list != NULL; list++)
	{
		if (selected != EWMH_ATOM_LIST_ALL && selected != list->name)
			continue;
		for (ewmh_atom *entry = list->list;
		     entry->name != NULL; entry++)
		{
			reference_visits++;
			if (entry->atom == atom)
				return entry;
		}
	}
	return NULL;
}

static ewmh_atom *reference_name(const char *name,
	ewmh_atom_list_name selected)
{
	for (ewmh_atom_list *list = atom_list; list->list; list++)
	{
		if (selected != EWMH_ATOM_LIST_ALL && selected != list->name)
			continue;
		for (ewmh_atom *entry = list->list;
			     entry->name; entry++)
			if (strcmp(name, entry->name) == 0)
				return entry;
	}
	return NULL;
}

static int reference_compare_name(const void *key, const void *entry)
{
	/* Explicit counted call: strcmp purity can hoist reference work out
	 * of the measurement window even when its return value is used. */
	return __wrap_strcmp(key, ((const ewmh_atom *)entry)->name);
}

static ewmh_atom *legacy_name(const char *name,
	ewmh_atom_list_name selected)
{
	for (ewmh_atom_list *list = atom_list; list->list; list++)
	{
		ewmh_atom *entry;

		if (selected != EWMH_ATOM_LIST_ALL && selected != list->name)
			continue;
		entry = bsearch(name, list->list, list->size - 1,
			sizeof(*entry), reference_compare_name);
		if (entry)
			return entry;
	}
	return NULL;
}

static void check_name(const char *name, ewmh_atom_list_name selected)
{
	ewmh_atom *expected = reference_name(name, selected);

	deleted = 0;
	checking_name = True;
	ewmh_DeleteProperty(Scr.NoFocusWin, name, selected);
	checking_name = False;
	assert(deleted == (expected != NULL));
	if (expected)
		assert(deleted_atom == expected->atom);
}

static void atom_names(Bool reentrant)
{
	unsigned queries = 0, legacy_misses = 0;

	for (unsigned selected = 0; selected <= EWMH_ATOM_LIST_END;
	     selected++)
	{
		for (ewmh_atom_list *list = atom_list; list->list; list++)
			for (ewmh_atom *entry = list->list;
			     entry->name; entry++)
			{
				check_name(entry->name, selected);
				queries++;
				legacy_misses += reference_name(entry->name,
					selected) != legacy_name(
					entry->name, selected);
			}
		check_name("", selected);
		check_name("_FVWM_STATE_NAME_MISSING", selected);
		check_name("\377", selected);
		check_name("_NET_WM_WINDOW_TYPE_NOTIFICATIONx", selected);
	}
	check_name("_NET_WM_STATE", (ewmh_atom_list_name)-1);
	if (!reentrant)
	{
		const char *keys[] = { "_NET_WORKAREA", "_NET_WM_PID",
			"_FVWM_STATE_NAME_MISSING", "_NET_CURRENT_DESKTOP" };
		unsigned capacity = 0;

		for (unsigned key = 0; key < 4; key++)
		{
			unsigned indexed, legacy;
			ewmh_atom *expected = reference_name(keys[key],
				EWMH_ATOM_LIST_ALL);

			name_comparisons = 0;
			for (unsigned i = 0; i < 64; i++)
				check_name(keys[key], EWMH_ATOM_LIST_ALL);
			indexed = name_comparisons;
			name_comparisons = 0;
			checking_name = True;
			for (unsigned i = 0; i < 64; i++)
				assert(legacy_name(keys[key],
					EWMH_ATOM_LIST_ALL)
					== expected);
			checking_name = False;
			legacy = name_comparisons;
			printf("STATE_NAME {\"key\":%u,\"indexed\":%u,"
				"\"legacy\":%u}\n", key, indexed, legacy);
			fflush(stdout);
			if (key < 3)
				assert(indexed < legacy);
		}

		for (ewmh_atom_list *list = atom_list; list->list; list++)
			capacity += list->size;
		printf("STATE_NAME_COVERAGE {\"queries\":%u,"
			"\"legacy_misses\":%u,\"metadata_bytes\":%zu}\n",
			queries, legacy_misses,
			2 * capacity * sizeof(ewmh_atom_name_entry));
		puts("atom names scoped/all/duplicate/unsorted/reentrant: "
			"passed");
	}
}

static int compare_duplicate(const void *key, const void *value)
{
	Atom atom = *(const Atom *)key;
	const ewmh_atom_index_entry *entry = value;

	return (atom > entry->atom) - (atom < entry->atom);
}

static void duplicate_atoms(void)
{
	for (size_t count = 128; count <= 65536; count *= 16)
	{
		ewmh_atom_index_entry *entries =
			malloc(count * sizeof(*entries));
		Atom atom = None;
		unsigned bound = 1;
		uint64_t start, indexed, legacy;
		size_t visits = 0;

		assert(entries);
		for (size_t n = count; n; n /= 2)
			bound++;
		for (size_t i = 0; i < count; i++)
			entries[i] = (ewmh_atom_index_entry){
				atom, i, entries + i };
		start = now_ns();
		for (unsigned i = 0; i < 64; i++)
			assert(ewmh_atom_index_find(entries, count, atom) ==
				entries);
		indexed = now_ns() - start;
		start = now_ns();
		for (unsigned i = 0; i < 64; i++)
		{
			ewmh_atom_index_entry *entry = bsearch(&atom, entries,
				count, sizeof(*entries), compare_duplicate);

			assert(entry);
			while (entry != entries && entry[-1].atom == atom)
			{
				entry--;
				visits++;
			}
			assert(entry->value == entries);
		}
		legacy = now_ns() - start;
		printf("STATE_DUPLICATE {\"count\":%zu,\"repeats\":64,"
			"\"legacy_rewind_visits\":%zu,\"comparison_bound\":%u,"
			"\"indexed_ns\":%llu,\"legacy_ns\":%llu}\n", count,
			visits, bound, (unsigned long long)indexed,
			(unsigned long long)legacy);
		free(entries);
	}
	puts("atom duplicate lower-bound priority: passed");
}

static void check_supported(void)
{
	Atom type, property = XInternAtom(dpy, "_NET_SUPPORTED", False);
	int format;
	unsigned long length, remaining;
	unsigned char *bytes = NULL;
	Atom *reference;
	unsigned count = 0;

	assert(XGetWindowProperty(dpy, Scr.Root, property, 0, 1024, False,
		XA_ATOM, &type, &format, &length, &remaining, &bytes)
		== Success);
	assert(type == XA_ATOM && format == 32 && !remaining && length);
	/* Execute the preceding publisher's allocation/copy with its supplied
	 * count; compare the resulting vector against the server's property. */
	allocations = 0;
	measuring = True;
	reference = __wrap_malloc(length * sizeof(*reference));
	measuring = False;
	assert(reference && allocations == 1);
	for (ewmh_atom_list *list = atom_list; list->list; list++)
		for (ewmh_atom *entry = list->list;
			     entry->name; entry++)
		{
			assert(count < length);
			reference[count++] = entry->atom;
		}
	assert(count == length);
	assert(memcmp(reference, bytes, length * sizeof(*reference)) == 0);
	printf("STATE_SUPPORTED {\"generation\":%u,\"atoms\":%u,"
		"\"allocations\":%u,\"reference_allocations\":%u,"
		"\"reference_copy_visits\":%u}\n", atom_generation, count,
		startup_allocations, allocations, count);
	free(reference);
	XFree(bytes);
}

static void name_properties(void)
{
	Window window = XCreateSimpleWindow(dpy, Scr.Root, 0, 0, 1, 1,
		0, 0, 0);
	unsigned long value = 1234;
	const char text[] = "probe";
	int size = -1;
	void *bytes;

	ewmh_ChangeProperty(window, "_NET_WM_PID", EWMH_ATOM_LIST_ALL,
		(unsigned char *)&value, 1);
	bytes = ewmh_AtomGetByName(window, "_NET_WM_PID",
		EWMH_ATOM_LIST_FIXED_PROPERTY, &size);
	assert(bytes && size == 4 && *(uint32_t *)bytes == value);
	free(bytes);
	ewmh_DeleteProperty(window, "_NET_WM_PID", EWMH_ATOM_LIST_ALL);
	bytes = ewmh_AtomGetByName(window, "_NET_WM_PID",
		EWMH_ATOM_LIST_FIXED_PROPERTY, &size);
	assert(bytes == NULL && size == 0);
	ewmh_ChangeProperty(window, "_NET_WM_NAME",
		EWMH_ATOM_LIST_PROPERTY_NOTIFY, (unsigned char *)text, 5);
	bytes = ewmh_AtomGetByName(window, "_NET_WM_NAME",
		EWMH_ATOM_LIST_ALL, &size);
	assert(bytes && size == 5 && memcmp(bytes, text, 5) == 0);
	free(bytes);
	bytes = ewmh_AtomGetByName(window, "_NET_WM_NAME",
		EWMH_ATOM_LIST_END, &size);
	assert(bytes == NULL && size == 0);
	XDestroyWindow(dpy, window);
	puts("atom named property cardinal/UTF8/read/delete/invalid: passed");
}

static void atoms(struct monitor *monitor)
{
	Atom missing = XInternAtom(dpy, "_FVWM_STATE_PROBE_MISSING", False);

	for (unsigned pass = 0; pass < 2; pass++)
	{
		check_supported();
		for (unsigned selected = 0; selected < EWMH_ATOM_LIST_END + 1;
		     selected++)
		{
			for (ewmh_atom_list *list = atom_list;
			     list->list; list++)
			{
				for (ewmh_atom *entry = list->list;
				     entry->name; entry++)
				{
					ewmh_atom *expected = reference_atom(
						entry->atom, selected);

					assert(ewmh_GetEwmhAtomByAtom(
						entry->atom, selected) ==
						expected);
				}
			}
			assert(ewmh_GetEwmhAtomByAtom(None, selected) ==
				reference_atom(None, selected));
			assert(ewmh_GetEwmhAtomByAtom(missing, selected) ==
				NULL);
		}
		index_calls = reference_visits = 0;
		measuring = True;
		for (unsigned i = 0; i < 64; i++)
			assert(ewmh_GetEwmhAtomByAtom(missing,
				EWMH_ATOM_LIST_ALL) == NULL);
		measuring = False;
		for (unsigned i = 0; i < 64; i++)
			assert(reference_atom(missing, EWMH_ATOM_LIST_ALL) ==
				NULL);
		assert(index_calls == 64);
		printf("STATE_ATOM {\"pass\":%u,\"indexed_calls\":%u,"
			"\"reference_entry_visits\":%u}\n", pass, index_calls,
			reference_visits);
		/* Rebuild the same owner, including duplicated names. */
		EWMH_Init(monitor);
	}
	check_supported();
	puts("atom scoped/all priority/miss/None/reinitialization: passed");
}

void __wrap_CaptureAllWindows(const exec_context_t *exc)
{
	struct monitor *monitor = monitor_get_current();

	assert(monitor && monitor->Desktops && Scr.FvwmRoot.next == NULL);
	client_atom = XInternAtom(dpy, "_NET_CLIENT_LIST", False);
	stack_atom = XInternAtom(dpy, "_NET_CLIENT_LIST_STACKING", False);
	client_lists(monitor);
	work_areas(monitor);
	configure_windows(1, 1, monitor);
	windows[0].strut.left = 7;
	windows[0].dyn_strut.left = 11;
	monitor->ewmhc.NeedsToCheckDesk = False;
	publications = 0;
	measuring = True;
	EWMH_WindowDestroyed();
	measuring = False;
	assert(publications == 2 &&
			published[0] == client_atom &&
		published[1] == stack_atom);
	assert(monitor->Desktops->ewmh_working_area.x == monitor->si->x + 7);
	assert(monitor->Desktops->ewmh_dyn_working_area.x ==
		monitor->si->x + 11);
	puts("destruction publication and query refresh barriers: passed");
	configure_windows(0, 0, monitor);
	name_properties();
	atom_names(False);
	duplicate_atoms();
	atoms(monitor);
	assert(supported_publications == 3);
	puts("ewmh state publication: passed");
	fflush(stdout);
	_exit(0);
}
