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

#include "config.h"

#include <time.h>
#include <errno.h>
#include <limits.h>
#include <stdint.h>
#include "script_file.h"
#include "script_command.h"
#include "script_output.h"
#include "script_index.h"
#include "script_process.h"
#include "script_fifo.h"
#include "script_value.h"
#include "script_storage.h"
#include "Widgets/Tools.h"

#include "types.h"
#include "libs/fvwmsignal.h"
#include "libs/ftime.h"
#include "libs/FGettext.h"
#include "libs/FEvent.h"
#include "libs/FSelection.h"
#include "libs/Bindings.h"
#include "libs/charmap.h"
#include "libs/wcontext.h"
#include "libs/modifiers.h"
#include "libs/Module.h"
#include "libs/ColorUtils.h"
#include "libs/Strings.h"
#include "libs/log.h"
#ifdef HAVE_GETPWUID
#  include <pwd.h>
#endif

extern int fd[2];
extern Window ref;

void (*TabCom[29]) (int NbArg,long *TabArg);
char *(*TabFunc[27]) (int *NbArg, long *TabArg);
int (*TabComp[7]) (char *arg1,char *arg2);

extern Display *dpy;
extern int screen;
extern X11base *x11base;
extern int grab_serve;
extern struct XObj *tabxobj[1000];
extern Binding *BindingsList;
extern void LoadIcon(struct XObj *xobj);

extern int nbobj;
extern char **TabVVar;
extern int TabIdObj[1001];
extern char *ScriptName;
extern ModuleArgs *module;
extern TypeBuffSend BuffSend;
extern Atom propriete;
extern char *LastString;
char *FvwmUserDir = NULL;
static char *output_cache;
static size_t output_cache_length;
static char *output_cache_command;
static time_t output_cache_time;

/*
 * Utilities
 */
void setFvwmUserDir(void)
{
  char *home_dir;

  if (FvwmUserDir != NULL)
    return;

  /* Figure out user's home directory */
  home_dir = getenv("HOME");
#ifdef HAVE_GETPWUID
  if (home_dir == NULL) {
    struct passwd* pw = getpwuid(getuid());
    if (pw != NULL)
      home_dir = fxstrdup(pw->pw_dir);
  }
#endif
  if (home_dir == NULL)
    home_dir = "/"; /* give up and use root dir */

  /* Figure out where to read and write user's data files. */
  FvwmUserDir = getenv("FVWM_USERDIR");
  if (FvwmUserDir == NULL)
  {
    xasprintf(&FvwmUserDir, "%s/.fvwm", home_dir);
  }
}

/*
 * Functions to compare 2 integers
 */
static int Inf(char *arg1,char *arg2)
{
  int left, right;

  if (!script_value_integer(arg1, &left) ||
      !script_value_integer(arg2, &right))
  {
    fvwm_debug(__func__, "[%s]: comparison integer overflow", ScriptName);
    return 0;
  }
  return left < right;
}

static int InfEq(char *arg1,char *arg2)
{
  int left, right;

  if (!script_value_integer(arg1, &left) ||
      !script_value_integer(arg2, &right))
  {
    fvwm_debug(__func__, "[%s]: comparison integer overflow", ScriptName);
    return 0;
  }
  return left <= right;
}

static int Equal(char *arg1,char *arg2)
{
  return strcmp(arg1, arg2) == 0;
}

static int SupEq(char *arg1,char *arg2)
{
  int left, right;

  if (!script_value_integer(arg1, &left) ||
      !script_value_integer(arg2, &right))
  {
    fvwm_debug(__func__, "[%s]: comparison integer overflow", ScriptName);
    return 0;
  }
  return left >= right;
}

static int Sup(char *arg1,char *arg2)
{
  int left, right;

  if (!script_value_integer(arg1, &left) ||
      !script_value_integer(arg2, &right))
  {
    fvwm_debug(__func__, "[%s]: comparison integer overflow", ScriptName);
    return 0;
  }
  return left > right;
}

static int Diff(char *arg1,char *arg2)
{
  return strcmp(arg1, arg2) != 0;
}

static Bool storage_allowed(script_storage_result result)
{
  if (result != SCRIPT_STORAGE_OK)
    FvwmScriptStopForStorage(result == SCRIPT_STORAGE_TOTAL_LIMIT);
  return result == SCRIPT_STORAGE_OK && FvwmScriptExecutionAllowed();
}

static char *bounded_value_copy(const char *text)
{
  if (text == NULL)
    text = "";
  if (!storage_allowed(script_storage_check_value(script_storage_runtime(),
                                                  strlen(text))))
    return fxstrdup("");
  return fxstrdup(text);
}

static char *integer_value_copy(long long value)
{
  char text[32];
  snprintf(text, sizeof(text), "%lld", value);
  return bounded_value_copy(text);
}

static Bool bounded_value_append(char **text, size_t *length, const char *value)
{
  size_t added = strlen(value);
  size_t limit = script_storage_runtime()->value_limit;

  if (*length > limit || added > limit - *length)
  {
    FvwmScriptStopForStorage(False);
    return False;
  }
  return FvwmScriptExecutionAllowed() &&
    script_text_append(text, length, value, limit);
}

/*
 * Funtion that returns an argument value
 */
static char *CalcArg (long *TabArg,int *Ix)
{
  char *TmpStr;
  int i;

  if (TabArg[*Ix]>100000)       /* Number coding case */
  {
    if (!script_value_decode_integer(TabArg[*Ix], &i))
      fvwm_debug(__func__, "[%s]: invalid numeric argument tag", ScriptName);
    TmpStr = integer_value_copy(i);
  }
  else if (TabArg[*Ix] < -200000)/* Comparison fuction ID case */
  {
   i = TabArg[*Ix]+250000;
   TmpStr = integer_value_copy(i);
  }
  else if (TabArg[*Ix] < -100000)       /* Function ID case */
  {
    TmpStr = TabFunc[TabArg[*Ix]+150000](Ix,TabArg);
  }
  else                          /* Variable case */
  {
    TmpStr=bounded_value_copy(TabVVar[TabArg[*Ix]]);
  }
  if (!storage_allowed(script_storage_check_value(script_storage_runtime(),
                                                  strlen(TmpStr))))
  {
    free(TmpStr);
    return fxstrdup("");
  }
  return TmpStr;
}

static int widget_index(const char *text, const char *caller)
{
  if (!FvwmScriptExecutionAllowed())
    return -1;
  int index = script_widget_index(text, TabIdObj,
    sizeof(TabIdObj) / sizeof(TabIdObj[0]),
    nbobj >= 0 && nbobj <= 1000 ? (size_t)nbobj : 0);

  if (index >= 0 && tabxobj[index] != NULL)
    return index;
  fvwm_debug(caller, "[%s]: invalid or missing widget ID: %s",
    ScriptName, text);
  return -1;
}

static Bool widget_integer(const char *text, int *value, const char *caller)
{
  if (script_value_integer(text, value))
    return True;
  fvwm_debug(caller, "[%s]: widget number outside integer range: %.80s",
    ScriptName, text);
  return False;
}

/*
 * Functions to get an object properties
 */

/* GetValue */
static char *FuncGetValue(int *NbArg, long *TabArg)
{
  char *tmp;
  int Id;

  (*NbArg)++;              /* GetValue has one single argument */
  tmp = CalcArg(TabArg,NbArg);
  Id = widget_index(tmp, __func__);
  free(tmp);
  if (Id < 0)
    return fxstrdup("");
  tmp = integer_value_copy(tabxobj[Id]->value);
  return tmp;
}

/* GetMinValue */
static char *FuncGetMinValue(int *NbArg, long *TabArg)
{
  char *tmp;
  int Id;

  (*NbArg)++;              /* GetValue has one single argument */
  tmp = CalcArg(TabArg,NbArg);
  Id = widget_index(tmp, __func__);
  free(tmp);
  if (Id < 0)
    return fxstrdup("");
  tmp = integer_value_copy(tabxobj[Id]->value2);
  return tmp;
}

/* GetMaxValue */
static char *FuncGetMaxValue(int *NbArg, long *TabArg)
{
  char *tmp;
  int Id;

  (*NbArg)++;         /* GetValue has one single argument */
  tmp = CalcArg(TabArg,NbArg);
  Id = widget_index(tmp, __func__);
  free(tmp);
  if (Id < 0)
    return fxstrdup("");
  tmp = integer_value_copy(tabxobj[Id]->value3);
  return tmp;
}

/* GetFore */
static char *FuncGetFore(int *NbArg, long *TabArg)
{
  char *tmp;
  int Id;
  XColor color;

  (*NbArg)++;           /* GetValue has one single argument */
  tmp = CalcArg(TabArg,NbArg);
  Id = widget_index(tmp, __func__);
  free(tmp);
  if (Id < 0)
    return fxstrdup("");
  color.pixel = tabxobj[Id]->TabColor[fore];
  XQueryColor(dpy, Pcmap, &color);
  char text[16];
  snprintf(text, sizeof(text), "%02x%02x%02x",
           color.red >> 8, color.green >> 8, color.blue >> 8);
  tmp = bounded_value_copy(text);
  return tmp;
}

/* GetBack */
static char *FuncGetBack(int *NbArg, long *TabArg)
{
  char *tmp;
  int Id;
  XColor color;

  (*NbArg)++;              /* GetValue has one single argument */
  tmp = CalcArg(TabArg,NbArg);
  Id = widget_index(tmp, __func__);
  free(tmp);
  if (Id < 0)
    return fxstrdup("");
  color.pixel = tabxobj[Id]->TabColor[back];
  XQueryColor(dpy, Pcmap, &color);
  char text[16];
  snprintf(text, sizeof(text), "%02x%02x%02x",
           color.red >> 8, color.green >> 8, color.blue >> 8);
  tmp = bounded_value_copy(text);
  return tmp;
}

/* GetHili */
static char *FuncGetHili(int *NbArg, long *TabArg)
{
  char *tmp;
  int Id;
  XColor color;

  (*NbArg)++;             /* GetValue has one single argument */
  tmp = CalcArg(TabArg,NbArg);
  Id = widget_index(tmp, __func__);
  free(tmp);
  if (Id < 0)
    return fxstrdup("");
  color.pixel = tabxobj[Id]->TabColor[hili];
  XQueryColor(dpy, Pcmap, &color);
  char text[16];
  snprintf(text, sizeof(text), "%02x%02x%02x",
           color.red >> 8, color.green >> 8, color.blue >> 8);
  tmp = bounded_value_copy(text);
  return tmp;
}

/* GetShad */
static char *FuncGetShad(int *NbArg, long *TabArg)
{
  char *tmp;
  int Id;
  XColor color;

  (*NbArg)++;             /* GetValue has one single argument */
  tmp = CalcArg(TabArg,NbArg);
  Id = widget_index(tmp, __func__);
  free(tmp);
  if (Id < 0)
    return fxstrdup("");
  color.pixel = tabxobj[Id]->TabColor[shad];
  XQueryColor(dpy, Pcmap, &color);
  char text[16];
  snprintf(text, sizeof(text), "%02x%02x%02x",
           color.red >> 8, color.green >> 8, color.blue >> 8);
  tmp = bounded_value_copy(text);
  return tmp;
}

/* Returns an object title */
static char *FuncGetTitle(int *NbArg, long *TabArg)
{
  char *tmp;
  int Id;

  (*NbArg)++;
  tmp = CalcArg(TabArg,NbArg);
  Id = widget_index(tmp, __func__);
  free(tmp);
  if (Id < 0)
    return fxstrdup("");
  return bounded_value_copy(tabxobj[Id]->title);
}

/* Returns a command output */
static char *FuncGetOutput(int *NbArg, long *TabArg)
{
  char *command, *value, *result;
  char *number_end;
  long line, word;
  time_t now;

  (*NbArg)++;
  command = CalcArg(TabArg, NbArg);
  (*NbArg)++;
  value = CalcArg(TabArg, NbArg);
  errno = 0;
  line = strtol(value, &number_end, 10);
  Bool valid = errno == 0 && number_end != value && *number_end == '\0' &&
    line >= INT_MIN && line <= INT_MAX;
  free(value);
  (*NbArg)++;
  value = CalcArg(TabArg, NbArg);
  errno = 0;
  word = strtol(value, &number_end, 10);
  valid = valid && errno == 0 && number_end != value &&
    *number_end == '\0' && word >= INT_MIN && word <= INT_MAX;
  free(value);
  if (!FvwmScriptExecutionAllowed() || !valid ||
      strlen(command) > SCRIPT_OUTPUT_COMMAND_MAX_BYTES)
  {
    fvwm_debug(__func__, "[%s]: invalid GetOutput index or command size",
               ScriptName);
    free(command);
    return fxstrdup("");
  }
  now = time(NULL);
  if (output_cache_command == NULL ||
      strcmp(output_cache_command, command) != 0 ||
      now < output_cache_time || difftime(now, output_cache_time) > 1)
  {
    char *buffer;
    size_t length;
    if (!script_process_read(command, x11base->command_timeout_ms,
                            &buffer, &length))
    {
      fvwm_debug(__func__, "[%s]: GetOutput failed: %s",
                 ScriptName, strerror(errno));
      free(command);
      return fxstrdup("");
    }
    free(output_cache);
    free(output_cache_command);
    output_cache = buffer;
    output_cache_length = length;
    output_cache_command = fxstrdup(command);
    output_cache_time = now;
  }
  result = script_output_extract_limited(output_cache, output_cache_length,
      (int)line, (int)word, script_storage_runtime()->value_limit);
  if (result == NULL)
  {
    FvwmScriptStopForStorage(False);
    result = fxstrdup("");
  }
  free(command);
  return result;
}

/* Numeric functions evaluate every argument once, including rejected input. */
static char *value_result(char *result, const char *caller)
{
  if (result != NULL)
    return result;
  fvwm_debug(caller, "[%s]: invalid numeric value or string bounds",
    ScriptName);
  return fxstrdup("");
}

/* Converting from decimal to hexadecimal */
static char *FuncNumToHex(int *NbArg, long *TabArg)
{
  char *number, *padding, *result = NULL;
  int value, width;

  (*NbArg)++;
  number = CalcArg(TabArg, NbArg);
  (*NbArg)++;
  padding = CalcArg(TabArg, NbArg);
  if (script_value_integer(number, &value) &&
      script_value_integer(padding, &width))
  {
    char natural[32];
    size_t length = snprintf(natural, sizeof(natural), "%X", (unsigned int)value);
    if (width > 0 && width <= SCRIPT_VALUE_HEX_MAX_WIDTH &&
        (size_t)width > length)
      length = width;
    if (storage_allowed(script_storage_check_value(script_storage_runtime(),
                                                   length)))
      result = script_value_hex(value, width);
  }
  free(number);
  free(padding);
  return value_result(result, __func__);
}

/* Converting from hexadecimal to decimal */
static char *FuncHexToNum(int *NbArg, long *TabArg)
{
  char *number, *result = NULL;
  int value;

  (*NbArg)++;
  number = CalcArg(TabArg, NbArg);
  if (script_value_hex_number(number, &value))
    result = integer_value_copy(value);
  free(number);
  return value_result(result, __func__);
}

static char *calculate_value(int *NbArg, long *TabArg,
  script_value_operation operation, const char *caller)
{
  char *first, *second, *result = NULL;
  int left, right, value;

  (*NbArg)++;
  first = CalcArg(TabArg, NbArg);
  (*NbArg)++;
  second = CalcArg(TabArg, NbArg);
  if (script_value_integer(first, &left) &&
      script_value_integer(second, &right) &&
      script_value_calculate(operation, left, right, &value))
    result = integer_value_copy(value);
  free(first);
  free(second);
  return value_result(result, caller);
}

static char *FuncAdd(int *NbArg, long *TabArg)
{
  return calculate_value(NbArg, TabArg, SCRIPT_VALUE_ADD, __func__);
}

static char *FuncMult(int *NbArg, long *TabArg)
{
  return calculate_value(NbArg, TabArg, SCRIPT_VALUE_MULTIPLY, __func__);
}

static char *FuncDiv(int *NbArg, long *TabArg)
{
  return calculate_value(NbArg, TabArg, SCRIPT_VALUE_DIVIDE, __func__);
}

static char *RemainderOfDiv(int *NbArg, long *TabArg)
{
  return calculate_value(NbArg, TabArg, SCRIPT_VALUE_REMAINDER, __func__);
}

/* StrCopy retains one-based inclusive positions and clamps values below one. */
static char *FuncStrCopy(int *NbArg, long *TabArg)
{
  char *text, *first_text, *last_text, *result = NULL;
  int first, last;

  (*NbArg)++;
  text = CalcArg(TabArg, NbArg);
  (*NbArg)++;
  first_text = CalcArg(TabArg, NbArg);
  (*NbArg)++;
  last_text = CalcArg(TabArg, NbArg);
  if (script_value_integer(first_text, &first) &&
      script_value_integer(last_text, &last))
    result = script_value_copy(text, first, last);
  free(text);
  free(first_text);
  free(last_text);
  return value_result(result, __func__);
}

/* Launching a script with pipe */
static char *LaunchScript (int *NbArg,long *TabArg)
{
  char *argument, *selection = NULL, *command, *result;
  int slot, suffix, child;
  Bool reserved;

  (*NbArg)++;
  argument = CalcArg(TabArg, NbArg);
  if (!FvwmScriptExecutionAllowed() || x11base->NbChild < 0 || x11base->NbChild >= 98 ||
      x11base->TabScriptId[1] == NULL ||
      strlen(argument) > SCRIPT_COMMAND_MAX_BYTES ||
      argument[strspn(argument, " \t\r\n")] == '\0')
    goto rejected;
  slot = x11base->NbChild + 2;
  /* Two hex digits are part of the existing parent/child ID contract.
   * Include launches awaiting ownership so one parent cannot reuse an ID. */
  for (suffix = 16; suffix <= 255; suffix++)
  {
    free(selection);
    xasprintf(&selection, "%s%x", x11base->TabScriptId[1], suffix);
    reserved = False;
    for (child = 2; child < slot; child++)
      if (x11base->TabScriptId[child] != NULL &&
          strcmp(selection, x11base->TabScriptId[child]) == 0)
      {
        reserved = True;
        break;
      }
    if (!reserved &&
        XGetSelectionOwner(dpy, XInternAtom(dpy, selection, False)) == None)
      break;
  }
  if (suffix > 255)
    goto rejected;
  command = script_launch_command(module->name, argument, selection);
  if (command == NULL)
    goto rejected;
  x11base->TabScriptId[slot] = selection;
  x11base->NbChild++;
  SendText(fd, command, ref);
  xasprintf(&result, "%d", slot);
  free(command);
  free(argument);
  return result;
rejected:
  fvwm_debug(__func__, "[%s]: script launch exceeds limits or is invalid",
             ScriptName);
  free(selection);
  free(argument);
  return fxstrdup("-1");
}

/* GetScriptFather */
static char *GetScriptFather (int *NbArg,long *TabArg)
{
  return fxstrdup("0");
}

/* GetTime */
static char *GetTime (int *NbArg,long *TabArg)
{
  char *str;
  time_t t;

  t = time(NULL);
  str = integer_value_copy((long long)t - x11base->BeginTime);
  return str;
}

/* GetScriptArg */
static char *GetScriptArg (int *NbArg,long *TabArg)
{
  char *str;
  int val1;

  (*NbArg)++;
  str = CalcArg(TabArg,NbArg);
  char *end;
  errno = 0;
  long index = strtol(str, &end, 10);
  if (errno != 0 || end == str || *end != '\0' || index < 0 ||
      index >= (long)(sizeof(x11base->TabArg) / sizeof(x11base->TabArg[0])))
  {
    free(str);
    return fxstrdup("");
  }
  val1 = index;
  free(str);

  return bounded_value_copy(x11base->TabArg[val1]);
}

/* ReceivFromScript */
static char *ReceivFromScript (int *NbArg,long *TabArg)
{
  char *arg,*msg;
  int send;
  Atom AReceiv,ASend;
  char *received;

  (*NbArg)++;
  arg = CalcArg(TabArg,NbArg);
  char *end;
  errno = 0;
  long index = strtol(arg, &end, 10);
  if (errno != 0 || end == arg || *end != '\0' || index < 0 || index >= 100)
  {
    free(arg);
    return fxstrdup("No message");
  }
  send = index;
  free(arg);

  msg = fxstrdup("No message");

  /* Get atoms */
  AReceiv = XInternAtom(dpy,x11base->TabScriptId[1],True);
  if (AReceiv == None)
  {
    fvwm_debug(__func__,
               "[%s][ReceivFromScript]: <<WARNING>> Error with atome\n",
               ScriptName);
    return msg;
  }

  if ( (send >=0 ) && (send < 100))
  {
    if (x11base->TabScriptId[send]!=NULL)
    {
      ASend=XInternAtom(dpy,x11base->TabScriptId[send],True);
      if (ASend==None)
	fvwm_debug(__func__,
                   "[%s][ReceivFromScript]: <<WARNING>> Error with atome\n",
                   ScriptName);
    }
    else
      return msg;
  }
  else
    return msg;

  received = FSelectionRequestTextWithService(dpy, ASend, AReceiv, propriete,
    CurrentTime, FSELECTION_TIMEOUT_MS, FvwmScriptModalService, NULL);
  if (received != NULL)
  {
    if (received[0] != '\0')
    {
      free(msg);
      msg = bounded_value_copy(received);
      free(received);
    }
    else
      free(received);
  }
  return msg;
}

/* GetPid */
static char *FuncGetPid(int *NbArg,long *TabArg)
{
  char *str;
  pid_t pid;

  pid = getpid();
  str = integer_value_copy(pid);
  return str;
}

/* SendMsgAndGet */
static bool fifo_cancelled(void *context)
{
  (void)context;
  return isTerminated != 0;
}

static char *FuncSendMsgAndGet(int *NbArg,long *TabArg)
{
  char *com_name, *command, *mode_text, *reply = NULL;
  size_t length;
  int mode;

  (*NbArg)++;
  com_name = CalcArg(TabArg, NbArg);
  (*NbArg)++;
  command = CalcArg(TabArg, NbArg);
  (*NbArg)++;
  mode_text = CalcArg(TabArg, NbArg);
  if (!FvwmScriptExecutionAllowed() ||
      !script_index_parse(mode_text, 0, 2, &mode))
  {
    free(com_name);
    free(command);
    free(mode_text);
    return fxstrdup("0");
  }
  free(mode_text);
  setFvwmUserDir();
  bool success = script_fifo_exchange(FvwmUserDir, com_name, command, mode,
      x11base->fifo_timeout_ms, fifo_cancelled, NULL, &reply, &length);
  if (!success)
    fvwm_debug(__func__, "[%s]: FIFO communication failed: %s",
               ScriptName, strerror(errno));
  free(com_name);
  free(command);
  if (!success || mode != 1)
    return fxstrdup(success ? "1" : "0");
  char *result = bounded_value_copy(reply);
  free(reply);
  return result;
}

/* Parse the complete four-digit length-framed string. */
static char *FuncParse(int *NbArg,long *TabArg)
{
  char *text, *position, *result = NULL;
  int index;

  (*NbArg)++;
  text = CalcArg(TabArg, NbArg);
  (*NbArg)++;
  position = CalcArg(TabArg, NbArg);
  if (script_value_integer(position, &index))
    result = script_value_record(text, index);
  free(text);
  free(position);
  return value_result(result, __func__);
}

/* GetLastString */
static char *FuncGetLastString(int *NbArg,long *TabArg)
{
  char *str;

  if (LastString == NULL) {
    str = fxcalloc(1, sizeof(char));
    str[0] ='\0';
  }
  else
  {
    str = bounded_value_copy(LastString);
  }
  return str;
}

/*
 * Object Commands
 */

/* Exec */
static void Exec (int NbArg,long *TabArg)
{
  char *command = fxstrdup(""), *value;
  size_t length = 0;
  int i;

  /* Functions may have side effects: evaluation and assembly are one pass. */
  for (i = 0; i < NbArg; i++)
  {
    value = CalcArg(TabArg, &i);
    if (!FvwmScriptExecutionAllowed())
    {
      free(value);
      free(command);
      return;
    }
    if (strlen(value) > SCRIPT_COMMAND_MAX_BYTES - length)
    {
      free(value);
      fvwm_debug(__func__, "[%s]: Do command exceeds %u bytes",
                 ScriptName, SCRIPT_COMMAND_MAX_BYTES);
      free(command);
      return;
    }
    if (!bounded_value_append(&command, &length, value))
    {
      free(value);
      free(command);
      return;
    }
    free(value);
  }
  SendText(fd, command, ref);
  free(command);
}

/* Hide */
static void HideObj (int NbArg,long *TabArg)
{
  char *arg[1];
  int IdItem;
  int i = 0;

  arg[0] = CalcArg(TabArg,&i);
  IdItem = widget_index(arg[0], __func__);
  if (IdItem < 0)
  {
    free(arg[0]);
    return;
  }


  tabxobj[IdItem]->flags[0] = True;
  /* We hide the window */
  XUnmapWindow(dpy,tabxobj[IdItem]->win);
  free(arg[0]);
}

/* Show */
static void ShowObj (int NbArg,long *TabArg)
{
  char *arg[1];
  int IdItem;
  int i = 0;

  arg[0] = CalcArg(TabArg,&i);
  IdItem = widget_index(arg[0], __func__);
  if (IdItem < 0)
  {
    free(arg[0]);
    return;
  }

  tabxobj[IdItem]->flags[0] = False;
  XMapWindow(dpy,tabxobj[IdItem]->win);
  tabxobj[IdItem]->DrawObj(tabxobj[IdItem], NULL);
  free(arg[0]);
}

/* ChangeValue */
static void ChangeValue (int NbArg,long *TabArg)
{
  int i = 0;
  int IdItem, value;
  char *arg[2];

  arg[0] =CalcArg(TabArg,&i);
  i++;
  arg[1] =CalcArg(TabArg,&i);

  IdItem = widget_index(arg[0], __func__);
  if (IdItem < 0 || !widget_integer(arg[1], &value, __func__))
  {
    free(arg[0]);
    free(arg[1]);
    return;
  }
  tabxobj[IdItem]->value = value;
  /* Redraw the object to refresh it */
  if (tabxobj[IdItem]->TypeWidget != SwallowExec)
    XClearWindow(dpy, tabxobj[IdItem]->win);
  tabxobj[IdItem]->DrawObj(tabxobj[IdItem],NULL);
  free(arg[0]);
  free(arg[1]);
}

/* ChangeValueMax */
static void ChangeValueMax (int NbArg,long *TabArg)
{
  int i=0;
  char *arg[2];
  int IdItem, value;

  arg[0] = CalcArg(TabArg,&i);
  i++;
  arg[1] = CalcArg(TabArg,&i);

  IdItem = widget_index(arg[0], __func__);
  if (IdItem < 0 || !widget_integer(arg[1], &value, __func__))
  {
    free(arg[0]);
    free(arg[1]);
    return;
  }
  tabxobj[IdItem]->value3 = value;
  /* Redraw the object to refresh it */
  if (tabxobj[IdItem]->value > tabxobj[IdItem]->value3)
    tabxobj[IdItem]->value = value;
  if (tabxobj[IdItem]->TypeWidget != SwallowExec)
    XClearWindow(dpy, tabxobj[IdItem]->win);
  tabxobj[IdItem]->DrawObj(tabxobj[IdItem],NULL);
  free(arg[0]);
  free(arg[1]);
}

/* ChangeValueMin */
static void ChangeValueMin (int NbArg,long *TabArg)
{
  int i=0;
  char *arg[2];
  int IdItem, value;

  arg[0] = CalcArg(TabArg,&i);
  i++;
  arg[1] = CalcArg(TabArg,&i);

  IdItem = widget_index(arg[0], __func__);
  if (IdItem < 0 || !widget_integer(arg[1], &value, __func__))
  {
    free(arg[0]);
    free(arg[1]);
    return;
  }
  tabxobj[IdItem]->value2 = value;
  /* Redraw the object to refresh it */
  if (tabxobj[IdItem]->value < tabxobj[IdItem]->value2)
    tabxobj[IdItem]->value = value;
  if (tabxobj[IdItem]->TypeWidget != SwallowExec)
    XClearWindow(dpy, tabxobj[IdItem]->win);
  tabxobj[IdItem]->DrawObj(tabxobj[IdItem],NULL);
  free(arg[0]);
  free(arg[1]);
}

/* ChangePosition */
static void ChangePos (int NbArg,long *TabArg)
{
  int i=0;
  char *arg[3];
  int an[3];
  int IdItem;

  arg[0] = CalcArg(TabArg,&i);
  i++;
  arg[1] = CalcArg(TabArg,&i);
  i++;
  arg[2] = CalcArg(TabArg,&i);

  IdItem = widget_index(arg[0], __func__);
  if (IdItem < 0 || !widget_integer(arg[1], &an[1], __func__) ||
      !widget_integer(arg[2], &an[2], __func__))
  {
    free(arg[0]);
    free(arg[1]);
    free(arg[2]);
    return;
  }
  if (!script_geometry_position(an[1], an[2]))
  {
    script_geometry_diagnostic(__func__, tabxobj[IdItem]->id);
    free(arg[0]);
    free(arg[1]);
    free(arg[2]);
    return;
  }
  tabxobj[IdItem]->x = an[1];
  tabxobj[IdItem]->y = an[2];
  XMoveWindow(dpy,tabxobj[IdItem]->win,an[1],an[2]);

  free(arg[0]);
  free(arg[1]);
  free(arg[2]);
}

/* ChangeFont */
static void ChangeFont (int NbArg,long *TabArg)
{
  int i=0;
  char *arg[2];
  int IdItem;
  FlocaleFont *Ffont;
  script_layout_geometry geometry;

  arg[0] = CalcArg(TabArg,&i);
  i++;
  arg[1] = CalcArg(TabArg,&i);
  IdItem = widget_index(arg[0], __func__);
  if (IdItem < 0)
  {
    free(arg[0]);
    free(arg[1]);
    return;
  }
  if ((Ffont =
       FlocaleLoadFont(dpy, arg[1], ScriptName)) == NULL) {
    fvwm_debug(__func__, "[%s][ChangeFont]: Couldn't load font %s\n",
               ScriptName,arg[1]);
    free(arg[0]);
    free(arg[1]);
    return;
  }
  if (!script_widget_layout(tabxobj[IdItem], Ffont, &geometry))
  {
    script_geometry_diagnostic(__func__, tabxobj[IdItem]->id);
    FlocaleUnloadFont(dpy, Ffont);
    free(arg[0]);
    free(arg[1]);
    return;
  }
  else
  {
    tabxobj[IdItem]->resource_revision++;
    free(tabxobj[IdItem]->font);
    tabxobj[IdItem]->font = fxstrdup(arg[1]);
    FlocaleUnloadFont(dpy, tabxobj[IdItem]->Ffont);
    tabxobj[IdItem]->Ffont = Ffont;
    if (Ffont->font != NULL)
      XSetFont(dpy,tabxobj[IdItem]->gc,tabxobj[IdItem]->Ffont->font->fid);
  }

  if (tabxobj[IdItem]->TypeWidget != SwallowExec)
    XClearWindow(dpy, tabxobj[IdItem]->win);
  tabxobj[IdItem]->DrawObj(tabxobj[IdItem],NULL);
  free(arg[0]);
  free(arg[1]);
}

/* ChangeSize */
static void ChangeSize (int NbArg,long *TabArg)
{
  int i=0;
  char *arg[3];
  int an[3];
  int IdItem;

  arg[0] = CalcArg(TabArg,&i);
  i++;
  arg[1] = CalcArg(TabArg,&i);
  i++;
  arg[2] = CalcArg(TabArg,&i);

  IdItem = widget_index(arg[0], __func__);
  if (IdItem < 0 || !widget_integer(arg[1], &an[1], __func__) ||
      !widget_integer(arg[2], &an[2], __func__))
  {
    free(arg[0]);
    free(arg[1]);
    free(arg[2]);
    return;
  }
  if (!script_geometry_size(an[1], an[2]))
  {
    script_geometry_diagnostic(__func__, tabxobj[IdItem]->id);
    free(arg[0]);
    free(arg[1]);
    free(arg[2]);
    return;
  }
  tabxobj[IdItem]->width = an[1];
  tabxobj[IdItem]->height = an[2];
  XResizeWindow(dpy,tabxobj[IdItem]->win,an[1],an[2]);
  tabxobj[IdItem]->DrawObj(tabxobj[IdItem],NULL);
  free(arg[0]);
  free(arg[1]);
  free(arg[2]);
}

/* ChangeTitle */
static void ChangeTitle (int NbArg,long *TabArg)
{
  int i=0;
  char *arg[2];
  int IdItem;

  arg[0] = CalcArg(TabArg,&i);
  i++;
  arg[1] = CalcArg(TabArg,&i);
  IdItem = widget_index(arg[0], __func__);
  if (IdItem < 0)
  {
    free(arg[0]);
    free(arg[1]);
    return;
  }
  tabxobj[IdItem]->resource_revision++;
  free(tabxobj[IdItem]->title);
  tabxobj[IdItem]->title=fxstrdup(arg[1]);
  if (tabxobj[IdItem]->TypeWidget != SwallowExec)
    XClearWindow(dpy, tabxobj[IdItem]->win);
  tabxobj[IdItem]->DrawObj(tabxobj[IdItem],NULL);
  free(arg[0]);
  free(arg[1]);
}

/* ChangeLocaleTitle */
static void ChangeLocaleTitle (int NbArg,long *TabArg)
{
  int i=0;
  char *arg[2];
  int IdItem;

  arg[0] = CalcArg(TabArg,&i);
  i++;
  arg[1] = CalcArg(TabArg,&i);
  IdItem = widget_index(arg[0], __func__);
  if (IdItem < 0)
  {
    free(arg[0]);
    free(arg[1]);
    return;
  }
  char *title = bounded_value_copy(FGettext(arg[1]));
  if (!FvwmScriptExecutionAllowed())
  {
    free(title);
    free(arg[0]);
    free(arg[1]);
    return;
  }
  tabxobj[IdItem]->resource_revision++;
  free(tabxobj[IdItem]->title);
  tabxobj[IdItem]->title = title;
  if (tabxobj[IdItem]->TypeWidget != SwallowExec)
    XClearWindow(dpy, tabxobj[IdItem]->win);
  tabxobj[IdItem]->DrawObj(tabxobj[IdItem],NULL);
  free(arg[0]);
  free(arg[1]);
}

/* ChangeIcon */
static void ChangeIcon (int NbArg,long *TabArg)
{
	int i=0;
	char *arg[2];
	int IdItem;

	arg[0] = CalcArg(TabArg,&i);
	i++;
	arg[1] = CalcArg(TabArg,&i);
	IdItem = widget_index(arg[0], __func__);
  if (IdItem < 0)
  {
    free(arg[0]);
    free(arg[1]);
    return;
  }
	if (tabxobj[IdItem]->icon)
	{
		free(tabxobj[IdItem]->icon);
	}
	tabxobj[IdItem]->icon = fxstrdup(arg[1]);
	LoadIcon(tabxobj[IdItem]);
	if (tabxobj[IdItem]->TypeWidget != SwallowExec)
	{
		XClearWindow(dpy, tabxobj[IdItem]->win);
	}
	tabxobj[IdItem]->DrawObj(tabxobj[IdItem],NULL);
	free(arg[0]);
	free(arg[1]);
}

/* ChangeForeColor */
static void ChangeForeColor (int NbArg,long *TabArg)
{
  int i=0;
  char *arg[2];
  int IdItem;

  arg[0] = CalcArg(TabArg,&i);
  i++;
  arg[1] = CalcArg(TabArg,&i);
  IdItem = widget_index(arg[0], __func__);
  if (IdItem < 0)
  {
    free(arg[0]);
    free(arg[1]);
    return;
  }

  /* Free color */
  if (tabxobj[IdItem]->colorset < 0)
    PictureFreeColors(
	    dpy,Pcmap,(void*)(&(tabxobj[IdItem])->TabColor[fore]),1,0, True);

  if (tabxobj[IdItem]->forecolor)
  {
	  free(tabxobj[IdItem]->forecolor);
  }
  tabxobj[IdItem]->forecolor= fxstrdup(arg[1]);

  tabxobj[IdItem]->TabColor[fore] = GetColor(tabxobj[IdItem]->forecolor);
  if (tabxobj[IdItem]->colorset >= 0) {
    tabxobj[IdItem]->TabColor[back] = GetColor(tabxobj[IdItem]->backcolor);
    tabxobj[IdItem]->TabColor[hili] = GetColor(tabxobj[IdItem]->hilicolor);
    tabxobj[IdItem]->TabColor[shad] = GetColor(tabxobj[IdItem]->shadcolor);
  }
  tabxobj[IdItem]->colorset = -1;

  if (tabxobj[IdItem]->TypeWidget != SwallowExec)
    XClearWindow(dpy, tabxobj[IdItem]->win);
  tabxobj[IdItem]->DrawObj(tabxobj[IdItem],NULL);

  free(arg[0]);
  free(arg[1]);

}

/* ChangeBackColor */
static void ChangeBackColor (int NbArg,long *TabArg)
{
  int i=0;
  char *arg[2];
  int IdItem;

  arg[0] = CalcArg(TabArg,&i);
  i++;
  arg[1] = CalcArg(TabArg,&i);
  IdItem = widget_index(arg[0], __func__);
  if (IdItem < 0)
  {
    free(arg[0]);
    free(arg[1]);
    return;
  }

  /* Free color */
  if (tabxobj[IdItem]->colorset < 0)
    PictureFreeColors(
	    dpy,Pcmap,(void*)(&(tabxobj[IdItem])->TabColor[back]),1,0,True);

  if (tabxobj[IdItem]->backcolor)
  {
	  free(tabxobj[IdItem]->backcolor);
  }
  tabxobj[IdItem]->backcolor= fxstrdup(arg[1]);

  tabxobj[IdItem]->TabColor[back] = GetColor(tabxobj[IdItem]->backcolor);
  if (tabxobj[IdItem]->colorset >= 0) {
    tabxobj[IdItem]->TabColor[fore] = GetColor(tabxobj[IdItem]->forecolor);
    tabxobj[IdItem]->TabColor[hili] = GetColor(tabxobj[IdItem]->hilicolor);
    tabxobj[IdItem]->TabColor[shad] = GetColor(tabxobj[IdItem]->shadcolor);
  }
  tabxobj[IdItem]->colorset = -1;

  if (tabxobj[IdItem]->TypeWidget != SwallowExec) {
    XSetWindowBackground(dpy,
			 tabxobj[IdItem]->win,
			 tabxobj[IdItem]->TabColor[back]);
    XClearWindow(dpy, tabxobj[IdItem]->win);
  }
  tabxobj[IdItem]->DrawObj(tabxobj[IdItem],NULL);

  free(arg[0]);
  free(arg[1]);
}

/* ChangeMainColorset */
static void ChangeMainColorset (int i)
{
  if (!AllocColorset(i))
  {
    return;
  }
 /* Free color */
  if (x11base->colorset < 0) {
    PictureFreeColors(dpy,Pcmap,&x11base->TabColor[fore],1,0,True);
    PictureFreeColors(dpy,Pcmap,&x11base->TabColor[back],1,0,True);
    PictureFreeColors(dpy,Pcmap,&x11base->TabColor[hili],1,0,True);
    PictureFreeColors(dpy,Pcmap,&x11base->TabColor[shad],1,0,True);
  }
  x11base->colorset = i;
  x11base->TabColor[fore] = GetColorset(i)->fg;
  x11base->TabColor[back] = GetColorset(i)->bg;
  x11base->TabColor[hili] = GetColorset(i)->hilite;
  x11base->TabColor[shad] = GetColorset(i)->shadow;
  SetWindowBackground(dpy, x11base->win,x11base->size.width,x11base->size.height,
		      GetColorset(i), Pdepth, x11base->gc, True);
}

/* ChangeMainColorset */
static void ChangeColorset (int NbArg,long *TabArg)
{
  int i=0;
  char *arg[2];
  int IdItem;
  int widget_id, colorset;

  arg[0] = CalcArg(TabArg,&i);
  i++;
  arg[1] = CalcArg(TabArg,&i);
  if (!FvwmScriptExecutionAllowed() ||
      !script_index_parse(arg[0], 0, 1000, &widget_id) ||
      !script_index_parse(arg[1], 0, INT_MAX, &colorset))
  {
    free(arg[0]);
    free(arg[1]);
    return;
  }
  if (widget_id == 0) {
    ChangeMainColorset(colorset);
    free(arg[0]);
    free(arg[1]);
    return;
  }
  IdItem = widget_index(arg[0], __func__);
  if (IdItem < 0)
  {
    free(arg[0]);
    free(arg[1]);
    return;
  }

  if (!AllocColorset(colorset))
  {
    free(arg[0]);
    free(arg[1]);
    return;
  }

  /* Free color */
  if (tabxobj[IdItem]->colorset < 0) {
    PictureFreeColors(dpy,Pcmap,&tabxobj[IdItem]->TabColor[fore],1,0,True);
    PictureFreeColors(dpy,Pcmap,&tabxobj[IdItem]->TabColor[back],1,0,True);
    PictureFreeColors(dpy,Pcmap,&tabxobj[IdItem]->TabColor[hili],1,0,True);
    PictureFreeColors(dpy,Pcmap,&tabxobj[IdItem]->TabColor[shad],1,0,True);
  }
  i = colorset;
  tabxobj[IdItem]->colorset = i;
  tabxobj[IdItem]->TabColor[fore] = GetColorset(i)->fg;
  tabxobj[IdItem]->TabColor[back] = GetColorset(i)->bg;
  tabxobj[IdItem]->TabColor[hili] = GetColorset(i)->hilite;
  tabxobj[IdItem]->TabColor[shad] = GetColorset(i)->shadow;

  if (tabxobj[IdItem]->TypeWidget != SwallowExec) {
    SetWindowBackground(dpy, tabxobj[IdItem]->win, tabxobj[IdItem]->width,
			tabxobj[IdItem]->height, GetColorset(i), Pdepth,
			tabxobj[IdItem]->gc, False);
    XClearWindow(dpy, tabxobj[IdItem]->win);
  }
  tabxobj[IdItem]->DrawObj(tabxobj[IdItem],NULL);

  free(arg[0]);
  free(arg[1]);
}

/* ChangeWindowTitle */
static void ChangeWindowTitle(int NbArg,long * TabArg){

  char *arg;
  int tmpVal=NbArg-1;

  arg=CalcArg(TabArg,&tmpVal);
  if (!FvwmScriptExecutionAllowed())
  {
    free(arg);
    return;
  }
  XChangeProperty(
	  dpy, x11base->win, XA_WM_NAME, XA_STRING, 8, PropModeReplace,
	  (unsigned char*)arg, strlen(arg));
  free(arg);
}


/* ChangeWindowTitleFromArg */
static void ChangeWindowTitleFromArg(int NbArg,long * TabArg){

  char *arg;
  int argVal;
  int tmpVal=NbArg-1;

  arg=CalcArg(TabArg,&tmpVal);
  if (!script_index_parse(arg, 0, 19, &argVal))
    argVal = -1;
  free(arg);

  arg = bounded_value_copy(argVal >= 0 ? x11base->TabArg[argVal] : "");
  if (!FvwmScriptExecutionAllowed())
  {
    free(arg);
    return;
  }

  XChangeProperty(
	  dpy, x11base->win, XA_WM_NAME, XA_STRING, 8, PropModeReplace,
	  (unsigned char*)arg, strlen(arg));
  free(arg);
}


/* SetVar */
static void SetVar (int NbArg,long *TabArg)
{
  char *str = fxstrdup(""), *value;
  size_t length = 0;
  size_t old_bytes = strlen(TabVVar[TabArg[0]]) + 1;
  script_storage *storage = script_storage_runtime();
  int i;

  for (i = 1; i < NbArg; i++)
  {
    size_t added;
    value = CalcArg(TabArg, &i);
    added = strlen(value);
    if (!FvwmScriptExecutionAllowed() || added > SIZE_MAX - 1 - length ||
        !storage_allowed(script_storage_check_replace(storage, old_bytes,
                                                        length + added)))
    {
      free(value);
      free(str);
      return;
    }
    if (!script_text_append(&str, &length, value, storage->value_limit))
    {
      FvwmScriptStopForStorage(False);
      free(value);
      free(str);
      return;
    }
    free(value);
  }
  if (!storage_allowed(script_storage_replace(storage, old_bytes, length)))
  {
    free(str);
    return;
  }
  free(TabVVar[TabArg[0]]);
  TabVVar[TabArg[0]] = str;
}

/* SendSign */
static void SendSign (int NbArg,long *TabArg)
{
  int i=0;
  char *arg[2];
  int IdItem;
  int TypeMsg;

  arg[0] = CalcArg(TabArg,&i);
  i++;
  arg[1] = CalcArg(TabArg,&i);
  IdItem = widget_index(arg[0], __func__);
  if (IdItem < 0)
  {
    free(arg[0]);
    free(arg[1]);
    return;
  }
  TypeMsg = atoi(arg[1]);
  SendMsg(tabxobj[IdItem],TypeMsg);
  free(arg[0]);
  free(arg[1]);
}

/* MyWarpPointer */
static void MyWarpPointer(int NbArg,long *TabArg)
{
  int i=0;
  char *arg;
  int IdItem;

  arg=CalcArg(TabArg,&i);
  IdItem = widget_index(arg, __func__);
  if (IdItem < 0)
  {
    free(arg);
    return;
  }
  /* Move pointer toward object */
  FWarpPointer(dpy, None, tabxobj[IdItem]->win, 0, 0, 0, 0,
	       tabxobj[IdItem]->width / 2, tabxobj[IdItem]->height + 10);
  free(arg);
}

/* Quit */
void Quit (int NbArg,long *TabArg)
{
  exit(0);
}

/* IfThen */
static void IfThen (int NbArg,long *TabArg)
{
  char *arg[10];
  int i,j;
  int CurrArg=0;
  int IdFuncComp = 0;

  /* Checking the condition */
  for (j=0; j<NbArg-2; j++)
  {
    if (TabArg[j] > 100000)     /* Number coding case */
    {
      if (!script_value_decode_integer(TabArg[j], &i))
        fvwm_debug(__func__, "[%s]: invalid numeric argument tag", ScriptName);
      arg[CurrArg] = integer_value_copy(i);
      CurrArg++;
    }
    else if (TabArg[j] < -200000)/* Comparison function ID case */
    {
      IdFuncComp = TabArg[j] + 250000;
    }
    else if (TabArg[j] < -100000)       /* Function ID case */
    {
      arg[CurrArg] = TabFunc[TabArg[j]+150000](&j,TabArg);
      CurrArg++;
    }
    else                                /* Variable case */
    {
      arg[CurrArg] = bounded_value_copy(TabVVar[TabArg[j]]);
      CurrArg++;
    }
  }

  /* Comparing the arguments */
  for (i = 0; i < CurrArg; i++)
    storage_allowed(script_storage_check_value(script_storage_runtime(),
                                               strlen(arg[i])));
  if (!FvwmScriptExecutionAllowed())
  {
    free(arg[0]);
    free(arg[1]);
    return;
  }
  if (TabComp[IdFuncComp](arg[0],arg[1]))
    ExecBloc((Bloc*)TabArg[NbArg-2]);
  else if (TabArg[NbArg-1]!=0)
    ExecBloc((Bloc*)TabArg[NbArg-1]);

  free(arg[0]);
  free(arg[1]);
}

/** Loop Instruction **/
static void Loop (int NbArg,long *TabArg)
{
  char *arg[2] = {NULL, NULL};
  int limits[2], count = 0;

  /* The last encoded entry owns the body, not a third numeric argument. */
  for (int i = 1; i < NbArg - 1; i++)
  {
    if (count >= 2)
      goto invalid;
    arg[count++] = CalcArg(TabArg, &i);
  }
  if (!FvwmScriptExecutionAllowed() || count != 2 ||
      !script_value_integer(arg[0], &limits[0]) ||
      !script_value_integer(arg[1], &limits[1]))
    goto invalid;
  free(arg[0]);
  free(arg[1]);
  /* Retain the legacy ascending-only range. A wider counter can advance
   * beyond INT_MAX without wrapping. The body may replace the variable's
   * allocation, so publish a newly owned value on every iteration. */
  for (int64_t value = limits[0]; value <= limits[1]; value++)
  {
    char buffer[32];
    char *text;
    if (!FvwmScriptExecutionStep())
      break;
    size_t length = snprintf(buffer, sizeof(buffer), "%d", (int)value);
    if (!storage_allowed(script_storage_replace(script_storage_runtime(),
                         strlen(TabVVar[TabArg[0]]) + 1, length)))
      break;
    text = fxstrdup(buffer);
    free(TabVVar[TabArg[0]]);
    TabVVar[TabArg[0]] = text;
    ExecBloc((Bloc *)TabArg[NbArg - 1]);
  }
  return;
invalid:
  free(arg[0]);
  free(arg[1]);
  fvwm_debug(__func__, "[%s]: invalid loop bounds", ScriptName);
}

/** While Instruction **/
static void While (int NbArg,long *TabArg)
{
  char *arg[3],*str;
  int i;
  int Loop=1;
  int IdFuncComp;

  while (Loop)
  {
    if (!FvwmScriptExecutionStep())
      break;
    i = 0;
    arg[0] = CalcArg(TabArg,&i);
    i++;
    str = CalcArg(TabArg,&i);
    IdFuncComp = atoi(str);
    free(str);
    i++;
    arg[1] = CalcArg(TabArg,&i);

    if (!FvwmScriptExecutionAllowed() || IdFuncComp < 1 || IdFuncComp > 6)
    {
      fvwm_debug(__func__, "[%s]: invalid comparison function", ScriptName);
      free(arg[0]);
      free(arg[1]);
      return;
    }
    Loop=TabComp[IdFuncComp](arg[0],arg[1]);
    if (Loop) ExecBloc((Bloc*)TabArg[NbArg-1]);
    free(arg[0]);
    free(arg[1]);
  }
}

/* WriteToFile */
static void WriteToFile (int NbArg,long *TabArg)
{
  int i = 0;
  char *path, *text = fxstrdup(""), *value;
  size_t length = 0;

  path = CalcArg(TabArg, &i);
  for (i++; i < NbArg; i++)
  {
    size_t added;

    value = CalcArg(TabArg, &i);
    added = strlen(value);
    if (!FvwmScriptExecutionAllowed())
    {
      free(value);
      goto done;
    }
    if (added > SCRIPT_TEXT_MAX_BYTES - length)
    {
      free(value);
      fvwm_debug(__func__, "[%s]: WriteToFile text exceeds 64 KiB",
                 ScriptName);
      goto done;
    }
    if (!bounded_value_append(&text, &length, value))
    {
      free(value);
      goto done;
    }
    free(value);
  }
  if (!FvwmScriptExecutionAllowed())
    goto done;
  if (path[0] != '/')
  {
    const char *home = getenv("HOME");
    char *relative = path;

    if (home == NULL || home[0] == '\0')
      goto done;
    xasprintf(&path, "%s/%s", home, relative);
    free(relative);
  }
  if (script_file_update(path, ScriptName, getpid(), text) < 0)
    fvwm_debug(__func__, "[%s]: WriteToFile %s: %s", ScriptName,
               path, strerror(errno));
done:
  free(path);
  free(text);
}

/* SendToScript */
static void SendToScript (int NbArg,long *TabArg)
{
  char *tempstr,*Msg,*R;
  int dest;
  int j=0;
  size_t length = 0;
  Atom myatom;

  /* Compute Receiver */
  tempstr=CalcArg(TabArg,&j);
  char *end;
  errno = 0;
  long index = strtol(tempstr, &end, 10);
  if (!FvwmScriptExecutionAllowed() || errno != 0 || end == tempstr || *end != '\0' || index < 0 ||
      index >= (long)(sizeof(x11base->TabScriptId) /
                      sizeof(x11base->TabScriptId[0])) ||
      x11base->TabScriptId[index] == NULL)
  {
    fvwm_debug(__func__, "[%s]: invalid SendToScript destination",
               ScriptName);
    free(tempstr);
    return;
  }
  dest = index;
  free(tempstr);

  /* Compute what's inside */
  Msg=fxstrdup("");
  for (j=1;j<NbArg;j++)
  {
    tempstr=CalcArg(TabArg,&j);
    if (!FvwmScriptExecutionAllowed() ||
        strlen(tempstr) >= FSELECTION_TEXT_MAX_BYTES - length)
    {
      fvwm_debug(__func__, "[%s]: SendToScript message exceeds selection cap",
                 ScriptName);
      free(tempstr);
      free(Msg);
      return;
    }
    if (!bounded_value_append(&Msg, &length, tempstr))
    {
      free(tempstr);
      free(Msg);
      return;
    }
    free(tempstr);
  }

  /* Compute Receiver */
  R=fxstrdup(x11base->TabScriptId[dest]);
  myatom=XInternAtom(dpy,R,True);

  if ((BuffSend.NbMsg<40)&&(XGetSelectionOwner(dpy,myatom)!=None))
  {
    /* Storing into message buffer */
    BuffSend.TabMsg[BuffSend.NbMsg].Msg=Msg;
    /* Storing into receiver buffer */
    BuffSend.TabMsg[BuffSend.NbMsg].R=R;
    /* Storing the message */
    BuffSend.NbMsg++;

    /* Waking the receiver up */
    XConvertSelection(dpy,
		      XInternAtom(dpy,x11base->TabScriptId[dest],True),
		      propriete, propriete, x11base->win, CurrentTime);
  }
  else
  {
    fvwm_debug(__func__, "[%s][SendToScript]: Too many messages sended\n",
               ScriptName);
    free(Msg);
    free(R);
  }
}

/* Key */
static void Key (int NbArg,long *TabArg)
{
  char *key_string,*in_modifier,*action,*str,*tmp,*widget,*sig;
  int modifier;
  int error = 0;
  int i,j=0;
  KeySym keysym = NoSymbol;
  size_t length = 0;

  /* the key */
  key_string = CalcArg(TabArg,&j);
  /* the modifier */
  j++;
  in_modifier = CalcArg(TabArg,&j);
  /* the widget */
  j++;
  widget = CalcArg(TabArg,&j);
  /* the signal */
  j++;
  sig = CalcArg(TabArg,&j);

  /* the string */
  str = fxstrdup("");
  for (i=j+1; i<NbArg; i++)
  {
    tmp = CalcArg(TabArg,&i);
    if (!bounded_value_append(&str, &length, tmp))
    {
      fvwm_debug(__func__, "[%s]: key string size overflow", ScriptName);
      free(tmp);
      free(str);
      free(sig);
      free(widget);
      free(key_string);
      free(in_modifier);
      return;
    }
    free(tmp);
  }

  if (!FvwmScriptExecutionAllowed())
  {
    free(sig);
    free(widget);
    free(str);
    free(key_string);
    free(in_modifier);
    return;
  }
  xasprintf(&tmp, "%s %s", widget, sig);
  xasprintf(&action, "%s %s", tmp, str);

  free(sig);
  free(widget);
  free(str);
  free(tmp);

  keysym = FvwmStringToKeysym(dpy, key_string);
  if (keysym == 0)
  {
    fvwm_debug(__func__, "[%s][Key]: No such key: %s", ScriptName, key_string);
    error = 1;
  }

  if (modifiers_string_to_modmask(in_modifier, &modifier)) {
    fvwm_debug(__func__, "[%s][Key]: bad modifier: %s\n",
               ScriptName,in_modifier);
    error = 1;
  }

  if (error) {
    free(key_string);
    free(in_modifier);
    free(action);
    return;
  }

  AddBinding(
	  dpy, &BindingsList, BIND_KEYPRESS, 0, keysym,
	  key_string, modifier, C_WINDOW, (void *)action, NULL, NULL);
  free(key_string);
  free(in_modifier);
}

/*
 * GetText Support
 */

static char *FuncGettext(int *NbArg,long *TabArg)
{
	char *str;
	char *string;

	(*NbArg)++;
	str = CalcArg(TabArg,NbArg);

	string = bounded_value_copy(FGettext(str));
	free(str);
	return (char *)string;
}

/*
 * Initializing TabCom and TabFunc
 */
void InitCom(void)
{
  /* Command */
  TabCom[1]=Exec;
  TabCom[2]=HideObj;
  TabCom[3]=ShowObj;
  TabCom[4]=ChangeValue;
  TabCom[5]=ChangePos;
  TabCom[6]=ChangeSize;
  TabCom[7]=ChangeIcon;
  TabCom[8]=ChangeTitle;
  TabCom[9]=ChangeFont;
  TabCom[10]=ChangeForeColor;
  TabCom[11]=SetVar;
  TabCom[12]=SendSign;
  TabCom[13]=Quit;
  TabCom[14]=IfThen;
  TabCom[15]=Loop;
  TabCom[16]=While;
  TabCom[17]=MyWarpPointer;
  TabCom[18]=WriteToFile;
  TabCom[19]=ChangeBackColor;
  TabCom[21]=ChangeValueMax;
  TabCom[22]=ChangeValueMin;
  TabCom[23]=SendToScript;
  TabCom[24]=ChangeColorset;
  TabCom[25]=Key;
  TabCom[26]=ChangeLocaleTitle;
  TabCom[27]=ChangeWindowTitle;
  TabCom[28]=ChangeWindowTitleFromArg;

  /* Function */
  TabFunc[1]=FuncGetValue;
  TabFunc[2]=FuncGetTitle;
  TabFunc[3]=FuncGetOutput;
  TabFunc[4]=FuncNumToHex;
  TabFunc[5]=FuncHexToNum;
  TabFunc[6]=FuncAdd;
  TabFunc[7]=FuncMult;
  TabFunc[8]=FuncDiv;
  TabFunc[9]=FuncStrCopy;
  TabFunc[10]=LaunchScript;
  TabFunc[11]=GetScriptFather;
  TabFunc[12]=ReceivFromScript;
  TabFunc[13]=RemainderOfDiv;
  TabFunc[14]=GetTime;
  TabFunc[15]=GetScriptArg;
  TabFunc[16]=FuncGetFore;
  TabFunc[17]=FuncGetBack;
  TabFunc[18]=FuncGetHili;
  TabFunc[19]=FuncGetShad;
  TabFunc[20]=FuncGetMinValue;
  TabFunc[21]=FuncGetMaxValue;
  TabFunc[22]=FuncGetPid;
  TabFunc[23]=FuncSendMsgAndGet;
  TabFunc[24]=FuncParse;
  TabFunc[25]=FuncGetLastString;
  TabFunc[26]=FuncGettext;

  /* Comparison Function */
  TabComp[1]=Inf;
  TabComp[2]=InfEq;
  TabComp[3]=Equal;
  TabComp[4]=SupEq;
  TabComp[5]=Sup;
  TabComp[6]=Diff;
}
