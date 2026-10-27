/* -*-c-*- */
/* FvwmForm is original work of Thomas Zuwei Feng.
 *
 * Copyright Feb 1995, Thomas Zuwei Feng.  No guarantees or warantees are
 * provided or implied in any way whatsoever.  Use this program at your own
 * risk.  Permission to use, modify, and redistribute this program is hereby
 * given, provided that this copyright is kept intact. */

/* This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see: <http://www.gnu.org/licenses/>
 */

#include "config.h"
#include "libs/log.h"

#include <stdio.h>
#include <ctype.h>
#include <event2/event.h>
#include <errno.h>
#include <poll.h>

#include "libs/ftime.h"
#include <fcntl.h>

#include "libs/FRender.h"
#include "libs/Module.h"		/* for headersize, etc. */
#include "libs/module_packet_reader.h"
#include "libs/fvwmlib.h"
#include "libs/fvwmsignal.h"
#include "libs/ColorUtils.h"
#include "libs/Cursor.h"
#include "libs/envvar.h"
#include "libs/Graphics.h"
#include "libs/Parse.h"
#include "libs/Strings.h"
#include "libs/System.h"
#include "libs/XError.h"
#include "libs/FEvent.h"

#include "libs/flocale_font_types.h"               /* for font definition stuff */
#include "libs/PictureBase.h"		 /* for PictureInitCMap */
#include "libs/Colorset.h"
#include "libs/FScreen.h"
#include "libs/FShape.h"
#include "libs/FRenderInit.h"
#include "libs/Rectangles.h"

#include "FvwmForm.h"
#include "libs/ui_layout.h"
#include "form_input.h"			/* common FvwmForm stuff */
#include <limits.h>

/* globals that are exported, keep in sync with externs in FvwmForm.h */
Form cur_form;			 /* current form */

/* Link list roots */
Item *root_item_ptr;		 /* pointer to root of item list */
Line root_line = {&root_line,	 /* ->next */
		  0,		 /* number of items */
		  L_CENTER,0,0,	 /* justify, size x/y */
		  0,		 /* current array size */
		  0};		 /* items ptr */
Line *cur_line = &root_line;		/* curr line in parse rtns */
char preload_yorn='n';		 /* init to non-preload */
Item *item;				/* current during parse */
Item *cur_sel, *cur_button;		/* current during parse */
Item *timer = NULL;			/* timeout tracking */
Display *dpy;
Atom wm_del_win;
int fd_x;		   /* fd for X connection */
Window root, ref;
int screen;
char *color_names[4];
char bg_state = 'd';			/* in default state */
  /* d = default state */
  /* s = set by command (must be in "d" state for first "back" cmd to set it) */
  /* u = used (color allocated, too late to accept "back") */
char endDefaultsRead = 'n';
char *font_names[4];
char *screen_background_color;
char *mname;
static ModuleArgs *module;
int Channel[2];
Bool Swallowed = False;

/* Font/color stuff
   The colors are:
   . defaults are changed by commands during parse
   . copied into items during parse
   . displayed in the customization dialog
   */
int colorset = -1;
int itemcolorset = 0;

/* global not exported */
const struct timeval tv_100ms = {0L, 100000L};
static struct event *pev;
static struct event_base *form_timer_base;
static module_packet_reader form_packet_reader;
/* Retain the original gravity/position flags when live layout changes minima. */
static XSizeHints form_size_hints;

/* prototypes */
static void RedrawSeparator(Item *item);
static void AssignDrawTable(Item *);
static void AddItem(void);
static void PutDataInForm(char *);
static void ReadFormData(void);
static void FormVarsCheck(char **);
static bool form_native_cleanup_register(void);
static void TimerHandler(evutil_socket_t, short int, void *);
static int ErrorHandler(Display *dpy, XErrorEvent *error_event);
static void cleanup_form_timer(void)
{
  if (pev != NULL)
  {
    event_free(pev);
    pev = NULL;
  }
  if (form_timer_base != NULL)
  {
    event_base_free(form_timer_base);
    form_timer_base = NULL;
  }
}

static Bool SetupTimer(void)
{
  form_timer_base = event_base_new();
  if (form_timer_base == NULL)
    return False;

  pev = event_new(form_timer_base, -1, EV_PERSIST, TimerHandler, NULL);
  if (pev == NULL || evtimer_add(pev, &tv_100ms) < 0 ||
      atexit(cleanup_form_timer) != 0)
  {
    cleanup_form_timer();
    return False;
  }
  return True;
}

/* copy a string until '"', or '\n', or '\0' */
static char *CopyQuotedString (const char *cp)
{
  /* FGettext borrows input through its legacy mutable-pointer signature. */
  return form_copy_quoted(_((char *)cp));
}

/* Solid tokens retain escapes, including escaped whitespace. */
static char *CopySolidString (const char *cp)
{
  return form_copy_token(cp);
}

/* Command parsing section */

/* FvwmAnimate++ type command table */
typedef struct CommandTable
{
  char *name;
  void (*function)(char *);
} ct;
static void ct_ActivateOnPress(char *);
static void ct_Back(char *);
static void ct_Button(char *);
static void ct_ButtonFont(char *);
static void ct_ButtonInPointer(char *);
static void ct_ButtonInPointerBack(char *);
static void ct_ButtonInPointerFore(char *);
static void ct_ButtonPointer(char *);
static void ct_ButtonPointerBack(char *);
static void ct_ButtonPointerFore(char *);
static void ct_Choice(char *);
static void ct_close_command(char *);
static void ct_Command(char *);
static void ct_CommandAppend(char *);
static void ct_Font(char *);
static void ct_Fore(char *);
static void ct_GrabServer(char *);
static void ct_Input(char *);
static void ct_InputAppend(char *);
static void ct_InputFont(char *);
static void ct_input_hex(char *);
static void ct_input_readonly(char *);
static void ct_ItemBack(char *);
static void ct_ItemFore(char *);
static void ct_Line(char *);
static void ct_List(char *);
static void ct_ListItem(char *);
static void ct_ListFocus(char *);
static void ct_ListGrid(char *);
static void ct_list_viewport(char *);
static void ct_list_versioned(char *);
static void ct_list_notify(char *);
static void ct_list_watch(char *);
static void ct_label(char *);
static void ct_label_viewport(char *);
static void form_label_viewports_prepare(void);
static void am_set_label(char *);
static void am_renew_timeout(char *);
static bool form_list_report(Item *, const char *, bool);
static void am_list_begin(char *);
static void am_list_entry(char *);
static void am_list_commit(char *);
static void am_list_abort(char *);
static void am_list_status(char *);
static void am_list_watch_status(char *);
static void am_list_focus(char *);
static void am_list_layout(char *);
static Item *form_current_list;
static void ct_Message(char *);
static void ct_Geometry(char *);
static void ct_Position(char *);
static void ct_Read(char *);
static void ct_Selection(char *);
static void ct_Separator(char *);
static void ct_Text(char *);
static void ct_text_hex(char *);
static void ct_InputPointer(char *);
static void ct_InputPointerBack(char *);
static void ct_InputPointerFore(char *);
static void ct_Timeout(char *);
static void ct_TimeoutFont(char *);
static void ct_Title(char *);
static void ct_UseData(char *);
static void ct_padVText(char *);
static void ct_WarpPointer(char *);
static void ct_Colorset(char *);
static void ct_ItemColorset(char *);

/* Must be in Alphabetic order (caseless) */
static struct CommandTable ct_table[] =
{
  {"ActivateOnPress",ct_ActivateOnPress},
  {"Back",ct_Back},
  {"Button",ct_Button},
  {"ButtonFont",ct_ButtonFont},
  {"ButtonInPointer",ct_ButtonInPointer},
  {"ButtonInPointerBack",ct_ButtonInPointerBack},
  {"ButtonInPointerFore",ct_ButtonInPointerFore},
  {"ButtonPointer",ct_ButtonPointer},
  {"ButtonPointerBack",ct_ButtonPointerBack},
  {"ButtonPointerFore",ct_ButtonPointerFore},
  {"Choice",ct_Choice},
  {"CloseCommand",ct_close_command},
  {"Colorset",ct_Colorset},
  {"Command",ct_Command},
  {"CommandAppend",ct_CommandAppend},
  {"Font",ct_Font},
  {"Fore",ct_Fore},
  {"Geometry",ct_Geometry},
  {"GrabServer",ct_GrabServer},
  {"Input",ct_Input},
  {"InputAppend",ct_InputAppend},
  {"InputFont",ct_InputFont},
  {"InputHex",ct_input_hex},
  {"InputPointer",ct_InputPointer},
  {"InputPointerBack",ct_InputPointerBack},
  {"InputPointerFore",ct_InputPointerFore},
  {"InputReadonly",ct_input_readonly},
  {"ItemBack",ct_ItemBack},
  {"ItemColorset",ct_ItemColorset},
  {"ItemFore",ct_ItemFore},
  {"Label",ct_label},
  {"LabelViewport",ct_label_viewport},
  {"Line",ct_Line},
  {"List",ct_List},
  {"ListFocus",ct_ListFocus},
  {"ListGrid",ct_ListGrid},
  {"ListItem",ct_ListItem},
  {"ListNotify",ct_list_notify},
  {"ListVersioned",ct_list_versioned},
  {"ListViewport",ct_list_viewport},
  {"ListWatch",ct_list_watch},
  {"Message",ct_Message},
  {"PadVText",ct_padVText},
  {"Position",ct_Position},
  {"Selection",ct_Selection},
  {"Separator",ct_Separator},
  {"Text",ct_Text},
  {"TextHex",ct_text_hex},
  {"Timeout",ct_Timeout},
  {"TimeoutFont",ct_TimeoutFont},
  {"Title",ct_Title},
  {"UIScale",form_scale_configure},
  {"UseData",ct_UseData},
  {"WarpPointer",ct_WarpPointer}
};
/* These commands are the default setting commands,
   read before the other form defining commands. */
static struct CommandTable def_table[] =
{
  {"ActivateOnPress",ct_ActivateOnPress},
  {"Back",ct_Back},
  {"ButtonFont",ct_ButtonFont},
  {"ButtonInPointer",ct_ButtonInPointer},
  {"ButtonInPointerBack",ct_ButtonInPointerBack},
  {"ButtonInPointerFore",ct_ButtonInPointerFore},
  {"ButtonPointer",ct_ButtonPointer},
  {"ButtonPointerBack",ct_ButtonPointerBack},
  {"ButtonPointerFore",ct_ButtonPointerFore},
  {"Colorset",ct_Colorset},
  {"Font",ct_Font},
  {"Fore",ct_Fore},
  {"InputFont",ct_InputFont},
  {"InputPointer",ct_InputPointer},
  {"InputPointerBack",ct_InputPointerBack},
  {"InputPointerFore",ct_InputPointerFore},
  {"ItemBack",ct_ItemBack},
  {"ItemColorset",ct_ItemColorset},
  {"ItemFore",ct_ItemFore},
  {"Read",ct_Read},
  {"TimeoutFont",ct_TimeoutFont}
};

/* If there were vars on the command line, do env var sustitution on
   all input. */
static void FormVarsCheck(char **p)
{
  if (CF.have_env_var) {		/* if cmd line vars */
    if (strlen(*p) + 200 > CF.expand_buffer_size) { /* fast and loose */
      CF.expand_buffer_size = strlen(*p) + 2000; /* new size */
      if (CF.expand_buffer) {		/* already have one */
	CF.expand_buffer = fxrealloc(CF.expand_buffer, CF.expand_buffer_size,
			sizeof(CF.expand_buffer));
      } else {				/* first time */
	CF.expand_buffer = fxmalloc(CF.expand_buffer_size);
      }
    }
    strcpy(CF.expand_buffer,*p);
    *p = CF.expand_buffer;
    envExpand(*p,CF.expand_buffer_size); /* expand the command in place */
  }
}

static void ParseDefaults(char *buf)
{
  char *p;
  struct CommandTable *e;
  if (form_scale_message(buf))
    return;
  if (!form_chomp_line(buf)) {
    return;
  }
  /* Accept commands beginning with "*FvwmFormDefault".
     This is to make sure defaults are read first.
     Note the hack w. bg_state. */
  if (strncasecmp(buf, "*FvwmFormDefault", 16) == 0) {
    p=buf+16;
    FormVarsCheck(&p);			 /* do var substitution if called for */
    e = FindToken(p,def_table,struct CommandTable);/* find cmd in table */
    if (e != 0) {			/* if its valid */
      p=p+strlen(e->name);		/* skip over name */
      while (isspace((unsigned char)*p)) p++;	       /* skip whitespace */
      e->function(p);			/* call cmd processor */
      bg_state = 'd';			/* stay in default state */
    }
  }
} /* end function */


static void ParseConfigLine(char *buf)
{
  char *p;
  struct CommandTable *e;
  if (form_scale_message(buf))
    return;
  if (!form_chomp_line(buf)) {
    return;
  }


  if (strncasecmp(buf, "Colorset", 8) == 0) {
    LoadColorset(&buf[8]);
    return;
  }
  if (strncasecmp(buf, mname, module->namelen+1) != 0) {/* If its not for me */
    return;
  } /* Now I know its for me. */
  p = buf+module->namelen+1;		      /* jump to end of my name */
  /* at this point we have recognized "*FvwmForm" */
  FormVarsCheck(&p);
  e = FindToken(p,ct_table,struct CommandTable);/* find cmd in table */
  if (e == 0) {			      /* if no match */
    fprintf(stderr,"%s: unknown command: %s\n",module->name,buf);
    return;
  }

  p=p+strlen(e->name);			/* skip over name */
  while (isspace((unsigned char)*p)) p++;	       /* skip whitespace */

  FormVarsCheck(&p);			 /* do var substitution if called for */

  e->function(p);			/* call cmd processor */
  return;
} /* end function */

/* Publish capacity only after storage is ready. Counts remain legacy ints. */
static void *form_reserve_array(void *array, int *capacity, int used,
    int spare, int initial, size_t element_size)
{
  int next;

  if (!form_array_capacity(*capacity, used, spare, initial, element_size,
      &next)) {
    fprintf(stderr, "%s: array capacity is not representable\n", module->name);
    exit(1);
  }
  if (next != *capacity) {
    array = fxrealloc(array, next, element_size);
    *capacity = next;
  }
  return array;
}

/* Function to add an item to the current line */
static void AddToLine(Item *newItem)
{
  cur_line->items = form_reserve_array(cur_line->items,
      &cur_line->item_array_size, cur_line->n, 2, ITEMS_PER_EXPANSION,
      sizeof(*cur_line->items));
  cur_line->items[cur_line->n++] = newItem; /* add to lines item array */
  cur_line->size_x += newItem->header.size_x; /* incr lines width */
  if (cur_line->size_y < newItem->header.size_y) { /* new item bigger */
    cur_line->size_y = newItem->header.size_y; /* then line is bigger */
  }
}


/* All the functions starting with "ct_" (command table) are called thru
   their function pointers. Arg 1 is always the rest of the command. */
static void ct_ActivateOnPress(char *cp)
{
  /* arg can be "on", "off", "0", 1", "true", "false", "" */
  char option[6];
  int i,j;
  if (strlen(cp) > 5) {
    fprintf(stderr,"%s: arg for ActivateOnPress (%s) too long\n",
	    module->name,cp);
    return;
  }
  for (i=0,j=0;i<strlen(cp);i++) {
    if (cp[i] != ' ') {			  /* remove any spaces */
      option[j++]=tolower(cp[i]);
    }
  }
  option[j]=0;
  if (strcmp(option,"on") == 0
      || strcmp(option,"true") == 0
      || strcmp(option,"1") == 0
      || (cp == 0 || strcmp(option,"")) == 0) {
    CF.activate_on_press = 1;
  } else if (strcmp(option,"off") == 0
	     || strcmp(option,"false") == 0
	     || strcmp(option,"0") == 0) {
    CF.activate_on_press = 0;
  } else {
    fprintf(stderr,"%s: arg for ActivateOnPress (%s/%s) invalid\n",
	    module->name,option,cp);
  }
}
static void ct_GrabServer(char *cp)
{
  CF.grab_server = 1;
}
static void ct_WarpPointer(char *cp)
{
  CF.warp_pointer = 1;
}
static void ct_Position(char *cp)
{
  CF.have_geom = 1;
  CF.gx = atoi(cp);
  CF.xneg = 0;
  if (CF.gx < 0)
  {
    CF.gx = -1 - CF.gx;
    CF.xneg = 1;
  }
  cp = (char *)form_next_argument(cp);
  CF.gy = atoi(cp);
  CF.yneg = 0;
  if (CF.gy < 0)
  {
    CF.gy = -1 - CF.gy;
    CF.yneg = 1;
  }
  myfprintf((stderr, "Position @ (%c%d%c%d)\n",
	     (CF.xneg)?'-':'+',CF.gx, (CF.yneg)?'-':'+',CF.gy));
}
static void ct_Geometry(char *cp)
{
  int x = 0;
  int y = 0;
  int flags;
  unsigned int dummy;

  CF.gx = 0;
  CF.gy = 0;
  CF.xneg = 0;
  CF.yneg = 0;
  while (isspace(*cp))
    cp++;
  flags = FScreenParseGeometry(cp, &x, &y, &dummy, &dummy);
  if (flags&XValue)
  {
    CF.have_geom = 1;
    CF.gx = x;
    CF.xneg = (flags&XNegative);
  }
  if (flags&YValue)
  {
    CF.have_geom = 1;
    CF.gy = y;
    CF.yneg = (flags&YNegative);
  }
  myfprintf((stderr, "Geometry @ (%c%d%c%d)\n",
	     (CF.xneg)?'-':'+',CF.gx, (CF.yneg)?'-':'+',CF.gy));
}
static void ct_Fore(char *cp)
{
  free(color_names[c_fg]);
  color_names[c_fg] = fxstrdup(cp);
  colorset = -1;
  myfprintf((stderr, "ColorFore: %s\n", color_names[c_fg]));
}
static void ct_Back(char *cp)
{
  free(color_names[c_bg]);
  color_names[c_bg] = fxstrdup(cp);
  if (bg_state == 'd')
  {
    free(screen_background_color);
    screen_background_color = fxstrdup(color_names[c_bg]);
    bg_state = 's';			/* indicate set by command */
  }
  colorset = -1;
  myfprintf((stderr, "ColorBack: %s, screen background %s, bg_state %c\n",
	     color_names[c_bg],screen_background_color,(int)bg_state));
}
static void ct_Colorset(char *cp)
{
  int requested;
  if (ParseColorsetId(cp, NULL, &requested) &&
      (requested < 0 || AllocColorset(requested)))
  {
    colorset = requested;
  }
}
static void ct_ItemFore(char *cp)
{
  free(color_names[c_item_fg]);
  color_names[c_item_fg] = fxstrdup(cp);
  itemcolorset = -1;
  myfprintf((stderr, "ColorItemFore: %s\n", color_names[c_item_fg]));
}
static void ct_ItemBack(char *cp)
{
  free(color_names[c_item_bg]);
  color_names[c_item_bg] = fxstrdup(cp);
  itemcolorset = -1;
  myfprintf((stderr, "ColorItemBack: %s\n", color_names[c_item_bg]));
}
static void ct_ItemColorset(char *cp)
{
  int requested;
  if (ParseColorsetId(cp, NULL, &requested) &&
      (requested < 0 || AllocColorset(requested)))
  {
    itemcolorset = requested;
  }
}
static void ct_Font(char *cp)
{
  free(font_names[f_text]);
  CopyStringWithQuotes(&font_names[f_text], cp);
  myfprintf((stderr, "Font: %s\n", font_names[f_text]));
}
static void ct_TimeoutFont(char *cp)
{
  free(font_names[f_timeout]);
  CopyStringWithQuotes(&font_names[f_timeout], cp);
  myfprintf((stderr, "TimeoutFont: %s\n", font_names[f_timeout]));
}
static void ct_ButtonFont(char *cp)
{
  free(font_names[f_button]);
  CopyStringWithQuotes(&font_names[f_button], cp);
  myfprintf((stderr, "ButtonFont: %s\n", font_names[f_button]));
}
static void ct_ButtonInPointer(char *cp)
{
  int cursor;
  cursor=fvwmCursorNameToIndex(cp);
  if (cursor == -1) {
    fprintf(stderr,"ButtonInPointer: invalid cursor name %s\n",cp);
  } else {
    CF.pointer[button_in_pointer] = XCreateFontCursor(dpy,cursor);
  }
}
static void ct_ButtonPointer(char *cp)
{
  int cursor;
  cursor=fvwmCursorNameToIndex(cp);
  if (cursor == -1) {
    fprintf(stderr,"ButtonPointer: invalid cursor name %s\n",cp);
  } else {
    CF.pointer[button_pointer] = XCreateFontCursor(dpy,cursor);
  }
}
static void ct_InputFont(char *cp)
{
  free(font_names[f_input]);
  CopyStringWithQuotes(&font_names[f_input], cp);
  myfprintf((stderr, "InputFont: %s\n", font_names[f_input]));
}
static void ct_InputPointer(char *cp)
{
  int cursor;
  cursor=fvwmCursorNameToIndex(cp);
  if (cursor == -1) {
    fprintf(stderr,"InputPointer: invalid cursor name %s\n",cp);
  } else {
    CF.pointer[input_pointer] = XCreateFontCursor(dpy,cursor);
  }
}
/* process buttons using an index to the ___ array */
static void color_arg(Ptr_color *color_cell, char *color_name);
static void color_arg(Ptr_color *color_cell, char *color_name) {
  if (color_name && *color_name) {
    color_cell->pointer_color.pixel = GetColor(color_name);
    color_cell->used=1;
  }
}

/* arg is a color name, alloc the color */
static void ct_ButtonInPointerBack(char *cp) {
  color_arg(&CF.p_c[button_in_back],cp);
}
static void ct_ButtonInPointerFore(char *cp) {
  color_arg(&CF.p_c[button_in_fore],cp);
}
static void ct_ButtonPointerBack(char *cp) {
  color_arg(&CF.p_c[button_back],cp);
}
static void ct_ButtonPointerFore(char *cp) {
  color_arg(&CF.p_c[button_fore],cp);
}
static void ct_InputPointerBack(char *cp) {
  color_arg(&CF.p_c[input_back],cp);
}
static void ct_InputPointerFore(char *cp) {
  color_arg(&CF.p_c[input_fore],cp);
}

static void ct_Line(char *cp)
{
  /* malloc new line */
  cur_line->next = fxmalloc(sizeof(struct _line));
  memset(cur_line->next, 0, sizeof(struct _line));
  cur_line = cur_line->next;		/* new current line */
  cur_line->next = &root_line;		/* new next ptr, (actually root) */

  if (strncasecmp(cp, "left", 4) == 0)
    cur_line->justify = L_LEFT;
  else if (strncasecmp(cp, "right", 5) == 0)
    cur_line->justify = L_RIGHT;
  else if (strncasecmp(cp, "center", 6) == 0)
    cur_line->justify = L_CENTER;
  else
    cur_line->justify = L_LEFTRIGHT;
}
/* Define an area on the form that contains the current
   fvwm error message.
   syntax: *FFMessage Length nn, Font fn, Fore color, Back color
   len default is 70
   font,fg.bg default to text.
  */

static void ct_Message(char *cp)
{
  AddItem();
  bg_state = 'u';			/* indicate b/g color now used. */
  item->type = I_TEXT;
  /* Item now added to list of items, now it needs a pointer
     to the correct DrawTable. */
  AssignDrawTable(item);
  item->header.name = "FvwmMessage";	/* No purpose to this? dje */
  item->text.value = fxmalloc(80);	/* point at last error recvd */
  item->text.n = 80;
  strcpy(item->text.value,"A mix of chars. MM20"); /* 20 mixed width chars */
  item->header.size_x = (FlocaleTextWidth(item->header.dt_ptr->dt_Ffont,
					  item->text.value,
					  item->text.n/4) * 4) + 2 * TEXT_SPC;
  item->header.size_y = item->header.dt_ptr->dt_Ffont->height
    + CF.padVText;
  memset(item->text.value,' ',80);	/* Clear it out */
  myfprintf((stderr, "Message area [%d, %d]\n",
	     item->header.size_x, item->header.size_y));
  AddToLine(item);
  CF.last_error = item;			/* save location of message item */
}

/* allocate colors and fonts needed */
static void CheckAlloc(Item *this_item,DrawTable *dt)
{
  XGCValues xgcv;
  int xgcv_mask = GCForeground;

  if (dt->dt_used == 2) {		/* fonts colors shadows */
    return;
  }
  if (dt->dt_used == 0) {		/* if nothing allocated */
    dt->dt_colors[c_fg] = (colorset < 0)
      ? GetColor(dt->dt_color_names[c_fg])
      : GetColorset(colorset)->fg;
    dt->dt_colors[c_bg] = (colorset < 0)
      ? GetColor(dt->dt_color_names[c_bg])
      : GetColorset(colorset)->bg;
    xgcv.foreground = dt->dt_colors[c_fg];
    xgcv.background = dt->dt_colors[c_bg];
    if (dt->dt_Ffont->font != NULL)
    {
      xgcv_mask |= GCFont;
      xgcv.font = dt->dt_Ffont->font->fid;
    }
    xgcv_mask |= GCBackground;
    dt->dt_GC = fvwmlib_XCreateGC(dpy, CF.frame, xgcv_mask, &xgcv);

    dt->dt_used = 1;			/* fore/back font allocated */
  }
  if (this_item->type == I_TEXT
      || this_item->type == I_TIMEOUT) {      /* If no shadows needed */
    return;
  }
  dt->dt_colors[c_item_fg] = (itemcolorset < 0)
    ? GetColor(dt->dt_color_names[c_item_fg])
    : GetColorset(itemcolorset)->fg;
  dt->dt_colors[c_item_bg] = (itemcolorset < 0)
    ? GetColor(dt->dt_color_names[c_item_bg])
    : GetColorset(itemcolorset)->bg;
  xgcv.foreground = dt->dt_colors[c_item_fg];
  xgcv.background = dt->dt_colors[c_item_bg];
  xgcv_mask = GCForeground;
  if (dt->dt_Ffont->font != NULL)
  {
    xgcv_mask |= GCFont;
    xgcv.font = dt->dt_Ffont->font->fid;
  }
  dt->dt_item_GC = fvwmlib_XCreateGC(dpy, CF.frame, xgcv_mask, &xgcv);
  if (Pdepth < 2) {
    dt->dt_colors[c_itemlo] = PictureBlackPixel();
    dt->dt_colors[c_itemhi] = PictureWhitePixel();
  } else {
    dt->dt_colors[c_itemlo] = (itemcolorset < 0)
      ? GetShadow(dt->dt_colors[c_item_bg])
      : GetColorset(itemcolorset)->shadow;
    dt->dt_colors[c_itemhi] = (itemcolorset < 0)
      ? GetHilite(dt->dt_colors[c_item_bg])
      : GetColorset(itemcolorset)->hilite;
  }
  dt->dt_used = 2;		       /* fully allocated */
}


/* Input is the current item.  Assign a drawTable entry to it. */
static void AssignDrawTable(Item *adt_item)
{
  DrawTable *find_dt, *last_dt;
  char *match_text_fore;
  char *match_text_back;
  char *match_item_fore;
  char *match_item_back;
  char *match_font;
  DrawTable *new_dt;			/* pointer to a new one */

  match_text_fore = color_names[c_fg];
  match_text_back = color_names[c_bg];
  match_item_fore = color_names[c_item_fg];
  match_item_back = color_names[c_item_bg];

  switch(adt_item->type) {
  case I_TEXT:
  case I_SEPARATOR:			/* hack */
    match_font = font_names[f_text];
    break;
  case	I_TIMEOUT:
    match_font = font_names[f_timeout];
    break;
  case I_INPUT:
    match_font = font_names[f_input];
    break;
  default:
    match_font = font_names[f_button]; /* choices same as buttons */
    break;
  }
  last_dt = 0;
  for (find_dt = CF.roots_dt;		   /* start at front */
       find_dt != 0;			/* until end */
       find_dt = find_dt->dt_next) {	/* follow next pointer */
    last_dt = find_dt;
    if ((strcasecmp(match_text_fore,find_dt->dt_color_names[c_fg]) == 0) &&
	(strcasecmp(match_text_back,find_dt->dt_color_names[c_bg]) == 0) &&
	(strcasecmp(match_item_fore,find_dt->dt_color_names[c_item_fg]) == 0) &&
	(strcasecmp(match_item_back,find_dt->dt_color_names[c_item_bg]) == 0) &&
	(strcasecmp(match_font,find_dt->dt_font_name) == 0)) { /* Match */
      adt_item->header.dt_ptr = find_dt;       /* point item to drawtable */
      return;				/* done */
    } /* end match */
  } /* end all drawtables checked, no match */

  /* Time to add a DrawTable */
  /* get one */
  new_dt = fxmalloc(sizeof(struct _drawtable));
  memset(new_dt, 0, sizeof(struct _drawtable));
  new_dt->dt_next = 0;			/* new end of list */
  if (CF.roots_dt == 0) {		   /* If first entry in list */
    CF.roots_dt = new_dt;		   /* set root pointer */
  } else {				/* not first entry */
    last_dt->dt_next = new_dt;		/* link old to new */
  }

  new_dt->dt_font_name = fxstrdup(match_font);
  new_dt->dt_color_names[c_fg]	    = fxstrdup(match_text_fore);
  new_dt->dt_color_names[c_bg]	    = fxstrdup(match_text_back);
  new_dt->dt_color_names[c_item_fg] = fxstrdup(match_item_fore);
  new_dt->dt_color_names[c_item_bg] = fxstrdup(match_item_back);
  new_dt->dt_used = 0;			/* show nothing allocated */
  new_dt->dt_Ffont = FlocaleLoadFont(dpy, new_dt->dt_font_name, module->name);
  if (new_dt->dt_Ffont == NULL) {
    /* A refused new table retains the preceding usable font, or the
     * default font when this is the first startup table. Own its reference. */
    fvwm_debug(__func__, "Cannot load form font; using previous/default font");
    new_dt->dt_Ffont = FlocaleLoadFont(dpy,
        last_dt && last_dt->dt_Ffont ? last_dt->dt_Ffont->name : NULL,
        module->name);
    if (new_dt->dt_Ffont == NULL) {
      fvwm_debug(__func__, "Cannot load fallback form font");
      exit(1);
    }
  }
  FlocaleAllocateWinString(&new_dt->dt_Fstr);

  myfprintf((stderr,"Created drawtable with %s %s %s %s %s\n",
	     new_dt->dt_color_names[c_fg], new_dt->dt_color_names[c_bg],
	     new_dt->dt_color_names[c_item_fg],
	     new_dt->dt_color_names[c_item_bg],
	     new_dt->dt_font_name));
  adt_item->header.dt_ptr = new_dt;	       /* point item to new drawtable */
}

/* input/output is global "item" - currently allocated last item */
static void AddItem(void)
{
  Item *save_item;
  save_item = (Item *)item;		/* save current item */
  item = fxcalloc(sizeof(Item), 1); /* get a new item */
  if (save_item == 0) {			/* if first item */
    root_item_ptr = item;		/* save root item */
  } else {				/* else not first item */
    save_item->header.next = item;	/* link prior to new */
  }
}

static void ct_Separator(char *cp)
{
  AddItem();
  item->header.name = "";		/* null = create no variables in form */
  item->type = I_SEPARATOR;
  item->header.size_y = 2;		/* 2 pixels high */
  AssignDrawTable(item);
  AddToLine(item);
  myfprintf((stderr, "ct_separator command set max width %d\n",(int)item->header.size_x));
}

static void form_add_text(char *value, bool value_owned)
{
  AddItem();
  bg_state = 'u';			/* indicate b/g color now used. */
  item->type = I_TEXT;
  /* Item now added to list of items, now it needs a pointer
     to the correct DrawTable. */
  AssignDrawTable(item);
  item->header.name = "";
  item->text.value = value;
  item->text.value_owned = value_owned;
  item->text.n = strlen(item->text.value);

  item->header.size_x = FlocaleTextWidth(item->header.dt_ptr->dt_Ffont,
				   item->text.value,
				   item->text.n) + 2 * TEXT_SPC;
  item->header.size_y = item->header.dt_ptr->dt_Ffont->height
     + CF.padVText;
  myfprintf((stderr, "Text \"%s\" [%d, %d]\n", item->text.value,
	     item->header.size_x, item->header.size_y));
  AddToLine(item);
}

static void ct_Text(char *cp)
{
  char *value;

  /* syntax: *FFText "<text>" */
  if (*cp == '\"')
    value = CopyQuotedString(++cp);
  else
    value = "";
  form_add_text(value, false);
}

static void ct_text_hex(char *cp)
{
  char *value = form_decode_hex_text(cp);

  if (value == NULL || !form_native_cleanup_register())
  {
    free(value);
    fprintf(stderr, "%s: invalid TextHex literal\n", module->name);
    return;
  }
  form_add_text(value, true);
}
/* Set the form's title.
   The default is the aliasname.
   If there is no quoted string, create a blank title. */
static void ct_Title(char *cp)
{
  /* syntax: *FFTitle "<text>" */
  if (*cp == '\"')
    CF.title = CopyQuotedString(++cp);
  else
    CF.title = "";
  myfprintf((stderr, "Title \"%s\"\n", CF.title));
}
static void ct_Timeout(char *cp)
{
  /* Prepare arguments before publishing an item or the one timer slot. */
  form_timeout_args args;
  char *command, *text;

  if (timer != NULL) {
    fprintf(stderr,"Only one timeout per form allowed, skipped %s.\n",cp);
    return;
  }
  if (!form_parse_timeout(cp, &args)) {
    fprintf(stderr,"Improper arguments specified for FvwmForm Timeout.\n");
    return;
  }
  if (args.quoted_command) {
    command = CopyQuotedString(args.command);
  } else {
    command = fxmalloc(args.command_length + 1);
    memcpy(command, args.command, args.command_length);
    command[args.command_length] = '\0';
  }
  text = args.quoted_text ? CopyQuotedString(args.text) : fxstrdup("");

  AddItem();
  bg_state = 'u';
  item->type = I_TIMEOUT;
  AssignDrawTable(item);
  item->header.name = "";
  item->timeout.timeleft = args.ticks;
  item->timeout.command = command;
  item->timeout.text = text;
  timer = item;
  item->timeout.len = strlen(item->timeout.text);

  item->header.size_x = FlocaleTextWidth(item->header.dt_ptr->dt_Ffont,
				   item->timeout.text,
				   item->timeout.len) + 2 * TEXT_SPC;
  item->header.size_y = item->header.dt_ptr->dt_Ffont->height
     + CF.padVText;
  myfprintf((stderr, "Timeout %d \"%s\" [%d, %d]\n", item->timeout.timeleft,
		item->timeout.text, item->header.size_x, item->header.size_y));
  AddToLine(item);
}
static void ct_padVText(char *cp)
{
  /* syntax: *FFText "<padVText pixels>" */
  CF.padVText = atoi(cp);
  myfprintf((stderr, "Text Vertical Padding %d\n", CF.padVText));
}
/* A supplied literal is owned here, including on geometry refusal. */
static void form_add_input(const form_input_args *args, char *literal)
{
  int size_x, size_y;
  Item prepared = {0};
  char *cp;

  prepared.type = I_INPUT;
  AssignDrawTable(&prepared);
  if (!form_input_geometry(args->width,
      prepared.header.dt_ptr->dt_Ffont->height,
      2 * TEXT_SPC + 2 * BOX_SPC, 3 * TEXT_SPC + 2 * BOX_SPC,
      &size_x, &size_y)) {
    fprintf(stderr, "%s: Input dimensions exceed the X11 range\n",
        module->name);
    free(literal);
    return;
  }
  AddItem();
  item->type = I_INPUT;
  item->header.dt_ptr = prepared.header.dt_ptr;
  item->header.name = CopySolidString((char *)args->name);
  item->input.width = args->width;
  cp = (char *)args->initial;
  item->input.init_value = literal != NULL ? literal : fxstrdup("");
  if (literal == NULL && *cp == '\"') {
    int num = -1, len;
    char *c;
    c = find_nth_UTF8_char(cp, NULL, &num, &len);
    c += len;  /* the end or the first invalid UTF-8 character */
    *c = '\0'; /* stop here */
    free(item->input.init_value);
    item->input.init_value = CopyQuotedString(++cp);
  }
  item->input.blanks = fxmalloc(item->input.width);
  memset(item->input.blanks, ' ', item->input.width);
  item->input.buf = strlen(item->input.init_value) + 1;
  item->input.value = fxmalloc(item->input.buf);
  item->input.value[0] = 0;		/* avoid reading unitialized data */
  item->input.size = 0;			/* value is empty */
  item->input.n = 0;			/* and contains 0 chars */

  item->header.size_x = size_x;
  item->header.size_y = size_y;
  myfprintf((stderr,"Input size_y is %d\n",item->header.size_y));

  if (CF.cur_input == 0) {		   /* first input field */
    item->input.next_input = item;	/* ring, next field is first field */
    item->input.prev_input = item;	/* ring, prev field is first field */
    CF.first_input = item;		   /* save loc of first item */
  } else {				/* not first field */
    CF.cur_input->input.next_input = item; /* old next ptr point to this item */
    item->input.prev_input = CF.cur_input; /* current items prev is old item */
    item->input.next_input = CF.first_input; /* next is first item */
    CF.first_input->input.prev_input = item; /* prev in first is this item */
  }
  CF.cur_input = item;			   /* new current input item */
  myfprintf((stderr, "Input, %s, [%d], \"%s\"\n", item->header.name,
	  item->input.width, item->input.init_value));
  AddToLine(item);
}

static void ct_Input(char *cp)
{
  form_input_args args;

  /* syntax: *FFInput <name> <size> "<init_value>" */
  if (!form_parse_input(cp, &args)) {
    fprintf(stderr, "%s: Input needs a positive representable width\n",
        module->name);
    return;
  }
  form_add_input(&args, NULL);
}

static void ct_input_hex(char *cp)
{
  form_input_args args;
  char *value;

  if (!form_parse_input(cp, &args) ||
      (value = form_decode_hex_text(args.initial)) == NULL)
  {
    fprintf(stderr, "%s: invalid InputHex name width literal\n", module->name);
    return;
  }
  form_add_input(&args, value);
}
static void ct_Read(char *cp)
{
  /* syntax: *FFRead 0 | 1 */
  myfprintf((stderr,"Got read command, char is %c\n",(int)*cp));
  endDefaultsRead = *cp;		/* copy whatever it is */
}
/* read and save vars from a file for later use in form
   painting.
*/
static void ct_UseData(char *cp)
{
  form_data_args args;
  char *filename, *prefix;

  /* syntax: *FFUseData filename cmd_prefix */
  if (!form_parse_data(cp, &args)) {
    fprintf(stderr, "%s: UseData requires a file and prefix\n", module->name);
    return;
  }
  filename = CopySolidString((char *)args.filename);
  prefix = CopySolidString((char *)args.prefix);
  free(CF.file_to_read);
  free(CF.leading);
  CF.file_to_read = filename;
  CF.leading = prefix;
  /* Cant do the actual reading of the data file here,
     we are already in a readconfig loop. */
}
static void ReadFormData(void)
{
  int leading_len;
  char *line_buf;			/* ptr to curr config line */
  char *cmd_buffer;
  xasprintf(&cmd_buffer, "read %s Quiet", CF.file_to_read);
  SendText(Channel,cmd_buffer,0);	/* read data */
  free(cmd_buffer);
  leading_len = strlen(CF.leading);
  InitGetConfigLine(Channel, CF.leading); /* ask for certain lines */
  while (GetConfigLine(Channel,&line_buf),line_buf) { /* while there is some */
    if (form_scale_message(line_buf))
      continue;
    if (strncasecmp(line_buf, CF.leading, leading_len) == 0) { /* leading = */
      form_chomp_line(line_buf);
      PutDataInForm(line_buf+leading_len); /* pass arg, space, value */
    } /* end match on arg 2, "leading" */
  } /* end while there is config data */
  form_scale_end_config();
  free(CF.file_to_read);		/* dont need it anymore */
  free(CF.leading);
  CF.file_to_read = 0;
  CF.leading = 0;
}
/*
  Input is a line with varname space value.
  Search form for matching input fields and set values.
  If you don't get a match on an input field, try a choice.
*/
static void PutDataInForm(char *cp)
{
  char *var_name;
  char *var_value;
  int var_len, i;
  Item *item;
  Line *line;

  var_name = CopySolidString(cp);	/* var */
  if (*var_name == 0) {
    free(var_name);
    return;
  }
  cp += strlen(var_name);
  while (isspace((unsigned char)*cp)) cp++;
  var_value = cp;
  if (CF.cur_input != 0) {
    item = CF.cur_input;
    do {
      if (strcasecmp(var_name,item->header.name) == 0) {
	int num = -1, len;
	char *c;
	c = find_nth_UTF8_char(cp, NULL, &num, &len);
	c += len;
	var_len = (int)(c - cp);
	free(item->input.init_value);
	item->input.init_value = fxmalloc(var_len + 1);
	strlcpy(item->input.init_value, cp, var_len + 1); /* new initial value */
	free(item->input.value);
	item->input.n = num+1;
	item->input.buf = var_len+1;
	item->input.size = var_len;
	item->input.value = fxstrdup(item->input.init_value); /* new value */
	free(var_name);			/* goto's have their uses */
	return;
      }
      item=item->input.next_input;	  /* next input field */
    } while (item != CF.cur_input);	  /* while not end of ring */
  }
  /* You have a matching line, but it doesn't match an input
     field.  What to do?  I know, try a choice. */
  line = &root_line;			 /* start at first line */
  do {					/* for all lines */
    for (i = 0; i < line->n; i++) {	/* all items on line */
      item = line->items[i];
      if (item->type == I_CHOICE) {	/* choice is good */
	if (strcasecmp(var_name,item->header.name) == 0) { /* match */
	  item->choice.init_on = 0;
	  if (strncasecmp(cp, "on", 2) == 0) {
	    item->choice.init_on = 1;	/* set default state */
	    free(var_name);		/* goto's have their uses */
	    return;
	  }
	}
      }
    } /* end all items in line */
    line = line->next;			/* go to next line */
  } while (line != &root_line);		/* do all lines */
  /* You have a matching line, it didn't match an input field,
     and it didn't match a choice.  I've got it, it may match a
     selection, in which case we should use the value to
     set the choices! */
  for (item = root_item_ptr; item != 0;
       item = item->header.next) {/* all items */
    if (item->type == I_SELECT) {     /* selection is good */
      if (strcasecmp(var_name,item->header.name) == 0) { /* match */
	/* Now find the choices for this selection... */
	Item *choice_ptr;
	for (i = 0;
	     i<item->selection.n;
	     i++) {
	  choice_ptr=item->selection.choices[i];
	  /* if the choice value matches the selection */
	  if (strcmp(choice_ptr->choice.value,var_value) == 0) {
	    choice_ptr->choice.init_on = 1;
	  } else {
	    choice_ptr->choice.init_on = 0;
	  }
	} /* end all choices */
      } /* end match */
    } /* end selection */
  } /* end all items */
  free(var_name);			/* not needed now */
}
static void form_list_abort(Item *p)
{
	if (p->list.update != NULL)
	{
		for (int i = 0; i < p->list.update->count; i++)
			free(p->list.pending[i].label);
		free(p->list.pending);
		free(p->list.update);
		p->list.pending = NULL;
		p->list.update = NULL;
		p->list.pending_capacity = 0;
	}
}

static void form_native_cleanup(void)
{
  Item *p;

  free(CF.close_command);
  CF.close_command = NULL;
  for (p = root_item_ptr; p != NULL; p = p->header.next)
  {
    if (p->type == I_TEXT)
    {
      if (p->text.fixed || p->text.value_owned)
        free(p->text.value);
      if (p->text.fixed)
        free(p->header.name);
    }
    if (p->type != I_LIST)
      continue;
    for (int i = 0; i < p->list.state->count; i++)
      free(p->list.entries[i].label);
    form_list_abort(p);
    free(p->list.entries);
    free(p->list.state);
    free(p->list.initial);
    free(p->header.name);
  }
}

static bool form_native_cleanup_register(void)
{
	static bool registered;

	if (!registered && atexit(form_native_cleanup) != 0)
		return false;
	registered = true;
	return true;
}

/* A malformed definition cannot leave the preceding list as append target. */
static void ct_List(char *cp)
{
  form_input_args args;
  Item prepared;
  int width, height, rows;
  const char *tail;

  form_current_list = NULL;
  if (!form_native_cleanup_register())
    goto invalid;
  if (!form_parse_input(cp, &args) ||
      !form_list_number(form_next_argument(cp), 4096, &args.width))
    goto invalid;
  tail = form_next_argument(form_next_argument(cp));
  if (!form_list_number(tail, 32, &rows) ||
      *form_next_argument(tail) != '\0')
    goto invalid;
  memset(&prepared, 0, sizeof(prepared));
  prepared.type = I_LIST;
  AssignDrawTable(&prepared);
  if (!form_input_geometry(args.width,
      prepared.header.dt_ptr->dt_Ffont->height,
      2 * (BOX_SPC + TEXT_SPC) + 20, 2 * TEXT_SPC, &width, &height) ||
      height > (32760 - 2 * BOX_SPC) / rows || width > 32760)
    goto invalid;
  AddItem();
  item->type = I_LIST;
  item->header.dt_ptr = prepared.header.dt_ptr;
  item->header.name = CopySolidString(cp);
  item->header.size_x = width;
  item->list.row_height = height;
  item->header.size_y = rows * item->list.row_height + 2 * BOX_SPC;
  item->list.state = fxcalloc(1, sizeof(*item->list.state));
  item->list.initial = fxcalloc(1, sizeof(*item->list.initial));
  item->list.state->rows = rows;
  item->list.state->columns = 1;
  *item->list.initial = *item->list.state;
  item->list.line = cur_line;
  form_current_list = item;
  AddToLine(item);
  return;
invalid:
  fprintf(stderr, "%s: invalid List name width rows (1..32)\n", module->name);
}

/* Opt-in keeps existing Base64 collection callbacks byte-for-byte usable. */
static void ct_list_versioned(char *cp)
{
	if (form_current_list == NULL || *cp != '\0' ||
	    form_current_list->list.state->count != 0)
	{
		fprintf(stderr, "%s: ListVersioned must precede entries\n",
			module->name);
		return;
	}
	form_current_list->list.state->generation = 1;
	form_current_list->list.update_serial = 1;
	*form_current_list->list.initial = *form_current_list->list.state;
}

/* Layout is immutable once entries have been admitted. */
static void ct_ListGrid(char *cp)
{
  Item *list = form_current_list;
  int columns, height;

  if (list == NULL || list->list.line != cur_line ||
      list->list.state->count != 0 || list->list.viewport_columns != 0 ||
      !form_list_number(cp, 8, &columns) || columns < 2 ||
      *form_next_argument(cp) != '\0' ||
      (list->header.size_x - 20) / columns < 64)
    goto invalid;
  height = 3 * list->header.dt_ptr->dt_Ffont->height + 4 * TEXT_SPC;
  if (height > (32760 - 2 * BOX_SPC) / list->list.state->rows)
    goto invalid;
  list->list.state->columns = columns;
  list->list.row_height = height;
  list->header.size_y = height * list->list.state->rows + 2 * BOX_SPC;
  *list->list.initial = *list->list.state;
  cur_line->size_y = 0;
  for (int i = 0; i < cur_line->n; i++)
    if (cur_line->items[i]->header.size_y > cur_line->size_y)
      cur_line->size_y = cur_line->items[i]->header.size_y;
  return;
invalid:
  if (list != NULL)
    list->list.state->refused = list->list.initial->refused = true;
  fprintf(stderr, "%s: invalid ListGrid columns (2..8)\n", module->name);
}

/* One optional expansion point, rather than a second general Form layout. */
static void ct_list_viewport(char *cp)
{
	Item *list = form_current_list;
	int units, columns, width, height;
	const char *tail = form_next_argument(cp);

	if (list == NULL || CF.viewport != NULL || list->list.line != cur_line ||
	    cur_line->n != 1 || list->list.state->count != 0 ||
	    list->list.state->generation != 0 ||
	    form_list_columns(list->list.state) != 1 ||
	    !form_list_number(cp, 4096, &units) ||
	    !form_list_number(tail, 8, &columns) ||
	    *form_next_argument(tail) != '\0' ||
	    !form_input_geometry(units, list->header.dt_ptr->dt_Ffont->height,
		0, 0, &width, &height) || width > 32700)
		goto invalid;
	if (columns > 1)
		height = 3 * list->header.dt_ptr->dt_Ffont->height + 4 * TEXT_SPC;
	else
		height = list->list.row_height;
	if (height > (32760 - 2 * BOX_SPC) / list->list.state->rows)
		goto invalid;
	list->list.viewport_cell = width;
	list->list.viewport_units = units;
	list->list.viewport_columns = columns;
	list->list.row_height = height;
	list->header.size_y = height * list->list.state->rows + 2 * BOX_SPC;
	cur_line->size_y = list->header.size_y;
	CF.viewport = list;
	return;
invalid:
	if (list != NULL)
		list->list.state->refused = list->list.initial->refused = true;
	fprintf(stderr, "%s: invalid ListViewport cell-units columns (1..8)\n",
		module->name);
}

static void ct_ListFocus(char *cp)
{
  int focus;

  if (form_current_list == NULL || !form_list_number(cp, FORM_LIST_MAX, &focus) ||
      focus > form_current_list->list.state->count ||
      *form_next_argument(cp) != '\0')
  {
    fprintf(stderr, "%s: invalid ListFocus (one-based row)\n", module->name);
    return;
  }
  form_list_focus(form_current_list->list.state, focus - 1);
  *form_current_list->list.initial = *form_current_list->list.state;
}

static void ct_ListItem(char *cp)
{
  Item *list = form_current_list;
  const char *label = form_next_argument(cp);
  const char *end = label;
  const char *tail;
  enum form_list_icon icon = FORM_ICON_NONE;
  bool valid_icon;
  bool on = strncmp(cp, "on", 2) == 0 && isspace((unsigned char)cp[2]);

  if (*end == '"')
  {
    for (end++; *end != '\0' && *end != '"'; end++)
      if (*end == '\\' && end[1] != '\0')
        end++;
  }

  tail = *end == '"' ? end + 1 : end;
  while (isspace((unsigned char)*tail)) tail++;
  valid_icon = form_list_parse_icon(tail, &icon);
  if (list == NULL || (*label != '"') ||
      (!on && !(strncmp(cp, "off", 3) == 0 &&
        isspace((unsigned char)cp[3]))) || *end != '"' ||
      !valid_icon ||
      strlen(label) > 1024 || !form_list_add(list->list.state, on))
  {
    if (list != NULL)
    {
      list->list.state->refused = true;
      list->list.initial->refused = true;
    }
    fprintf(stderr, "%s: invalid/over-limit ListItem\n", module->name);
    return;
  }
  list->list.entries = form_reserve_array(list->list.entries,
    &list->list.capacity, list->list.state->count - 1, 1, 32,
    sizeof(*list->list.entries));
  list->list.entries[list->list.state->count - 1] =
    (form_list_entry){CopyQuotedString(label + 1), icon};
  if (on && list->list.state->selected == 1)
    form_list_focus(list->list.state, list->list.state->count - 1);
  *list->list.initial = *list->list.state;
}

static void ct_Selection(char *cp)
{
  /* syntax: *FFSelection <name> single | multiple */
  AddItem();
  cur_sel = item;			/* save ptr as cur_sel */
  cur_sel->type = I_SELECT;
  cur_sel->header.name = CopySolidString(cp);
  cp += strlen(cur_sel->header.name);
  while (isspace((unsigned char)*cp)) cp++;
  if (strncasecmp(cp, "multiple", 8) == 0)
    cur_sel->selection.key = IS_MULTIPLE;
  else
    cur_sel->selection.key = IS_SINGLE;
}
static void ct_Choice(char *cp)
{
  /* syntax: *FFChoice <name> <value> [on | _off_] ["<text>"] */
  /* This next edit is a liitle weak, the selection should be right
     before the choice. At least a core dump is avoided. */
  if (cur_sel == 0) {			/* need selection for a choice */
    fprintf(stderr,"%s: Need selection for choice %s\n",
	    module->name, cp);
    return;
  }
  bg_state = 'u';			/* indicate b/g color now used. */
  AddItem();
  item->type = I_CHOICE;
  AssignDrawTable(item);
  item->header.name = CopySolidString(cp);
  cp += strlen(item->header.name);
  while (isspace((unsigned char)*cp)) cp++;
  item->choice.value = CopySolidString(cp);
  cp += strlen(item->choice.value);
  while (isspace((unsigned char)*cp)) cp++;
  if (strncasecmp(cp, "on", 2) == 0)
    item->choice.init_on = 1;
  else
    item->choice.init_on = 0;
  cp = (char *)form_next_argument(cp);
  if (*cp == '\"')
    item->choice.text = CopyQuotedString(++cp);
  else
    item->choice.text = "";
  item->choice.n = strlen(item->choice.text);
  item->choice.sel = cur_sel;

  cur_sel->selection.choices = form_reserve_array(cur_sel->selection.choices,
      &cur_sel->selection.choices_array_count, cur_sel->selection.n, 1,
      CHOICES_PER_SEL_EXPANSION, sizeof(*cur_sel->selection.choices));

  cur_sel->selection.choices[cur_sel->selection.n++] = item;
  item->header.size_y = item->header.dt_ptr->dt_Ffont->height
     + 2 * TEXT_SPC;
  /* this is weird, the x dimension is the sum of the height and width? */
  item->header.size_x = item->header.dt_ptr->dt_Ffont->height
     + 4 * TEXT_SPC +
     FlocaleTextWidth(item->header.dt_ptr->dt_Ffont,
       item->choice.text, item->choice.n);
  myfprintf((stderr, "Choice %s, \"%s\", [%d, %d]\n", item->header.name,
	  item->choice.text, item->header.size_x, item->header.size_y));
  AddToLine(item);
}
static void ct_Button(char *cp)
{
  form_button_args args;
  int alt_key = -1;

  /* syntax: *FFButton continue | restart | quit "<text>" */
  if (!form_parse_button(cp, &args)) {
    fprintf(stderr, "%s: incomplete Button arguments\n", module->name);
    return;
  }
  if (strncasecmp(args.key, "Alt-", 4) == 0 &&
      !form_button_alt_key(args.key, &alt_key)) {
    fprintf(stderr, "%s: invalid Button Alt shortcut\n", module->name);
    return;
  }
  cp = (char *)args.mode;
  AddItem();
  item->type = I_BUTTON;
  item->button.command_refused = 0;
  AssignDrawTable(item);
  item->header.name = "";
  if (strncasecmp(cp, "restart", 7) == 0)
    item->button.key = IB_RESTART;
  else if (strncasecmp(cp, "quit", 4) == 0)
    item->button.key = IB_QUIT;
  else
    item->button.key = IB_CONTINUE;
  if (args.quoted_text)
    item->button.text = CopyQuotedString((char *)args.text);
  else
    item->button.text = "";
  cp = (char *)args.key;
  if (alt_key >= 0)
    item->button.keypress = alt_key;
  else if (*cp == '^')
    item->button.keypress = *(++cp) - '@';
  else if (*cp == 'F')
    item->button.keypress = 256 + atoi(++cp);
  else
    item->button.keypress = -1;
  item->button.len = strlen(item->button.text);
  item->header.size_y = item->header.dt_ptr->dt_Ffont->height
     + 2 * TEXT_SPC + 2 * BOX_SPC;
  item->header.size_x = 2 * TEXT_SPC + 2 * BOX_SPC
  + FlocaleTextWidth(item->header.dt_ptr->dt_Ffont, item->button.text,
    item->button.len);
  AddToLine(item);
  cur_button = item;
  myfprintf((stderr,"Created button, fore %s, bg %s, text %s\n",
	     item->header.dt_ptr->dt_color_names[c_item_fg],
	     item->header.dt_ptr->dt_color_names[c_item_bg],
	     item->button.text));
}
static void ct_Command(char *cp)
{
  /* syntax: *FFCommand <command> */
  cur_button->button.commands = form_reserve_array(cur_button->button.commands,
      &cur_button->button.button_array_size, cur_button->button.n, 1,
      BUTTON_COMMAND_EXPANSION, sizeof(*cur_button->button.commands));
  cur_button->button.commands[cur_button->button.n++] = fxstrdup(cp);
}

static void ct_close_command(char *cp)
{
  if (!form_native_cleanup_register())
  {
    fvwm_debug(__func__, "Unable to register Form close-command cleanup");
    return;
  }
  if (!form_set_close_command(&CF.close_command, cp))
    fvwm_debug(__func__, "Refusing empty or oversized Form CloseCommand");
}

static void ct_CommandAppend(char *cp)
{
  if (cur_button == NULL || cur_button->button.n == 0)
  {
    fvwm_debug(__func__, "CommandAppend requires a preceding button command");
    return;
  }
  char *tail = fxmalloc(strlen(cp) + 2);
  tail[0] = ' ';
  strcpy(tail + 1, cp);
  if (!form_append_text(&cur_button->button.commands[cur_button->button.n - 1],
      tail, 1024U * 1024U))
  {
    cur_button->button.command_refused = 1;
    fvwm_debug(__func__, "Refusing excessive appended Form command");
  }
  free(tail);
}

static void ct_InputAppend(char *cp)
{
  if (item == NULL || item->type != I_INPUT || *cp != '"')
  {
    fvwm_debug(__func__, "InputAppend requires a preceding Input and quoted text");
    return;
  }
  char *tail = CopyQuotedString(cp + 1);
  if (!form_append_text(&item->input.init_value, tail, 65536))
    fvwm_debug(__func__, "Refusing excessive appended Form input");
  free(tail);
}

/* End of ct_ routines */

/* Init constants with values that can be freed later. */
static void InitConstants(void) {
  color_names[0]=fxstrdup("Light Gray");
  color_names[1]=fxstrdup("Black");
  color_names[2]=fxstrdup("Gray50");
  color_names[3]=fxstrdup("Wheat");
  font_names[0]=fxstrdup("8x13bold");
  font_names[1]=fxstrdup("8x13bold");
  font_names[2]=fxstrdup("8x13bold");
  font_names[3]=fxstrdup("8x13bold");
  screen_background_color=fxstrdup("Light Gray");
  CF.p_c[input_fore].pointer_color.pixel = PictureWhitePixel();
  CF.p_c[input_back].pointer_color.pixel = PictureBlackPixel();
  CF.p_c[button_fore].pointer_color.pixel = PictureBlackPixel();
  CF.p_c[button_back].pointer_color.pixel = PictureWhitePixel();
  /* The in pointer is the reverse of the hover pointer. */
  CF.p_c[button_in_fore].pointer_color.pixel = PictureWhitePixel();
  CF.p_c[button_in_back].pointer_color.pixel = PictureBlackPixel();
}

/* read the configuration file */
static void ReadDefaults(void)
{
  char *line_buf;			/* ptr to curr config line */

  /* default button is for initial functions */
  cur_button = &CF.def_button;
  CF.def_button.button.key = IB_CONTINUE;

   /*
    * Reading .FvwmFormDefault for every form seems slow.
    *
    * This next bit puts a command at the end of the module command
    * queue in fvwm that indicates whether the file has to be read.
    *
    * Read defaults looking for "*FvwmFormDefaultRead"
    * if not there, send read,
    * then "*FvwmFormDefaultRead y",
    * then look thru defaults again.
    * The customization dialog sends "*FvwmFormDefaultRead n"
    * to start over.
    */
  InitGetConfigLine(Channel,"*FvwmFormDefault");
  while (GetConfigLine(Channel,&line_buf),line_buf) { /* get config from fvwm */
    ParseDefaults(line_buf);		 /* process default config lines 1st */
  }
  form_scale_end_config();
  if (endDefaultsRead == 'y') {		/* defaults read already */
    myfprintf((stderr,"Defaults read, no need to read file.\n"));
    return;
  } /* end defaults read already */
  SendText(Channel,"read .FvwmForm Quiet",0); /* read default config */
  SendText(Channel,"*FvwmFormDefaultRead y",0); /* remember you read it */

  InitGetConfigLine(Channel,"*FvwmFormDefault");
  while (GetConfigLine(Channel,&line_buf),line_buf) { /* get config from fvwm */
    ParseDefaults(line_buf);		 /* process default config lines 1st */
  }
  form_scale_end_config();
} /* done */

static void ReadConfig(void)
{
  char *line_buf;			/* ptr to curr config line */

  InitGetConfigLine(Channel, mname);
  while (GetConfigLine(Channel,&line_buf),line_buf) { /* get config from fvwm */
    ParseConfigLine(line_buf);		/* process config lines */
  }
  form_scale_end_config();
} /* done */

/* After this config is read, figure it out */
static void MassageConfig(void)
{
  int i, extra;
  Line *line;				/* for scanning form lines */

  if (CF.file_to_read) {		/* if theres a data file to read */
    ReadFormData();			/* go read it */
  }
  /* get the geometry right */
  CF.max_width = 0;
  CF.total_height = ITEM_VSPC;
  line = &root_line;			 /* start at first line */
  do {					/* for all lines */
    for (i = 0; i < line->n; i++) {
      line->items[i]->header.pos_y = CF.total_height;
      if (line->items[i]->header.size_y < line->size_y)
	line->items[i]->header.pos_y +=
	  (line->size_y - line->items[i]->header.size_y) / 2 + 1 ;
    } /* end all items in line */
    CF.total_height += ITEM_VSPC + line->size_y;
    line->size_x += (line->n + 1) * ITEM_HSPC;
    if (line->size_x > CF.max_width)
      CF.max_width = line->size_x;
    line = line->next;			/* go to next line */
  } while (line != &root_line);		/* do all lines */
  do {					/* note, already back at root_line */
    int width;
    switch (line->justify) {
    case L_LEFT:
      width = ITEM_HSPC;
      for (i = 0; i < line->n; i++) {
	line->items[i]->header.pos_x = width;
	width += ITEM_HSPC + line->items[i]->header.size_x;
      }
      break;
    case L_RIGHT:
      width = CF.max_width - line->size_x + ITEM_HSPC;
      for (i = 0; i < line->n; i++) {
	line->items[i]->header.pos_x = width;
	width += ITEM_HSPC + line->items[i]->header.size_x;
      }
      break;
    case L_CENTER:
      width = (CF.max_width - line->size_x) / 2 + ITEM_HSPC;
      for (i = 0; i < line->n; i++) {
	line->items[i]->header.pos_x = width;
	myfprintf((stderr, "Line Item[%d] @ (%d, %d)\n", i,
	       line->items[i]->header.pos_x, line->items[i]->header.pos_y));
	width += ITEM_HSPC + line->items[i]->header.size_x;
      }
      break;
    case L_LEFTRIGHT:
    /* count the number of inputs on the line - the extra space will be
     * shared amongst these if there are any, otherwise it will be added
     * as space in between the elements
     */
      extra = 0 ;
      for (i = 0 ; i < line->n ; i++) {
	if (line->items[i]->type == I_INPUT)
	  extra++ ;
      }
      if (extra == 0) {
	if (line->n < 2) {  /* same as L_CENTER */
	  width = (CF.max_width - line->size_x) / 2 + ITEM_HSPC;
	  for (i = 0; i < line->n; i++) {
	    line->items[i]->header.pos_x = width;
	    width += ITEM_HSPC + line->items[i]->header.size_x;
	  }
	} else {
	  extra = (CF.max_width - line->size_x) / (line->n - 1);
	  width = ITEM_HSPC;
	  for (i = 0; i < line->n; i++) {
	    line->items[i]->header.pos_x = width;
	    width += ITEM_HSPC + line->items[i]->header.size_x + extra;
	  }
	}
      } else {
	extra = (CF.max_width - line->size_x) / extra ;
	width = ITEM_HSPC ;
	for (i = 0 ; i < line->n ; i++) {
	  line->items[i]->header.pos_x = width ;
	  if (line->items[i]->type == I_INPUT)
	    line->items[i]->header.size_x += extra ;
	  width += ITEM_HSPC + line->items[i]->header.size_x ;
	}
      }
      break;
    }
    line = line->next;			/* go to next line */
  } while (line != &root_line);		 /* do all lines */
}

/* Resize and explicit live layout share preparation and child positioning.
 * No state or X request changes until all candidate geometry is validated. */
static bool form_viewport_reflow(int width, int height, int cell,
	int row_height, int columns, int expected, int generation)
{
	Item *list = CF.viewport;
	Line *line = &root_line;
	ui_layout_grid_result grid;
	form_list_state state, initial;
	int y = ITEM_VSPC, content_width, list_height, minimum, min_height;

	if (list == NULL || width < 1 || height < 1 ||
	    width > 32767 || height > 32767 || cell < 1 || cell > 32700 ||
	    row_height < 1 || row_height > 32767 ||
	    columns < 1 || columns > 8)
		return false;
	minimum = cell + 2 * ITEM_HSPC + 2 * BOX_SPC + LIST_GUTTER;
	do
	{
		int fixed = (line->n + 1) * ITEM_HSPC;

		if (line != list->list.line)
		{
			for (int i = 0; i < line->n; i++)
				fixed += line->items[i]->type == I_TEXT &&
					line->items[i]->text.viewport ?
					line->items[i]->text.minimum_width :
					line->items[i]->header.size_x;
			if (fixed > minimum)
				minimum = fixed;
		}
		line = line->next;
	} while (line != &root_line);
	min_height = CF.viewport_fixed_height + row_height + 2 * BOX_SPC;
	if (minimum > 32767 || min_height > 32767)
		return false;
	/* Size hints cannot synchronously enlarge the actual parent. A live
	 * publication must fit it before acknowledging the new generation. */
	if (generation != 0 && (width < minimum || height < min_height))
		return false;
	if (width < minimum)
		width = minimum;
	if (height < min_height)
		height = min_height;
	for (Item *p = root_item_ptr; p != NULL; p = p->header.next)
	{
		int span;

		if (p->type == I_TEXT && p->text.viewport &&
		    !form_label_viewport_width(p->text.minimum_width,
			width, ITEM_HSPC, &span))
			return false;
	}
	content_width = width - 2 * ITEM_HSPC - 2 * BOX_SPC - LIST_GUTTER;
	list_height = height - CF.viewport_fixed_height;
	if (!ui_layout_grid(content_width, list_height - 2 * BOX_SPC,
		cell, row_height, 1, columns, 32, &grid))
		return false;
	state = *list->list.state;
	initial = *list->list.initial;
	if (generation != 0)
	{
		if (!form_list_layout(&state, expected, generation,
			grid.rows, grid.columns))
			return false;
		initial = state;
	}
	else if (!form_list_viewport(&state, grid.rows, grid.columns) ||
	    !form_list_viewport(&initial, grid.rows, grid.columns))
		return false;
	/* Everything below publishes one admitted layout in this event turn. */
	*list->list.state = state;
	*list->list.initial = initial;
	list->list.viewport_cell = cell;
	list->list.viewport_columns = columns;
	list->list.row_height = row_height;
	CF.viewport_min_width = minimum;
	CF.viewport_min_height = min_height;
	if (generation != 0)
		list->list.update_serial = generation;
	list->header.size_x = width - 2 * ITEM_HSPC;
	list->header.size_y = list_height;
	list->list.line->size_y = list_height;
	do
	{
		int line_width = (line->n + 1) * ITEM_HSPC;
		int x = ITEM_HSPC, gap = ITEM_HSPC;

		for (int i = 0; i < line->n; i++)
		{
			Item *p = line->items[i];

			/* All opted spans were admitted before publishing the view. */
			if (p->type == I_TEXT && p->text.viewport)
				p->header.size_x = width - 2 * ITEM_HSPC;
			line_width += p->header.size_x;
		}
		if (line != list->list.line)
		{
			if (line->justify == L_RIGHT)
				x += width - line_width;
			else if (line->justify == L_CENTER ||
			    (line->justify == L_LEFTRIGHT && line->n == 1))
				x += (width - line_width) / 2;
			else if (line->justify == L_LEFTRIGHT && line->n > 1)
				gap += (width - line_width) / (line->n - 1);
		}
		for (int i = 0; i < line->n; i++)
		{
			Item *p = line->items[i];

			p->header.pos_x = x;
			p->header.pos_y = y;
			if (p->header.size_y < line->size_y)
				p->header.pos_y +=
					(line->size_y - p->header.size_y) / 2 + 1;
			x += p->header.size_x + gap;
			if (p->header.win != None)
			{
				if (p == list || CF.ui_scale_resize_all)
					XMoveResizeWindow(dpy, p->header.win,
						p->header.pos_x, p->header.pos_y,
						p->header.size_x, p->header.size_y);
				else
					XMoveWindow(dpy, p->header.win,
						p->header.pos_x, p->header.pos_y);
			}
		}
		y += ITEM_VSPC + line->size_y;
		line = line->next;
	} while (line != &root_line);
	/* Core-font text is painted on the parent, so its old positions need
	 * clearing too; child moves alone would leave stale captions behind. */
	if (CF.frame != None)
		XClearWindow(dpy, CF.frame);
	return true;
}

void form_viewport_resize(int width, int height)
{
	Item *list = CF.viewport;

	if (list != NULL)
		form_viewport_reflow(width, height, list->list.viewport_cell,
			list->list.row_height,
			list->list.viewport_columns, 0, 0);
}

void form_viewport_set_minimum(int width, int height)
{
	form_size_hints.flags |= PMinSize;
	form_size_hints.min_width = width;
	form_size_hints.min_height = height;
	XSetWMNormalHints(dpy, CF.frame, &form_size_hints);
}

static void form_viewport_prepare(void)
{
	Item *list = CF.viewport;
	Line *line = &root_line;
	int minimum;

	if (list == NULL)
		return;
	if (list->list.line->n != 1)
	{
		fprintf(stderr, "%s: ListViewport needs a dedicated Line\n",
			module->name);
		list->list.state->refused = list->list.initial->refused = true;
		CF.viewport = NULL;
		return;
	}
	minimum = list->list.viewport_cell + 2 * ITEM_HSPC +
		2 * BOX_SPC + LIST_GUTTER;
	do
	{
		int fixed = (line->n + 1) * ITEM_HSPC;

		if (line != list->list.line)
		{
			for (int i = 0; i < line->n; i++)
				fixed += line->items[i]->type == I_TEXT &&
					line->items[i]->text.viewport ?
					line->items[i]->text.minimum_width :
					line->items[i]->header.size_x;
			if (fixed > minimum)
				minimum = fixed;
		}
		line = line->next;
	} while (line != &root_line);
	CF.viewport_fixed_height = CF.total_height - list->header.size_y;
	CF.viewport_min_width = minimum;
	CF.viewport_min_height = CF.viewport_fixed_height +
		list->list.row_height + 2 * BOX_SPC;
	if (minimum > 32767 || CF.viewport_min_height > 32767)
	{
		fprintf(stderr, "%s: ListViewport fixed controls exceed geometry\n",
			module->name);
		list->list.state->refused = list->list.initial->refused = true;
		CF.viewport = NULL;
		return;
	}
	if (CF.max_width < minimum)
		CF.max_width = minimum;
	form_viewport_resize(CF.max_width, CF.total_height);
}

/* Keep byte storage and the legacy UTF-8 character count distinct. */
Bool form_set_input_value(Item *item, const char *value)
{
  size_t bytes = strlen(value);
  int count = -1;

  if (bytes >= INT_MAX) {
    fvwm_debug(__func__, "Input value is not representable");
    return False;
  }
  find_nth_UTF8_char((char *)value, NULL, &count, NULL);
  if (item->input.value == NULL || item->input.buf <= (int)bytes) {
    char *copy = fxstrdup(value);
    free(item->input.value);
    item->input.value = copy;
    item->input.buf = (int)bytes + 1;
  } else {
    memmove(item->input.value, value, bytes + 1);
  }
  item->input.size = (int)bytes;
  item->input.n = count + 1;
  item->input.left = 0;
  return True;
}

/* reset all the values (also done on first display) */
static void Restart(void)
{
  Item *item;

  CF.cur_input = NULL;
  CF.cur_list = NULL;
  CF.abs_cursor = CF.rel_cursor = 0;
  for (item = root_item_ptr; item != 0;
       item = item->header.next) {/* all items */
    switch (item->type) {
    case I_LIST:
      *item->list.state = *item->list.initial;
      if (!CF.cur_input && !CF.cur_list)
        CF.cur_list = item;
      break;
    case I_INPUT:
      if (!CF.cur_input && !CF.cur_list)
	CF.cur_input = item;

      /* save old input values in a recall ring. */
      if (item->input.value && item->input.value[0] != 0) { /* ? to save */
	if (item->input.value_history_ptr == 0) {  /* no history yet */
	  item->input.value_history_ptr =
	    fxcalloc(sizeof(char *), 50);
	  item->input.value_history_ptr[0] = fxstrdup(item->input.value);
	  item->input.value_history_count = 1; /* next insertion point */
	  myfprintf((stderr,"Initial save of %s in slot 0\n",
		     item->input.value_history_ptr[0]));
	} else {			/* we have a history */
	  int prior;
	  Bool have_prior = form_history_position(
	    item->input.value_history_ptr, VH_SIZE,
	    item->input.value_history_count, -1, &prior);
	  myfprintf((stderr,"Prior is %d, compare %s to %s\n",
		     prior, item->input.value,
		     have_prior ? item->input.value_history_ptr[prior] : ""));

	  if (!have_prior ||
	      strcmp(item->input.value, item->input.value_history_ptr[prior])
	       != 0) {			/* different value */
	    if (item->input.value_history_ptr[item->input.value_history_count])
	    {
	      free(item->input.value_history_ptr[
			   item->input.value_history_count]);
	      myfprintf((stderr,"Freeing old item in slot %d\n",
			 item->input.value_history_count));
	    }
	    item->input.value_history_ptr[item->input.value_history_count] =
	      fxstrdup(item->input.value); /* save value ptr in array */
	    myfprintf((stderr,"Save of %s in slot %d\n",
		       item->input.value,
		       item->input.value_history_count));

	    /* leave count pointing at the next insertion point. */
	    if (item->input.value_history_count < VH_SIZE - 1) { /* not full */
	      ++item->input.value_history_count;    /* next slot */
	    } else {
	      item->input.value_history_count = 0;  /* wrap around */
	    }
	  } /* end something different */
	} /* end have a history */
	myfprintf((stderr,"New history yankat %d\n",
		   item->input.value_history_yankat));
      } /* end something to save */
      item->input.value_history_yankat = item->input.value_history_count;
      form_set_input_value(item, item->input.init_value);
      break;
    case I_CHOICE:
      item->choice.on = item->choice.init_on;
      break;
    }
  }
}

/* redraw the frame */
void RedrawFrame(XEvent *pev)
{
	Item *item;
	Region region = None;

	Bool clear = False;
	if (FftSupport)
	{
		item = root_item_ptr;
		while(item != 0 && !clear)
		{
			if ((item->type == I_TEXT || item->type == I_CHOICE))
				clear = True;
			item = item->header.next;
		}
		if (clear && pev)
		{
			XClearArea(
				dpy, CF.frame,
				pev->xexpose.x, pev->xexpose.y,
				pev->xexpose.width, pev->xexpose.height,
				False);
		}
		else
		{
			XClearWindow(dpy, CF.frame);
		}
	}
	if (pev)
	{
		XRectangle r;

		r.x = pev->xexpose.x;
		r.y =  pev->xexpose.y;
		r.width = pev->xexpose.width;
		r.height = pev->xexpose.height;
		region = XCreateRegion();
		XUnionRectWithRegion (&r, region, region);
	}
	for (item = root_item_ptr; item != 0;
	     item = item->header.next) {      /* all items */
		DrawTable *dt_ptr = item->header.dt_ptr;

		if (dt_ptr && dt_ptr->dt_Fstr)
		{
			if (region)
			{
				dt_ptr->dt_Fstr->flags.has_clip_region = True;
				dt_ptr->dt_Fstr->clip_region = region;
			}
			else
			{
				dt_ptr->dt_Fstr->flags.has_clip_region = False;
			}
		}
		switch (item->type) {
		case I_TEXT:
			RedrawText(item);
			break;
		case I_TIMEOUT:
			RedrawTimeout(item);
			break;
		case I_CHOICE:
			dt_ptr->dt_Fstr->win = CF.frame;
			dt_ptr->dt_Fstr->gc  = dt_ptr->dt_GC;
			dt_ptr->dt_Fstr->str = item->choice.text;
			dt_ptr->dt_Fstr->x   = item->header.pos_x
				+ TEXT_SPC + item->header.size_y;
			dt_ptr->dt_Fstr->y   = item->header.pos_y
				+ TEXT_SPC + dt_ptr->dt_Ffont->ascent;
			dt_ptr->dt_Fstr->len = item->choice.n;
			dt_ptr->dt_Fstr->flags.has_colorset = False;
			if (itemcolorset >= 0)
			{
				dt_ptr->dt_Fstr->colorset =
					GetColorset(itemcolorset);
				dt_ptr->dt_Fstr->flags.has_colorset
					= True;
			}
			FlocaleDrawString(dpy,
					  dt_ptr->dt_Ffont,
					  dt_ptr->dt_Fstr,
					  FWS_HAVE_LENGTH);
			break;
		case I_SEPARATOR:
			myfprintf((stderr, "redraw_frame calling RedrawSeparator\n"));
			RedrawSeparator(item);
			break;
		}
		if (dt_ptr && dt_ptr->dt_Fstr)
		{
			dt_ptr->dt_Fstr->flags.has_clip_region = False;
		}
	}
	if (region)
	{
		XDestroyRegion(region);
	}
}

static int form_label_next_character(void *context, const char *text)
{
	return FlocaleStringNumberOfBytes(context, text);
}

static int form_label_measure(void *context, const char *text, int length)
{
	return FlocaleTextWidth(context, (char *)text, length);
}

/* Filled rectangles leave the shared GC line attributes untouched. Both the
 * input redraw and incremental key path use the same bounded interior stroke. */
void form_draw_caret(Item *item, int x)
{
	form_caret_rectangle rectangle;

	if (form_caret_geometry(x, item->header.size_x, item->header.size_y,
		BOX_SPC, CF.ui_metrics.stroke, &rectangle))
		XFillRectangle(dpy, item->header.win,
			item->header.dt_ptr->dt_item_GC, rectangle.x, rectangle.y,
			rectangle.width, rectangle.height);
}

void RedrawText(Item *item)
{
  char *p;
  char display[FORM_LABEL_BYTES + 4];
  Region region = NULL;

  CheckAlloc(item,item->header.dt_ptr); /* alloc colors and fonts needed */
  item->header.dt_ptr->dt_Fstr->len = item->text.n;
  if ((p = memchr(item->text.value, '\0', item->header.dt_ptr->dt_Fstr->len))
      != NULL)
    item->header.dt_ptr->dt_Fstr->len = p - item->text.value;
  item->header.dt_ptr->dt_Fstr->win = CF.frame;
  item->header.dt_ptr->dt_Fstr->gc  = item->header.dt_ptr->dt_GC;
  item->header.dt_ptr->dt_Fstr->str = item->text.value;
  if (item->text.viewport)
  {
    int available = item->header.size_x - 2 * TEXT_SPC;
    int length = 0;

    if (!form_label_fit(item->text.value, item->text.n,
        available > 0 ? available : 0, item->header.dt_ptr->dt_Ffont,
        form_label_next_character, form_label_measure, display, &length))
      fprintf(stderr, "%s: refused LabelViewport text measurement\n",
        module->name);
    item->header.dt_ptr->dt_Fstr->str = display;
    item->header.dt_ptr->dt_Fstr->len = length;
  }
  item->header.dt_ptr->dt_Fstr->x   = item->header.pos_x + TEXT_SPC;
  item->header.dt_ptr->dt_Fstr->y   = item->header.pos_y + ( CF.padVText / 2 ) +
    item->header.dt_ptr->dt_Ffont->ascent;
  item->header.dt_ptr->dt_Fstr->flags.has_colorset = False;
  if (colorset >= 0)
  {
    item->header.dt_ptr->dt_Fstr->colorset = GetColorset(colorset);
    item->header.dt_ptr->dt_Fstr->flags.has_colorset = True;
  }
  if (item->text.fixed)
  {
    XRectangle clip = {item->header.pos_x, item->header.pos_y,
      item->header.size_x, item->header.size_y};
    XClearArea(dpy, CF.frame, clip.x, clip.y, clip.width, clip.height, False);
    region = XCreateRegion();
    XUnionRectWithRegion(&clip, region, region);
    /* Core/fontset drawing uses the GC clip; Xft uses the string region.
     * Form draw-table GCs have no persistent clip outside this paint. */
    XSetRegion(dpy, item->header.dt_ptr->dt_GC, region);
    item->header.dt_ptr->dt_Fstr->flags.has_clip_region = True;
    item->header.dt_ptr->dt_Fstr->clip_region = region;
  }
  FlocaleDrawString(dpy,
		    item->header.dt_ptr->dt_Ffont,
		    item->header.dt_ptr->dt_Fstr, FWS_HAVE_LENGTH);
  if (region != NULL)
  {
    XSetClipMask(dpy, item->header.dt_ptr->dt_GC, None);
    item->header.dt_ptr->dt_Fstr->flags.has_clip_region = False;
    item->header.dt_ptr->dt_Fstr->clip_region = NULL;
    item->header.dt_ptr->dt_Fstr->str = NULL;
    XDestroyRegion(region);
  }
  return;
}

/*
 *  Draw horizontal lines to form a separator
 *
 */
static void RedrawSeparator(Item *item)
{

  item->header.size_x = CF.max_width - 6;
  CheckAlloc(item,item->header.dt_ptr); /* alloc colors and fonts needed */
  if ( item->header.dt_ptr && item->header.dt_ptr->dt_colors[c_itemlo] ) { /* safety */
    XSetForeground(dpy, item->header.dt_ptr->dt_item_GC,item->header.dt_ptr->dt_colors[c_itemlo]);
    XDrawLine(dpy, item->header.win,item->header.dt_ptr->dt_item_GC, 0, 0, item->header.size_x, 0);
    XSetForeground(dpy, item->header.dt_ptr->dt_item_GC,item->header.dt_ptr->dt_colors[c_itemhi]);
    XDrawLine(dpy, item->header.win,item->header.dt_ptr->dt_item_GC, 0, 1, item->header.size_x, 1);

  } else {
    fprintf(stderr,"%s: Separators, no colors %p\n",module->name,item->header.dt_ptr);
  }
  return;
}

void RedrawTimeout(Item *item)
{
  char *tmpbuf, *tmpptr, *tmpbptr;
  int reallen;

  XClearArea(dpy, CF.frame,
	       item->header.pos_x, item->header.pos_y,
	       item->header.size_x, item->header.size_y,
	       False);

  tmpbuf = fxmalloc(item->timeout.len + 6);
  tmpbptr = tmpbuf;
  for (tmpptr = item->timeout.text; *tmpptr != '\0' &&
		!(tmpptr[0] == '%' && tmpptr[1] == '%'); tmpptr++) {
    *tmpbptr = *tmpptr;
    tmpbptr++;
  }
  if (tmpptr[0] == '%') {
    tmpptr++; tmpptr++;
    sprintf(tmpbptr, "%d", item->timeout.timeleft);
    tmpbptr += strlen(tmpbptr);
  }
  for (; *tmpptr != '\0'; tmpptr++) {
    *tmpbptr = *tmpptr;
    tmpbptr++;
  }
  *tmpbptr = '\0';

  reallen = strlen(tmpbuf);
  if (!CF.ui_scale_enabled) {
    item->header.size_x = FlocaleTextWidth(item->header.dt_ptr->dt_Ffont,
				     tmpbuf, reallen) + 2 * TEXT_SPC;
    item->header.size_y = item->header.dt_ptr->dt_Ffont->height + CF.padVText;
  }

  CheckAlloc(item,item->header.dt_ptr); /* alloc colors and fonts needed */
  item->header.dt_ptr->dt_Fstr->len = reallen;
  item->header.dt_ptr->dt_Fstr->win = CF.frame;
  item->header.dt_ptr->dt_Fstr->gc  = item->header.dt_ptr->dt_GC;
  item->header.dt_ptr->dt_Fstr->flags.has_colorset = False;
  if (colorset >= 0)
  {
    item->header.dt_ptr->dt_Fstr->colorset = GetColorset(colorset);
    item->header.dt_ptr->dt_Fstr->flags.has_colorset = True;
  }
  /* The draw table is shared: its preceding string may belong to another
   * item. Borrow this completed timeout buffer only for the draw call. */
  item->header.dt_ptr->dt_Fstr->str = tmpbuf;
  item->header.dt_ptr->dt_Fstr->x   = item->header.pos_x + TEXT_SPC;
  item->header.dt_ptr->dt_Fstr->y   = item->header.pos_y + ( CF.padVText / 2 ) +
    item->header.dt_ptr->dt_Ffont->ascent;
  FlocaleDrawString(dpy,
		    item->header.dt_ptr->dt_Ffont,
		    item->header.dt_ptr->dt_Fstr, FWS_HAVE_LENGTH);
  item->header.dt_ptr->dt_Fstr->str = NULL;
  free(tmpbuf);
  return;
}

/* redraw an item */
void RedrawItem (Item *item, int click, XEvent *pev)
{
  int dx, dy, len, x;
  char *bstr, *estr;
  static XSegment xsegs[4];
  XRectangle r,inter;
  Region region = None;
  Bool text_inter = True;
  Bool readonly_input, saved_clip = False;
  Region saved_region = None;
  DrawTable *dt_ptr;

  if (item == NULL)
    return;
  if (item->type == I_LIST)
  {
    form_list_draw(item);
    return;
  }
  dt_ptr = item->header.dt_ptr;
  readonly_input = item->type == I_INPUT && item->input.read_only;

  /* Init intersection to size of the item. */
  inter.x = BOX_SPC + TEXT_SPC - 1;
  inter.y = BOX_SPC;
  inter.width = item->header.size_x - (2 * BOX_SPC) - 2 - TEXT_SPC;
  inter.height = (item->header.size_y - 1) - 2 * BOX_SPC + 1;
  if (readonly_input)
  {
    /* Clear and clip the same complete interior, including the caret's
     * rightmost stroke. Repaint it once even for a partial expose. */
    inter.x = inter.y = BOX_SPC;
    inter.width = item->header.size_x - 2 * BOX_SPC;
    inter.height = item->header.size_y - 2 * BOX_SPC;
  }

  myfprintf((stderr,"%s: RedrawItem expose x=%d/y=%d h=%d/w=%d\n",module->name,
	     (int)pev->xexpose.x,
	     (int)pev->xexpose.y,
	     (int)pev->xexpose.width,
	     (int)pev->xexpose.height));

  /* This is a slightly altered expose event from ReadXServer. */
  if (pev && !readonly_input)
  {
	  r.x = pev->xexpose.x;
	  r.y =	 pev->xexpose.y;
	  r.width = pev->xexpose.width;
	  r.height = pev->xexpose.height;
	  text_inter = frect_get_intersection(
		  r.x, r.y, r.width, r.height,
		  inter.x,inter.y,inter.width,inter.height,
		  &inter);
  }
  else
  {
	  /* If its not an expose event, the area assume the
	     whole item is intersected. */
	  r.x = inter.x;
	  r.y = inter.y;
	  r.width = inter.width;
	  r.height = inter.height;
  }
  if (pev && !readonly_input && text_inter && dt_ptr && dt_ptr->dt_Fstr)
  {
	  region = XCreateRegion();
	  XUnionRectWithRegion (&r, region, region);
	  if (region)
	  {
		  dt_ptr->dt_Fstr->flags.has_clip_region = True;
		  dt_ptr->dt_Fstr->clip_region = region;
	  }
	  else
	  {
		  dt_ptr->dt_Fstr->flags.has_clip_region = False;
	  }
  }

  switch (item->type) {
  case I_INPUT:
    if (!text_inter)
    {
	    return;
    }
    /* Create frame (pressed in): */
    dx = item->header.size_x - 1;
    dy = item->header.size_y - 1;
    /*fprintf(stderr,"GC: %lu\n", item->header.dt_ptr->dt_item_GC);*/
    XSetForeground(dpy, item->header.dt_ptr->dt_item_GC,
		   item->header.dt_ptr->dt_colors[c_itemlo]);

    /* around 12/26/99, added XClearArea to this function.
       this was done to deal with display corruption during
       multifield paste.  dje */
    if (frect_get_intersection(
	    r.x, r.y, r.width, r.height,
	    inter.x, inter.y, inter.width, inter.height,
	    &inter))
    {
	    XClearArea(
		       dpy, item->header.win,
		       inter.x, inter.y, inter.width, inter.height, False);
    }

    xsegs[0].x1 = 0, xsegs[0].y1 = 0;
    xsegs[0].x2 = 0, xsegs[0].y2 = dy;
    xsegs[1].x1 = 0, xsegs[1].y1 = 0;
    xsegs[1].x2 = dx, xsegs[1].y2 = 0;
    xsegs[2].x1 = 1, xsegs[2].y1 = 1;
    xsegs[2].x2 = 1, xsegs[2].y2 = dy - 1;
    xsegs[3].x1 = 1, xsegs[3].y1 = 1;
    xsegs[3].x2 = dx - 1, xsegs[3].y2 = 1;
    XDrawSegments(dpy, item->header.win, item->header.dt_ptr->dt_item_GC,
		  xsegs, 4);
    XSetForeground(dpy, item->header.dt_ptr->dt_item_GC,
		   item->header.dt_ptr->dt_colors[c_itemhi]);
    xsegs[0].x1 = 1, xsegs[0].y1 = dy;
    xsegs[0].x2 = dx, xsegs[0].y2 = dy;
    xsegs[1].x1 = 2, xsegs[1].y1 = dy - 1;
    xsegs[1].x2 = dx, xsegs[1].y2 = dy - 1;
    xsegs[2].x1 = dx, xsegs[2].y1 = 1;
    xsegs[2].x2 = dx, xsegs[2].y2 = dy;
    xsegs[3].x1 = dx - 1, xsegs[3].y1 = 2;
    xsegs[3].x2 = dx - 1, xsegs[3].y2 = dy;
    XDrawSegments(dpy, item->header.win, item->header.dt_ptr->dt_item_GC,
		  xsegs, 4);

    if (readonly_input)
    {
      saved_clip = dt_ptr->dt_Fstr->flags.has_clip_region;
      saved_region = dt_ptr->dt_Fstr->clip_region;
      region = XCreateRegion();
      XUnionRectWithRegion(&inter, region, region);
      /* Core/fontset text and the caret use the GC; Xft uses the string
       * region. Draw-table GCs have no persistent clip outside a paint. */
      XSetRegion(dpy, dt_ptr->dt_item_GC, region);
      dt_ptr->dt_Fstr->flags.has_clip_region = True;
      dt_ptr->dt_Fstr->clip_region = region;
    }
    len = item->input.left;
    bstr = find_nth_UTF8_char(item->input.value, NULL, &len, NULL);
    len = CF.abs_cursor;
    estr = find_nth_UTF8_char(bstr, NULL, &len, NULL);
    len = (int)(estr - bstr);
    if (click) {
      x = BOX_SPC + TEXT_SPC +
	      FlocaleTextWidth(item->header.dt_ptr->dt_Ffont, bstr, len) - 1;
      XSetForeground(dpy, item->header.dt_ptr->dt_item_GC,
		     item->header.dt_ptr->dt_colors[c_item_bg]);
      form_draw_caret(item, x);
      myfprintf((stderr,"Line %d/%d - %d/%d (first)\n",
		     x, BOX_SPC, x, dy - BOX_SPC));
    }
    XSetForeground(dpy, item->header.dt_ptr->dt_item_GC,
		   item->header.dt_ptr->dt_colors[c_item_fg]);
    item->header.dt_ptr->dt_Fstr->win = item->header.win;
    item->header.dt_ptr->dt_Fstr->gc  = item->header.dt_ptr->dt_item_GC;
    item->header.dt_ptr->dt_Fstr->flags.has_colorset = False;
    if (itemcolorset >= 0)
    {
      item->header.dt_ptr->dt_Fstr->colorset = GetColorset(itemcolorset);
      item->header.dt_ptr->dt_Fstr->flags.has_colorset = True;
    }

    item->header.dt_ptr->dt_Fstr->str = item->input.blanks;
    item->header.dt_ptr->dt_Fstr->x = BOX_SPC + TEXT_SPC;
    item->header.dt_ptr->dt_Fstr->y = BOX_SPC + TEXT_SPC
		+ item->header.dt_ptr->dt_Ffont->ascent;
    item->header.dt_ptr->dt_Fstr->len = item->input.width;
    FlocaleDrawString(dpy,
		item->header.dt_ptr->dt_Ffont,
		item->header.dt_ptr->dt_Fstr, FWS_HAVE_LENGTH);

    item->header.dt_ptr->dt_Fstr->str = bstr;
    item->header.dt_ptr->dt_Fstr->x   = BOX_SPC + TEXT_SPC;
    item->header.dt_ptr->dt_Fstr->y   = BOX_SPC + TEXT_SPC
      + item->header.dt_ptr->dt_Ffont->ascent;
    item->header.dt_ptr->dt_Fstr->len = strlen(bstr);
    FlocaleDrawString(dpy,
		item->header.dt_ptr->dt_Ffont,
		item->header.dt_ptr->dt_Fstr, FWS_HAVE_LENGTH);

    if (item == CF.cur_input && !click) {
      len = (int)(estr - bstr);
      x = BOX_SPC + TEXT_SPC +
	FlocaleTextWidth(item->header.dt_ptr->dt_Ffont, bstr, len) - 1;
      form_draw_caret(item, x);
      myfprintf((stderr,"Line %d/%d - %d/%d\n",
		 x, BOX_SPC, x, dy - BOX_SPC));
    }
    myfprintf((stderr,"Just drew input field. click %d\n",(int)click));
    break;
  case I_CHOICE:
    dx = dy = item->header.size_y - 1;
    if (item->choice.on) {
	XSetForeground(dpy, item->header.dt_ptr->dt_item_GC,
		       item->header.dt_ptr->dt_colors[c_item_fg]);
      if (item->choice.sel->selection.key == IS_MULTIPLE) {
	xsegs[0].x1 = 5, xsegs[0].y1 = 5;
	xsegs[0].x2 = dx - 5, xsegs[0].y2 = dy - 5;
	xsegs[1].x1 = 5, xsegs[1].y1 = dy - 5;
	xsegs[1].x2 = dx - 5, xsegs[1].y2 = 5;
	XDrawSegments(dpy, item->header.win, item->header.dt_ptr->dt_item_GC,
		      xsegs, 2);
      } else {
	XDrawArc(dpy, item->header.win, item->header.dt_ptr->dt_item_GC,
		 5, 5, dx - 10, dy - 10, 0, 360 * 64);
      }
    } else
      XClearWindow(dpy, item->header.win);
    if (item->choice.on)
      XSetForeground(dpy, item->header.dt_ptr->dt_item_GC,
		     item->header.dt_ptr->dt_colors[c_itemlo]);
    else
      XSetForeground(dpy, item->header.dt_ptr->dt_item_GC,
		     item->header.dt_ptr->dt_colors[c_itemhi]);
    xsegs[0].x1 = 0, xsegs[0].y1 = 0;
    xsegs[0].x2 = 0, xsegs[0].y2 = dy;
    xsegs[1].x1 = 0, xsegs[1].y1 = 0;
    xsegs[1].x2 = dx, xsegs[1].y2 = 0;
    xsegs[2].x1 = 1, xsegs[2].y1 = 1;
    xsegs[2].x2 = 1, xsegs[2].y2 = dy - 1;
    xsegs[3].x1 = 1, xsegs[3].y1 = 1;
    xsegs[3].x2 = dx - 1, xsegs[3].y2 = 1;
    XDrawSegments(dpy, item->header.win, item->header.dt_ptr->dt_item_GC,
		  xsegs, 4);
    if (item->choice.on)
      XSetForeground(dpy, item->header.dt_ptr->dt_item_GC,
		     item->header.dt_ptr->dt_colors[c_itemhi]);
    else
      XSetForeground(dpy, item->header.dt_ptr->dt_item_GC,
		     item->header.dt_ptr->dt_colors[c_itemlo]);
    xsegs[0].x1 = 1, xsegs[0].y1 = dy;
    xsegs[0].x2 = dx, xsegs[0].y2 = dy;
    xsegs[1].x1 = 2, xsegs[1].y1 = dy - 1;
    xsegs[1].x2 = dx, xsegs[1].y2 = dy - 1;
    xsegs[2].x1 = dx, xsegs[2].y1 = 1;
    xsegs[2].x2 = dx, xsegs[2].y2 = dy;
    xsegs[3].x1 = dx - 1, xsegs[3].y1 = 2;
    xsegs[3].x2 = dx - 1, xsegs[3].y2 = dy;
    XDrawSegments(dpy, item->header.win, item->header.dt_ptr->dt_item_GC,
		  xsegs, 4);
    break;
  case I_BUTTON:
    if (!text_inter)
    {
	    return;
    }
    dx = item->header.size_x - 1;
    dy = item->header.size_y - 1;
    if (click)
      XSetForeground(dpy, item->header.dt_ptr->dt_item_GC,
		     item->header.dt_ptr->dt_colors[c_itemlo]);
    else
      XSetForeground(dpy, item->header.dt_ptr->dt_item_GC,
		     item->header.dt_ptr->dt_colors[c_itemhi]);
    xsegs[0].x1 = 0, xsegs[0].y1 = 0;
    xsegs[0].x2 = 0, xsegs[0].y2 = dy;
    xsegs[1].x1 = 0, xsegs[1].y1 = 0;
    xsegs[1].x2 = dx, xsegs[1].y2 = 0;
    xsegs[2].x1 = 1, xsegs[2].y1 = 1;
    xsegs[2].x2 = 1, xsegs[2].y2 = dy - 1;
    xsegs[3].x1 = 1, xsegs[3].y1 = 1;
    xsegs[3].x2 = dx - 1, xsegs[3].y2 = 1;
    XDrawSegments(dpy, item->header.win, item->header.dt_ptr->dt_item_GC,
		  xsegs, 4);
    if (click)
      XSetForeground(dpy, item->header.dt_ptr->dt_item_GC,
		     item->header.dt_ptr->dt_colors[c_itemhi]);
    else
      XSetForeground(dpy, item->header.dt_ptr->dt_item_GC,
		     item->header.dt_ptr->dt_colors[c_itemlo]);
    xsegs[0].x1 = 1, xsegs[0].y1 = dy;
    xsegs[0].x2 = dx, xsegs[0].y2 = dy;
    xsegs[1].x1 = 2, xsegs[1].y1 = dy - 1;
    xsegs[1].x2 = dx, xsegs[1].y2 = dy - 1;
    xsegs[2].x1 = dx, xsegs[2].y1 = 1;
    xsegs[2].x2 = dx, xsegs[2].y2 = dy;
    xsegs[3].x1 = dx - 1, xsegs[3].y1 = 2;
    xsegs[3].x2 = dx - 1, xsegs[3].y2 = dy;
    XDrawSegments(dpy, item->header.win, item->header.dt_ptr->dt_item_GC,
		  xsegs, 4);
    XSetForeground(dpy, item->header.dt_ptr->dt_item_GC,
		   item->header.dt_ptr->dt_colors[c_item_fg]);
    item->header.dt_ptr->dt_Fstr->win = item->header.win;
    item->header.dt_ptr->dt_Fstr->gc  = item->header.dt_ptr->dt_item_GC;
    item->header.dt_ptr->dt_Fstr->flags.has_colorset = False;
    if (itemcolorset >= 0)
    {
      item->header.dt_ptr->dt_Fstr->colorset = GetColorset(itemcolorset);
      item->header.dt_ptr->dt_Fstr->flags.has_colorset = True;
    }
    item->header.dt_ptr->dt_Fstr->str = item->button.text;
    item->header.dt_ptr->dt_Fstr->x   = BOX_SPC + TEXT_SPC;
    item->header.dt_ptr->dt_Fstr->y   = BOX_SPC + TEXT_SPC
      + item->header.dt_ptr->dt_Ffont->ascent;
    item->header.dt_ptr->dt_Fstr->len = item->button.len;
    if (FftSupport)
    {
	XClearArea(dpy, item->header.win,
		   inter.x, inter.y, inter.width, inter.height, False);
    }
    FlocaleDrawString(dpy,
		      item->header.dt_ptr->dt_Ffont,
		      item->header.dt_ptr->dt_Fstr, FWS_HAVE_LENGTH);
    myfprintf((stderr,"Just put %s into a button\n",
	       item->button.text));
    break;
  case I_SEPARATOR:
    RedrawSeparator(item);
    break;
  }
  if (readonly_input)
  {
    XSetClipMask(dpy, dt_ptr->dt_item_GC, None);
    dt_ptr->dt_Fstr->flags.has_clip_region = saved_clip;
    dt_ptr->dt_Fstr->clip_region = saved_region;
  }
  else if (dt_ptr && dt_ptr->dt_Fstr)
  {
	  dt_ptr->dt_Fstr->flags.has_clip_region = False;
  }
  if (region)
  {
	  XDestroyRegion(region);
  }
  XFlush(dpy);
}

/* Selection records own configuration state, not drawing resources. The
 * parent can borrow the first drawable item's GC without reordering items. */
static GC form_background_gc(void)
{
	for (Item *p = root_item_ptr; p != NULL; p = p->header.next)
	{
		if (p->header.dt_ptr != NULL)
		{
			CheckAlloc(p, p->header.dt_ptr);
			return p->header.dt_ptr->dt_GC;
		}
	}
	return None;
}

/* update transparency if background colorset is transparent */
/* window has moved redraw the background if it is transparent */
void UpdateRootTransapency(Bool pr_only, Bool do_draw)
{
	Item *item;

	if (CSET_IS_TRANSPARENT(colorset))
	{
		if (CSET_IS_TRANSPARENT_PR_PURE(colorset))
		{
			XClearArea(dpy, CF.frame, 0,0,0,0, False);
			if (do_draw)
			{
				RedrawFrame(NULL);
			}
		}
		else if (!pr_only || CSET_IS_TRANSPARENT_PR(colorset))
		{
			GC background_gc = form_background_gc();

			if (background_gc != None)
				SetWindowBackground(dpy, CF.frame,
					CF.max_width, CF.total_height,
					GetColorset((colorset)), Pdepth,
					background_gc, False);
			if (do_draw)
			{
				XClearArea(dpy, CF.frame, 0,0,0,0, False);
				RedrawFrame(NULL);
			}
		}
	}
	if (root_item_ptr == NULL || !root_item_ptr->header.next ||
	    !CSET_IS_TRANSPARENT(itemcolorset))
	{
		return;
	}
	for (item = root_item_ptr->header.next; item != NULL;
	     item = item->header.next)
	{
		if (item->header.win != None)
		{
			if (CSET_IS_TRANSPARENT_PR(itemcolorset) &&
			    !CSET_IS_TRANSPARENT(colorset))
			{
				continue;
			}
			if (CSET_IS_TRANSPARENT_PR_PURE(itemcolorset))
			{
				XClearArea(
					dpy, item->header.win, 0,0,0,0, False);
				if (do_draw)
				{
					RedrawItem(item, 0, NULL);
				}
			}
			else if (!pr_only ||
				 CSET_IS_TRANSPARENT_PR(itemcolorset))
			{
				SetWindowBackground(
					dpy, item->header.win,
					item->header.size_x, item->header.size_y,
					GetColorset((itemcolorset)), Pdepth,
					item->header.dt_ptr->dt_GC, False);
				XClearArea(
					dpy, item->header.win, 0,0,0,0, False);
				if (do_draw)
				{
					RedrawItem(item, 0, NULL);
				}
			}
		}
	}
}

/* execute a command */
void DoCommand (Item *cmd)
{
  int k, dn;
  char *sp;
  Item *item;

  if (cmd->button.command_refused || !form_command_begin())
  {
    fvwm_debug(__func__, "Refusing incomplete Form command construction");
    return;
  }

  /* pre-command */
  if (cmd->button.key == IB_QUIT)
  {
    if (!XWithdrawWindow(dpy, CF.frame, screen))
    {
      /* hm, what can we do now? just ignore this situation. */
    }
  }

  for (k = 0; k < cmd->button.n; k++) {
    char *parsed_command;
    /* construct command */
    parsed_command = ParseCommand(0, cmd->button.commands[k], '\0', &dn, &sp);
    if (parsed_command == NULL) {
      if (isTerminated)
        return;
      fvwm_debug(__func__, "Refusing incomplete or oversized Form command");
      continue;
    }
    myfprintf((stderr, "Final command[%d]: [%s]\n", k, parsed_command));

    /* send command */
    if ( parsed_command[0] == '!') {	/* If command starts with ! */
      int n;

      n = system(parsed_command+1);		/* Need synchronous execution */
      (void)n;
    } else {
      SendText(Channel,parsed_command, ref);
    }
  }

  /* post-command */
  if (CF.last_error) {			/* if form has last_error field */
    memset(CF.last_error->text.value, ' ', CF.last_error->text.n); /* clear */
    /* To do this more elegantly, the window resize logic should recalculate
       size_x for the Message as the window resizes.  Right now, just clear
       a nice wide area. dje */
    XClearArea(dpy,CF.frame,
	       CF.last_error->header.pos_x,
	       CF.last_error->header.pos_y,
	       /* CF.last_error->header.size_x, */
	       2000,
	       CF.last_error->header.size_y, False);
  } /* end form has last_error field */
  if (cmd->button.key == IB_QUIT) {
    if (CF.grab_server)
      XUngrabServer(dpy);
    /* This is a temporary bug workaround for the pipe drainage problem */
    SendQuitNotification(Channel);    /* let commands complete */
    /* Note how the window is withdrawn, but execution continues until
       the quit notifcation catches up with this module...
       Should not be a problem, there shouldn't be any more commands
       coming into FvwmForm.  dje */
  }
  else if (cmd->button.key == IB_RESTART) {
    Restart();
    for (item = root_item_ptr; item != 0;
	 item = item->header.next) {	/* all items */
      if (item->type == I_INPUT) {
	XClearWindow(dpy, item->header.win);
	RedrawItem(item, 0, NULL);
      }
      if (item->type == I_CHOICE || item->type == I_LIST)
	RedrawItem(item, 0, NULL);
    }
  }
}

/*
 * Return the pointer to the *num-th UTF-8 char (starting from 0,
 * or the last one if *num<0) that starts * _before_ before
 * (if before is non-NULL), put its index into *num,
 * and length into *len.
 * If vaind UTF-8 chars in the string end before the *num-th char,
 * return the pointer to '\0' or to the first offensive char,
 * and length *len=0, then a new value of *num becomes less than
 * the initial one.
 * *len == 0 or *num == -1 mean that no valid UTF-8 char returned.
 */

char* find_nth_UTF8_char(char *str, char *before,
	int *num, int *len)
{
    char *pstr = str; /* previous valid char */
    int i = 0; /* counter of chars got */
    int l = 0; /* length of the last chunk got */
    bool early; /* valid UTF-8 chars ended
		 * before the requested char */
    if (str == NULL || (before && (before <= str))) {
	if (num)
	    *num = -1;
	if (len)
	    *len = 0;
	return NULL;
    }

    while ((*str != '\0')
	    && (!before || str < before)
	    && (!num || *num < 0 || i <= *num))
    {
	/* parse UTF-8 string */
	early = true; /* break = no more chars */
	if ((str[0] & 0xe0) == 0xc0)        /* two-byte */
	{
	    if ((str[1] & 0xc0) != 0x80)
		break;
	    else {
		l = 2;
		pstr = str;
		str += 2;
		i++;
	    }
	}
	else if ((str[0] & 0xf0) == 0xe0) /* three-byte */
	{
	    if ((str[1] & 0xc0) != 0x80)
		break;
	    else if ((str[2] & 0xc0) != 0x80)
		break;
	    else {
		l = 3;
		pstr = str;
		str += 3;
		i++;
	    }
	}
	else if ((str[0] & 0xf8) == 0xf0) /* four-byte */
	{
	    if ((str[1] & 0xc0) != 0x80)
		break;
	    else if ((str[2] & 0xc0) != 0x80)
		break;
	    else if ((str[3] & 0xc0) != 0x80)
		break;
	    else {
		l = 4;
		pstr = str;
		str += 4;
		i++;
	    }
	}
	else {                  /* should be one-byte */
	    if ((*str & 0x80) ||
		(((unsigned char)(*str) < 0x20) &&
		    (*str != '\t') &&
		    (*str != '\r') &&
		    (*str != '\n') &&
		    (*str != '\f')))
		break;
	    else {
		l = 1;
		pstr = str;
		str += 1;
		i++;
	    }
	}
	early = false; /* proceed to the next char */

    }

    if (num && *num >= 0 && i <= *num) {
    /* ended prematurely => return '\0' of length zero */
	pstr = str;
	l = 0;
    }
    else if (early || (*str == '\0')) { /* invalid UTF-8 char or '\0' */
	l = (int)(str-pstr); /* one step back => return last valid char */
    }
    if (num)
	*num = i - 1;
    if (len)
	*len = l;
    return pstr;
}

/* open the windows */
static void OpenWindows(void)
{
  int x, y;
  int gravity = NorthWestGravity;
  Item *item;
  static XSetWindowAttributes xswa;
  static XWMHints wmh = { InputHint, True };
  static XSizeHints sh =
    { PPosition | PSize | USPosition | USSize | PWinGravity};
  XClassHint myclasshints;

  if (!CF.pointer[input_pointer]) {
    CF.pointer[input_pointer] = XCreateFontCursor(dpy, XC_xterm);
  }
  if (!CF.pointer[button_in_pointer]) {
    CF.pointer[button_in_pointer] = XCreateFontCursor(dpy, XC_hand2);
  }
  if (!CF.pointer[button_pointer]) {
    CF.pointer[button_pointer] = XCreateFontCursor(dpy,XC_hand2);
  }
  CF.screen_background = (colorset < 0)
    ? GetColor(screen_background_color)
    : GetColorset(colorset)->bg;

  if (!CF.p_c[input_back].used) {  /* if not set, use screen b/g */
    CF.p_c[input_back].pointer_color.pixel = CF.screen_background;
  }
  myfprintf((stderr,
	     "screen bg %X, getcolor bg %X, colorset bg %X colorset %d\n",
	     (int)CF.screen_background,
	     (int)GetColor(screen_background_color),
	     (int)GetColorset(colorset)->bg,
	     (int)colorset));
  XQueryColor(dpy, Pcmap, &CF.p_c[input_fore].pointer_color);
  XQueryColor(dpy, Pcmap, &CF.p_c[input_back].pointer_color);
  XRecolorCursor(dpy, CF.pointer[input_pointer],
		 &CF.p_c[input_fore].pointer_color,
		 &CF.p_c[input_back].pointer_color);
  myfprintf((stderr,"input fore %X, back %X\n",
	  (int)CF.p_c[input_fore].pointer_color.pixel,
	  (int)CF.p_c[input_back].pointer_color.pixel));
  /* The input cursor is handled differently than the 2 button cursors. */
  XQueryColor(dpy, Pcmap, &CF.p_c[button_fore].pointer_color);
  XQueryColor(dpy, Pcmap, &CF.p_c[button_back].pointer_color);
  XRecolorCursor(dpy, CF.pointer[button_pointer],
		 &CF.p_c[button_fore].pointer_color,
		 &CF.p_c[button_back].pointer_color);
  myfprintf((stderr,"button fore %X, back %X\n",
	  (int)CF.p_c[button_fore].pointer_color.pixel,
	  (int)CF.p_c[button_back].pointer_color.pixel));
  XQueryColor(dpy, Pcmap, &CF.p_c[button_in_fore].pointer_color);
  XQueryColor(dpy, Pcmap, &CF.p_c[button_in_back].pointer_color);
  XRecolorCursor(dpy, CF.pointer[button_in_pointer],
		 &CF.p_c[button_in_fore].pointer_color,
		 &CF.p_c[button_in_back].pointer_color);
  myfprintf((stderr,"button in fore %X, back %X\n",
	  (int)CF.p_c[button_in_fore].pointer_color.pixel,
	  (int)CF.p_c[button_in_back].pointer_color.pixel));
  /* the frame window first */
  if (CF.have_geom)
  {
    if (CF.xneg)
    {
      x = DisplayWidth(dpy, screen) - CF.max_width + CF.gx;
      gravity = NorthEastGravity;
    }
    else
    {
      x = CF.gx;
    }
    if (CF.yneg)
    {
      y = DisplayHeight(dpy, screen) - CF.total_height + CF.gy;
      gravity = SouthWestGravity;
    }
    else
    {
      y = CF.gy;
    }
    if (CF.xneg && CF.yneg)
    {
      gravity = SouthEastGravity;
    }
  } else {
    FScreenCenterOnScreen(
      NULL, FSCREEN_CURRENT, &x, &y, CF.max_width, CF.total_height);
    form_scale_startup_position();
  }
  /* hack to prevent mapping on wrong screen with StartsOnScreen */
  FScreenMangleScreenIntoUSPosHints(FSCREEN_CURRENT, &sh);
  xswa.background_pixel = CF.screen_background;
  xswa.border_pixel = 0;
  xswa.colormap = Pcmap;
  myfprintf((stderr,
	  "going to create window w. bg %s, b/g pixel %X, black pixel %X\n",
	     screen_background_color,
	     (int)xswa.background_pixel,
	     (int)BlackPixel(dpy, screen)));
  CF.frame = XCreateWindow(dpy, root, x, y, CF.max_width, CF.total_height, 0,
			   Pdepth, InputOutput, Pvisual,
			   CWColormap | CWBackPixel | CWBorderPixel, &xswa);
  wm_del_win = XInternAtom(dpy, "WM_DELETE_WINDOW", False);
  XSetWMProtocols(dpy, CF.frame, &wm_del_win, 1);
  XSelectInput(dpy, CF.frame,
	       KeyPressMask | ExposureMask | StructureNotifyMask |
	       VisibilityChangeMask);
  if (!CF.title) {
    CF.title = module->name;
  }
  XStoreName(dpy, CF.frame, CF.title);
  XSetWMHints(dpy, CF.frame, &wmh);
  myclasshints.res_name = module->name;
  myclasshints.res_class = "FvwmForm";
  XSetClassHint(dpy,CF.frame,&myclasshints);
  sh.width = CF.max_width;
  sh.height = CF.total_height;
  sh.win_gravity = gravity;
  if (CF.viewport != NULL)
  {
    sh.flags |= PMinSize;
    sh.min_width = CF.viewport_min_width;
    sh.min_height = CF.viewport_min_height;
  }
  XSetWMNormalHints(dpy, CF.frame, &sh);
  form_size_hints = sh;

  for (item = root_item_ptr; item != 0;
       item = item->header.next) {	/* all items */
    switch (item->type) {
    case I_LIST:
    case I_INPUT:
      myfprintf((stderr,"Checking alloc during OpenWindow on input\n"));
      CheckAlloc(item,item->header.dt_ptr); /* alloc colors and fonts needed */
      item->header.win =
	XCreateSimpleWindow(dpy, CF.frame,
			    item->header.pos_x, item->header.pos_y,
			    item->header.size_x, item->header.size_y,
			    0, CF.screen_background,
			    item->header.dt_ptr->dt_colors[c_item_bg]);
      XSelectInput(dpy, item->header.win, ButtonPressMask | ExposureMask);
      xswa.cursor = CF.pointer[input_pointer];
      XChangeWindowAttributes(dpy, item->header.win, CWCursor, &xswa);
      if (itemcolorset >= 0)
      {
	SetWindowBackground(dpy, item->header.win,
			    item->header.size_x, item->header.size_y,
			    GetColorset((itemcolorset)), Pdepth,
			    item->header.dt_ptr->dt_GC, True);
      }
      break;
    case I_CHOICE:
      myfprintf((stderr,"Checking alloc during Openwindow on choice\n"));
      CheckAlloc(item,item->header.dt_ptr); /* alloc colors and fonts needed */
      item->header.win =
	XCreateSimpleWindow(dpy, CF.frame,
			    item->header.pos_x, item->header.pos_y,
			    item->header.size_y, item->header.size_y,
			    0, CF.screen_background,
			    item->header.dt_ptr->dt_colors[c_item_bg]);
      XSelectInput(dpy, item->header.win, ButtonPressMask | ExposureMask);
      xswa.cursor = CF.pointer[button_pointer];
      XChangeWindowAttributes(dpy, item->header.win, CWCursor, &xswa);
      if (itemcolorset >= 0)
      {
	SetWindowBackground(dpy, item->header.win,
			    item->header.size_x, item->header.size_y,
			    GetColorset((itemcolorset)), Pdepth,
			    item->header.dt_ptr->dt_GC, True);
      }
      break;
    case I_BUTTON:
      myfprintf((stderr,"Checking alloc during Openwindow on button\n"));
      CheckAlloc(item,item->header.dt_ptr); /* alloc colors and fonts needed */
      item->header.win =
	XCreateSimpleWindow(dpy, CF.frame,
			    item->header.pos_x, item->header.pos_y,
			    item->header.size_x, item->header.size_y,
			    0, CF.screen_background,
			    item->header.dt_ptr->dt_colors[c_item_bg]);
      XSelectInput(dpy, item->header.win,
		   ButtonPressMask | ExposureMask);
      xswa.cursor = CF.pointer[button_pointer];
      XChangeWindowAttributes(dpy, item->header.win, CWCursor, &xswa);
      if (itemcolorset >= 0)
      {
	SetWindowBackground(dpy, item->header.win,
			    item->header.size_x, item->header.size_y,
			    GetColorset((itemcolorset)), Pdepth,
			    item->header.dt_ptr->dt_GC, True);
      }
      break;
    case I_SEPARATOR:
      myfprintf((stderr,"Checking alloc during OpenWindow on separator\n"));
      CheckAlloc(item,item->header.dt_ptr); /* alloc colors and fonts needed */
      item->header.size_x = CF.max_width - 6;
      myfprintf((stderr,"Create win %d/%d %d/%d\n",3,item->header.pos_y,
			    item->header.size_x, 2));
      item->header.win =
	XCreateSimpleWindow(dpy, CF.frame,
			    3, item->header.pos_y,
			    item->header.size_x, 2,
			    0, CF.screen_background,
			    item->header.dt_ptr->dt_colors[c_bg]);
      XSelectInput(dpy, item->header.win, ExposureMask);
      if (itemcolorset >= 0)
      {
	SetWindowBackground(dpy, item->header.win,
			    item->header.size_x, 2,
			    GetColorset((itemcolorset)), Pdepth,
			    item->header.dt_ptr->dt_GC, True);
      }
      break;
    }
  }
  Restart();
  if (colorset >= 0)
  {
    GC background_gc = form_background_gc();

    if (background_gc != None)
      SetWindowBackground(dpy, CF.frame, CF.max_width, CF.total_height,
			  GetColorset((colorset)), Pdepth, background_gc, True);
  }
  if (preload_yorn != 'y') {		/* if not a preload */
    XMapRaised(dpy, CF.frame);
    XMapSubwindows(dpy, CF.frame);
    if (CF.warp_pointer) {
      FWarpPointer(dpy, None, CF.frame, 0, 0, 0, 0,
		   CF.max_width / 2, CF.total_height - 1);
    }
  }
  DoCommand(&CF.def_button);
}

static void process_message(unsigned long, unsigned long *); /* proto */
static void ParseActiveMessage(char *); /* proto */
static bool form_native_message(char *);

/* read something from Fvwm */
static void ReadFvwm(void)
{

    FvwmPacket *packet;
    module_packet_read_status status =
      module_packet_reader_read(&form_packet_reader, &packet);

    if (status == MODULE_PACKET_READ_MORE)
      return;
    if (status != MODULE_PACKET_READ_READY) {
      if (status == MODULE_PACKET_READ_ERROR)
        fvwm_debug(__func__, "Unable to read Form packet: %s", strerror(errno));
      fvwmSetTerminate(0);
      return;
    }
    if (form_packet_is_valid(packet->type, packet->body,
                                  FvwmPacketBodySize(*packet)))
	process_message(packet->type, packet->body);
    else
	fvwm_debug(__func__, "Discarding incomplete Form packet body");
}
static void process_message(unsigned long type, unsigned long *body)
{
  if (type == M_STRING && form_native_message((char *)&body[3]))
    return;
  switch (type) {
  case M_END_CONFIG_INFO:
    form_scale_end_config();
    break;
  case M_CONFIG_INFO:			/* any module config command */
    if (form_scale_message((char *)&body[3]))
      break;
    myfprintf((stderr,"process_message: Got command: %s\n", (char *)&body[3]));
    ParseActiveMessage((char *)&body[3]);
    break;
  case MX_PROPERTY_CHANGE:
    if (body[0] == MX_PROPERTY_CHANGE_BACKGROUND &&
	((!Swallowed && body[2] == 0) || (Swallowed && body[2] == CF.frame)))
    {
      UpdateRootTransapency(True, True);
    }
    else if  (body[0] == MX_PROPERTY_CHANGE_SWALLOW && body[2] == CF.frame)
    {
	Swallowed = body[1];
    }
    break;
  case M_ERROR:
  case M_STRING:
    if (CF.last_error) {		/* if form has message area */
      /* ignore form size, its OK to write outside the window boundary */
      int msg_len;
      char *msg_ptr;
      /* Clear old message first */
      memset(CF.last_error->text.value, ' ', CF.last_error->text.n); /* clear */
      XClearArea(dpy,CF.frame,
		 CF.last_error->header.pos_x,
		 CF.last_error->header.pos_y,
		 2000,
		 CF.last_error->header.size_y, False);
      msg_ptr = (char *)&body[3];
      msg_len = strlen(msg_ptr);
      if (msg_len > 0 && msg_ptr[msg_len-1] == '\n') { /* line ends w newline */
	msg_ptr[msg_len-1] = '\0'; /* strip off \n */
      }
      if (CF.last_error->text.n <= msg_len) { /* if message wont fit */
	CF.last_error->text.value = fxrealloc(CF.last_error->text.value,
                                             msg_len * 2,
					     sizeof(CF.last_error->text.value));
	CF.last_error->text.n = msg_len * 2;
      }
      strncpy(CF.last_error->text.value,msg_ptr,
	      CF.last_error->text.n);
      RedrawText(CF.last_error);
      break;
    } /* module has last_error field */
  } /* end switch header */
}

/* These are the message from fvwm FvwmForm understands after form is
   active. */
static void am_Map(char *);
static void am_UnMap(char *);
static void am_Stop(char *);
static struct CommandTable am_table[] =
{
  {"ListAbort",am_list_abort},
  {"ListBegin",am_list_begin},
  {"ListCommit",am_list_commit},
  {"ListEntry",am_list_entry},
  {"ListFocus",am_list_focus},
  {"ListLayout",am_list_layout},
  {"ListStatus",am_list_status},
  {"ListWatchStatus",am_list_watch_status},
  {"Map",am_Map},
  {"RenewTimeout",am_renew_timeout},
  {"SetLabel",am_set_label},
  {"Stop",am_Stop},
  {"UIScaleStatus",form_scale_status},
  {"UnMap",am_UnMap}
};

/* Direct messages do not append runtime snapshots to core configuration. */
static bool form_native_message(char *buf)
{
	static struct CommandTable table[] = {
		{"ListAbort", am_list_abort}, {"ListBegin", am_list_begin},
		{"ListCommit", am_list_commit}, {"ListEntry", am_list_entry},
		{"ListFocus", am_list_focus},
		{"ListLayout", am_list_layout},
		{"ListStatus", am_list_status},
		{"ListWatchStatus", am_list_watch_status},
		{"RenewTimeout", am_renew_timeout}, {"SetLabel", am_set_label},
		{"UIScaleStatus", form_scale_status}
	};
	struct CommandTable *entry = FindToken(buf, table, struct CommandTable);
	char *p;

	if (entry == NULL)
		return false;
	if (!form_chomp_line(buf))
		return true;
	p = buf + strlen(entry->name);
	while (isspace((unsigned char)*p)) p++;
	FormVarsCheck(&p);
	entry->function(p);
	return true;
}

/* This is similar to the other 2 "Parse" functions. */
static void ParseActiveMessage(char *buf)
{
	char *p;
	struct CommandTable *e;
	if (!form_chomp_line(buf))
	{
		return;
	}

	if (strncasecmp(buf, "Colorset", 8) == 0)
	{
		Item *item;
		int n = LoadColorset(&buf[8]);
		if(n >= 0 && (n == colorset || n == itemcolorset))
		{
			for (item = root_item_ptr; item != 0;
			     item = item->header.next)
			{
				DrawTable *dt_ptr = item->header.dt_ptr;
				if (dt_ptr)
				{
					dt_ptr->dt_used = 0;
					if(dt_ptr->dt_GC)
					{
						XFreeGC(dpy, dt_ptr->dt_GC);
						dt_ptr->dt_GC = None;
					}
					if(dt_ptr->dt_item_GC)
					{
						XFreeGC(
							dpy,
							dt_ptr->dt_item_GC);
						dt_ptr->dt_item_GC = None;
					}
				}
			}
			for (item = root_item_ptr; item != 0;
			     item = item->header.next)
			{
				DrawTable *dt_ptr = item->header.dt_ptr;
				if (dt_ptr)
				{
					CheckAlloc(item, dt_ptr);
				}
			}
			if (colorset >= 0)
			{
				GC background_gc = form_background_gc();

				if (background_gc != None)
					SetWindowBackground(dpy, CF.frame,
						CF.max_width, CF.total_height,
						GetColorset((colorset)), Pdepth,
						background_gc, False);
				RedrawFrame(NULL);
			}
			for (item = root_item_ptr != NULL ?
			     root_item_ptr->header.next : NULL; item != NULL;
			     item = item->header.next)
			{
				DrawTable *dt_ptr = item->header.dt_ptr;
				if (dt_ptr && itemcolorset >= 0 &&
				    item->header.win != 0)
				{
					SetWindowBackground(
						dpy, item->header.win,
						item->header.size_x,
						item->header.size_y,
						GetColorset((itemcolorset)),
						Pdepth, dt_ptr->dt_GC, True);
					RedrawItem(item, 0, NULL);
				} /* end item has a drawtable */
			} /* end all items */
		}
		return;
	} /* end colorset command */
	if (strncasecmp(buf, mname, module->namelen+1) != 0)
	{
		/* If its not for me */
		return;
	} /* Now I know its for me. */
	p = buf+module->namelen+1;		    /* jump to end of my name */
	/* at this point we have recognized "*FvwmForm" */
	e = FindToken(p,am_table,struct CommandTable);/* find cmd in table */
	if (e == 0)
	{
		/* if no match */
		/* this may be a configuration command of another same form */
		if (FindToken(p, ct_table, struct CommandTable) == 0)
			fprintf(
				stderr,"%s: Active command unknown: %s\n",
				module->name,buf);
		return;				    /* ignore it */
	}

	p=p+strlen(e->name);			/* skip over name */
	while (isspace((unsigned char)*p)) p++; /* skip whitespace */

	FormVarsCheck(&p);
	e->function(p);			      /* call cmd processor */
	return;
} /* end function */

static void am_Map(char *cp)
{
  XMapRaised(dpy, CF.frame);
  XMapSubwindows(dpy, CF.frame);
  if (CF.warp_pointer) {
    FWarpPointer(dpy, None, CF.frame, 0, 0, 0, 0,
		 CF.max_width / 2, CF.total_height - 1);
  }
  myfprintf((stderr, "Map: got it\n"));
}
static void am_UnMap(char *cp)
{
  XUnmapWindow(dpy, CF.frame);
  myfprintf((stderr, "UnMap: got it\n"));
}
static void am_Stop(char *cp)
{
  /* syntax: *FFStop */
  myfprintf((stderr,"Got stop command.\n"));
  exit (0);				/* why bother, just exit. */
}

/* main event loop */
static void MainLoop(void)
{
  struct pollfd ready[2];

  while ( !isTerminated ) {
    /* TA:  20091219:  Automatically flush the buffer from the XServer and
     * process each request as we receive them.
     */
    if (!isTerminated && FPending(dpy))
	    ReadXServer();
    if (isTerminated)
      break;
    /* Libevent owns the countdown; the ordinary input owner never enters
     * its blocking dispatch loop. Preserve the configured tenth-second tick. */
    if (form_timer_base != NULL &&
        event_base_loop(form_timer_base, EVLOOP_NONBLOCK) < 0)
    {
      fvwm_debug(__func__, "Unable to service Form timer");
      break;
    }
    int timeout_ms = form_readiness_timeout(FPending(dpy) != 0,
      pev != NULL && event_pending(pev, EV_TIMEOUT, NULL));
    ready[0] = (struct pollfd){ Channel[1], POLLIN, 0 };
    ready[1] = (struct pollfd){ fd_x, POLLIN, 0 };
    int result = fvwmPoll(ready, 2, timeout_ms);
    if (result < 0)
    {
      if (errno == EINTR)
	continue;
      fvwm_debug(__func__, "poll failed: %s", strerror(errno));
      break;
    }
    if (isTerminated)
      break;
    if ((ready[0].revents & POLLNVAL) ||
        (ready[1].revents & (POLLERR | POLLHUP | POLLNVAL)))
      break;
    if (result > 0) {
      if (ready[0].revents & (POLLIN | POLLHUP))
	ReadFvwm();
      if (isTerminated || (ready[0].revents & POLLERR))
	break;
      /* The next pass owns one bounded X batch. Buffered Xlib events make
       * readiness nonblocking even when the connection fd is no longer ready. */
    }
  }
}


/* signal-handler to make the application quit */
static RETSIGTYPE
TerminateHandler(int sig)
{
  fvwmSetTerminate(sig);
  SIGNAL_RETURN;
}

/* signal-handler to make the timer work */
static void
TimerHandler(evutil_socket_t fd, short ev, void *arg)
{
  int dn;
  char *sp;
  char *parsed_command;

  timer->timeout.timeleft--;
  if (timer->timeout.timeleft <= 0) {
    evtimer_del(pev);

    /* construct command */
    parsed_command = ParseCommand(0, timer->timeout.command, '\0', &dn, &sp);
    if (parsed_command == NULL) {
      if (!isTerminated)
        fvwm_debug(__func__, "Refusing incomplete or oversized Form timeout command");
      return;
    }
    myfprintf((stderr, "Final command: %s\n", parsed_command));

    /* A refused command leaves the existing form available. */
    if (!XWithdrawWindow(dpy, CF.frame, screen))
    {
      /* hm, what can we do now? just ignore this situation. */
    }

    /* send command */
    if ( parsed_command[0] == '!') {	/* If command starts with ! */
      int n;

      n = system(parsed_command+1);		/* Need synchronous execution */
      (void)n;
    } else {
      SendText(Channel,parsed_command, ref);
    }

    /* post-command */
    if (CF.last_error) {		  /* if form has last_error field */
      memset(CF.last_error->text.value, ' ', CF.last_error->text.n); /* clear */
      /* To do this more elegantly, the window resize logic should recalculate
	 size_x for the Message as the window resizes.	Right now, just clear
	 a nice wide area. dje */
      XClearArea(dpy,CF.frame,
		 CF.last_error->header.pos_x,
		 CF.last_error->header.pos_y,
		 /* CF.last_error->header.size_x, */
		 2000,
		 CF.last_error->header.size_y, False);
    } /* end form has last_error field */
    if (CF.grab_server)
      XUngrabServer(dpy);
    /* This is a temporary bug workaround for the pipe drainage problem */
    SendQuitNotification(Channel);    /* let commands complete */
    /* Note how the window is withdrawn, but execution continues until
       the quit notifcation catches up with this module...
       Should not be a problem, there shouldn't be any more commands
       coming into FvwmForm.  dje */
  }
  else {
    RedrawTimeout(timer);
  }

  SIGNAL_RETURN;
}


/* main procedure */
int main (int argc, char **argv)
{
  int i;
  char cmd[200];

#ifdef DEBUGTOFILE
  freopen(".FvwmFormDebug","w",stderr);
#endif

  FlocaleInit(LC_CTYPE, "", "", "FvwmForm");

  FGettextInit("fvwm3", LOCALEDIR, "FvwmForm");

  module = ParseModuleArgs(argc,argv,1); /* allow an alias */
  if (module == NULL)
  {
    fprintf(
	    stderr,
	    "FvwmForm Version "VERSION" should only be executed by fvwm!\n");
    exit(1);
  }

  xasprintf(&mname, "*%s", module->name);

  {
    struct sigaction  sigact;

#ifdef SA_INTERRUPT
    sigact.sa_flags = SA_INTERRUPT;
#else
    sigact.sa_flags = 0;
#endif
    sigemptyset(&sigact.sa_mask);
    sigaddset(&sigact.sa_mask, SIGTERM);
    sigaddset(&sigact.sa_mask, SIGPIPE);
    sigaddset(&sigact.sa_mask, SIGINT);
    sigact.sa_handler = TerminateHandler;

    sigaction(SIGPIPE, &sigact, NULL);
    sigaction(SIGTERM, &sigact, NULL);
    sigaction(SIGINT, &sigact, NULL);
  }

  Channel[0] = module->to_fvwm;
  Channel[1] = module->from_fvwm;

  dpy = XOpenDisplay("");
  if (dpy==NULL) {
    fprintf(stderr,"%s: could not open display\n",module->name);
    exit(1);
  }
  /* From FvwmAnimate end */

  i = 7;
  if (argc >= 8) {			/* if have arg 7 */
    if (strcasecmp(argv[7],"preload") == 0) { /* if its preload */
      preload_yorn = 'y';		/* remember that. */
      i = 8;
    }
  }
  for (;i<argc;i++) {			/* look at remaining args */
    if (strchr(argv[i],'=')) {		/* if its a candidate */
      putenv(argv[i]);			/* save it away */
      CF.have_env_var = 'y';		/* remember we have at least one */
    }
  }
  ref = strtol(argv[4], NULL, 16);	/* capture reference window */
  if (ref == 0) ref = None;
  myfprintf((stderr, "ref == %d\n", (int)ref));

  flib_init_graphics(dpy);

  fd_x = XConnectionNumber(dpy);

  screen = DefaultScreen(dpy);
  root = RootWindow(dpy, screen);

  InitConstants();
  ReadDefaults();			/* get config from fvwm */

  if (strcasecmp(module->name,"FvwmForm") != 0) { /* if not already read */
    snprintf(cmd,sizeof(cmd),"read %s Quiet",module->name); /* read quiet modules config */
    SendText(Channel,cmd,0);
  }

  ReadConfig();				/* get config from fvwm */

  form_label_viewports_prepare();
  form_scale_capture(module->name);
  MassageConfig();			/* add data, calc window x/y */
  form_viewport_prepare();
  form_scale_initialize(module->name);

  /* tell fvwm about our mask */
  SetMessageMask(Channel,
    M_SENDCONFIG|M_CONFIG_INFO|M_END_CONFIG_INFO|M_ERROR|M_STRING);
  SetMessageMask(Channel, MX_PROPERTY_CHANGE);
  XSetErrorHandler(ErrorHandler);
  if (!module_packet_reader_init(&form_packet_reader, Channel[1])) {
    fprintf(stderr, "%s: Unable to initialize runtime packet reader: %s\n",
      module->name, strerror(errno));
    return 1;
  }
  OpenWindows();			/* create initial window */
  form_scale_retry();
  SendFinishedStartupNotification(Channel);/* tell fvwm we're running */
  for (Item *p = root_item_ptr; p != NULL; p = p->header.next)
    if (p->type == I_LIST && p->list.notify[0])
      form_list_report(p, p->list.notify, true);
  if (timer != NULL && !SetupTimer()) {
    fprintf(stderr, "%s: Unable to initialize timeout service\n", module->name);
    return 1;
  }
  MainLoop();				/* start */

  return 0;				/* */
}


RETSIGTYPE DeadPipe(int nonsense)
{
  exit(0);
  SIGNAL_RETURN;
}

/*
  X Error Handler
*/
static int
ErrorHandler(Display *dpy, XErrorEvent *event)
{
  /* some errors are OK=ish */
  if (event->error_code == BadPixmap)
    return 0;
  if (event->error_code == BadDrawable)
    return 0;
  if (FRenderGetErrorCodeBase() + FRenderBadPicture == event->error_code)
    return 0;

  PrintXErrorAndCoredump(dpy, event, module->name);
  return 0;
}
/*  Local Variables: */
/*  c-basic-offset: 8 */
/*  indent-tabs-mode: t */
/*  End: */


/* One bounded staging snapshot per list. A malformed addressed entry poisons
 * only that transaction; visible resources and user edits remain untouched. */
static Item *form_named_target(const char *cp, const char **tail, int type)
{
	const char *end = form_next_argument(cp);
	size_t length = end - cp;
	Item *target = NULL;

	while (length && isspace((unsigned char)cp[length - 1]))
		length--;
	*tail = end;
	for (Item *p = root_item_ptr; p != NULL; p = p->header.next)
		if (p->type == type && strlen(p->header.name) == length &&
		    memcmp(p->header.name, cp, length) == 0)
		{
			if (target != NULL)
				return NULL;
			target = p;
		}
	return target;
}

static Item *form_update_target(const char *cp, const char **tail)
{
	return form_named_target(cp, tail, I_LIST);
}

/* Startup modifier: keep the input ring and configured value unchanged. */
static void ct_input_readonly(char *cp)
{
	const char *tail;
	Item *p = form_named_target(cp, &tail, I_INPUT);

	if (p == NULL || *tail != '\0')
	{
		fprintf(stderr, "%s: invalid InputReadonly declared-input-name\n",
			module->name);
		return;
	}
	p->input.read_only = true;
}

static void am_list_begin(char *cp)
{
	const char *tail;
	Item *p = form_update_target(cp, &tail);
	int expected, generation;
	form_list_update prepared;

	if (p == NULL || p->list.update != NULL ||
	    !form_list_number(tail, INT_MAX, &expected) ||
	    !form_list_number(form_next_argument(tail), INT_MAX, &generation) ||
	    *form_next_argument(form_next_argument(tail)) != '\0' ||
	    generation <= p->list.update_serial ||
	    !form_list_update_begin(&prepared, p->list.state, expected, generation))
		goto invalid;
	p->list.update = fxmalloc(sizeof(prepared));
	*p->list.update = prepared;
	/* Aborting cannot make an old transaction identifier reusable. */
	p->list.update_serial = generation;
	return;
invalid:
	fprintf(stderr, "%s: refused ListBegin\n", module->name);
}

static void am_list_entry(char *cp)
{
	const char *tail, *source_text, *label, *end, *icon_text, *choice_text;
	char icon_name[16], choice_name[5];
	size_t icon_length;
	enum form_list_choice choice = FORM_LIST_KEEP;
	Item *p = form_update_target(cp, &tail);
	int generation, source;
	enum form_list_icon icon;

	/* Old transaction traffic cannot poison a newer staged generation. */
	if (p == NULL || p->list.update == NULL ||
	    !form_list_number(tail, INT_MAX, &generation) ||
	    generation != p->list.update->generation)
		goto invalid;
	source_text = form_next_argument(tail);
	label = form_next_argument(source_text);
	end = label;
	if (*end == '"')
		for (end++; *end && *end != '"'; end++)
			if (*end == '\\' && end[1]) end++;
	icon_text = *end == '"' ? end + 1 : end;
	while (isspace((unsigned char)*icon_text)) icon_text++;
	choice_text = form_next_argument(icon_text);
	icon_length = choice_text - icon_text;
	while (icon_length && isspace((unsigned char)icon_text[icon_length-1]))
		icon_length--;
	if (icon_length >= sizeof(icon_name)) goto poisoned;
	memcpy(icon_name, icon_text, icon_length);
	icon_name[icon_length] = '\0';
	if (*choice_text)
	{
		const char *choice_end = form_next_argument(choice_text);
		size_t length = choice_end - choice_text;

		while (length && isspace((unsigned char)choice_text[length-1]))
			length--;
		if (*choice_end || length >= sizeof(choice_name)) goto poisoned;
		memcpy(choice_name, choice_text, length);
		choice_name[length] = '\0';
		if (!strcasecmp(choice_name, "on")) choice = FORM_LIST_ON;
		else if (!strcasecmp(choice_name, "off")) choice = FORM_LIST_OFF;
		else if (strcasecmp(choice_name, "keep")) goto poisoned;
	}
	if (strncmp(source_text, "0 ", 2) == 0 ||
	    (source_text[0] == '0' && source_text[1] == '\t'))
		source = 0;
	else if (!form_list_number(source_text, FORM_LIST_MAX, &source))
		goto poisoned;
	if (*label != '"' || *end != '"' || strlen(label) > 1024 ||
	    !form_list_parse_icon(icon_name, &icon) ||
	    !form_list_update_add_choice(p->list.update, source, choice))
		goto poisoned;
	p->list.pending = form_reserve_array(p->list.pending,
		&p->list.pending_capacity, p->list.update->count - 1, 1, 32,
		sizeof(*p->list.pending));
	p->list.pending[p->list.update->count - 1] =
		(form_list_entry){CopyQuotedString(label + 1), icon};
	return;
poisoned:
	p->list.update->refused = true;
invalid:
	fprintf(stderr, "%s: refused ListEntry\n", module->name);
}

static void am_list_commit(char *cp)
{
	const char *tail;
	Item *p = form_update_target(cp, &tail);
	int generation, old_count;
	form_list_entry *old;

	if (p == NULL || p->list.update == NULL ||
	    !form_list_number(tail, INT_MAX, &generation) ||
	    *form_next_argument(tail) != '\0' ||
	    generation != p->list.update->generation)
		goto invalid;
	old_count = p->list.state->count;
	if (!form_list_update_commit(p->list.state, p->list.update))
	{
		p->list.update->refused = true;
		goto invalid;
	}
	old = p->list.entries;
	p->list.entries = p->list.pending;
	p->list.capacity = p->list.pending_capacity;
	p->list.pending = NULL;
	free(p->list.update);
	p->list.update = NULL;
	p->list.pending_capacity = 0;
	*p->list.initial = *p->list.state;
	for (int i = 0; i < old_count; i++)
		free(old[i].label);
	free(old);
	RedrawItem(p, 0, NULL);
	form_scale_retry();
	return;
invalid:
	fprintf(stderr, "%s: refused ListCommit\n", module->name);
}

static void am_list_focus(char *cp)
{
	const char *tail, *row;
	Item *p = form_update_target(cp, &tail);
	int generation, focus;

	if (p == NULL || p->list.update != NULL ||
	    !form_list_number(tail, INT_MAX, &generation))
		goto invalid;
	row = form_next_argument(tail);
	if (!form_list_number(row, FORM_LIST_MAX, &focus) ||
	    *form_next_argument(row) != '\0' ||
	    !form_list_focus_version(p->list.state, generation, focus - 1))
		goto invalid;
	*p->list.initial = *p->list.state;
	RedrawItem(p, 0, NULL);
	return;
invalid:
	fprintf(stderr, "%s: refused ListFocus\n", module->name);
}

static void am_list_layout(char *cp)
{
	const char *tail, *next, *cell_text, *columns_text;
	Item *p = form_update_target(cp, &tail);
	int expected, generation, units, columns, cell, row_height;

	if (p == NULL || p != CF.viewport || CF.frame == None ||
	    p->header.win == None || p->list.viewport_columns == 0 ||
	    p->list.line->n != 1 || p->list.update != NULL ||
	    !form_list_number(tail, INT_MAX, &expected))
		goto invalid;
	next = form_next_argument(tail);
	cell_text = form_next_argument(next);
	columns_text = form_next_argument(cell_text);
	if (!form_list_number(next, INT_MAX, &generation) ||
	    !form_list_number(cell_text, 4096, &units) ||
	    !form_list_number(columns_text, 8, &columns) ||
	    *form_next_argument(columns_text) != '\0' ||
	    generation <= p->list.update_serial ||
	    !form_input_geometry(units, p->header.dt_ptr->dt_Ffont->height,
		0, 2 * TEXT_SPC, &cell, &row_height) || cell > 32700)
		goto invalid;
	if (columns > 1)
		row_height = 3 * p->header.dt_ptr->dt_Ffont->height +
			4 * TEXT_SPC;
	if (CF.ui_scale_enabled &&
	    !form_scale_list_geometry(units, columns, &cell, &row_height))
		goto invalid;
	if (!form_viewport_reflow(CF.max_width, CF.total_height,
		cell, row_height, columns, expected, generation))
		goto invalid;
	p->list.viewport_units = units;
	form_size_hints.flags |= PMinSize;
	form_size_hints.min_width = CF.viewport_min_width;
	form_size_hints.min_height = CF.viewport_min_height;
	XSetWMNormalHints(dpy, CF.frame, &form_size_hints);
	RedrawFrame(NULL);
	RedrawItem(p, 0, NULL);
	form_scale_retry();
	return;
invalid:
	fprintf(stderr, "%s: refused ListLayout\n", module->name);
}

static void am_list_abort(char *cp)
{
	const char *tail;
	Item *p = form_update_target(cp, &tail);
	int generation;

	if (p == NULL || p->list.update == NULL ||
	    !form_list_number(tail, INT_MAX, &generation) ||
	    *form_next_argument(tail) != '\0' ||
	    generation != p->list.update->generation)
	{
		fprintf(stderr, "%s: refused ListAbort\n", module->name);
		return;
	}
	form_list_abort(p);
	form_scale_retry();
}


/* A native reply observes actual startup/publication and pending refusal. */
static bool form_list_report(Item *p, const char *token, bool selection)
{
	char reply[1024];
	char *packet = selection ? form_encoded_value(p) : NULL;
	int length;

	if (selection && packet == NULL)
		goto invalid;
	length = snprintf(reply, sizeof(reply),
		"Echo FVWM_FORM_LIST %s %d %d %d%s%s", token,
		p->list.state->generation,
		p->list.update ? p->list.update->generation : 0,
		p->list.update ? p->list.update->refused : 0,
		selection ? " x" : "", selection ? packet : "");
	free(packet);
	if (length < 0 || (size_t)length >= sizeof(reply))
		goto invalid;
	XFlush(dpy);
	SendText(Channel, reply, 0);
	return true;
invalid:
	fprintf(stderr, "%s: refused collection report\n", module->name);
	return false;
}

static void am_list_status(char *cp)
{
	const char *tail;
	Item *p = form_update_target(cp, &tail);
	char token[65];
	bool selection;

	if (p == NULL || p->list.state->generation == 0 ||
	    !form_list_status_token(tail, token, &selection))
	{
		fprintf(stderr, "%s: refused ListStatus\n", module->name);
		return;
	}
	form_list_report(p, token, selection);
}

/* The snapshot and rearm share this single-threaded handler. Input before it
 * is included; the next interactive mutation can send a fresh invalidation. */
static void am_list_watch_status(char *cp)
{
	const char *tail;
	Item *p = form_update_target(cp, &tail);
	char request[65];

	if (p == NULL || !form_list_watch_request(&p->list.watch,
	    p->list.state, tail, request))
	{
		fprintf(stderr, "%s: refused ListWatchStatus\n", module->name);
		return;
	}
	if (form_list_report(p, request, true))
		p->list.watch.outstanding = false;
}

static void ct_list_watch(char *cp)
{
	if (form_current_list == NULL ||
	    !form_list_watch_begin(&form_current_list->list.watch,
	    form_current_list->list.state, cp))
		fprintf(stderr, "%s: refused ListWatch\n", module->name);
}

static void ct_list_notify(char *cp)
{
	char token[65];
	bool selection;

	if (form_current_list == NULL ||
	    form_current_list->list.state->generation == 0 ||
	    !form_list_status_token(cp, token, &selection) || selection)
	{
		fprintf(stderr, "%s: refused ListNotify\n", module->name);
		return;
	}
	memcpy(form_current_list->list.notify, token, strlen(token) + 1);
}

static void ct_label(char *cp)
{
	form_input_args args;
	Item prepared;
	int width, height;
	const char *end = form_next_argument(cp);
	size_t length = end - cp;

	while (length && isspace((unsigned char)cp[length-1])) length--;
	if (length == 0 || length > 64) goto invalid;
	for (size_t i = 0; i < length; i++)
		if (!((cp[i] >= 'a' && cp[i] <= 'z') ||
		      (cp[i] >= 'A' && cp[i] <= 'Z') ||
		      (cp[i] >= '0' && cp[i] <= '9') || cp[i] == '_' || cp[i] == '-'))
			goto invalid;
	if (!form_parse_input(cp, &args) ||
	    !form_list_number(end, 4096, &args.width) ||
	    !form_label_text(args.initial) || !form_native_cleanup_register())
		goto invalid;
	memset(&prepared, 0, sizeof(prepared));
	prepared.type = I_TEXT;
	AssignDrawTable(&prepared);
	if (!form_input_geometry(args.width, prepared.header.dt_ptr->dt_Ffont->height,
	    2 * TEXT_SPC, CF.padVText, &width, &height) ||
	    width > 32760 || height > 32760)
		goto invalid;
	AddItem();
	bg_state = 'u';
	item->type = I_TEXT;
	item->header.dt_ptr = prepared.header.dt_ptr;
	item->header.name = CopySolidString(cp);
	item->header.size_x = width;
	item->header.size_y = height;
	item->text.fixed = true;
	item->text.minimum_width = width;
	item->text.value = form_copy_quoted(args.initial + 1);
	item->text.n = strlen(item->text.value);
	AddToLine(item);
	return;
invalid:
	fprintf(stderr, "%s: invalid Label name width quoted-text\n", module->name);
}

/* The modifier follows an already declared Label. Final validation waits for
 * the complete Form, so later items cannot silently share its dedicated line. */
static void ct_label_viewport(char *cp)
{
	const char *tail;
	Item *p = form_named_target(cp, &tail, I_TEXT);

	if (p == NULL || !p->text.fixed || *tail != '\0')
	{
		fprintf(stderr, "%s: invalid LabelViewport declared-label-name\n",
			module->name);
		return;
	}
	p->text.viewport = true;
}

static void form_label_viewports_prepare(void)
{
	Line *line = &root_line;
	int lists = 0;
	bool supported;

	for (Item *p = root_item_ptr; p != NULL; p = p->header.next)
		if (p->type == I_LIST)
			lists++;
	supported = lists == 1 && CF.viewport != NULL &&
		CF.viewport->list.line->n == 1;
	do
	{
		for (int i = 0; i < line->n; i++)
		{
			Item *p = line->items[i];

			if (p->type != I_TEXT || !p->text.viewport)
				continue;
			if (!supported || line->n != 1)
			{
				fprintf(stderr, "%s: LabelViewport %s needs a "
					"dedicated Line in one ListViewport Form\n",
					module->name, p->header.name);
				p->text.viewport = false;
			}
		}
		line = line->next;
	} while (line != &root_line);
}

static void am_set_label(char *cp)
{
	const char *text;
	Item *p = form_named_target(cp, &text, I_TEXT);
	char *copy;

	if (p == NULL || !p->text.fixed || !form_label_text(text))
	{
		fprintf(stderr, "%s: refused SetLabel\n", module->name);
		return;
	}
	copy = form_copy_quoted(text + 1);
	p->header.dt_ptr->dt_Fstr->str = NULL;
	free(p->text.value);
	p->text.value = copy;
	p->text.n = strlen(copy);
	RedrawText(p);
}


/* Renew a live lease, never resurrect a timeout already dispatched. */
static void am_renew_timeout(char *cp)
{
	int ticks;

	if (timer == NULL || pev == NULL || timer->timeout.timeleft <= 0 ||
	    !event_pending(pev, EV_TIMEOUT, NULL) ||
	    !form_list_number(cp, 99999, &ticks) ||
	    *form_next_argument(cp) != '\0')
	{
		fprintf(stderr, "%s: refused RenewTimeout\n", module->name);
		return;
	}
	timer->timeout.timeleft = ticks;
}
