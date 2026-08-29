#include "ImageSafety.h"

#include <assert.h>
#include <stdint.h>

static void test_image_dimensions(void)
{
	size_t pixels = 0;

	assert(FImageSizeIsSafe(1920, 1080, 32768, 64U * 1024U * 1024U,
		&pixels));
	assert(pixels == 1920U * 1080U);
	assert(!FImageSizeIsSafe(0, 1080, 32768, 64U * 1024U * 1024U,
		&pixels));
	assert(!FImageSizeIsSafe(32769, 1, 32768, 64U * 1024U * 1024U,
		&pixels));
	assert(!FImageSizeIsSafe(16384, 16384, 32768,
		64U * 1024U * 1024U, &pixels));
}

static void test_ewmh_icon_selection(void)
{
	const uint32_t icons[] = {
		1, 1, 0xff000000,
		2, 2, 1, 2, 3, 4,
	};
	size_t start = 0;
	uint32_t width = 0;
	uint32_t height = 0;

	assert(FEwmhIconFindBest(
		icons, sizeof(icons) / sizeof(icons[0]), 2, 2, 4096,
		16U * 1024U * 1024U, &start, &width, &height));
	assert(start == 5);
	assert(width == 2);
	assert(height == 2);
}

static void test_ewmh_icon_rejects_malformed_data(void)
{
	const uint32_t truncated[] = {2, 2, 1, 2, 3};
	const uint32_t overflow[] = {65536, 65536};
	const uint32_t zero_size[] = {0, 1};
	const uint32_t trailing_data[] = {1, 1, 1, 2};
	size_t start = 99;
	uint32_t width = 99;
	uint32_t height = 99;

	assert(!FEwmhIconFindBest(
		truncated, sizeof(truncated) / sizeof(truncated[0]), 2, 2,
		4096, 16U * 1024U * 1024U, &start, &width, &height));
	assert(start == 0 && width == 0 && height == 0);
	assert(!FEwmhIconFindBest(
		overflow, sizeof(overflow) / sizeof(overflow[0]), 2, 2,
		4096, 16U * 1024U * 1024U, &start, &width, &height));
	assert(!FEwmhIconFindBest(
		zero_size, sizeof(zero_size) / sizeof(zero_size[0]), 2, 2,
		4096, 16U * 1024U * 1024U, &start, &width, &height));
	assert(!FEwmhIconFindBest(
		trailing_data, sizeof(trailing_data) / sizeof(trailing_data[0]),
		2, 2, 4096, 16U * 1024U * 1024U,
		&start, &width, &height));
}

static void test_ewmh_icon_skips_oversized_entries(void)
{
	const uint32_t icons[] = {
		3, 1, 1, 2, 3,
		1, 1, 4,
	};
	size_t start = 0;
	uint32_t width = 0;
	uint32_t height = 0;

	assert(FEwmhIconFindBest(
		icons, sizeof(icons) / sizeof(icons[0]), 1, 1, 2, 4,
		&start, &width, &height));
	assert(start == 7);
	assert(width == 1 && height == 1);
}

int main(void)
{
	test_image_dimensions();
	test_ewmh_icon_selection();
	test_ewmh_icon_rejects_malformed_data();
	test_ewmh_icon_skips_oversized_entries();

	return 0;
}
