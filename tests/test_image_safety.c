#include "config.h"
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

static void test_exact_icon_records(void)
{
	const uint32_t data[] = { 1, 1, 11, 2, 1, 21, 22,
		1, 1, 31, 1, 2, 41, 42 };
	const size_t starts[] = { 0, 3, 10 };
	const size_t lengths[] = { 3, 4, 4 };
	const uint32_t widths[] = { 1, 2, 1 };
	const uint32_t heights[] = { 1, 1, 2 };
	const uint32_t malformed[][3] = { { 0, 1, 0 },
		{ 1, 0, 0 }, { UINT32_MAX, UINT32_MAX, 0 },
		{ 65536, 65536, 0 }, { 2, 2, 1 } };
	size_t start, words;

	/* Every prefix: exact matching must not accept an incomplete record.
	 * Duplicates choose the first; a later unconsumed suffix is irrelevant. */
	for (size_t cut = 0; cut <= sizeof(data) / sizeof(*data); cut++)
		for (unsigned wanted = 0; wanted < 3; wanted++)
		{
			FEwmhIconResult result;

			start = words = 99;
			result = FEwmhIconFindExact(data, cut, widths[wanted],
				heights[wanted], &start, &words);
			if (cut >= starts[wanted] + lengths[wanted])
			{
				assert(result == F_EWMH_ICON_FOUND);
				assert(start == starts[wanted] &&
					words == lengths[wanted]);
			}
			else
			{
				assert(result != F_EWMH_ICON_FOUND);
				assert(start == 0 && words == 0);
			}
		}
	for (unsigned i = 0; i < sizeof(malformed) / sizeof(*malformed); i++)
	{
		start = words = 99;
		assert(FEwmhIconFindExact(malformed[i], 3, 2, 2,
			&start, &words) == F_EWMH_ICON_INVALID);
		assert(start == 0 && words == 0);
	}
	assert(FEwmhIconFindExact(data, 14, 3, 3, &start, &words) ==
		F_EWMH_ICON_NOT_FOUND);
	assert(FEwmhIconFindExact(NULL, 0, 1, 1, &start, &words) ==
		F_EWMH_ICON_NOT_FOUND);
	assert(FEwmhIconFindExact(NULL, 1, 1, 1, &start, &words) ==
		F_EWMH_ICON_INVALID);
	assert(FEwmhIconFindExact(data, 14, 1, 1, NULL, &words) ==
		F_EWMH_ICON_INVALID && words == 0);
	assert(FEwmhIconFindExact(data, 14, 1, 1, &start, NULL) ==
		F_EWMH_ICON_INVALID && start == 0);
}

static void test_icon_output_size(void)
{
	const size_t ceiling = 64U * 1024U * 1024U + 64U * 1024U;
	size_t pixels, words;

	assert(FEwmhIconSizeIsSafe(2, 3, 7, ceiling, &pixels, &words));
	assert(pixels == 6 && words == 15);
	assert(FEwmhIconSizeIsSafe(4096, 4096, 16382, ceiling,
		&pixels, &words));
	assert(pixels == 16777216 && words == ceiling / 4);
	assert(!FEwmhIconSizeIsSafe(4096, 4096, 16383, ceiling,
		&pixels, &words) && !pixels && !words);
	assert(!FEwmhIconSizeIsSafe(0, 1, 0, ceiling, &pixels, &words));
	assert(!FEwmhIconSizeIsSafe(1, 0, 0, ceiling, &pixels, &words));
	assert(!FEwmhIconSizeIsSafe(4097, 1, 0, ceiling, &pixels, &words));
	assert(!FEwmhIconSizeIsSafe(UINT64_MAX, UINT64_MAX, 0, ceiling,
		&pixels, &words));
	assert(!FEwmhIconSizeIsSafe(1, 1, SIZE_MAX, ceiling, &pixels, &words));
	assert(!FEwmhIconSizeIsSafe(1, 1, 0, 11, &pixels, &words));
	assert(FEwmhIconSizeIsSafe(1, 1, 0, 12, &pixels, &words));
	assert(!FEwmhIconSizeIsSafe(1, 1, 0, ceiling, NULL, &words));
	assert(!FEwmhIconSizeIsSafe(1, 1, 0, ceiling, &pixels, NULL));
}

int main(void)
{
	test_image_dimensions();
	test_exact_icon_records();
	test_icon_output_size();
	test_ewmh_icon_selection();
	test_ewmh_icon_rejects_malformed_data();
	test_ewmh_icon_skips_oversized_entries();

	return 0;
}
