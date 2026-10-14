/* This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */
#include "config.h"
#include "modules/FvwmScript/script_file.h"
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static void test_section_updates(void)
{
	char *first = script_file_build("", 0, "demo", 42, "");
	assert(first && strcmp(first, "\n#demo,42\n\n#end\n\n") == 0);
	char *second = script_file_build(
		first, strlen(first), "demo", 42, "abc");
	assert(second && strcmp(second, "\n#demo,42\n\nabc\n#end\n\n") == 0);
	char *third = script_file_build(
		second, strlen(second), "demo", 43, "new\n");
	assert(third && strcmp(third, "\n#demo,43\nnew\n#end\n\n") == 0);
	char *literal = script_file_build("", 0, "demo", 42, "#ending");
	assert(literal && strstr(literal, "#ending\n"));
	free(literal);
	free(first);
	free(second);
	free(third);
	const char *original = "prefix\n#other,1\nkeep\n#end\n"
		"#demo,1\nold\n#end\nsuffix";
	char *result = script_file_build(
		original, strlen(original), "demo", 2, "new");
	assert(result && strcmp(result,
		"prefix\n#other,1\nkeep\n#end\n"
		"#demo,2\nnew\n#end\nsuffix") == 0);
	free(result);
}

static void test_malformed_and_limits(void)
{
	const char *invalid[] = {"#demo,2\nmissing", "#demo,bad\n#end\n",
		"#demo,1\n#demo,2\n#end\n",
		"#demo,99999999999999999999999\n#end\n"};
	for (size_t i = 0; i < sizeof(invalid) / sizeof(invalid[0]); i++)
	{
		assert(script_file_build(invalid[i], strlen(invalid[i]),
			"demo", 2, "x") == NULL);
		assert(errno == EINVAL);
	}
	assert(script_file_build("x\0y", 3, "demo", 1, "x") == NULL);
	assert(script_file_build("", 0, "bad,name", 1, "x") == NULL);
	assert(script_file_build("", 0, "demo", 1, "#end") == NULL);
	assert(script_file_build("", 0, "demo", 1, "#demo,9\n") == NULL);
	char *source = malloc(SCRIPT_FILE_MAX_BYTES + 1);
	char *text = malloc(SCRIPT_TEXT_MAX_BYTES + 2);
	assert(source && text);
	memset(source, 's', SCRIPT_FILE_MAX_BYTES + 1);
	memset(text, 'x', SCRIPT_TEXT_MAX_BYTES + 1);
	text[SCRIPT_TEXT_MAX_BYTES] = '\0';
	/* Former 32,000-byte exact read had no terminator. */
	char *result = script_file_build(source, 32000, "demo", 2, text);
	assert(result && strlen(result) > 32000 + SCRIPT_TEXT_MAX_BYTES);
	free(result);
	assert(script_file_build(source, SCRIPT_FILE_MAX_BYTES,
		"demo", 2, text) == NULL);
	assert(errno == EFBIG);
	assert(script_file_build(source, SCRIPT_FILE_MAX_BYTES + 1,
		"demo", 2, "x") == NULL);
	text[SCRIPT_TEXT_MAX_BYTES] = 'x';
	text[SCRIPT_TEXT_MAX_BYTES + 1] = '\0';
	assert(script_file_build("", 0, "demo", 2, text) == NULL);
	assert(errno == EFBIG);
	free(source);
	free(text);
}

static void test_file_validation(void)
{
	char directory[] = "/tmp/fvwm-script-file-XXXXXX";
	assert(mkdtemp(directory));
	char path[256], alias[256];
	snprintf(path, sizeof(path), "%s/output", directory);
	snprintf(alias, sizeof(alias), "%s/alias", directory);
	assert(script_file_update(path, "demo", 1, "old") == 0);
	assert(script_file_update(path, "demo", 1, "new") == 0);
	FILE *file = fopen(path, "r");
	char saved[128] = {0};
	assert(file && fread(saved, 1, sizeof(saved) - 1, file) > 0);
	assert(fclose(file) == 0);
	assert(strstr(saved, "old\nnew\n#end\n"));
	assert(script_file_update(path, "demo", 1, "#end") < 0);
	file = fopen(path, "r");
	char after[128] = {0};
	assert(file && fread(after, 1, sizeof(after) - 1, file) > 0);
	assert(fclose(file) == 0 && strcmp(saved, after) == 0);
	assert(symlink(path, alias) == 0);
	assert(script_file_update(alias, "demo", 1, "bad") < 0);
	assert(unlink(alias) == 0 && link(path, alias) == 0);
	assert(script_file_update(path, "demo", 1, "bad") < 0);
	assert(unlink(alias) == 0);
	assert(mkfifo(alias, 0600) == 0);
	assert(script_file_update(alias, "demo", 1, "bad") < 0);
	assert(unlink(alias) == 0 && unlink(path) == 0);
	assert(rmdir(directory) == 0);
}

int main(void)
{
	test_section_updates();
	test_malformed_and_limits();
	test_file_validation();
	return 0;
}
