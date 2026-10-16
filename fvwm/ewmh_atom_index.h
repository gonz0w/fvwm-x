#ifndef FVWM_EWMH_ATOM_INDEX_H
#define FVWM_EWMH_ATOM_INDEX_H

#include <stddef.h>
#include <X11/Xlib.h>

typedef struct
{
	Atom atom;
	size_t order;
	void *value;
} ewmh_atom_index_entry;

/* Equal atoms retain the original table's first-entry priority. */
void ewmh_atom_index_sort(ewmh_atom_index_entry *entries, size_t count);
void *ewmh_atom_index_find(const ewmh_atom_index_entry *entries, size_t count,
	Atom atom);

#endif
