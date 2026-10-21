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

/* ---------------------------- included header files ---------------------- */

#include "config.h"

#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <limits.h>
#include <stdio.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "add_window.h"
#include "bindings.h"
#include "borders.h"
#include "builtins.h"
#include "colorset.h"
#include "commands.h"
#include "cursor.h"
#include "decorations.h"
#include "events.h"
#include "ewmh.h"
#include "externs.h"
#include "focus.h"
#include "frame.h"
#include "functions.h"
#include "fvwm.h"
#include "infostore.h"
#include "libs/ClientMsg.h"
#include "libs/ColorUtils.h"
#include "libs/FEvent.h"
#include "libs/FGettext.h"
#include "libs/monitor_types.h"
#include "libs/Ficonv.h"
#include "libs/flocale_font_types.h"
#include "libs/Grab.h"
#include "libs/Graphics.h"
#include "libs/Parse.h"
#include "libs/Picture.h"
#include "libs/PictureUtils.h"
#include "libs/picture_async.h"
#include "libs/cJSON.h"
#include "libs/charmap.h"
#include "libs/envvar.h"
#include "libs/fvwm_x11.h"
#include "libs/fvwmlib.h"
#include "libs/fvwmsignal.h"
#include "libs/log.h"
#include "libs/monitor_topology.h"
#include "libs/setpgrp.h"
#include "libs/wcontext.h"
#include "libs/wild.h"
#include "libs/wm_ui_metrics.h"
#include "menus.h"
#include "misc.h"
#include "module_interface.h"
#include "monitor_scale_config.h"
#include "move_resize.h"
#include "screen.h"
#include "status_json.h"
#include "status_pipe.h"
#include "style.h"
#include "update.h"
#include "virtual.h"
#include "libs/picture_types.h"
/* ---------------------------- local definitions -------------------------- */

static void
status_init_pipe(void);

/* ---------------------------- local macros ------------------------------- */

/* ---------------------------- imports ------------------------------------ */

extern float rgpctMovementDefault[32];
extern int   cpctMovementDefault;
extern int   cmsDelayDefault;

/* ---------------------------- included code files ------------------------ */

/* ---------------------------- local types -------------------------------- */
typedef enum
{
	FakeMouseEvent,
	FakeKeyEvent
} FakeEventType;
/* ---------------------------- forward declarations ----------------------- */

/* ---------------------------- local variables ---------------------------- */

static char *exec_shell_name = "/bin/sh";

/* button state strings must match the enumerated states */
static char *button_states[BS_MaxButtonStateName + 1] = {"ActiveUp",
    "ActiveDown", "InactiveUp", "InactiveDown", "ToggledActiveUp",
    "ToggledActiveDown", "ToggledInactiveUp", "ToggledInactiveDown", "Active",
    "Inactive", "ToggledActive", "ToggledInactive", "AllNormal", "AllToggled",
    "AllActive", "AllInactive", "AllUp", "AllDown", "AllActiveUp",
    "AllActiveDown", "AllInactiveUp", "AllInactiveDown", NULL};

/* ---------------------------- exported variables (globals) --------------- */

char	    *ModulePath	   = FVWM_MODULEDIR;
int	     moduleTimeout = DEFAULT_MODULE_TIMEOUT;
static fvwm_status_pipe status_pipe = FVWM_STATUS_PIPE_INIT;
static bool status_dirty;

#define STATUSENV "FVWM_STATUS_PIPE"

/* ---------------------------- local functions ---------------------------- */

int
status_pending_fd(void)
{
	return status_pipe.pending != NULL ? status_pipe.fd : -1;
}

void
status_flush(void)
{
	if (status_pipe_flush(&status_pipe) < 0)
	{
		fvwm_debug(__func__, "Status write failed: %s",
		    strerror(errno));
		status_close();
	}
	else if (status_pipe.pending == NULL && status_dirty)
	{
		status_send();
	}
}

void
status_send(void)
{
	cJSON				*msg = NULL;
	struct monitor			*m_cur;
	const monitor_topology_snapshot *topology;
	char				*as_json = NULL;
	const char			*desktop_mode;
	int flushed;

	if (status_pipe.fd < 0 || !Scr.flags.are_windows_captured)
		return;

	status_dirty = true;
	flushed = status_pipe_flush(&status_pipe);
	if (flushed < 0)
	{
		fvwm_debug(__func__, "Status write failed: %s",
		    strerror(errno));
		status_close();
		return;
	}
	if (flushed == 0)
	{
		return;
	}

	m_cur	     = monitor_get_current();
	topology     = FScreenGetTopologySnapshot();
	desktop_mode = monitor_mode == MONITOR_TRACKING_G && is_tracking_shared
			   ? "shared"
		       : monitor_mode == MONITOR_TRACKING_G ? "global"
		       : monitor_mode == MONITOR_TRACKING_M ? "per-monitor"
							    : "unknown";
	msg = fvwm_status_json_create(m_cur->si->name, desktop_mode, topology,
	    &monitor_q, get_focus_window(), desk_get_fw_urgent,
	    desk_get_fw_count);
	if (msg == NULL)
		return;
	if ((as_json = cJSON_PrintUnformatted(msg)) == NULL)
		goto out;

	if (!status_pipe_queue(&status_pipe, as_json, strlen(as_json)))
	{
		fvwm_debug(__func__, "Status record refused: %s",
		    strerror(errno));
	}
	else
	{
		status_dirty = false;
		if (status_pipe_flush(&status_pipe) < 0)
		{
			fvwm_debug(__func__, "Status write failed: %s",
			    strerror(errno));
			status_close();
		}
	}

out:
	cJSON_free(as_json);
	cJSON_Delete(msg);
}

void
status_close(void)
{
	const char *published = getenv(STATUSENV);

	if (published != NULL && status_pipe.path != NULL &&
	    strcmp(published, status_pipe.path) == 0)
	{
		flib_unsetenv(STATUSENV);
	}
	status_pipe_close(&status_pipe);
	status_dirty = false;
}

static void
status_init_pipe(void)
{
	const char *directory = getenv("TMPDIR");

	if (directory == NULL)
	{
		directory = "/tmp";
	}
	if (!status_pipe_open(&status_pipe, directory))
	{
		fvwm_debug(__func__, "Status pipe creation failed: %s",
		    strerror(errno));
		return;
	}
	if (setenv(STATUSENV, status_pipe.path, 1) != 0)
	{
		fvwm_debug(__func__, "Status environment failed: %s",
		    strerror(errno));
		status_close();
		return;
	}
	fvwm_debug(__func__, "Pipe opened: %s (fd: %d)", status_pipe.path,
	    status_pipe.fd);
}

/*
 *
 * Reads a title button description (veliaa@rpi.edu)
 *
 */
static char *
ReadTitleButton(char *s, TitleButton *tb, Bool append, int button)
{
	char	 *end = NULL;
	char	 *spec;
	char	 *t;
	int	  i;
	int	  bs;
	int	  bs_start, bs_end;
	int	  pstyle = 0;
	DecorFace tmpdf;

	Bool multiple;
	int  use_mask = 0;
	int  set_mask = 0;

	s = SkipSpaces(s, NULL, 0);
	t = GetNextTokenIndex(s, button_states, 0, &bs);
	if (bs != BS_All) {
		s = SkipSpaces(t, NULL, 0);
	}

	if (bs == BS_All) {
		use_mask = 0;
		set_mask = 0;
	} else if (bs == BS_Active) {
		use_mask = BS_MASK_INACTIVE | BS_MASK_TOGGLED;
		set_mask = 0;
	} else if (bs == BS_Inactive) {
		use_mask = BS_MASK_INACTIVE | BS_MASK_TOGGLED;
		set_mask = BS_MASK_INACTIVE;
	} else if (bs == BS_ToggledActive) {
		use_mask = BS_MASK_INACTIVE | BS_MASK_TOGGLED;
		set_mask = BS_MASK_TOGGLED;
	} else if (bs == BS_ToggledInactive) {
		use_mask = BS_MASK_INACTIVE | BS_MASK_TOGGLED;
		set_mask = BS_MASK_INACTIVE | BS_MASK_TOGGLED;
	} else if (bs == BS_AllNormal) {
		use_mask = BS_MASK_TOGGLED;
		set_mask = 0;
	} else if (bs == BS_AllToggled) {
		use_mask = BS_MASK_TOGGLED;
		set_mask = BS_MASK_TOGGLED;
	} else if (bs == BS_AllActive) {
		use_mask = BS_MASK_INACTIVE;
		set_mask = 0;
	} else if (bs == BS_AllInactive) {
		use_mask = BS_MASK_INACTIVE;
		set_mask = BS_MASK_INACTIVE;
	} else if (bs == BS_AllUp) {
		use_mask = BS_MASK_DOWN;
		set_mask = 0;
	} else if (bs == BS_AllDown) {
		use_mask = BS_MASK_DOWN;
		set_mask = BS_MASK_DOWN;
	} else if (bs == BS_AllActiveUp) {
		use_mask = BS_MASK_INACTIVE | BS_MASK_DOWN;
		set_mask = 0;
	} else if (bs == BS_AllActiveDown) {
		use_mask = BS_MASK_INACTIVE | BS_MASK_DOWN;
		set_mask = BS_MASK_DOWN;
	} else if (bs == BS_AllInactiveUp) {
		use_mask = BS_MASK_INACTIVE | BS_MASK_DOWN;
		set_mask = BS_MASK_INACTIVE;
	} else if (bs == BS_AllInactiveDown) {
		use_mask = BS_MASK_INACTIVE | BS_MASK_DOWN;
		set_mask = BS_MASK_INACTIVE | BS_MASK_DOWN;
	}

	if ((bs & BS_MaxButtonStateMask) == bs) {
		multiple = False;
		bs_start = bs;
		bs_end	 = bs;
	} else {
		multiple = True;
		bs_start = 0;
		bs_end	 = BS_MaxButtonState - 1;
		for (i = bs_start; (i & use_mask) != set_mask && i <= bs_end;
		    i++) {
			bs_start++;
		}
	}

	if (*s == '(') {
		int len;
		pstyle = 1;
		if (!(end = strchr(++s, ')'))) {
			fvwm_debug(__func__, "missing parenthesis: %s", s);
			return NULL;
		}
		s   = SkipSpaces(s, NULL, 0);
		len = end - s + 1;
		/* TA:  FIXME!  xasprintf() */
		spec = fxmalloc(len);
		strncpy(spec, s, len - 1);
		spec[len - 1] = 0;
	} else {
		spec = s;
	}

	spec = SkipSpaces(spec, NULL, 0);
	/* setup temporary in case button read fails */
	memset(&tmpdf, 0, sizeof(DecorFace));
	DFS_FACE_TYPE(tmpdf.style) = SimpleButton;

	if (strncmp(spec, "--", 2) == 0) {
		/* only change flags */
		Bool verbose = True;
		for (i = bs_start; i <= bs_end; ++i) {
			if (multiple && (i & use_mask) != set_mask) {
				continue;
			}
			ReadDecorFace(spec, &TB_STATE(*tb)[i], button, verbose);
			verbose = False;
		}
	} else if (ReadDecorFace(spec, &tmpdf, button, True)) {
		if (append) {
			DecorFace *head = &TB_STATE(*tb)[bs_start];
			DecorFace *tail = head;
			DecorFace *next;

			while (tail->next) {
				tail = tail->next;
			}
			tail->next = fxcalloc(1, sizeof(DecorFace));
			DFS_FACE_TYPE(tail->next->style) = SimpleButton;
			replace_decor_face(tail->next, &tmpdf);
			if (DFS_FACE_TYPE(tail->next->style) == VectorButton
			    && DFS_FACE_TYPE((&TB_STATE(*tb)[bs_start])->style)
				   == DefaultVectorButton) {
				/* override the default vector style */
				memcpy(&tail->next->style, &head->style,
				    sizeof(DecorFaceStyle));
				DFS_FACE_TYPE(tail->next->style) = VectorButton;
				next				 = head->next;
				head->next			 = NULL;
				replace_decor_face(head, next);
				free(next);
			}
			for (i = bs_start + 1; i <= bs_end; ++i) {
				if (multiple && (i & use_mask) != set_mask) {
					continue;
				}
				head = &TB_STATE(*tb)[i];
				tail = head;
				while (tail->next) {
					tail = tail->next;
				}
				tail->next = fxcalloc(1, sizeof(DecorFace));
				DFS_FACE_TYPE(tail->next->style) = SimpleButton;
				tail->next->next		 = NULL;
				ReadDecorFace(spec, tail->next, button, False);
				if (DFS_FACE_TYPE(tail->next->style)
					== VectorButton
				    && DFS_FACE_TYPE((&TB_STATE(*tb)[i])->style)
					   == DefaultVectorButton) {
					/* override the default vector style */
					memcpy(&tail->next->style, &head->style,
					    sizeof(DecorFaceStyle));
					DFS_FACE_TYPE(tail->next->style) =
					    VectorButton;
					next	   = head->next;
					head->next = NULL;
					replace_decor_face(head, next);
					free(next);
				}
			}
		} else {
			replace_decor_face(&TB_STATE(*tb)[bs_start], &tmpdf);
			for (i = bs_start + 1; i <= bs_end; ++i) {
				if (multiple && (i & use_mask) != set_mask) {
					continue;
				}
				ReadDecorFace(
				    spec, &TB_STATE(*tb)[i], button, False);
			}
		}
	} else {
		FreeDecorFace(dpy, &tmpdf);
	}
	if (pstyle) {
		free(spec);
		end++;
		end = SkipSpaces(end, NULL, 0);
	}

	return end;
}

/* Remove the given decor from all windows */
static void
_remove_window_decors(F_CMD_ARGS, FvwmDecor *d)
{
	const exec_context_t  *exc2;
	exec_context_changes_t ecc;
	FvwmWindow	      *t;

	for (t = Scr.FvwmRoot.next; t; t = t->next) {
		if (t->decor == d) {
			/* remove the extra title height now because we delete
			 * the current decor before calling ChangeDecor(). */
			t->g.frame.height -= t->decor->title_height;
			t->decor       = NULL;
			ecc.w.fw       = t;
			ecc.w.wcontext = C_WINDOW;
			exc2 =
			    exc_clone_context(exc, &ecc, ECC_FW | ECC_WCONTEXT);
			execute_function(
			    cond_rc, exc2, "ChangeDecor Default", pc, 0);
			exc_destroy_context(exc2);
		}
	}

	return;
}

static void
do_title_style(F_CMD_ARGS, Bool do_add)
{
	char	  *parm;
	char	  *prev;
	FvwmDecor *decor = Scr.cur_decor ? Scr.cur_decor : &Scr.DefaultDecor;

	Scr.flags.do_need_window_update	  = 1;
	decor->flags.has_changed	  = 1;
	decor->titlebar.flags.has_changed = 1;

	for (prev = action; (parm = PeekToken(action, &action));
	    prev  = action) {
		if (!do_add && StrEquals(parm, "centered")) {
			TB_JUSTIFICATION(decor->titlebar) = JUST_CENTER;
		} else if (!do_add && StrEquals(parm, "leftjustified")) {
			TB_JUSTIFICATION(decor->titlebar) = JUST_LEFT;
		} else if (!do_add && StrEquals(parm, "rightjustified")) {
			TB_JUSTIFICATION(decor->titlebar) = JUST_RIGHT;
		} else if (!do_add && StrEquals(parm, "height")) {
			int height = 0;
			int next   = 0;

			if (!action
			    || sscanf(action, "%d%n", &height, &next) <= 0
			    || height < MIN_FONT_HEIGHT
			    || height > MAX_FONT_HEIGHT) {
				if (height != 0) {
					fvwm_debug(__func__,
					    "bad height argument (height"
					    " must be from 5 to 256)");
					height = 0;
				}
			}
			if (decor->title_height != height
			    || decor->min_title_height != 0) {
				decor->title_height		      = height;
				decor->min_title_height		      = 0;
				decor->flags.has_title_height_changed = 1;
			}
			if (action)
				action += next;
		} else if (!do_add && StrEquals(parm, "MinHeight")) {
			int height = 0;
			int next   = 0;

			if (!action
			    || sscanf(action, "%d%n", &height, &next) <= 0
			    || height < MIN_FONT_HEIGHT
			    || height > MAX_FONT_HEIGHT) {
				if (height < MIN_FONT_HEIGHT)
					height = MIN_FONT_HEIGHT;
				else if (height > MAX_FONT_HEIGHT)
					height = 0;
			}
			if (decor->min_title_height != height) {
				decor->title_height		      = 0;
				decor->min_title_height		      = height;
				decor->flags.has_title_height_changed = 1;
			}
			if (action)
				action += next;
		} else {
			action =
			    ReadTitleButton(prev, &decor->titlebar, do_add, -1);
		}
	}

	return;
}

/*
 *
 * Reads a multi-pixmap titlebar config. (tril@igs.net)
 *
 */
static FvwmPicture *clone_decor_picture(FvwmPicture *picture)
{
	if (picture->is_view && picture->pending)
	{
		/* MultiPixmap Buttons assigns three parts. Each needs its own
		 * old backing and cancellation interest. Decoding coalesces. */
		return PCacheFvwmPictureAtScale(dpy, Scr.NoFocusWin, NULL,
		    picture->name,
		    (FvwmPictureAttributes){.mask = picture->fpa_mask},
		    (monitor_scale_factor){picture->scale_quarters},
		    picture->target_width, picture->target_height,
		    picture->generation);
	}
	return PCloneFvwmPicture(picture);
}

static char *
ReadMultiPixmapDecor(char *s, DecorFace *df)
{
	static char  *pm_names[TBMP_NUM_PIXMAPS + 1] = {"Main", "LeftMain",
	     "RightMain", "LeftButtons", "RightButtons", "UnderText",
	     "LeftOfText", "RightOfText", "LeftEnd", "RightEnd", "Buttons",
	     NULL};
	FvwmPicture **pm;
	FvwmAcs	     *acs;
	Pixel	     *pixels;
	char	     *token;
	Bool	      stretched;
	Bool	      load_pixmap = False;
	int	      pm_id, i = 0;
	FvwmPictureAttributes fpa;

	df->style.face_type = MultiPixmap;
	df->u.mp.pixmaps    = pm =
	    fxcalloc(TBMP_NUM_PIXMAPS, sizeof(FvwmPicture *));
	df->u.mp.acs = acs = fxmalloc(TBMP_NUM_PIXMAPS * sizeof(FvwmAcs));
	df->u.mp.pixels = pixels = fxmalloc(TBMP_NUM_PIXMAPS * sizeof(Pixel));
	for (i = 0; i < TBMP_NUM_PIXMAPS; i++) {
		acs[i].cs	     = -1;
		acs[i].alpha_percent = 100;
	}
	s = GetNextTokenIndex(s, pm_names, 0, &pm_id);
	while (pm_id >= 0) {
		stretched   = False;
		load_pixmap = False;
		s	    = DoPeekToken(s, &token, ",()", NULL, NULL);
		if (StrEquals(token, "stretched")) {
			stretched = True;
			s	  = DoPeekToken(s, &token, ",", NULL, NULL);
		} else if (StrEquals(token, "tiled")) {
			s = DoPeekToken(s, &token, ",", NULL, NULL);
		}
		if (!token) {
			break;
		}
		if (pm[pm_id] || acs[pm_id].cs >= 0
		    || (df->u.mp.solid_flags & (1 << pm_id))) {
			fvwm_debug(__func__, "Ignoring: already-specified %s",
			    pm_names[i]);
			continue;
		}
		if (stretched) {
			df->u.mp.stretch_flags |= (1 << pm_id);
		}
		if (strncasecmp(token, "Colorset", 8) == 0) {
			int   val;
			char *tmp;

			tmp = DoPeekToken(s, &token, ",", NULL, NULL);
			if (!GetColorsetArgument(token, NULL, &val)
			    || val < 0) {
				fvwm_debug(__func__,
				    "Colorset should take one or two "
				    "positive integers as argument");
			} else {
				if (!alloc_colorset(val))
				{
					return NULL;
				}
				acs[pm_id].cs = val;
				s   = tmp;
				tmp = DoPeekToken(s, &token, ",", NULL, NULL);
				if (GetIntegerArguments(token, NULL, &val, 1)) {
					acs[pm_id].alpha_percent =
					    max(0, min(100, val));
					s = tmp;
				}
			}
		} else if (strncasecmp(token, "TiledPixmap", 11) == 0) {
			s	    = DoPeekToken(s, &token, ",", NULL, NULL);
			load_pixmap = True;
		} else if (strncasecmp(token, "AdjustedPixmap", 14) == 0) {
			s	    = DoPeekToken(s, &token, ",", NULL, NULL);
			load_pixmap = True;
			df->u.mp.stretch_flags |= (1 << pm_id);
		} else if (strncasecmp(token, "Solid", 5) == 0) {
			s = DoPeekToken(s, &token, ",", NULL, NULL);
			if (token) {
				df->u.mp.pixels[pm_id] = GetColor(token);
				df->u.mp.solid_flags |= (1 << pm_id);
			}
		} else {
			load_pixmap = True;
		}
		if (load_pixmap && token) {
			fpa.mask  = (Pdepth <= 8) ? FPAM_DITHER : 0; /* ? */
			pm[pm_id] = PCacheFvwmPicture(
			    dpy, Scr.NoFocusWin, NULL, token, fpa);
			if (!pm[pm_id]) {
				fvwm_debug(__func__,
				    "Pixmap '%s' could not be loaded", token);
			}
		}
		if (pm_id == TBMP_BUTTONS) {
			if (pm[TBMP_LEFT_BUTTONS]) {
				PDestroyFvwmPicture(dpy, pm[TBMP_LEFT_BUTTONS]);
			}
			if (pm[TBMP_RIGHT_BUTTONS]) {
				PDestroyFvwmPicture(
				    dpy, pm[TBMP_RIGHT_BUTTONS]);
			}
			df->u.mp.stretch_flags &= ~(1 << TBMP_LEFT_BUTTONS);
			df->u.mp.stretch_flags &= ~(1 << TBMP_RIGHT_BUTTONS);
			df->u.mp.solid_flags &= ~(1 << TBMP_LEFT_BUTTONS);
			df->u.mp.solid_flags &= ~(1 << TBMP_RIGHT_BUTTONS);
			if (pm[TBMP_BUTTONS]) {
				pm[TBMP_LEFT_BUTTONS] =
				    clone_decor_picture(pm[TBMP_BUTTONS]);
				acs[TBMP_LEFT_BUTTONS].cs = -1;
				pm[TBMP_RIGHT_BUTTONS] =
				    clone_decor_picture(pm[TBMP_BUTTONS]);
				acs[TBMP_RIGHT_BUTTONS].cs = -1;
			} else {
				pm[TBMP_RIGHT_BUTTONS] = pm[TBMP_LEFT_BUTTONS] =
				    NULL;
				acs[TBMP_RIGHT_BUTTONS].cs =
				    acs[TBMP_LEFT_BUTTONS].cs =
					acs[TBMP_BUTTONS].cs;
				acs[TBMP_RIGHT_BUTTONS].alpha_percent =
				    acs[TBMP_LEFT_BUTTONS].alpha_percent =
					acs[TBMP_BUTTONS].alpha_percent;
				pixels[TBMP_LEFT_BUTTONS] =
				    pixels[TBMP_RIGHT_BUTTONS] =
					pixels[TBMP_BUTTONS];
			}
			if (stretched) {
				df->u.mp.stretch_flags |=
				    (1 << TBMP_LEFT_BUTTONS)
				    | (1 << TBMP_RIGHT_BUTTONS);
			}
			if (df->u.mp.solid_flags & (1 << TBMP_BUTTONS)) {
				df->u.mp.solid_flags |=
				    (1 << TBMP_LEFT_BUTTONS);
				df->u.mp.solid_flags |=
				    (1 << TBMP_RIGHT_BUTTONS);
			}
			if (pm[TBMP_BUTTONS]) {
				PDestroyFvwmPicture(dpy, pm[TBMP_BUTTONS]);
				pm[TBMP_BUTTONS] = NULL;
			}
			acs[TBMP_BUTTONS].cs = -1;
			df->u.mp.solid_flags &= ~(1 << TBMP_BUTTONS);
		}
		s = SkipSpaces(s, NULL, 0);
		s = GetNextTokenIndex(s, pm_names, 0, &pm_id);
	}

	if (!(pm[TBMP_MAIN] || acs[TBMP_MAIN].cs >= 0
		|| (df->u.mp.solid_flags & TBMP_MAIN))
	    && !(pm[TBMP_LEFT_MAIN] || acs[TBMP_LEFT_MAIN].cs >= 0
		 || (df->u.mp.solid_flags & TBMP_LEFT_MAIN))
	    && !(pm[TBMP_RIGHT_MAIN] || acs[TBMP_RIGHT_MAIN].cs >= 0
		 || (df->u.mp.solid_flags & TBMP_RIGHT_MAIN))) {
		fvwm_debug(__func__,
		    "No Main pixmap/colorset/solid found for TitleStyle "
		    "MultiPixmap  (you must specify either Main, "
		    "or both LeftMain and RightMain)");
		for (i = 0; i < TBMP_NUM_PIXMAPS; i++) {
			if (pm[i]) {
				PDestroyFvwmPicture(dpy, pm[i]);
			} else if (!!(df->u.mp.solid_flags & i)) {
				PictureFreeColors(dpy, Pcmap,
				    &df->u.mp.pixels[i], 1, 0, False);
			}
		}
		free(pm);
		free(acs);
		free(pixels);
		return NULL;
	}

	return s;
}

/*
 *
 *  DestroyFvwmDecor -- frees all memory assocated with an FvwmDecor
 *      structure, but does not free the FvwmDecor itself
 *
 */
static void
DestroyFvwmDecor(FvwmDecor *decor)
{
	int i;
	/* reset to default button set (frees allocated mem) */
	DestroyAllButtons(decor);
	for (i = 0; i < BS_MaxButtonState; ++i) {
		FreeDecorFace(dpy, &TB_STATE(decor->titlebar)[i]);
	}
	FreeDecorFace(dpy, &decor->BorderStyle.active);
	FreeDecorFace(dpy, &decor->BorderStyle.inactive);
	free(decor->tag);
	decor->tag = NULL;

	return;
}

static void
SetLayerButtonFlag(
    int layer, int multi, int set, FvwmDecor *decor, TitleButton *tb)
{
	int i;
	int start = 0;
	int add	  = 2;

	if (multi) {
		if (multi == 2) {
			start = 1;
		} else if (multi == 3) {
			add = 1;
		}
		for (i = start; i < NUMBER_OF_TITLE_BUTTONS; i += add) {
			if (set) {
				TB_FLAGS(decor->buttons[i]).has_layer = 1;
				TB_LAYER(decor->buttons[i])	      = layer;
			} else {
				TB_FLAGS(decor->buttons[i]).has_layer = 0;
			}
		}
	} else {
		if (set) {
			TB_FLAGS(*tb).has_layer = 1;
			TB_LAYER(*tb)		= layer;
		} else {
			TB_FLAGS(*tb).has_layer = 0;
		}
	}

	return;
}

/*
 *
 * Changes a button decoration style (changes by veliaa@rpi.edu)
 *
 */
static void
SetMWMButtonFlag(
    mwm_flags flag, int multi, int set, FvwmDecor *decor, TitleButton *tb)
{
	int i;
	int start = 0;
	int add	  = 2;

	if (multi) {
		if (multi == 2) {
			start = 1;
		} else if (multi == 3) {
			add = 1;
		}
		for (i = start; i < NUMBER_OF_TITLE_BUTTONS; i += add) {
			if (set) {
				TB_MWM_DECOR_FLAGS(decor->buttons[i]) |= flag;
			} else {
				TB_MWM_DECOR_FLAGS(decor->buttons[i]) &= ~flag;
			}
		}
	} else {
		if (set) {
			TB_MWM_DECOR_FLAGS(*tb) |= flag;
		} else {
			TB_MWM_DECOR_FLAGS(*tb) &= ~flag;
		}
	}

	return;
}

static void
do_button_style(F_CMD_ARGS, Bool do_add)
{
	int	     i;
	int	     multi  = 0;
	int	     button = 0;
	int	     do_return;
	char	    *text  = NULL;
	char	    *prev  = NULL;
	char	    *parm  = NULL;
	TitleButton *tb	   = NULL;
	FvwmDecor   *decor = Scr.cur_decor ? Scr.cur_decor : &Scr.DefaultDecor;

	parm = PeekToken(action, &text);
	if (parm && isdigit(*parm)) {
		button = atoi(parm);
		button = BUTTON_INDEX(button);
	}

	if (parm == NULL || button >= NUMBER_OF_TITLE_BUTTONS || button < 0) {
		fvwm_debug(__func__, "Bad button style (1) in line %s", action);
		return;
	}

	Scr.flags.do_need_window_update = 1;

	do_return = 0;
	if (!isdigit(*parm)) {
		if (StrEquals(parm, "left")) {
			multi = 1; /* affect all left buttons */
		} else if (StrEquals(parm, "right")) {
			multi = 2; /* affect all right buttons */
		} else if (StrEquals(parm, "all")) {
			multi = 3; /* affect all buttons */
		} else {
			/* we're either resetting buttons or an invalid button
			 * set was specified */
			if (StrEquals(parm, "reset")) {
				ResetAllButtons(decor);
			} else {
				fvwm_debug(__func__,
				    "Bad button style (2) in line %s", action);
			}
			multi	  = 3;
			do_return = 1;
		}
	}
	/* mark button style and decor as changed */
	decor->flags.has_changed = 1;
	if (multi == 0) {
		/* a single button was specified */
		tb			  = &decor->buttons[button];
		TB_FLAGS(*tb).has_changed = 1;
	} else {
		for (i = 0; i < NUMBER_OF_TITLE_BUTTONS; ++i) {
			if (((multi & 1) && !(i & 1))
			    || ((multi & 2) && (i & 1))) {
				TB_FLAGS(decor->buttons[i]).has_changed = 1;
			}
		}
	}
	if (do_return == 1) {
		return;
	}
	for (prev = text; (parm = PeekToken(text, &text)); prev = text) {
		if (!do_add && strcmp(parm, "-") == 0) {
			char *tok;
			text = GetNextToken(text, &tok);
			while (tok) {
				int   set     = 1;
				char *old_tok = NULL;

				if (*tok == '!') {
					/* flag negate */
					set	= 0;
					old_tok = tok;
					tok++;
				}
				if (StrEquals(tok, "Clear")) {
					if (multi) {
						for (i = 0;
						    i < NUMBER_OF_TITLE_BUTTONS;
						    ++i) {
							if (((multi & 1)
								&& !(i & 1))
							    || ((multi & 2)
								&& (i & 1))) {
								TB_JUSTIFICATION(
								    decor->buttons
									[i]) =
								    (set)
									? JUST_CENTER
									: JUST_RIGHT;
								memset(
								    &TB_FLAGS(
									decor->buttons
									    [i]),
								    (set)
									? 0
									: 0xff,
								    sizeof(TB_FLAGS(
									decor->buttons
									    [i])));
								/* ? not very
								 * useful if set
								 * == 0 ? */
							}
						}
					} else {
						TB_JUSTIFICATION(*tb) =
						    (set) ? JUST_CENTER
							  : JUST_RIGHT;
						memset(&TB_FLAGS(*tb),
						    (set) ? 0 : 0xff,
						    sizeof(TB_FLAGS(*tb)));
						/* ? not very useful if
						 * set == 0 ? */
					}
				} else if (StrEquals(tok, "MWMDecorMenu")) {
					SetMWMButtonFlag(MWM_DECOR_MENU, multi,
					    set, decor, tb);
				} else if (StrEquals(tok, "MWMDecorMin")) {
					SetMWMButtonFlag(MWM_DECOR_MINIMIZE,
					    multi, set, decor, tb);
				} else if (StrEquals(tok, "MWMDecorMax")) {
					SetMWMButtonFlag(MWM_DECOR_MAXIMIZE,
					    multi, set, decor, tb);
				} else if (StrEquals(tok, "MWMDecorShade")) {
					SetMWMButtonFlag(MWM_DECOR_SHADE, multi,
					    set, decor, tb);
				} else if (StrEquals(tok, "MWMDecorStick")) {
					SetMWMButtonFlag(MWM_DECOR_STICK, multi,
					    set, decor, tb);
				} else if (StrEquals(tok, "MwmDecorLayer")) {
					int   layer, got_number;
					char *ltok;
					text = GetNextToken(text, &ltok);
					if (ltok) {
						got_number =
						    (sscanf(ltok, "%d", &layer)
							== 1);
						free(ltok);
					} else {
						got_number = 0;
					}
					if (!ltok || !got_number) {
						fvwm_debug(__func__,
						    "could not read"
						    " integer value for"
						    " layer -- line: %s",
						    text);
					} else {
						SetLayerButtonFlag(layer, multi,
						    set, decor, tb);
					}

				} else {
					fvwm_debug(__func__,
					    "unknown title button flag"
					    " %s -- line: %s",
					    tok, text);
				}
				if (set) {
					free(tok);
				} else {
					free(old_tok);
				}
				text = GetNextToken(text, &tok);
			}
			break;
		} else {
			if (multi) {
				for (i = 0; i < NUMBER_OF_TITLE_BUTTONS; ++i) {
					if (((multi & 1) && !(i & 1))
					    || ((multi & 2) && (i & 1))) {
						text = ReadTitleButton(prev,
						    &decor->buttons[i], do_add,
						    i);
					}
				}
			} else if (!(text = ReadTitleButton(
					 prev, tb, do_add, button))) {
				break;
			}
		}
	}

	return;
}

static int
update_decorface_colorset(DecorFace *df, int cset)
{
	DecorFace *tdf;
	int	   has_changed = 0;

	for (tdf = df; tdf != NULL; tdf = tdf->next) {
		if (DFS_FACE_TYPE(tdf->style) == ColorsetButton
		    && tdf->u.acs.cs == cset) {
			tdf->flags.has_changed = 1;
			has_changed	       = 1;
		} else if (DFS_FACE_TYPE(tdf->style) == MultiPixmap) {
			int i;

			for (i = 0; i < TBMP_NUM_PIXMAPS; i++) {
				if (tdf->u.mp.acs[i].cs == cset) {
					tdf->flags.has_changed = 1;
					has_changed	       = 1;
				}
			}
		}
	}

	return has_changed;
}

static int
update_titlebutton_colorset(TitleButton *tb, int cset)
{
	int i;
	int has_changed = 0;

	for (i = 0; i < BS_MaxButtonState; i++) {
		tb->state[i].flags.has_changed =
		    update_decorface_colorset(&(tb->state[i]), cset);
		has_changed |= tb->state[i].flags.has_changed;
	}
	return has_changed;
}

static void
update_decors_colorset(int cset)
{
	int	   i;
	FvwmDecor *decor = &Scr.DefaultDecor;

	for (decor = &Scr.DefaultDecor; decor != NULL; decor = decor->next) {
		for (i = 0; i < NUMBER_OF_TITLE_BUTTONS; i++) {
			decor->flags.has_changed |= update_titlebutton_colorset(
			    &(decor->buttons[i]), cset);
		}
		decor->flags.has_changed |=
		    update_titlebutton_colorset(&(decor->titlebar), cset);
		decor->flags.has_changed |= update_decorface_colorset(
		    &(decor->BorderStyle.active), cset);
		decor->flags.has_changed |= update_decorface_colorset(
		    &(decor->BorderStyle.inactive), cset);
		if (decor->flags.has_changed) {
			Scr.flags.do_need_window_update = 1;
		}
	}
}

static Bool
_parse_vector_line_one_coord(
    char **ret_action, int *pcoord, int *poff, char *action)
{
	int offset;
	int n;

	*ret_action = action;
	n	    = sscanf(action, "%d%n", pcoord, &offset);
	if (n < 1) {
		return False;
	}
	action += offset;
	/* check for offest */
	if (*action == '+' || *action == '-') {
		n = sscanf(action, "%dp%n", poff, &offset);
		if (n < 1) {
			return False;
		}
		if (*poff < -128) {
			*poff = -128;
		} else if (*poff > 127) {
			*poff = 127;
		}
		action += offset;
	} else {
		*poff = 0;
	}
	*ret_action = action;

	return True;
}

static Bool
_parse_vector_line(char **ret_action, int *px, int *py, int *pxoff, int *pyoff,
    int *pc, char *action)
{
	Bool is_valid = True;
	int  offset;
	int  n;

	*ret_action = action;
	if (_parse_vector_line_one_coord(&action, px, pxoff, action) == False) {
		return False;
	}
	if (*action != 'x') {
		return False;
	}
	action++;
	if (_parse_vector_line_one_coord(&action, py, pyoff, action) == False) {
		return False;
	}
	if (*action != '@') {
		return False;
	}
	action++;
	/* read the line style */
	n = sscanf(action, "%d%n", pc, &offset);
	if (n < 1) {
		return False;
	}
	action += offset;
	*ret_action = action;

	return is_valid;
}

/* ---------------------------- interface functions ------------------------ */

void
refresh_window(Window w, Bool window_update)
{
	XSetWindowAttributes attributes;
	unsigned long	     valuemask;

	valuemask =
	    CWOverrideRedirect | CWBackingStore | CWSaveUnder | CWBackPixmap;
	attributes.override_redirect = True;
	attributes.save_under	     = False;
	attributes.background_pixmap = None;
	attributes.backing_store     = NotUseful;
	w = XCreateWindow(dpy, w, 0, 0, monitor_get_all_widths(),
	    monitor_get_all_heights(), 0, CopyFromParent, CopyFromParent,
	    CopyFromParent, valuemask, &attributes);
	XMapWindow(dpy, w);
	if (Scr.flags.do_need_window_update && window_update) {
		flush_window_updates();
	}
	XDestroyWindow(dpy, w);
	XSync(dpy, 0);
	handle_all_expose();

	return;
}

void
ApplyDefaultFontAndColors(void)
{
	XGCValues     gcv;
	unsigned long gcm;
	int	      cset = 0;

	/* make GC's */
	gcm	     = GCFunction | GCLineWidth | GCForeground | GCBackground;
	gcv.function = GXcopy;
	if (Scr.DefaultFont->font) {
		gcm |= GCFont;
		gcv.font = Scr.DefaultFont->font->fid;
	}
	gcv.line_width = 0;
	gcv.foreground = GetColorset(cset)->fg;
	gcv.background = GetColorset(cset)->bg;

	if (Scr.StdGC) {
		XChangeGC(dpy, Scr.StdGC, gcm, &gcv);
	} else {
		Scr.StdGC = fvwmlib_XCreateGC(dpy, Scr.NoFocusWin, gcm, &gcv);
	}

	gcm	       = GCFunction | GCLineWidth | GCForeground;
	gcv.foreground = GetColorset(cset)->hilite;

	if (Scr.StdReliefGC) {
		XChangeGC(dpy, Scr.StdReliefGC, gcm, &gcv);
	} else {
		Scr.StdReliefGC =
		    fvwmlib_XCreateGC(dpy, Scr.NoFocusWin, gcm, &gcv);
	}

	gcv.foreground = GetColorset(cset)->shadow;
	if (Scr.StdShadowGC) {
		XChangeGC(dpy, Scr.StdShadowGC, gcm, &gcv);
	} else {
		Scr.StdShadowGC =
		    fvwmlib_XCreateGC(dpy, Scr.NoFocusWin, gcm, &gcv);
	}
	/* update the geometry window for move/resize */
	if (Scr.SizeWindow.win != None) {
		resize_geometry_window();
	}
	UpdateAllMenuStyles();

	return;
}

typedef struct decor_image_owner
{
	DecorFace *face;
	FvwmDecor *decor;
	FvwmPicture *source; /* Borrowed until this lease's finalizer. */
	unsigned references;
	struct
	{
		FvwmPicture *picture;
		FvwmPicture *source;
	} scaled[MONITOR_SCALE_QUARTERS_MAX - MONITOR_SCALE_QUARTERS_MIN + 1];
} decor_image_owner;

typedef struct
{
	decor_image_owner *owner;
	FvwmPicture *source;
	unsigned index;
} decor_scale_owner;

static void decor_image_unref(decor_image_owner *owner)
{
	if (--owner->references == 0)
	{
		free(owner);
	}
}

static void decor_image_release(void *context)
{
	decor_image_owner *owner = context;

	owner->face = NULL;
	owner->source = NULL;
	for (unsigned i = 0; i < sizeof(owner->scaled) /
	    sizeof(owner->scaled[0]); i++)
	{
		PDestroyFvwmPicture(dpy, owner->scaled[i].picture);
		PDestroyFvwmPicture(dpy, owner->scaled[i].source);
	}
	decor_image_unref(owner);
}

static FvwmPicture *decor_image_part(const DecorFace *face, unsigned part)
{
	switch (DFS_FACE_TYPE(face->style))
	{
	case PixmapButton:
	case TiledPixmapButton:
	case StretchedPixmapButton:
	case AdjustedPixmapButton:
	case ShrunkPixmapButton:
		return part == 0 ? face->u.p : NULL;
	case MultiPixmap:
		return face->u.mp.pixmaps != NULL ?
		    face->u.mp.pixmaps[part] : NULL;
	default:
		return NULL;
	}
}

static picture_async_commit decor_image_policy(FvwmPicture *view,
	void *context)
{
	decor_image_owner *owner = context;

	if (owner->face != NULL)
	{
		for (unsigned i = 0; i < TBMP_NUM_PIXMAPS; i++)
		{
			if (decor_image_part(owner->face, i) == view)
			{
				return PICTURE_ASYNC_APPLY;
			}
		}
	}
	return PICTURE_ASYNC_DISCARD;
}

static void decor_image_settled(FvwmPicture *view, Bool applied,
	void *context)
{
	decor_image_owner *owner = context;

	if (!applied ||
	    decor_image_policy(view, context) != PICTURE_ASYNC_APPLY)
	{
		return;
	}
	owner->face->flags.has_changed = 1;
	owner->decor->flags.has_changed = 1;
	owner->decor->titlebar.flags.has_changed = 1;
	for (unsigned i = 0; i < NUMBER_OF_TITLE_BUTTONS; i++)
	{
		TB_FLAGS(owner->decor->buttons[i]).has_changed = 1;
	}
	Scr.flags.do_need_window_update = 1;
}

static picture_async_commit decor_scale_policy(FvwmPicture *view,
	void *context)
{
	decor_scale_owner *scale = context;
	decor_image_owner *owner = scale->owner;

	if (owner->source == NULL || owner->source->backing != scale->source ||
	    owner->scaled[scale->index].picture != view)
	{
		return PICTURE_ASYNC_DISCARD;
	}
	return decor_image_policy(owner->source, owner);
}

static void decor_scale_settled(FvwmPicture *view, Bool applied,
	void *context)
{
	decor_scale_owner *scale = context;

	if (applied && decor_scale_policy(view, context) == PICTURE_ASYNC_APPLY)
	{
		decor_image_settled(scale->owner->source, True, scale->owner);
	}
}

static void decor_scale_release(void *context)
{
	decor_scale_owner *scale = context;

	PDestroyFvwmPicture(dpy, scale->source);
	decor_image_unref(scale->owner);
	free(scale);
}

FvwmPicture *decor_picture_at_scale(FvwmPicture *source,
	monitor_scale_factor factor)
{
	decor_image_owner *owner = picture_async_context(source,
	    decor_image_settled);
	FvwmPicture *snapshot = picture_snapshot(source);

	if (owner == NULL || snapshot == NULL)
	{
		return snapshot;
	}
	if (source->pending && source->scale_quarters == factor.quarters)
	{
		return snapshot;
	}
	if (factor.quarters < MONITOR_SCALE_QUARTERS_MIN ||
	    factor.quarters > MONITOR_SCALE_QUARTERS_MAX)
	{
		PDestroyFvwmPicture(dpy, snapshot);
		return NULL;
	}
	unsigned index = factor.quarters - MONITOR_SCALE_QUARTERS_MIN;
	FvwmPicture *old = owner->scaled[index].picture;

	if (owner->scaled[index].source == snapshot && old != NULL &&
	    old->is_view && old->pending)
	{
		PDestroyFvwmPicture(dpy, snapshot);
		return picture_snapshot(old);
	}
	FvwmPicture *input = source->pending ? snapshot : source;
	FvwmPicture *picture = PCacheFvwmPictureAtScale(dpy, Scr.NoFocusWin,
	    NULL, input->name, (FvwmPictureAttributes){.mask = input->fpa_mask},
	    factor, 0, 0, WM_UI_RESOURCE_GENERATION_INITIAL);

	if (picture != NULL && picture->is_view && picture->pending)
	{
		decor_scale_owner *scale = fxmalloc(sizeof(*scale));

		picture_view_set_backing(dpy, picture, old);
		*scale = (decor_scale_owner){owner, PCloneFvwmPicture(snapshot),
		    index};
		owner->references++;
		if (!picture_async_bind(picture, decor_scale_policy,
		    decor_scale_settled, scale, decor_scale_release))
		{
			decor_scale_release(scale);
			PDestroyFvwmPicture(dpy, picture);
			picture = NULL;
		}
	}
	if (picture == NULL)
	{
		PDestroyFvwmPicture(dpy, snapshot);
		return picture_snapshot(old);
	}
	/* Slots belong to the live face, not temporary paints or raw windows.
	 * Resolve each window's current factor at draw time. The existing scale
	 * domain bounds the slots. All leases share one worker. */
	owner->scaled[index].picture = picture;
	PDestroyFvwmPicture(dpy, old);
	PDestroyFvwmPicture(dpy, owner->scaled[index].source);
	owner->scaled[index].source = snapshot;
	return picture_snapshot(picture);
}

static void inherit_decor_scales(decor_image_owner *owner,
	decor_image_owner *old)
{
	for (unsigned i = 0; i < sizeof(owner->scaled) /
	    sizeof(owner->scaled[0]); i++)
	{
		if (owner->scaled[i].picture == NULL)
		{
			owner->scaled[i].picture =
			    picture_snapshot(old->scaled[i].picture);
			owner->scaled[i].source =
			    PCloneFvwmPicture(old->scaled[i].source);
		}
	}
}

static decor_image_owner *bind_decor_image(FvwmPicture *view, DecorFace *face)
{
	decor_image_owner *owner = picture_async_context(view,
	    decor_image_settled);

	if (owner == NULL)
	{
		owner = fxcalloc(1, sizeof(*owner));
		owner->face = face;
		owner->decor = Scr.cur_decor ? Scr.cur_decor :
		    &Scr.DefaultDecor;
		owner->source = view;
		owner->references = 1;
		if (!picture_async_bind(view, decor_image_policy,
		    decor_image_settled, owner, decor_image_release))
		{
			free(owner);
			return NULL;
		}
	}
	return owner;
}

void replace_decor_face(DecorFace *df, DecorFace *prepared)
{
	/* Command parsers build temporary faces. Bind only through the final
	 * transfer and relocate earlier bindings; callbacks must not retain
	 * a parser's stack address after installation. */
	for (unsigned i = 0; i < TBMP_NUM_PIXMAPS; i++)
	{
		FvwmPicture *view = decor_image_part(prepared, i);

		if (view != NULL && view->is_view && view->pending)
		{
			FvwmPicture *old = decor_image_part(df, i);
			decor_image_owner *old_owner =
			    picture_async_context(old,
			    decor_image_settled);
			decor_image_owner *owner =
			    bind_decor_image(view, prepared);

			picture_view_set_backing(dpy, view, old);
			if (owner != NULL && old_owner != NULL)
			{
				inherit_decor_scales(owner, old_owner);
			}
		}
	}
	FreeDecorFace(dpy, df);
	*df = *prepared;
	memset(prepared, 0, sizeof(*prepared));
	DFS_FACE_TYPE(prepared->style) = SimpleButton;
	for (unsigned i = 0; i < TBMP_NUM_PIXMAPS; i++)
	{
		FvwmPicture *view = decor_image_part(df, i);
		decor_image_owner *owner = picture_async_context(view,
		    decor_image_settled);

		if (owner != NULL)
		{
			owner->face = df;
		}
	}
}

void
FreeDecorFace(Display *disp, DecorFace *df)
{
	/* The commit batch may temporarily retain a cancelled lease. Invalidate
	 * its face address before releasing resources or freeing a decor. */
	for (unsigned i = 0; i < TBMP_NUM_PIXMAPS; i++)
	{
		decor_image_owner *owner = picture_async_context(
		    decor_image_part(df, i), decor_image_settled);

		if (owner != NULL && owner->face == df)
		{
			owner->face = NULL;
		}
	}
	switch (DFS_FACE_TYPE(df->style)) {
	case GradientButton:
		if (df->u.grad.d_pixels != NULL && df->u.grad.d_npixels) {
			PictureFreeColors(disp, Pcmap, df->u.grad.d_pixels,
			    df->u.grad.d_npixels, 0, False);
			free(df->u.grad.d_pixels);
		} else if (Pdepth <= 8 && df->u.grad.xcs != NULL
			   && df->u.grad.npixels > 0 && !df->u.grad.do_dither) {
			Pixel *p;
			int    i;

			p = fxmalloc(df->u.grad.npixels * sizeof(Pixel));
			for (i = 0; i < df->u.grad.npixels; i++) {
				p[i] = df->u.grad.xcs[i].pixel;
			}
			PictureFreeColors(
			    disp, Pcmap, p, df->u.grad.npixels, 0, False);
			free(p);
		}
		if (df->u.grad.xcs != NULL) {
			free(df->u.grad.xcs);
		}
		break;

	case PixmapButton:
	case TiledPixmapButton:
	case StretchedPixmapButton:
	case AdjustedPixmapButton:
	case ShrunkPixmapButton:
		if (df->u.p) {
			PDestroyFvwmPicture(disp, df->u.p);
		}
		break;

	case MultiPixmap:
		if (df->u.mp.pixmaps) {
			int i;

			for (i = 0; i < TBMP_NUM_PIXMAPS; i++) {
				if (df->u.mp.pixmaps[i]) {
					PDestroyFvwmPicture(
					    disp, df->u.mp.pixmaps[i]);
				} else if (!!(df->u.mp.solid_flags & i)) {
					PictureFreeColors(disp, Pcmap,
					    &df->u.mp.pixels[i], 1, 0, False);
				}
			}
			free(df->u.mp.pixmaps);
		}
		if (df->u.mp.acs) {
			free(df->u.mp.acs);
		}
		if (df->u.mp.pixels) {
			free(df->u.mp.pixels);
		}
		break;
	case VectorButton:
	case DefaultVectorButton:
		if (df->u.vector.x) {
			free(df->u.vector.x);
		}
		if (df->u.vector.y) {
			free(df->u.vector.y);
		}
		/* free offsets for coord */
		if (df->u.vector.xoff) {
			free(df->u.vector.xoff);
		}
		if (df->u.vector.yoff) {
			free(df->u.vector.yoff);
		}
		if (df->u.vector.c) {
			free(df->u.vector.c);
		}
		break;
	default:
		/* see below */
		break;
	}
	/* delete any compound styles */
	if (df->next) {
		FreeDecorFace(disp, df->next);
		free(df->next);
	}
	df->next = NULL;
	memset(&df->style, 0, sizeof(df->style));
	memset(&df->u, 0, sizeof(df->u));
	DFS_FACE_TYPE(df->style) = SimpleButton;

	return;
}

/*
 *
 * Reads a button face line into a structure (veliaa@rpi.edu)
 *
 */
static Bool
read_decor_face(char *s, DecorFace *df, int button, int verbose)
{
	int   offset;
	char  style[256], *file;
	char *action = s;

	/* some variants of scanf do not increase the assign count when %n is
	 * used, so a return value of 1 is no error. */
	if (sscanf(s, "%255s%n", style, &offset) < 1) {
		if (verbose) {
			fvwm_debug(__func__, "error in face `%s'", s);
		}
		return False;
	}
	style[255] = 0;

	if (strncasecmp(style, "--", 2) != 0) {
		s += offset;

		FreeDecorFace(dpy, df);

		/* determine button style */
		if (strncasecmp(style, "Simple", 6) == 0) {
			memset(&df->style, 0, sizeof(df->style));
			DFS_FACE_TYPE(df->style) = SimpleButton;
		} else if (strncasecmp(style, "Default", 7) == 0) {
			int b = -1, n = sscanf(s, "%d%n", &b, &offset);

			if (n < 1) {
				if (button == -1) {
					if (verbose) {
						fvwm_debug(__func__,
						    "need default button"
						    " number to load");
					}
					return False;
				}
				b = button;
			} else {
				b = BUTTON_INDEX(b);
				s += offset;
			}
			if (b >= 0 && b < NUMBER_OF_TITLE_BUTTONS) {
				LoadDefaultButton(df, b);
			} else {
				if (verbose) {
					fvwm_debug(__func__,
					    "button number out of range:"
					    " %d",
					    b);
				}
				return False;
			}
		} else if (strncasecmp(style, "Vector", 6) == 0
			   || (strlen(style) <= 2 && isdigit(*style))) {
			/* normal coordinate list button style */
			int		      i, num_coords, num;
			struct vector_coords *vc = &df->u.vector;

			/* get number of points */
			if (strncasecmp(style, "Vector", 6) == 0) {
				num = sscanf(s, "%d%n", &num_coords, &offset);
				s += offset;
			} else {
				num = sscanf(style, "%d", &num_coords);
			}

			if (num < 1 || num_coords < 2
			    || num_coords > MAX_TITLE_BUTTON_VECTOR_LINES) {
				if (verbose) {
					fvwm_debug(__func__,
					    "Bad button style (2) in line:"
					    " %s",
					    action);
				}
				return False;
			}

			vc->num	     = num_coords;
			vc->use_fgbg = 0;
			vc->x	     = fxmalloc(sizeof(char) * num_coords);
			vc->y	     = fxmalloc(sizeof(char) * num_coords);
			vc->xoff     = fxmalloc(sizeof(char) * num_coords);
			vc->yoff     = fxmalloc(sizeof(char) * num_coords);
			vc->c	     = fxmalloc(sizeof(char) * num_coords);

			/* get the points */
			for (i = 0; i < vc->num; ++i) {
				int x;
				int y;
				int xoff = 0;
				int yoff = 0;
				int c;

				if (_parse_vector_line(
					&s, &x, &y, &xoff, &yoff, &c, s)
				    == False) {
					break;
				}
				if (x < 0) {
					x = 0;
				}
				if (x > 100) {
					x = 100;
				}
				if (y < 0) {
					y = 0;
				}
				if (y > 100) {
					y = 100;
				}
				if (c < 0 || c > 4) {
					c = 4;
				}
				vc->x[i]    = x;
				vc->y[i]    = y;
				vc->c[i]    = c;
				vc->xoff[i] = xoff;
				vc->yoff[i] = yoff;
				if (c == 2 || c == 3) {
					vc->use_fgbg = 1;
				}
			}
			if (i < vc->num) {
				if (verbose) {
					fvwm_debug(__func__,
					    "Bad button style (3) in line"
					    " %s",
					    action);
				}
				free(vc->x);
				free(vc->y);
				free(vc->c);
				free(vc->xoff);
				free(vc->yoff);
				vc->x	 = NULL;
				vc->y	 = NULL;
				vc->c	 = NULL;
				vc->xoff = NULL;
				vc->yoff = NULL;
				return False;
			}
			memset(&df->style, 0, sizeof(df->style));
			DFS_FACE_TYPE(df->style) = VectorButton;
		} else if (strncasecmp(style, "Solid", 5) == 0) {
			s = GetNextToken(s, &file);
			if (file) {
				memset(&df->style, 0, sizeof(df->style));
				DFS_FACE_TYPE(df->style) = SolidButton;
				df->u.back		 = GetColor(file);
				free(file);
			} else {
				if (verbose) {
					fvwm_debug(__func__,
					    "no color given for Solid"
					    " face type: %s",
					    action);
				}
				return False;
			}
		} else if (strncasecmp(style + 1, "Gradient", 8) == 0) {
			char  **s_colors;
			int	npixels, nsegs, *perc;
			XColor *xcs;
			Bool	do_dither = False;

			if (!IsGradientTypeSupported(style[0])) {
				return False;
			}
			/* translate the gradient string into an array of
			 * colors etc */
			npixels =
			    ParseGradient(s, &s, &s_colors, &perc, &nsegs);
			while (*s && isspace(*s)) {
				s++;
			}
			if (npixels <= 0) {
				return False;
			}
			/* grab the colors */
			if (Pdepth <= 16) {
				do_dither = True;
			}
			xcs = AllocAllGradientColors(
			    s_colors, perc, nsegs, npixels, do_dither);
			if (xcs == None)
				return False;
			df->u.grad.xcs	     = xcs;
			df->u.grad.npixels   = npixels;
			df->u.grad.do_dither = do_dither;
			df->u.grad.d_pixels  = NULL;
			memset(&df->style, 0, sizeof(df->style));
			DFS_FACE_TYPE(df->style) = GradientButton;
			df->u.grad.gradient_type = toupper(style[0]);
		} else if (strncasecmp(style, "Pixmap", 6) == 0
			   || strncasecmp(style, "TiledPixmap", 11) == 0
			   || strncasecmp(style, "StretchedPixmap", 15) == 0
			   || strncasecmp(style, "AdjustedPixmap", 14) == 0
			   || strncasecmp(style, "ShrunkPixmap", 12) == 0) {
			FvwmPictureAttributes fpa;

			s	 = GetNextToken(s, &file);
			fpa.mask = (Pdepth <= 8) ? FPAM_DITHER : 0; /* ? */
			df->u.p	 = PCacheFvwmPicture(
			     dpy, Scr.NoFocusWin, NULL, file, fpa);
			if (df->u.p == NULL) {
				if (file) {
					if (verbose) {
						fvwm_debug(__func__,
						    "couldn't load pixmap"
						    " %s",
						    file);
					}
					free(file);
				}
				return False;
			}
			free(file);
			file = NULL;

			memset(&df->style, 0, sizeof(df->style));
			if (strncasecmp(style, "Tiled", 5) == 0) {
				DFS_FACE_TYPE(df->style) = TiledPixmapButton;
			} else if (strncasecmp(style, "Stretched", 9) == 0) {
				DFS_FACE_TYPE(df->style) =
				    StretchedPixmapButton;
			} else if (strncasecmp(style, "Adjusted", 8) == 0) {
				DFS_FACE_TYPE(df->style) = AdjustedPixmapButton;
			} else if (strncasecmp(style, "Shrunk", 6) == 0) {
				DFS_FACE_TYPE(df->style) = ShrunkPixmapButton;
			} else {
				DFS_FACE_TYPE(df->style) = PixmapButton;
			}
		} else if (strncasecmp(style, "MultiPixmap", 11) == 0) {
			if (button != -1) {
				if (verbose) {
					fvwm_debug(__func__,
					    "MultiPixmap is only valid"
					    " for TitleStyle");
				}
				return False;
			}
			s = ReadMultiPixmapDecor(s, df);
			if (!s) {
				return False;
			}
		} else if (FMiniIconsSupported
			   && strncasecmp(style, "MiniIcon", 8) == 0) {
			memset(&df->style, 0, sizeof(df->style));
			DFS_FACE_TYPE(df->style) = MiniIconButton;
			/* pixmap read in when the window is created */
			df->u.p = NULL;
		} else if (strncasecmp(style, "Colorset", 8) == 0) {
			int val[2];
			int n;

			n = GetColorsetArgument(s, NULL, val);
			if (n > 0 && GetIntegerArguments(SkipNTokens(s, 1), NULL,
			    &val[1], 1))
			{
				n = 2;
			}
			if (n == 0) {
			}
			if (n > 0 && val[0] >= 0 && !alloc_colorset(val[0]))
			{
				return False;
			}
			memset(&df->style, 0, sizeof(df->style));
			if (n > 0 && val[0] >= 0) {

				df->u.acs.cs = val[0];
				DFS_FACE_TYPE(df->style) = ColorsetButton;
			}
			df->u.acs.alpha_percent = 100;
			if (n > 1) {
				df->u.acs.alpha_percent =
				    max(0, min(100, val[1]));
			}
			s = SkipNTokens(s, n);
		} else {
			if (verbose) {
				fvwm_debug(__func__, "unknown style %s: %s",
				    style, action);
			}
			return False;
		}
	}

	/* Process button flags ("--" signals start of flags,
	   it is also checked for above) */
	s = GetNextToken(s, &file);
	if (file && (strcmp(file, "--") == 0)) {
		char *tok;
		s = GetNextToken(s, &tok);
		while (tok && *tok) {
			int   set     = 1;
			char *old_tok = NULL;

			if (*tok == '!') { /* flag negate */
				set	= 0;
				old_tok = tok;
				tok++;
			}
			if (StrEquals(tok, "Clear")) {
				memset(&DFS_FLAGS(df->style), (set) ? 0 : 0xff,
				    sizeof(DFS_FLAGS(df->style)));
				/* ? what is set == 0 good for ? */
			} else if (StrEquals(tok, "Left")) {
				if (set) {
					DFS_H_JUSTIFICATION(df->style) =
					    JUST_LEFT;
				} else {
					DFS_H_JUSTIFICATION(df->style) =
					    JUST_RIGHT;
				}
			} else if (StrEquals(tok, "Right")) {
				if (set) {
					DFS_H_JUSTIFICATION(df->style) =
					    JUST_RIGHT;
				} else {
					DFS_H_JUSTIFICATION(df->style) =
					    JUST_LEFT;
				}
			} else if (StrEquals(tok, "Centered")) {
				DFS_H_JUSTIFICATION(df->style) = JUST_CENTER;
				DFS_V_JUSTIFICATION(df->style) = JUST_CENTER;
			} else if (StrEquals(tok, "Top")) {
				if (set) {
					DFS_V_JUSTIFICATION(df->style) =
					    JUST_TOP;
				} else {
					DFS_V_JUSTIFICATION(df->style) =
					    JUST_BOTTOM;
				}
			} else if (StrEquals(tok, "Bottom")) {
				if (set) {
					DFS_V_JUSTIFICATION(df->style) =
					    JUST_BOTTOM;
				} else {
					DFS_V_JUSTIFICATION(df->style) =
					    JUST_TOP;
				}
			} else if (StrEquals(tok, "Flat")) {
				if (set) {
					DFS_BUTTON_RELIEF(df->style) =
					    DFS_BUTTON_IS_FLAT;
				} else if (DFS_BUTTON_RELIEF(df->style)
					   == DFS_BUTTON_IS_FLAT) {
					DFS_BUTTON_RELIEF(df->style) =
					    DFS_BUTTON_IS_UP;
				}
			} else if (StrEquals(tok, "Sunk")) {
				if (set) {
					DFS_BUTTON_RELIEF(df->style) =
					    DFS_BUTTON_IS_SUNK;
				} else if (DFS_BUTTON_RELIEF(df->style)
					   == DFS_BUTTON_IS_SUNK) {
					DFS_BUTTON_RELIEF(df->style) =
					    DFS_BUTTON_IS_UP;
				}
			} else if (StrEquals(tok, "Raised")) {
				if (set) {
					DFS_BUTTON_RELIEF(df->style) =
					    DFS_BUTTON_IS_UP;
				} else {
					DFS_BUTTON_RELIEF(df->style) =
					    DFS_BUTTON_IS_SUNK;
				}
			} else if (StrEquals(tok, "UseTitleStyle")) {
				if (set) {
					DFS_USE_TITLE_STYLE(df->style)	= 1;
					DFS_USE_BORDER_STYLE(df->style) = 0;
				} else
					DFS_USE_TITLE_STYLE(df->style) = 0;
			} else if (StrEquals(tok, "HiddenHandles")) {
				DFS_HAS_HIDDEN_HANDLES(df->style) = !!set;
			} else if (StrEquals(tok, "NoInset")) {
				DFS_HAS_NO_INSET(df->style) = !!set;
			} else if (StrEquals(tok, "UseBorderStyle")) {
				if (set) {
					DFS_USE_BORDER_STYLE(df->style) = 1;
					DFS_USE_TITLE_STYLE(df->style)	= 0;
				} else {
					DFS_USE_BORDER_STYLE(df->style) = 0;
				}
			} else if (verbose) {
				fvwm_debug(__func__,
				    "unknown button face flag '%s'"
				    " -- line: %s",
				    tok, action);
			}
			if (set) {
				free(tok);
			} else {
				free(old_tok);
			}
			s = GetNextToken(s, &tok);
		}
	}
	if (file) {
		free(file);
	}

	return True;
}

Bool ReadDecorFace(char *s, DecorFace *df, int button, int verbose)
{
	char *start = SkipSpaces(s, NULL, 0);
	DecorFace prepared = {0};

	if (start == NULL)
	{
		return False;
	}
	if (*start == '\0' || strncasecmp(start, "--", 2) == 0)
	{
		return read_decor_face(s, df, button, verbose);
	}
	DFS_FACE_TYPE(prepared.style) = SimpleButton;
	if (!read_decor_face(s, &prepared, button, verbose))
	{
		/* Preserve immediate parser-refusal behavior. Deferred worker
		 * refusal keeps the successfully parsed face's backing. */
		FreeDecorFace(dpy, df);
		FreeDecorFace(dpy, &prepared);
		return False;
	}
	replace_decor_face(df, &prepared);
	return True;
}

/*
 *
 * Diverts a style definition to an FvwmDecor structure (veliaa@rpi.edu)
 *
 */
void
AddToDecor(F_CMD_ARGS, FvwmDecor *decor)
{
	if (!action) {
		return;
	}
	while (*action && isspace((unsigned char)*action)) {
		++action;
	}
	if (!*action) {
		return;
	}
	Scr.cur_decor = decor;
	execute_function(F_PASS_ARGS, 0);
	Scr.cur_decor = NULL;

	return;
}

/*
 *
 *  InitFvwmDecor -- initializes an FvwmDecor structure to defaults
 *
 */
void
InitFvwmDecor(FvwmDecor *decor)
{
	int	  i;
	DecorFace tmpdf;

	/* zero out the structures */
	memset(decor, 0, sizeof(FvwmDecor));
	memset(&tmpdf, 0, sizeof(DecorFace));

	/* initialize title-bar button styles */
	DFS_FACE_TYPE(tmpdf.style) = SimpleButton;
	for (i = 0; i < NUMBER_OF_TITLE_BUTTONS; ++i) {
		int j = 0;
		for (; j < BS_MaxButtonState; ++j) {
			TB_STATE(decor->buttons[i])[j] = tmpdf;
		}
	}
	/* reset to default button set */
	ResetAllButtons(decor);
	/* initialize title-bar styles */
	for (i = 0; i < BS_MaxButtonState; ++i) {
		DFS_FACE_TYPE(TB_STATE(decor->titlebar)[i].style) =
		    SimpleButton;
	}

	/* initialize border texture styles */
	DFS_FACE_TYPE(decor->BorderStyle.active.style)	 = SimpleButton;
	DFS_FACE_TYPE(decor->BorderStyle.inactive.style) = SimpleButton;

	return;
}

void
reset_decor_changes(void)
{
	FvwmDecor *decor;
	for (decor = &Scr.DefaultDecor; decor; decor = decor->next) {
		decor->flags.has_changed	      = 0;
		decor->flags.has_title_height_changed = 0;
	}
	/* todo: must reset individual change flags too */

	return;
}

void
update_fvwm_colorset(int cset)
{
	/* Menus update through their own owner. Styles and decors mark the
	 * global pass only when referenced; zero keeps its legacy fallback. */
	if (cset == 0)
		Scr.flags.do_need_window_update = 1;

	UpdateMenuColorset(cset);
	update_style_colorset(cset);
	update_decors_colorset(cset);

	return;
}

/* ---------------------------- builtin commands --------------------------- */

void
CMD_Status(F_CMD_ARGS)
{
	if (strcasecmp(action, "on") == 0) {
		if (status_pipe.fd < 0)
		{
			status_init_pipe();
		}
	} else if (strcasecmp(action, "off") == 0) {
		status_close();
		return;
	} else {
		fvwm_debug(__func__, "unknown option: %s", action);
		return;
	}

	status_send();
}

void
CMD_Beep(F_CMD_ARGS)
{
	XBell(dpy, 0);

	return;
}

void
CMD_Nop(F_CMD_ARGS)
{
	return;
}

void
CMD_EscapeFunc(F_CMD_ARGS)
{
	return;
}

void
CMD_Delete(F_CMD_ARGS)
{
	FvwmWindow *const fw = exc->w.fw;

	if (!is_function_allowed(F_DELETE, NULL, fw, RQORIG_PROGRAM_US, True)) {
		XBell(dpy, 0);

		return;
	}
	if (IS_TEAR_OFF_MENU(fw)) {
		/* 'soft' delete tear off menus.  Note: we can't send the
		 * message to the menu window directly because it was created
		 * using a different display.  The client message would never
		 * be read from there. */
		send_clientmessage(
		    dpy, FW_W_PARENT(fw), _XA_WM_DELETE_WINDOW, CurrentTime);

		return;
	}
	if (WM_DELETES_WINDOW(fw)) {
		send_clientmessage(
		    dpy, FW_W(fw), _XA_WM_DELETE_WINDOW, CurrentTime);

		return;
	} else {
		XBell(dpy, 0);
	}
	XFlush(dpy);

	return;
}

void
CMD_Destroy(F_CMD_ARGS)
{
	FvwmWindow *const fw = exc->w.fw;

	if (IS_TEAR_OFF_MENU(fw)) {
		CMD_Delete(F_PASS_ARGS);
		return;
	}
	if (!is_function_allowed(F_DESTROY, NULL, fw, True, True)) {
		XBell(dpy, 0);
		return;
	}
	if (XGetGeometry(dpy, FW_W(fw), &JunkRoot, &JunkX, &JunkY,
		(unsigned int *)&JunkWidth, (unsigned int *)&JunkHeight,
		(unsigned int *)&JunkBW, (unsigned int *)&JunkDepth)
	    != 0) {
		XKillClient(dpy, FW_W(fw));
	}
	destroy_window(fw);
	XFlush(dpy);

	return;
}

void
CMD_Close(F_CMD_ARGS)
{
	FvwmWindow *const fw = exc->w.fw;

	if (IS_TEAR_OFF_MENU(fw)) {
		CMD_Delete(F_PASS_ARGS);
		return;
	}
	if (!is_function_allowed(F_CLOSE, NULL, fw, True, True)) {
		XBell(dpy, 0);
		return;
	}
	if (WM_DELETES_WINDOW(fw)) {
		send_clientmessage(
		    dpy, FW_W(fw), _XA_WM_DELETE_WINDOW, CurrentTime);
		return;
	}
	if (XGetGeometry(dpy, FW_W(fw), &JunkRoot, &JunkX, &JunkY,
		(unsigned int *)&JunkWidth, (unsigned int *)&JunkHeight,
		(unsigned int *)&JunkBW, (unsigned int *)&JunkDepth)
	    != 0) {
		XKillClient(dpy, FW_W(fw));
	}

	destroy_window(fw);
	XFlush(dpy);

	return;
}

void
CMD_Restart(F_CMD_ARGS)
{
	Done(1, action);

	return;
}

void
CMD_ExecUseShell(F_CMD_ARGS)
{
	char	   *arg	      = NULL;
	static char shell_set = 0;

	if (shell_set) {
		free(exec_shell_name);
	}
	shell_set = 1;
	action	  = GetNextToken(action, &arg);
	if (arg) /* specific shell was specified */
	{
		exec_shell_name = arg;
	} else /* no arg, so use $SHELL -- not working??? */
	{
		if (getenv("SHELL")) {
			exec_shell_name = fxstrdup(getenv("SHELL"));
		} else {
			/* if $SHELL not set, use default */
			exec_shell_name = fxstrdup("/bin/sh");
		}
	}
}

void
CMD_Exec(F_CMD_ARGS)
{
	pid_t child = fork();

	if (child < 0)
	{
		fvwm_debug(__func__, "fork failed (%s)", strerror(errno));
		return;
	}
	if (child == 0)
	{
		int fd;

		fvmm_deinstall_signals();
		fd = open("/dev/null", O_RDONLY | O_NOCTTY);
		if (fd < 0 || dup2(fd, STDIN_FILENO) < 0)
		{
			fvwm_debug(__func__, "stdin setup failed (%s)",
				strerror(errno));
			_exit(100);
		}
		if (fd != STDIN_FILENO)
			close(fd);

		if (fvwm_setpgrp() == -1)
		{
			fvwm_debug(
			    __func__, "setpgrp failed (%s)", strerror(errno));
			_exit(100);
		}
		/* fork owns the command copy. Failed children must not run the
		 * parent's exit callbacks or flush its copied stdio buffers. */
		execl(exec_shell_name, exec_shell_name, "-c", action,
			(char *)NULL);
		fvwm_debug(__func__, "execl failed (%s)", strerror(errno));
		_exit(100);
	}
}

void
CMD_Refresh(F_CMD_ARGS)
{
	refresh_window(Scr.Root, True);

	return;
}

void
CMD_RefreshWindow(F_CMD_ARGS)
{
	FvwmWindow *const fw = exc->w.fw;

	refresh_window(
	    (exc->w.wcontext == C_ICON) ? FW_W_ICON_TITLE(fw) : FW_W_FRAME(fw),
	    True);

	return;
}

void
CMD_Wait(F_CMD_ARGS)
{
	Bool	    done	    = False;
	Bool	    redefine_cursor = False;
	Bool	    is_ungrabbed;
	char	   *escape;
	Window	    nonewin = None;
	char	   *wait_string, *rest;
	FvwmWindow *t;

	if (!Scr.flags.are_windows_captured)
		return;

	/* try to get a single token */
	rest = GetNextToken(action, &wait_string);
	if (wait_string) {
		while (*rest && isspace((unsigned char)*rest)) {
			rest++;
		}
		if (*rest) {
			int   i;
			char *temp;

			/* nope, multiple tokens - try old syntax */

			/* strip leading and trailing whitespace */
			temp = action;
			while (*temp && isspace((unsigned char)*temp)) {
				temp++;
			}
			free(wait_string);
			wait_string = fxstrdup(temp);
			for (i = strlen(wait_string) - 1;
			    i >= 0 && isspace(wait_string[i]); i--) {
				wait_string[i] = 0;
			}
		}
	} else {
		wait_string = fxstrdup("");
	}

	is_ungrabbed = UngrabEm(GRAB_NORMAL);
	while (!done && !isTerminated) {
		XEvent e;
		if (BUSY_WAIT & Scr.BusyCursor) {
			XDefineCursor(dpy, Scr.Root, Scr.FvwmCursors[CRS_WAIT]);
			redefine_cursor = True;
		}
		if (My_XNextEvent(dpy, &e)) {
			dispatch_event(&e);
			if (XFindContext(
				dpy, e.xmap.window, FvwmContext, (caddr_t *)&t)
			    == XCNOENT) {
				t = NULL;
			}

			if (e.type == MapNotify && e.xmap.event == Scr.Root) {
				if (!*wait_string) {
					done = True;
				}
				if (t
				    && matchWildcards(wait_string, t->name.name)
					   == True) {
					done = True;
				} else if (t && t->class.res_class
					   && matchWildcards(wait_string,
						  t->class.res_class)
						  == True) {
					done = True;
				} else if (t && t->class.res_name
					   && matchWildcards(wait_string,
						  t->class.res_name)
						  == True) {
					done = True;
				}
			} else if (e.type == KeyPress) {
				/* should I be using <t> or <exc->w.fw>?
				 * DV: t
				 */
				int context;
				XClassHint *class;
				char *name;

				context = GetContext(&t, t, &e, &nonewin);
				if (t != NULL) {
					class = &(t->class);
					name  = t->name.name;
				} else {
					class = NULL;
					name  = NULL;
				}
				escape = CheckBinding(Scr.AllBindings,
				    e.xkey.keycode, e.xkey.state,
				    GetUnusedModifiers(), context,
				    BIND_KEYPRESS, class, name);
				if (escape != NULL) {
					if (!strcasecmp(escape, "escapefunc")) {
						done = True;
					}
				}
			}
		}
	}
	if (redefine_cursor) {
		XDefineCursor(dpy, Scr.Root, Scr.FvwmCursors[CRS_ROOT]);
	}
	if (is_ungrabbed) {
		GrabEm(CRS_NONE, GRAB_NORMAL);
	}
	free(wait_string);

	return;
}

void
CMD_Quit(F_CMD_ARGS)
{
	Done(0, NULL);

	return;
}

void
CMD_Echo(F_CMD_ARGS)
{
	int len;

	if (!action) {
		action = "";
	}
	len = strlen(action);
	if (len != 0) {
		if (action[len - 1] == '\n') {
			action[len - 1] = '\0';
		}
	}
	BroadcastName(MX_ECHO, -1, -1, -1, action);
	fvwm_debug(__func__, "%s\n", action);

	return;
}

static char *
topology_encoded_name(const char *name)
{
	cJSON *string;
	char  *encoded;

	string = cJSON_CreateString(name);
	if (string == NULL) {
		return NULL;
	}
	encoded = cJSON_PrintUnformatted(string);
	cJSON_Delete(string);
	return encoded;
}

static void
print_topology_info(void)
{
	const monitor_topology_snapshot *snapshot;
	int				 monitor_index;

	snapshot = FScreenGetTopologySnapshot();
	if (snapshot == NULL) {
		fvwm_debug(__func__, "No committed topology");
		return;
	}
	fvwm_debug(__func__, "Topology epoch=%" PRIu64 " monitors=%d",
	    snapshot->epoch, snapshot->monitor_count);
	for (monitor_index = 0; monitor_index < snapshot->monitor_count;
	    monitor_index++) {
		const monitor_topology_monitor *monitor =
		    &snapshot->monitors[monitor_index];
		const size_t output_size = sizeof(RROutput) * CHAR_BIT + 2;
		char	    *encoded_name;
		char	    *line;
		size_t	     line_size;
		size_t	     used;
		int	     output_index;
		int	     written;

		encoded_name = topology_encoded_name(monitor->name);
		if (encoded_name == NULL) {
			fvwm_debug(__func__,
			    "Unable to format topology monitor name_atom=%lu",
			    (unsigned long)monitor->name_atom);
			continue;
		}
		if ((size_t)monitor->output_count
		    > (SIZE_MAX - strlen(encoded_name) - 256) / output_size) {
			cJSON_free(encoded_name);
			fvwm_debug(__func__,
			    "Topology monitor output list is too large");
			continue;
		}
		line_size = strlen(encoded_name) + 256
			    + (size_t)monitor->output_count * output_size;
		line = malloc(line_size);
		if (line == NULL) {
			cJSON_free(encoded_name);
			fvwm_debug(
			    __func__, "Unable to format topology monitor");
			continue;
		}
		written = snprintf(line, line_size,
		    "Topology monitor name=%s name_atom=%lu primary=%s "
		    "geometry=%dx%d%+d%+d physical_mm=%dx%d outputs=[",
		    encoded_name, (unsigned long)monitor->name_atom,
		    monitor->primary ? "true" : "false", monitor->width,
		    monitor->height, monitor->x, monitor->y, monitor->mm_width,
		    monitor->mm_height);
		cJSON_free(encoded_name);
		if (written < 0 || (size_t)written >= line_size) {
			free(line);
			fvwm_debug(
			    __func__, "Unable to format topology monitor");
			continue;
		}
		used = (size_t)written;
		for (output_index = 0; output_index < monitor->output_count;
		    output_index++) {
			written = snprintf(line + used, line_size - used,
			    "%s%lu", output_index == 0 ? "" : ",",
			    (unsigned long)monitor->outputs[output_index]);
			if (written < 0
			    || (size_t)written >= line_size - used) {
				break;
			}
			used += (size_t)written;
		}
		if (output_index != monitor->output_count
		    || snprintf(line + used, line_size - used, "]") != 1) {
			free(line);
			fvwm_debug(
			    __func__, "Unable to format topology monitor");
			continue;
		}
		fvwm_debug(__func__, "%s", line);
		free(line);
	}
}

void
CMD_PrintInfo(F_CMD_ARGS)
{
	int   verbose;
	char *rest, *subject = NULL;

	rest = GetNextToken(action, &subject);
	if (!rest || GetIntegerArguments(rest, NULL, &verbose, 1) != 1) {
		verbose = 0;
	}
	if (StrEquals(subject, "Colors")) {
		PicturePrintColorInfo(verbose);
	} else if (StrEquals(subject, "Locale")) {
		FlocalePrintLocaleInfo(dpy, verbose);
	} else if (StrEquals(subject, "NLS")) {
		FGettextPrintLocalePath(verbose);
	} else if (StrEquals(subject, "ImageCache")) {
		PicturePrintImageCache(verbose);
	} else if (StrEquals(subject, "Bindings")) {
		print_bindings();
	} else if (StrEquals(subject, "InfoStore")) {
		print_infostore();
	} else if (StrEquals(subject, "Scale")) {
		monitor_scale_config_print_info();
	} else if (StrEquals(subject, "Topology")) {
		print_topology_info();
	} else {
		fvwm_debug(__func__, "Unknown subject '%s'", action);
	}
	if (subject) {
		free(subject);
	}
	return;
}

void
CMD_ColormapFocus(F_CMD_ARGS)
{
	if (MatchToken(action, "FollowsFocus")) {
		Scr.ColormapFocus = COLORMAP_FOLLOWS_FOCUS;
	} else if (MatchToken(action, "FollowsMouse")) {
		Scr.ColormapFocus = COLORMAP_FOLLOWS_MOUSE;
	} else {
		fvwm_debug(__func__,
		    "ColormapFocus requires 1 arg: FollowsFocus or"
		    " FollowsMouse");
		return;
	}

	return;
}

void
CMD_ClickTime(F_CMD_ARGS)
{
	int val;

	if (GetIntegerArguments(action, NULL, &val, 1) != 1) {
		Scr.ClickTime = DEFAULT_CLICKTIME;
	} else {
		Scr.ClickTime = (val < 0) ? 0 : val;
	}

	/* Use a negative value during startup and change sign afterwards. This
	 * speeds things up quite a bit. */
	if (fFvwmInStartup) {
		Scr.ClickTime = -Scr.ClickTime;
	}

	return;
}

void
CMD_ImagePath(F_CMD_ARGS)
{
	PictureSetImagePath(action);

	return;
}

void
CMD_LocalePath(F_CMD_ARGS)
{
	FGettextSetLocalePath(action);

	return;
}

void
CMD_ModulePath(F_CMD_ARGS)
{
	static int need_to_free = 0;

	setPath(&ModulePath, action, need_to_free);
	need_to_free = 1;

	return;
}

void
CMD_ModuleTimeout(F_CMD_ARGS)
{
	int timeout;

	moduleTimeout = DEFAULT_MODULE_TIMEOUT;
	if (GetIntegerArguments(action, NULL, &timeout, 1) == 1
	    && timeout > 0) {
		moduleTimeout = timeout;
	}

	return;
}

void
CMD_TitleStyle(F_CMD_ARGS)
{
	do_title_style(F_PASS_ARGS, False);

	return;
} /* SetTitleStyle */

/*
 *
 * Appends a titlestyle (veliaa@rpi.edu)
 *
 */
void
CMD_AddTitleStyle(F_CMD_ARGS)
{
	do_title_style(F_PASS_ARGS, True);

	return;
}

void
CMD_PropertyChange(F_CMD_ARGS)
{
	char	      string[256];
	char	     *token;
	char	     *rest;
	int	      ret;
	unsigned long argument;
	unsigned long data1;
	unsigned long data2;

	/* argument */
	token = PeekToken(action, &rest);
	if (token == NULL) {
		return;
	}
	ret = sscanf(token, "%lu", &argument);
	if (ret < 1) {
		return;
	}
	/* data1 */
	data1 = 0;
	token = PeekToken(rest, &rest);
	if (token != NULL) {
		ret = sscanf(token, "%lu", &data1);
		if (ret < 1) {
			rest = NULL;
		}
	}
	/* data2 */
	data2 = 0;
	token = PeekToken(rest, &rest);
	if (token != NULL) {
		ret = sscanf(token, "%lu", &data2);
		if (ret < 1) {
			rest = NULL;
		}
	}
	/* string */
	memset(string, 0, 256);
	if (rest != NULL) {
		ret = sscanf(rest, "%255c", &(string[0]));
	}
	BroadcastPropertyChange(argument, data1, data2, string);

	return;
}

void
CMD_DefaultIcon(F_CMD_ARGS)
{
	if (Scr.DefaultIcon) {
		free(Scr.DefaultIcon);
	}
	GetNextToken(action, &Scr.DefaultIcon);

	return;
}

void
CMD_DefaultFont(F_CMD_ARGS)
{
	char	    *font;
	FlocaleFont *new_font;
	FlocaleFont *old_font;
	FvwmWindow  *t;

	font = PeekToken(action, &action);
	if (!font) {
		/* Try 'fixed', pass NULL font name */
	}
	if (!(new_font = FlocaleLoadFont(dpy, font, "fvwm"))) {
		if (Scr.DefaultFont == NULL) {
			exit(1);
		} else {
			return;
		}
	}
	old_font = Scr.DefaultFont;
	Scr.DefaultFont = new_font;
	/* we should do that here because a redraw can happen before
	   flush_window_updates is called ... */
	for (t = Scr.FvwmRoot.next; t != NULL; t = t->next) {
		if (USING_DEFAULT_ICON_FONT(t)) {
			if (t->icon_font != NULL && t->icon_font != old_font &&
			    t->icon_font != Scr.DefaultFont) {
				FlocaleUnloadFont(dpy, t->icon_font);
			}
			t->icon_font = bind_default_font_at_scale(
				Scr.DefaultFont, t->m);
		}
		if (USING_DEFAULT_WINDOW_FONT(t)) {
			if (t->title_font != NULL &&
			    t->title_font != old_font &&
			    t->title_font != Scr.DefaultFont) {
				FlocaleUnloadFont(dpy, t->title_font);
			}
			t->title_font = bind_default_font_at_scale(
				Scr.DefaultFont, t->m);
		}
	}
	FlocaleUnloadFont(dpy, old_font);
	/* set flags to indicate that the font has changed */
	Scr.flags.do_need_window_update	   = 1;
	Scr.flags.has_default_font_changed = 1;

	return;
}

/*
 *
 * Changes the window's FvwmDecor pointer (veliaa@rpi.edu)
 *
 */
void
CMD_ChangeDecor(F_CMD_ARGS)
{
	char		 *item;
	FvwmDecor	 *decor = &Scr.DefaultDecor;
	FvwmDecor	 *found = NULL;
	FvwmWindow *const fw	= exc->w.fw;

	item = PeekToken(action, &action);
	if (!action || !item) {
		return;
	}
	/* search for tag */
	for (; decor; decor = decor->next) {
		if (decor->tag && StrEquals(item, decor->tag)) {
			found = decor;
			break;
		}
	}
	if (!found) {
		XBell(dpy, 0);
		return;
	}
	SET_DECOR_CHANGED(fw, 1);
	fw->decor = found;
	apply_decor_change(fw);

	return;
}

/*
 *
 * Destroys an FvwmDecor (veliaa@rpi.edu)
 *
 */
void
CMD_DestroyDecor(F_CMD_ARGS)
{
	char	  *item;
	FvwmDecor *decor = Scr.DefaultDecor.next;
	FvwmDecor *prev = &Scr.DefaultDecor, *found = NULL;
	Bool	   do_recreate = False;

	item = PeekToken(action, &action);
	if (!item) {
		return;
	}
	if (StrEquals(item, "recreate")) {
		do_recreate = True;
		item	    = PeekToken(action, NULL);
	}
	if (!item) {
		return;
	}

	/* search for tag */
	for (; decor; decor = decor->next) {
		if (decor->tag && StrEquals(item, decor->tag)) {
			found = decor;
			break;
		}
		prev = decor;
	}

	if (found && (found != &Scr.DefaultDecor || do_recreate)) {
		if (!do_recreate) {
			_remove_window_decors(F_PASS_ARGS, found);
		}
		DestroyFvwmDecor(found);
		if (do_recreate) {
			int i;

			InitFvwmDecor(found);
			found->tag			      = fxstrdup(item);
			Scr.flags.do_need_window_update	      = 1;
			found->flags.has_changed	      = 1;
			found->flags.has_title_height_changed = 0;
			found->titlebar.flags.has_changed     = 1;
			for (i = 0; i < NUMBER_OF_TITLE_BUTTONS; ++i) {
				TB_FLAGS(found->buttons[i]).has_changed = 1;
			}
		} else {
			prev->next = found->next;
			free(found);
		}
	}

	return;
}

/*
 *
 * Initiates an AddToDecor (veliaa@rpi.edu)
 *
 */
void
CMD_AddToDecor(F_CMD_ARGS)
{
	FvwmDecor *decor;
	FvwmDecor *found = NULL;
	char	  *item	 = NULL;

	action = GetNextToken(action, &item);
	if (!item) {
		return;
	}
	if (!action) {
		free(item);
		return;
	}
	/* search for tag */
	for (decor = &Scr.DefaultDecor; decor; decor = decor->next) {
		if (decor->tag && StrEquals(item, decor->tag)) {
			found = decor;
			break;
		}
	}
	if (!found) {
		/* then make a new one */
		found = fxmalloc(sizeof *found);
		InitFvwmDecor(found);
		found->tag = item; /* tag it */
		/* add it to list */
		for (decor = &Scr.DefaultDecor; decor->next;
		    decor  = decor->next) {
			/* nop */
		}
		decor->next = found;
	} else {
		free(item);
	}

	if (found) {
		AddToDecor(F_PASS_ARGS, found);
		/* Set + state to last decor */
		set_last_added_item(ADDED_DECOR, found);
	}

	return;
}

/*
 *
 * Updates window decoration styles (veliaa@rpi.edu)
 *
 */
void
CMD_UpdateDecor(F_CMD_ARGS)
{
	FvwmWindow *fw2;

	FvwmDecor  *decor, *found = NULL;
	FvwmWindow *hilight = Scr.Hilite;
	char	   *item    = NULL;

	action = GetNextToken(action, &item);
	if (item) {
		/* search for tag */
		for (decor = &Scr.DefaultDecor; decor; decor = decor->next) {
			if (decor->tag && StrEquals(item, decor->tag)) {
				found = decor;
				break;
			}
		}
		free(item);
	}

	for (fw2 = Scr.FvwmRoot.next; fw2; fw2 = fw2->next) {
		/* update specific decor, or all */
		if (found) {
			if (fw2->decor == found) {
				border_draw_decorations(fw2, PART_ALL, True,
				    True, CLEAR_ALL, NULL, NULL);
				border_draw_decorations(fw2, PART_ALL, False,
				    True, CLEAR_ALL, NULL, NULL);
			}
		} else {
			border_draw_decorations(
			    fw2, PART_ALL, True, True, CLEAR_ALL, NULL, NULL);
			border_draw_decorations(
			    fw2, PART_ALL, False, True, CLEAR_ALL, NULL, NULL);
		}
	}
	border_draw_decorations(
	    hilight, PART_ALL, True, True, CLEAR_ALL, NULL, NULL);
}

void
CMD_ButtonStyle(F_CMD_ARGS)
{
	do_button_style(F_PASS_ARGS, False);

	return;
}

/*
 *
 * Appends a button decoration style (veliaa@rpi.edu)
 *
 */
void
CMD_AddButtonStyle(F_CMD_ARGS)
{
	do_button_style(F_PASS_ARGS, True);

	return;
}

void
CMD_SetEnv(F_CMD_ARGS)
{
	char *szVar    = NULL;
	char *szValue  = NULL;
	char *szPutenv = NULL;

	action = GetNextToken(action, &szVar);
	if (!szVar) {
		return;
	}

	if (strchr(szVar, '=') != NULL) {
		fvwm_debug(__func__, "Invalid envvar: %s", szVar);
		goto out;
	}

	action = GetNextToken(action, &szValue);
	if (!szValue) {
		szValue = fxstrdup("");
	}
	xasprintf(&szPutenv, "%s=%s", szVar, szValue);
	flib_putenv(szVar, szPutenv);
out:
	free(szVar);
	free(szPutenv);
	free(szValue);

	return;
}

void
CMD_UnsetEnv(F_CMD_ARGS)
{
	char *szVar = NULL;

	szVar = PeekToken(action, &action);
	if (!szVar) {
		return;
	}
	flib_unsetenv(szVar);

	return;
}

void
CMD_BugOpts(F_CMD_ARGS)
{
	char *opt;
	int   toggle;
	char *optstring;

	/* fvwm_msg(DBG,"SetGlobalOptions","init action == '%s'\n",action); */
	while (action && *action && *action != '\n') {
		action = GetNextFullOption(action, &optstring);
		if (!optstring) {
			/* no more options */
			return;
		}
		toggle = ParseToggleArgument(
		    SkipNTokens(optstring, 1), NULL, 2, False);
		opt = PeekToken(optstring, NULL);
		free(optstring);

		if (!opt) {
			return;
		}
		/* toggle = ParseToggleArgument(rest, &rest, 2, False);*/

		if (StrEquals(opt, "FlickeringMoveWorkaround")) {
			switch (toggle) {
			case -1:
				Scr.bo.do_disable_configure_notify ^= 1;
				break;
			case 0:
			case 1:
				Scr.bo.do_disable_configure_notify = toggle;
				break;
			default:
				Scr.bo.do_disable_configure_notify = 0;
				break;
			}
		} else if (StrEquals(opt, "MixedVisualWorkaround")) {
			switch (toggle) {
			case -1:
				Scr.bo.do_install_root_cmap ^= 1;
				break;
			case 0:
			case 1:
				Scr.bo.do_install_root_cmap = toggle;
				break;
			default:
				Scr.bo.do_install_root_cmap = 0;
				break;
			}
		} else if (StrEquals(opt, "ModalityIsEvil")) {
			switch (toggle) {
			case -1:
				Scr.bo.is_modality_evil ^= 1;
				break;
			case 0:
			case 1:
				Scr.bo.is_modality_evil = toggle;
				break;
			default:
				Scr.bo.is_modality_evil = 0;
				break;
			}
			if (Scr.bo.is_modality_evil) {
				SetMWM_INFO(Scr.NoFocusWin);
			}
		} else if (StrEquals(opt, "RaiseOverNativeWindows")) {
			switch (toggle) {
			case -1:
				Scr.bo.is_raise_hack_needed ^= 1;
				break;
			case 0:
			case 1:
				Scr.bo.is_raise_hack_needed = toggle;
				break;
			default:
				Scr.bo.is_raise_hack_needed = 0;
				break;
			}
		} else if (StrEquals(opt, "RaiseOverUnmanaged")) {
			switch (toggle) {
			case -1:
				Scr.bo.do_raise_over_unmanaged ^= 1;
				break;
			case 0:
			case 1:
				Scr.bo.do_raise_over_unmanaged = toggle;
				break;
			default:
				Scr.bo.do_raise_over_unmanaged = 0;
				break;
			}
		} else if (StrEquals(opt, "FlickeringQtDialogsWorkaround")) {
			switch (toggle) {
			case -1:
				Scr.bo
				    .do_enable_flickering_qt_dialogs_workaround ^=
				    1;
				break;
			case 0:
			case 1:
				Scr.bo
				    .do_enable_flickering_qt_dialogs_workaround =
				    toggle;
				break;
			default:
				Scr.bo
				    .do_enable_flickering_qt_dialogs_workaround =
				    0;
				break;
			}
		} else if (StrEquals(opt, "QtDragnDropWorkaround")) {
			switch (toggle) {
			case -1:
				Scr.bo.do_enable_qt_drag_n_drop_workaround ^= 1;
				break;
			case 0:
			case 1:
				Scr.bo.do_enable_qt_drag_n_drop_workaround =
				    toggle;
				break;
			default:
				Scr.bo.do_enable_qt_drag_n_drop_workaround = 0;
				break;
			}
		} else if (EWMH_BugOpts(opt, toggle)) {
			/* work is done in EWMH_BugOpts */
		} else if (StrEquals(opt, "DisplayNewWindowNames")) {
			switch (toggle) {
			case -1:
				Scr.bo.do_display_new_window_names ^= 1;
				break;
			case 0:
			case 1:
				Scr.bo.do_display_new_window_names = toggle;
				break;
			default:
				Scr.bo.do_display_new_window_names = 0;
				break;
			}
		} else if (StrEquals(opt, "ExplainWindowPlacement")) {
			switch (toggle) {
			case -1:
				Scr.bo.do_explain_window_placement ^= 1;
				break;
			case 0:
			case 1:
				Scr.bo.do_explain_window_placement = toggle;
				break;
			default:
				Scr.bo.do_explain_window_placement = 0;
				break;
			}
		} else if (StrEquals(opt, "DebugCRMotionMethod")) {
			switch (toggle) {
			case -1:
				Scr.bo.do_debug_cr_motion_method ^= 1;
				break;
			case 0:
			case 1:
				Scr.bo.do_debug_cr_motion_method = toggle;
				break;
			default:
				Scr.bo.do_debug_cr_motion_method = 0;
				break;
			}
		} else if (StrEquals(opt, "DebugRandR")) {
			switch (toggle) {
			case -1:
				Scr.bo.do_debug_randr ^= 1;
				break;
			case 0:
			case 1:
				Scr.bo.do_debug_randr = toggle;
				break;
			default:
				Scr.bo.do_debug_randr = 0;
				break;
			}
			if (Scr.bo.do_debug_randr) {
				monitor_dump_state(NULL);
			}
		} else if (StrEquals(opt, "TransliterateUtf8")) {
			FiconvSetTransliterateUtf8(toggle);
		} else {
			fvwm_debug(__func__, "Unknown Bug Option '%s'", opt);
		}
	}

	return;
}

void
CMD_Emulate(F_CMD_ARGS)
{
	char *style;

	style = PeekToken(action, NULL);
	if (!style || StrEquals(style, "fvwm")) {
		Scr.gs.do_emulate_mwm = False;
		Scr.gs.do_emulate_win = False;
	} else if (StrEquals(style, "mwm")) {
		Scr.gs.do_emulate_mwm = True;
		Scr.gs.do_emulate_win = False;
	} else if (StrEquals(style, "win")) {
		Scr.gs.do_emulate_mwm = False;
		Scr.gs.do_emulate_win = True;
	} else {
		fvwm_debug(__func__, "Unknown style '%s'", style);
		return;
	}
	/* This command might have been issued after any GeometryWindow
	 * commands, in which case set those values to their defaults.
	 */
	Scr.SizeWindow.is_configured = false;
	Scr.SizeWindow.m	     = NULL;

	Scr.flags.do_need_window_update	   = 1;
	Scr.flags.has_default_font_changed = 1;

	return;
}

/* set animation parameters */
void
CMD_SetAnimation(F_CMD_ARGS)
{
	char *opt;
	int   delay;
	float pct;
	int   i = 0;

	opt = PeekToken(action, &action);
	if (!opt || sscanf(opt, "%d", &delay) != 1) {
		fvwm_debug(
		    __func__, "Improper milli-second delay as first argument");
		return;
	}
	if (delay > 500) {
		fvwm_debug(__func__,
		    "Using longer than .5 seconds as between frame"
		    " animation delay");
	}
	cmsDelayDefault = delay;
	for (opt = PeekToken(action, &action); opt;
	    opt	 = PeekToken(action, &action)) {
		if (sscanf(opt, "%f", &pct) != 1) {
			fvwm_debug(__func__,
			    "Use fractional values ending in 1.0 as args"
			    " 2 and on");
			return;
		}
		rgpctMovementDefault[i++] = pct;
	}
	/* No pct entries means don't change them at all */
	if (i > 0 && rgpctMovementDefault[i - 1] != 1.0) {
		rgpctMovementDefault[i++] = 1.0;
	}

	return;
}

/* Determine which modifiers are required with a keycode to make <keysym>. */
static Bool
FKeysymToKeycode(Display *disp, KeySym keysym, unsigned int *keycode,
    unsigned int *modifiers)
{
	int m;

	*keycode   = XKeysymToKeycode(disp, keysym);
	*modifiers = 0;

	for (m = 0; m <= 8; ++m) {
		KeySym ks = fvwm_KeycodeToKeysym(disp, *keycode, m, 0);
		if (ks == keysym) {
			switch (m) {
			case 0: /* No modifiers */
				break;
			case 1: /* Shift modifier */
				*modifiers |= ShiftMask;
				break;
			default:
				fvwm_debug(
				    __func__, "Unhandled modifier %d", m);
				break;
			}
			return True;
		}
	}
	return False;
}

static void
_fake_event(F_CMD_ARGS, FakeEventType type)
{
	char	    *token;
	char	    *optlist[] = {"press", "p", "release", "r", "wait", "w",
		   "modifiers", "m", "depth", "d", NULL};
	unsigned int mask      = 0;
	Window	     root      = Scr.Root;
	int	     maxdepth  = 0;
	static char  args[128];
	strncpy(args, action, sizeof(args) - 1);

	/* get the mask of pressed/released buttons/keys */
	FQueryPointer(dpy, Scr.Root, &root, &JunkRoot, &JunkX, &JunkY, &JunkX,
	    &JunkY, &mask);

	token = PeekToken(action, &action);
	while (token && action) {
		int    index = GetTokenIndex(token, optlist, 0, NULL);
		int    val, depth;
		XEvent e;
		Window w;
		Window child_w;
		int    x  = 0;
		int    y  = 0;
		int    rx = 0;
		int    ry = 0;
		Bool   do_unset;
		long   add_mask = 0;
		KeySym keysym	= NoSymbol;

		XFlush(dpy);
		do_unset = True;
		switch (index) {
		case 0:
		case 1:
			do_unset = False;
			/* fall through */
		case 2:
		case 3:
			/* key/button press or release */
			if (type == FakeMouseEvent) {
				if ((GetIntegerArguments(
					 action, &action, &val, 1)
					!= 1)
				    || val < 1
				    || val > NUMBER_OF_EXTENDED_MOUSE_BUTTONS) {
					fvwm_debug(__func__,
					    "Invalid button specifier in"
					    " \"%s\" for FakeClick.",
					    args);
					return; /* error */
				}
			} else /* type == FakeKeyEvent */
			{
				char *key = PeekToken(action, &action);
				if (key == NULL) {
					fvwm_debug(__func__,
					    "No keysym specifier in \"%s\""
					    " for FakeKeypress.",
					    args);
					return;
				}

				/* Do *NOT* use FvwmStringToKeysym() as it is
				 * case insensitive. */
				keysym = XStringToKeysym(key);
				if (keysym == NoSymbol) {
					fvwm_debug(__func__,
					    "Invalid keysym specifier (%s)"
					    " in \"%s\" for FakeKeypress.",
					    key, args);
					return;
				}
			}

			w	= None;
			child_w = root;
			for (depth = 1; depth != maxdepth && w != child_w
					&& child_w != None;
			    depth++) {
				w = child_w;
				FQueryPointer(dpy, w, &root, &child_w, &rx, &ry,
				    &x, &y, &JunkMask);
			}

			if (type == FakeMouseEvent) {
				e.type =
				    (do_unset) ? ButtonRelease : ButtonPress;
				e.xbutton.display     = dpy;
				e.xbutton.window      = w;
				e.xbutton.subwindow   = None;
				e.xbutton.root	      = root;
				e.xbutton.time	      = fev_get_evtime();
				e.xbutton.x	      = x;
				e.xbutton.y	      = y;
				e.xbutton.x_root      = rx;
				e.xbutton.y_root      = ry;
				e.xbutton.button      = val;
				e.xbutton.state	      = mask;
				e.xbutton.same_screen = (Scr.Root == root);
				/* SS: I think this mask handling code is
				 * buggy.
				 * The value of <mask> is overridden during a
				 * "wait" operation. Also why are we only using
				 * Button1Mask? What if the user has requested
				 * a FakeClick using some other button? */
				/* DV: Button1Mask is actually a bit.  Shifting
				 * it by (val -1) bits to the left gives
				 * Button2Mask, Button3Mask etc. */
				if (do_unset) {
					mask &= ~(Button1Mask << (val - 1));
				} else {
					mask |= (Button1Mask << (val - 1));
				}
				add_mask = (do_unset) ? ButtonPressMask
						      : ButtonReleaseMask;
			} else {
				/* type == FakeKeyEvent */
				e.type = (do_unset ? KeyRelease : KeyPress);
				e.xkey.display	   = dpy;
				e.xkey.subwindow   = None;
				e.xkey.root	   = root;
				e.xkey.time	   = fev_get_evtime();
				e.xkey.x	   = x;
				e.xkey.y	   = y;
				e.xkey.x_root	   = rx;
				e.xkey.y_root	   = ry;
				e.xkey.same_screen = (Scr.Root == root);

				w = e.xkey.window = exc->w.w;

				if (FKeysymToKeycode(dpy, keysym,
					&(e.xkey.keycode), &(e.xkey.state))
				    != True) {
					fvwm_debug(__func__,
					    "FKeysymToKeycode failed");
					return;
				}
				e.xkey.state |= mask;
				add_mask =
				    (do_unset) ? KeyReleaseMask : KeyPressMask;
			}
			FSendEvent(dpy, w, True,
			    SubstructureNotifyMask | add_mask, &e);
			XFlush(dpy);
			break;
		case 4:
		case 5:
			/* wait */
			if ((GetIntegerArguments(action, &action, &val, 1) != 1)
			    || val <= 0 || val > 1000000) {
				fvwm_debug(__func__,
				    "Invalid wait value in \"%s\"", args);
				return;
			}

			usleep(1000 * val);
			FQueryPointer(dpy, Scr.Root, &root, &JunkRoot, &JunkX,
			    &JunkY, &JunkX, &JunkY, &mask);
			break;
		case 6:
		case 7:
			/* set modifier */
			if (GetIntegerArguments(action, &action, &val, 1)
			    != 1) {
				fvwm_debug(__func__,
				    "Invalid modifier value in \"%s\"", args);
				return;
			}
			do_unset = False;
			if (val < 0) {
				do_unset = True;
				val	 = -val;
			}
			if (val == 6) {
				val = ShiftMask;
			} else if (val == 7) {
				val = LockMask;
			} else if (val == 8) {
				val = ControlMask;
			} else if (val >= 1 && val <= 5) {
				val = (Mod1Mask << (val - 1));
			} else {
				/* error */
				return;
			}
			/* SS: Could be buggy if a "modifier" operation
			 * preceeds a "wait" operation. */
			if (do_unset) {
				mask &= ~val;
			} else {
				mask |= val;
			}
			break;
		case 8:
		case 9:
			/* new max depth */
			if (GetIntegerArguments(action, &action, &val, 1)
			    != 1) {
				fvwm_debug(__func__,
				    "Invalid depth value in \"%s\"", args);
				return;
			}
			maxdepth = val;
			break;
		default:
			fvwm_debug(__func__, "Invalid command (%s) in \"%s\"",
			    token, args);
			return;
		}
		if (action) {
			token = PeekToken(action, &action);
		}
	}

	return;
}

void
CMD_FakeClick(F_CMD_ARGS)
{
	_fake_event(F_PASS_ARGS, FakeMouseEvent);

	return;
}

void
CMD_FakeKeypress(F_CMD_ARGS)
{
	_fake_event(F_PASS_ARGS, FakeKeyEvent);

	return;
}

void
CMD_State(F_CMD_ARGS)
{
	unsigned int	  state;
	int		  toggle;
	int		  n;
	FvwmWindow *const fw = exc->w.fw;

	n = GetIntegerArguments(action, &action, (int *)&state, 1);
	if (n <= 0) {
		return;
	}
	if (state > 31) {
		fvwm_debug(__func__, "Illegal state %d\n", state);
		return;
	}
	toggle = ParseToggleArgument(action, NULL, -1, 0);
	state  = (1 << state);
	switch (toggle) {
	case -1:
		TOGGLE_USER_STATES(fw, state);
		break;
	case 0:
		CLEAR_USER_STATES(fw, state);
		break;
	case 1:
	default:
		SET_USER_STATES(fw, state);
		break;
	}

	return;
}
