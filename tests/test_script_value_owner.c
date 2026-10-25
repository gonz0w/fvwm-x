/* This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include "config.h"
#include "modules/FvwmScript/types.h"
#include "modules/FvwmScript/script.h"
#include "modules/FvwmScript/script_value_table.h"
#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern char **TabNVar, **TabVVar;
extern int NbVar;
extern FILE *yyin;
extern int yyparse(void);
void __real_free(void *pointer);

/* The wrapper has no compiler builtin/leaf free contract. Exact live
 * addresses permit allocator reuse once a watched allocation is released. */
enum owner_kind
{
	OWNER_TOKEN, OWNER_NAME, OWNER_VALUE, OWNER_ARRAY, OWNER_KINDS
};
static struct { void *pointer; enum owner_kind kind; } volatile owners[512];
static volatile size_t outstanding;
static volatile size_t allocated[OWNER_KINDS], released[OWNER_KINDS];

static bool watched(void *pointer)
{
	for (size_t i = 0; i < outstanding; i++)
		if (owners[i].pointer == pointer)
			return true;
	return false;
}

static void watch(void *pointer, enum owner_kind kind)
{
	assert(pointer != NULL && !watched(pointer));
	assert(outstanding < sizeof(owners) / sizeof(owners[0]));
	owners[outstanding].pointer = pointer;
	owners[outstanding++].kind = kind;
	allocated[kind]++;
}

void __wrap_free(void *pointer)
{
	for (size_t i = 0; i < outstanding; i++)
	{
		if (owners[i].pointer != pointer)
			continue;
		released[owners[i].kind]++;
		owners[i].pointer = owners[--outstanding].pointer;
		owners[i].kind = owners[outstanding].kind;
		break;
	}
	__real_free(pointer);
}

static void watch_table(char **names, char **values, size_t count)
{
	if (names != NULL)
		watch(names, OWNER_ARRAY);
	if (values != NULL)
		watch(values, OWNER_ARRAY);
	for (size_t i = 0; i < count; i++)
	{
		watch(names[i], OWNER_NAME);
		watch(values[i], OWNER_VALUE);
	}
}

#ifndef SCRIPT_VALUE_MODULE_PROBE
char *ScriptName = "value-owner-test";
int __real_yylex(void);
static size_t token_kinds[3];

int __wrap_yylex(void)
{
	int token = __real_yylex();

	/* This fixture contains only argument strings: no header/widget tokens
	 * that transfer ownership through other grammar actions. */
	if (token == VAR || token == STR || token == GSTR)
	{
		watch(yylval.str, OWNER_TOKEN);
		token_kinds[token == VAR ? 0 : token == STR ? 1 : 2]++;
	}
	return token;
}

static void parsed_case(bool baseline)
{
	script_storage *storage = script_storage_runtime();

	yyin = tmpfile();
	assert(yyin != NULL);
	fputs("Init\nBegin\nSet $new=plain\nSet $new={brace words}\n"
		"Set $other={}\nSet $new=$new\nEnd\n", yyin);
	rewind(yyin);
	assert(yyparse() == 0);
	assert(fclose(yyin) == 0);
	assert(token_kinds[0] == 5 && token_kinds[1] == 1 &&
		token_kinds[2] == 2 && allocated[OWNER_TOKEN] == 8);
	assert(released[OWNER_TOKEN] == (baseline ? 0 : 8));
	assert(NbVar == 4);
	assert(strcmp(TabNVar[0], "new") == 0 &&
		strcmp(TabNVar[3], "other") == 0);
	assert(strcmp(TabVVar[1], "plain") == 0 &&
		strcmp(TabVVar[2], "brace words") == 0);
	assert(TabVVar[0][0] == '\0' && TabVVar[3][0] == '\0' &&
		TabVVar[4][0] == '\0' && TabNVar[4][0] == '\0');
	if (baseline)
	{
		printf("old actual parser: %zu argument tokens retained "
			"(new/repeated variables, STR, GSTR, empty literal)\n",
			outstanding);
		assert(outstanding == 8);
		/* Observe before reclaiming tokens; keep the stored copies. */
		while (outstanding != 0)
			free(owners[0].pointer);
	}
	watch_table(TabNVar, TabVVar, (size_t)(NbVar + 1));
	script_value_table table = {TabNVar, TabVVar,
		(size_t)(NbVar + 1), storage, true};
	size_t used = storage->used;

	/* The owner freezes the completed extent. Runtime replacements mutate
	 * these slots; an original-value snapshot would miss the new owner. */
	NbVar = 999;
	assert(!script_value_table_clear(&table, 1));
	assert(storage->used == used && outstanding == 12);
	assert(script_storage_replace(storage, strlen(TabVVar[0]) + 1,
		strlen("replacement")) == SCRIPT_STORAGE_OK);
	free(TabVVar[0]);
	TabVVar[0] = strdup("replacement");
	watch(TabVVar[0], OWNER_VALUE);
	assert(!script_value_table_clear(&table, 3));
	assert(strcmp(TabVVar[0], "replacement") == 0);
	assert(script_value_table_clear(&table, 0));
	assert(outstanding == 0 &&
		allocated[OWNER_NAME] == released[OWNER_NAME] &&
		allocated[OWNER_VALUE] == released[OWNER_VALUE] &&
		allocated[OWNER_ARRAY] == released[OWNER_ARRAY]);
	assert(storage->used == 0 && !table.published && table.count == 0 &&
		table.names == NULL && table.values == NULL &&
		table.storage == NULL);
	assert(script_value_table_clear(&table, 0));
}

static void empty_case(void)
{
	script_storage storage = {64, 128, 7};
	script_value_table unpublished = {.storage = &storage};
	script_value_table empty = {.storage = &storage, .published = true};

	assert(script_value_table_clear(&unpublished, 0));
	assert(storage.used == 7);
	assert(!script_value_table_clear(&empty, 1));
	assert(storage.used == 7 && empty.published);
	assert(script_value_table_clear(&empty, 0));
	assert(storage.used == 0 && !empty.published);
	assert(script_value_table_clear(&empty, 0));
}

int main(int argc, char **argv)
{
	bool baseline = argc == 2 && strcmp(argv[1], "--baseline") == 0;

	assert(argc == 1 || baseline);
	empty_case();
	parsed_case(baseline);
	puts("actual parser tokens and complete value ownership passed");
	return 0;
}
#else
/* Link real module objects with --wrap=main, --wrap=yyparse,
 * --wrap=script_value_table_clear and --wrap=free. */
int __real_main(int argc, char **argv);
int __real_yyparse(void);
bool __real_script_value_table_clear(script_value_table *, unsigned int);
static const char *scenario;
static size_t complete_count;
static unsigned int clears;
static bool parsed;

static void capture_current_values(void)
{
	size_t used = 0;

	for (size_t i = 0; i < complete_count; i++)
	{
		assert(TabNVar[i] != NULL && TabVVar[i] != NULL);
		assert(watched(TabNVar[i]));
		used += strlen(TabNVar[i]) + 1 + strlen(TabVVar[i]) + 1;
		if (!watched(TabVVar[i]))
			watch(TabVVar[i], OWNER_VALUE);
	}
	assert(script_storage_runtime()->used == used);
}

int __wrap_yyparse(void)
{
	int result = __real_yyparse();

	assert(result == 0 && !parsed && NbVar >= 0);
	complete_count = (size_t)(NbVar + 1);
	watch_table(TabNVar, TabVVar, complete_count);
	assert(script_storage_runtime()->used > 0);
	parsed = true;
	return result;
}

bool __wrap_script_value_table_clear(script_value_table *table,
	unsigned int active_depth)
{
	assert(parsed && active_depth == 0 && clears == 0 && table->published);
	assert(table->names == TabNVar && table->values == TabVVar &&
		table->count == complete_count &&
		table->storage == script_storage_runtime());
	capture_current_values();
	assert(__real_script_value_table_clear(table, active_depth));
	clears++;
	assert(outstanding == 0 && script_storage_runtime()->used == 0);
	return true;
}

static void report_owners(void)
{
	bool active = strcmp(scenario, "init-quit") == 0 ||
		strcmp(scenario, "event-quit") == 0 ||
		strcmp(scenario, "periodic-quit") == 0;
	const char *path = getenv("SCRIPT_VALUE_OWNER_REPORT");
	FILE *report;

	assert(parsed && path != NULL);
	if (active)
	{
		assert(clears == 0 && TabNVar != NULL && TabVVar != NULL &&
			(size_t)(NbVar + 1) == complete_count);
		assert(released[OWNER_NAME] == 0 && released[OWNER_ARRAY] == 0);
		capture_current_values();
		assert(outstanding == 2 * complete_count + 2);
		assert(script_storage_runtime()->used > 0);
		bool found = false;

		for (size_t i = 0; i < complete_count; i++)
			if (strcmp(TabNVar[i], "text") == 0)
			{
				assert(strcmp(TabVVar[i],
					"seed:1:2:quit") == 0);
				found = true;
			}
		assert(found);
	}
	else
	{
		assert(clears == 1 && outstanding == 0);
		assert(TabNVar == NULL && TabVVar == NULL && NbVar == -1);
		assert(script_storage_runtime()->used == 0);
	}
	report = fopen(path, "w");
	assert(report != NULL);
	fprintf(report, "{\"entries\":%zu,\"names\":%zu,"
		"\"names_freed\":%zu,\"values\":%zu,\"values_freed\":%zu,"
		"\"arrays_freed\":%zu,\"retained\":%zu,\"clears\":%u,"
		"\"used\":%zu}\n", complete_count, allocated[OWNER_NAME],
		released[OWNER_NAME], allocated[OWNER_VALUE],
		released[OWNER_VALUE],
		released[OWNER_ARRAY], outstanding, clears,
		script_storage_runtime()->used);
	assert(fclose(report) == 0);
}

int __wrap_main(int argc, char **argv)
{
	scenario = getenv("SCRIPT_VALUE_SCENARIO");
	assert(scenario != NULL);
	/* ShutdownX is registered later, so it runs before this observer. */
	assert(atexit(report_owners) == 0);
	return __real_main(argc, argv);
}
#endif
