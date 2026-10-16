/* This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */
#include "config.h"

#include <stdlib.h>
#include "ewmh_atom_index.h"

static int compare_entries(const void *left, const void *right)
{
	const ewmh_atom_index_entry *a = left, *b = right;

	if (a->atom != b->atom)
		return a->atom < b->atom ? -1 : 1;
	return (a->order > b->order) - (a->order < b->order);
}

static int compare_atom(const void *key, const void *value)
{
	Atom atom = *(const Atom *)key;
	const ewmh_atom_index_entry *entry = value;

	return (atom > entry->atom) - (atom < entry->atom);
}

void ewmh_atom_index_sort(ewmh_atom_index_entry *entries, size_t count)
{
	if (count > 1)
		qsort(entries, count, sizeof(*entries), compare_entries);
}

void *ewmh_atom_index_find(const ewmh_atom_index_entry *entries, size_t count,
	Atom atom)
{
	const ewmh_atom_index_entry *entry;

	if (count == 0)
		return NULL;
	entry = bsearch(&atom, entries, count, sizeof(*entries), compare_atom);
	if (entry == NULL)
		return NULL;
	/* bsearch may return any equal element. Restore the original priority,
	 * including None and duplicate identities from failed interning. */
	while (entry != entries && entry[-1].atom == atom)
		entry--;
	return entry->value;
}
