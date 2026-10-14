/* -*-c-*- */

#ifndef FVWMLIB_PICTURE_H
#define FVWMLIB_PICTURE_H

#include "PictureBase.h"
#include "PictureImageLoader.h"
#include "libs/monitor_scale.h"

/* Optional process-local loader used by the core's deferred owner. On success
 * it takes path ownership; on failure the caller frees path. Cache lookup
 * precedes this hook. Utilities/modules leave it unset. */
typedef FvwmPicture *(*picture_loader_fn)(Display *dpy, Window drawable,
	char *path, FvwmPictureAttributes attributes, monitor_scale_factor factor,
	unsigned target_width, unsigned target_height, unsigned generation,
	Bool cache, void *context);
void picture_set_loader(picture_loader_fn loader, void *context);
/* Publish an owned count-one immutable result with its complete cache key.
 * Views retain it through ordinary reference counting. */
void picture_cache_publish(FvwmPicture *picture);

/** Manipulating FvwmPictures **/

/**
 * For PGetFvwmPicture() and PCacheFvwmPicture(), setting
 * ImagePath to NULL means "search the default image path".
 **/

/* <pubfunc>PGetFvwmPicture
 * <description>
 * Return an FvwmPicture loaded from the file pictureName found in the
 * ImagePath.. If ImagePath is NULL the default image path is used.
 * </description>
 */
FvwmPicture* PGetFvwmPicture(
	Display* dpy, Window win, char* ImagePath, const char* pictureName,
	FvwmPictureAttributes fpa);

/* <pubfunc>PFreeFvwmPictureData
 * <description>
 * Just free the data allocated by PGetFvwmPicture. This function does not
 * Free the pixmaps for example.
 * </description>
 */
void PFreeFvwmPictureData(FvwmPicture *p);

/* <pubfunc>PCacheFvwmPicture
 * <description>
 * Return the FvwmPicture loaded from the file pictureName found in the
 * ImagePath. Fisrt the picture is searched in the FvwmPicture cache (so
 * if this picture has been already loaded it is not loaded again and a
 * weight is added to the found picture). If the picture is not in the cache
 * it is loaded from the file and added to the FvwmPicture cache.
 * If ImagePath is NULL the default image path is used.
 * </description>
 */
FvwmPicture* PCacheFvwmPicture(
	Display *dpy, Window win, char* ImagePath, const char* pictureName,
	FvwmPictureAttributes fpa);

/* <pubfunc>PCacheFvwmPictureAtScale
 * <description>
 * Same as PCacheFvwmPicture, keyed additionally by scale quarters, optional
 * target dimensions, and resource generation. Target 0,0 means intrinsic
 * decoded size after the scale factor is applied to the source raster.
 * </description>
 */
FvwmPicture* PCacheFvwmPictureAtScale(
	Display *dpy, Window win, char* ImagePath, const char* pictureName,
	FvwmPictureAttributes fpa, monitor_scale_factor factor,
	unsigned target_width, unsigned target_height, unsigned generation);

/* <pubfunc>PLoadFvwmPictureFromPixmap
 * <description>
 * Return a FvwmPicture from the given data.
 * </description>
 */
FvwmPicture *PLoadFvwmPictureFromPixmap(
	Display *dpy, Window win, char *name, Pixmap pixmap, Pixmap mask,
	Pixmap alpha, int width, int height, int nalloc_pixels,
	Pixel *alloc_pixels, int no_limit);

/* <pubfunc>PDestroyFvwmPicture
 * <description>
 * Return a FvwmPicture from the given data. The picture is added to the
 * FvwmPicture cache. This is not really useful as it is not possible
 * to really cache a picture from the given data.
 * </description>
 */
FvwmPicture *PCacheFvwmPictureFromPixmap(
	Display *dpy, Window win, char *name, Pixmap pixmap,
	Pixmap mask, Pixmap alpha, int width, int height, int nalloc_pixels,
	Pixel *alloc_pixels, int no_limit);

/* <pubfunc>PDestroyFvwmPicture
 * <description>
 * Remove a weight to the FvwmPicture p from the FvwmPicture cache.
 * If the weight is zero the allocated datas from p are freed
 * </description>
 */
void PDestroyFvwmPicture(Display *dpy, FvwmPicture *p);

/* <pubfunc>PCloneFvwmPicture
 * <description>
 * Duplicate an already allocated FvwmPicture in the FvwmPicture cache
 * (a weight is added to the picture).
 * </description>
 */
FvwmPicture *PCloneFvwmPicture(FvwmPicture *pic);


void PicturePrintImageCache(int verbose);

#endif
