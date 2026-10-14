/* This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */
#include "config.h"
#include "menu_image.h"
#include "fvwm.h"
#include "execcontext.h"
#include "externs.h"
#include "menuroot.h"
#include "menustyle.h"
#include "screen.h"
#include "libs/Picture.h"
#include "libs/picture_async.h"
#include "libs/safemalloc.h"

#include <stdlib.h>

typedef struct
{
	FvwmPicture **slot;
	MenuRootStatic *menu;
	MenuStyle *style;
} menu_image_owner;

static picture_async_commit menu_image_policy(FvwmPicture *view,
	void *context)
{
	menu_image_owner *owner = context;

	if (owner->slot == NULL || *owner->slot != view)
	{
		return PICTURE_ASYNC_DISCARD;
	}
	/* A displayed menu keeps its original dimensions and pixels. A
	 * tear-off has its own immutable snapshots, so it cannot defer the
	 * source menu or unrelated menus indefinitely. */
	if ((owner->menu != NULL && owner->menu->usage_count != 0) ||
	    (owner->style != NULL && ST_USAGE_COUNT(owner->style) != 0))
	{
		return PICTURE_ASYNC_DEFER;
	}
	return PICTURE_ASYNC_APPLY;
}

static void menu_image_settled(FvwmPicture *view, Bool applied, void *context)
{
	menu_image_owner *owner = context;

	if (!applied || menu_image_policy(view, context) != PICTURE_ASYNC_APPLY)
	{
		return;
	}
	if (owner->menu != NULL)
	{
		owner->menu->flags.is_updated = 1;
	}
	if (owner->style != NULL)
	{
		ST_IS_UPDATED(owner->style) = 1;
	}
}

static menu_image_owner *menu_image_binding(FvwmPicture **slot)
{
	FvwmPicture *view = *slot;
	menu_image_owner *owner = picture_async_context(view,
	    menu_image_settled);

	if (owner == NULL && view != NULL && view->is_view && view->pending)
	{
		owner = fxcalloc(1, sizeof(*owner));
		if (!picture_async_bind(view, menu_image_policy,
		    menu_image_settled, owner, free))
		{
			free(owner);
			return NULL;
		}
	}
	if (owner != NULL)
	{
		owner->slot = slot;
	}
	return owner;
}

void menu_image_bind_root(MenuRoot *menu, FvwmPicture **slot)
{
	menu_image_owner *owner = menu_image_binding(slot);

	if (owner != NULL)
	{
		/* Continuation merging can move an item to another static root. */
		owner->menu = menu->s;
		owner->style = NULL;
	}
}

void menu_image_bind_style(MenuStyle *style)
{
	menu_image_owner *owner = menu_image_binding(&ST_SIDEPIC(style));

	if (owner != NULL)
	{
		/* MenuStyle parsing transfers a temporary structure into its
		 * final address. Rebind after that transfer, before returning. */
		owner->style = style;
		owner->menu = NULL;
	}
}

void menu_image_destroy(FvwmPicture **slot)
{
	FvwmPicture *view = *slot;
	menu_image_owner *owner = picture_async_context(view,
	    menu_image_settled);

	if (owner != NULL)
	{
		/* A commit batch may retain this view after the slot is freed. */
		owner->slot = NULL;
		owner->menu = NULL;
		owner->style = NULL;
	}
	*slot = NULL;
	PDestroyFvwmPicture(dpy, view);
}
