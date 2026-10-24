/* This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */
#include "config.h"

#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#ifdef HAVE_PNG
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <png.h>
#include "libs/capture_policy.h"

#define CAPTURE_PATH_LIMIT 4096

/* This one-shot helper owns its X connection and error policy. No core or
 * resident X state is inherited, and the parent owns its outer deadline. */
static int capture_x_error;

static int capture_error(Display *display, XErrorEvent *event)
{
	capture_x_error = event->error_code;
	return 0;
}

static void capture_png_error(png_structp png, png_const_charp message)
{
	png_longjmp(png, 1);
}

static bool capture_write(FILE *stream, XImage *image, size_t row_bytes,
	const capture_channel channels[3])
{
	png_structp png;
	png_infop info;
	unsigned char *row;
	int x, y;
	volatile bool result = false;

	/* Unlike the fatal allocation helpers, refusal here must remove the
	 * private partial output. Nothing owned by this frame changes after
	 * setjmp except the success result, which is set only after writing. */
	row = malloc(row_bytes);
	if (row == NULL)
		return false;
	png = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL,
		capture_png_error, NULL);
	if (png == NULL)
	{
		free(row);
		return false;
	}
	info = png_create_info_struct(png);
	if (info == NULL)
	{
		png_destroy_write_struct(&png, NULL);
		free(row);
		return false;
	}
	if (setjmp(png_jmpbuf(png)) == 0)
	{
		png_init_io(png, stream);
		png_set_IHDR(png, info, image->width, image->height, 8,
			PNG_COLOR_TYPE_RGB, PNG_INTERLACE_NONE,
			PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);
		png_write_info(png, info);
		for (y = 0; y < image->height; y++)
		{
			for (x = 0; x < image->width; x++)
				capture_rgb(XGetPixel(image, x, y), channels,
					row + (size_t)x * 3);
			png_write_row(png, row);
		}
		png_write_end(png, info);
		result = true;
	}
	png_destroy_write_struct(&png, &info);
	free(row);
	return result;
}

static bool capture_directory(const char *directory)
{
	char path[CAPTURE_PATH_LIMIT + 1];
	char *part;
	struct stat status;

	memcpy(path, directory, strlen(directory) + 1);
	for (part = path + 1; *part != '\0'; part++)
	{
		if (*part != '/')
			continue;
		*part = '\0';
		if (mkdir(path, 0777) != 0 && errno != EEXIST)
			return false;
		*part = '/';
	}
	if (mkdir(path, 0777) != 0 && errno != EEXIST)
		return false;
	return stat(path, &status) == 0 && S_ISDIR(status.st_mode);
}

static int capture(const char *directory)
{
	Display *display = NULL;
	XImage *image = NULL;
	XErrorHandler previous;
	capture_channel channels[3];
	unsigned long masks[3];
	char filename[CAPTURE_PATH_LIMIT + 80];
	char stamp[32];
	const char *error = "Cannot capture the X display";
	FILE *stream = NULL;
	int attempt, screen, width, height, fd = -1;
	int result = 1;
	size_t row_bytes;
	time_t now;
	struct tm local;
	struct timespec pause = {0, 100000000};
	bool created = false;

	for (attempt = 0; attempt < 3; attempt++)
	{
		display = XOpenDisplay(NULL);
		if (display != NULL)
			break;
		if (attempt < 2)
			nanosleep(&pause, NULL);
	}
	if (display == NULL)
	{
		fprintf(stderr, "fvwm-desktop-capture: Cannot open the X display\n");
		return 1;
	}
	previous = XSetErrorHandler(capture_error);
	screen = DefaultScreen(display);
	width = DisplayWidth(display, screen);
	height = DisplayHeight(display, screen);
	/* Admit geometry before asking Xlib to allocate the server image. */
	if (width <= 0 || height <= 0 ||
	    (uint64_t)width * height > FVWM_CAPTURE_MAX_PIXELS)
	{
		error = "Screenshot exceeds the 16 megapixel limit";
		goto done;
	}
	if (DefaultVisual(display, screen)->class != TrueColor)
	{
		error = "Screenshot needs a TrueColor visual";
		goto done;
	}
	image = XGetImage(display, RootWindow(display, screen), 0, 0,
		width, height, AllPlanes, ZPixmap);
	XSync(display, False);
	if (image == NULL || capture_x_error != 0)
		goto done;
	masks[0] = image->red_mask;
	masks[1] = image->green_mask;
	masks[2] = image->blue_mask;
	if (image->width != width || image->height != height ||
	    image->data == NULL || image->format != ZPixmap ||
	    !capture_geometry(width, height, image->xoffset,
	    image->bytes_per_line, image->bits_per_pixel, &row_bytes) ||
	    !capture_channels(masks, image->bits_per_pixel, channels))
	{
		error = "Unsupported screenshot image layout";
		goto done;
	}
	if (!capture_directory(directory))
	{
		error = "Cannot create the screenshot directory";
		goto done;
	}
	now = time(NULL);
	if (localtime_r(&now, &local) == NULL ||
	    strftime(stamp, sizeof(stamp), "%Y%m%d-%H%M%S", &local) == 0)
	{
		error = "Cannot prepare the screenshot name";
		goto done;
	}
	snprintf(filename, sizeof(filename), "%s/FVWM-X-%s-XXXXXX.png",
		directory, stamp);
	/* mkstemps keeps the established suffix and creates an exclusive 0600
	 * file. The output is never a pre-existing user file. */
	fd = mkstemps(filename, 4);
	if (fd < 0)
	{
		error = "Cannot create a private screenshot file";
		goto done;
	}
	created = true;
	if (fcntl(fd, F_SETFD, FD_CLOEXEC) < 0 ||
	    (stream = fdopen(fd, "wb")) == NULL)
	{
		error = "Cannot open the screenshot stream";
		goto done;
	}
	fd = -1; /* stream owns it, including failed-write cleanup. */
	if (!capture_write(stream, image, row_bytes, channels))
	{
		error = "Cannot encode or write the screenshot";
		goto done;
	}
	if (fclose(stream) != 0)
	{
		stream = NULL;
		error = "Cannot finish the screenshot file";
		goto done;
	}
	stream = NULL;
	printf("Screenshot saved: %s\n", filename);
	result = 0;

done:
	if (stream != NULL)
		fclose(stream);
	if (fd >= 0)
		close(fd);
	if (created && result != 0)
		unlink(filename);
	if (image != NULL)
		XDestroyImage(image);
	XCloseDisplay(display);
	XSetErrorHandler(previous);
	if (result != 0)
		fprintf(stderr, "fvwm-desktop-capture: %s\n", error);
	return result;
}
#endif

int main(int argc, char **argv)
{
#ifdef HAVE_PNG
	if (argc != 2 || argv[1][0] == '\0' ||
	    strlen(argv[1]) > CAPTURE_PATH_LIMIT)
	{
		fprintf(stderr, "Capture requires one bounded output directory\n");
		return 2;
	}
	/* Return write refusal through cleanup even under a file-size quota. */
	signal(SIGXFSZ, SIG_IGN);
	return capture(argv[1]);
#else
	fprintf(stderr, "fvwm-desktop-capture: PNG support is unavailable\n");
	return 1;
#endif
}
