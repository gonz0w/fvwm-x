/* This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License, version 2 or later. */
#include "config.h"
#include "modules/FvwmTray/tray_icons.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>

static void hex_text(const char *text, char *output)
{
	static const char digits[] = "0123456789abcdef";

	while (*text)
	{
		unsigned char value = (unsigned char)*text++;
		*output++ = digits[value >> 4];
		*output++ = digits[value & 15];
	}
	*output = 0;
}

int main(void)
{
	tray_icon_request requests[TRAY_ICON_BATCH_MAX] = {
	    {.id = UINT64_MAX, .generation = 7, .size = 24, .quarters = 4,
	     .name = "app", .theme = "hicolor", .theme_path = ""},
	    {.id = 2, .generation = 9, .size = 128, .quarters = 16,
	     .name = "other", .theme = "hicolor", .theme_path = "/tmp/icons"}};
	tray_icon_result results[TRAY_ICON_BATCH_MAX];
	char frame[65537], encoded[4097], response[8193];
	const char *directory = "/tmp/fvwm-tray-icons-test";
	const char *path = "/tmp/fvwm-tray-icons-test/.fvwm-xdg-"
	    "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef"
	    ".png";

	size_t length = tray_icons_encode(requests, 2, directory, "png,svg,xpm",
	    frame, sizeof(frame));
	assert(length && strlen(frame) == length);
	assert(strstr(frame, "18446744073709551615 7 24 4 617070 "));
	assert(!strcmp(frame + length - 4, "END\n"));
	assert(!tray_icons_encode(requests, 2, directory, "png,svg,xpm",
	    frame, length));
	assert(!tray_icons_encode(requests, 0, directory, "png,xpm",
	    frame, sizeof(frame)));
	assert(!tray_icons_encode(requests, 9, directory, "png,xpm",
	    frame, sizeof(frame)));
	assert(!tray_icons_encode(requests, 2, directory, "png,png",
	    frame, sizeof(frame)));
	requests[1].id = requests[0].id;
	assert(!tray_icons_encode(requests, 2, directory, "xpm",
	    frame, sizeof(frame)));
	requests[1].id = 2;
	requests[1].size = 7;
	assert(!tray_icons_encode(requests, 2, directory, "xpm",
	    frame, sizeof(frame)));
	requests[1].size = 128;
	requests[1].name = "injected\nline";
	assert(!tray_icons_encode(requests, 2, directory, "xpm",
	    frame, sizeof(frame)));
	requests[1].name = "other";

	hex_text(path, encoded);
	snprintf(response, sizeof(response), "FVWM_TRAY_ICONS 1 2\n"
	    "18446744073709551615 7 %s\n2 9 -\nEND\n", encoded);
	assert(tray_icons_decode(requests, 2, directory,
	    (unsigned char *)response, strlen(response), results));
	assert(results[0].id == UINT64_MAX && results[0].generation == 7);
	assert(!strcmp(results[0].path, path) && !results[1].path[0]);
	requests[0].generation = 8;
	assert(!tray_icons_decode(requests, 2, directory,
	    (unsigned char *)response, strlen(response), results));
	assert(!results[0].id && !results[0].path[0]);
	requests[0].generation = 7;
	assert(!tray_icons_decode(requests, 2, "/tmp/other",
	    (unsigned char *)response, strlen(response), results));
	assert(!tray_icons_decode(requests, 2, directory,
	    (unsigned char *)response, strlen(response) - 1, results));
	strcat(response, "EXTRA\n");
	assert(!tray_icons_decode(requests, 2, directory,
	    (unsigned char *)response, strlen(response), results));
	hex_text("/tmp/fvwm-tray-icons-test/../escape.png", encoded);
	snprintf(response, sizeof(response), "FVWM_TRAY_ICONS 1 2\n"
	    "18446744073709551615 7 %s\n2 9 -\nEND\n", encoded);
	assert(!tray_icons_decode(requests, 2, directory,
	    (unsigned char *)response, strlen(response), results));
	assert(!tray_icons_decode(NULL, 2, directory,
	    (unsigned char *)response, strlen(response), results));
	char svg[512];
	snprintf(svg, sizeof(svg), "%.*s.svg:24x24",
	    (int)strlen(path) - 4, path);
	hex_text(svg, encoded);
	snprintf(response, sizeof(response), "FVWM_TRAY_ICONS 1 2\n"
	    "18446744073709551615 7 %s\n2 9 -\nEND\n", encoded);
	assert(tray_icons_decode(requests, 2, directory,
	    (unsigned char *)response, strlen(response), results));
	requests[0].quarters = 8;
	assert(!tray_icons_decode(requests, 2, directory,
	    (unsigned char *)response, strlen(response), results));
	requests[0].quarters = 4;
	snprintf(svg, sizeof(svg), "%.*s.svg", (int)strlen(path) - 4, path);
	hex_text(svg, encoded);
	snprintf(response, sizeof(response), "FVWM_TRAY_ICONS 1 2\n"
	    "18446744073709551615 7 %s\n2 9 -\nEND\n", encoded);
	assert(!tray_icons_decode(requests, 2, directory,
	    (unsigned char *)response, strlen(response), results));

	tray_icons *icons = tray_icons_create("relative-worker");
	assert(tray_icons_error(icons) && !tray_icons_busy(icons));
	assert(!tray_icons_available(icons));
	assert(tray_icons_read_fd(icons) == -1);
	assert(tray_icons_write_fd(icons) == -1);
	assert(tray_icons_timeout_ms(icons) == -1);
	assert(tray_icons_destroy(icons));
	char temporary[] = "/tmp/fvwm-tray-worker-policy-XXXXXX";
	char worker[256];
	assert(mkdtemp(temporary));
	assert(snprintf(worker, sizeof(worker), "%s/worker", temporary) > 0);
	icons = tray_icons_create(worker);
	assert(!tray_icons_available(icons));
	FILE *file = fopen(worker, "w");
	assert(file);
	assert(fputs("#!/bin/sh\nexit 0\n", file) >= 0);
	assert(fclose(file) == 0);
	assert(chmod(worker, 0600) == 0 && !tray_icons_available(icons));
	assert(chmod(worker, 0700) == 0 && tray_icons_available(icons));
	assert(tray_icons_destroy(icons));
	assert(unlink(worker) == 0 && rmdir(temporary) == 0);
	return 0;
}
