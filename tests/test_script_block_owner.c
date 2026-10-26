/* This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include "config.h"
#include "modules/FvwmScript/script_block.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

extern FILE *yyin;
extern int yyparse(void);
extern ScriptProp *scriptprop;
extern int nbobj;
extern Bloc **TabIObj;
extern CaseObj *TabCObj;
#ifdef SCRIPT_BLOCK_MODULE_PROBE
extern char *ScriptName;
static void *init_root;
static volatile bool init_released;
#else
char *ScriptName = "block-owner-test";
#endif

/* Linker wrapping is invisible to the compiler's builtin/leaf free contract.
 * Volatile observations keep the before-control's direct frees observable. */
static void *volatile watched[256];
static volatile size_t watched_count, frees;

void __real_free(void *pointer);

static void watch(void *pointer)
{
	if (pointer == NULL)
		return;
	assert(watched_count < sizeof(watched) / sizeof(watched[0]));
	for (size_t i = 0; i < watched_count; i++)
		assert(watched[i] != pointer);
	watched[watched_count++] = pointer;
}

void __wrap_free(void *pointer)
{
#ifdef SCRIPT_BLOCK_MODULE_PROBE
	if (pointer != NULL && pointer == init_root)
		init_released = true;
#endif
	for (size_t i = 0; i < watched_count; i++)
	{
		if (watched[i] == pointer)
		{
			watched[i] = watched[--watched_count];
			frees++;
			break;
		}
	}
	__real_free(pointer);
}

typedef struct expected_block expected_block;
typedef struct
{
	int type;
	const expected_block *first;
	const expected_block *second;
} expected_instruction;

struct expected_block
{
	int count;
	const expected_instruction *instructions;
};

static const expected_block empty = {0, NULL};
static const expected_instruction leaf_instructions[] = {{1, NULL, NULL}};
static const expected_block leaf = {1, leaf_instructions};
static const expected_instruction if_instructions[] = {{14, &leaf, NULL}};
static const expected_block if_only = {1, if_instructions};
#ifndef SCRIPT_BLOCK_MODULE_PROBE
static const expected_instruction else_instructions[] = {{14, &leaf, &leaf}};
static const expected_block if_else = {1, else_instructions};
static const expected_instruction loop_instructions[] = {{15, &leaf, NULL}};
static const expected_block loop = {1, loop_instructions};
static const expected_instruction while_instructions[] = {{16, &leaf, NULL}};
static const expected_block while_block = {1, while_instructions};
static const expected_instruction mixed_instructions[] = {
	{1, NULL, NULL}, {14, &leaf, &leaf}, {15, &leaf, NULL},
	{16, &if_only, NULL}
};
static const expected_block mixed = {4, mixed_instructions};
#endif

/* The oracle describes the source syntax, independently of the destructor's
 * type dispatch. Assert that the real parser produces exactly this shape. */
static void expect_block(Bloc *block, const expected_block *expected, bool heap)
{
	assert(block != NULL && block->NbInstr == expected->count - 1);
	if (heap)
		watch(block);
	watch(block->TabInstr);
	for (int i = 0; i < expected->count; i++)
	{
		Instr *instruction = &block->TabInstr[i];
		const expected_instruction *item = &expected->instructions[i];

		assert(instruction->Type == item->type);
		watch(instruction->TabArg);
		if (item->first != NULL)
		{
			int position = instruction->NbArg -
				(item->type == 14 ? 2 : 1);

			expect_block((Bloc *)instruction->TabArg[position],
				item->first, true);
		}
		if (item->second != NULL)
			expect_block((Bloc *)instruction->TabArg[
				instruction->NbArg - 1], item->second, true);
		else if (item->type == 14)
			assert(instruction->TabArg[instruction->NbArg - 1] == 0);
	}
}

#ifndef SCRIPT_BLOCK_MODULE_PROBE
static const char *bodies[] = {
	"",
	"Do {ordinary}\n",
	"If 1==1 Then\nBegin\nDo {then}\nEnd\n",
	"If 1==1 Then\nBegin\nDo {then}\nEnd\n"
		"Else\nBegin\nDo {else}\nEnd\n",
	"For $counter=1 To 2 Do\nBegin\nDo {loop}\nEnd\n",
	"While 0==1 Do\nBegin\nDo {while}\nEnd\n",
	"Do {ordinary}\nIf 1==1 Then\nBegin\nDo {then}\nEnd\n"
		"Else\nBegin\nDo {else}\nEnd\n"
		"For $counter=1 To 2 Do\nBegin\nDo {loop}\nEnd\n"
		"While 0==1 Do\nBegin\nIf 1==1 Then\n"
		"Begin\nDo {nested}\nEnd\nEnd\n"
};
static const expected_block *shapes[] = {
	&empty, &leaf, &if_only, &if_else, &loop, &while_block, &mixed
};

static void parse(const char *body, bool extra_roots)
{
	yyin = tmpfile();
	assert(yyin != NULL);
	fputs("WindowTitle {BlockOwners}\nWindowSize 100 60\nInit\nBegin\n",
		yyin);
	fputs(body, yyin);
	fputs("End\n", yyin);
	if (extra_roots)
	{
		fputs("PeriodicTasks\nBegin\nDo {periodic}\nEnd\n"
			"QuitFunc\nBegin\nDo {quit}\nEnd\n"
			"Widget 1\nProperty\nType ItemDraw\nPosition 1 1\n"
			"Size 10 10\nMain\nCase message of\n"
			"1 :\nBegin\nDo {case-one}\nEnd\n"
			"2 :\nBegin\nEnd\nEnd\n"
			"Widget 2\nProperty\nType ItemDraw\nPosition 1 1\n"
			"Size 10 10\nMain\nCase message of\nEnd\n", yyin);
	}
	assert(!ferror(yyin));
	rewind(yyin);
	assert(yyparse() == 0);
	assert(fclose(yyin) == 0);
}

static void parsed_case(size_t index, bool baseline)
{
	script_block_owner owner;

	parse(bodies[index], !baseline);
	expect_block(scriptprop->initbloc, shapes[index], true);
	if (baseline)
	{
		size_t allocated = watched_count;

		/* Private before-control: these are the exact two old BuildGUI
		 * release operations, on real parsed output. */
		free(scriptprop->initbloc->TabInstr);
		free(scriptprop->initbloc);
		printf("old Init cleanup: %zu owned, %zu freed, %zu retained\n",
			allocated, frees, watched_count);
		fflush(stdout);
		assert(allocated == 21 && frees == 2 && watched_count == 19);
		/* Observe before reclamation; every watched allocation is unique.
		 * No dereference of the shallow-freed root is needed. */
		while (watched_count != 0)
			__real_free(watched[--watched_count]);
		return;
	}
	assert(nbobj == 1 && TabCObj[0].NbCase == 1);
	assert(TabCObj[1].NbCase == -1);
	expect_block(scriptprop->periodictasks, &leaf, true);
	expect_block(scriptprop->quitfunc, &leaf, true);
	expect_block(&TabIObj[0][0], &leaf, false);
	expect_block(&TabIObj[0][1], &empty, false);
	watch(TabIObj[0]);
	watch(TabCObj[0].LstCase);
	watch(TabIObj);
	watch(TabCObj);
	/* Simulate allocator poison in the later empty Main slot. */
	TabIObj[1] = (Bloc *)(uintptr_t)1;
	TabCObj[1].LstCase = (int *)(uintptr_t)1;
	owner = (script_block_owner){scriptprop->initbloc,
		scriptprop->periodictasks, scriptprop->quitfunc, TabIObj,
		TabCObj, (size_t)(nbobj + 1), true};
	size_t allocated = watched_count;

	/* The owner uses its frozen extent after the parser is finished. */
	nbobj = 999;
	assert(!script_block_owner_release_init(&owner, 1));
	assert(!script_block_owner_clear(&owner, 2));
	assert(frees == 0 && watched_count == allocated && owner.init != NULL);
	assert(script_block_owner_release_init(&owner, 0));
	assert(owner.init == NULL && owner.periodic != NULL && owner.quit != NULL);
	assert(watched_count != 0 && watched_count < allocated);
	assert(script_block_owner_release_init(&owner, 0));
	assert(script_block_owner_clear(&owner, 0));
	assert(watched_count == 0 && frees == allocated);
	assert(owner.object_count == 0 && owner.object_blocks == NULL &&
		owner.object_cases == NULL && !owner.published);
	assert(script_block_owner_clear(&owner, 0));
	assert(frees == allocated);
}

static void synthetic_case(void)
{
	Bloc embedded = {calloc(1, sizeof(Instr)), 0};
	long borrowed_value = 123;

	assert(embedded.TabInstr != NULL);
	embedded.TabInstr[0] = (Instr){1, malloc(sizeof(long)), 1};
	assert(embedded.TabInstr[0].TabArg != NULL);
	embedded.TabInstr[0].TabArg[0] = (long)&borrowed_value;
	watch(embedded.TabInstr);
	watch(embedded.TabInstr[0].TabArg);
	script_block_clear(&embedded);
	assert(watched_count == 0 && frees == 2 && borrowed_value == 123);
	assert(embedded.TabInstr == NULL && embedded.NbInstr == -1);
	script_block_clear(&embedded);
	Bloc *root = malloc(sizeof(*root));

	assert(root != NULL);
	*root = (Bloc){NULL, -1};
	watch(root);
	script_block_destroy(&root);
	assert(root == NULL && watched_count == 0 && frees == 3);
	script_block_destroy(&root);
	script_block_owner owner = {
		.init = malloc(sizeof(Bloc)),
		.object_blocks = malloc(2 * sizeof(Bloc *)),
		.object_cases = malloc(2 * sizeof(CaseObj)),
		.object_count = 2,
		.published = true
	};

	assert(owner.init && owner.object_blocks && owner.object_cases);
	*owner.init = (Bloc){NULL, -1};
	for (int i = 0; i < 2; i++)
	{
		owner.object_cases[i].NbCase = -1;
		owner.object_cases[i].LstCase = (int *)(uintptr_t)1;
		owner.object_blocks[i] = (Bloc *)(uintptr_t)1;
	}
	watch(owner.init);
	watch(owner.object_blocks);
	watch(owner.object_cases);
	assert(!script_block_owner_clear(&owner, 1));
	assert(watched_count == 3 && frees == 3);
	assert(script_block_owner_clear(&owner, 0));
	assert(watched_count == 0 && frees == 6);
	assert(script_block_owner_clear(&owner, 0));
}

int main(int argc, char **argv)
{
	if (argc == 2 && strcmp(argv[1], "--baseline-init") == 0)
	{
		parsed_case(6, true);
		fflush(stdout);
		/* Parser string/variable/header ownership is outside this test. */
		_exit(0);
	}
	assert(argc == 1);
	for (size_t i = 0; i < sizeof(shapes) / sizeof(shapes[0]); i++)
	{
		int status;
		pid_t child = fork();

		assert(child >= 0);
		if (child == 0)
		{
			parsed_case(i, false);
			_exit(0);
		}
		assert(waitpid(child, &status, 0) == child);
		assert(WIFEXITED(status) && WEXITSTATUS(status) == 0);
	}
	synthetic_case();
	return 0;
}

#else
/* Private executable probe: link the real module objects with --wrap=main,
 * --wrap=yyparse and --wrap=free. No production ownership hook is required. */
extern X11base *x11base;
extern Bloc *PileBloc[10];
int __real_main(int argc, char **argv);
int __real_yyparse(void);
void __real_SendFinishedStartupNotification(int *fd);

void __wrap_SendFinishedStartupNotification(int *fd)
{
	/* BuildGUI must retire Init before publishing successful startup. */
	assert(init_released);
	__real_SendFinishedStartupNotification(fd);
}
static size_t complete_count, init_count;
static const char *scenario;
static bool parsed;

static const expected_instruction write_instructions[] = {{18, NULL, NULL}};
static const expected_block write_block = {1, write_instructions};
static const expected_instruction write_quit_instructions[] = {
	{18, NULL, NULL}, {13, NULL, NULL}
};
static const expected_block write_quit = {2, write_quit_instructions};
static const expected_instruction live_init_instructions[] = {
	{1, NULL, NULL}, {14, &leaf, &leaf}, {15, &leaf, NULL},
	{16, &if_only, NULL}, {18, NULL, NULL}, {13, NULL, NULL}
};

int __wrap_yyparse(void)
{
	int result = __real_yyparse();
	bool init_quit = strcmp(scenario, "init-quit") == 0;
	expected_block initial = {init_quit ? 6 : 5, live_init_instructions};

	assert(result == 0 && !parsed && nbobj == 1);
	expect_block(scriptprop->initbloc, &initial, true);
	init_root = scriptprop->initbloc;
	init_count = watched_count;
	expect_block(scriptprop->periodictasks,
		strcmp(scenario, "periodic-quit") == 0 ?
		&write_quit : &write_block, true);
	expect_block(scriptprop->quitfunc, &write_block, true);
	assert(TabCObj[0].NbCase == 1 && TabCObj[1].NbCase == -1);
	expect_block(&TabIObj[0][0],
		strcmp(scenario, "event-quit") == 0 ?
		&write_quit : &write_block, false);
	expect_block(&TabIObj[0][1], &empty, false);
	watch(TabIObj[0]);
	watch(TabCObj[0].LstCase);
	watch(TabIObj);
	watch(TabCObj);
	complete_count = watched_count;
	parsed = true;
	return result;
}

static void report_module_ownership(void)
{
	bool active_init = strcmp(scenario, "init-quit") == 0;
	bool active_callback = strcmp(scenario, "event-quit") == 0 ||
		strcmp(scenario, "periodic-quit") == 0;
	size_t retained = active_init ? complete_count :
		(active_callback ? complete_count - init_count : 0);
	const char *path = getenv("SCRIPT_BLOCK_OWNER_REPORT");
	FILE *report;

	assert(parsed && path != NULL);
	assert(watched_count == retained && frees + retained == complete_count);
	assert(init_released == !active_init);
	/* Read pointer fields only: never dereference a retired allocation. */
	if (retained == 0)
	{
		assert(TabIObj == NULL && TabCObj == NULL);
		assert(x11base == NULL);
		if (scriptprop != NULL)
			assert(scriptprop->initbloc == NULL &&
				scriptprop->periodictasks == NULL &&
				scriptprop->quitfunc == NULL);
	}
	else
	{
		assert(TabIObj != NULL && TabCObj != NULL);
		assert(x11base->periodictasks != NULL && x11base->quitfunc != NULL);
	}
	for (int i = 0; i < 10; i++)
		assert(PileBloc[i] == NULL);
	report = fopen(path, "w");
	assert(report != NULL);
	fprintf(report, "{\"owned\":%zu,\"freed\":%zu,"
		"\"retained\":%zu,\"init_released\":%s}\n",
		complete_count, frees, retained, init_released ? "true" : "false");
	assert(fclose(report) == 0);
}

int __wrap_main(int argc, char **argv)
{
	scenario = getenv("SCRIPT_BLOCK_SCENARIO");
	assert(scenario != NULL);
	/* ShutdownX is registered later, and therefore runs before this observer. */
	assert(atexit(report_module_ownership) == 0);
	return __real_main(argc, argv);
}
#endif
