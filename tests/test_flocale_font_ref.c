/* This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */
#include "config.h"
#include "libs/flocale_font_types.h"
#include <assert.h>
#include <limits.h>
#include <sys/wait.h>
#include <unistd.h>

static void rejected_count(int count)
{
	pid_t child = fork();
	int status = 0;

	assert(child >= 0);
	if (child == 0)
	{
		FlocaleFont font = {0};

		font.count = count;
		FlocaleRetainFont(&font);
		_exit(0);
	}
	assert(waitpid(child, &status, 0) == child);
	assert(WIFEXITED(status) && WEXITSTATUS(status) == 1);
}

int main(void)
{
	FlocaleFont font = {0};
	FlocaleFont *item, *title, *replacement;

	assert(FlocaleRetainFont(NULL) == NULL);
	FlocaleUnloadFont(NULL, NULL);
	font.count = 1; /* The external owner survives both popup slots. */
	item = FlocaleRetainFont(&font);
	title = FlocaleRetainFont(&font);
	assert(item == &font && title == item && font.count == 3);
	/* A cache hit/fallback can return the very same owner. Acquire the
	 * replacement before dropping the previous slot; the count balances. */
	replacement = FlocaleRetainFont(item);
	assert(font.count == 4);
	FlocaleUnloadFont(NULL, item);
	item = replacement;
	assert(font.count == 3);
	FlocaleUnloadFont(NULL, item);
	assert(font.count == 2);
	FlocaleUnloadFont(NULL, title);
	assert(font.count == 1);
	font.count = INT_MAX - 1;
	assert(FlocaleRetainFont(&font) == &font && font.count == INT_MAX);
	FlocaleUnloadFont(NULL, &font);
	assert(font.count == INT_MAX - 1);
	rejected_count(0);
	rejected_count(-1);
	rejected_count(INT_MAX);
	return 0;
}
