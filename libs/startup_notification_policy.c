/* This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */
#include "config.h"
#include "startup_notification_policy.h"

#include <limits.h>
#include <string.h>

_Static_assert(sizeof(struct startup_notification_state) <= 512 * 1024,
	"startup observation storage must remain bounded");

enum startup_fields {
	FIELD_ID = 1, FIELD_NAME = 2, FIELD_ICON = 4, FIELD_APPLICATION = 8,
	FIELD_CLASS = 16, FIELD_SCREEN = 32
};

struct startup_message {
	unsigned int fields;
	unsigned int screen;
	int operation;
	char id[STARTUP_TEXT_MAX + 1];
	char name[STARTUP_TEXT_MAX + 1];
	char icon[STARTUP_TEXT_MAX + 1];
	char application_id[STARTUP_TEXT_MAX + 1];
	char wmclass[STARTUP_TEXT_MAX + 1];
};

static uint64_t deadline(uint64_t now, uint64_t delay)
{
	return now > UINT64_MAX - delay ? UINT64_MAX : now + delay;
}

static void changed(struct startup_notification_state *state)
{
	/* Zero permanently refuses revision-bound queries after wrap. */
	if (state->generation)
		state->generation++;
}

static void count(struct startup_notification_state *state, uint64_t *counter)
{
	if (*counter != UINT64_MAX)
		(*counter)++;
	changed(state);
}

static bool utf8(const unsigned char *text)
{
	while (*text)
	{
		uint32_t value, minimum;
		unsigned int following;
		unsigned char first = *text++;

		if (first < 0x80)
			continue;
		if (first >= 0xc2 && first <= 0xdf)
			value = first & 0x1f, minimum = 0x80, following = 1;
		else if (first >= 0xe0 && first <= 0xef)
			value = first & 0x0f, minimum = 0x800, following = 2;
		else if (first >= 0xf0 && first <= 0xf4)
			value = first & 7, minimum = 0x10000, following = 3;
		else
			return false;
		for (unsigned int i = 0; i < following; i++)
		{
			if ((*text & 0xc0) != 0x80)
				return false;
			value = (value << 6) | (*text++ & 0x3f);
		}
		if (value < minimum || value > 0x10ffff ||
		    (value >= 0xd800 && value <= 0xdfff))
			return false;
	}
	return true;
}

bool startup_notification_text_valid(const char *text, bool nonempty)
{
	if (!text || (nonempty && !*text) ||
	    strnlen(text, STARTUP_TEXT_MAX + 1) > STARTUP_TEXT_MAX)
		return false;
	for (const unsigned char *p = (const unsigned char *)text; *p; p++)
		if (*p < 0x20 || *p == 0x7f)
			return false;
	return utf8((const unsigned char *)text);
}

static bool screen_number(const char *text, unsigned int *number)
{
	unsigned int value = 0;

	if (!*text)
		return false;
	for (; *text; text++)
	{
		if (*text < '0' || *text > '9' ||
		    value > (UINT_MAX - (unsigned int)(*text - '0')) / 10)
			return false;
		value = value * 10 + (unsigned int)(*text - '0');
	}
	*number = value;
	return true;
}

static bool parse(const char *text, struct startup_message *message)
{
	memset(message, 0, sizeof(*message));
	if (!utf8((const unsigned char *)text))
		return false;
	if (strncmp(text, "new:", 4) == 0)
		message->operation = 1, text += 4;
	else if (strncmp(text, "change:", 7) == 0)
		message->operation = 2, text += 7;
	else if (strncmp(text, "remove:", 7) == 0)
		message->operation = 3, text += 7;
	else
		return false;
	while (*text)
	{
		const char *key;
		size_t key_length, length = 0;
		char value[STARTUP_MESSAGE_MAX];
		char *target = NULL;
		unsigned int field = 0;
		bool quoted = false;

		while (*text == ' ')
			text++;
		if (!*text)
			break;
		key = text;
		/* Keys are bounded by the UTF-8-validated message. Interpret
		 * known names exactly and safely skip UTF-8 extension names. */
		while (*text && *text != '=' && *text != ' ')
		{
			unsigned char c = (unsigned char)*text;

			if (c < 0x21 || c == 0x7f || c == '"' || c == '\\')
				return false;
			text++;
		}
		key_length = (size_t)(text - key);
		if (!key_length || *text++ != '=')
			return false;
		while (*text && (*text != ' ' || quoted))
		{
			char c = *text++;

			if (c == '"')
			{
				quoted = !quoted;
				continue;
			}
			if (c == '\\')
			{
				if (!*text)
					return false;
				c = *text++;
			}
			if (length + 1 >= sizeof(value))
				return false;
			value[length++] = c;
		}
		if (quoted)
			return false;
		value[length] = '\0';
#define STARTUP_KEY(key_name, flag, member) \
		if (key_length == sizeof(key_name) - 1 && \
		    memcmp(key, key_name, key_length) == 0) \
			field = flag, target = message->member
		STARTUP_KEY("ID", FIELD_ID, id);
		else STARTUP_KEY("NAME", FIELD_NAME, name);
		else STARTUP_KEY("ICON", FIELD_ICON, icon);
		else STARTUP_KEY("APPLICATION_ID", FIELD_APPLICATION,
			application_id);
		else STARTUP_KEY("WMCLASS", FIELD_CLASS, wmclass);
		else if (key_length == 6 && memcmp(key, "SCREEN", 6) == 0)
			field = FIELD_SCREEN;
#undef STARTUP_KEY
		if (field && (message->fields & field))
			return false;
		message->fields |= field;
		if (target)
		{
			if (!startup_notification_text_valid(value,
				field == FIELD_ID || field == FIELD_NAME))
				return false;
			memcpy(target, value, length + 1);
		}
		else if (field == FIELD_SCREEN &&
		    !screen_number(value, &message->screen))
			return false;
	}
	return (message->fields & FIELD_ID) &&
		(message->operation != 1 ||
		(message->fields & (FIELD_NAME | FIELD_SCREEN)) ==
		(FIELD_NAME | FIELD_SCREEN));
}

void startup_notification_init(struct startup_notification_state *state,
	unsigned int screen)
{
	memset(state, 0, sizeof(*state));
	state->generation = 1;
	state->screen = screen;
}

void startup_notification_expire(struct startup_notification_state *state,
	uint64_t now_ms)
{
	for (unsigned int i = 0; i < STARTUP_STREAM_MAX; i++)
		if (state->streams[i].producer &&
		    now_ms >= state->streams[i].deadline_ms)
		{
			state->streams[i].producer = 0;
			count(state, &state->partial_expired);
		}
	for (unsigned int i = 0; i < STARTUP_RECORD_MAX; i++)
	{
		struct startup_record *record = state->records + i;

		if (record->state == STARTUP_UNUSED ||
		    now_ms < record->deadline_ms)
			continue;
		if (record->state == STARTUP_PENDING)
		{
			record->state = STARTUP_EXPIRED;
			record->deadline_ms = deadline(
				now_ms, STARTUP_RETENTION_MS);
		}
		else
			memset(record, 0, sizeof(*record));
		changed(state);
	}
}

static void apply(struct startup_notification_state *state,
	const char *text, uint64_t now_ms)
{
	struct startup_message message;
	struct startup_record *record = NULL, *empty = NULL;

	if (!parse(text, &message) ||
	    ((message.fields & FIELD_SCREEN) &&
	    message.screen != state->screen))
	{
		count(state, &state->malformed);
		return;
	}
	for (unsigned int i = 0; i < STARTUP_RECORD_MAX; i++)
	{
		struct startup_record *candidate = state->records + i;

		if (candidate->state == STARTUP_UNUSED)
		{
			if (!empty)
				empty = candidate;
		}
		else if (strcmp(candidate->id, message.id) == 0)
			record = candidate;
	}
	if (!record)
	{
		if (message.operation == 3)
			return;
		if (!empty)
		{
			count(state, &state->dropped);
			return;
		}
		record = empty;
		memcpy(record->id, message.id, sizeof(record->id));
		record->state = STARTUP_ORPHAN;
		record->started_ms = now_ms;
		record->deadline_ms = deadline(
			now_ms, STARTUP_RETENTION_MS);
	}
	if (record->state != STARTUP_PENDING && record->state != STARTUP_ORPHAN)
		return;
	if (message.operation == 3)
	{
		record->state = STARTUP_PROTOCOL_ENDED;
		record->deadline_ms = deadline(
			now_ms, STARTUP_RETENTION_MS);
		changed(state);
		return;
	}
	if (message.operation == 1 && record->state == STARTUP_ORPHAN)
	{
		record->visible = true;
		record->state = STARTUP_PENDING;
		record->started_ms = now_ms;
		record->deadline_ms = deadline(now_ms, STARTUP_PENDING_MS);
	}
#define STARTUP_COPY(flag, member) \
	if (message.fields & flag) \
		memcpy(record->member, message.member, sizeof(record->member))
	STARTUP_COPY(FIELD_NAME, name);
	STARTUP_COPY(FIELD_ICON, icon);
	STARTUP_COPY(FIELD_APPLICATION, application_id);
	STARTUP_COPY(FIELD_CLASS, wmclass);
#undef STARTUP_COPY
	changed(state);
}

void startup_notification_feed(struct startup_notification_state *state,
	uint64_t producer, uint64_t root, bool begin,
	const unsigned char fragment[20], uint64_t now_ms)
{
	struct startup_stream *stream = NULL, *empty = NULL;

	startup_notification_expire(state, now_ms);
	if (!producer || !root || !fragment)
	{
		count(state, &state->malformed);
		return;
	}
	for (unsigned int i = 0; i < STARTUP_STREAM_MAX; i++)
	{
		struct startup_stream *candidate = state->streams + i;

		if (!candidate->producer)
		{
			if (!empty)
				empty = candidate;
		}
		else if (candidate->producer == producer &&
		    candidate->root == root)
			stream = candidate;
	}
	if (begin)
	{
		if (!stream)
			stream = empty;
		if (!stream)
		{
			count(state, &state->dropped);
			return;
		}
		stream->producer = producer;
		stream->root = root;
		stream->length = 0;
		stream->deadline_ms = deadline(now_ms, STARTUP_PARTIAL_MS);
	}
	if (!stream)
	{
		count(state, &state->malformed);
		return;
	}
	for (unsigned int i = 0; i < 20; i++)
	{
		if (!fragment[i])
		{
			stream->message[stream->length] = '\0';
			apply(state, stream->message, now_ms);
			stream->producer = 0;
			return;
		}
		if (stream->length + 1 >= STARTUP_MESSAGE_MAX)
		{
			stream->producer = 0;
			count(state, &state->malformed);
			return;
		}
		stream->message[stream->length++] = (char)fragment[i];
	}
}

int startup_notification_timeout_ms(
	const struct startup_notification_state *state, uint64_t now_ms)
{
	uint64_t next = UINT64_MAX;
	bool found = false;

	for (unsigned int i = 0; i < STARTUP_STREAM_MAX; i++)
		if (state->streams[i].producer &&
		    state->streams[i].deadline_ms <= next)
			next = state->streams[i].deadline_ms, found = true;
	for (unsigned int i = 0; i < STARTUP_RECORD_MAX; i++)
		if (state->records[i].state != STARTUP_UNUSED &&
		    state->records[i].deadline_ms <= next)
			next = state->records[i].deadline_ms, found = true;
	if (!found)
		return -1;
	if (next <= now_ms)
		return 0;
	return next - now_ms > INT_MAX ? INT_MAX : (int)(next - now_ms);
}

bool startup_notification_pending(
	const struct startup_notification_state *state)
{
	for (unsigned int i = 0; i < STARTUP_RECORD_MAX; i++)
		if (state->records[i].state == STARTUP_PENDING)
			return true;
	return false;
}

static bool latin1_equal(const char *utf8_text, const char *latin1)
{
	const unsigned char *s = (const unsigned char *)utf8_text;
	const unsigned char *l = (const unsigned char *)latin1;

	if (!l || strnlen(latin1, STARTUP_TEXT_MAX + 1) > STARTUP_TEXT_MAX)
		return false;
	for (; *l; l++)
	{
		if (*l < 0x80)
		{
			if (*s++ != *l)
				return false;
		}
		else
		{
			if (*s++ != (0xc0 | (*l >> 6)) ||
			    *s++ != (0x80 | (*l & 0x3f)))
				return false;
		}
	}
	return !*s;
}

const char *startup_notification_observe(
	struct startup_notification_state *state,
	const char *id, bool id_present, const char *res_name,
	const char *res_class, uint64_t window, uint64_t now_ms)
{
	struct startup_record *match = NULL;

	startup_notification_expire(state, now_ms);
	if (!window ||
	    (id_present && !startup_notification_text_valid(id, true)))
		return NULL;
	for (unsigned int i = 0; i < STARTUP_RECORD_MAX; i++)
	{
		struct startup_record *record = state->records + i;

		if (record->state != STARTUP_PENDING)
			continue;
		if (id_present)
		{
			if (strcmp(record->id, id) == 0)
				match = record;
		}
		else if (record->wmclass[0] &&
		    (latin1_equal(record->wmclass, res_name) ||
		    latin1_equal(record->wmclass, res_class)))
		{
			if (match)
				return NULL;
			match = record;
		}
	}
	if (match)
	{
		match->state = STARTUP_WINDOW_OBSERVED;
		match->window = window;
		match->deadline_ms = deadline(now_ms, STARTUP_RETENTION_MS);
		changed(state);
		return match->id;
	}
	return NULL;
}

const char *startup_notification_state_name(enum startup_activity_state state)
{
	switch (state)
	{
	case STARTUP_PENDING: return "pending";
	case STARTUP_PROTOCOL_ENDED: return "protocol-ended";
	case STARTUP_WINDOW_OBSERVED: return "window-observed";
	case STARTUP_EXPIRED: return "expired";
	default: return "unavailable";
	}
}
