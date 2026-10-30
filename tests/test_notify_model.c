/* This program is free software; you can redistribute it and/or modify it
 * under the GNU General Public License, version 2 or any later version. */
#include "config.h"
#include "modules/FvwmNotify/notify_model.h"
#include <assert.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

static notify_request request(void)
{
	notify_request value = {0};
	strcpy(value.record.owner, ":1.2");
	strcpy(value.record.app, "Test application");
	strcpy(value.record.summary, "A notification");
	strcpy(value.record.body, "Plain text\nsecond line\twith tab");
	strcpy(value.record.actions[0].key, "default");
	strcpy(value.record.actions[0].label, "Open");
	value.record.action_count = 1;
	value.record.urgency = 1;
	value.timeout = -1;
	return value;
}

static void text_boundaries(void)
{
	assert(notify_model_text("", 0, false));
	assert(!notify_model_text(NULL, 4, false));
	assert(notify_model_text("1234", 4, false));
	assert(!notify_model_text("12345", 4, false));
	assert(notify_model_text("\xc2\xa0\xe2\x82\xac\xf0\x9f\x92\xac", 9, false));
	assert(!notify_model_text("\xc0\xaf", 4, false));
	assert(!notify_model_text("\xed\xa0\x80", 4, false));
	assert(!notify_model_text("\xf4\x90\x80\x80", 4, false));
	assert(!notify_model_text("\xe2\x82", 4, false));
	assert(!notify_model_text("\x80", 4, false));
	assert(!notify_model_text("\xc2\x80", 4, false));
	assert(!notify_model_text("\n", 1, false));
	assert(notify_model_text("\n\t", 2, true));
	assert(!notify_model_text("\r", 1, true));
	assert(!notify_model_text("\x7f", 1, true));
}

static void atomic_replacement(notify_model *model, notify_model *before)
{
	notify_request value = request();
	uint32_t id = 99;
	assert(notify_model_put(model, &value, 10, &id) == NOTIFY_ACCEPTED);
	assert(id == 1 && model->count == 1 && model->active[0].expires == 15);
	value.replaces = id;
	strcpy(value.record.summary, "Updated atomically");
	assert(notify_model_put(model, &value, 12, &id) == NOTIFY_ACCEPTED);
	assert(id == 1 && model->count == 1 && model->active[0].expires == 17);
	assert(!strcmp(model->active[0].summary, "Updated atomically"));
	*before = *model;
	strcpy(value.record.owner, ":1.3");
	/* Standard IDs are server-global; a transient client may replace on
	 * a fresh connection, and subsequent actions target that new sender. */
	assert(notify_model_put(model, &value, 12, &id) == NOTIFY_ACCEPTED);
	assert(id == 1 && model->count == 1);
	assert(!strcmp(model->active[0].owner, ":1.3"));
	*before = *model;
	strcpy(value.record.owner, ":1.2");
	value.replaces = 999;
	assert(notify_model_put(model, &value, 12, &id) == NOTIFY_UNKNOWN_ID);
	assert(!memcmp(model, before, sizeof(*model)));
	value.replaces = 0;
	value.timeout = -2;
	assert(notify_model_put(model, &value, 12, &id) == NOTIFY_INVALID);
	assert(!memcmp(model, before, sizeof(*model)));
	value.timeout = -1;
	assert(notify_model_put(model, &value, NAN, &id) == NOTIFY_INVALID);
	assert(notify_model_put(model, &value, -1, &id) == NOTIFY_INVALID);
	assert(!memcmp(model, before, sizeof(*model)));
}

static void bounded_records(notify_model *model, notify_model *before)
{
	notify_request value = request();
	uint32_t id;
	memset(model, 0, sizeof(*model));
	for (unsigned int i = 0; i < NOTIFY_ACTIVE_MAX; i++)
	{
		assert(notify_model_put(model, &value, 1, &id) == NOTIFY_ACCEPTED);
		assert(id == i + 1);
	}
	*before = *model;
	assert(notify_model_put(model, &value, 1, &id) == NOTIFY_FULL);
	assert(!id && !memcmp(model, before, sizeof(*model)));
	/* A full table still accepts atomic replacement. */
	value.replaces = 16;
	assert(notify_model_put(model, &value, 2, &id) == NOTIFY_ACCEPTED);
	assert(id == 16 && model->count == NOTIFY_ACTIVE_MAX);
	assert(notify_model_close(model, 1, 2, NULL));
	value.replaces = 0;
	assert(notify_model_put(model, &value, 2, &id) == NOTIFY_ACCEPTED);
	assert(id == 33);
	memset(model, 0, sizeof(*model));
	model->next_id = UINT32_MAX;
	assert(notify_model_put(model, &value, 2, &id) == NOTIFY_ACCEPTED);
	assert(id == UINT32_MAX);
	assert(notify_model_close(model, id, 2, NULL));
	*before = *model;
	assert(notify_model_put(model, &value, 2, &id) == NOTIFY_EXHAUSTED);
	assert(!id && !memcmp(model, before, sizeof(*model)));
}

static void invalid_records(notify_model *model, notify_model *before)
{
	notify_request value;
	uint32_t id;
	memset(model, 0, sizeof(*model));
	*before = *model;
	for (unsigned int i = 0; i < 8; i++)
	{
		value = request();
		switch (i)
		{
		case 0: value.record.action_count = NOTIFY_ACTION_MAX + 1; break;
		case 1: value.record.urgency = 3; break;
		case 2: value.timeout = 86400001; break;
		case 3: value.record.owner[0] = 'x'; break;
		case 4: memset(value.record.body, 'x', sizeof(value.record.body)); break;
		case 5: strcpy(value.record.summary, "bad\nline"); break;
		case 6: value.record.actions[0].key[0] = 0; break;
		case 7:
			value.record.action_count = 2;
			value.record.actions[1] = value.record.actions[0];
			break;
		}
		assert(notify_model_put(model, &value, 1, &id) == NOTIFY_INVALID);
		assert(!id && !memcmp(model, before, sizeof(*model)));
	}
}

static void expiration_and_history(notify_model *model)
{
	notify_request value = request();
	notify_record closed;
	uint32_t ids[4];
	memset(model, 0, sizeof(*model));
	assert(!notify_model_deadline(model));
	assert(!notify_model_expired(model, 100));
	assert(notify_model_history_limit(model, 2));
	for (unsigned int i = 0; i < 4; i++)
	{
		value.record.urgency = i < 3 ? i : 1;
		value.timeout = i == 3 ? 0 : -1;
		assert(notify_model_put(model, &value, 10, &ids[i]) == NOTIFY_ACCEPTED);
	}
	assert(notify_model_deadline(model) == 13);
	assert(!notify_model_expired(model, 12.999));
	assert(notify_model_expired(model, 13) == ids[0]);
	assert(!notify_model_close(model, ids[0], 0, &closed));
	assert(!closed.id);
	assert(notify_model_close(model, ids[0], 1, &closed));
	assert(closed.id == ids[0] && !notify_model_find(model, ids[0]));
	assert(!notify_model_close(model, ids[0], 1, NULL));
	assert(notify_model_expired(model, 15) == ids[1]);
	assert(notify_model_close(model, ids[1], 1, NULL));
	assert(!notify_model_deadline(model));
	assert(!notify_model_expired(model, 100000));
	assert(notify_model_close(model, ids[2], 2, NULL));
	assert(model->history_count == 2);
	assert(model->history[0].record.id == ids[2]);
	assert(model->history[1].record.id == ids[1]);
	assert(!model->history[0].record.owner[0]);
	assert(!model->history[0].record.actions[0].key[0]);
	assert(!model->history[0].record.action_count);
	assert(!notify_model_history_limit(model, NOTIFY_HISTORY_MAX + 1));
	assert(model->history_count == 2);
	assert(notify_model_history_limit(model, 0));
	assert(!model->history_count && !model->history[0].record.summary[0]);
	assert(notify_model_close(model, ids[3], 3, NULL));
	assert(!model->history_count);
}

int main(void)
{
	notify_model *model = calloc(1, sizeof(*model));
	notify_model *before = calloc(1, sizeof(*before));
	assert(model && before);
	text_boundaries();
	atomic_replacement(model, before);
	bounded_records(model, before);
	invalid_records(model, before);
	expiration_and_history(model);
	free(before);
	free(model);
	return 0;
}
