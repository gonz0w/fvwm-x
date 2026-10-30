/* This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or any later version.
 */
#include "config.h"
#include "fvwm/session_clients_policy.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static const char token[] =
	"000000000000000000000000000000000000000000000001";

static void test_grammar(void)
{
	struct session_clients_request request;
	const char *invalid[] = {NULL, "", "a 1 delete - -",
		"a 0000000000000001 delete - -",
		"a 0000000000000001 quit-if-empty - -",
		"a 0000000000000001 delegate-xsmp - - extra",
		"a 0000000000000001 Close - -",
		"a;Exec 0000000000000001 delegate-xsmp - -",
		"a 0000000000000001 delegate-xsmp target -"};
	char text[256];

	snprintf(text, sizeof(text), "abc 0000000000000001 delete %s -", token);
	assert(session_clients_parse(text, &request));
	assert(request.operation == SESSION_CLIENTS_DELETE);
	assert(strcmp(request.target, token) == 0);
	snprintf(text, sizeof(text), "abc 0000000000000002 quit-if-empty - %s",
		token);
	assert(session_clients_parse(text, &request));
	assert(request.operation == SESSION_CLIENTS_QUIT_IF_EMPTY);
	assert(strcmp(request.generation, token) == 0);
	assert(session_clients_parse("abc 0000000000000003 delegate-xsmp - -",
		&request));
	assert(request.operation == SESSION_CLIENTS_DELEGATE_XSMP);
	for (size_t i = 0; i < sizeof(invalid) / sizeof(invalid[0]); i++)
	{
		assert(!session_clients_parse(invalid[i], &request));
		assert(!request.cookie[0] && !request.target[0]);
	}
	snprintf(text, sizeof(text), "abc 0000000000000001 delete %s %s",
		token, token);
	assert(!session_clients_parse(text, &request));
}

static void test_membership(void)
{
	struct session_clients_membership state = {.revision = 1};
	uint64_t identities[SESSION_CLIENTS_MAX + 1];
	uint64_t first;

	assert(session_clients_membership_update(&state, NULL, 0));
	assert(state.revision == 1);
	for (size_t i = 0; i < SESSION_CLIENTS_MAX + 1; i++)
		identities[i] = i + 1;
	assert(session_clients_membership_update(&state, identities, 2));
	first = state.revision;
	assert(session_clients_membership_update(&state, identities, 2));
	assert(state.revision == first);
	/* Same cardinality, different allocation identity. */
	identities[1] = 999;
	assert(session_clients_membership_update(&state, identities, 2));
	assert(state.revision != first && state.identities[1] == 999);
	assert(session_clients_membership_update(&state, identities,
		SESSION_CLIENTS_MAX));
	first = state.revision;
	assert(!session_clients_membership_update(&state, identities,
		SESSION_CLIENTS_MAX + 1));
	assert(state.revision == first && state.count == SESSION_CLIENTS_MAX);
	assert(!session_clients_membership_update(&state, NULL, 1));
	identities[0] = 0;
	assert(!session_clients_membership_update(&state, identities, 1));
	assert(session_clients_membership_update(&state, NULL, 0));
	assert(state.count == 0 && state.revision != first);
	state.revision = UINT64_MAX;
	identities[0] = 1;
	assert(!session_clients_membership_update(&state, identities, 1));
	assert(!state.revision);
	assert(!session_clients_membership_update(&state, NULL, 0));
}

static void test_titles(void)
{
	char title[SESSION_CLIENTS_TITLE_MAX + 1], input[256];
	size_t encoded = 0;

	session_clients_title(title, NULL);
	assert(strcmp(title, "Unnamed application") == 0);
	session_clients_title(title, "a\nb\177c");
	assert(strcmp(title, "a?b?c") == 0);
	memset(input, '"', sizeof(input) - 1);
	input[sizeof(input) - 1] = '\0';
	session_clients_title(title, input);
	for (size_t i = 0; title[i]; i++)
		encoded += title[i] == '"' || title[i] == '\\' ? 2 : 1;
	assert(encoded <= SESSION_CLIENTS_TITLE_MAX);
	assert(strcmp(title + strlen(title) - 3, "...") == 0);
	memset(input, 'a', 76);
	memcpy(input + 76, "\342\202\254xxxx", 8);
	input[84] = '\0';
	session_clients_title(title, input);
	assert(strlen(title) == 79);
	assert(strcmp(title + 76, "...") == 0);
}

int main(void)
{
	test_grammar();
	test_membership();
	test_titles();
	return 0;
}
