/* -*-c-*- */
/* Copyright May 1996, Frederic Cordier.  No guarantees or warantees are
 * provided or implied in any way whatsoever.  Use this program at your own
 * risk.  Permission to use, modify, and redistribute this program is hereby
 * given, provided that this copyright is kept intact.
 */

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

#include "types.h"
#include "script_index.h"
#include "script_process.h"
#include "script_fifo.h"
#include "script_execution.h"
#include "script_storage.h"
#include "script_delay.h"
#include "script_packet.h"
#include <errno.h>
#include "Widgets/Tools.h"
#include "libs/Bindings.h"
#include "libs/charmap.h"
#include "libs/ColorUtils.h"
#include "libs/Cursor.h"
#include "libs/FEvent.h"
#include "libs/FGettext.h"
#include "libs/FProperty.h"
#include "libs/FRender.h"
#include "libs/FRenderInit.h"
#include "libs/FScreen.h"
#include "libs/FShape.h"
#include "libs/fvwmlib.h"
#include "libs/fvwmsignal.h"
#include "libs/Graphics.h"
#include "libs/log.h"
#include "libs/module_packet_reader.h"
#include "libs/Parse.h"
#include "libs/Picture.h"
#include "libs/PictureGraphics.h"
#include "libs/Strings.h"
#include "libs/wcontext.h"
#include "libs/XError.h"

#ifdef MEMDEBUG                 /* For debugging */
#include <unchecked.h>
#endif

/* Variables used by syntax interpreter */
extern ScriptProp *scriptprop;
extern int nbobj;                       /* Number of objects */
extern int numligne;                    /* Line Number */
extern TabObj *tabobj;                  /* Object array, limit=1000 */
extern char **TabVVar;                  /* Script variables array */
extern int TabIdObj[1001];
extern Bloc **TabIObj;
extern CaseObj *TabCObj;
#ifdef MEMDEBUG
extern int __bounds_debug_no_checking;
#endif

/* Color constants used in TabColor array */
#define back 0
#define fore 1
#define shad 2
#define hili 3


/* Global Variables */
char *ScriptName;       /* Filename for GUI script */
char *ScriptBaseName;
char *ScriptPath = "";
ModuleArgs *module;
int fd[2];                      /* pipe pair */
int fd_err;
int x_fd;                       /* fd for X */
Window ref;
FlocaleWinString *FwinString;

extern int yyparse(void);
extern void (*TabCom[]) (int NbArg,long *TabArg);

Display *dpy;
int screen;
Window  Root;
X11base *x11base;               /* For X server */
script_message_queue BuffSend;          /* For inter script communications */
int grab_server = 0;
/* Opt-in keyboard focus leaves legacy scripts' pointer routing untouched. */
static char *keyboard_focus_title = NULL;
static int keyboard_focus = -1;
static script_execution execution = {
  .step_limit = SCRIPT_EXECUTION_DEFAULT_STEPS,
  .depth_limit = SCRIPT_EXECUTION_DEFAULT_DEPTH
};
static unsigned int keyboard_lock_mask = LockMask;
struct XObj *tabxobj[1000];
char *Scrapt;
Atom propriete,type;
static Atom wm_del_win;
char *imagePath = NULL;
static Bool is_dead_pipe = False;
static int initialized_widgets;
KeySym shift_tab_ks;  /* shift-Tab keysym */
char *LastString = NULL;  /* last string sent by a SendString ot Key cmd */
Binding *BindingsList;

extern void InitCom(void);
static void TryToFind(char *filename);
static void ReadFvwmScriptArg(int argc, char **argv, int IsFather);

char *default_cursor;

/* Exit procedure - called whenever we call exit(), or when main() ends */
static void
ShutdownX(void)
{
  int i;
  XEvent event;
  struct pollfd ready;
  struct timespec start, now;
  Atom MyAtom;

#ifdef DEBUG                    /* For debugging */
  XSync(dpy,0);
#endif

  /* execute the QuitFunc */
  if (x11base->quitfunc != NULL)
    ExecBloc(x11base->quitfunc);

  if (is_dead_pipe)
  {
    script_message_clear(&BuffSend);
    return;
  }

  /* We hide the window */
  if (x11base->win != None)
    XUnmapWindow(dpy,x11base->win);
  XFlush(dpy);

  /* Keep the selection owned while receivers collect queued messages.
   * One total monotonic grace period covers all requests and interruptions;
   * Xlib calls themselves still depend on a responsive X server. */
  if (clock_gettime(CLOCK_MONOTONIC, &start) < 0)
    fvwm_debug(__func__, "Unable to initialize message shutdown clock");
  else
    while (!isTerminated && BuffSend.count > 0)
    {
      if (clock_gettime(CLOCK_MONOTONIC, &now) < 0)
      {
        fvwm_debug(__func__, "Unable to read message shutdown clock");
        break;
      }
      int timeout = script_shutdown_timeout_ms(&start, &now);
      if (timeout == 0)
        break;
      if (FPending(dpy))
      {
        /* Consume unrelated events too: leaving them queued would spin.
         * Recheck the deadline after each event, including refusals. */
        FNextEvent(dpy, &event);
        if (event.type == SelectionRequest)
          SendMsgToScript(event);
        continue;
      }
      ready = (struct pollfd){ x_fd, POLLIN, 0 };
      int result = fvwmPoll(&ready, 1, timeout);
      if (result < 0 && errno == EINTR)
        continue;
      if (result < 0 ||
          (ready.revents & (POLLERR | POLLHUP | POLLNVAL)))
      {
        fvwm_debug(__func__, "Message shutdown X connection failed");
        break;
      }
    }
  script_message_clear(&BuffSend);

  MyAtom = XInternAtom(dpy, x11base->TabScriptId[1], False);
  XSetSelectionOwner(dpy, MyAtom, x11base->root, CurrentTime);
  XFlush(dpy);

  for (i=0; i<initialized_widgets; i++)
    tabxobj[i]->DestroyObj(tabxobj[i]);
  XFlush(dpy);
  fsm_close();
  XCloseDisplay(dpy);
}


void Debug(void)
{
  int i,j;

  for (j=1;j<=nbobj;j++)
    for (i=0; i<=TabCObj[TabIdObj[j]].NbCase; i++)
    {
      /* Execution of instruction block */
      fvwm_debug(__func__, "Id de l'objet %d\n",TabIdObj[j]);
      fvwm_debug(__func__, "Nb Instruction %d\n",
                 TabIObj[TabIdObj[j]][i].NbInstr);
    }

}

extern FILE *yyin;
/* Script file reading */
void ReadConfig (char *ScriptName)
{
  char s[FILENAME_MAX];

  /* This is legacy support.  The first try is the command line file name
     with the scriptfile directory.  If the scriptfile directory wasn't set
     it ends up trying the command line file name undecorated, which
     could be a full path, or it could be relative to the current directory.
     Not very pretty, dje 12/26/99 */
  snprintf(s,sizeof(s),"%s%s%s",ScriptPath,(!*ScriptPath ? "" : "/"),ScriptName);
  yyin = fopen(s,"r");
  if (yyin == NULL) {                   /* file not found yet, */
    TryToFind(ScriptName);                   /* look in some other places */
  }
  if (yyin == NULL)
  {
    fprintf(stderr,"[%s][ReadConfig]: <<ERROR>> Can't open the script %s\n",
	    module->name,s);
    exit(1);
  }
  /* We don't define yyout again, it's the standard output */

  /* Applying syntaxic and lexical interpreter */
  if (yyparse() != 0)
  {
    fprintf(stderr, "[%s] Refusing an incomplete script after a parse error\n",
	    module->name);
    exit(1);
  }
  /* Closing the script */

  fclose(yyin);
}

/* Original method for finding the config file didn't work,
   try using the same strategy fvwm uses in its read command.
   1. If the file name starts with a slash, just try it.
   2. Look in the home directory, using FVWM_USERDIR.
   3. If that doesn't work, look in FVWM_DATADIR.
   The extern "yyin" indicates success or failure.
*/
static void TryToFind(char *filename) {
  char path[FILENAME_MAX];

  if (filename[0] == '/') {             /* if absolute path */
    yyin = fopen(filename,"r");
    return;
  }
  snprintf(path,sizeof(path),"%s/%s",getenv("FVWM_USERDIR"),filename);
  yyin = fopen( path, "r" );
  if ( yyin == NULL ) {
    snprintf(path,sizeof(path),"%s/%s",FVWM_DATADIR, filename );
    yyin = fopen( path, "r" );
  }
  return;
}

/* Exiting with Delete button */
RETSIGTYPE DeadPipe(int nonsense)
{
  is_dead_pipe = True;
  Quit (0,NULL);
  SIGNAL_RETURN;
}

/* Reading system.fvwmrc or .fvwmrc file */
void ParseOptions(void)
{
  char *tline;

  InitGetConfigLine(fd,"*FvwmScript");
  GetConfigLine(fd,&tline);
  while(tline != (char *)0) {
    if (strlen(tline) > 1) {
      if (strncasecmp(tline,"ImagePath",9) == 0)
	CopyString(&imagePath,&tline[9]);
      else if (strncasecmp(tline,"*FvwmScriptPath",15) == 0)
	CopyString(&ScriptPath,&tline[15]);
      else if (strncasecmp(tline,"*FvwmScriptDefaultColorset",26) == 0)
      {
	int requested;
	if (ParseColorsetId(&tline[26], NULL, &requested) &&
	    (requested < 0 || AllocColorset(requested)))
	{
		x11base->colorset = requested;
	}
      }
      else if (strncasecmp(tline,"*FvwmScriptDefaultBack",22) == 0)
      {
	x11base->colorset = -1;
	CopyString(&x11base->backcolor,&tline[22]);
      }
      else if (strncasecmp(tline,"*FvwmScriptDefaultFore",22) == 0)
      {
	x11base->colorset = -1;
	CopyString(&x11base->forecolor,&tline[22]);
      }
      else if (strncasecmp(tline,"*FvwmScriptDefaultHilight",25) == 0)
      {
	x11base->colorset = -1;
	CopyString(&x11base->hilicolor,&tline[25]);
      }
      else if (strncasecmp(tline,"*FvwmScriptDefaultShadow",24) == 0)
      {
	x11base->colorset = -1;
	CopyString(&x11base->shadcolor,&tline[24]);
      }
      else if (strncasecmp(tline, "*FvwmScriptFIFOTimeout", 22) == 0)
      {
        char *text = NULL;
        int timeout;

        CopyString(&text, &tline[22]);
        if (script_index_parse(text, 1, SCRIPT_FIFO_MAX_TIMEOUT_MS, &timeout))
          x11base->fifo_timeout_ms = timeout;
        else
          fvwm_debug(__func__, "[%s]: invalid FIFOTimeout", ScriptName);
        free(text);
      }
      else if (strncasecmp(tline, "*FvwmScriptCommandTimeout", 25) == 0)
      {
        char *text = NULL;
        int timeout;

        CopyString(&text, &tline[25]);
        if (script_index_parse(text, 1, SCRIPT_PROCESS_MAX_TIMEOUT_MS,
                               &timeout))
          x11base->command_timeout_ms = timeout;
        else
          fvwm_debug(__func__, "[%s]: invalid CommandTimeout", ScriptName);
        free(text);
      }
      else if (strncasecmp(tline, "*FvwmScriptExecutionLimit", 25) == 0 ||
               strncasecmp(tline, "*FvwmScriptRecursionLimit", 25) == 0)
      {
        char *text = NULL;
        int limit;
        Bool steps = strncasecmp(tline, "*FvwmScriptExecutionLimit", 25) == 0;
        int maximum = steps ? SCRIPT_EXECUTION_MAX_STEPS :
                              SCRIPT_EXECUTION_MAX_DEPTH;

        CopyString(&text, &tline[25]);
        if (script_index_parse(text, 1, maximum, &limit))
        {
          if (steps)
            execution.step_limit = limit;
          else
            execution.depth_limit = limit;
        }
        else
          fvwm_debug(__func__, "[%s]: invalid %s", ScriptName,
                     steps ? "ExecutionLimit" : "RecursionLimit");
        free(text);
      }
      else if (strncasecmp(tline, "*FvwmScriptValueLimit", 21) == 0 ||
               strncasecmp(tline, "*FvwmScriptMemoryLimit", 22) == 0)
      {
        char *text = NULL;
        int limit;
        Bool value = strncasecmp(tline, "*FvwmScriptValueLimit", 21) == 0;
        script_storage *storage = script_storage_runtime();

        CopyString(&text, &tline[value ? 21 : 22]);
        if (script_index_parse(text, 1, SCRIPT_STORAGE_MAX_BYTES, &limit))
        {
          if (value)
            storage->value_limit = limit;
          else
            storage->total_limit = limit;
        }
        else
          fvwm_debug(__func__, "[%s]: invalid %s", ScriptName,
                     value ? "ValueLimit" : "MemoryLimit");
        free(text);
      }
      else if (strncasecmp(tline,"*FvwmScriptDefaultFont",22) == 0)
	CopyStringWithQuotes(&x11base->font,&tline[22]);
      else if (strncasecmp(tline,"*FvwmScriptKeyboardFocus",24) == 0)
	CopyStringWithQuotes(&keyboard_focus_title,&tline[24]);
      else if (strncasecmp(tline,"Colorset",8) == 0)
	LoadColorset(&tline[8]);
      else if (strncasecmp(tline,"*FvwmScriptDefaultCursor",24) == 0)
      {
	CopyString(&x11base->cursor,&tline[24]);
	default_cursor = x11base->cursor;
      }
    }

    GetConfigLine(fd,&tline);
  }
}

Bool FvwmScriptUsesKeyboardFocus(void)
{
	return keyboard_focus_title != NULL && x11base->title != NULL &&
		strcmp(keyboard_focus_title, x11base->title) == 0;
}

static void update_keyboard_lock_mask(void)
{
	XModifierKeymap *map = XGetModifierMapping(dpy);
	KeyCode num_lock = XKeysymToKeycode(dpy, XK_Num_Lock);
	int modifier, key;

	keyboard_lock_mask = LockMask;
	if (map == NULL)
		return;
	for (modifier = 0; modifier < 8; modifier++)
		for (key = 0; key < map->max_keypermod; key++)
			if (num_lock != 0 && map->modifiermap[
			    modifier * map->max_keypermod + key] == num_lock)
				keyboard_lock_mask |= 1U << modifier;
	XFreeModifiermap(map);
}

static int myErrorHandler(Display *dpy, XErrorEvent *event)
{
  /* some errors are acceptable, mostly they're caused by
   * trying to use a deleted pixmap */
  if((event->error_code == BadDrawable) || (event->error_code == BadPixmap) ||
     (event->error_code == BadWindow) ||
     (event->error_code == FRenderGetErrorCodeBase() + FRenderBadPicture))
    return 0;

  PrintXErrorAndCoredump(dpy, event, x11base->title);

  /* return (*oldErrorHandler)(dpy,event); */
  return 0;
}

/* Initialize X server and global variables */
void Xinit(int IsFather)
{
  char *name;
  Atom myatom;
  int i = 16;

  /* Connection to X server */
#ifdef MEMDEBUG
  __bounds_debug_no_checking=True;
#endif

  dpy=XOpenDisplay(NULL);
  if (dpy == NULL)
  {
    fvwm_debug(__func__, "[%s]: <<ERROR>> Can't open display %s",
               ScriptName, XDisplayName(NULL));
    exit(1);
  }
  screen = DefaultScreen(dpy);
  Root = RootWindow(dpy,screen);
  update_keyboard_lock_mask();
  flib_init_graphics(dpy);
  FlocaleAllocateWinString(&FwinString);
  XSetErrorHandler(myErrorHandler);

#ifdef MEMDEBUG
  __bounds_debug_no_checking=False;
#endif

  if (IsFather)
  {
    size_t len = strlen("FvwmScript") + 10;
    name = fxcalloc(sizeof(char), len);
    do
    {
      snprintf(name, len,"%c%xFvwmScript",161,i);
      i++;
      myatom = XInternAtom(dpy, name, False);
    }
    while (XGetSelectionOwner(dpy,myatom) != None)
      ;
    x11base->TabScriptId[1] = name;
    x11base->TabScriptId[0] = NULL;
  }
  x11base->NbChild = 0;
  x11base->root = RootWindow(dpy,screen);
  x_fd = XConnectionNumber(dpy);

  /* install exit procedure to close X down again */
  atexit(ShutdownX);
}

/*
 * Reading an icon
 */
void LoadIcon(struct XObj *xobj)
{
	char *path;
	int depth;
	FvwmPictureAttributes fpa;

	fpa.mask = 0;
	if (xobj->colorset >= 0 && GetColorset(xobj->colorset)->do_dither_icon)
	{
		fpa.mask |= FPAM_DITHER;
	}
	if (xobj->iconPixmap != None)
	{
		XFreePixmap(dpy, xobj->iconPixmap);
		xobj->iconPixmap = None;
	}
	if (xobj->icon_maskPixmap != None)
	{
		XFreePixmap(dpy, xobj->icon_maskPixmap);
		xobj->icon_maskPixmap = None;
	}
	if (xobj->icon_alphaPixmap != None)
	{
		XFreePixmap(dpy, xobj->icon_alphaPixmap);
		xobj->icon_alphaPixmap = None;
	}
	if (xobj->alloc_pixels != NULL && xobj->nalloc_pixels > 0)
	{
		PictureFreeColors(
			dpy, Pcmap, xobj->alloc_pixels, xobj->nalloc_pixels, 0,
			False);
		xobj->alloc_pixels = NULL;
		xobj->nalloc_pixels = 0;
	}
	if ((xobj->icon) == NULL)
		return;
	if ((path = PictureFindImageFile(xobj->icon,imagePath,R_OK)) == NULL)
	{
		return;
	}
	if (!PImageLoadPixmapFromFile(
		dpy, x11base->win, path, &xobj->iconPixmap,
		&xobj->icon_maskPixmap, &xobj->icon_alphaPixmap, &xobj->icon_w,
		&xobj->icon_h, &depth, &xobj->nalloc_pixels,
		&xobj->alloc_pixels, 0, fpa))
	{
		fvwm_debug(__func__, "[%s][LoadIcon]: <<WARNING>> Unable to "
			   "load pixmap %s\n",
			   ScriptName,xobj->icon);
		free(path);
		return;
	}
	if (depth <= 1)
	{
		XGCValues gcv;
		GC m_gc;
		Pixmap temp;

		gcv.background= WhitePixel(dpy, screen);
		gcv.foreground= BlackPixel(dpy, screen);
		m_gc = fvwmlib_XCreateGC(dpy, x11base->win,
				       GCForeground | GCBackground, &gcv);
		temp = XCreatePixmap(dpy, x11base->win,
				     xobj->icon_w, xobj->icon_h, Pdepth);
		XCopyPlane(dpy, xobj->iconPixmap, temp, m_gc,
			   0,0,xobj->icon_w, xobj->icon_h, 0,0,1);
		XFreePixmap(dpy, xobj->iconPixmap);
		XFreeGC(dpy,m_gc);
		xobj->iconPixmap = temp;
	}
	free(path);
}


/* Opening a GUI window */
void OpenWindow (void)
{
  XTextProperty Name;
  XWMHints *IndicWM;
  XSizeHints *IndicNorm;
  unsigned long mask;
  XClassHint classHints;
  XSetWindowAttributes Attr;
  int xcnum;

  /* Allocation for colors */
  if (x11base->colorset >= 0) {
    x11base->TabColor[fore] = GetColorset(x11base->colorset)->fg;
    x11base->TabColor[back] = GetColorset(x11base->colorset)->bg;
    x11base->TabColor[shad] = GetColorset(x11base->colorset)->shadow;
    x11base->TabColor[hili] = GetColorset(x11base->colorset)->hilite;
  } else {
    x11base->TabColor[fore] = GetColor(x11base->forecolor);
    x11base->TabColor[back] = GetColor(x11base->backcolor);
    x11base->TabColor[shad] = GetColor(x11base->shadcolor);
    x11base->TabColor[hili] = GetColor(x11base->hilicolor);
  }

  /* Window properties definition */
  mask = CWBackPixel | CWBorderPixel | CWColormap;
  Attr.background_pixel = x11base->TabColor[back];
  Attr.border_pixel = 0;
  Attr.colormap = Pcmap;

  xcnum = fvwmCursorNameToIndex(default_cursor);
  if (xcnum != -1) {
    Attr.cursor = XCreateFontCursor(dpy, xcnum);
    mask |= CWCursor;
  } else {
    /* Bad name or value, clear the config slot. */
    x11base->cursor = NULL;
  }
  free(default_cursor);

  x11base->win=XCreateWindow(
    dpy, x11base->root, x11base->size.x,x11base->size.y, x11base->size.width,
    x11base->size.height, 0, Pdepth, InputOutput, Pvisual, mask, &Attr);
  x11base->gc=fvwmlib_XCreateGC(dpy,x11base->win,0,NULL);
  if (x11base->colorset >= 0)
    SetWindowBackground(
      dpy, x11base->win, x11base->size.width, x11base->size.height,
      GetColorset(x11base->colorset), Pdepth, x11base->gc, True);

  /* Select which events are received by the window */
  XSelectInput(dpy,x11base->win, KeyPressMask|ButtonPressMask|
	       ExposureMask|ButtonReleaseMask|EnterWindowMask|LeaveWindowMask|
	       ButtonMotionMask|StructureNotifyMask);
  XSelectInput(dpy, x11base->root, PropertyChangeMask);

  /* Specify which parameters are used by the window manager */
  if (XStringListToTextProperty(&x11base->title,1,&Name) == 0)
    fvwm_debug(__func__,
               "[%s][OpenWindow]: <<WARNING>> Can't use icon name\n",
               ScriptName);
  IndicNorm = XAllocSizeHints();
  if (x11base->size.x!=-1)
  {
    IndicNorm->x = x11base->size.x;
    IndicNorm->y = x11base->size.y;
    IndicNorm->flags = PSize|PMinSize|PMaxSize|PBaseSize|PPosition;
  }
  else
    IndicNorm->flags = PSize|PMinSize|PMaxSize|PBaseSize;
  IndicNorm->width = x11base->size.width;
  IndicNorm->height = x11base->size.height;
  IndicNorm->min_width = x11base->size.width;
  IndicNorm->min_height = x11base->size.height;
  IndicNorm->max_width = x11base->size.width;
  IndicNorm->max_height = x11base->size.height;
  IndicWM = XAllocWMHints();
  IndicWM->input = True;
  IndicWM->initial_state = NormalState;
  IndicWM->flags = InputHint|StateHint;

  classHints.res_name = fxstrdup(ScriptBaseName);
  classHints.res_class = fxstrdup(module->name);

  XSetWMProperties(dpy, x11base->win, &Name,
		   &Name, NULL, 0, IndicNorm, IndicWM, &classHints);
  Scrapt = fxcalloc(sizeof(char), 1);

  /* Build atoms for inter application communication */
  propriete = XInternAtom(dpy,"Prop_selection",False);
  wm_del_win = XInternAtom(dpy,"WM_DELETE_WINDOW",False);
  XSetWMProtocols(dpy,x11base->win,&wm_del_win,1);

  free(classHints.res_class);
  free(classHints.res_name);
}

/*
 * Executing an instruction sequence
*/
Bool FvwmScriptExecutionStep(void)
{
  return script_execution_step(&execution);
}

Bool FvwmScriptExecutionAllowed(void)
{
  return execution.failure == SCRIPT_EXECUTION_OK;
}

void FvwmScriptStopForStorage(Bool total)
{
  if (execution.failure == SCRIPT_EXECUTION_OK)
    execution.failure = total ? SCRIPT_EXECUTION_STORAGE : SCRIPT_EXECUTION_VALUE;
}

void ExecBloc(Bloc *bloc)
{
  int i;

  if (bloc == NULL || !script_execution_enter(&execution))
    return;
  for (i = 0; i <= bloc->NbInstr; i++)
  {
    if (!FvwmScriptExecutionStep())
      break;
    TabCom[bloc->TabInstr[i].Type](bloc->TabInstr[i].NbArg,
                                   bloc->TabInstr[i].TabArg);
  }
  script_execution_leave(&execution);
  if (execution.depth == 0 && execution.failure != SCRIPT_EXECUTION_OK)
  {
    const char *limit;
    switch (execution.failure)
    {
      case SCRIPT_EXECUTION_VALUE:
        limit = "ValueLimit";
        break;
      case SCRIPT_EXECUTION_STORAGE:
        limit = "MemoryLimit";
        break;
      case SCRIPT_EXECUTION_STEPS:
        limit = "ExecutionLimit";
        break;
      default:
        limit = "RecursionLimit";
        break;
    }
    if (log_get_fd() >= 0)
      fvwm_debug(__func__, "[%s]: handler stopped at %s", ScriptName, limit);
    else
      fprintf(stderr, "[%s]: handler stopped at %s\n", ScriptName, limit);
  }
}


/* Building the GUI */
void BuildGUI(int IsFather, int argc, char **argv)
{
  int i;

  if (scriptprop->font != NULL)
  {
    /* leak! */
    x11base->font = scriptprop->font;
  }

  if (scriptprop->forecolor != NULL)
  {
    x11base->colorset = -1;
    /* leak! */
    x11base->forecolor = scriptprop->forecolor;
  }
  if (scriptprop->backcolor != NULL)
  {
    x11base->colorset = -1;
    /* leak! */
    x11base->backcolor = scriptprop->backcolor;
  }
  if (scriptprop->shadcolor != NULL)
  {
    x11base->colorset=-1;
    /* leak! */
    x11base->shadcolor = scriptprop->shadcolor;
  }
  if (scriptprop->hilicolor != NULL)
  {
    x11base->colorset=-1;
    /* leak! */
    x11base->hilicolor = scriptprop->hilicolor;
  }
  if (scriptprop->colorset != -1)
    x11base->colorset = scriptprop->colorset;

  x11base->icon = scriptprop->icon;

  {
    int sx;
    int sy;
    int64_t x, y;

    FScreenGetScrRect(NULL, FSCREEN_CURRENT, &sx, &sy, NULL, NULL);
    x = (int64_t)scriptprop->x + sx;
    y = (int64_t)scriptprop->y + sy;
    if (!script_geometry_position(x, y) ||
        !script_geometry_size(scriptprop->width, scriptprop->height))
    {
      script_geometry_diagnostic(__func__, 0);
      exit(1);
    }
    x11base->size.x = (int)x;
    x11base->size.y = (int)y;
  }
  x11base->size.width = scriptprop->width;
  x11base->size.height = scriptprop->height;
  x11base->title = scriptprop->titlewin;

  /* Window initialization */
  OpenWindow();

  /* Go through every graphical object */
  nbobj++;
  for (i=0; i<nbobj; i++)
  {
    tabxobj[i] = fxcalloc(1, sizeof(struct XObj));
    tabxobj[i]->id = (*tabobj)[i].id;
    tabxobj[i]->x = (*tabobj)[i].x;
    tabxobj[i]->y = (*tabobj)[i].y;
    tabxobj[i]->width = (*tabobj)[i].width;
    tabxobj[i]->height = (*tabobj)[i].height;
    if (!script_geometry_startup_size(tabxobj[i]->width,
        tabxobj[i]->height, &tabxobj[i]->width, &tabxobj[i]->height) ||
        !script_geometry_position(tabxobj[i]->x, tabxobj[i]->y))
    {
      script_geometry_diagnostic(__func__, tabxobj[i]->id);
      exit(1);
    }
    tabxobj[i]->value = (*tabobj)[i].value;
    tabxobj[i]->value2 = (*tabobj)[i].value2;
    tabxobj[i]->value3 = (*tabobj)[i].value3;
    tabxobj[i]->flags[0] = (*tabobj)[i].flags[0];
    tabxobj[i]->flags[1] = (*tabobj)[i].flags[1];
    tabxobj[i]->flags[2] = (*tabobj)[i].flags[2];
    tabxobj[i]->flags[3] = (*tabobj)[i].flags[3];
    tabxobj[i]->icon = (*tabobj)[i].icon;
    tabxobj[i]->swallow = (*tabobj)[i].swallow;

    if ((*tabobj)[i].title == NULL)
      tabxobj[i]->title = fxcalloc(1, 200);
    else
      tabxobj[i]->title = (*tabobj)[i].title;

    if ((*tabobj)[i].font == NULL && x11base->font != NULL)
      tabxobj[i]->font = fxstrdup(x11base->font);
    else
      tabxobj[i]->font = (*tabobj)[i].font;

    if ((*tabobj)[i].forecolor == NULL)
      tabxobj[i]->forecolor = fxstrdup(x11base->forecolor);
    else
      tabxobj[i]->forecolor = (*tabobj)[i].forecolor;

    if ((*tabobj)[i].backcolor == NULL)
      tabxobj[i]->backcolor = fxstrdup(x11base->backcolor);
    else
      tabxobj[i]->backcolor = (*tabobj)[i].backcolor;

    if ((*tabobj)[i].shadcolor == NULL)
      tabxobj[i]->shadcolor = fxstrdup(x11base->shadcolor);
    else
      tabxobj[i]->shadcolor = (*tabobj)[i].shadcolor;

    if ((*tabobj)[i].hilicolor == NULL)
      tabxobj[i]->hilicolor = fxstrdup(x11base->hilicolor);
    else
      tabxobj[i]->hilicolor = (*tabobj)[i].hilicolor;

    if ((*tabobj)[i].colorset >= 0)
      tabxobj[i]->colorset=(*tabobj)[i].colorset;
    else if ((*tabobj)[i].backcolor == NULL && (*tabobj)[i].forecolor == NULL &&
	     (*tabobj)[i].shadcolor == NULL && (*tabobj)[i].hilicolor == NULL)
      tabxobj[i]->colorset = x11base->colorset;
    else tabxobj[i]->colorset = -1;


    ChooseFunction(tabxobj[i],(*tabobj)[i].type);
    tabxobj[i]->gc = x11base->gc;
    tabxobj[i]->ParentWin = &(x11base->win);
    tabxobj[i]->iconPixmap = None;
    tabxobj[i]->icon_maskPixmap = None;

    LoadIcon(tabxobj[i]);                  /* Loading the widget icon */

    if (!script_widget_prepare(tabxobj[i]))
    {
      script_geometry_diagnostic(__func__, tabxobj[i]->id);
      exit(1);
    }
    tabxobj[i]->InitObj(tabxobj[i]);
    initialized_widgets++;
  }

  /* Registering the periodic task block */
  x11base->periodictasks = scriptprop->periodictasks;
  /* the QuitFunc */
  x11base->quitfunc = scriptprop->quitfunc;

  /* Register communication before Init can launch or message children. */
  ReadFvwmScriptArg(argc, argv, IsFather);

  /* If an initialization block exists for the script, it's executed here */
  if (scriptprop->initbloc != NULL)
  {
    ExecBloc(scriptprop->initbloc);
    free(scriptprop->initbloc->TabInstr);
    free(scriptprop->initbloc);
  }

  free(tabobj);
  free(scriptprop);
  XMapRaised(dpy,x11base->win);
  for (i=0; i<nbobj; i++)
    if (tabxobj[i]->flags[0] != True)
      XMapWindow(dpy,tabxobj[i]->win);
}

/*
 * Managing messages between objects
 */
void SendMsg(struct XObj *xobj,int TypeMsg)
{
  int i;

  for (i=0; i <= TabCObj[TabIdObj[xobj->id]].NbCase; i++)
    if (TabCObj[TabIdObj[xobj->id]].LstCase[i] == TypeMsg)
    {
      /* Executing the instruction block */
      ExecBloc(&TabIObj[TabIdObj[xobj->id]][i]);
    }
}

/*
 * Managing messages between objects
 */
void SendMsgAndString(char *action, char *type)
{
  int val[2];
  char *arg1 = NULL;
  char *arg2 = NULL;
  XTextProperty window_name;
  int i;

  if (type == NULL)
  {
    fvwm_debug(__func__, "Ignoring empty SendToModule command");
    return;
  }

  if(StrEquals(type,"SendString") || StrEquals(type,"CheckBinding")){
    if (GetIntegerArguments(action, &action, val, 2) == 2){
      if (val[0] > 1000 || val[0] < 1){
	fvwm_debug(__func__,
                   "[%s][%s]: <<WARNING>> Widget id out of range: %i\n",
                   ScriptName,type,val[0]);
	return;
      }

      i = TabIdObj[val[0]];
      if (i != -1) {
	/* The checked parser returns a borrowed continuation after both
	 * integers. Do not allocate disposable tokens or lose their owners. */
	free(LastString);
	LastString = NULL;
	if (action != NULL && strlen(action) > 0)
	  CopyString(&LastString,action);

	SendMsg(tabxobj[i],val[1]);
      }else{
	fvwm_debug(__func__, "[%s][%s]: <<WARNING>> no Widget %i\n",
                   ScriptName,type,val[0]);
      }
    }else{
      fvwm_debug(__func__, "[%s][%s]: <<WARNING>> Syntax Error: %s\n",
                 ScriptName,type,action);
    }
  }else{

    if(StrEquals(type,"ChangeWindowTitle")){
      action=GetNextToken(action, &arg1);
      action=GetNextToken(action, &arg2);

      if (arg1 == NULL)
      {
        fvwm_debug(__func__, "ChangeWindowTitle requires a title");
      }
      else if(arg2 == NULL){

	 XChangeProperty(
		 dpy, x11base->win, XA_WM_NAME, XA_STRING, 8, PropModeReplace,
		 (unsigned char*)arg1, strlen(arg1));

      }else{
	if (!FGetWMName(dpy, x11base->win, &window_name) ||
	    window_name.encoding != XA_STRING) {
		fvwm_debug(__func__, "[%s][SendMsgAndString]: <<ERROR>> "
                           "Can't find the title of a window\n", module->name);
	}else{
	  if(StrEquals(arg2,(char *)window_name.value)){
	    XChangeProperty(
		    dpy, x11base->win, XA_WM_NAME, XA_STRING, 8,
		    PropModeReplace, (unsigned char*)arg1, strlen(arg1));
	  }
	}
	if (window_name.value != NULL)
	  XFree(window_name.value);
      }
    }
    else
    {
	    fvwm_debug(__func__, "[%s][SendMsgAndString]: <<ERROR>> "
                       "Unknown SendToModule command: %s\n", module->name,
                       type);
    }

    free(arg1);
    free(arg2);

  }
}

/*
 * Called by a select request
 */
void SendMsgToScript(XEvent event)
{
  Atom sender;
  XEvent reply = {0};
  XSelectionRequestEvent *request = &event.xselectionrequest;
  char *message;

  sender = XInternAtom(dpy, x11base->TabScriptId[1], True);
  reply.xselection.type = SelectionNotify;
  reply.xselection.requestor = request->requestor;
  reply.xselection.selection = request->selection;
  reply.xselection.target = request->target;
  reply.xselection.time = request->time;
  reply.xselection.property = None;

  /* A refusal must echo the requested target, even for an empty queue. */
  if (sender != None && request->selection == sender &&
      request->target != None)
  {
    message = script_message_take(&BuffSend, request->target);
    if (message != NULL)
    {
      /* ICCCM permits old requestors to omit the property atom. */
      reply.xselection.property = request->property != None ?
        request->property : request->target;
      XChangeProperty(dpy, request->requestor, reply.xselection.property,
                      request->target, 8, PropModeReplace,
                      (unsigned char *)message, strlen(message) + 1);
      free(message);
    }
  }
  FSendEvent(dpy, request->requestor, False, 0, &reply);
}

static
void UpdateRootTransparency(Bool pr_only)
{
	int i;

	if (CSET_IS_TRANSPARENT(x11base->colorset))
	{
		if (CSET_IS_TRANSPARENT_PR_PURE(x11base->colorset))
		{
			XClearArea(dpy, x11base->win, 0, 0, 0, 0, True);
		}
		else if (!pr_only || CSET_IS_TRANSPARENT_PR(x11base->colorset))
		{
			SetWindowBackground(
				dpy, x11base->win, x11base->size.width,
				x11base->size.height,
				GetColorset(x11base->colorset), Pdepth,
				x11base->gc, True);
		}
	}
	for (i=0; i<nbobj; i++)
	{
		if (Rectangle == tabxobj[i]->TypeWidget ||
		    SwallowExec == tabxobj[i]->TypeWidget)
		{
			continue;
		}
		if (!CSET_IS_TRANSPARENT(tabxobj[i]->colorset) ||
		    (CSET_IS_TRANSPARENT_PR(tabxobj[i]->colorset) &&
		     !CSET_IS_TRANSPARENT(x11base->colorset)))
		{
			continue;
		}
		if (CSET_IS_TRANSPARENT_PR_PURE(tabxobj[i]->colorset))
		{
			XClearArea(dpy, tabxobj[i]->win, 0, 0, 0, 0, True);
		}
		else if (!pr_only ||
			 CSET_IS_TRANSPARENT_PR(tabxobj[i]->colorset))
		{
			SetWindowBackground(
				dpy, tabxobj[i]->win, tabxobj[i]->width,
				tabxobj[i]->height,
				GetColorset(tabxobj[i]->colorset), Pdepth,
				tabxobj[i]->gc, True);
		}
	}
}


/* read an X event */
static void script_dispatch_x_event(const XEvent *input)
{
  XEvent event = *input, evnt_sel;
  int i;
  int isTab = 0;
  KeySym ks;
  static unsigned char buf[10];         /* unsigned for international */
  Bool find = False;
  char *action;
  int ex, ey, ex2, ey2;
  XClassHint tmp;

    switch (event.type)
    {
    case Expose:
      ex = event.xexpose.x;
      ey = event.xexpose.y;
      ex2 = event.xexpose.x + event.xexpose.width;
      ey2 = event.xexpose.y + event.xexpose.height;
      for (unsigned budget = 0; budget < 32 &&
           FCheckTypedWindowEvent(dpy, event.xany.window, Expose, &event);
           budget++)
      {
	      ex = min(ex, event.xexpose.x);
	      ey = min(ey, event.xexpose.y);
	      ex2 = max(ex2, event.xexpose.x + event.xexpose.width);
	      ey2 = max(ey2 , event.xexpose.y + event.xexpose.height);
      }
      event.xexpose.x = ex;
      event.xexpose.y = ey;
      event.xexpose.width = ex2 - ex;
      event.xexpose.height = ey2 - ey;
      for (i=0; i<nbobj; i++)
      {
	      if (event.xexpose.window == tabxobj[i]->win)
	      {
		      tabxobj[i]->DrawObj(tabxobj[i], &event);
		      break;
	      }
	      /* handle exposes on x11base that need an object to render */
	      if (event.xexpose.window == x11base->win)
	      {
		      /* redraw first menu item to get the 3d menubar */
		      for (i=0; i<nbobj; i++)
		      {
			      if (Menu == tabxobj[i]->TypeWidget)
			      {
				      tabxobj[i]->DrawObj(tabxobj[i], &event);
				      break;
			      }
		      }
		      /* redraw all Rectangles and SwallowExec's */
		      for (i=0; i<nbobj; i++)
		      {
			      if ((Rectangle == tabxobj[i]->TypeWidget)
				  ||(SwallowExec == tabxobj[i]->TypeWidget))
			      {
				      tabxobj[i]->DrawObj(tabxobj[i], &event);
			      }
		      }
	      }
      }
      break;
    case ConfigureNotify:
    {
      Bool moved = False;

      fev_sanitise_configure_notify(&event.xconfigure);
      moved = event.xconfigure.send_event;
      for (unsigned budget = 0; budget < 32 &&
           FCheckTypedEvent(dpy, ConfigureNotify, &event); budget++)
      {
	fev_sanitise_configure_notify(&event.xconfigure);
	/* check for movement */
	if (event.xconfigure.send_event)
	  moved = True;
      }
      if (moved)
      {
	UpdateRootTransparency(False);
      }
    }
    break;
    case KeyPress:
      /* Key pressed in an object */
      isTab = 0;
      find = False;
      XLookupString(&event.xkey, (char *)buf, sizeof(buf), &ks, NULL);
      event.xkey.keycode =
	XKeysymToKeycode(dpy,fvwm_KeycodeToKeysym(dpy,event.xkey.keycode,0,0));

      /* check for bindings defined by the Key instruction */
	  tmp.res_class = tmp.res_name = "root";
      if ((action = CheckBinding(
		   BindingsList, event.xkey.keycode,
		   event.xkey.state,
		   FvwmScriptUsesKeyboardFocus() ? keyboard_lock_mask : LockMask,
		   C_WINDOW, BIND_KEYPRESS,
		   &tmp, tmp.res_class)) != NULL)
      {
	SendMsgAndString(action, "CheckBinding");
	break;
      }

      if (ks == XK_Tab) {
	isTab = 1;
	if (event.xkey.state & ShiftMask) {
	  /* does not work in general see AddBinding */
	  isTab = 2;
	}
      }
      else if (ks == shift_tab_ks)
	isTab = 2;
      if (keyboard_focus_title != NULL &&
	  strcmp(keyboard_focus_title, x11base->title) == 0)
      {
	/* Search fields must accept typing before a pointer enters them. */
	if (keyboard_focus < 0)
	  for (i = 0; i < nbobj; i++)
	    if (tabxobj[i]->TypeWidget == TextField &&
		!IS_HIDDEN(tabxobj[i]) && !HAS_NO_FOCUS(tabxobj[i]))
	    {
	      keyboard_focus = i;
	      break;
	    }
	if (keyboard_focus >= 0 && isTab == 0)
	{
	  tabxobj[keyboard_focus]->EvtKey(tabxobj[keyboard_focus],
					 &event.xkey);
	  break;
	}
	if (keyboard_focus >= 0)
	  event.xkey.subwindow = tabxobj[keyboard_focus]->win;
      }
      if (event.xkey.subwindow!=0)
      {
	/* Sending event to object */
	for (i=0; i<nbobj; i++)
	{
	  if (tabxobj[i]->win == event.xkey.subwindow)
	  {
	    find = True;
	    if (isTab == 0 && !HAS_NO_FOCUS(tabxobj[i])) {
	      tabxobj[i]->EvtKey(tabxobj[i],&event.xkey);
	    }
	    break;
	  }
	}
      }
      if (isTab > 0) {
	Bool loop = False;
	if (!find)
	  i = (isTab == 1)? -1:nbobj;
	find = False;
	while(!find) {
	  i = (isTab == 1)? i+1 : i-1;
	  if (i == nbobj || i == -1) {
	    i = (isTab == 1)? 0 : nbobj-1;
	    if (loop) {
	      i = -1;
	      break;
	    }
	    loop = True;
	  }
	  if (!IS_HIDDEN(tabxobj[i]) && !HAS_NO_FOCUS(tabxobj[i]) &&
	      tabxobj[i]->TypeWidget != Rectangle &&
	      tabxobj[i]->TypeWidget != HDipstick &&
	      tabxobj[i]->TypeWidget != VDipstick)
	    find = True;
	}
	if (find)
	{
	  if (keyboard_focus_title != NULL &&
	      strcmp(keyboard_focus_title, x11base->title) == 0)
	    keyboard_focus = i;
	  else
	    FWarpPointer(dpy, x11base->win, tabxobj[i]->win, 0, 0, 0, 0,
			 tabxobj[i]->width/2, 10);
	}
      }
      break;
    case ButtonPress:
      /* Which window is clicked in? */
      if (event.xbutton.subwindow != 0)
	for (i=0; i<nbobj; i++)
	  if (tabxobj[i]->win == event.xbutton.subwindow) {
	    if (!HAS_NO_FOCUS(tabxobj[i]))
	      keyboard_focus = i;
	    tabxobj[i]->EvtMouse(tabxobj[i],&event.xbutton);
	    break;
	  }
      break;
    case ButtonRelease:
      break;
    case EnterNotify:
      break;
    case LeaveNotify:
      break;
    case MotionNotify:
      break;
    case MappingNotify:
      XRefreshKeyboardMapping((XMappingEvent*)&event);
      update_keyboard_lock_mask();
      break;
    case SelectionRequest:
      if (event.xselectionrequest.selection == XA_PRIMARY)
      {
	evnt_sel.type = SelectionNotify;
	evnt_sel.xselection.requestor = event.xselectionrequest.requestor;
	evnt_sel.xselection.selection = event.xselectionrequest.selection;
	evnt_sel.xselection.target = event.xselectionrequest.target;
	evnt_sel.xselection.time = event.xselectionrequest.time;
	evnt_sel.xselection.property = event.xselectionrequest.property;
	switch (event.xselectionrequest.target)
	{
	case XA_STRING:
	  XChangeProperty(dpy,
			  evnt_sel.xselection.requestor,
			  evnt_sel.xselection.property,
			  evnt_sel.xselection.target,
			  8,
			  PropModeReplace,
			  (unsigned char *)(Scrapt),
			  strlen(Scrapt)+1);
	  break;
	default:evnt_sel.xselection.property = None;
	}
	FSendEvent(dpy,evnt_sel.xselection.requestor,
		   False,0,&evnt_sel);
      }
      else
	SendMsgToScript(event);
      break;
    case SelectionClear:
      if (event.xselectionclear.selection == XA_PRIMARY)
	UnselectAllTextField(tabxobj);
      break;
    case ClientMessage:
      if ((event.xclient.format == 32) &&
	  (event.xclient.data.l[0] == wm_del_win))
	DeadPipe(1);
      break;
    case PropertyNotify:
    {
      unsigned char *data = NULL;
      size_t bytes = 0;
      FPropertyLimits limits = {0, FPROPERTY_TEXT_MAX_BYTES, 8};

      if (event.xproperty.window != Root ||
          event.xproperty.atom < XA_CUT_BUFFER0 ||
          event.xproperty.atom > XA_CUT_BUFFER7)
        break;
      FPropertyResult result = FPropertyRead(dpy, Root,
          event.xproperty.atom, XA_STRING, limits, &data, &bytes);
      if (result == FPROPERTY_OK && bytes != 0)
      {
        free(Scrapt);
        Scrapt = (char *)data;
      }
      else
      {
        free(data);
        if (result != FPROPERTY_OK && result != FPROPERTY_MISSING)
          fvwm_debug(__func__, "Ignoring invalid cut-buffer text");
      }
      break;
    }
    }
}

void ReadXServer(void)
{
  /* Modal handlers borrow the same dispatcher only for non-input work;
   * never recursively invoke another widget from an active interaction. */
  for (unsigned budget = 0; budget < 32 && !isTerminated &&
       FEventsQueued(dpy, QueuedAfterReading); budget++)
  {
    XEvent event;
    FNextEvent(dpy, &event);
    script_dispatch_x_event(&event);
  }
}


/* main event loop */
static void script_process_packet(FvwmPacket *packet)
{
  int i;

	if (!script_packet_is_valid(packet->type, packet->body,
	    FvwmPacketBodySize(*packet)))
	{
	  fvwm_debug(__func__, "Discarding incomplete Script packet body");
	}
	else if (packet->type == M_CONFIG_INFO) {
	  char *line, *token;
	  int n;

	  line = (char*)&(packet->body[3]);
	  line = GetNextToken(line, &token);
	  if (StrEquals(token, "Colorset")) {
	    /* track all colorset changes and update display if necessary */
	    n = LoadColorset(line);
	    for (i=0; i<nbobj; i++) {
	      if (n >= 0 && n == tabxobj[i]->colorset) {
		tabxobj[i]->TabColor[fore] = GetColorset(n)->fg;
		tabxobj[i]->TabColor[back] = GetColorset(n)->bg;
		tabxobj[i]->TabColor[shad] = GetColorset(n)->shadow;
		tabxobj[i]->TabColor[hili] = GetColorset(n)->hilite;
		if (tabxobj[i]->TypeWidget != SwallowExec) {
		  SetWindowBackground(dpy, tabxobj[i]->win, tabxobj[i]->width,
				      tabxobj[i]->height, GetColorset(n), Pdepth,
				      tabxobj[i]->gc, False);
		  XClearWindow(dpy, tabxobj[i]->win);
		}
		tabxobj[i]->DrawObj(tabxobj[i],NULL);
	      }
	    }
	    if (n >= 0 && n == x11base->colorset) {
	      x11base->TabColor[fore] = GetColorset(n)->fg;
	      x11base->TabColor[back] = GetColorset(n)->bg;
	      x11base->TabColor[shad] = GetColorset(n)->shadow;
	      x11base->TabColor[hili] = GetColorset(n)->hilite;
	      SetWindowBackground(dpy, x11base->win, x11base->size.width,
				  x11base->size.height, GetColorset(n), Pdepth,
				  x11base->gc, True);
	      for (i=0; i<nbobj; i++)
	      {
		if (Rectangle != tabxobj[i]->TypeWidget &&
		    SwallowExec != tabxobj[i]->TypeWidget &&
		    CSET_IS_TRANSPARENT_PR(tabxobj[i]->colorset))
		{
		  XClearArea(dpy, tabxobj[i]->win, 0, 0, 0, 0, True);
		}
	      }
	    }
	  }
	  free(token);
	}
	else if (packet->type == MX_PROPERTY_CHANGE)
	{
	  if (packet->body[0] == MX_PROPERTY_CHANGE_BACKGROUND &&
	      ((!x11base->swallowed && packet->body[2] == 0) ||
	       (x11base->swallowed && packet->body[2] == x11base->win)))
	  {
	    UpdateRootTransparency(True);
	  }
	  else if  (packet->body[0] == MX_PROPERTY_CHANGE_SWALLOW &&
		    packet->body[2] == x11base->win)
	  {
		  char *str;
		  unsigned long u;
		  Window s;
		  char cmd[256];

		  x11base->swallowed = packet->body[1];
		  str = (char *)&(packet->body[3]);
		  if (x11base->swallowed && str && sscanf(str,"%lu",&u) == 1)
		  {
			  x11base->swallower_win = (Window)u;
		  }
		  else
		  {
			  x11base->swallower_win = 0;
		  }
		  /* update the swallower */
		  s = ((x11base->swallower_win && x11base->swallowed)?
		       x11base->swallower_win:x11base->win);
		  for (i=0; i<nbobj; i++)
		  {
			  if (tabxobj[i]->TypeWidget == SwallowExec &&
			      tabxobj[i]->win != None)
			  {
				  snprintf(cmd,sizeof(cmd),"PropertyChange %u %u %lu %lu",
					  MX_PROPERTY_CHANGE_SWALLOW, 1,
					  tabxobj[i]->win, s);
				  SendText(fd,cmd,0);
			  }
		  }
		  UpdateRootTransparency(False);
	  }
	}
	else if (packet->type == M_STRING) {
	  char *action, *token;
	  action = (char*)&(packet->body[3]);
	  action = GetNextToken(action, &token);

          SendMsgAndString(action, token);
	  free(token);
	}
	else
	  for (i=0; i<nbobj; i++)
	    if (tabxobj[i]->ProcessMsg != NULL)
	      tabxobj[i]->ProcessMsg(tabxobj[i], packet->type, packet->body);
}

typedef struct
{
  module_packet_reader packets;
  struct timespec last_periodic;
  Bool periodic;
  Bool session_pending;
  Bool servicing;
} script_runtime;

/* Borrowed only while MainLoop owns the reader and periodic clock. Widget
 * waits share that exact continuation; they never start another packet reader. */
static script_runtime *script_active_runtime;

static bool script_service_runtime(int timeout_ms)
{
  script_runtime *runtime = script_active_runtime;
  struct pollfd ready[FD_SETSIZE + 2];
  struct timespec now = {0};
  size_t session_count;
  int result, deadline;

  if (isTerminated || runtime == NULL)
    return false;
  if (runtime->servicing || execution.depth != 0)
    return true;
  if (runtime->periodic && clock_gettime(CLOCK_MONOTONIC, &now) < 0)
    goto failed;
  deadline = script_readiness_timeout_ms(runtime->periodic,
    runtime->session_pending, &runtime->last_periodic, &now);
  if (timeout_ms >= 0 && (deadline < 0 || deadline > timeout_ms))
    deadline = timeout_ms;
  ready[0] = (struct pollfd){ x_fd, POLLIN, 0 };
  ready[1] = (struct pollfd){ fd[1], POLLIN, 0 };
  session_count = fsm_pollfds(ready + 2, FD_SETSIZE);
  result = fvwmPoll(ready, session_count + 2, deadline);
  if (result < 0)
  {
    if (errno == EINTR)
      return !isTerminated;
    goto failed;
  }
  if (isTerminated)
    return false;
  for (size_t index = 0; index < session_count + 2; index++)
    if (ready[index].revents & POLLNVAL)
    {
      errno = EBADF;
      goto failed;
    }
  if (ready[0].revents & (POLLERR | POLLHUP))
  {
    errno = EIO;
    goto failed;
  }
  runtime->servicing = True;
  if (ready[1].revents & (POLLIN | POLLHUP))
  {
    FvwmPacket *packet;
    module_packet_read_status status =
      module_packet_reader_read(&runtime->packets, &packet);
    if (status == MODULE_PACKET_READ_READY)
      script_process_packet(packet);
    else if (status != MODULE_PACKET_READ_MORE)
    {
      runtime->servicing = False;
      if (status == MODULE_PACKET_READ_ERROR)
        goto failed;
      fvwmSetTerminate(0);
      return false;
    }
  }
  if (ready[1].revents & POLLERR)
    fvwmSetTerminate(0);
  if (!isTerminated && (result > 0 || runtime->session_pending))
    runtime->session_pending = fsm_process_pollfds(ready + 2, session_count);
  if (!isTerminated && runtime->periodic)
  {
    if (clock_gettime(CLOCK_MONOTONIC, &now) < 0)
    {
      runtime->servicing = False;
      goto failed;
    }
    if (script_periodic_timeout_ms(&runtime->last_periodic, &now) == 0)
    {
      /* Advance before callbacks, including nested script execution. */
      runtime->last_periodic = now;
      ExecBloc(x11base->periodictasks);
    }
  }
  runtime->servicing = False;
  return !isTerminated;
failed:
  fvwm_debug(__func__, "Script readiness/transport failed: %s", strerror(errno));
  fvwmSetTerminate(0);
  return false;
}

static bool script_modal_unchanged(const struct XObj *before,
  const struct XObj *after)
{
  /* Objects live until module teardown. Do not dereference saved resources:
   * a serviced command can replace the font/title while a popup is open. */
  return before->resource_revision == after->resource_revision &&
    before->win == after->win && before->Ffont == after->Ffont &&
    before->title == after->title && before->x == after->x &&
    before->y == after->y && before->width == after->width &&
    before->height == after->height && before->value == after->value &&
    before->value2 == after->value2 && before->value3 == after->value3 &&
    before->colorset == after->colorset &&
    memcmp(before->flags, after->flags, sizeof(before->flags)) == 0 &&
    memcmp(before->TabColor, after->TabColor, sizeof(before->TabColor)) == 0;
}

bool FvwmScriptModalService(void *context)
{
  struct XObj *object = context;

  if (object == NULL)
    return !isTerminated && (script_active_runtime == NULL ||
      script_service_runtime(0));
  struct XObj before = *object;

  return script_service_runtime(0) && script_modal_unchanged(&before, object);
}

bool FvwmScriptModalEvent(struct XObj *object, XEvent *event, int timeout_ms)
{
  struct XObj before = *object;

  memset(event, 0, sizeof(*event));
  /* One packet/session pass and at most one periodic callback between X
   * events. XPending flushes output, but no X read is allowed to block. */
  if (!script_service_runtime(FPending(dpy) ? 0 : timeout_ms) ||
      !script_modal_unchanged(&before, object))
    return false;
  if (FPending(dpy))
  {
    FNextEvent(dpy, event);
    switch (event->type)
    {
      case Expose:
      case ConfigureNotify:
      case SelectionRequest:
      case SelectionClear:
      case ClientMessage:
      case PropertyNotify:
      case MappingNotify:
        script_dispatch_x_event(event);
        event->type = 0;
        break;
    }
  }
  return !isTerminated && script_modal_unchanged(&before, object);
}

void MainLoop (void)
{
  script_runtime runtime = {.periodic = x11base->periodictasks != NULL};

  if (!module_packet_reader_init(&runtime.packets, fd[1]))
  {
    fvwm_debug(__func__, "Unable to initialize runtime packet reader: %s",
      strerror(errno));
    return;
  }
  if (runtime.periodic &&
      clock_gettime(CLOCK_MONOTONIC, &runtime.last_periodic) < 0)
  {
    fvwm_debug(__func__, "Unable to initialize periodic clock");
    return;
  }
  script_active_runtime = &runtime;
  while (!isTerminated)
  {
    if (FPending(dpy))
      ReadXServer();
    if (!isTerminated && !script_service_runtime(FPending(dpy) ? 0 : 100))
      break;
  }
  script_active_runtime = NULL;
}

static void ReadFvwmScriptArg(int argc, char **argv,int IsFather)
{
  int i;
  Atom myatom;

  for (i=2; i<100; i++)
    x11base->TabScriptId[i]=NULL;

  if (IsFather)                 /* Father case */
  {
    myatom = XInternAtom(dpy,x11base->TabScriptId[1],True);
    XSetSelectionOwner(dpy, myatom, x11base->win, CurrentTime);
  }
  else
  {                             /* Son case */
    if (argc <= 7 || strlen(argv[7]) < 3)
    {
      fvwm_debug(__func__, "Invalid child script identifier");
      exit(1);
    }
    char *a7_cpy = fxstrdup(argv[7]);

    a7_cpy[strlen(a7_cpy) - 2] = '\0';

    free(x11base->TabScriptId[0]);
    xasprintf(&x11base->TabScriptId[0], "%s", a7_cpy);
    free(a7_cpy);
    x11base->TabScriptId[1] = argv[7];
    myatom=XInternAtom(dpy, x11base->TabScriptId[1], True);
    XSetSelectionOwner(dpy, myatom, x11base->win, CurrentTime);
  }
}


/* signal handler to close down the module */
static RETSIGTYPE
TerminateHandler(int sig)
{
  fvwmSetTerminate(sig);
  SIGNAL_RETURN;
}


/* main procedure */
int main (int argc, char **argv)
{
  int IsFather;
  int i;

  FlocaleInit(LC_CTYPE, "", "", "FvwmScript");

  module = ParseModuleArgs(argc,argv,0); /* no alias */
  if (module == NULL)
  {
    fvwm_debug(__func__, "FvwmScript must be started by Fvwm.\n");
    exit(1);
  }

  if (module->user_argc == 0)
  {
    fvwm_debug(__func__, "FvwmScript requires the script's name or path.\n");
    exit(1);
  }

  /* Check whether the script has a father */
  if (module->user_argc >= 2)
    IsFather = (module->user_argv[1][0] != (char)161);
  else
    IsFather = 1;

  ScriptName = module->user_argv[0];
  ScriptBaseName = GetFileNameFromPath(ScriptName);
  ref = strtol(argv[4], NULL, 16);
  if (ref == 0) ref = None;
  fd[0] = module->to_fvwm;
  fd[1] = module->from_fvwm;
  SetMessageMask(fd, M_NEW_DESK | M_END_WINDOWLIST| M_STRING |
		 M_MAP|  M_RES_NAME| M_RES_CLASS| M_CONFIG_INFO|
		 M_END_CONFIG_INFO| M_WINDOW_NAME | M_SENDCONFIG);
  SetMessageMask(fd, MX_PROPERTY_CHANGE);
  /* Saving the script arguments */
  x11base = fxcalloc(1, sizeof(X11base));
  x11base->TabArg[0] = module->name;
  for (i=2-IsFather; i< module->user_argc &&
       i-1+IsFather < (int)(sizeof(x11base->TabArg) /
                           sizeof(x11base->TabArg[0])); i++)
    x11base->TabArg[i-1+IsFather] = module->user_argv[i];
  /* Default colors and fonts */
  x11base->font = NULL;
  x11base->forecolor = fxstrdup("black");
  x11base->backcolor = fxstrdup("grey85");
  x11base->shadcolor = fxstrdup("grey55");
  x11base->hilicolor = fxstrdup("grey100");
  x11base->colorset = -1;
  x11base->command_timeout_ms = SCRIPT_PROCESS_DEFAULT_TIMEOUT_MS;
  x11base->fifo_timeout_ms = SCRIPT_FIFO_DEFAULT_TIMEOUT_MS;
  x11base->swallowed = False;

  /* Initializing X server and window */
  Xinit(IsFather);

  ParseOptions();

  SendText(fd,"Send_WindowList",0);

  ReadConfig(ScriptName);       /* Read and interpret the script */

  /* Initializing TabCom and TabFunc   */
  InitCom();

#ifdef HAVE_SIGACTION
  {
    struct sigaction  sigact;

    sigemptyset(&sigact.sa_mask);
    sigaddset(&sigact.sa_mask, SIGPIPE);
    sigaddset(&sigact.sa_mask, SIGINT);
    sigaddset(&sigact.sa_mask, SIGHUP);
    sigaddset(&sigact.sa_mask, SIGTERM);
    sigaddset(&sigact.sa_mask, SIGQUIT);
#ifdef SA_INTERRUPT
    sigact.sa_flags = SA_INTERRUPT;
#else
    sigact.sa_flags = 0;
#endif
    sigact.sa_handler = TerminateHandler;
    sigaction(SIGPIPE, &sigact, NULL);
    sigaction(SIGINT,  &sigact, NULL);
    sigaction(SIGHUP,  &sigact, NULL);
    sigaction(SIGTERM, &sigact, NULL);
    sigaction(SIGQUIT, &sigact, NULL);
  }
#else
#ifdef USE_BSD_SIGNALS
  fvwmSetSignalMask( sigmask(SIGPIPE) |
		     sigmask(SIGINT) |
		     sigmask(SIGHUP) |
		     sigmask(SIGTERM) |
		     sigmask(SIGQUIT) );
#endif
  signal (SIGPIPE, TerminateHandler);
  signal (SIGINT, TerminateHandler);  /* cleanup on other ways of closing too */
  signal (SIGHUP, TerminateHandler);
  signal (SIGQUIT, TerminateHandler);
  signal (SIGTERM, TerminateHandler);
#ifdef HAVE_SIGINTERRUPT
  siginterrupt(SIGPIPE, 1);
  siginterrupt(SIGINT,  1);
  siginterrupt(SIGHUP,  1);
  siginterrupt(SIGQUIT, 1);
  siginterrupt(SIGTERM, 1);
#endif
#endif

  /* Compute the Shift-Tab keysym */
  {
    KeyCode tab = XKeysymToKeycode(dpy, XK_Tab);
    shift_tab_ks = fvwm_KeycodeToKeysym(dpy, tab, ShiftMask, 0);
  }

  /* Building buttons and window */
  BuildGUI(IsFather, argc, argv);

  /* tell fvwm we're running */
  SendFinishedStartupNotification(fd);

  MainLoop();
  return 0;
}
