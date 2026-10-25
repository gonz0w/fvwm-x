/* This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include "config.h"
#include "modules/FvwmScript/types.h"
#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern ScriptProp *scriptprop;
extern TabObj *tabobj;
extern FILE *yyin;
int __real_yyparse(void);
void __real_free(void *pointer);

enum owner_kind { TOKEN_OWNER, COPY_OWNER, DEFAULT_OWNER, TYPE_OWNER,
	FINAL_OWNER, BASE_OWNER, OWNER_KINDS };
static struct { void *pointer; enum owner_kind kind; } volatile owners[512];
static volatile size_t outstanding;
static volatile size_t allocated[OWNER_KINDS], released[OWNER_KINDS];
#ifdef SCRIPT_PROPERTY_MODULE_PROBE
static void observe_parser_retirement(void);
static bool parser_retired;
#endif

static bool watched(void *pointer)
{
	for (size_t i = 0; i < outstanding; i++)
		if (owners[i].pointer == pointer)
			return true;
	return false;
}

static void watch(void *pointer, enum owner_kind kind)
{
	assert(pointer != NULL && !watched(pointer) && outstanding < 512);
	owners[outstanding].pointer = pointer;
	owners[outstanding++].kind = kind;
	allocated[kind]++;
}

void __wrap_free(void *pointer)
{
#ifdef SCRIPT_PROPERTY_MODULE_PROBE
	if (!parser_retired && pointer != NULL && pointer == tabobj)
	{
		parser_retired = true;
		observe_parser_retirement();
	}
#endif
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

#ifndef SCRIPT_PROPERTY_MODULE_PROBE
#include "modules/FvwmScript/script.h"
#include "libs/Strings.h"
char *ScriptName = "property-owner-test";
int yyparse(void);
int __real_yylex(void);
char *__real_fxstrdup(const char *text);
static char catalog_title[] = " \tcatalog-title \nsecond line  ";
static const char *header_copy_source;
static unsigned int translated, untranslated;

int __wrap_yylex(void)
{
	int token = __real_yylex();

	if (token == STR || token == GSTR || token == FONT)
		watch(yylval.str, TOKEN_OWNER);
	return token;
}

static const char *translate(char *text)
{
	if (strcmp(text, "translated") == 0)
	{
		translated++;
		return catalog_title;
	}
	untranslated++;
	return text;
}

const char *__wrap_FGettext(char *text)
{
	header_copy_source = translate(text);
	return header_copy_source;
}

char *__wrap_fxstrdup(const char *text)
{
	char *copy = __real_fxstrdup(text);

	if (text == header_copy_source)
	{
		assert(copy != text && strcmp(copy, text) == 0);
		watch(copy, COPY_OWNER);
		header_copy_source = NULL;
	}
	return copy;
}

char *__wrap_FGettextCopy(char *text)
{
	char *copy = NULL;

	/* Match production FGettextCopy's CopyString semantics while controlling
	 * the otherwise borrowed catalog result. Header copies must stay exact. */
	CopyString(&copy, translate(text));
	assert(strcmp(copy, strcmp(text, "translated") == 0 ?
		"catalog-title" : "missing") == 0);

	watch(copy, COPY_OWNER);
	return copy;
}

static void repetitions(FILE *file, const char *key, bool plain,
	const char *final)
{
	if (plain)
		fprintf(file, "%s same\n%s same\n%s different\n%s %s\n",
			key, key, key, key, final);
	else
		fprintf(file, "%s {same}\n%s {same}\n%s {}\n%s {%s}\n",
			key, key, key, key, final);
}

static void title_repetitions(FILE *file, const char *ordinary,
	const char *localized)
{
	fprintf(file, "%s {ordinary}\n%s {translated}\n%s {}\n"
		"%s {  missing  }\n%s {translated}\n", ordinary, localized,
		ordinary, localized, localized);
}

int main(int argc, char **argv)
{
	bool baseline = argc == 2 && strcmp(argv[1], "--baseline") == 0;
	const char *colors[] = {"ForeColor", "BackColor", "ShadowColor",
		"HilightColor"};
	char **finals[16];
	size_t n = 0;

	assert(argc == 1 || baseline);
	yyin = tmpfile();
	assert(yyin != NULL);
	title_repetitions(yyin, "WindowTitle", "WindowLocaleTitle");
	repetitions(yyin, "Icon", true, "final-icon");
	/* FONT uses GetNextToken rather than GSTR: braces stay literal. */
	repetitions(yyin, "Font", true, "fixed");
	for (size_t i = 0; i < 4; i++)
		repetitions(yyin, colors[i], false, "black");
	fputs("WindowSize 200 100\nWidget 1\nProperty\n", yyin);
	repetitions(yyin, "Type", true, "ItemDraw");
	title_repetitions(yyin, "Title", "LocaleTitle");
	repetitions(yyin, "SwallowExec", false, "");
	repetitions(yyin, "Icon", true, "final-icon");
	repetitions(yyin, "Font", true, "fixed");
	for (size_t i = 0; i < 4; i++)
		repetitions(yyin, colors[i], false, "black");
	fputs("Position 1 1\nSize 20 20\nEnd\n", yyin);
	rewind(yyin);
	assert(yyparse() == 0 && fclose(yyin) == 0);
	assert(allocated[TOKEN_OWNER] == 66);
	assert(allocated[COPY_OWNER] == (baseline ? 3 : 6));
	assert(translated == 4 && untranslated == 2);
#define FINAL(field) finals[n++] = &(field)
	FINAL(scriptprop->titlewin);
	FINAL(scriptprop->icon);
	FINAL(scriptprop->font);
	FINAL(scriptprop->forecolor);
	FINAL(scriptprop->backcolor);
	FINAL(scriptprop->shadcolor);
	FINAL(scriptprop->hilicolor);
	FINAL((*tabobj)[0].type);
	FINAL((*tabobj)[0].title);
	FINAL((*tabobj)[0].swallow);
	FINAL((*tabobj)[0].icon);
	FINAL((*tabobj)[0].font);
	FINAL((*tabobj)[0].forecolor);
	FINAL((*tabobj)[0].backcolor);
	FINAL((*tabobj)[0].shadcolor);
	FINAL((*tabobj)[0].hilicolor);
#undef FINAL
	assert(n == 16 && scriptprop->colorset == -1 &&
		(*tabobj)[0].colorset == -1);
	assert(strcmp(scriptprop->titlewin, catalog_title) == 0 &&
		strcmp((*tabobj)[0].title, "catalog-title") == 0);
	const char *expected[] = {catalog_title, "final-icon", "fixed",
		"black", "black", "black", "black", "ItemDraw",
		"catalog-title", "", "final-icon", "fixed", "black",
		"black", "black", "black"};

	for (size_t i = 0; i < n; i++)
		assert(strcmp(*finals[i], expected[i]) == 0);
	for (size_t i = 0; i < n; i++)
		assert(watched(*finals[i]) ||
			(baseline && i == 0 && *finals[i] == catalog_title));
	if (baseline)
	{
		assert(outstanding == 69 && released[TOKEN_OWNER] == 0 &&
			released[COPY_OWNER] == 0);
		printf("old actual parser: %zu property/input owners retained\n",
			outstanding);
		/* Reclaim only after observing the old producer's retention. */
		while (outstanding != 0)
			free(owners[0].pointer);
	}
	else
	{
		assert(outstanding == n && released[TOKEN_OWNER] == 52 &&
			released[COPY_OWNER] == 4);
		assert(scriptprop->titlewin != catalog_title);
		for (size_t i = 0; i < n; i++)
		{
			assert(watched(*finals[i]));
			free(*finals[i]);
			*finals[i] = NULL;
		}
		assert(outstanding == 0);
	}
	free(scriptprop);
	free(tabobj);
	extern Bloc **TabIObj;
	extern CaseObj *TabCObj;
	free(TabIObj);
	free(TabCObj);
	puts("actual parser property replacement ownership passed");
	return 0;
}
#else
/* Private probe linked with the actual production module objects. */
extern X11base *x11base;
extern struct XObj *tabxobj[1000];
extern int nbobj;
int __real_main(int argc, char **argv);
void __real_ChooseFunction(struct XObj *object, char *type);
static const char *scenario;
static bool baseline, parsed;
static unsigned int chosen;
static size_t object_count;

static void observe_parser_retirement(void)
{
	assert(parsed && chosen == object_count);
	for (size_t i = 0; i < object_count; i++)
		assert(((*tabobj)[i].type == NULL) == !baseline);
}

int __wrap_yyparse(void)
{
	char *defaults[] = {x11base->font, x11base->forecolor,
		x11base->backcolor, x11base->shadcolor, x11base->hilicolor};

	for (size_t i = 0; i < 5; i++)
		watch(defaults[i], DEFAULT_OWNER);
	int result = __real_yyparse();

	assert(result == 0 && !parsed && nbobj == 3);
	object_count = (size_t)(nbobj + 1);
	for (size_t i = 0; i < object_count; i++)
		watch((*tabobj)[i].type, TYPE_OWNER);
	char *base[] = {scriptprop->font, scriptprop->forecolor,
		scriptprop->backcolor, scriptprop->shadcolor,
		scriptprop->hilicolor, scriptprop->titlewin};

	for (size_t i = 0; i < 6; i++)
		watch(base[i], BASE_OWNER);
	parsed = true;
	return result;
}

void __wrap_ChooseFunction(struct XObj *object, char *type)
{
	MyObject *property = &(*tabobj)[chosen];

	assert(parsed && object->id == (int)chosen + 1 && type == property->type);
	assert(watched(type) && strcmp(type, "ItemDraw") == 0);
	assert(released[DEFAULT_OWNER] == (baseline ? 0 : 5));
	assert(x11base->font == scriptprop->font &&
		x11base->forecolor == scriptprop->forecolor &&
		x11base->backcolor == scriptprop->backcolor &&
		x11base->shadcolor == scriptprop->shadcolor &&
		x11base->hilicolor == scriptprop->hilicolor &&
		x11base->title == scriptprop->titlewin);
	assert(x11base->colorset == 0);
	assert(object->colorset == (chosen == 2 ? -1 : 0));
	assert(strcmp(object->font, "fixed") == 0);
	assert(property->font != NULL ? object->font == property->font :
		object->font != x11base->font);
	char *strings[] = {object->title, object->swallow, object->icon,
		object->font, object->forecolor, object->backcolor,
		object->shadcolor, object->hilicolor};

	for (size_t i = 0; i < 8; i++)
		if (strings[i] != NULL)
			watch(strings[i], FINAL_OWNER);
	__real_ChooseFunction(object, type);
	chosen++;
}

static void report_owners(void)
{
	const char *path = getenv("SCRIPT_PROPERTY_OWNER_REPORT");
	bool before_prepare = strcmp(scenario, "before-prepare") == 0;
	FILE *report;

	assert(parsed && path != NULL && chosen == (before_prepare ? 3 : 4));
	assert(released[DEFAULT_OWNER] == (baseline ? 0 : 5));
	assert(released[TYPE_OWNER] == (baseline ? 0 : chosen));
	assert(released[FINAL_OWNER] == allocated[FINAL_OWNER]);
	assert(released[BASE_OWNER] == 0 && allocated[BASE_OWNER] == 6);
	assert(strcmp(x11base->title, "PropertyOwners") == 0 &&
		watched(x11base->title));
	report = fopen(path, "w");
	assert(report != NULL);
	fprintf(report, "{\"defaults_freed\":%zu,\"types_freed\":%zu,"
		"\"chosen\":%u,\"widget_strings\":%zu,"
		"\"widget_strings_freed\":%zu,\"base_retained\":%zu}\n",
		released[DEFAULT_OWNER], released[TYPE_OWNER], chosen,
		allocated[FINAL_OWNER], released[FINAL_OWNER],
		allocated[BASE_OWNER] - released[BASE_OWNER]);
	assert(fclose(report) == 0);
}

int __wrap_main(int argc, char **argv)
{
	scenario = getenv("SCRIPT_PROPERTY_SCENARIO");
	baseline = getenv("SCRIPT_PROPERTY_BASELINE") != NULL;
	assert(scenario != NULL && atexit(report_owners) == 0);
	return __real_main(argc, argv);
}
#endif
