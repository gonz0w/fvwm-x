/* This program is free software; you can redistribute it and/or modify
 * it under the GNU General Public License, version 2 or any later version. */
#include "config.h"
#include "tray_icons.h"
#include "libs/image_process.h"
#include "libs/monitor_scale.h"
#include "libs/fd_safety.h"
#include "libs/safemalloc.h"
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <math.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#define TRAY_ICON_WIRE_MAX 65536U

struct tray_icons
{
	char executable[4097], directory[1025];
	char input[TRAY_ICON_WIRE_MAX + 1];
	unsigned char output[TRAY_ICON_WIRE_MAX + 1];
	size_t received;
	int directory_fd;
	struct stat directory_identity;
	image_input source;
	image_process *process;
	tray_icon_request requests[TRAY_ICON_BATCH_MAX];
	tray_icon_result results[TRAY_ICON_BATCH_MAX];
	unsigned int count;
	bool ready, failed;
	char error[128];
};

static bool bounded_text(const char *text, size_t maximum, bool empty)
{
	size_t length;

	if (!text || (length = strnlen(text, maximum + 1)) > maximum ||
	    (!empty && !length))
		return false;
	for (size_t i = 0; i < length; i++)
		if ((unsigned char)text[i] < 0x20 || text[i] == 0x7f)
			return false;
	return true;
}

static bool append_text(char *output, size_t capacity, size_t *used,
	const char *text)
{
	size_t length = strlen(text);

	if (*used >= capacity || length >= capacity - *used)
		return false;
	memcpy(output + *used, text, length + 1);
	*used += length;
	return true;
}

static bool append_hex(char *output, size_t capacity, size_t *used,
	const char *text)
{
	static const char digits[] = "0123456789abcdef";
	size_t length = strlen(text);

	if (!length)
		return append_text(output, capacity, used, "-");
	if (*used >= capacity || length > (capacity - *used - 1) / 2)
		return false;
	for (size_t i = 0; i < length; i++)
	{
		unsigned char value = (unsigned char)text[i];
		output[(*used)++] = digits[value >> 4];
		output[(*used)++] = digits[value & 15];
	}
	output[*used] = 0;
	return true;
}

size_t tray_icons_encode(const tray_icon_request *requests, unsigned int count,
	const char *directory, const char *formats, char *output,
	size_t capacity)
{
	char number[128];
	size_t used = 0;

	if (!output || !capacity)
		return 0;
	output[0] = 0;
	if (!requests || !count || count > TRAY_ICON_BATCH_MAX ||
	    !bounded_text(directory, 1024, false) || directory[0] != '/' ||
	    !formats || (strcmp(formats, "xpm") &&
	    strcmp(formats, "png,xpm") && strcmp(formats, "svg,xpm") &&
	    strcmp(formats, "png,svg,xpm")))
		return 0;
	snprintf(number, sizeof(number), "FVWM_TRAY_ICONS 1 %u ", count);
	if (!append_text(output, capacity, &used, number) ||
	    !append_hex(output, capacity, &used, directory) ||
	    !append_text(output, capacity, &used, " ") ||
	    !append_text(output, capacity, &used, formats) ||
	    !append_text(output, capacity, &used, "\n"))
		return 0;
	for (unsigned int i = 0; i < count; i++)
	{
		const tray_icon_request *r = &requests[i];
		const char *theme = r->theme ? r->theme : "hicolor";
		const char *root = r->theme_path ? r->theme_path : "";

		if (!r->id || !r->generation || r->size < 8 || r->size > 128 ||
		    r->quarters < MONITOR_SCALE_QUARTERS_MIN ||
		    r->quarters > MONITOR_SCALE_QUARTERS_MAX ||
		    !bounded_text(r->name, TRAY_ICON_NAME_MAX, false) ||
		    !bounded_text(theme, TRAY_ICON_THEME_MAX, true) ||
		    !bounded_text(root, TRAY_ICON_ROOT_MAX, true) ||
		    (root[0] && root[0] != '/'))
			return 0;
		for (unsigned int j = 0; j < i; j++)
			if (requests[j].id == r->id)
				return 0;
		snprintf(number, sizeof(number), "%" PRIu64 " %" PRIu64
		    " %u %u ", r->id, r->generation, r->size, r->quarters);
		if (!append_text(output, capacity, &used, number) ||
		    !append_hex(output, capacity, &used, r->name) ||
		    !append_text(output, capacity, &used, " ") ||
		    !append_hex(output, capacity, &used, theme) ||
		    !append_text(output, capacity, &used, " ") ||
		    !append_hex(output, capacity, &used, root) ||
		    !append_text(output, capacity, &used, "\n"))
			return 0;
	}
	if (!append_text(output, capacity, &used, "END\n") ||
	    used > TRAY_ICON_WIRE_MAX)
		return 0;
	return used;
}

static int hex_digit(unsigned char value)
{
	if (value >= '0' && value <= '9')
		return value - '0';
	if (value >= 'a' && value <= 'f')
		return value - 'a' + 10;
	return -1;
}

static bool take_number(const unsigned char **cursor, const unsigned char *end,
	uint64_t *value)
{
	const unsigned char *p = *cursor;
	uint64_t number = 0;
	unsigned int digits = 0;

	while (p < end && *p >= '0' && *p <= '9')
	{
		unsigned int digit = *p++ - '0';
		if (++digits > 20 || number > (UINT64_MAX - digit) / 10)
			return false;
		number = number * 10 + digit;
	}
	if (!digits || !number || p == end || *p++ != ' ')
		return false;
	*cursor = p;
	*value = number;
	return true;
}

static bool private_name(const char *name)
{
	const char *prefix = ".fvwm-xdg-";

	if (strncmp(name, prefix, strlen(prefix)))
		return false;
	name += strlen(prefix);
	for (unsigned int i = 0; i < 64; i++)
		if (!name[i] || hex_digit((unsigned char)name[i]) < 0)
			return false;
	name += 64;
	if (!strcmp(name, ".png") || !strcmp(name, ".xpm") ||
	    !strcmp(name, ".svg"))
		return true;
	/* Shared SVG presentation appends an explicit bounded output size. */
	if (strncmp(name, ".svg:", 5))
		return false;
	name += 5;
	unsigned int width = 0, height = 0, digits = 0;
	while (*name >= '0' && *name <= '9')
	{
		if (++digits > 4)
			return false;
		width = width * 10 + (*name++ - '0');
	}
	if (!digits || *name++ != 'x')
		return false;
	digits = 0;
	while (*name >= '0' && *name <= '9')
	{
		if (++digits > 4)
			return false;
		height = height * 10 + (*name++ - '0');
	}
	return digits && !*name && width > 0 && height > 0 &&
	    width <= 1024 && height <= 1024;
}

bool tray_icons_decode(const tray_icon_request *requests, unsigned int count,
	const char *directory, const unsigned char *bytes, size_t length,
	tray_icon_result results[TRAY_ICON_BATCH_MAX])
{
	char header[64];
	const unsigned char *p = bytes, *end;
	size_t directory_length;
	tray_icon_result parsed[TRAY_ICON_BATCH_MAX] = {0};

	if (results)
		memset(results, 0, sizeof(parsed));
	if (!requests || !count || count > TRAY_ICON_BATCH_MAX || !results ||
	    !bounded_text(directory, 1024, false) || directory[0] != '/' ||
	    !bytes || !length || length > TRAY_ICON_WIRE_MAX)
		return false;
	end = p + length;
	snprintf(header, sizeof(header), "FVWM_TRAY_ICONS 1 %u\n", count);
	size_t header_length = strlen(header);
	if (length < header_length || memcmp(p, header, header_length))
		return false;
	p += header_length;
	directory_length = strlen(directory);
	for (unsigned int i = 0; i < count; i++)
	{
		if (!take_number(&p, end, &parsed[i].id) ||
		    !take_number(&p, end, &parsed[i].generation) ||
		    parsed[i].id != requests[i].id ||
		    parsed[i].generation != requests[i].generation)
			return false;
		if (end - p >= 2 && p[0] == '-' && p[1] == '\n')
		{
			p += 2;
			continue;
		}
		size_t used = 0;
		while (p < end && *p != '\n')
		{
			if (end - p < 2 || used >= TRAY_ICON_PATH_MAX)
				return false;
			int high = hex_digit(p[0]), low = hex_digit(p[1]);
			if (high < 0 || low < 0)
				return false;
			unsigned char value =
			    (unsigned char)((high << 4) | low);
			if (value < 0x20 || value == 0x7f)
				return false;
			parsed[i].path[used++] = (char)value;
			p += 2;
		}
		if (p == end || *p++ != '\n' || !used ||
		    strncmp(parsed[i].path, directory, directory_length) ||
		    parsed[i].path[directory_length] != '/' ||
		    !private_name(parsed[i].path + directory_length + 1))
			return false;
		const char *svg = strstr(parsed[i].path + directory_length + 1,
		    ".svg");
		if (svg)
		{
			char expected[32];
			if (requests[i].size < 8 || requests[i].size > 128 ||
			    requests[i].quarters < MONITOR_SCALE_QUARTERS_MIN ||
			    requests[i].quarters > MONITOR_SCALE_QUARTERS_MAX)
				return false;
			unsigned int physical = (requests[i].size *
			    requests[i].quarters + 3) / 4;
			snprintf(expected, sizeof(expected), ".svg:%ux%u",
			    physical, physical);
			if (strcmp(svg, expected))
				return false;
		}
	}
	if (end - p != 4 || memcmp(p, "END\n", 4))
		return false;
	memcpy(results, parsed, sizeof(parsed));
	return true;
}

static void set_error(tray_icons *icons, const char *error)
{
	icons->failed = true;
	icons->ready = false;
	snprintf(icons->error, sizeof(icons->error), "%s", error);
}

static bool collect(const unsigned char *bytes, size_t length, void *context)
{
	tray_icons *icons = context;

	if (length > TRAY_ICON_WIRE_MAX - icons->received)
		return false;
	memcpy(icons->output + icons->received, bytes, length);
	icons->received += length;
	return true;
}

static bool retire_directory(tray_icons *icons)
{
	if (!icons->directory[0])
		return true;
	int duplicate = icons->directory_fd < 0 ? -1 :
	    dup(icons->directory_fd);
	DIR *directory = NULL;
	unsigned int count = 0;
	bool okay = duplicate >= 0;

	if (okay && !fvwm_fd_set_cloexec(duplicate, true))
	{
		close(duplicate);
		duplicate = -1;
		okay = false;
	}
	if (okay)
	{
		directory = fdopendir(duplicate);
		if (!directory)
		{
			close(duplicate);
			okay = false;
		}
	}

	if (directory)
	{
		struct dirent *entry;
		rewinddir(directory);
		while ((entry = readdir(directory)))
		{
			if (!strcmp(entry->d_name, ".") ||
			    !strcmp(entry->d_name, ".."))
				continue;
			/* Retire only this worker's publications or interrupted
			 * private temporary writes; never recurse. */
			struct stat info;
			if (++count > TRAY_ICON_BATCH_MAX ||
			    strncmp(entry->d_name, ".fvwm-xdg-", 10) ||
			    fstatat(dirfd(directory), entry->d_name, &info,
				AT_SYMLINK_NOFOLLOW) ||
			    !S_ISREG(info.st_mode) || info.st_uid != getuid() ||
			    (info.st_mode & 077) || info.st_nlink != 1 ||
			    unlinkat(dirfd(directory), entry->d_name, 0))
			{
				okay = false;
				break;
			}
		}
		closedir(directory);
	}
	struct stat current;
	if (okay && (lstat(icons->directory, &current) ||
	    !S_ISDIR(current.st_mode) ||
	    current.st_dev != icons->directory_identity.st_dev ||
	    current.st_ino != icons->directory_identity.st_ino))
		okay = false;
	if (okay && rmdir(icons->directory) == 0)
	{
		close(icons->directory_fd);
		icons->directory_fd = -1;
		icons->directory[0] = 0;
	}
	else
		okay = false;
	return okay;
}

tray_icons *tray_icons_create(const char *executable)
{
	tray_icons *icons = fxcalloc(1, sizeof(*icons));

	icons->directory_fd = -1;
	if (!executable)
	{
		executable = getenv("FVWM_DESKTOP_WORKER");
		if (!executable || !executable[0])
			executable = FVWM_MODULEDIR "/fvwm-desktop-worker";
	}
	if (!bounded_text(executable, 4096, false) || executable[0] != '/')
		set_error(icons, "invalid private icon worker path");
	else
		strcpy(icons->executable, executable);
	return icons;
}

bool tray_icons_available(const tray_icons *icons)
{
	struct stat status;

	return icons && icons->executable[0] &&
	    stat(icons->executable, &status) == 0 && S_ISREG(status.st_mode) &&
	    access(icons->executable, X_OK) == 0;
}

bool tray_icons_begin(tray_icons *icons, const tray_icon_request *requests,
	unsigned int count, double now)
{
	const char *formats;

	if (!icons || icons->process || icons->ready || icons->failed ||
	    !isfinite(now) || now < 0 || now > 1e12)
		return false;
	if (icons->directory[0])
		return false;
	strcpy(icons->directory, "/tmp/fvwm-tray-icons-XXXXXX");
	if (!mkdtemp(icons->directory))
	{
		icons->directory[0] = 0;
		set_error(icons, "cannot create private icon directory");
		return false;
	}
	icons->directory_fd = open(icons->directory,
	    O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC);
	if (icons->directory_fd < 0 ||
	    fstat(icons->directory_fd, &icons->directory_identity))
	{
		if (icons->directory_fd >= 0)
			close(icons->directory_fd);
		icons->directory_fd = -1;
		(void)rmdir(icons->directory);
		icons->directory[0] = 0;
		set_error(icons,
		    "cannot retain private icon directory identity");
		return false;
	}
#if HAVE_PNG && HAVE_RSVG
	formats = "png,svg,xpm";
#elif HAVE_PNG
	formats = "png,xpm";
#elif HAVE_RSVG
	formats = "svg,xpm";
#else
	formats = "xpm";
#endif
	icons->source.bytes = (unsigned char *)icons->input;
	icons->source.length = tray_icons_encode(requests, count,
	    icons->directory, formats, icons->input, sizeof(icons->input));
	if (!icons->source.length)
	{
		set_error(icons, "invalid private icon batch");
		(void)retire_directory(icons);
		return false;
	}
	icons->count = count;
	/* Caller strings may retire; retain only numeric request identity. */
	for (unsigned int i = 0; i < count; i++)
		icons->requests[i] = (tray_icon_request){
		    .id = requests[i].id, .generation = requests[i].generation,
		    .size = requests[i].size, .quarters = requests[i].quarters};
	char *argv[] = {icons->executable, "--tray-icons", NULL};
	icons->process = image_process_start(&icons->source, icons->executable,
	    argv, TRAY_ICON_WIRE_MAX, 4000, 0, collect, icons);
	if (!icons->process)
	{
		set_error(icons, "private icon worker unavailable");
		(void)retire_directory(icons);
		return false;
	}
	return true;
}

int tray_icons_write_fd(const tray_icons *icons)
{
	struct pollfd descriptors[2];

	if (!icons || !icons->process)
		return -1;
	image_process_descriptors(icons->process, descriptors);
	return descriptors[1].fd;
}

int tray_icons_read_fd(const tray_icons *icons)
{
	struct pollfd descriptors[2];

	if (!icons || !icons->process)
		return -1;
	image_process_descriptors(icons->process, descriptors);
	return descriptors[0].fd;
}

int tray_icons_timeout_ms(const tray_icons *icons)
{
	return icons && icons->process ?
	    image_process_timeout_ms(icons->process) : -1;
}

void tray_icons_service(tray_icons *icons, double now)
{
	(void)now;
	if (!icons || !icons->process)
		return;
	image_process_status status = image_process_step(icons->process);
	if (status == IMAGE_PROCESS_RUNNING)
		return;
	bool success = status == IMAGE_PROCESS_SUCCEEDED;
	(void)image_process_destroy(icons->process);
	icons->process = NULL;
	if (!success || !tray_icons_decode(icons->requests, icons->count,
	    icons->directory, icons->output, icons->received, icons->results))
	{
		set_error(icons,
		    "private icon worker failed or returned stale data");
		(void)retire_directory(icons);
		return;
	}
	struct stat current;
	if (lstat(icons->directory, &current) || !S_ISDIR(current.st_mode) ||
	    current.st_dev != icons->directory_identity.st_dev ||
	    current.st_ino != icons->directory_identity.st_ino)
	{
		set_error(icons, "private icon directory identity changed");
		(void)retire_directory(icons);
		return;
	}
	/* A worker filename must still name an owned private regular snapshot.
	 * SVG size suffixes are presentation syntax, not filename bytes. */
	for (unsigned int i = 0; i < icons->count; i++)
	{
		char path[TRAY_ICON_PATH_MAX + 1];
		struct stat info;

		if (!icons->results[i].path[0])
			continue;
		strcpy(path, icons->results[i].path);
		char *suffix = strstr(path, ".svg:");
		if (suffix)
			suffix[4] = 0;
		if (lstat(path, &info) || !S_ISREG(info.st_mode) ||
		    info.st_uid != getuid() || (info.st_mode & 077) ||
		    info.st_nlink != 1 || info.st_size < 0 ||
		    info.st_size > 2 * 1024 * 1024)
		{
			set_error(icons,
			    "private icon output identity refused");
			(void)retire_directory(icons);
			return;
		}
	}
	icons->ready = true;
}

bool tray_icons_busy(const tray_icons *icons)
{
	return icons && icons->process;
}

bool tray_icons_ready(const tray_icons *icons)
{
	return icons && icons->ready;
}

unsigned int tray_icons_count(const tray_icons *icons)
{
	return icons && icons->ready ? icons->count : 0;
}

const tray_icon_result *tray_icons_result(const tray_icons *icons,
	unsigned int index)
{
	return icons && icons->ready && index < icons->count ?
	    &icons->results[index] : NULL;
}

const char *tray_icons_error(const tray_icons *icons)
{
	return icons && icons->failed ? icons->error : NULL;
}

void tray_icons_release(tray_icons *icons)
{
	if (!icons || icons->process)
		return;
	if (!retire_directory(icons))
	{
		set_error(icons, "private icon artifact retirement failed");
		return;
	}
	icons->ready = icons->failed = false;
	icons->received = icons->count = 0;
	icons->error[0] = 0;
	memset(icons->results, 0, sizeof(icons->results));
}

void tray_icons_cancel(tray_icons *icons)
{
	if (!icons)
		return;
	if (icons->process)
		image_process_cancel(icons->process);
	icons->ready = false;
}

bool tray_icons_destroy(tray_icons *icons)
{
	if (!icons)
		return true;
	if (icons->process)
	{
		image_process_cancel(icons->process);
		errno = EBUSY;
		return false;
	}
	if (!retire_directory(icons))
	{
		errno = EIO;
		return false;
	}
	free(icons);
	return true;
}
