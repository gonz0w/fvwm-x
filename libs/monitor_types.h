/* Complete monitor record for topology, workspace and geometry owners. */
#ifndef FVWMLIB_MONITOR_TYPES_H
#define FVWMLIB_MONITOR_TYPES_H

#include "FScreen.h"

struct monitor
{
	struct screen_info  *si;
	int		     flags;
	int		     emit;
	int		     number;
	int		     dx, dy;
	bool		     is_prev, was_primary;
	monitor_scale_result effective_scale;

	/* info for some desktops; the first entries should be generic info
	 * correct for any desktop not in the list
	 */
	DesktopsInfo *Desktops;

	/* Information about EWMH. */
	struct
	{
		unsigned NumberOfDesktops;
		unsigned MaxDesktops;
		unsigned CurrentNumberOfDesktops;
		Bool	 NeedsToCheckDesk;

		struct
		{
			int left;
			int right;
			int top;
			int bottom;
		} BaseStrut;

	} ewmhc;

	struct
	{
		bool top;
		bool bottom;
		bool left;
		bool right;
	} edge;

	struct
	{
		int VxMax;
		int VyMax;
		int Vx;
		int Vy;

		int edge_thickness;
		int last_edge_thickness;

		int EdgeScrollX;
		int EdgeScrollY;

		int CurrentDesk;
		int prev_page_x;
		int prev_page_y;
		int prev_desk;
		int prev_desk_and_page_desk;
		int prev_desk_and_page_page_x;
		int prev_desk_and_page_page_y;

		bool is_swapping;
	} virtual_scr;

	PanFrame PanFrameTop;
	PanFrame PanFrameLeft;
	PanFrame PanFrameRight;
	PanFrame PanFrameBottom;
	bool	 pan_frames_mapped;

	RB_ENTRY(monitor) entry;
	TAILQ_ENTRY(monitor) oentry;
};

#endif
