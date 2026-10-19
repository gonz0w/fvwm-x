/* This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */
#include "config.h"

#include <stdlib.h>
#include <string.h>
#include "ewmh_atom_index.h"

static int compare_entries(const void *left, const void *right)
{
	const ewmh_atom_index_entry *a = left, *b = right;

	if (a->atom != b->atom)
		return a->atom < b->atom ? -1 : 1;
	return (a->order > b->order) - (a->order < b->order);
}

void ewmh_atom_index_sort(ewmh_atom_index_entry *entries, size_t count)
{
	if (count > 1)
		qsort(entries, count, sizeof(*entries), compare_entries);
}

void *ewmh_atom_index_find(const ewmh_atom_index_entry *entries, size_t count,
	Atom atom)
{
	size_t low = 0, high = count;

	/* Lower bound retains first-entry priority without a duplicate walk,
	 * including None identities from failed interning. */
	while (low < high)
	{
		size_t middle = low + (high - low) / 2;

		if (entries[middle].atom < atom)
			low = middle + 1;
		else
			high = middle;
	}
	return low < count && entries[low].atom == atom ?
		entries[low].value : NULL;
}

static int compare_names(const void *left, const void *right)
{
	const ewmh_atom_name_entry *a = left, *b = right;
	int result = strcmp(a->name, b->name);

	return result ? result :
		(a->order > b->order) - (a->order < b->order);
}

void ewmh_atom_name_sort(ewmh_atom_name_entry *entries, size_t count)
{
	if (count > 1)
		qsort(entries, count, sizeof(*entries), compare_names);
}

void *ewmh_atom_name_find(const ewmh_atom_name_entry *entries, size_t count,
	const char *name)
{
	size_t low = 0, high = count;

	while (low < high)
	{
		size_t middle = low + (high - low) / 2;

		if (strcmp(entries[middle].name, name) < 0)
			low = middle + 1;
		else
			high = middle;
	}
	return low < count && strcmp(entries[low].name, name) == 0 ?
		entries[low].value : NULL;
}
