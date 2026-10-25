#ifndef FVWMLIB_FSVG_H
#define FVWMLIB_FSVG_H

#include "config.h"

#ifdef HAVE_RSVG
#define USE_SVG 1
#include <librsvg/rsvg.h>
#include <cairo.h>
#else
#define USE_SVG 0
#endif

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#if USE_SVG
	typedef RsvgHandle			FRsvgHandle;
	typedef cairo_surface_t			Fcairo_surface_t;
	typedef cairo_t				Fcairo_t;

#	define FCAIRO_FORMAT_ARGB32		CAIRO_FORMAT_ARGB32
#	define FCAIRO_STATUS_SUCCESS		CAIRO_STATUS_SUCCESS

#	define FG_OBJECT(a)			G_OBJECT(a)
#	define Fg_object_unref(a)		g_object_unref(a)

#	define Fcairo_create(a)			cairo_create(a)
#	define Fcairo_destroy(a)		cairo_destroy(a)
#	define Fcairo_image_surface_create_for_data(a,b,c,d,e) \
 			cairo_image_surface_create_for_data(a,b,c,d,e)
#	define Fcairo_rotate(a, b)		cairo_rotate(a, b)
#	define Fcairo_scale(a, b, c)		cairo_scale(a, b, c)
#	define Fcairo_status(a)			cairo_status(a)
#	define Fcairo_surface_destroy(a)	cairo_surface_destroy(a)
#	define Fcairo_surface_status(a)		cairo_surface_status(a)
#	define Fcairo_translate(a, b, c)	cairo_translate(a, b, c)
#endif

#endif /* FVWMLIB_FSVG_H */
