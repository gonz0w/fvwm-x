#ifndef FVWM_MENU_IMAGE_H
#define FVWM_MENU_IMAGE_H

#include "libs/PictureBase.h"

struct MenuRoot;
struct MenuStyle;

/* Slots and parent state are borrowed only until explicit owner teardown. */
void menu_image_bind_root(struct MenuRoot *menu, FvwmPicture **slot);
void menu_image_bind_style(struct MenuStyle *style);
void menu_image_destroy(FvwmPicture **slot);

#endif
