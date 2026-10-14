/* This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or any later version.
 */
#include "config.h"
#include <assert.h>
#include <stddef.h>
#include <stdlib.h>
#include "libs/Parse.h"

struct token_entry
{
	char *name;
	int value;
};

int main(void)
{
	char *words[] = {"Alpha", "Beta", "Gamma"};
	struct token_entry records[] = {
		{"Alpha", 1}, {"Beta", 2}, {"Gamma", 3}
	};
	struct token_entry *found;
	char **word;
	char *empty = NULL;
	char *high = "\xff";
	int (*compare)(const void *, const void *) = XCmpToken;

	word = FindToken("bETA", words, char *);
	assert(word == &words[1]);
	found = FindToken("Beta tail", records, struct token_entry);
	assert(found == &records[1]);
	assert(found->value == 2);
	found = FindToken("Gamma: value", records, struct token_entry);
	assert(found == &records[2]);
	assert(FindToken("Betamax", words, char *) == NULL);
	assert(FindToken("Unknown", records, struct token_entry) == NULL);
	assert(compare(NULL, &words[0]) < 0);
	assert(compare("Alpha", &empty) > 0);
	assert(compare("\xff", &high) == 0);
	assert(compare("\xfe", &high) != 0);
	assert(FindToken("\xff", words, char *) == NULL);
	return 0;
}
