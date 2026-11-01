/* This program is free software; you can redistribute it and/or modify it
 * under the GNU General Public License, version 2 or any later version. */
#include "config.h"
#include "file_transfer_policy.h"
#include "libs/cJSON.h"
#include <limits.h>
#include <string.h>

bool
ft_decimal(const char *text, uint64_t *value)
{
	uint64_t n = 0;

	if (value)
		*value = 0;
	if (!text || !value || !*text || text[0] == '0')
		return false;
	for (const unsigned char *p = (const unsigned char *)text; *p; p++)
	{
		if (*p < '0' || *p > '9' || n > (UINT64_MAX - (*p - '0')) / 10)
			return false;
		n = n * 10 + *p - '0';
	}
	*value = n;
	return true;
}

bool
ft_time_valid(uint32_t time, uint32_t since)
{
	return time && (!since || (int32_t)(time - since) >= 0);
}

static int
hex(unsigned char c)
{
	if (c >= '0' && c <= '9')
		return c - '0';
	if (c >= 'A' && c <= 'F')
		return c - 'A' + 10;
	if (c >= 'a' && c <= 'f')
		return c - 'a' + 10;
	return -1;
}

bool
ft_uris_valid(const char *text, const char *hostname)
{
	size_t total, count = 0;
	const char *p = text;

	if (!text || !hostname || !(total = strlen(text)) ||
	    total > FILE_TRANSFER_MAX_URI_BYTES)
		return false;
	while (*p)
	{
		const char *end = strchr(p, '\n'), *path, *authority;
		size_t length = end ? (size_t)(end - p) : strlen(p);
		char component[4097];
		size_t used = 0, decoded = 0;
		bool component_seen = false;

		if (length && p[length - 1] == '\r')
			length--;
		if (!length || *p == '#')
		{
			p = end ? end + 1 : p + strlen(p);
			continue;
		}
		if (++count > FILE_TRANSFER_MAX_SELECTED || length < 8 ||
		    memcmp(p, "file://", 7))
			return false;
		authority = p + 7;
		path = memchr(authority, '/', length - 7);
		if (!path)
			return false;
		size_t hostlen = (size_t)(path - authority);
		if (hostlen && !(hostlen == 9 &&
		    !memcmp(authority, "localhost", 9)) &&
		    !(hostlen == strlen(hostname) &&
		    !memcmp(authority, hostname, hostlen)))
			return false;
		for (const char *q = path; q < p + length; q++)
		{
			unsigned char c = *q;

			if (c <= 32 || c >= 127 || c == '?' || c == '#' ||
			    c == '\\' || c == '[' || c == ']' || c == '^' ||
			    c == '`' || c == '{' || c == '}' || c == '|' ||
			    c == '"' || c == '<' || c == '>')
				return false;
			if (c == '%')
			{
				if (q + 2 >= p + length || hex(q[1]) < 0 || hex(q[2]) < 0)
					return false;
				c = hex(q[1]) * 16 + hex(q[2]);
				q += 2;
			}
			if (!c || ++decoded > 4096)
				return false;
			if (c == '/')
			{
				if ((used == 1 && component[0] == '.') ||
				    (used == 2 && !memcmp(component, "..", 2)))
					return false;
				used = 0;
			}
			else
			{
				component[used++] = c;
				component_seen = true;
			}
		}
		if (decoded <= 1 || !component_seen ||
		    (used == 1 && component[0] == '.') ||
		    (used == 2 && !memcmp(component, "..", 2)))
			return false;
		p = end ? end + 1 : p + strlen(p);
	}
	return count != 0;
}

static bool
number(const cJSON *item, uint32_t *n)
{
	if (!cJSON_IsNumber(item) || item->valuedouble < 0 ||
	    item->valuedouble > UINT32_MAX ||
	    item->valuedouble != (double)(uint32_t)item->valuedouble)
		return false;
	*n = (uint32_t)item->valuedouble;
	return true;
}

bool
ft_command_parse(const char *line, size_t length, struct ft_command *out)
{
	static const char *const names[] = {"v", "id", "command", "epoch",
		"generation", "enabled", "action", "uris", "time", "transfer",
		"success", "lease"};
	static const char *const commands[] = {"expect-form", "context", "offer",
		"read", "drag-data", "admit", "complete", "clear", "cancel",
		"shutdown"};
	/* Required and optional field masks are deliberately per-command. */
	static const unsigned int required[] = {8, 8|16|32, 64|128, 0,
		64|128|512, 512, 512|64|1024, 2048, 512, 0};
	static const unsigned int optional[] = {0, 0, 256, 256, 0, 0, 0, 0, 0, 0};
	const cJSON *fields[12] = {0};
	cJSON *root = NULL, *item;
	const char *end;
	unsigned int mask = 0;
	bool quoted = false, escaped = false, valid = false;
	struct ft_command result = {0};

	if (out)
		memset(out, 0, sizeof(*out));
	if (!out || !line || !length || length > FILE_TRANSFER_FRAME_BYTES ||
	    memchr(line, 0, length))
		return false;
	while (length && (*line == ' ' || *line == '\t' || *line == '\r'))
		line++, length--;
	while (length && (line[length - 1] == ' ' ||
	    line[length - 1] == '\t' || line[length - 1] == '\r'))
		length--;
	if (!length)
		return false;
	/* Primitive-only records bound parser recursion before allocation. */
	for (size_t i = 0; i < length; i++)
	{
		unsigned char c = line[i];
		if ((c < 32 && !(c == '\t' && !quoted)) || c >= 127)
			return false;
		if (escaped)
		{
			if (c == 'u' && i + 4 < length &&
			    !memcmp(line + i + 1, "0000", 4))
				return false;
			escaped = false;
			continue;
		}
		if (quoted && c == '\\')
			escaped = true;
		else if (c == '"')
			quoted = !quoted;
		else if (!quoted && (c == '[' || c == ']' ||
		    (c == '{' && i != 0) || (c == '}' && i != length - 1)))
			return false;
	}
	root = cJSON_ParseWithLengthOpts(line, length, &end, false);
	if (!cJSON_IsObject(root) || end != line + length)
		goto done;
	cJSON_ArrayForEach(item, root)
	{
		size_t i;
		for (i = 0; i < 12; i++)
			if (item->string && !strcmp(item->string, names[i]))
				break;
		if (i == 12 || fields[i])
			goto done;
		fields[i] = item;
		mask |= 1U << i;
	}
	if (!fields[0] || !cJSON_IsNumber(fields[0]) ||
	    fields[0]->valuedouble != 1 || !cJSON_IsString(fields[1]) ||
	    !ft_decimal(fields[1]->valuestring, &result.id) ||
	    !cJSON_IsString(fields[2]))
		goto done;
	size_t kind;
	for (kind = 0; kind < 10; kind++)
		if (!strcmp(fields[2]->valuestring, commands[kind]))
			break;
	if (kind == 10 || (mask & required[kind]) != required[kind] ||
	    (mask & ~(7U | required[kind] | optional[kind])))
		goto done;
	result.kind = kind;
	if ((fields[3] && (!cJSON_IsString(fields[3]) ||
	    !ft_decimal(fields[3]->valuestring, &result.epoch))) ||
	    (fields[4] && (!number(fields[4], &result.generation) ||
	    !result.generation)) ||
	    (fields[5] && !cJSON_IsBool(fields[5])) ||
	    (fields[8] && (!number(fields[8], &result.time) || !result.time)) ||
	    (fields[9] && (!cJSON_IsString(fields[9]) ||
	    !ft_decimal(fields[9]->valuestring, &result.transfer))) ||
	    (fields[10] && !cJSON_IsBool(fields[10])) ||
	    (fields[11] && (!cJSON_IsString(fields[11]) ||
	    !ft_decimal(fields[11]->valuestring, &result.lease))))
		goto done;
	if (fields[6])
	{
		if (!cJSON_IsString(fields[6]) ||
		    (strcmp(fields[6]->valuestring, "copy") &&
		    strcmp(fields[6]->valuestring, "move")))
			goto done;
		result.move = !strcmp(fields[6]->valuestring, "move");
	}
	if (fields[7])
	{
		if (!cJSON_IsString(fields[7]) ||
		    strlen(fields[7]->valuestring) > FILE_TRANSFER_MAX_URI_BYTES)
			goto done;
		strcpy(result.uris, fields[7]->valuestring);
	}
	result.has_time = fields[8] != NULL;
	result.enabled = cJSON_IsTrue(fields[5]);
	result.success = cJSON_IsTrue(fields[10]);
	*out = result;
	valid = true;
done:
	cJSON_Delete(root);
	return valid;
}
