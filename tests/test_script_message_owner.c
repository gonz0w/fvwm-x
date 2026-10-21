/* This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include "config.h"
#include "modules/FvwmScript/types.h"
#include <assert.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

extern int TabIdObj[1001];
extern CaseObj *TabCObj;
extern struct XObj *tabxobj[1000];
extern char *LastString;
void SendMsgAndString(char *, char *);
char *__real_GetNextToken(char *, char **);
void __real_free(void *);
static void *tokens[8];
static unsigned int outstanding;
static bool watching;

char *__wrap_GetNextToken(char *text, char **token)
{
	char *next = __real_GetNextToken(text, token);

	if (watching && *token != NULL)
	{
		assert(outstanding < sizeof(tokens) / sizeof(tokens[0]));
		tokens[outstanding++] = *token;
	}
	return next;
}

void __wrap_free(void *pointer)
{
	for (unsigned int i = 0; i < outstanding; i++)
	{
		if (tokens[i] == pointer)
		{
			tokens[i] = tokens[--outstanding];
			break;
		}
	}
	__real_free(pointer);
}

int __wrap_main(int argc, char **argv)
{
	CaseObj cases = {.NbCase = -1};
	struct XObj widget = {.id = 1};
	const char *actions[] = {"1 7 payload with spaces",
		"'0001' '+7' quoted",
		"1 7", "1 7   {literal, # tail}"};
	const char *expected[] = {"payload with spaces", "quoted", NULL,
		"{literal, # tail}"};

	(void)argc;
	(void)argv;
	TabIdObj[1] = 0;
	TabCObj = &cases;
	tabxobj[0] = &widget;
	for (unsigned int repeat = 0; repeat < 100; repeat++)
	{
		for (size_t i = 0;
		    i < sizeof(actions) / sizeof(actions[0]); i++)
		{
			char *input = strdup(actions[i]);

			assert(input != NULL);
			watching = true;
			SendMsgAndString(input, repeat & 1 ?
				"SendString" : "CheckBinding");
			watching = false;
			assert(outstanding == 0);
			assert(expected[i] == NULL ? LastString == NULL :
				LastString != NULL &&
				strcmp(LastString, expected[i]) == 0);
			free(input);
		}
	}
	free(LastString);
	LastString = NULL;
	TabCObj = NULL;
	tabxobj[0] = NULL;
	puts("actual Script messages retain payload and release token owners");
	return 0;
}
