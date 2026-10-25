/* This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include "config.h"
#include "modules/FvwmScript/types.h"
#include "libs/Bindings.h"
#include "libs/Strings.h"
#include <assert.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

extern Binding *BindingsList;
extern char *LastString;
extern char **TabVVar;
extern void InitCom(void);
extern void ExecBloc(Bloc *);
void __real_free(void *);

/* Linker wrapping must not inherit the compiler's leaf/builtin free contract.
 * Track exact live addresses; allocator reuse after release is legitimate. */
enum owner_kind { CONSTRUCTOR, BINDING, MESSAGE, OWNER_KINDS };
static struct { void *pointer; enum owner_kind kind; } volatile owners[256];
static volatile size_t outstanding;
static volatile size_t allocated[OWNER_KINDS], released[OWNER_KINDS];

static void watch(void *pointer, enum owner_kind kind)
{
	if (pointer == NULL)
		return;
	assert(outstanding < sizeof(owners) / sizeof(owners[0]));
	for (size_t i = 0; i < outstanding; i++)
		assert(owners[i].pointer != pointer);
	owners[outstanding].pointer = pointer;
	owners[outstanding++].kind = kind;
	allocated[kind]++;
}

void __wrap_free(void *pointer)
{
	for (size_t i = 0; i < outstanding; i++)
	{
		if (pointer != owners[i].pointer)
			continue;
		released[owners[i].kind]++;
		owners[i].pointer = owners[--outstanding].pointer;
		owners[i].kind = owners[outstanding].kind;
		break;
	}
	__real_free(pointer);
}

static void watch_binding(Binding *binding, void *constructor)
{
	assert(binding->Action != constructor);
	watch(binding, BINDING);
	watch(binding->key_name, BINDING);
	watch(binding->Action, BINDING);
	watch(binding->Action2, BINDING);
	watch(binding->windowName, BINDING);
}

#ifdef SCRIPT_BINDING_MODULE_PROBE
int __real_AddBinding(Display *, Binding **, binding_t, int, KeySym, char *,
	int, int, void *, void *, char *);
void __real_CopyString(char **, const char *);
int __real_main(int, char **);
static const char *scenario;
#else
static int copy_count;
static volatile unsigned int add_calls;

KeySym __wrap_FvwmStringToKeysym(Display *display, char *name)
{
	(void)display;
	return strcmp(name, "invalid-key") == 0 ? NoSymbol : XK_F12;
}
#endif

int __wrap_AddBinding(Display *display, Binding **list, binding_t type,
	int button, KeySym keysym, char *name, int modifiers, int contexts,
	void *action, void *action2, char *window_name)
{
	Binding *before = *list;
	int count;

	assert(action != NULL);
	watch(action, CONSTRUCTOR);
#ifdef SCRIPT_BINDING_MODULE_PROBE
	count = __real_AddBinding(display, list, type, button, keysym, name,
		modifiers, contexts, action, action2, window_name);
#else
	(void)display;
	(void)keysym;
	assert(type == BIND_KEYPRESS && action2 == NULL && window_name == NULL);
	assert(strcmp(action, "1 7 payload with spaces") == 0);
	assert(modifiers == 0);
	add_calls++;
	count = copy_count;
	/* A controlled consumer tests the producer contract for 0/1/many
	 * matches. Actual shared AddBinding is exercised by the X probe. */
	for (int i = 0; i < count; i++)
	{
		Binding *binding = calloc(1, sizeof(*binding));

		assert(binding != NULL);
		binding->type = type;
		binding->Button_Key = button;
		binding->Modifier = modifiers;
		binding->Context = contexts;
		binding->key_name = strdup(name);
		binding->Action = strdup(action);
		assert(binding->key_name && binding->Action);
		binding->NextBinding = *list;
		*list = binding;
	}
#endif
	int seen = 0;
	for (Binding *binding = *list; binding != before;
	    binding = binding->NextBinding)
	{
		assert(binding != NULL && binding->Action != NULL);
		assert(strcmp(binding->Action, action) == 0);
		watch_binding(binding, action);
		seen++;
	}
	assert(seen == count);
	return count;
}

#ifdef SCRIPT_BINDING_MODULE_PROBE
void __wrap_CopyString(char **destination, const char *source)
{
	__real_CopyString(destination, source);
	if (destination == &LastString)
		watch(*destination, MESSAGE);
}

static void report_owners(void)
{
	bool active = strcmp(scenario, "init-quit") == 0 ||
		strcmp(scenario, "event-quit") == 0 ||
		strcmp(scenario, "periodic-quit") == 0;
	bool refused = strcmp(scenario, "partial-startup") == 0;
	const char *path = getenv("SCRIPT_BINDING_OWNER_REPORT");
	FILE *report;

	assert(path != NULL);
	assert(allocated[CONSTRUCTOR] == released[CONSTRUCTOR]);
	assert(refused || allocated[BINDING] > 0);
	if (!active)
	{
		assert(outstanding == 0);
		assert(BindingsList == NULL && LastString == NULL);
	}
	else
	{
		assert(BindingsList != NULL);
		assert(released[BINDING] == 0);
		if (strcmp(scenario, "init-quit") == 0)
		{
			assert(LastString == NULL);
			assert(outstanding == allocated[BINDING]);
		}
		else
		{
			assert(LastString != NULL);
			assert(allocated[MESSAGE] - released[MESSAGE] == 1);
			assert(outstanding == allocated[BINDING] + 1);
		}
	}
	report = fopen(path, "w");
	assert(report != NULL);
	fprintf(report, "{\"constructors\":%zu,\"constructors_freed\":%zu,"
		"\"binding_allocations\":%zu,\"bindings_freed\":%zu,"
		"\"messages\":%zu,\"messages_freed\":%zu,\"retained\":%zu}\n",
		allocated[CONSTRUCTOR], released[CONSTRUCTOR],
		allocated[BINDING], released[BINDING], allocated[MESSAGE],
		released[MESSAGE], outstanding);
	assert(fclose(report) == 0);
}

int __wrap_main(int argc, char **argv)
{
	scenario = getenv("SCRIPT_BINDING_SCENARIO");
	assert(scenario != NULL);
	/* ShutdownX is registered later and runs before this observer. */
	assert(atexit(report_owners) == 0);
	return __real_main(argc, argv);
}
#else
int __wrap_main(int argc, char **argv)
{
	bool baseline = argc == 2 && strcmp(argv[1], "--baseline") == 0;
	char *variables[] = {"F12", "N", "1", "7", "payload with spaces"};
	long arguments[] = {0, 1, 2, 3, 4};
	Instr instruction = {25, arguments, 5};
	Bloc block = {&instruction, 0};
	const int copies[] = {0, 1, 3};

	assert(argc == 1 || baseline);
	TabVVar = variables;
	InitCom();
	for (size_t i = 0; i < sizeof(copies) / sizeof(copies[0]); i++)
	{
		size_t before = released[CONSTRUCTOR];

		copy_count = copies[i];
		ExecBloc(&block);
		assert(add_calls == i + 1);
		assert(released[CONSTRUCTOR] == before + (baseline ? 0 : 1));
		for (Binding *binding = BindingsList; binding;
		    binding = binding->NextBinding)
			assert(strcmp(binding->Action,
				"1 7 payload with spaces") == 0);
		FreeBindingList(BindingsList);
		BindingsList = NULL;
		assert(allocated[BINDING] == released[BINDING]);
		assert(outstanding == (baseline ? i + 1 : 0));
	}
	variables[0] = "invalid-key";
	ExecBloc(&block);
	variables[0] = "F12";
	variables[1] = "invalid-modifier";
	ExecBloc(&block);
	assert(add_calls == 3 && BindingsList == NULL);
	if (baseline)
	{
		printf("old production Key: %zu constructors retained for "
			"0/1/3 copied matches\n", outstanding);
		assert(outstanding == 3);
		/* Reclaim the observed owners after the measurement. */
		while (outstanding != 0)
			free(owners[0].pointer);
	}
	assert(outstanding == 0);
	TabVVar = NULL;
	puts("actual Key producer and shared destructor ownership passed");
	return 0;
}
#endif
