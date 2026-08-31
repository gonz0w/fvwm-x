/* -*-c-*- */
/* This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see: <http://www.gnu.org/licenses/>
 */

#include "config.h"

#include "libs/defaults.h"
#include "libs/Flocale.h"
#include "libs/Picture.h"
#include "libs/PictureBase.h"
#include "libs/wm_ui_metrics.h"

#include <assert.h>
#include <limits.h>
#include <stddef.h>
#include <stdbool.h>

static void
test_source_handle_width_scales_at_1x_and_2x(void)
{
	monitor_scale_factor one_x = {4};
	monitor_scale_factor two_x = {8};
	int expected = 99;
	int scaled = 99;

	assert(monitor_scale_apply(7, one_x, &expected)
	       == MONITOR_SCALE_CONVERSION_OK);
	assert(expected == 7);
	assert(wm_ui_scale_px(7, one_x, &scaled)
	       == MONITOR_SCALE_CONVERSION_OK);
	assert(scaled == 7);
	assert(scaled == expected);

	assert(monitor_scale_apply(7, two_x, &expected)
	       == MONITOR_SCALE_CONVERSION_OK);
	assert(expected == 14);
	assert(wm_ui_scale_px(7, two_x, &scaled)
	       == MONITOR_SCALE_CONVERSION_OK);
	assert(scaled == 14);
	assert(scaled == expected);
}

static void
test_cache_key_misses_across_quarters_and_generation(void)
{
	wm_ui_cache_key one_x = {
	    .scale_quarters = 4,
	    .target_width = 16,
	    .target_height = 16,
	    .generation = WM_UI_RESOURCE_GENERATION_INITIAL,
	};
	wm_ui_cache_key two_x = one_x;
	wm_ui_cache_key same = one_x;
	wm_ui_cache_key next_generation = one_x;
	wm_ui_resource_generation current;

	two_x.scale_quarters = 8;
	next_generation.generation = 2U;

	assert(!wm_ui_cache_key_equal(&one_x, &two_x));
	assert(wm_ui_cache_key_equal(&one_x, &same));
	assert(!wm_ui_cache_key_equal(&one_x, &next_generation));

	current = wm_ui_resource_generation_current();
	assert(current.generation == WM_UI_RESOURCE_GENERATION_INITIAL);
}

static void
test_chrome_sources_match_apply_at_supported_factors(void)
{
	static const int sources[] = {1, 7, 13};
	static const unsigned int quarters[] = {4, 5, 6, 8, 16};
	size_t i;
	size_t j;

	for (i = 0; i < sizeof(sources) / sizeof(sources[0]); i++) {
		for (j = 0; j < sizeof(quarters) / sizeof(quarters[0]); j++) {
			monitor_scale_factor factor = {quarters[j]};
			int expected = 99;
			int scaled = 99;

			assert(monitor_scale_apply(
				   sources[i], factor, &expected)
			       == MONITOR_SCALE_CONVERSION_OK);
			assert(wm_ui_scale_px(sources[i], factor, &scaled)
			       == MONITOR_SCALE_CONVERSION_OK);
			assert(scaled == expected);
		}
	}
}

static void
test_explicit_title_below_font_is_raised(void)
{
	monitor_scale_factor one_x = {4};
	int extra = 99;
	int thickness = 99;
	int offset = 99;

	assert(wm_ui_scale_px(EXTRA_TITLE_FONT_HEIGHT, one_x, &extra)
	       == MONITOR_SCALE_CONVERSION_OK);
	assert(wm_ui_compose_title_thickness(16, EXTRA_TITLE_FONT_HEIGHT, 0, 0,
		   one_x, &thickness, &offset)
	       == MONITOR_SCALE_CONVERSION_OK);
	assert(thickness == 16 + extra);
	assert(offset == 0);

	assert(wm_ui_compose_title_thickness(16, EXTRA_TITLE_FONT_HEIGHT, 10, 0,
		   one_x, &thickness, &offset)
	       == MONITOR_SCALE_CONVERSION_OK);
	assert(thickness == 16);
	assert(offset == 0);
}

static void
test_explicit_title_above_font_centers(void)
{
	monitor_scale_factor one_x = {4};
	int extra = 99;
	int thickness = 99;
	int offset = 99;
	int base;

	assert(wm_ui_scale_px(EXTRA_TITLE_FONT_HEIGHT, one_x, &extra)
	       == MONITOR_SCALE_CONVERSION_OK);
	base = 16 + extra;
	assert(wm_ui_compose_title_thickness(16, EXTRA_TITLE_FONT_HEIGHT, 40, 0,
		   one_x, &thickness, &offset)
	       == MONITOR_SCALE_CONVERSION_OK);
	assert(thickness == 40);
	assert(offset == (40 - base) / 2);
}

static void
test_overflow_initializes_output_zero(void)
{
	monitor_scale_factor one_x = {4};
	monitor_scale_factor factor = {5};
	int px = 99;
	int thickness = 99;
	int offset = 99;

	assert(wm_ui_scale_px(INT_MAX, factor, &px)
	       == MONITOR_SCALE_CONVERSION_OVERFLOW);
	assert(px == 0);
	assert(wm_ui_scale_px(1, one_x, NULL)
	       == MONITOR_SCALE_CONVERSION_INVALID_ARGUMENT);

	assert(wm_ui_compose_title_thickness(16, EXTRA_TITLE_FONT_HEIGHT,
		   INT_MAX, 0, factor, &thickness, &offset)
	       == MONITOR_SCALE_CONVERSION_OVERFLOW);
	assert(thickness == 0);
	assert(offset == 0);
	assert(wm_ui_compose_title_thickness(16, EXTRA_TITLE_FONT_HEIGHT, 0, 0,
		   one_x, NULL, &offset)
	       == MONITOR_SCALE_CONVERSION_INVALID_ARGUMENT);
}

static void
test_hotspot_scaled_then_clamped(void)
{
	monitor_scale_factor two_x = {8};
	int hot = 99;

	assert(wm_ui_scale_hotspot(4, 16, two_x, &hot)
	       == MONITOR_SCALE_CONVERSION_OK);
	assert(hot == 8);
	assert(wm_ui_scale_hotspot(8, 16, two_x, &hot)
	       == MONITOR_SCALE_CONVERSION_OK);
	assert(hot == 15);
	hot = 99;
	assert(wm_ui_scale_hotspot(4, 0, two_x, &hot)
	       == MONITOR_SCALE_CONVERSION_INVALID_ARGUMENT);
	assert(hot == 0);
	assert(wm_ui_scale_hotspot(4, 16, two_x, NULL)
	       == MONITOR_SCALE_CONVERSION_INVALID_ARGUMENT);
}

static void
test_edge_zero_stays_zero(void)
{
	static const unsigned int quarters[] = {4, 6, 8, 16};
	size_t i;
	int px = 99;

	for (i = 0; i < sizeof(quarters) / sizeof(quarters[0]); i++) {
		monitor_scale_factor factor = {quarters[i]};

		px = 99;
		assert(wm_ui_scale_edge_thickness(0, factor, &px)
		       == MONITOR_SCALE_CONVERSION_OK);
		assert(px == 0);
	}
	assert(wm_ui_scale_edge_thickness(1, (monitor_scale_factor){8}, &px)
	       == MONITOR_SCALE_CONVERSION_OK);
	assert(px == 2);
}

static void
test_move_threshold_and_snap_sources(void)
{
	int px = 99;

	assert(wm_ui_scale_px(
		   DEFAULT_MOVE_THRESHOLD, (monitor_scale_factor){6}, &px)
	       == MONITOR_SCALE_CONVERSION_OK);
	assert(px == 5);
	assert(wm_ui_scale_px(8, (monitor_scale_factor){8}, &px)
	       == MONITOR_SCALE_CONVERSION_OK);
	assert(px == 16);
}

static int
picture_cache_extras_agree(const FvwmPicture *p, unsigned int scale_quarters,
    unsigned int target_width, unsigned int target_height,
    unsigned int generation)
{
	return p->scale_quarters == scale_quarters
	    && p->target_width == target_width
	    && p->target_height == target_height
	    && p->generation == generation;
}

static void
test_picture_lookup_requires_scale_and_generation(void)
{
	FvwmPicture cached = {0};
	FvwmPictureAttributes fpa = {0};
	unsigned int scale_quarters = MONITOR_SCALE_QUARTERS_MIN;
	unsigned int target_width = 0;
	unsigned int target_height = 0;
	unsigned int generation = WM_UI_RESOURCE_GENERATION_INITIAL;

	cached.fpa_mask = 0;
	cached.scale_quarters = MONITOR_SCALE_QUARTERS_MIN;
	cached.target_width = 0;
	cached.target_height = 0;
	cached.generation = WM_UI_RESOURCE_GENERATION_INITIAL;

	assert(offsetof(FvwmPicture, scale_quarters)
	    > offsetof(FvwmPicture, alpha));
	assert(PICTURE_FPA_AGREE((&cached), fpa));
	assert(picture_cache_extras_agree(&cached, scale_quarters,
	    target_width, target_height, generation));

	/* Same name/stamp/fpa at 2x must miss the 1x pixmap. */
	assert(!picture_cache_extras_agree(&cached, 8U, target_width,
	    target_height, generation));
	/* A later generation must miss even at 1x and 0,0. */
	assert(!picture_cache_extras_agree(&cached, scale_quarters,
	    target_width, target_height, 2U));
}

static void
test_pcache_wrapper_forwards_1x_generation_1(void)
{
	FvwmPicture *(*unscaled)(Display *, Window, char *, const char *,
	    FvwmPictureAttributes) = PCacheFvwmPicture;
	FvwmPicture *(*scaled)(Display *, Window, char *, const char *,
	    FvwmPictureAttributes, monitor_scale_factor, unsigned, unsigned,
	    unsigned) = PCacheFvwmPictureAtScale;

	assert(unscaled != NULL);
	assert(scaled != NULL);
}

static int
flocale_cache_extras_agree(const FlocaleFont *flf, unsigned int scale_quarters,
    unsigned int generation)
{
	return flf->scale_quarters == scale_quarters
	    && flf->generation == generation;
}

static void
test_flocale_lookup_requires_scale_and_generation(void)
{
	FlocaleFont cached = {0};

	cached.scale_quarters = MONITOR_SCALE_QUARTERS_MIN;
	cached.generation = WM_UI_RESOURCE_GENERATION_INITIAL;

	assert(offsetof(FlocaleFont, scale_quarters)
	    > offsetof(FlocaleFont, count));
	assert(flocale_cache_extras_agree(&cached, MONITOR_SCALE_QUARTERS_MIN,
	    WM_UI_RESOURCE_GENERATION_INITIAL));

	/* Same name at 2x must miss the 1x face. */
	assert(!flocale_cache_extras_agree(&cached, 8U,
	    WM_UI_RESOURCE_GENERATION_INITIAL));
	/* A later generation must miss even at 1x. */
	assert(!flocale_cache_extras_agree(&cached, MONITOR_SCALE_QUARTERS_MIN,
	    2U));
}

static void
test_flocale_wrapper_forwards_1x_generation_1(void)
{
	FlocaleFont *(*unscaled)(Display *, char *, char *) = FlocaleLoadFont;
	FlocaleFont *(*scaled)(Display *, char *, char *, monitor_scale_factor,
	    unsigned) = FlocaleLoadFontAtScale;

	assert(unscaled != NULL);
	assert(scaled != NULL);
}

static void
test_fft_get_font_at_scale_exists(void)
{
	FftFontType *(*unscaled)(Display *, char *, char *) = FftGetFont;
	FftFontType *(*scaled)(Display *, char *, char *,
	    monitor_scale_factor) = FftGetFontAtScale;

	assert(unscaled != NULL);
	assert(scaled != NULL);
}

static void
test_pimage_load_fvwm_picture_from_file_takes_scale_and_targets(void)
{
	FvwmPicture *(*loader)(Display *, Window, char *,
	    FvwmPictureAttributes, monitor_scale_factor, unsigned,
	    unsigned) = PImageLoadFvwmPictureFromFile;

	assert(loader != NULL);
}

static void
test_zero_targets_convert_intrinsic_with_factor(void)
{
	monitor_scale_factor one_x = {MONITOR_SCALE_QUARTERS_MIN};
	monitor_scale_factor two_x = {8};
	int width = 99;
	int height = 99;

	/* 0,0 destination is wm_ui_scale_px of intrinsic. 1x is intrinsic. */
	assert(wm_ui_scale_px(48, one_x, &width)
	       == MONITOR_SCALE_CONVERSION_OK);
	assert(width == 48);
	assert(wm_ui_scale_px(32, one_x, &height)
	       == MONITOR_SCALE_CONVERSION_OK);
	assert(height == 32);
	assert(wm_ui_scale_px(48, two_x, &width)
	       == MONITOR_SCALE_CONVERSION_OK);
	assert(width == 96);
	assert(wm_ui_scale_px(32, two_x, &height)
	       == MONITOR_SCALE_CONVERSION_OK);
	assert(height == 64);
}

static void
test_menu_and_icon_wanted_sizes(void)
{
	monitor_scale_factor two_x = {8};
	int px = 99;

	assert(wm_ui_scale_px(16, two_x, &px) == MONITOR_SCALE_CONVERSION_OK);
	assert(px == 32);
	assert(wm_ui_scale_px(56, two_x, &px) == MONITOR_SCALE_CONVERSION_OK);
	assert(px == 112);
	assert(wm_ui_scale_px(MENU_TRIANGLE_WIDTH, two_x, &px)
	       == MONITOR_SCALE_CONVERSION_OK);
	assert(px == 10);
	assert(wm_ui_scale_px(DEFAULT_MENU_BORDER_WIDTH, two_x, &px)
	       == MONITOR_SCALE_CONVERSION_OK);
	assert(px == 4);
}

int
main(void)
{
	test_source_handle_width_scales_at_1x_and_2x();
	test_cache_key_misses_across_quarters_and_generation();
	test_chrome_sources_match_apply_at_supported_factors();
	test_explicit_title_below_font_is_raised();
	test_explicit_title_above_font_centers();
	test_overflow_initializes_output_zero();
	test_hotspot_scaled_then_clamped();
	test_edge_zero_stays_zero();
	test_move_threshold_and_snap_sources();
	test_menu_and_icon_wanted_sizes();
	test_picture_lookup_requires_scale_and_generation();
	test_pcache_wrapper_forwards_1x_generation_1();
	test_flocale_lookup_requires_scale_and_generation();
	test_flocale_wrapper_forwards_1x_generation_1();
	test_fft_get_font_at_scale_exists();
	test_pimage_load_fvwm_picture_from_file_takes_scale_and_targets();
	test_zero_targets_convert_intrinsic_with_factor();

	return 0;
}
