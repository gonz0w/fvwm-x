/* -*-c-*- */
/* Copyright (C) 2002 Olivier Chapuis */

#ifndef FVWMLIB_FPNG_H
#define FVWMLIB_FPNG_H

#include "PictureBase.h"

/* The decoder is compiled separately from the PNG-disabled stub. Keep only
 * real library types and the legacy API aliases used by that decoder. */
#if PngSupport
#include <png.h>

typedef png_uint_32 Fpng_uint_32;
typedef png_int_32 Fpng_int_32;
typedef png_uint_16 Fpng_uint_16;
typedef png_int_16 Fpng_int_16;
typedef png_byte Fpng_byte;
typedef png_size_t Fpng_size_t;

typedef png_struct Fpng_struct;
typedef png_structp Fpng_structp;
typedef png_structpp Fpng_structpp;
typedef png_info Fpng_info;
typedef png_infop Fpng_infop;
typedef png_infopp Fpng_infopp;

#define FPNG_BYTES_TO_CHECK 4

#define FPNG_LIBPNG_VER_STRING PNG_LIBPNG_VER_STRING
#define FPNG_COLOR_TYPE_PALETTE PNG_COLOR_TYPE_PALETTE
#define FPNG_COLOR_TYPE_RGB_ALPHA PNG_COLOR_TYPE_RGB_ALPHA
#define FPNG_COLOR_TYPE_GRAY_ALPHA PNG_COLOR_TYPE_GRAY_ALPHA
#define FPNG_COLOR_TYPE_GRAY PNG_COLOR_TYPE_GRAY
#define FPNG_FILLER_BEFORE PNG_FILLER_BEFORE
#define FPNG_FILLER_AFTER PNG_FILLER_AFTER
#define FPNG_INFO_tRNS PNG_INFO_tRNS

#if PNG_LIBPNG_VER >= 10400
#define Fpng_check_sig(a,b) (!png_sig_cmp(a,0,b))
#else
#define Fpng_check_sig(a,b) png_check_sig(a,b)
#endif
#define Fpng_create_read_struct(a,b,c,d) png_create_read_struct(a,b,c,d)
#define Fpng_create_info_struct(a) png_create_info_struct(a)
#define Fpng_destroy_read_struct(a,b,c) png_destroy_read_struct(a,b,c)
#define Fpng_init_io(a,b) png_init_io(a,b)
#define Fpng_read_info(a,b) png_read_info(a,b)
#define Fpng_get_color_type(a, b) png_get_color_type(a, b)
#define Fpng_get_IHDR(a,b,c,d,e,f,g,h,i) png_get_IHDR(a,b,c,d,e,f,g,h,i)
#define Fpng_set_expand(a) png_set_expand(a)
#define Fpng_set_swap_alpha(a) png_set_swap_alpha(a)
#define Fpng_set_filler(a,b,c) png_set_filler(a,b,c)
#define Fpng_set_bgr(a) png_set_bgr(a)
#define Fpng_set_strip_16(a) png_set_strip_16(a)
#define Fpng_set_packing(a) png_set_packing(a)
#define Fpng_set_gray_to_rgb(a) png_set_gray_to_rgb(a)
#define Fpng_get_bit_depth(a,b) png_get_bit_depth(a,b)
#if PNG_LIBPNG_VER >= 10400
#define Fpng_set_gray_1_2_4_to_8(a) png_set_expand_gray_1_2_4_to_8(a)
#else
#define Fpng_set_gray_1_2_4_to_8(a) png_set_gray_1_2_4_to_8(a)
#endif
#define Fpng_get_valid(a,b,c) png_get_valid(a,b,c)
#define Fpng_read_end(a,b) png_read_end(a,b)
#define Fpng_set_interlace_handling(a) png_set_interlace_handling(a)
#define Fpng_read_rows(a,b,c,d) png_read_rows(a,b,c,d)
#define Fpng_read_image(a,b) png_read_image(a,b)

#endif /* PngSupport */
#endif /* FVWMLIB_FPNG_H */
