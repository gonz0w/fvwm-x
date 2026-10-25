/* -*-c-*- */

#ifndef FVWMSCRIPT_WIDGETS_TOOLS_H
#define FVWMSCRIPT_WIDGETS_TOOLS_H

#include "../types.h"
#include "../script_widget_math.h"
#include "../script_text.h"
#include "../script_geometry.h"

#include "libs/Flocale.h"

extern Display *dpy;
extern int screen;
extern Window Root;
extern char *Scrapt;
extern Atom propriete;
extern Atom type;
extern char *imagePath;
extern X11base *x11base;
extern int nbobj;
extern struct XObj *tabxobj[1000];
extern int x_fd;
extern char *ScriptName;
extern FlocaleWinString *FwinString;

/* Constant for messages types sent between objects */
/* <0 value for internal messages */
/* >0 messages sent by user */
#define SingleClic -1
#define DoubleClic -2

/* if the time in X ms between a Button Press and ButtonRelease is < to *
 * MENU_DRAG_TIME, then we consider the mouse click as simple click and
 * not "menu drag" (use in Menu.c and PopupMenu.c */
#define MENU_DRAG_TIME 300
#define GRAB_EVMASK (ButtonPressMask | ButtonReleaseMask | ButtonMotionMask | PointerMotionMask | EnterWindowMask | LeaveWindowMask)

/* Cancellation-aware modal readiness shares MainLoop packet/session ownership.
 * A zero-type event means a finite timeout. A callback changing the active
 * widget cancels the old interaction before its cached geometry is used. */
bool FvwmScriptModalService(void *context);
bool FvwmScriptModalEvent(struct XObj *object, XEvent *event, int timeout_ms);
bool script_widget_next_event(struct XObj *object, XEvent *event);
bool script_widget_wait_release(struct XObj *object, int ticks);

/* helper functions */
/* Call widget destructors for all completed initializers before releasing any
 * slots.  The bool is the completed-InitObj boundary, not font preparation. */
void script_widget_release(struct XObj **slot, bool initialized);
/* Measure without mutating a live widget; font ownership stays with caller. */
bool script_widget_layout(struct XObj *xobj, FlocaleFont *font,
	script_layout_geometry *geometry);
bool script_widget_prepare(struct XObj *xobj);
/* Popup sizing precedes translation, allocation and pointer grabbing. */
bool script_widget_popup(struct XObj *xobj, int first, int padding,
	bool align_value, script_layout_geometry *geometry, int *row_height);
void script_geometry_diagnostic(const char *operation, int id);
/* get byte offset corresponding to character offset, including
   bounds check to represent end of text */
int getByteOffsetBoundsCheck(FlocaleFont *flf, char *str, int offset);
/* opposite of the above, return character offset */
int getCharOffsetBoundsCheck(FlocaleFont *flf, char *str, int offset);
size_t script_locale_text_next(FlocaleFont *flf, const char *str,
	size_t remaining);

void MyDrawString(
	Display *dpy, struct XObj *xobj, Window win, int x, int y,
	char *str, unsigned long ForeC,unsigned long HiC,
	unsigned long BackC, int WithRelief, XRectangle *clip, XEvent *evp);

int GetXTextPosition(struct XObj *xobj, int obj_width, int str_len,
		     int left_offset, int center_offset, int right_offset);

char* GetMenuTitle(char *str,int id);

void DrawPMenu(struct XObj *xobj,Window WinPop,int h,int StrtOpt);

void UnselectMenu(struct XObj *xobj,Window WinPop,int hOpt,int value,
		  unsigned int width, int asc, int start);
void SelectMenu(struct XObj *xobj,Window WinPop,int hOpt,int value);

int CountOption(char *str);

void DrawIconStr(int offset, struct XObj *xobj, int DoRedraw,
		 int l_offset, int c_offset, int r_offset,
		 XRectangle *str_clip, XRectangle *icon_clip, XEvent *evp);

void DrawReliefRect(int x,int y,int width,int height,struct XObj *xobj,
		unsigned int LiC, unsigned int ShadC);

char *GetText(struct XObj *xobj,int End);

void SelectOneTextField(struct XObj *xobj);
/* Opt-in script keyboard policy, shared with the native text editor. */
Bool FvwmScriptUsesKeyboardFocus(void);

void DrawArrowN(struct XObj *xobj,int x,int y,int Press);

void DrawArrowS(struct XObj *xobj,int x,int y,int Press);

void DrawArrowE(struct XObj *xobj,int x,int y,int Press);

void DrawArrowW(struct XObj *xobj,int x,int y,int Press);

int PtInRect(XPoint pt,XRectangle rect);

#endif /* FVWMSCRIPT_WIDGETS_TOOLS_H */
