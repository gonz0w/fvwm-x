/* This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include "config.h"
#include "modules/FvwmScript/script_storage.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <unistd.h>

extern FILE *yyin;
extern int yyparse(void);
char *ScriptName = "parser-test";

enum input_kind { ARGUMENTS, FUNCTIONS, BLOCKS, VALUES, WIDGETS, TITLE_ARG, NUMBERS };

static void input_script(FILE *file, enum input_kind kind, int count)
{
	fputs("WindowTitle {ParserProbe}\nWindowSize 100 60\nInit\nBegin\n",
		file);
	if (kind == ARGUMENTS)
	{
		fputs(" Do", file);
		for (int i = 0; i < count; i++)
			fputs(" {word}", file);
		fputs("\n", file);
	}
	else if (kind == FUNCTIONS)
	{
		fputs(" Set $x=", file);
		for (int i = 0; i < count; i++)
			fputs("(Add 1 ", file);
		fputs("1", file);
		for (int i = 0; i < count; i++)
			fputs(")", file);
		fputs("\n", file);
	}
	else if (kind == BLOCKS)
	{
		for (int i = 0; i < count; i++)
			fputs(" If 1==1 Then\n Begin\n", file);
		fputs(" Do {Echo bounded}\n", file);
		for (int i = 0; i < count; i++)
			fputs(" End\n", file);
	}
	else if (kind == VALUES)
	{
		for (int i = 0; i < count; i++)
			fputs(" Do {value}\n", file);
	}
	else if (kind == NUMBERS)
	{
		const char *numbers[] = {"2147483647", "-2147483648",
			"2147483648", "-2147483649", "999999999999999999999999"};
		fprintf(file, " Set $number=%s\n", numbers[count]);
	}
	else if (kind == TITLE_ARG)
	{
		fputs(" ChangeWindowTitleFromArg 19\n", file);
		fputs(" If 1==1 Then\n ChangeWindowTitleFromArg 0\n", file);
	}
	fputs("End\n", file);
	int widgets = kind == WIDGETS ? count : 1;
	for (int i = 1; i <= widgets; i++)
		fprintf(file, "Widget %d\nProperty\n Type ItemDraw\n"
			" Position 5 5\n Size 60 20\n"
			" Title {widget}\nEnd\n", i);
}

static void test_parse_limits(enum input_kind kind, int count, int expected,
	size_t value_limit, size_t total_limit)
{
	int status;
	pid_t child = fork();

	assert(child >= 0);
	if (child == 0)
	{
		script_storage_runtime()->value_limit = value_limit;
		script_storage_runtime()->total_limit = total_limit;
		yyin = tmpfile();
		assert(yyin != NULL);
		input_script(yyin, kind, count);
		assert(!ferror(yyin));
		rewind(yyin);
		int result = yyparse();
		assert(fclose(yyin) == 0);
		_exit(result == 0 ? 0 : 1);
	}
	assert(waitpid(child, &status, 0) == child);
	if (!WIFEXITED(status) || WEXITSTATUS(status) != expected)
		fprintf(stderr,
			"parser case %d count %d: status %d, expected %d\n",
			kind, count, status, expected);
	assert(WIFEXITED(status) && WEXITSTATUS(status) == expected);
}

static void test_parse(enum input_kind kind, int count, int expected)
{
	test_parse_limits(kind, count, expected, SCRIPT_STORAGE_DEFAULT_VALUE,
		SCRIPT_STORAGE_DEFAULT_TOTAL);
}

int main(void)
{
	test_parse(NUMBERS, 0, 0);
	test_parse(NUMBERS, 1, 0);
	test_parse(NUMBERS, 2, 1);
	test_parse(NUMBERS, 3, 1);
	test_parse(NUMBERS, 4, 1);
	test_parse_limits(VALUES, 18, 0, 64, 128);
	test_parse_limits(VALUES, 19, 1, 64, 128);
	test_parse_limits(VALUES, 1, 1, 3, 128);
	test_parse(ARGUMENTS, 20, 0);
	test_parse(ARGUMENTS, 21, 1);
	test_parse(FUNCTIONS, 4, 0);
	test_parse(FUNCTIONS, 5, 1);
	test_parse(BLOCKS, 8, 0);
	test_parse(BLOCKS, 9, 1);
	test_parse(VALUES, 5120, 0);
	test_parse(VALUES, 5121, 1);
	test_parse(WIDGETS, 1000, 0);
	test_parse(WIDGETS, 1001, 1);
	test_parse(TITLE_ARG, 0, 0);
	return 0;
}
