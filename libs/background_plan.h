#ifndef FVWM_BACKGROUND_PLAN_H
#define FVWM_BACKGROUND_PLAN_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define BACKGROUND_PATH_MAX 256
#define BACKGROUND_SUFFIX_MAX 362
#define PREVIEW_PLAN_TOKEN_MAX 701
#define BACKGROUND_TRANSPARENT 1U
#define BACKGROUND_MODE_MASK 6U
#define BACKGROUND_PRESERVE_MENU 8U

enum background_mode { BACKGROUND_RETAIN, BACKGROUND_SOLID, BACKGROUND_PNG };
struct background_plan
{
	unsigned flags;
	unsigned menu_opacity, launcher_opacity;
	uint32_t rgb;
	char path[BACKGROUND_PATH_MAX + 1];
};

bool background_plan_parse(const char *text, struct background_plan *plan);
bool background_plan_format(char *output, size_t capacity,
	const struct background_plan *plan);

#endif
