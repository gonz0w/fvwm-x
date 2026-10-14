/* This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or any later version.
 */
#include "config.h"
#include "libs/FScreen.h"
#include "libs/FShape.h"
#include "libs/Module.h"
#include "libs/fvwmlib.h"
#include "fvwm/fvwm.h"
#include "FvwmPager.h"
#include "pager_shape.h"
#include "pager_geometry.h"

/* Empty cells require no X rectangles; work follows displayed desks only. */
void pager_update_window_shape(void)
{
	if (!ShapeLabels || label_h == 0 || fAlwaysCurrentDesk)
		return;

	int cnt;
	int64_t x_pos, y_pos, height;
	XRectangle *shape;
	struct fpmonitor *fp;
	shape = fxmalloc(ndesks * sizeof (XRectangle));

	for (cnt = 0; cnt < ndesks; cnt++) {
		x_pos = (int64_t)(cnt % Columns) * (desk_w + 1);
		y_pos = (int64_t)(LabelsBelow ? 0 : label_h) +
			(int64_t)(cnt / Columns) * (desk_h + 2 + label_h);
		/* Default shape/hide labels. */
		height = desk_h + 2;
		shape[cnt].x = pager_pixel_position(x_pos);
		shape[cnt].width = pager_pixel_size((int64_t)desk_w + 1);

		/* Show labels for active monitors. */
		TAILQ_FOREACH(fp, &fp_monitor_q, entry) {
			if (fp->disabled ||
			    (monitor_to_track != NULL &&
			    monitor_to_track != fp) ||
			    cnt + desk1 !=
			    fp->m->virtual_scr.CurrentDesk)
				continue;

			y_pos -= (LabelsBelow) ? 0 : label_h;
			height += label_h;
		}
		shape[cnt].y = pager_pixel_position(y_pos);
		shape[cnt].height = pager_pixel_size(height);
	}
	FShapeCombineRectangles(dpy, Scr.pager_w, FShapeBounding,
				0, 0, shape, cnt, FShapeSet, 0);
	free(shape);
}
