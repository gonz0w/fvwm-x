/* This program is free software; you can redistribute it and/or modify it
 * under the GNU General Public License, version 2 or any later version. */
#include "config.h"
#include "notify_model.h"
#include <limits.h>
#include <math.h>
#include <stddef.h>
#include <string.h>

bool notify_model_text(const char *text, unsigned int maximum, bool multiline)
{
	const unsigned char *p = (const unsigned char *)text;
	size_t length;

	if (!text || !(length = strnlen(text, (size_t)maximum + 1)) ||
	    length > maximum)
		return text && text[0] == '\0';
	for (size_t i = 0; i < length;)
	{
		unsigned char first = p[i++];
		unsigned int count, value, minimum;
		if (first < 0x80)
		{
			if ((first < 0x20 &&
			     !(multiline && (first == '\n' || first == '\t'))) ||
			    first == 0x7f)
				return false;
			continue;
		}
		if (first >= 0xc2 && first <= 0xdf)
			count = 1, value = first & 0x1f, minimum = 0x80;
		else if (first >= 0xe0 && first <= 0xef)
			count = 2, value = first & 0x0f, minimum = 0x800;
		else if (first >= 0xf0 && first <= 0xf4)
			count = 3, value = first & 7, minimum = 0x10000;
		else
			return false;
		if (count > length - i)
			return false;
		while (count--)
		{
			if ((p[i] & 0xc0) != 0x80)
				return false;
			value = (value << 6) | (p[i++] & 0x3f);
		}
		if (value < minimum || value > 0x10ffff ||
		    (value >= 0xd800 && value <= 0xdfff) ||
		    (value >= 0x80 && value <= 0x9f))
			return false;
	}
	return true;
}

static bool request_valid(const notify_request *request)
{
	const notify_record *record = &request->record;

	if (record->owner[0] != ':' ||
	    !notify_model_text(record->owner, NOTIFY_OWNER_MAX, false) ||
	    !notify_model_text(record->app, NOTIFY_APP_MAX, false) ||
	    !notify_model_text(record->summary, NOTIFY_SUMMARY_MAX, false) ||
	    !notify_model_text(record->body, NOTIFY_BODY_MAX, true) ||
	    record->urgency > 2 || record->action_count > NOTIFY_ACTION_MAX ||
	    request->timeout < -1 || request->timeout > 86400000)
		return false;
	for (unsigned int i = 0; i < record->action_count; i++)
	{
		const notify_action *action = &record->actions[i];
		if (!action->key[0] ||
		    !notify_model_text(action->key, NOTIFY_ACTION_KEY_MAX, false) ||
		    !notify_model_text(action->label, NOTIFY_ACTION_LABEL_MAX, false))
			return false;
		for (unsigned int j = 0; j < i; j++)
			if (!strcmp(action->key, record->actions[j].key))
				return false;
	}
	return true;
}

const notify_record *notify_model_find(const notify_model *model, uint32_t id)
{
	if (id)
		for (unsigned int i = 0; i < model->count; i++)
			if (model->active[i].id == id)
				return &model->active[i];
	return NULL;
}

notify_result notify_model_put(notify_model *model,
	const notify_request *request, double now, uint32_t *id)
{
	notify_record record;
	unsigned int index = model->count;
	uint32_t candidate;
	int32_t timeout;

	if (id)
		*id = 0;
	if (!id || !request || !isfinite(now) || now < 0 ||
	    !request_valid(request))
		return NOTIFY_INVALID;
	if (request->replaces)
	{
		const notify_record *previous =
		    notify_model_find(model, request->replaces);
		if (!previous)
			return NOTIFY_UNKNOWN_ID;
		index = (unsigned int)(previous - model->active);
		candidate = request->replaces;
	}
	else
	{
		if (model->count == NOTIFY_ACTIVE_MAX)
			return NOTIFY_FULL;
		if (model->next_id > UINT32_MAX)
			return NOTIFY_EXHAUSTED;
		candidate = model->next_id ? (uint32_t)model->next_id : 1;
	}
	record = request->record;
	record.id = candidate;
	timeout = request->timeout;
	if (timeout == -1)
		timeout = record.urgency == 2 ? 0 :
		    record.urgency == 0 ? 3000 : 5000;
	record.expires = timeout ? now + timeout / 1000.0 : 0;
	if (!isfinite(record.expires))
		return NOTIFY_INVALID;
	model->active[index] = record;
	if (!request->replaces)
	{
		model->count++;
		model->next_id = (uint64_t)candidate + 1;
	}
	model->revision++;
	*id = candidate;
	return NOTIFY_ACCEPTED;
}

bool notify_model_close(notify_model *model, uint32_t id, uint32_t reason,
	notify_record *closed)
{
	const notify_record *record = notify_model_find(model, id);
	notify_record retired;
	unsigned int index;

	if (closed)
		memset(closed, 0, sizeof(*closed));
	if (!record || reason < 1 || reason > 4)
		return false;
	retired = *record;
	index = (unsigned int)(record - model->active);
	model->count--;
	memmove(&model->active[index], &model->active[index + 1],
		(model->count - index) * sizeof(*record));
	memset(&model->active[model->count], 0, sizeof(*record));
	if (model->history_limit)
	{
		unsigned int count = model->history_count;
		if (count == model->history_limit)
			count--;
		memmove(&model->history[1], &model->history[0],
			count * sizeof(model->history[0]));
		model->history[0] = (notify_history){retired, reason};
		memset(model->history[0].record.owner, 0,
			sizeof(retired.owner));
		memset(model->history[0].record.actions, 0,
			sizeof(retired.actions));
		model->history[0].record.action_count = 0;
		model->history[0].record.expires = 0;
		model->history_count = count + 1;
	}
	model->revision++;
	if (closed)
		*closed = retired;
	return true;
}

uint32_t notify_model_expired(const notify_model *model, double now)
{
	if (!isfinite(now) || now < 0)
		return 0;
	for (unsigned int i = 0; i < model->count; i++)
		if (model->active[i].expires && model->active[i].expires <= now)
			return model->active[i].id;
	return 0;
}

double notify_model_deadline(const notify_model *model)
{
	double deadline = 0;
	for (unsigned int i = 0; i < model->count; i++)
		if (model->active[i].expires &&
		    (!deadline || model->active[i].expires < deadline))
			deadline = model->active[i].expires;
	return deadline;
}

bool notify_model_history_limit(notify_model *model, unsigned int limit)
{
	if (limit > NOTIFY_HISTORY_MAX)
		return false;
	if (limit == model->history_limit)
		return true;
	if (model->history_count > limit)
		model->history_count = limit;
	memset(&model->history[model->history_count], 0,
		(NOTIFY_HISTORY_MAX - model->history_count) *
		    sizeof(model->history[0]));
	model->history_limit = limit;
	model->revision++;
	return true;
}
