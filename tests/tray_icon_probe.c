/* This program is free software; you can redistribute it and/or modify
 * it under the GNU General Public License, version 2 or later. */
#include "config.h"
#include "modules/FvwmTray/tray.h"
#include "modules/FvwmTray/tray_bus.h"
#include "modules/FvwmTray/tray_icons.h"
#include "libs/Picture.h"
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* Private optional test binary only. The installed tray never links this file.
 * No pointer/model lease survives a callback; observations copy scalar IDs and
 * bounded paths. A held completed batch reports BOTH not-ready and busy. */
#define PROBE_LOAD_MAX 64U
#define PROBE_REPLY_MAX (TRAY_ICON_PATH_MAX * 2U + 512U)

typedef struct
{
	uint64_t item, item_generation, menu;
	bool open, ready, stale;
} probe_identity;

static struct
{
	int fd;
	bool initialized, bound, overflow, synced;
	unsigned int phase, old_count, load_count, errors;
	char socket_path[sizeof(((struct sockaddr_un *)0)->sun_path)];
	char expected[TRAY_ICON_THEME_MAX + 1];
	char old[TRAY_ICON_BATCH_MAX][TRAY_ICON_PATH_MAX + 1];
	char loads[PROBE_LOAD_MAX][TRAY_ICON_PATH_MAX + 1];
	probe_identity current, held;
} probe = {.fd = -1};

bool __real_tray_icons_ready(const tray_icons *icons);
bool __real_tray_icons_busy(const tray_icons *icons);
int __real_tray_icons_timeout_ms(const tray_icons *icons);
const tray_sni_model *__real_tray_bus_model(const tray_bus *bus);
void __real_tray_sni_sync(tray_ui *ui);
FvwmPicture *__real_PCacheFvwmPictureAtScale(Display *display, Window window,
	char *image_path, const char *name, FvwmPictureAttributes attributes,
	monitor_scale_factor factor, unsigned int width, unsigned int height,
	unsigned int generation);

bool __wrap_tray_icons_ready(const tray_icons *icons);
bool __wrap_tray_icons_busy(const tray_icons *icons);
int __wrap_tray_icons_timeout_ms(const tray_icons *icons);
const tray_sni_model *__wrap_tray_bus_model(const tray_bus *bus);
void __wrap_tray_sni_sync(tray_ui *ui);
FvwmPicture *__wrap_PCacheFvwmPictureAtScale(Display *display, Window window,
	char *image_path, const char *name, FvwmPictureAttributes attributes,
	monitor_scale_factor factor, unsigned int width, unsigned int height,
	unsigned int generation);

static void probe_close(void)
{
	if (probe.fd >= 0)
		close(probe.fd);
	if (probe.bound)
		unlink(probe.socket_path);
	probe.fd = -1;
	probe.bound = false;
}

static void probe_init(void)
{
	if (probe.initialized)
		return;
	probe.initialized = true;
	const char *path = getenv("FVWM_TRAY_ICON_PROBE");
	if (!path || path[0] != '/' ||
	    strnlen(path, sizeof(probe.socket_path)) >=
	        sizeof(probe.socket_path))
		return;
	char parent[sizeof(probe.socket_path)];
	strcpy(parent, path);
	char *slash = strrchr(parent, '/');
	if (!slash || slash == parent)
		return;
	*slash = 0;
	struct stat info;
	if (lstat(parent, &info) || !S_ISDIR(info.st_mode) ||
	    info.st_uid != geteuid() || (info.st_mode & 0077))
		return;
	int fd = socket(AF_UNIX, SOCK_DGRAM, 0);
	if (fd < 0)
		return;
	int receive_bytes = 8192;
	if (fcntl(fd, F_SETFD, FD_CLOEXEC) < 0 ||
	    fcntl(fd, F_SETFL, O_NONBLOCK) < 0 ||
	    setsockopt(fd, SOL_SOCKET, SO_RCVBUF, &receive_bytes,
	        sizeof(receive_bytes)) < 0)
	{
		close(fd);
		return;
	}
	struct sockaddr_un address = {.sun_family = AF_UNIX};
	strcpy(address.sun_path, path);
	mode_t previous = umask(0077);
	int result = bind(fd, (struct sockaddr *)&address, sizeof(address));
	umask(previous);
	if (result < 0)
	{
		close(fd);
		return; /* Never unlink another instance's endpoint. */
	}
	probe.fd = fd;
	probe.bound = true;
	strcpy(probe.socket_path, path);
	(void)atexit(probe_close);
}

static bool copy_path(char *target, const char *path)
{
	if (!path || strnlen(path, TRAY_ICON_PATH_MAX + 1) >
	    TRAY_ICON_PATH_MAX)
	{
		probe.overflow = true;
		return false;
	}
	strcpy(target, path);
	return true;
}

static void hex_path(const char *path, char *reply)
{
	static const char digits[] = "0123456789abcdef";
	while (*path)
	{
		unsigned char value = (unsigned char)*path++;
		*reply++ = digits[value >> 4];
		*reply++ = digits[value & 15];
	}
	*reply = 0;
}

static bool theme_valid(const char *value)
{
	size_t length = strlen(value);
	if (!length || length > TRAY_ICON_THEME_MAX)
		return false;
	for (size_t i = 0; i < length; i++)
		if (!((value[i] >= 'a' && value[i] <= 'z') ||
		    (value[i] >= 'A' && value[i] <= 'Z') ||
		    (value[i] >= '0' && value[i] <= '9') ||
		    strchr("._-", value[i])))
			return false;
	return true;
}

static void probe_command(char *command, char *reply)
{
	if (!strcmp(command, "hello"))
		snprintf(reply, PROBE_REPLY_MAX, "version=1 pid=%ld",
		    (long)getpid());
	else if (!strcmp(command, "arm"))
	{
		probe.phase = 1;
		probe.old_count = probe.load_count = 0;
		probe.overflow = probe.synced = false;
		probe.held = (probe_identity){0};
		strcpy(reply, "armed=1");
	}
	else if (!strcmp(command, "disarm"))
	{
		probe.phase = 0;
		strcpy(reply, "disarmed=1");
	}
	else if (!strcmp(command, "release"))
	{
		/* Cleanup release works even after an observation overflow/error. */
		probe.phase = 3;
		strcpy(reply, "released=1");
	}
	else if (!strncmp(command, "expect ", 7) &&
	    theme_valid(command + 7))
	{
		strcpy(probe.expected, command + 7);
		probe.synced = false;
		strcpy(reply, "expected=1");
	}
	else if (!strcmp(command, "state"))
	{
		snprintf(reply, PROBE_REPLY_MAX,
		    "phase=%u old=%u loads=%u synced=%u overflow=%u errors=%u "
		    "item=%" PRIu64 " item_generation=%" PRIu64
		    " menu=%" PRIu64 " open=%u ready=%u stale=%u "
		    "held_item=%" PRIu64 " held_item_generation=%" PRIu64
		    " held_menu=%" PRIu64,
		    probe.phase, probe.old_count, probe.load_count,
		    (unsigned int)probe.synced, (unsigned int)probe.overflow, probe.errors,
		    probe.current.item, probe.current.item_generation,
		    probe.current.menu, (unsigned int)probe.current.open,
		    (unsigned int)probe.current.ready, (unsigned int)probe.current.stale,
		    probe.held.item,
		    probe.held.item_generation, probe.held.menu);
	}
	else
	{
		const char *index = NULL;
		bool old = false;
		if (!strncmp(command, "old ", 4))
		{
			old = true;
			index = command + 4;
		}
		else if (!strncmp(command, "load ", 5))
			index = command + 5;
		if (!index || !*index)
			goto invalid;
		for (const char *p = index; *p; p++)
			if (*p < '0' || *p > '9')
				goto invalid;
		errno = 0;
		char *end;
		unsigned long value = strtoul(index, &end, 10);
		if (errno || *end || value >=
		    (old ? probe.old_count : probe.load_count))
			goto invalid;
		hex_path(old ? probe.old[value] : probe.loads[value], reply);
	}
	return;
invalid:
	probe.errors++;
	strcpy(reply, "error=invalid-command");
}

static void probe_service(void)
{
	probe_init();
	if (probe.fd < 0)
		return;
	for (unsigned int i = 0; i < 8; i++)
	{
		char message[257], reply[PROBE_REPLY_MAX];
		struct sockaddr_un sender;
		socklen_t length = sizeof(sender);
		ssize_t count = recvfrom(probe.fd, message, sizeof(message) - 1,
		    MSG_DONTWAIT | MSG_TRUNC, (struct sockaddr *)&sender,
		    &length);
		if (count < 0)
		{
			if (errno == EINTR)
				continue;
			if (errno != EAGAIN && errno != EWOULDBLOCK)
				probe.errors++;
			break;
		}
		if (!count || count >= (ssize_t)sizeof(message) ||
		    length < sizeof(sender.sun_family) || length > sizeof(sender) ||
		    sender.sun_family != AF_UNIX ||
		    memchr(message, 0, (size_t)count))
		{
			probe.errors++;
			continue;
		}
		message[count] = 0;
		probe_command(message, reply);
		(void)sendto(probe.fd, reply, strlen(reply), MSG_DONTWAIT,
		    (struct sockaddr *)&sender, length);
	}
}

const tray_sni_model *__wrap_tray_bus_model(const tray_bus *bus)
{
	const tray_sni_model *model = __real_tray_bus_model(bus);
	probe_service();
	probe.current = (probe_identity){0};
	if (model)
		probe.current = (probe_identity){model->menu.item_id,
		    model->menu.item_generation, model->menu.generation,
		    model->menu.open, model->menu.ready, model->menu.stale};
	return model;
}

void __wrap_tray_sni_sync(tray_ui *ui)
{
	probe_service();
	__real_tray_sni_sync(ui);
	if (probe.fd >= 0 && ui && probe.expected[0] &&
	    !strcmp(ui->icon_theme, probe.expected))
		probe.synced = true;
}

bool __wrap_tray_icons_ready(const tray_icons *icons)
{
	probe_service();
	bool ready = __real_tray_icons_ready(icons);
	if (probe.fd >= 0 && probe.phase == 1 && ready &&
	    probe.current.open && probe.current.ready && !probe.current.stale)
	{
		unsigned int count = tray_icons_count(icons);
		if (count > TRAY_ICON_BATCH_MAX)
			probe.overflow = true;
		else
		{
			for (unsigned int i = 0; i < count; i++)
			{
				const tray_icon_result *result = tray_icons_result(icons, i);
				if (result && result->path[0] &&
				    copy_path(probe.old[probe.old_count], result->path))
					probe.old_count++;
			}
			if (probe.old_count)
			{
				probe.held = probe.current;
				probe.phase = 2;
			}
		}
	}
	return probe.fd >= 0 && probe.phase == 2 ? false : ready;
}

bool __wrap_tray_icons_busy(const tray_icons *icons)
{
	probe_service();
	return probe.fd >= 0 && icons && probe.phase == 2 ? true :
	    __real_tray_icons_busy(icons);
}

int __wrap_tray_icons_timeout_ms(const tray_icons *icons)
{
	probe_service();
	int timeout = __real_tray_icons_timeout_ms(icons);
	/* The private control fd is deliberately outside the production poll ABI.
	 * Bound test-only control latency and avoid a held-ready busy spin. */
	return probe.fd >= 0 && (timeout < 0 || timeout > 20 ||
	    probe.phase == 2) ? 20 : timeout;
}

FvwmPicture *__wrap_PCacheFvwmPictureAtScale(Display *display, Window window,
	char *image_path, const char *name, FvwmPictureAttributes attributes,
	monitor_scale_factor factor, unsigned int width, unsigned int height,
	unsigned int generation)
{
	probe_service();
	if (probe.fd >= 0 && probe.phase && name)
	{
		if (probe.load_count == PROBE_LOAD_MAX)
			probe.overflow = true;
		else if (copy_path(probe.loads[probe.load_count], name))
			probe.load_count++;
	}
	return __real_PCacheFvwmPictureAtScale(display, window, image_path,
	    name, attributes, factor, width, height, generation);
}
