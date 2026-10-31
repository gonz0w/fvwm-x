/* This program is free software; you can redistribute it and/or modify it
 * under the GNU General Public License, version 2 or any later version. */
#include "config.h"
#include "file_manager_policy.h"
#include "libs/cJSON.h"
#include <string.h>

bool file_manager_text_valid(const char *text, size_t maximum)
{
	size_t length, i = 0;

	if (!text || (length = strnlen(text, maximum + 1)) > maximum)
		return false;
	while (i < length)
	{
		unsigned char first = (unsigned char)text[i++];
		uint32_t value;
		unsigned int remaining;

		if (first < 0x20 || first == 0x7f)
			return false;
		if (first < 0x80)
			continue;
		if (first >= 0xc2 && first <= 0xdf)
		{
			value = first & 0x1f;
			remaining = 1;
		}
		else if (first >= 0xe0 && first <= 0xef)
		{
			value = first & 0x0f;
			remaining = 2;
		}
		else if (first >= 0xf0 && first <= 0xf4)
		{
			value = first & 0x07;
			remaining = 3;
		}
		else
			return false;
		if (remaining > length - i)
			return false;
		for (unsigned int j = 0; j < remaining; j++)
		{
			unsigned char next = (unsigned char)text[i++];

			if ((next & 0xc0) != 0x80)
				return false;
			value = (value << 6) | (next & 0x3f);
		}
		if ((remaining == 1 && value < 0x80) ||
		    (remaining == 2 && value < 0x800) ||
		    (remaining == 3 && value < 0x10000) || value > 0x10ffff ||
		    (value >= 0xd800 && value <= 0xdfff) ||
		    (value >= 0x80 && value <= 0x9f))
			return false;
	}
	return true;
}

bool file_manager_reply_parse(const char *line, size_t length,
	struct file_manager_reply *reply)
{
	static const char *const statuses[] = {
		"accepted", "invalid", "refused", "unknown"
	};
	cJSON *root = NULL, *item;
	const cJSON *version = NULL, *id = NULL, *status = NULL, *detail = NULL;
	const char *end;
	uint64_t number = 0;
	bool quoted = false, escaped = false, valid = false;

	memset(reply, 0, sizeof(*reply));
	if (!line || !length || length > FILE_MANAGER_REPLY_BYTES ||
	    memchr(line, '\0', length))
		return false;
	/* Bound parser depth before cJSON allocates. Reject NUL escapes so a
	 * decoded C string cannot conceal a suffix from field validation. */
	for (size_t i = 0; i < length; i++)
	{
		if (escaped)
		{
			if (line[i] == 'u' && i + 4 < length &&
			    memcmp(line + i + 1, "0000", 4) == 0)
				return false;
			escaped = false;
		}
		else if (quoted && line[i] == '\\')
			escaped = true;
		else if (line[i] == '"')
			quoted = !quoted;
		else if (!quoted && (line[i] == '[' ||
		    (line[i] == '{' && i != 0)))
			return false;
	}
	root = cJSON_ParseWithLengthOpts(line, length, &end, false);
	if (!cJSON_IsObject(root))
		goto out;
	while (end < line + length && (*end == ' ' || *end == '\t' ||
	    *end == '\r'))
		end++;
	if (end != line + length)
		goto out;
	cJSON_ArrayForEach(item, root)
	{
		const cJSON **field;

		if (strcmp(item->string, "version") == 0)
			field = &version;
		else if (strcmp(item->string, "id") == 0)
			field = &id;
		else if (strcmp(item->string, "status") == 0)
			field = &status;
		else if (strcmp(item->string, "detail") == 0)
			field = &detail;
		else
			goto out;
		if (*field)
			goto out;
		*field = item;
	}
	if (!cJSON_IsNumber(version) || version->valuedouble != 1 ||
	    !cJSON_IsString(id) || !cJSON_IsString(status) ||
	    !cJSON_IsString(detail) || !id->valuestring[0] ||
	    id->valuestring[0] == '0' ||
	    !file_manager_text_valid(detail->valuestring,
		FILE_MANAGER_REPLY_BYTES))
		goto out;
	for (const char *p = id->valuestring; *p; p++)
	{
		if (*p < '0' || *p > '9' ||
		    number > (UINT64_MAX - (unsigned int)(*p - '0')) / 10)
			goto out;
		number = number * 10 + (unsigned int)(*p - '0');
	}
	for (size_t i = 0; i < sizeof(statuses) / sizeof(statuses[0]); i++)
		if (strcmp(status->valuestring, statuses[i]) == 0)
		{
			reply->id = number;
			reply->status = (enum file_manager_reply_status)i;
			strcpy(reply->detail, detail->valuestring);
			valid = true;
			break;
		}
out:
	cJSON_Delete(root);
	return valid;
}
