/* This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */
#include "config.h"
#include "libs/startup_notification_policy.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static struct startup_notification_state state;

static void message(uint64_t sender, const char *text, uint64_t now)
{
	size_t length = strlen(text) + 1;

	for (size_t offset = 0; offset < length; offset += 20)
	{
		unsigned char fragment[20] = {0};
		size_t n = length - offset < 20 ? length - offset : 20;

		memcpy(fragment, text + offset, n);
		startup_notification_feed(&state, sender, 1, offset == 0,
			fragment, now);
	}
}

static struct startup_record *find(const char *id)
{
	for (unsigned int i = 0; i < STARTUP_RECORD_MAX; i++)
		if (state.records[i].state != STARTUP_UNUSED &&
		    strcmp(state.records[i].id, id) == 0)
			return state.records + i;
	return NULL;
}

int main(void)
{
	char text[512];
	struct startup_record *r;
	uint64_t revision;
	unsigned char fragment[20];

	assert(sizeof(state) <= 512 * 1024);
	startup_notification_init(&state, 0);
	message(1, "new: ID=one "
		"NAME=\"First \\\"quoted\\\" app\" SCREEN=0", 100);
	r = find("one");
	assert(r && r->visible && r->state == STARTUP_PENDING);
	assert(strcmp(r->name, "First \"quoted\" app") == 0);
	assert(startup_notification_timeout_ms(&state, 100) == 60000);
	revision = state.generation;
	startup_notification_expire(&state, 59999);
	assert(state.generation == revision && r->state == STARTUP_PENDING);
	message(2, "change: ID=one NAME=Changed", 60000);
	assert(strcmp(r->name, "Changed") == 0 && r->deadline_ms == 60100);
	startup_notification_expire(&state, 60100);
	assert(r->state == STARTUP_EXPIRED);
	message(3, "new: ID=one NAME=Late SCREEN=0", 60101);
	assert(r->state == STARTUP_EXPIRED);
	startup_notification_expire(&state, 120100);
	assert(find("one") == NULL);

	message(1, "change: ID=orphan ICON=test", 200000);
	r = find("orphan");
	assert(r && !r->visible && r->state == STARTUP_ORPHAN);
	message(2, "new: ID=orphan NAME=Known SCREEN=0", 200100);
	assert(r->visible && strcmp(r->icon, "test") == 0);
	message(3, "remove: ID=orphan", 200200);
	assert(r->state == STARTUP_PROTOCOL_ENDED);
	message(4, "change: ID=orphan NAME=Late", 200300);
	assert(strcmp(r->name, "Known") == 0);
	message(4, "remove: ID=unknown", 200300);
	assert(find("unknown") == NULL);

	startup_notification_init(&state, 0);
	message(1, "new: ID=class1 NAME=One SCREEN=0 WMCLASS=Example", 0);
	message(2, "new: ID=class2 NAME=Two SCREEN=0 WMCLASS=Example", 0);
	assert(startup_notification_observe(&state, NULL, false,
		"example", "Example", 42, 1) == NULL);
	assert(find("class1")->state == STARTUP_PENDING);
	assert(find("class2")->state == STARTUP_PENDING);
	assert(strcmp(startup_notification_observe(&state, "class2", true,
		"example", "Example", 42, 2), "class2") == 0);
	assert(find("class2")->state == STARTUP_WINDOW_OBSERVED);
	assert(find("class2")->window == 42);
	startup_notification_observe(&state, NULL, true,
		"example", "Example", 43, 3);
	assert(find("class1")->state == STARTUP_PENDING);
	assert(strcmp(startup_notification_observe(&state, NULL, false,
		"example", "Example", 44, 4), "class1") == 0);
	assert(find("class1")->state == STARTUP_WINDOW_OBSERVED);

	startup_notification_init(&state, 0);
	memset(fragment, 'x', sizeof(fragment));
	startup_notification_feed(&state, 9, 1, true, fragment, 10);
	assert(startup_notification_timeout_ms(&state, 10) == 5000);
	startup_notification_expire(&state, 5010);
	assert(state.partial_expired == 1);
	assert(startup_notification_timeout_ms(&state, 5010) == -1);
	startup_notification_feed(&state, 9, 1, false, fragment, 5011);
	assert(state.malformed == 1);
	message(1, "new: ID=missing NAME=NoScreen", 5012);
	message(1, "new: ID=screen NAME=WrongScreen SCREEN=1", 5012);
	message(1, "new: ID=bad NAME=\"unclosed SCREEN=0", 5012);
	message(1, "new: ID=bad NAME=x SCREEN=0 ID=duplicate", 5012);
	assert(find("missing") == NULL && find("screen") == NULL);
	assert(find("bad") == NULL);
	assert(!startup_notification_text_valid("\xc0\xaf", false));
	assert(!startup_notification_text_valid("\xed\xa0\x80", false));
	assert(startup_notification_text_valid("Caf\xc3\xa9", true));

	startup_notification_init(&state, 0);
	for (unsigned int i = 0; i < STARTUP_RECORD_MAX + 1; i++)
	{
		snprintf(text, sizeof(text),
			"new: ID=capacity%u NAME=Test SCREEN=0", i);
		message(i + 1, text, 0);
	}
	assert(state.dropped == 1 && find("capacity0"));
	assert(find("capacity64") == NULL);
	assert(startup_notification_pending(&state));

	/* Fragments from one producer on separate root contexts never mix. */
	startup_notification_init(&state, 0);
	const char *a = "new: ID=interleave-a NAME=First SCREEN=0";
	const char *b = "new: ID=interleave-b NAME=Second SCREEN=0";

	memcpy(fragment, a, 20);
	startup_notification_feed(&state, 7, 1, true, fragment, 100);
	memcpy(fragment, b, 20);
	startup_notification_feed(&state, 7, 2, true, fragment, 100);
	for (size_t offset = 20; offset <= strlen(a); offset += 20)
	{
		size_t n = strlen(a) + 1 - offset;

		memset(fragment, 0, 20);
		memcpy(fragment, a + offset, n < 20 ? n : 20);
		startup_notification_feed(&state, 7, 1, false, fragment, 101);
	}
	for (size_t offset = 20; offset <= strlen(b); offset += 20)
	{
		size_t n = strlen(b) + 1 - offset;

		memset(fragment, 0, 20);
		memcpy(fragment, b + offset, n < 20 ? n : 20);
		startup_notification_feed(&state, 7, 2, false, fragment, 102);
	}
	assert(find("interleave-a") && find("interleave-b"));
	message(9, "new: ID=interleave-a NAME=Updated SCREEN=0", 1000);
	assert(find("interleave-a")->deadline_ms == 60101);
	message(9, "remove: ID=interleave-a", 1001);
	assert(find("interleave-a")->state == STARTUP_PROTOCOL_ENDED);
	startup_notification_expire(&state, 61001);
	assert(!find("interleave-a"));

	/* Protocol strings are UTF-8; cached ICCCM classes are Latin-1. */
	startup_notification_init(&state, 0);
	message(1, "new: ID=latin NAME=Cafe SCREEN=0 WMCLASS=Caf\xc3\xa9", 0);
	startup_notification_observe(&state, NULL, false, NULL,
		"Caf\xe9", 77, 1);
	assert(find("latin")->state == STARTUP_WINDOW_OBSERVED);

	/* Unknown bounded fields are ignored; oversized known fields refuse. */
	startup_notification_init(&state, 0);
	char large[STARTUP_MESSAGE_MAX + 32];

	strcpy(large, "new: ID=unknown-field NAME=Known SCREEN=0 FUTURE=");
	memset(large + strlen(large), 'x', 300);
	large[strlen("new: ID=unknown-field NAME=Known SCREEN=0 FUTURE=") +
		300] = 0;
	message(1, large, 0);
	assert(find("unknown-field"));
	strcpy(large, "new: ID=large-name SCREEN=0 NAME=");
	memset(large + strlen(large), 'x', STARTUP_TEXT_MAX + 1);
	large[strlen("new: ID=large-name SCREEN=0 NAME=") +
		STARTUP_TEXT_MAX + 1] = 0;
	message(2, large, 0);
	assert(!find("large-name"));
	memset(large, 'x', STARTUP_MESSAGE_MAX);
	large[STARTUP_MESSAGE_MAX] = 0;
	message(3, large, 0);
	assert(state.malformed >= 2);
	/* A valid short final fragment ignores all trailing wire padding. */
	message(4, "new: ID=pad NAME=Padding SCREEN=0", 0);
	memset(fragment, 'z', 20);
	memcpy(fragment, "remove: ID=pad", sizeof("remove: ID=pad"));
	startup_notification_feed(&state, 4, 1, true, fragment, 1);
	assert(find("pad")->state == STARTUP_PROTOCOL_ENDED);

	startup_notification_init(&state, 0);
	memset(fragment, 'x', 20);
	for (unsigned int i = 0; i <= STARTUP_STREAM_MAX; i++)
		startup_notification_feed(&state, i + 1, 1, true, fragment, 0);
	assert(state.dropped == 1);
	startup_notification_expire(&state, STARTUP_PARTIAL_MS);
	assert(state.partial_expired == STARTUP_STREAM_MAX);
	assert(startup_notification_timeout_ms(
		&state, STARTUP_PARTIAL_MS) == -1);

	/* Unknown extension keys do not change known-key interpretation. The
	 * colon separator permits zero spaces before the first key. */
	startup_notification_init(&state, 0);
	message(1, "new:ID=extensions NAME=App SCREEN=0 "
		"X-vendor=\"future value\" lower_case=ignored "
		"id=not-the-ID", 0);
	assert(find("extensions") && !find("not-the-ID"));
	assert(strcmp(find("extensions")->name, "App") == 0);
	message(2, "change:ID=extensions X-test=yes NAME=Changed", 1);
	assert(strcmp(find("extensions")->name, "Changed") == 0);
	message(3, "remove:ID=extensions lower=ignored", 2);
	assert(find("extensions")->state == STARTUP_PROTOCOL_ENDED);
	message(4, "new:ID=bad-key NAME=App SCREEN=0 X-\tbad=value", 3);
	assert(!find("bad-key"));
	message(4, "new:ID=duplicate NAME=App SCREEN=0 "
		"X-vendor=yes ID=again", 3);
	assert(!find("duplicate") && !find("again"));

	/* UTF-8 extension names are ignored after whole-message validation. */
	startup_notification_init(&state, 0);
	message(1, "new:ID=utf8-key NAME=App SCREEN=0 "
		"X-\xc3\xa9=ignored", 0);
	assert(find("utf8-key"));
	assert(strcmp(find("utf8-key")->name, "App") == 0);
	message(2, "new:ID=bad-utf8-key NAME=App SCREEN=0 "
		"X-\xc0\xaf=ignored", 1);
	assert(!find("bad-utf8-key"));
	puts("startup notification policy: passed");
	return 0;
}
