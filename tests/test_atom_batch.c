/* This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */
#include "config.h"

#include <assert.h>
#include <string.h>
#include "libs/atom_batch.h"

static int batch_calls, single_calls;
static Bool partial_failure, single_failure;

Status __wrap_XInternAtoms(Display *display, char **names, int count,
	Bool only_if_exists, Atom *atoms)
{
	assert(display == NULL && count == 3 && !only_if_exists);
	assert(strcmp(names[0], "first") == 0);
	for (int i = 0; i < count; i++)
	{
		assert(atoms[i] == None);
		atoms[i] = partial_failure && i != 1 ? None : 100 + i;
	}
	batch_calls++;
	return !partial_failure;
}

Atom __wrap_XInternAtom(Display *display, const char *name, Bool only_if_exists)
{
	assert(display == NULL && !only_if_exists);
	assert(strcmp(name, "first") == 0 || strcmp(name, "last") == 0);
	single_calls++;
	if (single_failure && strcmp(name, "last") == 0)
	{
		return None;
	}
	return strcmp(name, "first") == 0 ? 100 : 102;
}

static void test_batch_and_partial_failure(void)
{
	char *names[] = { "first", "middle", "last" };
	Atom atoms[3] = { 999, 999, 999 };

	assert(atom_batch_intern(NULL, names, 3, atoms));
	assert(batch_calls == 1 && single_calls == 0);
	for (int i = 0; i < 3; i++)
	{
		assert(atoms[i] == 100 + i);
	}
	partial_failure = True;
	assert(atom_batch_intern(NULL, names, 3, atoms));
	assert(batch_calls == 2 && single_calls == 2);
	for (int i = 0; i < 3; i++)
	{
		assert(atoms[i] == 100 + i);
	}
	single_failure = True;
	assert(!atom_batch_intern(NULL, names, 3, atoms));
	assert(batch_calls == 3 && single_calls == 4);
	assert(atoms[0] == 100 && atoms[1] == 101 && atoms[2] == None);
}

int main(void)
{
	assert(atom_batch_intern(NULL, NULL, 0, NULL));
	assert(!atom_batch_intern(NULL, NULL, -1, NULL));
	assert(batch_calls == 0 && single_calls == 0);
	test_batch_and_partial_failure();
	return 0;
}
