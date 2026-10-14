/* This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */
#include "config.h"
#include "script_file.h"
#include "libs/fd_safety.h"
#include "libs/safemalloc.h"
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static const char *line_find(const char *input, const char *needle)
{
	const char *p = input;
	size_t length = strlen(needle);

	do
	{
		if (strncmp(p, needle, length) == 0)
			return p;
		p = strchr(p, '\n');
		if (p != NULL)
			p++;
	} while (p != NULL && *p != '\0');
	return NULL;
}

static int has_end_marker(const char *text)
{
	const char *p = text;

	do
	{
		if (strncmp(p, "#end", 4) == 0 &&
		    (p[4] == '\n' || p[4] == '\0'))
			return 1;
		p = strchr(p, '\n');
		if (p != NULL)
			p++;
	} while (p != NULL && *p != '\0');
	return 0;
}

char *script_file_build(const char *input, size_t length, const char *name,
	pid_t pid, const char *text)
{
	char *source, *prefix, *header, *result = NULL;
	const char *begin, *end;
	size_t before, after, text_length, header_length, newline, total;
	long old_pid;

	if (length > SCRIPT_FILE_MAX_BYTES ||
	    strlen(text) > SCRIPT_TEXT_MAX_BYTES ||
	    strlen(name) > SCRIPT_TEXT_MAX_BYTES)
	{
		errno = EFBIG;
		return NULL;
	}
	if (memchr(input, '\0', length) != NULL || name[0] == '\0' ||
	    strpbrk(name, ",\r\n") != NULL ||
	    has_end_marker(text) || pid <= 0)
	{
		errno = EINVAL;
		return NULL;
	}
	source = fxmalloc(length + 1);
	memcpy(source, input, length);
	source[length] = '\0';
	prefix = fxmalloc(strlen(name) + 3);
	sprintf(prefix, "#%s,", name);
	header = fxmalloc(strlen(prefix) + 32);
	sprintf(header, "%s%ld\n", prefix, (long)pid);
	if (line_find(text, prefix) != NULL)
		goto invalid;
	begin = line_find(source, prefix);
	text_length = strlen(text);
	newline = text_length == 0 || text[text_length - 1] != '\n';
	if (begin == NULL)
	{
		before = length;
		after = length;
		header_length = strlen(header);
	}
	else
	{
		const char *digits = begin + strlen(prefix);
		char *number_end;

		errno = 0;
		old_pid = strtol(digits, &number_end, 10);
		if (errno != 0 || old_pid <= 0 || number_end == digits ||
		    *number_end != '\n')
			goto invalid;
		end = line_find(number_end + 1, "#end\n");
		/* Never rewrite a truncated or duplicated section. */
		if (end == NULL || line_find(number_end + 1, prefix) != NULL)
			goto invalid;
		before = (old_pid == (long)pid ? end : begin) - source;
		after = (old_pid == (long)pid ? end : end + 5) - source;
		header_length = old_pid == (long)pid ? 0 : strlen(header);
	}
	/* Each component is capped before addition. Allow one separator when
	 * appending a new section to arbitrary existing file content. */
	total = before + header_length + text_length + newline +
		length - after + (header_length != 0 ? 5 : 0) +
		(begin == NULL ? 2 : 0);
	if (total > SCRIPT_FILE_MAX_BYTES)
	{
		errno = EFBIG;
		goto done;
	}
	result = fxmalloc(total + 1);
	memcpy(result, source, before);
	size_t pos = before;
	if (begin == NULL)
		result[pos++] = '\n';
	memcpy(result + pos, header, header_length);
	pos += header_length;
	memcpy(result + pos, text, text_length);
	pos += text_length;
	if (newline)
		result[pos++] = '\n';
	if (header_length != 0)
	{
		memcpy(result + pos, "#end\n", 5);
		pos += 5;
	}
	if (begin == NULL)
		result[pos++] = '\n';
	memcpy(result + pos, source + after, length - after);
	pos += length - after;
	result[pos] = '\0';
	goto done;
invalid:
	errno = EINVAL;
done:
	free(source);
	free(prefix);
	free(header);
	return result;
}

int script_file_update(const char *path, const char *name, pid_t pid,
	const char *text)
{
	int fd, status = -1, saved_errno;
	struct stat metadata;
	struct flock lock = {0};
	char *input = NULL, *output = NULL;
	size_t used = 0, length;
	ssize_t count;

	fd = open(path, O_RDWR | O_CREAT | O_NOFOLLOW | O_NONBLOCK, 0600);
	if (fd < 0)
		return -1;
	if (!fvwm_fd_set_cloexec(fd, true) || fstat(fd, &metadata) < 0)
		goto done;
	if (!S_ISREG(metadata.st_mode) || metadata.st_uid != getuid() ||
	    metadata.st_nlink != 1)
	{
		errno = EINVAL;
		goto done;
	}
	if (metadata.st_size < 0 || metadata.st_size > SCRIPT_FILE_MAX_BYTES)
	{
		errno = EFBIG;
		goto done;
	}
	lock.l_type = F_WRLCK;
	lock.l_whence = SEEK_SET;
	if (fcntl(fd, F_SETLK, &lock) < 0)
		goto done;
	input = fxmalloc(SCRIPT_FILE_MAX_BYTES + 1);
	while (used <= SCRIPT_FILE_MAX_BYTES)
	{
		count = read(fd, input + used,
			SCRIPT_FILE_MAX_BYTES + 1 - used);
		if (count < 0 && errno == EINTR)
			continue;
		if (count < 0)
			goto done;
		if (count == 0)
			break;
		used += count;
	}
	output = script_file_build(input, used, name, pid, text);
	if (output == NULL || lseek(fd, 0, SEEK_SET) < 0)
		goto done;
	length = strlen(output);
	used = 0;
	while (used < length)
	{
		count = write(fd, output + used, length - used);
		if (count < 0 && errno == EINTR)
			continue;
		if (count <= 0)
		{
			if (count == 0)
				errno = EIO;
			goto done;
		}
		used += count;
	}
	status = ftruncate(fd, length);
done:
	saved_errno = errno;
	free(input);
	free(output);
	if (close(fd) < 0 && status == 0)
		return -1;
	errno = saved_errno;
	return status;
}
