/* This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */
#include "config.h"
#include <errno.h>
#include <float.h>
#include <inttypes.h>
#include <limits.h>
#include <math.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/resource.h>
#include <time.h>
#ifdef SERIALIZER_YYJSON
#include "yyjson.h"
#if YYJSON_VERSION_HEX != 0x000D00
#error The comparison is frozen to yyjson 0.13.0
#endif
#define BACKEND "yyjson-0.13.0"
#else
#include "libs/cJSON.h"
#define BACKEND "patched-cJSON-1.7.19"
#endif

/* Synthetic, non-installed comparison only. No adapter enters production.
 * Native timing and tracked trials are distinct: custom cJSON hooks disable
 * its realloc fast path. Requested-byte high-water is not allocator RSS. */
struct allocation_stats {
	size_t attempts, fail_at, budget, live, peak, blocks, requested;
	bool invariant_failure;
};
union allocation_header {
	max_align_t alignment;
	struct { size_t bytes; } value;
};
static struct allocation_stats *active_stats;

static void *counted_allocate(size_t bytes)
{
	struct allocation_stats *s = active_stats;
	union allocation_header *h;

	if (s->attempts++ == s->fail_at || bytes > s->budget - s->live ||
		bytes > SIZE_MAX - sizeof(*h) ||
		bytes > SIZE_MAX - s->requested)
		return NULL;
	h = malloc(sizeof(*h) + bytes);
	if (!h)
		return NULL;
	h->value.bytes = bytes;
	s->requested += bytes;
	s->live += bytes;
	s->blocks++;
	if (s->live > s->peak)
		s->peak = s->live;
	return h + 1;
}

static void counted_free(void *pointer)
{
	union allocation_header *h;
	struct allocation_stats *s = active_stats;

	if (!pointer)
		return;
	h = (union allocation_header *)pointer - 1;
	if (!s->blocks || h->value.bytes > s->live)
	{
		s->invariant_failure = true;
		return;
	}
	s->live -= h->value.bytes;
	s->blocks--;
	free(h);
}

#ifdef SERIALIZER_YYJSON
static void *candidate_allocate(void *context, size_t bytes)
{
	(void)context;
	return counted_allocate(bytes);
}

static void candidate_free(void *context, void *pointer)
{
	(void)context;
	counted_free(pointer);
}

static void *candidate_resize(void *context, void *pointer, size_t old_size,
	size_t bytes)
{
	union allocation_header *h;
	void *result;

	(void)context;
	if (!pointer)
		return counted_allocate(bytes);
	h = (union allocation_header *)pointer - 1;
	if (h->value.bytes != old_size)
	{
		active_stats->invariant_failure = true;
		return NULL;
	}
	if (!bytes)
	{
		counted_free(pointer);
		return NULL;
	}
	/* Deliberately count overlapping old/new storage and the copy. This
	 * instrumentation is not yyjson's native realloc timing behavior. */
	result = counted_allocate(bytes);
	if (result)
	{
		memcpy(result, pointer, old_size < bytes ? old_size : bytes);
		counted_free(pointer);
	}
	return result;
}
#endif

struct comparison {
	bool native, borrowed, mutate;
#ifdef SERIALIZER_YYJSON
	yyjson_mut_doc *document;
#endif
};
#ifdef SERIALIZER_YYJSON
typedef yyjson_mut_val value;
#else
typedef cJSON value;
#endif

static value *object(struct comparison *c)
{
#ifdef SERIALIZER_YYJSON
	return yyjson_mut_obj(c->document);
#else
	(void)c;
	return cJSON_CreateObject();
#endif
}

static value *array(struct comparison *c)
{
#ifdef SERIALIZER_YYJSON
	return yyjson_mut_arr(c->document);
#else
	(void)c;
	return cJSON_CreateArray();
#endif
}

static value *text(struct comparison *c, const char *string)
{
#ifdef SERIALIZER_YYJSON
	return c->borrowed ? yyjson_mut_str(c->document, string) :
		yyjson_mut_strcpy(c->document, string);
#else
	(void)c;
	return cJSON_CreateString(string);
#endif
}

static value *number(struct comparison *c, double n)
{
#ifdef SERIALIZER_YYJSON
	return yyjson_mut_real(c->document, n);
#else
	(void)c;
	return cJSON_CreateNumber(n);
#endif
}

static bool attach(struct comparison *c, value *parent, const char *key,
	value *child)
{
#ifdef SERIALIZER_YYJSON
	value *name;

	if (!parent || !child)
		return false;
	if (!key)
		return yyjson_mut_arr_append(parent, child);
	name = text(c, key);
	return name && yyjson_mut_obj_add(parent, name, child);
#else
	(void)c;
	if (parent && child && (key ? cJSON_AddItemToObject(parent, key,
		child) : cJSON_AddItemToArray(parent, child)))
		return true;
	cJSON_Delete(child);
	return false;
#endif
}

static value *build_record(struct comparison *c, const char *title)
{
	value *root = object(c), *content = object(c);

	/* Attach before population so all partial cJSON nodes have one owner.
	 * yyjson nodes belong to its document even before attachment. */
	if (!attach(c, root, "window_name", content) ||
		!attach(c, content, "window", text(c, "0x1234")) ||
		!attach(c, content, "name", text(c, title)))
	{
#ifndef SERIALIZER_YYJSON
		cJSON_Delete(root);
#endif
		return NULL;
	}
	return root;
}

static value *build(struct comparison *c, const char *name,
	const char *workspace)
{
	value *root, *child;

	if (!strcmp(name, "ordinary"))
		return build_record(c, "quote\"\\\n\t\001 caf\303\251");
	if (!strcmp(name, "legacy"))
		return build_record(c, "\377cut\342\202");
	if (!strcmp(name, "unicode"))
		return build_record(c, "caf\303\251 \360\237\230\200");
	if (!strcmp(name, "legacy-keys"))
	{
		root = object(c);
		if (!attach(c, root, "\377", number(c, 1)) ||
			!attach(c, root, "\376", number(c, 2)))
			goto fail;
		return root;
	}
	if (!strcmp(name, "numbers"))
	{
		const double values[] = {0.0, -0.0, INT_MIN, INT_MAX, 0.25,
			DBL_MIN, DBL_MAX, -DBL_MAX, INFINITY, NAN};

		root = array(c);
		if (!root)
			return NULL;
		for (size_t i = 0; i < sizeof(values) / sizeof(*values); i++)
			if (!attach(c, root, NULL, number(c, values[i])))
				goto fail;
#ifdef SERIALIZER_YYJSON
		child = yyjson_mut_uint(c->document, UINT64_MAX);
#else
		child = cJSON_CreateRaw("18446744073709551615");
#endif
		if (!attach(c, root, NULL, child))
			goto fail;
		return root;
	}
	if (!strcmp(name, "large") || !strcmp(name, "queue-below") ||
		!strcmp(name, "queue-at") || !strcmp(name, "status-below") ||
		!strcmp(name, "status-at"))
		return text(c, workspace);
	root = array(c);
	if (!root)
		return NULL;
	for (unsigned int i = 0; i < 256; i++)
		if (!attach(c, root, NULL, build_record(c, workspace)))
			goto fail;
	return root;
fail:
#ifndef SERIALIZER_YYJSON
	cJSON_Delete(root);
#endif
	return NULL;
}

static char *serialize(struct comparison *c, const char *name,
	char *workspace, const char **outcome, size_t *length)
{
	value *root;
	char *encoded = NULL;
#ifdef SERIALIZER_YYJSON
	yyjson_alc allocator = {candidate_allocate, candidate_resize,
		candidate_free, NULL};

	c->document = yyjson_mut_doc_new(c->native ? NULL : &allocator);
	if (!c->document)
		return NULL;
#endif
	root = build(c, name, workspace);
	if (root)
	{
		if (c->mutate)
			workspace[0] = 'y';
		*outcome = "write-refused";
#ifdef SERIALIZER_YYJSON
		yyjson_mut_doc_set_root(c->document, root);
		encoded = yyjson_mut_write_opts(c->document,
			YYJSON_WRITE_INF_AND_NAN_AS_NULL |
			YYJSON_WRITE_LOWERCASE_HEX,
			c->native ? NULL : &allocator, length, NULL);
#else
		encoded = cJSON_PrintUnformatted(root);
		if (encoded)
			*length = strlen(encoded);
#endif
		if (c->mutate)
			workspace[0] = 'x';
	}
#ifdef SERIALIZER_YYJSON
	yyjson_mut_doc_free(c->document);
	c->document = NULL;
#else
	cJSON_Delete(root);
#endif
	if (encoded)
		*outcome = "complete";
	return encoded;
}

static bool integer(const char *text, size_t max, size_t *value)
{
	char *end;
	uintmax_t parsed;

	if (!text[0])
		return false;
	for (const char *p = text; *p; p++)
		if (*p < '0' || *p > '9')
			return false;
	errno = 0;
	parsed = strtoumax(text, &end, 10);
	if (errno || *end || parsed > max)
		return false;
	*value = (size_t)parsed;
	return true;
}

static uint64_t clock_ns(clockid_t clock)
{
	struct timespec now;

	if (clock_gettime(clock, &now) < 0 || now.tv_sec < 0 ||
		now.tv_nsec < 0 || now.tv_nsec >= 1000000000L ||
		(uint64_t)now.tv_sec >
		(UINT64_MAX - (uint64_t)now.tv_nsec) / 1000000000ULL)
		return 0;
	return (uint64_t)now.tv_sec * 1000000000ULL + now.tv_nsec;
}

int main(int argc, char **argv)
{
	const char *cases[] = {"ordinary", "legacy", "legacy-keys", "unicode",
		"numbers",
		"large", "stress", "queue-below", "queue-at", "status-below",
		"status-at"};
	struct allocation_stats stats = {.fail_at = SIZE_MAX,
		.budget = 16U * 1024U * 1024U};
	struct comparison comparison = {0};
	const char *outcome = "construct-refused";
	size_t repeat = 1, fail_at = SIZE_MAX, workspace_bytes = 257;
	size_t length = 0, complete = 0;
	char *workspace;
	char allocation_fields[256];
	struct rusage usage = {0};
	bool usage_available;
	bool emit;
	uint64_t start, cpu, elapsed, cpu_elapsed;

	if (argc < 3 || (strcmp(argv[1], "emit") &&
		strcmp(argv[1], "measure")))
		return 2;
	emit = !strcmp(argv[1], "emit");
	unsigned int selected;

	for (selected = 0; selected < sizeof(cases) / sizeof(*cases); selected++)
		if (!strcmp(argv[2], cases[selected]))
			break;
	if (selected == sizeof(cases) / sizeof(*cases))
		return 2;
	for (int i = 3; i < argc; i++)
	{
		if (!strcmp(argv[i], "--native"))
			comparison.native = true;
		else if (!strcmp(argv[i], "--borrowed"))
			comparison.borrowed = true;
		else if (!strcmp(argv[i], "--mutate-input"))
			comparison.mutate = true;
		else if (i + 1 < argc && !strcmp(argv[i], "--repeat"))
		{
			if (!integer(argv[++i], 1000, &repeat) || !repeat)
				return 2;
		}
		else if (i + 1 < argc && !strcmp(argv[i], "--fail-at"))
		{
			if (!integer(argv[++i], 100000, &fail_at))
				return 2;
		}
		else if (i + 1 < argc && !strcmp(argv[i], "--budget"))
		{
			if (!integer(argv[++i], 16U * 1024U * 1024U,
				&stats.budget) || !stats.budget)
				return 2;
		}
		else
			return 2;
	}
#ifndef SERIALIZER_YYJSON
	if (comparison.borrowed)
		return 2;
#endif
	if (emit && repeat != 1)
		return 2;
	if (comparison.mutate && strcmp(argv[2], "large") &&
		strcmp(argv[2], "stress"))
		return 2;
	if (comparison.native && (fail_at != SIZE_MAX ||
		stats.budget != 16U * 1024U * 1024U))
		return 2;
	if (!strcmp(argv[2], "large"))
		workspace_bytes = 65537;
	if (!strncmp(argv[2], "queue-", 6))
		workspace_bytes = 256U * 1024U -
			(!strcmp(argv[2], "queue-below") ? 2 : 1);
	if (!strncmp(argv[2], "status-", 7))
		workspace_bytes = 1024U * 1024U -
			(!strcmp(argv[2], "status-below") ? 2 : 1);
	workspace = malloc(workspace_bytes);
	if (!workspace)
		return 1;
	memset(workspace, 'x', workspace_bytes - 1);
	workspace[workspace_bytes - 1] = '\0';
	active_stats = &stats;
#ifndef SERIALIZER_YYJSON
	cJSON_Hooks hooks = {counted_allocate, counted_free};

	cJSON_InitHooks(comparison.native ? NULL : &hooks);
#endif
	start = clock_ns(CLOCK_MONOTONIC);
	cpu = clock_ns(CLOCK_PROCESS_CPUTIME_ID);
	for (size_t i = 0; i < repeat; i++)
	{
		char *encoded;

		/* Inject the same allocation ordinal independently in each trial. */
		stats.attempts = 0;
		stats.fail_at = fail_at;
		outcome = "construct-refused";
		length = 0;
		encoded = serialize(&comparison, argv[2], workspace, &outcome,
			&length);
		if (encoded)
		{
			complete++;
			if (emit && (fwrite(encoded, 1, length, stdout) != length ||
				putchar('\n') == EOF))
				stats.invariant_failure = true;
			if (comparison.native)
				free(encoded);
			else
				counted_free(encoded);
		}
		if (stats.live || stats.blocks || stats.invariant_failure)
			break;
	}
	elapsed = clock_ns(CLOCK_MONOTONIC);
	cpu_elapsed = clock_ns(CLOCK_PROCESS_CPUTIME_ID);
	free(workspace);
#ifndef SERIALIZER_YYJSON
	cJSON_InitHooks(NULL);
#endif
	active_stats = NULL;
	if (!start || !cpu || elapsed < start || cpu_elapsed < cpu ||
		stats.live || stats.blocks || stats.invariant_failure)
		return 1;
	if (emit)
		return complete == 1 ? 0 : 3;
	int fields = comparison.native ? snprintf(allocation_fields,
		sizeof(allocation_fields), "\"attempts_last\":null,"
		"\"requested_total\":null,\"requested_peak\":null,") :
		snprintf(allocation_fields, sizeof(allocation_fields),
		"\"attempts_last\":%zu,\"requested_total\":%zu,"
		"\"requested_peak\":%zu,", stats.attempts, stats.requested,
		stats.peak);
	if (fields < 0 || (size_t)fields >= sizeof(allocation_fields))
		return 1;
	usage_available = getrusage(RUSAGE_SELF, &usage) == 0;
	/* Transport caps include a newline. These are empty-queue models, not
	 * production transport execution or construction-allocation bounds. */
	int written = printf("{\"backend\":\"%s\",\"allocator\":\"%s\","
		"\"strings\":\"%s\",\"outcome\":\"%s\","
		"\"repeat\":%zu,\"complete\":%zu,\"encoded_bytes\":%zu,"
		"%s\"live_final\":%zu,"
		"\"workspace_bytes\":%zu,\"elapsed_ns\":%" PRIu64 ","
		"\"cpu_ns\":%" PRIu64 ",\"mfl_empty_admitted\":%s,"
		"\"mfl_128_queued_admitted\":%s,"
		"\"status_empty_admitted\":%s,\"rusage_available\":%s,"
		"\"rusage_maxrss_raw\":%ld}\n", BACKEND,
		comparison.native ? "native" : "tracked-copy-realloc",
		comparison.borrowed ? "caller-borrowed" : "copied", outcome,
		repeat, complete, length, allocation_fields,
		stats.live, workspace_bytes, elapsed - start,
		cpu_elapsed - cpu, complete && length < 256U * 1024U ?
			"true" : "false",
		complete && length < 256U * 1024U - 128 ?
			"true" : "false",
		complete && length < 1024U * 1024U ? "true" : "false",
		usage_available ? "true" : "false",
		usage_available ? usage.ru_maxrss : -1L) < 0 ?
		1 : 0;
	return written || fflush(stdout) == EOF ? 1 : 0;
}
