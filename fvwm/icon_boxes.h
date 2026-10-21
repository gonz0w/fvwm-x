#ifndef FVWM_ICON_BOXES_H
#define FVWM_ICON_BOXES_H

/*
  For 1 style statement, there can be any number of IconBoxes.
  The name list points at the first one in the chain.
*/
struct icon_boxes_struct
{
	struct icon_boxes_struct *next;       /* next icon_boxes or zero */
	unsigned int use_count;
	int IconBox[4];                       /* x/y x/y for iconbox */
	char *IconScreen;                     /* screen */
	short IconGrid[2];                    /* x incr, y incr */
	char IconSign[4];                     /* because of -0, need to save */
	unsigned is_orphan : 1;
	unsigned IconFlags : 3;               /* some bits */
	unsigned do_free_screen : 1;
	/* IconFill only takes 3 bits.  Defaults are top, left, vert co-ord
	 * first eg: t l = 0,0,0; l t = 0,0,1; b r = 1,1,0 */
#define ICONFILLBOT (1<<0)
#define ICONFILLRGT (1<<1)
#define ICONFILLHRZ (1<<2)
};

#endif /* FVWM_ICON_BOXES_H */
