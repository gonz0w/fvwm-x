/* A Bison parser, made by GNU Bison 3.8.2.  */

/* Bison implementation for Yacc-like parsers in C

   Copyright (C) 1984, 1989-1990, 2000-2015, 2018-2021 Free Software Foundation,
   Inc.

   This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <https://www.gnu.org/licenses/>.  */

/* As a special exception, you may create a larger work that contains
   part or all of the Bison parser skeleton and distribute that work
   under terms of your choice, so long as that work isn't itself a
   parser generator using the skeleton or a modified version thereof
   as a parser skeleton.  Alternatively, if you modify or redistribute
   the parser skeleton itself, you may (at your option) remove this
   special exception, which will cause the skeleton and the resulting
   Bison output files to be licensed under the GNU General Public
   License without this special exception.

   This special exception was added by the Free Software Foundation in
   version 2.2 of Bison.  */

/* C LALR(1) parser skeleton written by Richard Stallman, by
   simplifying the original so-called "semantic" parser.  */

/* DO NOT RELY ON FEATURES THAT ARE NOT DOCUMENTED in the manual,
   especially those whose name start with YY_ or yy_.  They are
   private implementation details that can be changed or removed.  */

/* All symbols defined below should begin with yy or YY, to avoid
   infringing on user name space.  This should be done even for local
   variables, as they might otherwise be expanded by user macros.
   There are some unavoidable exceptions within include files to
   define necessary library symbols; they are noted "INFRINGES ON
   USER NAME SPACE" below.  */

/* Identify Bison output, and Bison version.  */
#define YYBISON 30802

/* Bison version string.  */
#define YYBISON_VERSION "3.8.2"

/* Skeleton name.  */
#define YYSKELETON_NAME "yacc.c"

/* Pure parsers.  */
#define YYPURE 0

/* Push parsers.  */
#define YYPUSH 0

/* Pull parsers.  */
#define YYPULL 1




/* First part of user prologue.  */
#line 1 "script.y"

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
#include "script_storage.h"
#include "script_value.h"
#include "libs/FGettext.h"
#include "libs/log.h"

#define MAX_VARS 5120
extern int numligne;
ScriptProp *scriptprop;
int nbobj=-1;			/* Number of objects */
int HasPosition,HasType=0;
TabObj *tabobj;		/* Array of objects, limit=1000 */
int TabIdObj[1001]; 	/* Array of object IDs */
Bloc **TabIObj;		/* TabIObj[Obj][Case] -> block linked to case */
Bloc *PileBloc[10];	/* 10 imbrications max of conditional loops */
int TopPileB=0;		/* Top of block stack */
CaseObj *TabCObj;	/* Structure to store cases values and their ID */
int CurrCase;
int i;
char **TabNVar;		/* Array of variable names */
char **TabVVar;		/* Array of variable values */
int NbVar;
long BuffArg[6][20];	/* Arguments are added by layers for each function */
int NbArg[6];		/* Array: number of arguments for each layer */
int SPileArg;		/* Size of argument stack */
long l;
extern char* ScriptName;

/* Global Initialization */
void InitVarGlob(void)
{
 scriptprop=fxcalloc(1, sizeof(ScriptProp));
 scriptprop->x=-1;
 scriptprop->y=-1;
 scriptprop->colorset = -1;
 scriptprop->initbloc=NULL;

 tabobj=fxcalloc(1, sizeof(TabObj));
 for (i=0;i<1001;i++)
  TabIdObj[i]=-1;
 TabNVar=NULL;
 TabVVar=NULL;
 NbVar=-1;
 script_storage_runtime()->used = 0;

 SPileArg=-1;
 scriptprop->usegettext = False;
 scriptprop->periodictasks=NULL;
 scriptprop->quitfunc=NULL;
}

/* Object Initialization */
void InitObjTabCase(int HasMainLoop)
{
 if (nbobj==0)
 {
  TabIObj=fxcalloc(1, sizeof(long));
  TabCObj=fxcalloc(1, sizeof(CaseObj));
 }
 else
 {
  TabIObj=(Bloc**)realloc(TabIObj,sizeof(long)*(nbobj+1));
  TabCObj=(CaseObj*)realloc(TabCObj,sizeof(CaseObj)*(nbobj+1));
 }

 if (!HasMainLoop)
  TabIObj[nbobj]=NULL;
 CurrCase=-1;
 TabCObj[nbobj].NbCase=-1;
}

/* Add a case into TabCase array */
/* Initialization of a case: increase array size */
void InitCase(int cond)
{
 CurrCase++;

 /* We store the case condition */
 TabCObj[nbobj].NbCase++;
 if (TabCObj[nbobj].NbCase==0)
  TabCObj[nbobj].LstCase=fxcalloc(1, sizeof(int));
 else
  TabCObj[nbobj].LstCase=(int*)realloc(TabCObj[nbobj].LstCase,sizeof(int)*(CurrCase+1));
 TabCObj[nbobj].LstCase[CurrCase]=cond;

 if (CurrCase==0)
  TabIObj[nbobj]=fxcalloc(1, sizeof(Bloc));
 else
  TabIObj[nbobj]=(Bloc*)realloc(TabIObj[nbobj],sizeof(Bloc)*(CurrCase+1));

 TabIObj[nbobj][CurrCase].NbInstr=-1;
 TabIObj[nbobj][CurrCase].TabInstr=NULL;

 /* This case is for the current instruction block: we stack it */
 PileBloc[0]=&TabIObj[nbobj][CurrCase];
 TopPileB=0;
}

/* Remove a level of args in BuffArg stack */
void RmLevelBufArg(void)
{
  if (SPileArg < 0 || SPileArg >= 6)
  {
    fvwm_debug(__func__, "[%s] Line %d: invalid argument stack",
      ScriptName, numligne);
    exit(1);
  }
  SPileArg--;
}

/* Function to concatenate the n latest levels of stack */
/* Returns the sorted and unstacked elements and size */
long *Depile(int NbLevelArg, int *s)
{
 long *Temp;
 int j;
 int i;
 int size;

 if (NbLevelArg < 0 || NbLevelArg > SPileArg + 1 || SPileArg >= 6)
 {
  fvwm_debug(__func__, "[%s] Line %d: invalid argument stack",
    ScriptName, numligne);
  exit(1);
 }
 if (NbLevelArg>0)
 {
  Temp=fxcalloc(1, sizeof(long));
  size=0;
  for (i=SPileArg-NbLevelArg+1;i<=SPileArg;i++)
  {
   size=NbArg[i]+size+1;
   Temp=(long*)realloc (Temp,sizeof(long)*size);
   for (j=0;j<=NbArg[i];j++)
   {
    Temp[j+size-NbArg[i]-1]=BuffArg[i][j];
   }
  }
  *s=size;
  for (i=0;i<NbLevelArg;i++)	/* Unstack the argument layers */
   RmLevelBufArg();
  return Temp;
 }
 else
 {
  *s=0;
  return NULL;
 }
}

/* Add a command */
void AddCom(int Type, int NbLevelArg)
{
 int CurrInstr;


 PileBloc[TopPileB]->NbInstr++;
 CurrInstr=PileBloc[TopPileB]->NbInstr;

 if (CurrInstr==0)
  PileBloc[TopPileB]->TabInstr=fxcalloc(1, sizeof(Instr) * (CurrInstr + 1));
 else
  PileBloc[TopPileB]->TabInstr=(Instr*)realloc(PileBloc[TopPileB]->TabInstr,
				sizeof(Instr)*(CurrInstr+1));
 /* Put instructions into block */
 PileBloc[TopPileB]->TabInstr[CurrInstr].Type=Type;
 /* We remove the last arguments layer and put it in command */

 PileBloc[TopPileB]->TabInstr[CurrInstr].TabArg=Depile(NbLevelArg,
		&PileBloc[TopPileB]->TabInstr[CurrInstr].NbArg);
}

/* Initialization of the buffer containing the current command arguments */
/* Add a layer of arguments into stack */
void AddLevelBufArg(void)
{
 /* Refuse excessive nesting before accessing the fixed stack. */
 if (SPileArg < -1 || SPileArg >= 5)
 {
  fvwm_debug(__func__, "[%s] Line %d: too many argument layers (>6)",
    ScriptName, numligne);
  exit(1);
 }
 SPileArg++;
 NbArg[SPileArg]=-1;
}

/* Add an argument into the argument layer on top of TabArg stack */
void AddBufArg(long *TabLong,int NbLong)
{
 int i;

 if (SPileArg < 0 || SPileArg >= 6 || NbLong < 0 ||
     NbArg[SPileArg] < -1 || NbArg[SPileArg] >= 20 ||
     NbLong > 19 - NbArg[SPileArg])
 {
  fvwm_debug(__func__, "[%s] Line %d: too many arguments (>20)",
    ScriptName, numligne);
  exit(1);
 }

 for (i=0;i<NbLong;i++)
 {
  BuffArg[SPileArg][i+NbArg[SPileArg]+1]=TabLong[i];
 }
 NbArg[SPileArg]=NbArg[SPileArg]+NbLong;
}

static void reserve_value(size_t length)
{
 script_storage_result result =
   script_storage_replace(script_storage_runtime(), 0, length);
 if (result != SCRIPT_STORAGE_OK)
 {
  fprintf(stderr, "[%s] Line %d: %s exceeded\n", ScriptName, numligne,
    result == SCRIPT_STORAGE_VALUE_LIMIT ? "ValueLimit" : "MemoryLimit");
  exit(1);
 }
}

/* Search for a variable name in TabVar, create it if nonexistant */
/* Returns an ID */
void AddVar(char *Name)		/* add a variable at the end of last pointed command */
{
 int i;

 /* Comparison with already existing variables */
 for (i=0;i<=NbVar;i++)
  if (strcmp(TabNVar[i],Name)==0)
  {
   l=(long)i;
   AddBufArg(&l,1);
   return ;
  }

 if (NbVar>MAX_VARS-2)
 {
  fvwm_debug(__func__,
    "[%s] Line %d: too many variables (>5120)\n",ScriptName,numligne);
  exit(1);
 }

 /* Stored names are fixed; replacements account only for the old value. */
 reserve_value(strlen(Name));
 reserve_value(0);
 NbVar++;

 if (NbVar==0)
 {
  TabNVar=fxcalloc(1, sizeof(*TabNVar));
  TabVVar=fxcalloc(1, sizeof(*TabVVar));
 }
 else
 {
  TabNVar=(char**)realloc(TabNVar,sizeof(*TabNVar)*(NbVar+1));
  TabVVar=(char**)realloc(TabVVar,sizeof(*TabVVar)*(NbVar+1));
 }

 TabNVar[NbVar]=fxstrdup(Name);
 TabVVar[NbVar]=fxcalloc(1, sizeof(char));
 TabVVar[NbVar][0]='\0';


 /* Add variable into argument buffer */
 l=(long)NbVar;
 AddBufArg(&l,1);
 return ;
}

/* Add a string constant as argument */
void AddConstStr(char *Name)
{
 /* Constants share the same value table as named variables. */
 if (NbVar >= MAX_VARS - 1)
 {
  fvwm_debug(__func__, "[%s] Line %d: too many values (>5120)",
    ScriptName, numligne);
  exit(1);
 }
 reserve_value(0);
 reserve_value(strlen(Name));
 NbVar++;
 if (NbVar==0)
 {
  TabVVar=fxcalloc(1, sizeof(*TabVVar));
  TabNVar=fxcalloc(1, sizeof(*TabNVar));
 }
 else
 {
  TabVVar=(char**)realloc(TabVVar,sizeof(*TabVVar)*(NbVar+1));
  TabNVar=(char**)realloc(TabNVar,sizeof(*TabNVar)*(NbVar+1));
 }

 TabNVar[NbVar]=fxcalloc(1, sizeof(char));
 TabNVar[NbVar][0]='\0';
 TabVVar[NbVar]=fxstrdup(Name);

 /* Add the constant ID into the current arguments list */
 l=(long)NbVar;
 AddBufArg(&l,1);
}

/* Add a number constant as argument */
void AddConstNum(long num)
{

 /* We don't create a new variable */
 /* We code the number value to form an ID */
 l=script_value_encode_integer((int)num);
 /* Add the constant into the current arguments list */
 AddBufArg(&l,1);
}

/* Add a functon as argument */
/* Remove function arguments from stack, */
/* concatene them, and put them into stack */
void AddFunct(int code,int NbLevelArg)
{
 int size;
 long *l;
 int i;

 /* Method: unstack BuffArg and complete the bottom level of BuffArg */
 l=Depile(NbLevelArg, &size);

 size++;
 if (size==1)
  l=fxcalloc(1, sizeof(long));
 else
 {
  l=(long*)realloc(l,sizeof(long)*(size));
  for (i=size-2;i>-1;i--)	/* Move arguments */
  {
   l[i+1]=l[i];
  }
 }
 l[0]=(long)code-150000;

 AddBufArg(l,size);
 free(l);
}

/* Add a test instruction to execute one block or more */
/* store the instruction, and these blocks field becomes NULL */
void AddComBloc(int TypeCond, int NbLevelArg, int NbBloc)
{
 int i;
 int OldNA;
 int CurrInstr;

 /* Add the test instruction as a command */
 AddCom(TypeCond, NbLevelArg);

 /* We then initialize both reserved fields bloc1 and bloc2 */
 CurrInstr=PileBloc[TopPileB]->NbInstr;
 /* Caution: NbArg can change if we use a function as argument */
 OldNA=PileBloc[TopPileB]->TabInstr[CurrInstr].NbArg;

 PileBloc[TopPileB]->TabInstr[CurrInstr].TabArg=(long*)realloc(
		PileBloc[TopPileB]->TabInstr[CurrInstr].TabArg,sizeof(long)*(OldNA+NbBloc));
 for (i=0;i<NbBloc;i++)
 {
  PileBloc[TopPileB]->TabInstr[CurrInstr].TabArg[OldNA+i]=0;
 }
 PileBloc[TopPileB]->TabInstr[CurrInstr].NbArg=OldNA+NbBloc;
}

/* Create a new block, and stack it: it's becoming the current block */
void EmpilerBloc(void)
{
 Bloc *TmpBloc;

 TmpBloc=fxcalloc(1, sizeof(Bloc));
 TmpBloc->NbInstr=-1;
 TmpBloc->TabInstr=NULL;
 if (TopPileB < 0 || TopPileB >= 9)
 {
  fvwm_debug(__func__, "[%s] Line %d: too many block layers (>10)",
    ScriptName, numligne);
  exit(1);
 }
 TopPileB++;
 PileBloc[TopPileB]=TmpBloc;

}

/* Unstack the script initialization block and put it into its special location */
void DepilerBloc(int IdBloc)
{
 Bloc *Bloc1;
 Instr *IfInstr;

 if (TopPileB < 1 || TopPileB >= 10)
 {
  fvwm_debug(__func__, "[%s] Line %d: invalid block stack",
    ScriptName, numligne);
  exit(1);
 }
 Bloc1=PileBloc[TopPileB];
 TopPileB--;
 IfInstr=&PileBloc[TopPileB]->TabInstr[PileBloc[TopPileB]->NbInstr];
 IfInstr->TabArg[IfInstr->NbArg-IdBloc]=(long)Bloc1;
}

/* Syntax error management */
int yyerror(char *errmsg)
{
 fvwm_debug(__func__, "[%s] Line %d: %s\n",ScriptName,numligne,errmsg);
 return 0;
}



#line 500 "script.c"

# ifndef YY_CAST
#  ifdef __cplusplus
#   define YY_CAST(Type, Val) static_cast<Type> (Val)
#   define YY_REINTERPRET_CAST(Type, Val) reinterpret_cast<Type> (Val)
#  else
#   define YY_CAST(Type, Val) ((Type) (Val))
#   define YY_REINTERPRET_CAST(Type, Val) ((Type) (Val))
#  endif
# endif
# ifndef YY_NULLPTR
#  if defined __cplusplus
#   if 201103L <= __cplusplus
#    define YY_NULLPTR nullptr
#   else
#    define YY_NULLPTR 0
#   endif
#  else
#   define YY_NULLPTR ((void*)0)
#  endif
# endif

#include "script.h"
/* Symbol kind.  */
enum yysymbol_kind_t
{
  YYSYMBOL_YYEMPTY = -2,
  YYSYMBOL_YYEOF = 0,                      /* "end of file"  */
  YYSYMBOL_YYerror = 1,                    /* error  */
  YYSYMBOL_YYUNDEF = 2,                    /* "invalid token"  */
  YYSYMBOL_STR = 3,                        /* STR  */
  YYSYMBOL_GSTR = 4,                       /* GSTR  */
  YYSYMBOL_VAR = 5,                        /* VAR  */
  YYSYMBOL_FONT = 6,                       /* FONT  */
  YYSYMBOL_NUMBER = 7,                     /* NUMBER  */
  YYSYMBOL_WINDOWTITLE = 8,                /* WINDOWTITLE  */
  YYSYMBOL_WINDOWLOCALETITLE = 9,          /* WINDOWLOCALETITLE  */
  YYSYMBOL_WINDOWSIZE = 10,                /* WINDOWSIZE  */
  YYSYMBOL_WINDOWPOSITION = 11,            /* WINDOWPOSITION  */
  YYSYMBOL_USEGETTEXT = 12,                /* USEGETTEXT  */
  YYSYMBOL_FORECOLOR = 13,                 /* FORECOLOR  */
  YYSYMBOL_BACKCOLOR = 14,                 /* BACKCOLOR  */
  YYSYMBOL_SHADCOLOR = 15,                 /* SHADCOLOR  */
  YYSYMBOL_LICOLOR = 16,                   /* LICOLOR  */
  YYSYMBOL_COLORSET = 17,                  /* COLORSET  */
  YYSYMBOL_OBJECT = 18,                    /* OBJECT  */
  YYSYMBOL_INIT = 19,                      /* INIT  */
  YYSYMBOL_PERIODICTASK = 20,              /* PERIODICTASK  */
  YYSYMBOL_QUITFUNC = 21,                  /* QUITFUNC  */
  YYSYMBOL_MAIN = 22,                      /* MAIN  */
  YYSYMBOL_END = 23,                       /* END  */
  YYSYMBOL_PROP = 24,                      /* PROP  */
  YYSYMBOL_TYPE = 25,                      /* TYPE  */
  YYSYMBOL_SIZE = 26,                      /* SIZE  */
  YYSYMBOL_POSITION = 27,                  /* POSITION  */
  YYSYMBOL_VALUE = 28,                     /* VALUE  */
  YYSYMBOL_VALUEMIN = 29,                  /* VALUEMIN  */
  YYSYMBOL_VALUEMAX = 30,                  /* VALUEMAX  */
  YYSYMBOL_TITLE = 31,                     /* TITLE  */
  YYSYMBOL_SWALLOWEXEC = 32,               /* SWALLOWEXEC  */
  YYSYMBOL_ICON = 33,                      /* ICON  */
  YYSYMBOL_FLAGS = 34,                     /* FLAGS  */
  YYSYMBOL_WARP = 35,                      /* WARP  */
  YYSYMBOL_WRITETOFILE = 36,               /* WRITETOFILE  */
  YYSYMBOL_LOCALETITLE = 37,               /* LOCALETITLE  */
  YYSYMBOL_HIDDEN = 38,                    /* HIDDEN  */
  YYSYMBOL_NOFOCUS = 39,                   /* NOFOCUS  */
  YYSYMBOL_NORELIEFSTRING = 40,            /* NORELIEFSTRING  */
  YYSYMBOL_CENTER = 41,                    /* CENTER  */
  YYSYMBOL_LEFT = 42,                      /* LEFT  */
  YYSYMBOL_RIGHT = 43,                     /* RIGHT  */
  YYSYMBOL_CASE = 44,                      /* CASE  */
  YYSYMBOL_SINGLECLIC = 45,                /* SINGLECLIC  */
  YYSYMBOL_DOUBLECLIC = 46,                /* DOUBLECLIC  */
  YYSYMBOL_BEG = 47,                       /* BEG  */
  YYSYMBOL_POINT = 48,                     /* POINT  */
  YYSYMBOL_EXEC = 49,                      /* EXEC  */
  YYSYMBOL_HIDE = 50,                      /* HIDE  */
  YYSYMBOL_SHOW = 51,                      /* SHOW  */
  YYSYMBOL_CHFONT = 52,                    /* CHFONT  */
  YYSYMBOL_CHFORECOLOR = 53,               /* CHFORECOLOR  */
  YYSYMBOL_CHBACKCOLOR = 54,               /* CHBACKCOLOR  */
  YYSYMBOL_CHCOLORSET = 55,                /* CHCOLORSET  */
  YYSYMBOL_CHWINDOWTITLE = 56,             /* CHWINDOWTITLE  */
  YYSYMBOL_CHWINDOWTITLEFARG = 57,         /* CHWINDOWTITLEFARG  */
  YYSYMBOL_KEY = 58,                       /* KEY  */
  YYSYMBOL_GETVALUE = 59,                  /* GETVALUE  */
  YYSYMBOL_GETMINVALUE = 60,               /* GETMINVALUE  */
  YYSYMBOL_GETMAXVALUE = 61,               /* GETMAXVALUE  */
  YYSYMBOL_GETFORE = 62,                   /* GETFORE  */
  YYSYMBOL_GETBACK = 63,                   /* GETBACK  */
  YYSYMBOL_GETHILIGHT = 64,                /* GETHILIGHT  */
  YYSYMBOL_GETSHADOW = 65,                 /* GETSHADOW  */
  YYSYMBOL_CHVALUE = 66,                   /* CHVALUE  */
  YYSYMBOL_CHVALUEMAX = 67,                /* CHVALUEMAX  */
  YYSYMBOL_CHVALUEMIN = 68,                /* CHVALUEMIN  */
  YYSYMBOL_ADD = 69,                       /* ADD  */
  YYSYMBOL_DIV = 70,                       /* DIV  */
  YYSYMBOL_MULT = 71,                      /* MULT  */
  YYSYMBOL_GETTITLE = 72,                  /* GETTITLE  */
  YYSYMBOL_GETOUTPUT = 73,                 /* GETOUTPUT  */
  YYSYMBOL_STRCOPY = 74,                   /* STRCOPY  */
  YYSYMBOL_NUMTOHEX = 75,                  /* NUMTOHEX  */
  YYSYMBOL_HEXTONUM = 76,                  /* HEXTONUM  */
  YYSYMBOL_QUIT = 77,                      /* QUIT  */
  YYSYMBOL_LAUNCHSCRIPT = 78,              /* LAUNCHSCRIPT  */
  YYSYMBOL_GETSCRIPTFATHER = 79,           /* GETSCRIPTFATHER  */
  YYSYMBOL_SENDTOSCRIPT = 80,              /* SENDTOSCRIPT  */
  YYSYMBOL_RECEIVFROMSCRIPT = 81,          /* RECEIVFROMSCRIPT  */
  YYSYMBOL_GET = 82,                       /* GET  */
  YYSYMBOL_SET = 83,                       /* SET  */
  YYSYMBOL_SENDSIGN = 84,                  /* SENDSIGN  */
  YYSYMBOL_REMAINDEROFDIV = 85,            /* REMAINDEROFDIV  */
  YYSYMBOL_GETTIME = 86,                   /* GETTIME  */
  YYSYMBOL_GETSCRIPTARG = 87,              /* GETSCRIPTARG  */
  YYSYMBOL_GETPID = 88,                    /* GETPID  */
  YYSYMBOL_SENDMSGANDGET = 89,             /* SENDMSGANDGET  */
  YYSYMBOL_PARSE = 90,                     /* PARSE  */
  YYSYMBOL_LASTSTRING = 91,                /* LASTSTRING  */
  YYSYMBOL_GETTEXT = 92,                   /* GETTEXT  */
  YYSYMBOL_IF = 93,                        /* IF  */
  YYSYMBOL_THEN = 94,                      /* THEN  */
  YYSYMBOL_ELSE = 95,                      /* ELSE  */
  YYSYMBOL_FOR = 96,                       /* FOR  */
  YYSYMBOL_TO = 97,                        /* TO  */
  YYSYMBOL_DO = 98,                        /* DO  */
  YYSYMBOL_WHILE = 99,                     /* WHILE  */
  YYSYMBOL_BEGF = 100,                     /* BEGF  */
  YYSYMBOL_ENDF = 101,                     /* ENDF  */
  YYSYMBOL_EQUAL = 102,                    /* EQUAL  */
  YYSYMBOL_INFEQ = 103,                    /* INFEQ  */
  YYSYMBOL_SUPEQ = 104,                    /* SUPEQ  */
  YYSYMBOL_INF = 105,                      /* INF  */
  YYSYMBOL_SUP = 106,                      /* SUP  */
  YYSYMBOL_DIFF = 107,                     /* DIFF  */
  YYSYMBOL_YYACCEPT = 108,                 /* $accept  */
  YYSYMBOL_script = 109,                   /* script  */
  YYSYMBOL_initvar = 110,                  /* initvar  */
  YYSYMBOL_head = 111,                     /* head  */
  YYSYMBOL_initbloc = 112,                 /* initbloc  */
  YYSYMBOL_periodictask = 113,             /* periodictask  */
  YYSYMBOL_quitfunc = 114,                 /* quitfunc  */
  YYSYMBOL_object = 115,                   /* object  */
  YYSYMBOL_id = 116,                       /* id  */
  YYSYMBOL_init = 117,                     /* init  */
  YYSYMBOL_flags = 118,                    /* flags  */
  YYSYMBOL_verify = 119,                   /* verify  */
  YYSYMBOL_mainloop = 120,                 /* mainloop  */
  YYSYMBOL_addtabcase = 121,               /* addtabcase  */
  YYSYMBOL_case = 122,                     /* case  */
  YYSYMBOL_clic = 123,                     /* clic  */
  YYSYMBOL_number = 124,                   /* number  */
  YYSYMBOL_bloc = 125,                     /* bloc  */
  YYSYMBOL_instr = 126,                    /* instr  */
  YYSYMBOL_oneinstr = 127,                 /* oneinstr  */
  YYSYMBOL_exec = 128,                     /* exec  */
  YYSYMBOL_hide = 129,                     /* hide  */
  YYSYMBOL_show = 130,                     /* show  */
  YYSYMBOL_chvalue = 131,                  /* chvalue  */
  YYSYMBOL_chvaluemax = 132,               /* chvaluemax  */
  YYSYMBOL_chvaluemin = 133,               /* chvaluemin  */
  YYSYMBOL_position = 134,                 /* position  */
  YYSYMBOL_size = 135,                     /* size  */
  YYSYMBOL_icon = 136,                     /* icon  */
  YYSYMBOL_title = 137,                    /* title  */
  YYSYMBOL_font = 138,                     /* font  */
  YYSYMBOL_chforecolor = 139,              /* chforecolor  */
  YYSYMBOL_chbackcolor = 140,              /* chbackcolor  */
  YYSYMBOL_chcolorset = 141,               /* chcolorset  */
  YYSYMBOL_set = 142,                      /* set  */
  YYSYMBOL_sendsign = 143,                 /* sendsign  */
  YYSYMBOL_quit = 144,                     /* quit  */
  YYSYMBOL_warp = 145,                     /* warp  */
  YYSYMBOL_sendtoscript = 146,             /* sendtoscript  */
  YYSYMBOL_writetofile = 147,              /* writetofile  */
  YYSYMBOL_key = 148,                      /* key  */
  YYSYMBOL_localetitle = 149,              /* localetitle  */
  YYSYMBOL_ifthenelse = 150,               /* ifthenelse  */
  YYSYMBOL_loop = 151,                     /* loop  */
  YYSYMBOL_while = 152,                    /* while  */
  YYSYMBOL_headif = 153,                   /* headif  */
  YYSYMBOL_else = 154,                     /* else  */
  YYSYMBOL_creerbloc = 155,                /* creerbloc  */
  YYSYMBOL_bloc1 = 156,                    /* bloc1  */
  YYSYMBOL_bloc2 = 157,                    /* bloc2  */
  YYSYMBOL_headloop = 158,                 /* headloop  */
  YYSYMBOL_headwhile = 159,                /* headwhile  */
  YYSYMBOL_var = 160,                      /* var  */
  YYSYMBOL_str = 161,                      /* str  */
  YYSYMBOL_gstr = 162,                     /* gstr  */
  YYSYMBOL_num = 163,                      /* num  */
  YYSYMBOL_singleclic2 = 164,              /* singleclic2  */
  YYSYMBOL_doubleclic2 = 165,              /* doubleclic2  */
  YYSYMBOL_addlbuff = 166,                 /* addlbuff  */
  YYSYMBOL_function = 167,                 /* function  */
  YYSYMBOL_args = 168,                     /* args  */
  YYSYMBOL_arg = 169,                      /* arg  */
  YYSYMBOL_numarg = 170,                   /* numarg  */
  YYSYMBOL_strarg = 171,                   /* strarg  */
  YYSYMBOL_gstrarg = 172,                  /* gstrarg  */
  YYSYMBOL_vararg = 173,                   /* vararg  */
  YYSYMBOL_compare = 174                   /* compare  */
};
typedef enum yysymbol_kind_t yysymbol_kind_t;




#ifdef short
# undef short
#endif

/* On compilers that do not define __PTRDIFF_MAX__ etc., make sure
   <limits.h> and (if available) <stdint.h> are included
   so that the code can choose integer types of a good width.  */

#ifndef __PTRDIFF_MAX__
# include <limits.h> /* INFRINGES ON USER NAME SPACE */
# if defined __STDC_VERSION__ && 199901 <= __STDC_VERSION__
#  include <stdint.h> /* INFRINGES ON USER NAME SPACE */
#  define YY_STDINT_H
# endif
#endif

/* Narrow types that promote to a signed type and that can represent a
   signed or unsigned integer of at least N bits.  In tables they can
   save space and decrease cache pressure.  Promoting to a signed type
   helps avoid bugs in integer arithmetic.  */

#ifdef __INT_LEAST8_MAX__
typedef __INT_LEAST8_TYPE__ yytype_int8;
#elif defined YY_STDINT_H
typedef int_least8_t yytype_int8;
#else
typedef signed char yytype_int8;
#endif

#ifdef __INT_LEAST16_MAX__
typedef __INT_LEAST16_TYPE__ yytype_int16;
#elif defined YY_STDINT_H
typedef int_least16_t yytype_int16;
#else
typedef short yytype_int16;
#endif

/* Work around bug in HP-UX 11.23, which defines these macros
   incorrectly for preprocessor constants.  This workaround can likely
   be removed in 2023, as HPE has promised support for HP-UX 11.23
   (aka HP-UX 11i v2) only through the end of 2022; see Table 2 of
   <https://h20195.www2.hpe.com/V2/getpdf.aspx/4AA4-7673ENW.pdf>.  */
#ifdef __hpux
# undef UINT_LEAST8_MAX
# undef UINT_LEAST16_MAX
# define UINT_LEAST8_MAX 255
# define UINT_LEAST16_MAX 65535
#endif

#if defined __UINT_LEAST8_MAX__ && __UINT_LEAST8_MAX__ <= __INT_MAX__
typedef __UINT_LEAST8_TYPE__ yytype_uint8;
#elif (!defined __UINT_LEAST8_MAX__ && defined YY_STDINT_H \
       && UINT_LEAST8_MAX <= INT_MAX)
typedef uint_least8_t yytype_uint8;
#elif !defined __UINT_LEAST8_MAX__ && UCHAR_MAX <= INT_MAX
typedef unsigned char yytype_uint8;
#else
typedef short yytype_uint8;
#endif

#if defined __UINT_LEAST16_MAX__ && __UINT_LEAST16_MAX__ <= __INT_MAX__
typedef __UINT_LEAST16_TYPE__ yytype_uint16;
#elif (!defined __UINT_LEAST16_MAX__ && defined YY_STDINT_H \
       && UINT_LEAST16_MAX <= INT_MAX)
typedef uint_least16_t yytype_uint16;
#elif !defined __UINT_LEAST16_MAX__ && USHRT_MAX <= INT_MAX
typedef unsigned short yytype_uint16;
#else
typedef int yytype_uint16;
#endif

#ifndef YYPTRDIFF_T
# if defined __PTRDIFF_TYPE__ && defined __PTRDIFF_MAX__
#  define YYPTRDIFF_T __PTRDIFF_TYPE__
#  define YYPTRDIFF_MAXIMUM __PTRDIFF_MAX__
# elif defined PTRDIFF_MAX
#  ifndef ptrdiff_t
#   include <stddef.h> /* INFRINGES ON USER NAME SPACE */
#  endif
#  define YYPTRDIFF_T ptrdiff_t
#  define YYPTRDIFF_MAXIMUM PTRDIFF_MAX
# else
#  define YYPTRDIFF_T long
#  define YYPTRDIFF_MAXIMUM LONG_MAX
# endif
#endif

#ifndef YYSIZE_T
# ifdef __SIZE_TYPE__
#  define YYSIZE_T __SIZE_TYPE__
# elif defined size_t
#  define YYSIZE_T size_t
# elif defined __STDC_VERSION__ && 199901 <= __STDC_VERSION__
#  include <stddef.h> /* INFRINGES ON USER NAME SPACE */
#  define YYSIZE_T size_t
# else
#  define YYSIZE_T unsigned
# endif
#endif

#define YYSIZE_MAXIMUM                                  \
  YY_CAST (YYPTRDIFF_T,                                 \
           (YYPTRDIFF_MAXIMUM < YY_CAST (YYSIZE_T, -1)  \
            ? YYPTRDIFF_MAXIMUM                         \
            : YY_CAST (YYSIZE_T, -1)))

#define YYSIZEOF(X) YY_CAST (YYPTRDIFF_T, sizeof (X))


/* Stored state numbers (used for stacks). */
typedef yytype_int16 yy_state_t;

/* State numbers in computations.  */
typedef int yy_state_fast_t;

#ifndef YY_
# if defined YYENABLE_NLS && YYENABLE_NLS
#  if ENABLE_NLS
#   include <libintl.h> /* INFRINGES ON USER NAME SPACE */
#   define YY_(Msgid) dgettext ("bison-runtime", Msgid)
#  endif
# endif
# ifndef YY_
#  define YY_(Msgid) Msgid
# endif
#endif


#ifndef YY_ATTRIBUTE_PURE
# if defined __GNUC__ && 2 < __GNUC__ + (96 <= __GNUC_MINOR__)
#  define YY_ATTRIBUTE_PURE __attribute__ ((__pure__))
# else
#  define YY_ATTRIBUTE_PURE
# endif
#endif

#ifndef YY_ATTRIBUTE_UNUSED
# if defined __GNUC__ && 2 < __GNUC__ + (7 <= __GNUC_MINOR__)
#  define YY_ATTRIBUTE_UNUSED __attribute__ ((__unused__))
# else
#  define YY_ATTRIBUTE_UNUSED
# endif
#endif

/* Suppress unused-variable warnings by "using" E.  */
#if ! defined lint || defined __GNUC__
# define YY_USE(E) ((void) (E))
#else
# define YY_USE(E) /* empty */
#endif

/* Suppress an incorrect diagnostic about yylval being uninitialized.  */
#if defined __GNUC__ && ! defined __ICC && 406 <= __GNUC__ * 100 + __GNUC_MINOR__
# if __GNUC__ * 100 + __GNUC_MINOR__ < 407
#  define YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN                           \
    _Pragma ("GCC diagnostic push")                                     \
    _Pragma ("GCC diagnostic ignored \"-Wuninitialized\"")
# else
#  define YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN                           \
    _Pragma ("GCC diagnostic push")                                     \
    _Pragma ("GCC diagnostic ignored \"-Wuninitialized\"")              \
    _Pragma ("GCC diagnostic ignored \"-Wmaybe-uninitialized\"")
# endif
# define YY_IGNORE_MAYBE_UNINITIALIZED_END      \
    _Pragma ("GCC diagnostic pop")
#else
# define YY_INITIAL_VALUE(Value) Value
#endif
#ifndef YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
# define YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
# define YY_IGNORE_MAYBE_UNINITIALIZED_END
#endif
#ifndef YY_INITIAL_VALUE
# define YY_INITIAL_VALUE(Value) /* Nothing. */
#endif

#if defined __cplusplus && defined __GNUC__ && ! defined __ICC && 6 <= __GNUC__
# define YY_IGNORE_USELESS_CAST_BEGIN                          \
    _Pragma ("GCC diagnostic push")                            \
    _Pragma ("GCC diagnostic ignored \"-Wuseless-cast\"")
# define YY_IGNORE_USELESS_CAST_END            \
    _Pragma ("GCC diagnostic pop")
#endif
#ifndef YY_IGNORE_USELESS_CAST_BEGIN
# define YY_IGNORE_USELESS_CAST_BEGIN
# define YY_IGNORE_USELESS_CAST_END
#endif


#define YY_ASSERT(E) ((void) (0 && (E)))

#if !defined yyoverflow

/* The parser invokes alloca or malloc; define the necessary symbols.  */

# ifdef YYSTACK_USE_ALLOCA
#  if YYSTACK_USE_ALLOCA
#   ifdef __GNUC__
#    define YYSTACK_ALLOC __builtin_alloca
#   elif defined __BUILTIN_VA_ARG_INCR
#    include <alloca.h> /* INFRINGES ON USER NAME SPACE */
#   elif defined _AIX
#    define YYSTACK_ALLOC __alloca
#   elif defined _MSC_VER
#    include <malloc.h> /* INFRINGES ON USER NAME SPACE */
#    define alloca _alloca
#   else
#    define YYSTACK_ALLOC alloca
#    if ! defined _ALLOCA_H && ! defined EXIT_SUCCESS
#     include <stdlib.h> /* INFRINGES ON USER NAME SPACE */
      /* Use EXIT_SUCCESS as a witness for stdlib.h.  */
#     ifndef EXIT_SUCCESS
#      define EXIT_SUCCESS 0
#     endif
#    endif
#   endif
#  endif
# endif

# ifdef YYSTACK_ALLOC
   /* Pacify GCC's 'empty if-body' warning.  */
#  define YYSTACK_FREE(Ptr) do { /* empty */; } while (0)
#  ifndef YYSTACK_ALLOC_MAXIMUM
    /* The OS might guarantee only one guard page at the bottom of the stack,
       and a page size can be as small as 4096 bytes.  So we cannot safely
       invoke alloca (N) if N exceeds 4096.  Use a slightly smaller number
       to allow for a few compiler-allocated temporary stack slots.  */
#   define YYSTACK_ALLOC_MAXIMUM 4032 /* reasonable circa 2006 */
#  endif
# else
#  define YYSTACK_ALLOC YYMALLOC
#  define YYSTACK_FREE YYFREE
#  ifndef YYSTACK_ALLOC_MAXIMUM
#   define YYSTACK_ALLOC_MAXIMUM YYSIZE_MAXIMUM
#  endif
#  if (defined __cplusplus && ! defined EXIT_SUCCESS \
       && ! ((defined YYMALLOC || defined malloc) \
             && (defined YYFREE || defined free)))
#   include <stdlib.h> /* INFRINGES ON USER NAME SPACE */
#   ifndef EXIT_SUCCESS
#    define EXIT_SUCCESS 0
#   endif
#  endif
#  ifndef YYMALLOC
#   define YYMALLOC malloc
#   if ! defined malloc && ! defined EXIT_SUCCESS
void *malloc (YYSIZE_T); /* INFRINGES ON USER NAME SPACE */
#   endif
#  endif
#  ifndef YYFREE
#   define YYFREE free
#   if ! defined free && ! defined EXIT_SUCCESS
void free (void *); /* INFRINGES ON USER NAME SPACE */
#   endif
#  endif
# endif
#endif /* !defined yyoverflow */

#if (! defined yyoverflow \
     && (! defined __cplusplus \
         || (defined YYSTYPE_IS_TRIVIAL && YYSTYPE_IS_TRIVIAL)))

/* A type that is properly aligned for any stack member.  */
union yyalloc
{
  yy_state_t yyss_alloc;
  YYSTYPE yyvs_alloc;
};

/* The size of the maximum gap between one aligned stack and the next.  */
# define YYSTACK_GAP_MAXIMUM (YYSIZEOF (union yyalloc) - 1)

/* The size of an array large to enough to hold all stacks, each with
   N elements.  */
# define YYSTACK_BYTES(N) \
     ((N) * (YYSIZEOF (yy_state_t) + YYSIZEOF (YYSTYPE)) \
      + YYSTACK_GAP_MAXIMUM)

# define YYCOPY_NEEDED 1

/* Relocate STACK from its old location to the new one.  The
   local variables YYSIZE and YYSTACKSIZE give the old and new number of
   elements in the stack, and YYPTR gives the new location of the
   stack.  Advance YYPTR to a properly aligned location for the next
   stack.  */
# define YYSTACK_RELOCATE(Stack_alloc, Stack)                           \
    do                                                                  \
      {                                                                 \
        YYPTRDIFF_T yynewbytes;                                         \
        YYCOPY (&yyptr->Stack_alloc, Stack, yysize);                    \
        Stack = &yyptr->Stack_alloc;                                    \
        yynewbytes = yystacksize * YYSIZEOF (*Stack) + YYSTACK_GAP_MAXIMUM; \
        yyptr += yynewbytes / YYSIZEOF (*yyptr);                        \
      }                                                                 \
    while (0)

#endif

#if defined YYCOPY_NEEDED && YYCOPY_NEEDED
/* Copy COUNT objects from SRC to DST.  The source and destination do
   not overlap.  */
# ifndef YYCOPY
#  if defined __GNUC__ && 1 < __GNUC__
#   define YYCOPY(Dst, Src, Count) \
      __builtin_memcpy (Dst, Src, YY_CAST (YYSIZE_T, (Count)) * sizeof (*(Src)))
#  else
#   define YYCOPY(Dst, Src, Count)              \
      do                                        \
        {                                       \
          YYPTRDIFF_T yyi;                      \
          for (yyi = 0; yyi < (Count); yyi++)   \
            (Dst)[yyi] = (Src)[yyi];            \
        }                                       \
      while (0)
#  endif
# endif
#endif /* !YYCOPY_NEEDED */

/* YYFINAL -- State number of the termination state.  */
#define YYFINAL  3
/* YYLAST -- Last index in YYTABLE.  */
#define YYLAST   1219

/* YYNTOKENS -- Number of terminals.  */
#define YYNTOKENS  108
/* YYNNTS -- Number of nonterminals.  */
#define YYNNTS  67
/* YYNRULES -- Number of rules.  */
#define YYNRULES  217
/* YYNSTATES -- Number of states.  */
#define YYNSTATES  476

/* YYMAXUTOK -- Last valid token kind.  */
#define YYMAXUTOK   362


/* YYTRANSLATE(TOKEN-NUM) -- Symbol number corresponding to TOKEN-NUM
   as returned by yylex, with out-of-bounds checking.  */
#define YYTRANSLATE(YYX)                                \
  (0 <= (YYX) && (YYX) <= YYMAXUTOK                     \
   ? YY_CAST (yysymbol_kind_t, yytranslate[YYX])        \
   : YYSYMBOL_YYUNDEF)

/* YYTRANSLATE[TOKEN-NUM] -- Symbol number corresponding to TOKEN-NUM
   as returned by yylex.  */
static const yytype_int8 yytranslate[] =
{
       0,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     1,     2,     3,     4,
       5,     6,     7,     8,     9,    10,    11,    12,    13,    14,
      15,    16,    17,    18,    19,    20,    21,    22,    23,    24,
      25,    26,    27,    28,    29,    30,    31,    32,    33,    34,
      35,    36,    37,    38,    39,    40,    41,    42,    43,    44,
      45,    46,    47,    48,    49,    50,    51,    52,    53,    54,
      55,    56,    57,    58,    59,    60,    61,    62,    63,    64,
      65,    66,    67,    68,    69,    70,    71,    72,    73,    74,
      75,    76,    77,    78,    79,    80,    81,    82,    83,    84,
      85,    86,    87,    88,    89,    90,    91,    92,    93,    94,
      95,    96,    97,    98,    99,   100,   101,   102,   103,   104,
     105,   106,   107
};

#if YYDEBUG
/* YYRLINE[YYN] -- Source line where rule number YYN was defined.  */
static const yytype_int16 yyrline[] =
{
       0,   458,   458,   461,   465,   466,   471,   477,   482,   487,
     491,   497,   503,   509,   515,   521,   527,   534,   540,   541,
     546,   547,   552,   553,   562,   563,   566,   582,   583,   587,
     591,   596,   599,   602,   605,   608,   611,   614,   617,   621,
     625,   629,   633,   639,   642,   644,   645,   648,   651,   654,
     657,   660,   666,   677,   678,   681,   683,   684,   685,   688,
     689,   692,   695,   700,   701,   702,   703,   704,   705,   706,
     707,   708,   709,   710,   711,   712,   713,   714,   715,   716,
     717,   718,   719,   720,   721,   722,   723,   724,   725,   726,
     727,   731,   732,   733,   734,   735,   736,   737,   738,   739,
     740,   741,   742,   743,   744,   745,   746,   747,   748,   749,
     750,   751,   752,   753,   754,   755,   756,   759,   761,   763,
     765,   767,   769,   771,   773,   775,   777,   779,   781,   783,
     785,   787,   789,   791,   793,   795,   797,   799,   801,   803,
     805,   807,   811,   813,   814,   816,   818,   819,   822,   823,
     827,   831,   836,   838,   840,   842,   844,   846,   848,   850,
     851,   852,   853,   854,   855,   856,   857,   858,   859,   860,
     861,   862,   863,   864,   865,   866,   867,   868,   869,   870,
     871,   872,   873,   874,   875,   880,   881,   882,   883,   884,
     885,   886,   887,   891,   892,   893,   894,   895,   896,   897,
     901,   902,   903,   904,   905,   909,   910,   911,   915,   916,
     917,   921,   925,   926,   927,   928,   929,   930
};
#endif

/** Accessing symbol of state STATE.  */
#define YY_ACCESSING_SYMBOL(State) YY_CAST (yysymbol_kind_t, yystos[State])

#if YYDEBUG || 0
/* The user-facing name of the symbol whose (internal) number is
   YYSYMBOL.  No bounds checking.  */
static const char *yysymbol_name (yysymbol_kind_t yysymbol) YY_ATTRIBUTE_UNUSED;

/* YYTNAME[SYMBOL-NUM] -- String name of the symbol SYMBOL-NUM.
   First, the terminals, then, starting at YYNTOKENS, nonterminals.  */
static const char *const yytname[] =
{
  "\"end of file\"", "error", "\"invalid token\"", "STR", "GSTR", "VAR",
  "FONT", "NUMBER", "WINDOWTITLE", "WINDOWLOCALETITLE", "WINDOWSIZE",
  "WINDOWPOSITION", "USEGETTEXT", "FORECOLOR", "BACKCOLOR", "SHADCOLOR",
  "LICOLOR", "COLORSET", "OBJECT", "INIT", "PERIODICTASK", "QUITFUNC",
  "MAIN", "END", "PROP", "TYPE", "SIZE", "POSITION", "VALUE", "VALUEMIN",
  "VALUEMAX", "TITLE", "SWALLOWEXEC", "ICON", "FLAGS", "WARP",
  "WRITETOFILE", "LOCALETITLE", "HIDDEN", "NOFOCUS", "NORELIEFSTRING",
  "CENTER", "LEFT", "RIGHT", "CASE", "SINGLECLIC", "DOUBLECLIC", "BEG",
  "POINT", "EXEC", "HIDE", "SHOW", "CHFONT", "CHFORECOLOR", "CHBACKCOLOR",
  "CHCOLORSET", "CHWINDOWTITLE", "CHWINDOWTITLEFARG", "KEY", "GETVALUE",
  "GETMINVALUE", "GETMAXVALUE", "GETFORE", "GETBACK", "GETHILIGHT",
  "GETSHADOW", "CHVALUE", "CHVALUEMAX", "CHVALUEMIN", "ADD", "DIV", "MULT",
  "GETTITLE", "GETOUTPUT", "STRCOPY", "NUMTOHEX", "HEXTONUM", "QUIT",
  "LAUNCHSCRIPT", "GETSCRIPTFATHER", "SENDTOSCRIPT", "RECEIVFROMSCRIPT",
  "GET", "SET", "SENDSIGN", "REMAINDEROFDIV", "GETTIME", "GETSCRIPTARG",
  "GETPID", "SENDMSGANDGET", "PARSE", "LASTSTRING", "GETTEXT", "IF",
  "THEN", "ELSE", "FOR", "TO", "DO", "WHILE", "BEGF", "ENDF", "EQUAL",
  "INFEQ", "SUPEQ", "INF", "SUP", "DIFF", "$accept", "script", "initvar",
  "head", "initbloc", "periodictask", "quitfunc", "object", "id", "init",
  "flags", "verify", "mainloop", "addtabcase", "case", "clic", "number",
  "bloc", "instr", "oneinstr", "exec", "hide", "show", "chvalue",
  "chvaluemax", "chvaluemin", "position", "size", "icon", "title", "font",
  "chforecolor", "chbackcolor", "chcolorset", "set", "sendsign", "quit",
  "warp", "sendtoscript", "writetofile", "key", "localetitle",
  "ifthenelse", "loop", "while", "headif", "else", "creerbloc", "bloc1",
  "bloc2", "headloop", "headwhile", "var", "str", "gstr", "num",
  "singleclic2", "doubleclic2", "addlbuff", "function", "args", "arg",
  "numarg", "strarg", "gstrarg", "vararg", "compare", YY_NULLPTR
};

static const char *
yysymbol_name (yysymbol_kind_t yysymbol)
{
  return yytname[yysymbol];
}
#endif

#define YYPACT_NINF (-296)

#define yypact_value_is_default(Yyn) \
  ((Yyn) == YYPACT_NINF)

#define YYTABLE_NINF (-1)

#define yytable_value_is_error(Yyn) \
  0

/* YYPACT[STATE-NUM] -- Index in YYTABLE of the portion describing
   STATE-NUM.  */
static const yytype_int16 yypact[] =
{
    -296,     2,  -296,  -296,   753,  -296,    10,    11,    12,    16,
      13,    14,    21,    24,    25,    23,  -296,     6,    15,  -296,
    -296,    26,    27,  -296,  -296,  -296,  -296,  -296,  -296,   -16,
    -296,  -296,    34,  -296,  -296,  -296,    19,  -296,  -296,   645,
    -296,    30,    50,  -296,  -296,  -296,  -296,  -296,  -296,  -296,
    -296,  -296,  -296,  -296,  -296,  -296,  -296,  -296,  -296,  -296,
    -296,  -296,  -296,  -296,  -296,  -296,  -296,  -296,  -296,  -296,
    -296,   756,  -296,    75,  -296,    73,  -296,    73,  -296,    73,
    -296,    73,  -296,    73,  -296,     5,  -296,    73,  -296,    17,
    -296,    73,  -296,    73,  -296,    73,  -296,    73,  -296,    73,
    -296,    73,     7,    73,  -296,     5,  -296,    73,  -296,    73,
    -296,    73,  -296,  -296,    73,  -296,    79,  -296,    73,  -296,
    -296,    69,  -296,  -296,    79,  -296,  -296,    69,  -296,   811,
    -296,    64,  -296,  -296,  -296,  -296,  -296,  -296,  -296,  -296,
    -296,  -296,  -296,  -296,  -296,  -296,  -296,  -296,  -296,  -296,
    -296,  -296,  -296,  -296,    17,    17,    17,    17,    17,    17,
    -296,  -296,  -296,  -296,  -296,  -296,  -296,  -296,  -296,  -296,
    -296,  -296,  -296,  -296,  -296,  -296,  -296,  -296,    22,  -296,
    1031,  -296,  -296,  -296,  -296,  -296,  -296,  -296,  -296,  1086,
      28,  1086,  -296,  -296,  -296,  1127,    73,    73,     7,     5,
    1127,    17,     7,  1127,  -296,  -296,  -296,  -296,  -296,  -296,
      17,     7,     7,    73,  1127,     5,    73,    73,    73,    17,
    -296,    73,  -296,  -296,  -296,  -296,  -296,  -296,  -296,  -296,
    -296,  -296,  -296,  -296,  -296,  -296,  -296,  -296,  -296,  -296,
    -296,  -296,  -296,  -296,  -296,  -296,  -296,  -296,  -296,  -296,
      -3,  1127,    18,  -296,  -296,  -296,  -296,  -296,    18,   720,
      73,    73,    73,    73,    73,    73,    73,    73,    73,    73,
      73,     7,     7,    73,     7,     7,  -296,    73,    73,  -296,
      73,  -296,     7,     7,  -296,     7,    -7,  -296,  -296,  -296,
    -296,     0,  -296,  -296,    29,  -296,  -296,  -296,  -296,    35,
    -296,  -296,  -296,  -296,  -296,    17,  -296,  -296,  -296,  -296,
    -296,  -296,  -296,  -296,   866,  -296,  -296,  -296,  -296,  -296,
    -296,  -296,     7,    73,  -296,  -296,  -296,  -296,  -296,  -296,
    -296,  -296,  -296,  -296,  -296,  -296,    36,  -296,  -296,  -296,
    -296,  -296,  -296,  -296,   921,    69,  -296,  -296,    94,    99,
     108,   109,   104,   123,   121,   122,   124,   132,   133,   139,
     147,   149,  -296,   155,   -18,  -296,  -296,  -296,  -296,  -296,
    -296,  -296,    73,    73,    73,  -296,    73,    73,    73,  -296,
    -296,  -296,    73,  -296,     7,    73,  -296,  -296,    73,    73,
    -296,    17,  -296,    73,  -296,  -296,  -296,  -296,  1086,  -296,
      69,  -296,    56,    69,  -296,  -296,  -296,  -296,  -296,  -296,
     153,   154,  -296,  -296,  -296,  -296,  -296,  -296,   107,  -296,
    -296,  -296,  -296,  -296,  -296,  -296,    73,    73,  -296,  -296,
      73,  -296,  -296,  -296,  -296,  -296,  -296,    68,  -296,    70,
    -296,  -296,  -296,  -296,  -296,  -296,  -296,  -296,   127,  -296,
    -296,  -296,    73,  -296,    69,  -296,  -296,  -296,    76,    63,
      17,  -296,  -296,  -296,  -296,  -296,   128,   129,  -296,   131,
     131,  -296,  -296,  -296,   976,  -296
};

/* YYDEFACT[STATE-NUM] -- Default reduction number in state STATE-NUM.
   Performed when YYTABLE does not specify something else to do.  Zero
   means the default is an error.  */
static const yytype_uint8 yydefact[] =
{
       3,     0,     4,     1,    18,    17,     0,     0,     0,     0,
       6,     0,     0,     0,     0,     0,   145,     0,    20,     7,
       8,     0,     0,     5,    13,    12,    14,    15,    16,     0,
       9,   145,    22,    11,    10,    63,     0,   145,    24,     0,
      63,     0,     2,    19,   158,   158,   158,   158,   158,   158,
     158,   158,   158,   158,   158,   158,   158,   158,   158,   158,
     158,   158,   158,   158,   133,   158,   158,   158,   158,   158,
     158,     0,    63,     0,    75,     0,    74,     0,    76,     0,
      78,     0,    65,     0,    66,     0,    77,     0,    64,   185,
      67,     0,    68,     0,    79,     0,    80,     0,    81,     0,
      82,     0,     0,     0,    90,     0,    69,     0,    70,     0,
      71,     0,    85,    86,     0,    83,     0,    84,     0,    87,
     145,     0,    88,   145,     0,    89,   145,     0,    21,     0,
      26,     0,   152,   155,   156,   157,   158,   203,   202,   200,
     201,   158,   158,   158,   158,   134,   153,   158,   205,   206,
     158,   158,   154,   158,   185,   185,   185,   185,   185,   185,
     117,   118,   119,   158,   158,   158,   158,   158,   208,   209,
      72,    73,   158,   158,   158,   158,   158,   211,     0,   158,
       0,   158,   193,   197,   196,   198,   194,   195,   158,     0,
       0,     0,   158,    23,    27,     0,     0,     0,     0,     0,
       0,   185,     0,     0,   188,   190,   189,   191,   186,   187,
     185,     0,     0,     0,     0,     0,     0,     0,     0,   185,
     158,     0,   158,   158,   158,   158,   158,   158,   158,    63,
     158,   158,   158,   158,   158,   158,   158,   158,   158,   158,
     158,   158,   158,   133,   158,   158,   158,   158,   158,   147,
     143,     0,     0,    63,   149,   140,   158,   141,     0,    52,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,   169,     0,     0,   172,
       0,   180,     0,     0,   183,     0,     0,   158,   158,   126,
     125,     0,   136,   138,     0,   127,   128,   129,   130,     0,
     158,   120,   121,   122,   135,   185,   132,   102,   101,   103,
     105,    92,    93,   104,     0,    91,    94,    95,   106,   107,
     108,   109,     0,     0,   116,    96,    97,    98,   112,   113,
     110,   111,   114,   115,   145,   139,     0,   214,   213,   215,
     212,   216,   217,   158,     0,     0,   158,    43,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,    45,     0,     0,   159,   178,   179,   174,   175,
     176,   177,     0,     0,     0,   160,     0,     0,     0,   163,
     168,   170,     0,   173,     0,     0,   184,   204,     0,     0,
     207,   185,   210,     0,   131,   146,    99,   100,     0,   199,
       0,   148,     0,     0,    39,    38,    40,    41,    42,    28,
       0,     0,    31,    32,    33,    34,    36,    37,    44,    35,
      55,    53,    25,   164,   166,   165,     0,     0,   162,   171,
       0,   182,   124,   123,   192,   158,   144,     0,   158,     0,
      29,    30,    46,    48,    47,    49,    50,    51,     0,   161,
     167,   181,     0,   142,     0,   151,    56,   158,     0,     0,
     185,   150,    61,    54,    59,    60,     0,     0,   137,     0,
       0,    63,    57,    58,     0,    62
};

/* YYPGOTO[NTERM-NUM].  */
static const yytype_int16 yypgoto[] =
{
    -296,  -296,  -296,  -296,  -296,  -296,  -296,  -296,  -296,  -296,
    -296,  -296,  -296,  -296,  -296,  -296,  -296,  -295,   -40,     1,
     -50,   -49,   -46,   -53,   -51,   -54,   -32,   -29,   -33,   -28,
     -39,   -37,   -36,   -41,   -47,   -44,   -43,   -23,   -24,   -20,
     -31,    31,  -296,   -35,   -14,  -296,  -296,   -30,  -296,  -188,
    -296,  -296,   185,   209,   192,   200,   317,   324,    -9,  -187,
     330,  -101,   -12,   -99,   241,   116,   -17
};

/* YYDEFGOTO[NTERM-NUM].  */
static const yytype_int16 yydefgoto[] =
{
       0,     1,     2,     4,    18,    32,    38,    42,   131,   259,
     418,   364,   422,   448,   459,   466,   467,   472,    39,   254,
      88,    90,    92,   106,   108,   110,    76,    74,    80,    78,
      94,    96,    98,   100,   115,   117,   112,    82,   113,    84,
     104,    86,   119,   122,   125,   120,   335,    29,   250,   255,
     123,   126,   137,   155,   169,   138,   139,   140,    75,   286,
     160,   188,   141,   150,   170,   178,   343
};

/* YYTABLE[YYPACT[STATE-NUM]] -- What to do in state STATE-NUM.  If
   positive, shift that token.  If negative, reduce the rule whose
   number is the opposite.  If YYTABLE_NINF, syntax error.  */
static const yytype_int16 yytable[] =
{
      71,    36,     3,   257,   420,   421,   172,    41,   146,    30,
     132,   152,   132,   291,    19,    20,   294,    23,    24,    21,
     146,   152,   132,    22,   133,    25,   192,   299,    26,    27,
      28,    35,   129,    33,    34,    31,    77,    79,    81,    83,
      85,    87,    89,    91,    93,    95,    97,    99,   101,   102,
     103,   105,   107,   109,   111,    37,   114,   116,   118,   121,
     124,   127,   134,   135,   336,   142,    40,   143,    73,   144,
     462,   145,   146,   152,   132,   151,   133,    72,   132,   161,
     133,   162,   130,   163,   132,   164,   463,   165,   194,   166,
     180,   171,   334,   189,   387,   173,   191,   174,   404,   175,
     290,   390,   176,   405,   220,   147,   179,   167,   464,   465,
     256,   408,   406,   407,   134,   135,   300,   153,   134,   135,
     337,   338,   339,   340,   341,   342,   409,   195,   410,   411,
     391,   412,   196,   197,   198,   199,   392,   399,   200,   413,
     414,   201,   202,   415,   203,   442,   443,   444,   445,   446,
     447,   416,   417,   438,   210,   211,   212,   213,   214,   419,
     440,   441,   453,   215,   216,   217,   218,   219,   455,   181,
     221,   456,   251,   136,   461,   473,   469,   470,   471,   252,
     315,   249,   316,   258,   287,   288,   317,   325,   327,   314,
     326,   308,   310,   307,   318,   321,   309,   319,   330,   320,
     328,   298,   331,   311,   301,   302,   303,   312,   324,   306,
     436,   305,   332,   344,    77,    79,    81,    83,    85,    87,
     329,    89,    91,    93,    95,    97,    99,   101,   322,   323,
     105,   107,   109,   111,   333,   114,   116,   118,   124,   127,
     190,   346,     0,     0,   402,     0,     0,   345,   365,   366,
     367,   368,   369,   370,   371,   372,   373,   374,   375,   313,
       0,   378,     0,     0,     0,   381,   382,     0,   383,     0,
     148,     0,     0,     0,   154,     0,     0,     0,   388,   389,
       0,   156,     0,     0,     0,     0,     0,   168,     0,   157,
     148,   393,     0,     0,   149,     0,     0,     0,     0,   437,
       0,   177,   439,     0,   398,     0,   182,     0,     0,   177,
       0,   397,   182,   184,   149,     0,     0,     0,     0,   184,
       0,   185,     0,     0,     0,     0,     0,   185,     0,     0,
     183,     0,     0,     0,   400,     0,   183,   403,     0,   154,
     154,   154,   154,   154,   154,     0,   156,   156,   156,   156,
     156,   156,     0,   458,   157,   157,   157,   157,   157,   157,
     423,   424,   425,     0,   426,   427,   428,     0,     0,     0,
     429,     0,     0,   431,     0,     0,   432,   433,     0,     0,
       0,   435,     0,   168,   148,     0,   154,   168,     0,     0,
       0,     0,     0,   156,     0,   154,   168,   168,     0,     0,
     148,   157,   156,     0,   154,     0,   158,     0,   149,     0,
     157,   156,     0,   159,   449,   450,     0,     0,   451,   157,
       0,     0,     0,     0,   149,     0,   452,     0,     0,   454,
       0,   474,     0,     0,     0,     0,     0,     0,   186,   289,
     457,     0,     0,   293,   186,   187,     0,     0,   460,     0,
       0,   187,   296,   297,     0,     0,   168,   168,     0,   168,
     168,     0,     0,     0,     0,     0,     0,   168,   168,     0,
     168,   158,   158,   158,   158,   158,   158,     0,   159,   159,
     159,   159,   159,   159,   204,   205,   206,   207,   208,   209,
     154,     0,     0,     0,     0,     0,     0,   156,     0,     0,
       0,     0,     0,     0,     0,   157,     0,   168,     0,     0,
       0,     0,   376,   377,     0,   379,   380,     0,   158,     0,
       0,     0,     0,   384,   385,   159,   386,   158,     0,     0,
     182,   292,     0,     0,   159,     0,   158,   184,     0,     0,
     295,     0,     0,   159,     0,   185,     0,     0,     0,   304,
       0,     0,     0,     0,   183,     0,     0,     0,     0,     0,
       0,     0,     0,   396,     0,     0,     0,     0,     0,   168,
       0,     0,     0,     0,     0,     0,   154,     0,     0,     0,
       0,     0,     0,   156,     0,   182,     0,     0,   182,     0,
       0,   157,   184,     0,     0,   184,     0,     0,     0,     0,
     185,     0,     0,   185,     0,     0,     0,     0,     0,   183,
       0,     0,   183,     0,     0,     0,     0,     0,     0,     0,
       0,     0,   158,     0,     0,   430,     0,     0,     0,   159,
       0,     0,     0,     0,     0,   394,     0,     0,     0,   182,
       0,     0,     0,     0,     0,   154,   184,     0,     0,     0,
       0,     0,   156,     0,   185,     0,     0,     0,     0,     0,
     157,     0,   186,   183,     0,     0,     0,     0,    43,   187,
       0,    44,    45,     0,     0,     0,    46,     0,    47,     0,
      48,    49,    50,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,    51,    52,    53,    54,    55,    56,
      57,    58,    59,    60,     0,     0,     0,     0,   158,     0,
       0,    61,    62,    63,     0,   159,     0,   186,     0,     0,
     186,   434,    64,     0,   187,    65,   347,   187,    66,    67,
       0,     0,     0,   348,   349,   350,   351,   352,    68,     0,
       0,    69,     0,     0,    70,   353,   354,   355,   356,   357,
     358,   359,   360,   361,   362,     0,     0,   363,     0,     5,
       0,     6,     7,     8,     9,    10,    11,    12,    13,    14,
      15,   186,    16,     0,     0,     0,     0,   158,   187,   128,
       0,     0,    44,    45,   159,     0,    17,    46,     0,    47,
     468,    48,    49,    50,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,    51,    52,    53,    54,    55,
      56,    57,    58,    59,    60,     0,     0,     0,     0,     0,
       0,     0,    61,    62,    63,     0,     0,     0,     0,     0,
       0,     0,     0,    64,   193,     0,    65,    44,    45,    66,
      67,     0,    46,     0,    47,     0,    48,    49,    50,    68,
       0,     0,    69,     0,     0,    70,     0,     0,     0,     0,
      51,    52,    53,    54,    55,    56,    57,    58,    59,    60,
       0,     0,     0,     0,     0,     0,     0,    61,    62,    63,
       0,     0,     0,     0,     0,     0,     0,     0,    64,   395,
       0,    65,    44,    45,    66,    67,     0,    46,     0,    47,
       0,    48,    49,    50,    68,     0,     0,    69,     0,     0,
      70,     0,     0,     0,     0,    51,    52,    53,    54,    55,
      56,    57,    58,    59,    60,     0,     0,     0,     0,     0,
       0,     0,    61,    62,    63,     0,     0,     0,     0,     0,
       0,     0,     0,    64,   401,     0,    65,    44,    45,    66,
      67,     0,    46,     0,    47,     0,    48,    49,    50,    68,
       0,     0,    69,     0,     0,    70,     0,     0,     0,     0,
      51,    52,    53,    54,    55,    56,    57,    58,    59,    60,
       0,     0,     0,     0,     0,     0,     0,    61,    62,    63,
       0,     0,     0,     0,     0,     0,     0,     0,    64,   475,
       0,    65,    44,    45,    66,    67,     0,    46,     0,    47,
       0,    48,    49,    50,    68,     0,     0,    69,     0,     0,
      70,     0,     0,     0,     0,    51,    52,    53,    54,    55,
      56,    57,    58,    59,    60,     0,     0,     0,     0,     0,
       0,     0,    61,    62,    63,     0,     0,     0,     0,     0,
       0,     0,     0,    64,     0,     0,    65,   222,   223,    66,
      67,     0,   224,     0,   225,     0,   226,   227,   228,    68,
       0,     0,    69,     0,     0,    70,     0,     0,   229,     0,
     230,   231,   232,   233,   234,   235,   236,   237,   238,   239,
       0,     0,     0,     0,     0,     0,     0,   240,   241,   242,
       0,     0,     0,     0,     0,     0,     0,     0,   243,     0,
       0,   244,   222,   223,   245,   246,     0,   224,     0,   225,
       0,   226,   227,   228,     0,     0,     0,   247,     0,     0,
     248,     0,     0,   253,     0,   230,   231,   232,   233,   234,
     235,   236,   237,   238,   239,     0,     0,     0,     0,     0,
       0,     0,   240,   241,   242,     0,     0,     0,     0,     0,
       0,     0,     0,   243,     0,     0,   244,     0,     0,   245,
     246,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,   247,     0,     0,   248,   260,   261,   262,   263,
     264,   265,   266,     0,     0,     0,   267,   268,   269,   270,
     271,   272,   273,   274,     0,   275,   276,     0,   277,     0,
       0,     0,   278,   279,   280,   281,   282,   283,   284,   285
};

static const yytype_int16 yycheck[] =
{
      40,    31,     0,   191,    22,    23,   105,    37,     3,     3,
       5,     4,     5,   200,     4,     4,   203,     4,     4,     7,
       3,     4,     5,     7,     7,     4,   127,   214,     4,     4,
       7,    47,    72,     7,     7,    20,    45,    46,    47,    48,
      49,    50,    51,    52,    53,    54,    55,    56,    57,    58,
      59,    60,    61,    62,    63,    21,    65,    66,    67,    68,
      69,    70,    45,    46,   251,    77,    47,    79,    18,    81,
       7,    83,     3,     4,     5,    87,     7,    47,     5,    91,
       7,    93,     7,    95,     5,    97,    23,    99,    24,   101,
     120,   103,    95,   123,   101,   107,   126,   109,     4,   111,
     199,   101,   114,     4,    82,   100,   118,   100,    45,    46,
      82,     7,     4,     4,    45,    46,   215,   100,    45,    46,
     102,   103,   104,   105,   106,   107,     3,   136,     7,     7,
     101,     7,   141,   142,   143,   144,   101,   101,   147,     7,
       7,   150,   151,     4,   153,    38,    39,    40,    41,    42,
      43,     4,     3,    97,   163,   164,   165,   166,   167,     4,
       7,     7,    94,   172,   173,   174,   175,   176,    98,   100,
     179,    44,   181,   100,    98,   470,    48,    48,    47,   188,
     230,   180,   231,   192,   196,   197,   232,   240,   242,   229,
     241,   223,   225,   222,   233,   236,   224,   234,   245,   235,
     243,   213,   246,   226,   216,   217,   218,   227,   239,   221,
     398,   220,   247,   253,   223,   224,   225,   226,   227,   228,
     244,   230,   231,   232,   233,   234,   235,   236,   237,   238,
     239,   240,   241,   242,   248,   244,   245,   246,   247,   248,
     124,   258,    -1,    -1,   345,    -1,    -1,   256,   260,   261,
     262,   263,   264,   265,   266,   267,   268,   269,   270,   228,
      -1,   273,    -1,    -1,    -1,   277,   278,    -1,   280,    -1,
      85,    -1,    -1,    -1,    89,    -1,    -1,    -1,   287,   288,
      -1,    89,    -1,    -1,    -1,    -1,    -1,   102,    -1,    89,
     105,   300,    -1,    -1,    85,    -1,    -1,    -1,    -1,   400,
      -1,   116,   403,    -1,   334,    -1,   121,    -1,    -1,   124,
      -1,   323,   127,   121,   105,    -1,    -1,    -1,    -1,   127,
      -1,   121,    -1,    -1,    -1,    -1,    -1,   127,    -1,    -1,
     121,    -1,    -1,    -1,   343,    -1,   127,   346,    -1,   154,
     155,   156,   157,   158,   159,    -1,   154,   155,   156,   157,
     158,   159,    -1,   454,   154,   155,   156,   157,   158,   159,
     372,   373,   374,    -1,   376,   377,   378,    -1,    -1,    -1,
     382,    -1,    -1,   385,    -1,    -1,   388,   389,    -1,    -1,
      -1,   393,    -1,   198,   199,    -1,   201,   202,    -1,    -1,
      -1,    -1,    -1,   201,    -1,   210,   211,   212,    -1,    -1,
     215,   201,   210,    -1,   219,    -1,    89,    -1,   199,    -1,
     210,   219,    -1,    89,   426,   427,    -1,    -1,   430,   219,
      -1,    -1,    -1,    -1,   215,    -1,   435,    -1,    -1,   438,
      -1,   471,    -1,    -1,    -1,    -1,    -1,    -1,   121,   198,
     452,    -1,    -1,   202,   127,   121,    -1,    -1,   457,    -1,
      -1,   127,   211,   212,    -1,    -1,   271,   272,    -1,   274,
     275,    -1,    -1,    -1,    -1,    -1,    -1,   282,   283,    -1,
     285,   154,   155,   156,   157,   158,   159,    -1,   154,   155,
     156,   157,   158,   159,   154,   155,   156,   157,   158,   159,
     305,    -1,    -1,    -1,    -1,    -1,    -1,   305,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,   305,    -1,   322,    -1,    -1,
      -1,    -1,   271,   272,    -1,   274,   275,    -1,   201,    -1,
      -1,    -1,    -1,   282,   283,   201,   285,   210,    -1,    -1,
     345,   201,    -1,    -1,   210,    -1,   219,   345,    -1,    -1,
     210,    -1,    -1,   219,    -1,   345,    -1,    -1,    -1,   219,
      -1,    -1,    -1,    -1,   345,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,   322,    -1,    -1,    -1,    -1,    -1,   384,
      -1,    -1,    -1,    -1,    -1,    -1,   391,    -1,    -1,    -1,
      -1,    -1,    -1,   391,    -1,   400,    -1,    -1,   403,    -1,
      -1,   391,   400,    -1,    -1,   403,    -1,    -1,    -1,    -1,
     400,    -1,    -1,   403,    -1,    -1,    -1,    -1,    -1,   400,
      -1,    -1,   403,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,   305,    -1,    -1,   384,    -1,    -1,    -1,   305,
      -1,    -1,    -1,    -1,    -1,   305,    -1,    -1,    -1,   454,
      -1,    -1,    -1,    -1,    -1,   460,   454,    -1,    -1,    -1,
      -1,    -1,   460,    -1,   454,    -1,    -1,    -1,    -1,    -1,
     460,    -1,   345,   454,    -1,    -1,    -1,    -1,    23,   345,
      -1,    26,    27,    -1,    -1,    -1,    31,    -1,    33,    -1,
      35,    36,    37,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    49,    50,    51,    52,    53,    54,
      55,    56,    57,    58,    -1,    -1,    -1,    -1,   391,    -1,
      -1,    66,    67,    68,    -1,   391,    -1,   400,    -1,    -1,
     403,   391,    77,    -1,   400,    80,     6,   403,    83,    84,
      -1,    -1,    -1,    13,    14,    15,    16,    17,    93,    -1,
      -1,    96,    -1,    -1,    99,    25,    26,    27,    28,    29,
      30,    31,    32,    33,    34,    -1,    -1,    37,    -1,     6,
      -1,     8,     9,    10,    11,    12,    13,    14,    15,    16,
      17,   454,    19,    -1,    -1,    -1,    -1,   460,   454,    23,
      -1,    -1,    26,    27,   460,    -1,    33,    31,    -1,    33,
     460,    35,    36,    37,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    49,    50,    51,    52,    53,
      54,    55,    56,    57,    58,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    66,    67,    68,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    77,    23,    -1,    80,    26,    27,    83,
      84,    -1,    31,    -1,    33,    -1,    35,    36,    37,    93,
      -1,    -1,    96,    -1,    -1,    99,    -1,    -1,    -1,    -1,
      49,    50,    51,    52,    53,    54,    55,    56,    57,    58,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    66,    67,    68,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    77,    23,
      -1,    80,    26,    27,    83,    84,    -1,    31,    -1,    33,
      -1,    35,    36,    37,    93,    -1,    -1,    96,    -1,    -1,
      99,    -1,    -1,    -1,    -1,    49,    50,    51,    52,    53,
      54,    55,    56,    57,    58,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    66,    67,    68,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    77,    23,    -1,    80,    26,    27,    83,
      84,    -1,    31,    -1,    33,    -1,    35,    36,    37,    93,
      -1,    -1,    96,    -1,    -1,    99,    -1,    -1,    -1,    -1,
      49,    50,    51,    52,    53,    54,    55,    56,    57,    58,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    66,    67,    68,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    77,    23,
      -1,    80,    26,    27,    83,    84,    -1,    31,    -1,    33,
      -1,    35,    36,    37,    93,    -1,    -1,    96,    -1,    -1,
      99,    -1,    -1,    -1,    -1,    49,    50,    51,    52,    53,
      54,    55,    56,    57,    58,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    66,    67,    68,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    77,    -1,    -1,    80,    26,    27,    83,
      84,    -1,    31,    -1,    33,    -1,    35,    36,    37,    93,
      -1,    -1,    96,    -1,    -1,    99,    -1,    -1,    47,    -1,
      49,    50,    51,    52,    53,    54,    55,    56,    57,    58,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    66,    67,    68,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    77,    -1,
      -1,    80,    26,    27,    83,    84,    -1,    31,    -1,    33,
      -1,    35,    36,    37,    -1,    -1,    -1,    96,    -1,    -1,
      99,    -1,    -1,    47,    -1,    49,    50,    51,    52,    53,
      54,    55,    56,    57,    58,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    66,    67,    68,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    77,    -1,    -1,    80,    -1,    -1,    83,
      84,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    96,    -1,    -1,    99,    59,    60,    61,    62,
      63,    64,    65,    -1,    -1,    -1,    69,    70,    71,    72,
      73,    74,    75,    76,    -1,    78,    79,    -1,    81,    -1,
      -1,    -1,    85,    86,    87,    88,    89,    90,    91,    92
};

/* YYSTOS[STATE-NUM] -- The symbol kind of the accessing symbol of
   state STATE-NUM.  */
static const yytype_uint8 yystos[] =
{
       0,   109,   110,     0,   111,     6,     8,     9,    10,    11,
      12,    13,    14,    15,    16,    17,    19,    33,   112,     4,
       4,     7,     7,     4,     4,     4,     4,     4,     7,   155,
       3,    20,   113,     7,     7,    47,   155,    21,   114,   126,
      47,   155,   115,    23,    26,    27,    31,    33,    35,    36,
      37,    49,    50,    51,    52,    53,    54,    55,    56,    57,
      58,    66,    67,    68,    77,    80,    83,    84,    93,    96,
      99,   126,    47,    18,   135,   166,   134,   166,   137,   166,
     136,   166,   145,   166,   147,   166,   149,   166,   128,   166,
     129,   166,   130,   166,   138,   166,   139,   166,   140,   166,
     141,   166,   166,   166,   148,   166,   131,   166,   132,   166,
     133,   166,   144,   146,   166,   142,   166,   143,   166,   150,
     153,   166,   151,   158,   166,   152,   159,   166,    23,   126,
       7,   116,     5,     7,    45,    46,   100,   160,   163,   164,
     165,   170,   170,   170,   170,   170,     3,   100,   160,   161,
     171,   170,     4,   100,   160,   161,   162,   163,   164,   165,
     168,   170,   170,   170,   170,   170,   170,   100,   160,   162,
     172,   170,   171,   170,   170,   170,   170,   160,   173,   170,
     155,   100,   160,   161,   162,   163,   164,   165,   169,   155,
     173,   155,   169,    23,    24,   166,   166,   166,   166,   166,
     166,   166,   166,   166,   168,   168,   168,   168,   168,   168,
     166,   166,   166,   166,   166,   166,   166,   166,   166,   166,
      82,   166,    26,    27,    31,    33,    35,    36,    37,    47,
      49,    50,    51,    52,    53,    54,    55,    56,    57,    58,
      66,    67,    68,    77,    80,    83,    84,    96,    99,   127,
     156,   166,   166,    47,   127,   157,    82,   157,   166,   117,
      59,    60,    61,    62,    63,    64,    65,    69,    70,    71,
      72,    73,    74,    75,    76,    78,    79,    81,    85,    86,
      87,    88,    89,    90,    91,    92,   167,   170,   170,   172,
     171,   167,   168,   172,   167,   168,   172,   172,   170,   167,
     171,   170,   170,   170,   168,   166,   170,   135,   134,   137,
     136,   145,   147,   149,   126,   128,   129,   130,   138,   139,
     140,   141,   166,   166,   148,   131,   132,   133,   144,   146,
     142,   143,   151,   152,    95,   154,   167,   102,   103,   104,
     105,   106,   107,   174,   126,   166,   174,     6,    13,    14,
      15,    16,    17,    25,    26,    27,    28,    29,    30,    31,
      32,    33,    34,    37,   119,   170,   170,   170,   170,   170,
     170,   170,   170,   170,   170,   170,   172,   172,   170,   172,
     172,   170,   170,   170,   172,   172,   172,   101,   166,   166,
     101,   101,   101,   166,   168,    23,   172,   170,   155,   101,
     166,    23,   169,   166,     4,     4,     4,     4,     7,     3,
       7,     7,     7,     7,     7,     4,     4,     3,   118,     4,
      22,    23,   120,   170,   170,   170,   170,   170,   170,   170,
     172,   170,   170,   170,   168,   170,   157,   169,    97,   169,
       7,     7,    38,    39,    40,    41,    42,    43,   121,   170,
     170,   170,   166,    94,   166,    98,    44,   170,   169,   122,
     166,    98,     7,    23,    45,    46,   123,   124,   168,    48,
      48,    47,   125,   125,   126,    23
};

/* YYR1[RULE-NUM] -- Symbol kind of the left-hand side of rule RULE-NUM.  */
static const yytype_uint8 yyr1[] =
{
       0,   108,   109,   110,   111,   111,   111,   111,   111,   111,
     111,   111,   111,   111,   111,   111,   111,   111,   112,   112,
     113,   113,   114,   114,   115,   115,   116,   117,   117,   117,
     117,   117,   117,   117,   117,   117,   117,   117,   117,   117,
     117,   117,   117,   117,   117,   118,   118,   118,   118,   118,
     118,   118,   119,   120,   120,   121,   122,   122,   122,   123,
     123,   124,   125,   126,   126,   126,   126,   126,   126,   126,
     126,   126,   126,   126,   126,   126,   126,   126,   126,   126,
     126,   126,   126,   126,   126,   126,   126,   126,   126,   126,
     126,   127,   127,   127,   127,   127,   127,   127,   127,   127,
     127,   127,   127,   127,   127,   127,   127,   127,   127,   127,
     127,   127,   127,   127,   127,   127,   127,   128,   129,   130,
     131,   132,   133,   134,   135,   136,   137,   138,   139,   140,
     141,   142,   143,   144,   145,   146,   147,   148,   149,   150,
     151,   152,   153,   154,   154,   155,   156,   156,   157,   157,
     158,   159,   160,   161,   162,   163,   164,   165,   166,   167,
     167,   167,   167,   167,   167,   167,   167,   167,   167,   167,
     167,   167,   167,   167,   167,   167,   167,   167,   167,   167,
     167,   167,   167,   167,   167,   168,   168,   168,   168,   168,
     168,   168,   168,   169,   169,   169,   169,   169,   169,   169,
     170,   170,   170,   170,   170,   171,   171,   171,   172,   172,
     172,   173,   174,   174,   174,   174,   174,   174
};

/* YYR2[RULE-NUM] -- Number of symbols on the right-hand side of rule RULE-NUM.  */
static const yytype_int8 yyr2[] =
{
       0,     2,     6,     0,     0,     3,     2,     3,     3,     3,
       4,     4,     3,     3,     3,     3,     3,     2,     0,     5,
       0,     5,     0,     5,     0,     7,     1,     0,     3,     4,
       4,     3,     3,     3,     3,     3,     3,     3,     3,     3,
       3,     3,     3,     2,     3,     0,     2,     2,     2,     2,
       2,     2,     0,     1,     5,     0,     0,     4,     4,     1,
       1,     1,     3,     0,     3,     3,     3,     3,     3,     3,
       3,     3,     4,     4,     3,     3,     3,     3,     3,     3,
       3,     3,     3,     3,     3,     3,     3,     3,     3,     3,
       3,     2,     2,     2,     2,     2,     2,     2,     2,     3,
       3,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       4,     4,     4,     6,     6,     4,     4,     4,     4,     4,
       4,     5,     4,     0,     2,     4,     4,    10,     4,     4,
       3,     3,     7,     0,     3,     0,     3,     1,     3,     1,
       9,     7,     1,     1,     1,     1,     1,     1,     0,     2,
       2,     4,     3,     2,     3,     3,     3,     4,     2,     1,
       2,     3,     1,     2,     2,     2,     2,     2,     2,     2,
       1,     4,     3,     1,     2,     0,     2,     2,     2,     2,
       2,     2,     5,     1,     1,     1,     1,     1,     1,     4,
       1,     1,     1,     1,     4,     1,     1,     4,     1,     1,
       4,     1,     1,     1,     1,     1,     1,     1
};


enum { YYENOMEM = -2 };

#define yyerrok         (yyerrstatus = 0)
#define yyclearin       (yychar = YYEMPTY)

#define YYACCEPT        goto yyacceptlab
#define YYABORT         goto yyabortlab
#define YYERROR         goto yyerrorlab
#define YYNOMEM         goto yyexhaustedlab


#define YYRECOVERING()  (!!yyerrstatus)

#define YYBACKUP(Token, Value)                                    \
  do                                                              \
    if (yychar == YYEMPTY)                                        \
      {                                                           \
        yychar = (Token);                                         \
        yylval = (Value);                                         \
        YYPOPSTACK (yylen);                                       \
        yystate = *yyssp;                                         \
        goto yybackup;                                            \
      }                                                           \
    else                                                          \
      {                                                           \
        yyerror (YY_("syntax error: cannot back up")); \
        YYERROR;                                                  \
      }                                                           \
  while (0)

/* Backward compatibility with an undocumented macro.
   Use YYerror or YYUNDEF. */
#define YYERRCODE YYUNDEF


/* Enable debugging if requested.  */
#if YYDEBUG

# ifndef YYFPRINTF
#  include <stdio.h> /* INFRINGES ON USER NAME SPACE */
#  define YYFPRINTF fprintf
# endif

# define YYDPRINTF(Args)                        \
do {                                            \
  if (yydebug)                                  \
    YYFPRINTF Args;                             \
} while (0)




# define YY_SYMBOL_PRINT(Title, Kind, Value, Location)                    \
do {                                                                      \
  if (yydebug)                                                            \
    {                                                                     \
      YYFPRINTF (stderr, "%s ", Title);                                   \
      yy_symbol_print (stderr,                                            \
                  Kind, Value); \
      YYFPRINTF (stderr, "\n");                                           \
    }                                                                     \
} while (0)


/*-----------------------------------.
| Print this symbol's value on YYO.  |
`-----------------------------------*/

static void
yy_symbol_value_print (FILE *yyo,
                       yysymbol_kind_t yykind, YYSTYPE const * const yyvaluep)
{
  FILE *yyoutput = yyo;
  YY_USE (yyoutput);
  if (!yyvaluep)
    return;
  YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
  YY_USE (yykind);
  YY_IGNORE_MAYBE_UNINITIALIZED_END
}


/*---------------------------.
| Print this symbol on YYO.  |
`---------------------------*/

static void
yy_symbol_print (FILE *yyo,
                 yysymbol_kind_t yykind, YYSTYPE const * const yyvaluep)
{
  YYFPRINTF (yyo, "%s %s (",
             yykind < YYNTOKENS ? "token" : "nterm", yysymbol_name (yykind));

  yy_symbol_value_print (yyo, yykind, yyvaluep);
  YYFPRINTF (yyo, ")");
}

/*------------------------------------------------------------------.
| yy_stack_print -- Print the state stack from its BOTTOM up to its |
| TOP (included).                                                   |
`------------------------------------------------------------------*/

static void
yy_stack_print (yy_state_t *yybottom, yy_state_t *yytop)
{
  YYFPRINTF (stderr, "Stack now");
  for (; yybottom <= yytop; yybottom++)
    {
      int yybot = *yybottom;
      YYFPRINTF (stderr, " %d", yybot);
    }
  YYFPRINTF (stderr, "\n");
}

# define YY_STACK_PRINT(Bottom, Top)                            \
do {                                                            \
  if (yydebug)                                                  \
    yy_stack_print ((Bottom), (Top));                           \
} while (0)


/*------------------------------------------------.
| Report that the YYRULE is going to be reduced.  |
`------------------------------------------------*/

static void
yy_reduce_print (yy_state_t *yyssp, YYSTYPE *yyvsp,
                 int yyrule)
{
  int yylno = yyrline[yyrule];
  int yynrhs = yyr2[yyrule];
  int yyi;
  YYFPRINTF (stderr, "Reducing stack by rule %d (line %d):\n",
             yyrule - 1, yylno);
  /* The symbols being reduced.  */
  for (yyi = 0; yyi < yynrhs; yyi++)
    {
      YYFPRINTF (stderr, "   $%d = ", yyi + 1);
      yy_symbol_print (stderr,
                       YY_ACCESSING_SYMBOL (+yyssp[yyi + 1 - yynrhs]),
                       &yyvsp[(yyi + 1) - (yynrhs)]);
      YYFPRINTF (stderr, "\n");
    }
}

# define YY_REDUCE_PRINT(Rule)          \
do {                                    \
  if (yydebug)                          \
    yy_reduce_print (yyssp, yyvsp, Rule); \
} while (0)

/* Nonzero means print parse trace.  It is left uninitialized so that
   multiple parsers can coexist.  */
int yydebug;
#else /* !YYDEBUG */
# define YYDPRINTF(Args) ((void) 0)
# define YY_SYMBOL_PRINT(Title, Kind, Value, Location)
# define YY_STACK_PRINT(Bottom, Top)
# define YY_REDUCE_PRINT(Rule)
#endif /* !YYDEBUG */


/* YYINITDEPTH -- initial size of the parser's stacks.  */
#ifndef YYINITDEPTH
# define YYINITDEPTH 200
#endif

/* YYMAXDEPTH -- maximum size the stacks can grow to (effective only
   if the built-in stack extension method is used).

   Do not make this value too large; the results are undefined if
   YYSTACK_ALLOC_MAXIMUM < YYSTACK_BYTES (YYMAXDEPTH)
   evaluated with infinite-precision integer arithmetic.  */

#ifndef YYMAXDEPTH
# define YYMAXDEPTH 10000
#endif






/*-----------------------------------------------.
| Release the memory associated to this symbol.  |
`-----------------------------------------------*/

static void
yydestruct (const char *yymsg,
            yysymbol_kind_t yykind, YYSTYPE *yyvaluep)
{
  YY_USE (yyvaluep);
  if (!yymsg)
    yymsg = "Deleting";
  YY_SYMBOL_PRINT (yymsg, yykind, yyvaluep, yylocationp);

  YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
  YY_USE (yykind);
  YY_IGNORE_MAYBE_UNINITIALIZED_END
}


/* Lookahead token kind.  */
int yychar;

/* The semantic value of the lookahead symbol.  */
YYSTYPE yylval;
/* Number of syntax errors so far.  */
int yynerrs;




/*----------.
| yyparse.  |
`----------*/

int
yyparse (void)
{
    yy_state_fast_t yystate = 0;
    /* Number of tokens to shift before error messages enabled.  */
    int yyerrstatus = 0;

    /* Refer to the stacks through separate pointers, to allow yyoverflow
       to reallocate them elsewhere.  */

    /* Their size.  */
    YYPTRDIFF_T yystacksize = YYINITDEPTH;

    /* The state stack: array, bottom, top.  */
    yy_state_t yyssa[YYINITDEPTH];
    yy_state_t *yyss = yyssa;
    yy_state_t *yyssp = yyss;

    /* The semantic value stack: array, bottom, top.  */
    YYSTYPE yyvsa[YYINITDEPTH];
    YYSTYPE *yyvs = yyvsa;
    YYSTYPE *yyvsp = yyvs;

  int yyn;
  /* The return value of yyparse.  */
  int yyresult;
  /* Lookahead symbol kind.  */
  yysymbol_kind_t yytoken = YYSYMBOL_YYEMPTY;
  /* The variables used to return semantic value and location from the
     action routines.  */
  YYSTYPE yyval;



#define YYPOPSTACK(N)   (yyvsp -= (N), yyssp -= (N))

  /* The number of symbols on the RHS of the reduced rule.
     Keep to zero when no symbol should be popped.  */
  int yylen = 0;

  YYDPRINTF ((stderr, "Starting parse\n"));

  yychar = YYEMPTY; /* Cause a token to be read.  */

  goto yysetstate;


/*------------------------------------------------------------.
| yynewstate -- push a new state, which is found in yystate.  |
`------------------------------------------------------------*/
yynewstate:
  /* In all cases, when you get here, the value and location stacks
     have just been pushed.  So pushing a state here evens the stacks.  */
  yyssp++;


/*--------------------------------------------------------------------.
| yysetstate -- set current state (the top of the stack) to yystate.  |
`--------------------------------------------------------------------*/
yysetstate:
  YYDPRINTF ((stderr, "Entering state %d\n", yystate));
  YY_ASSERT (0 <= yystate && yystate < YYNSTATES);
  YY_IGNORE_USELESS_CAST_BEGIN
  *yyssp = YY_CAST (yy_state_t, yystate);
  YY_IGNORE_USELESS_CAST_END
  YY_STACK_PRINT (yyss, yyssp);

  if (yyss + yystacksize - 1 <= yyssp)
#if !defined yyoverflow && !defined YYSTACK_RELOCATE
    YYNOMEM;
#else
    {
      /* Get the current used size of the three stacks, in elements.  */
      YYPTRDIFF_T yysize = yyssp - yyss + 1;

# if defined yyoverflow
      {
        /* Give user a chance to reallocate the stack.  Use copies of
           these so that the &'s don't force the real ones into
           memory.  */
        yy_state_t *yyss1 = yyss;
        YYSTYPE *yyvs1 = yyvs;

        /* Each stack pointer address is followed by the size of the
           data in use in that stack, in bytes.  This used to be a
           conditional around just the two extra args, but that might
           be undefined if yyoverflow is a macro.  */
        yyoverflow (YY_("memory exhausted"),
                    &yyss1, yysize * YYSIZEOF (*yyssp),
                    &yyvs1, yysize * YYSIZEOF (*yyvsp),
                    &yystacksize);
        yyss = yyss1;
        yyvs = yyvs1;
      }
# else /* defined YYSTACK_RELOCATE */
      /* Extend the stack our own way.  */
      if (YYMAXDEPTH <= yystacksize)
        YYNOMEM;
      yystacksize *= 2;
      if (YYMAXDEPTH < yystacksize)
        yystacksize = YYMAXDEPTH;

      {
        yy_state_t *yyss1 = yyss;
        union yyalloc *yyptr =
          YY_CAST (union yyalloc *,
                   YYSTACK_ALLOC (YY_CAST (YYSIZE_T, YYSTACK_BYTES (yystacksize))));
        if (! yyptr)
          YYNOMEM;
        YYSTACK_RELOCATE (yyss_alloc, yyss);
        YYSTACK_RELOCATE (yyvs_alloc, yyvs);
#  undef YYSTACK_RELOCATE
        if (yyss1 != yyssa)
          YYSTACK_FREE (yyss1);
      }
# endif

      yyssp = yyss + yysize - 1;
      yyvsp = yyvs + yysize - 1;

      YY_IGNORE_USELESS_CAST_BEGIN
      YYDPRINTF ((stderr, "Stack size increased to %ld\n",
                  YY_CAST (long, yystacksize)));
      YY_IGNORE_USELESS_CAST_END

      if (yyss + yystacksize - 1 <= yyssp)
        YYABORT;
    }
#endif /* !defined yyoverflow && !defined YYSTACK_RELOCATE */


  if (yystate == YYFINAL)
    YYACCEPT;

  goto yybackup;


/*-----------.
| yybackup.  |
`-----------*/
yybackup:
  /* Do appropriate processing given the current state.  Read a
     lookahead token if we need one and don't already have one.  */

  /* First try to decide what to do without reference to lookahead token.  */
  yyn = yypact[yystate];
  if (yypact_value_is_default (yyn))
    goto yydefault;

  /* Not known => get a lookahead token if don't already have one.  */

  /* YYCHAR is either empty, or end-of-input, or a valid lookahead.  */
  if (yychar == YYEMPTY)
    {
      YYDPRINTF ((stderr, "Reading a token\n"));
      yychar = yylex ();
    }

  if (yychar <= YYEOF)
    {
      yychar = YYEOF;
      yytoken = YYSYMBOL_YYEOF;
      YYDPRINTF ((stderr, "Now at end of input.\n"));
    }
  else if (yychar == YYerror)
    {
      /* The scanner already issued an error message, process directly
         to error recovery.  But do not keep the error token as
         lookahead, it is too special and may lead us to an endless
         loop in error recovery. */
      yychar = YYUNDEF;
      yytoken = YYSYMBOL_YYerror;
      goto yyerrlab1;
    }
  else
    {
      yytoken = YYTRANSLATE (yychar);
      YY_SYMBOL_PRINT ("Next token is", yytoken, &yylval, &yylloc);
    }

  /* If the proper action on seeing token YYTOKEN is to reduce or to
     detect an error, take that action.  */
  yyn += yytoken;
  if (yyn < 0 || YYLAST < yyn || yycheck[yyn] != yytoken)
    goto yydefault;
  yyn = yytable[yyn];
  if (yyn <= 0)
    {
      if (yytable_value_is_error (yyn))
        goto yyerrlab;
      yyn = -yyn;
      goto yyreduce;
    }

  /* Count tokens shifted since error; after three, turn off error
     status.  */
  if (yyerrstatus)
    yyerrstatus--;

  /* Shift the lookahead token.  */
  YY_SYMBOL_PRINT ("Shifting", yytoken, &yylval, &yylloc);
  yystate = yyn;
  YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
  *++yyvsp = yylval;
  YY_IGNORE_MAYBE_UNINITIALIZED_END

  /* Discard the shifted token.  */
  yychar = YYEMPTY;
  goto yynewstate;


/*-----------------------------------------------------------.
| yydefault -- do the default action for the current state.  |
`-----------------------------------------------------------*/
yydefault:
  yyn = yydefact[yystate];
  if (yyn == 0)
    goto yyerrlab;
  goto yyreduce;


/*-----------------------------.
| yyreduce -- do a reduction.  |
`-----------------------------*/
yyreduce:
  /* yyn is the number of a rule to reduce with.  */
  yylen = yyr2[yyn];

  /* If YYLEN is nonzero, implement the default value of the action:
     '$$ = $1'.

     Otherwise, the following line sets YYVAL to garbage.
     This behavior is undocumented and Bison
     users should not rely upon it.  Assigning to YYVAL
     unconditionally makes the parser a bit smaller, and it avoids a
     GCC warning that YYVAL may be used uninitialized.  */
  yyval = yyvsp[1-yylen];


  YY_REDUCE_PRINT (yyn);
  switch (yyn)
    {
  case 3: /* initvar: %empty  */
#line 461 "script.y"
                                { InitVarGlob(); }
#line 2142 "script.c"
    break;

  case 5: /* head: head USEGETTEXT GSTR  */
#line 467 "script.y"
{
	FGettextInit("fvwm3", LOCALEDIR, "fvwm3");
	FGettextSetLocalePath((yyvsp[0].str));
}
#line 2151 "script.c"
    break;

  case 6: /* head: head USEGETTEXT  */
#line 472 "script.y"
{
	fprintf(stderr,"UseGettext!\n");
	FGettextInit("fvwm3", LOCALEDIR, "fvwm3");
}
#line 2160 "script.c"
    break;

  case 7: /* head: head WINDOWTITLE GSTR  */
#line 478 "script.y"
{
	/* Window Title */
	scriptprop->titlewin=(yyvsp[0].str);
}
#line 2169 "script.c"
    break;

  case 8: /* head: head WINDOWLOCALETITLE GSTR  */
#line 483 "script.y"
{
	/* Window Title */
	scriptprop->titlewin=(char *)FGettext((yyvsp[0].str));
}
#line 2178 "script.c"
    break;

  case 9: /* head: head ICON STR  */
#line 488 "script.y"
{
	scriptprop->icon=(yyvsp[0].str);
}
#line 2186 "script.c"
    break;

  case 10: /* head: head WINDOWPOSITION NUMBER NUMBER  */
#line 492 "script.y"
{
	/* Window Position and Size */
	scriptprop->x=(yyvsp[-1].number);
	scriptprop->y=(yyvsp[0].number);
}
#line 2196 "script.c"
    break;

  case 11: /* head: head WINDOWSIZE NUMBER NUMBER  */
#line 498 "script.y"
{
	/* Window Width and Height */
	scriptprop->width=(yyvsp[-1].number);
	scriptprop->height=(yyvsp[0].number);
}
#line 2206 "script.c"
    break;

  case 12: /* head: head BACKCOLOR GSTR  */
#line 504 "script.y"
{
	/* Background Color */
	scriptprop->backcolor=(yyvsp[0].str);
	scriptprop->colorset = -1;
}
#line 2216 "script.c"
    break;

  case 13: /* head: head FORECOLOR GSTR  */
#line 510 "script.y"
{
	/* Foreground Color */
	scriptprop->forecolor=(yyvsp[0].str);
	scriptprop->colorset = -1;
}
#line 2226 "script.c"
    break;

  case 14: /* head: head SHADCOLOR GSTR  */
#line 516 "script.y"
{
	/* Shaded Color */
	scriptprop->shadcolor=(yyvsp[0].str);
	scriptprop->colorset = -1;
}
#line 2236 "script.c"
    break;

  case 15: /* head: head LICOLOR GSTR  */
#line 522 "script.y"
{
	/* Highlighted Color */
	scriptprop->hilicolor=(yyvsp[0].str);
	scriptprop->colorset = -1;
}
#line 2246 "script.c"
    break;

  case 16: /* head: head COLORSET NUMBER  */
#line 528 "script.y"
{
	if ((yyvsp[0].number) < 0 || AllocColorset((yyvsp[0].number)))
	{
		scriptprop->colorset = (yyvsp[0].number);
	}
}
#line 2257 "script.c"
    break;

  case 17: /* head: head FONT  */
#line 535 "script.y"
{
	scriptprop->font=(yyvsp[0].str);
}
#line 2265 "script.c"
    break;

  case 19: /* initbloc: INIT creerbloc BEG instr END  */
#line 541 "script.y"
                                       {
				 scriptprop->initbloc=PileBloc[TopPileB];
				 TopPileB--;
				}
#line 2274 "script.c"
    break;

  case 21: /* periodictask: PERIODICTASK creerbloc BEG instr END  */
#line 547 "script.y"
                                                   {
				 scriptprop->periodictasks=PileBloc[TopPileB];
				 TopPileB--;
				}
#line 2283 "script.c"
    break;

  case 23: /* quitfunc: QUITFUNC creerbloc BEG instr END  */
#line 553 "script.y"
                                               {
				 scriptprop->quitfunc=PileBloc[TopPileB];
				 TopPileB--;
				}
#line 2292 "script.c"
    break;

  case 26: /* id: NUMBER  */
#line 566 "script.y"
                                { nbobj++;
				  if (nbobj>=1000)
				  { yyerror("Too many items\n");
				    exit(1);}
				  if (((yyvsp[0].number)<1)||((yyvsp[0].number)>1000))
				  { yyerror("Choose item id between 1 and 1000\n");
				    exit(1);}
				  if (TabIdObj[(yyvsp[0].number)]!=-1)
				  { i=(yyvsp[0].number); fprintf(stderr,"Line %d: item id %d already used:\n",numligne,(yyvsp[0].number));
				    exit(1);}
			          TabIdObj[(yyvsp[0].number)]=nbobj;
				  (*tabobj)[nbobj].id=(yyvsp[0].number);
				  (*tabobj)[nbobj].colorset = -1;
				}
#line 2311 "script.c"
    break;

  case 28: /* init: init TYPE STR  */
#line 583 "script.y"
                                {
				 (*tabobj)[nbobj].type=(yyvsp[0].str);
				 HasType=1;
				}
#line 2320 "script.c"
    break;

  case 29: /* init: init SIZE NUMBER NUMBER  */
#line 587 "script.y"
                                {
				 (*tabobj)[nbobj].width=(yyvsp[-1].number);
				 (*tabobj)[nbobj].height=(yyvsp[0].number);
				}
#line 2329 "script.c"
    break;

  case 30: /* init: init POSITION NUMBER NUMBER  */
#line 591 "script.y"
                                  {
				 (*tabobj)[nbobj].x=(yyvsp[-1].number);
				 (*tabobj)[nbobj].y=(yyvsp[0].number);
				 HasPosition=1;
				}
#line 2339 "script.c"
    break;

  case 31: /* init: init VALUE NUMBER  */
#line 596 "script.y"
                                {
				 (*tabobj)[nbobj].value=(yyvsp[0].number);
				}
#line 2347 "script.c"
    break;

  case 32: /* init: init VALUEMIN NUMBER  */
#line 599 "script.y"
                                        {
				 (*tabobj)[nbobj].value2=(yyvsp[0].number);
				}
#line 2355 "script.c"
    break;

  case 33: /* init: init VALUEMAX NUMBER  */
#line 602 "script.y"
                                        {
				 (*tabobj)[nbobj].value3=(yyvsp[0].number);
				}
#line 2363 "script.c"
    break;

  case 34: /* init: init TITLE GSTR  */
#line 605 "script.y"
                                {
				 (*tabobj)[nbobj].title= (yyvsp[0].str);
				}
#line 2371 "script.c"
    break;

  case 35: /* init: init LOCALETITLE GSTR  */
#line 608 "script.y"
                                {
				 (*tabobj)[nbobj].title= FGettextCopy((yyvsp[0].str));
				}
#line 2379 "script.c"
    break;

  case 36: /* init: init SWALLOWEXEC GSTR  */
#line 611 "script.y"
                                {
				 (*tabobj)[nbobj].swallow=(yyvsp[0].str);
				}
#line 2387 "script.c"
    break;

  case 37: /* init: init ICON STR  */
#line 614 "script.y"
                                {
				 (*tabobj)[nbobj].icon=(yyvsp[0].str);
				}
#line 2395 "script.c"
    break;

  case 38: /* init: init BACKCOLOR GSTR  */
#line 617 "script.y"
                                {
				 (*tabobj)[nbobj].backcolor=(yyvsp[0].str);
				 (*tabobj)[nbobj].colorset = -1;
				}
#line 2404 "script.c"
    break;

  case 39: /* init: init FORECOLOR GSTR  */
#line 621 "script.y"
                                {
				 (*tabobj)[nbobj].forecolor=(yyvsp[0].str);
				 (*tabobj)[nbobj].colorset = -1;
				}
#line 2413 "script.c"
    break;

  case 40: /* init: init SHADCOLOR GSTR  */
#line 625 "script.y"
                                {
				 (*tabobj)[nbobj].shadcolor=(yyvsp[0].str);
				 (*tabobj)[nbobj].colorset = -1;
				}
#line 2422 "script.c"
    break;

  case 41: /* init: init LICOLOR GSTR  */
#line 629 "script.y"
                                {
				 (*tabobj)[nbobj].hilicolor=(yyvsp[0].str);
				 (*tabobj)[nbobj].colorset = -1;
				}
#line 2431 "script.c"
    break;

  case 42: /* init: init COLORSET NUMBER  */
#line 633 "script.y"
                                {
				 if ((yyvsp[0].number) < 0 || AllocColorset((yyvsp[0].number)))
	{
		(*tabobj)[nbobj].colorset = (yyvsp[0].number);
	}
				}
#line 2442 "script.c"
    break;

  case 43: /* init: init FONT  */
#line 639 "script.y"
                                {
				 (*tabobj)[nbobj].font=(yyvsp[0].str);
				}
#line 2450 "script.c"
    break;

  case 46: /* flags: flags HIDDEN  */
#line 645 "script.y"
                                {
				 (*tabobj)[nbobj].flags[0]=True;
				}
#line 2458 "script.c"
    break;

  case 47: /* flags: flags NORELIEFSTRING  */
#line 648 "script.y"
                                {
				 (*tabobj)[nbobj].flags[1]=True;
				}
#line 2466 "script.c"
    break;

  case 48: /* flags: flags NOFOCUS  */
#line 651 "script.y"
                                {
				 (*tabobj)[nbobj].flags[2]=True;
				}
#line 2474 "script.c"
    break;

  case 49: /* flags: flags CENTER  */
#line 654 "script.y"
                                {
				 (*tabobj)[nbobj].flags[3]=TEXT_POS_CENTER;
				}
#line 2482 "script.c"
    break;

  case 50: /* flags: flags LEFT  */
#line 657 "script.y"
                                {
				 (*tabobj)[nbobj].flags[3]=TEXT_POS_LEFT;
				}
#line 2490 "script.c"
    break;

  case 51: /* flags: flags RIGHT  */
#line 660 "script.y"
                                {
				 (*tabobj)[nbobj].flags[3]=TEXT_POS_RIGHT;
				}
#line 2498 "script.c"
    break;

  case 52: /* verify: %empty  */
#line 666 "script.y"
                                 {
				  if (!HasPosition)
				   { yyerror("No position for object");
				     exit(1);}
				  if (!HasType)
				   { yyerror("No type for object");
				     exit(1);}
				  HasPosition=0;
				  HasType=0;
				 }
#line 2513 "script.c"
    break;

  case 53: /* mainloop: END  */
#line 677 "script.y"
                                { InitObjTabCase(0); }
#line 2519 "script.c"
    break;

  case 55: /* addtabcase: %empty  */
#line 681 "script.y"
                                { InitObjTabCase(1); }
#line 2525 "script.c"
    break;

  case 59: /* clic: SINGLECLIC  */
#line 688 "script.y"
                        { InitCase(-1); }
#line 2531 "script.c"
    break;

  case 60: /* clic: DOUBLECLIC  */
#line 689 "script.y"
                        { InitCase(-2); }
#line 2537 "script.c"
    break;

  case 61: /* number: NUMBER  */
#line 692 "script.y"
                                { InitCase((yyvsp[0].number)); }
#line 2543 "script.c"
    break;

  case 72: /* instr: instr CHWINDOWTITLE addlbuff gstrarg  */
#line 709 "script.y"
                                           {AddCom(27,1);}
#line 2549 "script.c"
    break;

  case 73: /* instr: instr CHWINDOWTITLEFARG addlbuff numarg  */
#line 710 "script.y"
                                              {AddCom(28,1);}
#line 2555 "script.c"
    break;

  case 99: /* oneinstr: CHWINDOWTITLE addlbuff gstrarg  */
#line 739 "script.y"
                                         {AddCom(27,1);}
#line 2561 "script.c"
    break;

  case 100: /* oneinstr: CHWINDOWTITLEFARG addlbuff numarg  */
#line 740 "script.y"
                                            {AddCom(28,1);}
#line 2567 "script.c"
    break;

  case 117: /* exec: addlbuff args  */
#line 759 "script.y"
                                        { AddCom(1,1); }
#line 2573 "script.c"
    break;

  case 118: /* hide: addlbuff numarg  */
#line 761 "script.y"
                                        { AddCom(2,1);}
#line 2579 "script.c"
    break;

  case 119: /* show: addlbuff numarg  */
#line 763 "script.y"
                                        { AddCom(3,1);}
#line 2585 "script.c"
    break;

  case 120: /* chvalue: addlbuff numarg addlbuff numarg  */
#line 765 "script.y"
                                                { AddCom(4,2);}
#line 2591 "script.c"
    break;

  case 121: /* chvaluemax: addlbuff numarg addlbuff numarg  */
#line 767 "script.y"
                                                { AddCom(21,2);}
#line 2597 "script.c"
    break;

  case 122: /* chvaluemin: addlbuff numarg addlbuff numarg  */
#line 769 "script.y"
                                                { AddCom(22,2);}
#line 2603 "script.c"
    break;

  case 123: /* position: addlbuff numarg addlbuff numarg addlbuff numarg  */
#line 771 "script.y"
                                                                { AddCom(5,3);}
#line 2609 "script.c"
    break;

  case 124: /* size: addlbuff numarg addlbuff numarg addlbuff numarg  */
#line 773 "script.y"
                                                                { AddCom(6,3);}
#line 2615 "script.c"
    break;

  case 125: /* icon: addlbuff numarg addlbuff strarg  */
#line 775 "script.y"
                                        { AddCom(7,2);}
#line 2621 "script.c"
    break;

  case 126: /* title: addlbuff numarg addlbuff gstrarg  */
#line 777 "script.y"
                                        { AddCom(8,2);}
#line 2627 "script.c"
    break;

  case 127: /* font: addlbuff numarg addlbuff args  */
#line 779 "script.y"
                                    { AddCom(9,2);}
#line 2633 "script.c"
    break;

  case 128: /* chforecolor: addlbuff numarg addlbuff gstrarg  */
#line 781 "script.y"
                                                { AddCom(10,2);}
#line 2639 "script.c"
    break;

  case 129: /* chbackcolor: addlbuff numarg addlbuff gstrarg  */
#line 783 "script.y"
                                                { AddCom(19,2);}
#line 2645 "script.c"
    break;

  case 130: /* chcolorset: addlbuff numarg addlbuff numarg  */
#line 785 "script.y"
                                                { AddCom(24,2);}
#line 2651 "script.c"
    break;

  case 131: /* set: addlbuff vararg GET addlbuff args  */
#line 787 "script.y"
                                        { AddCom(11,2);}
#line 2657 "script.c"
    break;

  case 132: /* sendsign: addlbuff numarg addlbuff numarg  */
#line 789 "script.y"
                                          { AddCom(12,2);}
#line 2663 "script.c"
    break;

  case 133: /* quit: %empty  */
#line 791 "script.y"
                                        { AddCom(13,0);}
#line 2669 "script.c"
    break;

  case 134: /* warp: addlbuff numarg  */
#line 793 "script.y"
                                        { AddCom(17,1);}
#line 2675 "script.c"
    break;

  case 135: /* sendtoscript: addlbuff numarg addlbuff args  */
#line 795 "script.y"
                                                { AddCom(23,2);}
#line 2681 "script.c"
    break;

  case 136: /* writetofile: addlbuff strarg addlbuff args  */
#line 797 "script.y"
                                                { AddCom(18,2);}
#line 2687 "script.c"
    break;

  case 137: /* key: addlbuff strarg addlbuff strarg addlbuff numarg addlbuff numarg addlbuff args  */
#line 799 "script.y"
                                                                                   { AddCom(25,5);}
#line 2693 "script.c"
    break;

  case 138: /* localetitle: addlbuff numarg addlbuff gstrarg  */
#line 801 "script.y"
                                                { AddCom(26,2);}
#line 2699 "script.c"
    break;

  case 142: /* headif: addlbuff arg addlbuff compare addlbuff arg THEN  */
#line 811 "script.y"
                                                                { AddComBloc(14,3,2); }
#line 2705 "script.c"
    break;

  case 145: /* creerbloc: %empty  */
#line 816 "script.y"
                                { EmpilerBloc(); }
#line 2711 "script.c"
    break;

  case 146: /* bloc1: BEG instr END  */
#line 818 "script.y"
                                { DepilerBloc(2); }
#line 2717 "script.c"
    break;

  case 147: /* bloc1: oneinstr  */
#line 819 "script.y"
                                { DepilerBloc(2); }
#line 2723 "script.c"
    break;

  case 148: /* bloc2: BEG instr END  */
#line 822 "script.y"
                                { DepilerBloc(1); }
#line 2729 "script.c"
    break;

  case 149: /* bloc2: oneinstr  */
#line 823 "script.y"
                                { DepilerBloc(1); }
#line 2735 "script.c"
    break;

  case 150: /* headloop: addlbuff vararg GET addlbuff arg TO addlbuff arg DO  */
#line 827 "script.y"
                                                                { AddComBloc(15,3,1); }
#line 2741 "script.c"
    break;

  case 151: /* headwhile: addlbuff arg addlbuff compare addlbuff arg DO  */
#line 831 "script.y"
                                                                { AddComBloc(16,3,1); }
#line 2747 "script.c"
    break;

  case 152: /* var: VAR  */
#line 836 "script.y"
                                { AddVar((yyvsp[0].str)); free((yyvsp[0].str)); }
#line 2753 "script.c"
    break;

  case 153: /* str: STR  */
#line 838 "script.y"
                                { AddConstStr((yyvsp[0].str)); free((yyvsp[0].str)); }
#line 2759 "script.c"
    break;

  case 154: /* gstr: GSTR  */
#line 840 "script.y"
                                { AddConstStr((yyvsp[0].str)); free((yyvsp[0].str)); }
#line 2765 "script.c"
    break;

  case 155: /* num: NUMBER  */
#line 842 "script.y"
                                { AddConstNum((yyvsp[0].number)); }
#line 2771 "script.c"
    break;

  case 156: /* singleclic2: SINGLECLIC  */
#line 844 "script.y"
                                { AddConstNum(-1); }
#line 2777 "script.c"
    break;

  case 157: /* doubleclic2: DOUBLECLIC  */
#line 846 "script.y"
                                { AddConstNum(-2); }
#line 2783 "script.c"
    break;

  case 158: /* addlbuff: %empty  */
#line 848 "script.y"
                                { AddLevelBufArg(); }
#line 2789 "script.c"
    break;

  case 159: /* function: GETVALUE numarg  */
#line 850 "script.y"
                                { AddFunct(1,1); }
#line 2795 "script.c"
    break;

  case 160: /* function: GETTITLE numarg  */
#line 851 "script.y"
                                { AddFunct(2,1); }
#line 2801 "script.c"
    break;

  case 161: /* function: GETOUTPUT gstrarg numarg numarg  */
#line 852 "script.y"
                                          { AddFunct(3,1); }
#line 2807 "script.c"
    break;

  case 162: /* function: NUMTOHEX numarg numarg  */
#line 853 "script.y"
                                 { AddFunct(4,1); }
#line 2813 "script.c"
    break;

  case 163: /* function: HEXTONUM gstrarg  */
#line 854 "script.y"
                                { AddFunct(5,1); }
#line 2819 "script.c"
    break;

  case 164: /* function: ADD numarg numarg  */
#line 855 "script.y"
                            { AddFunct(6,1); }
#line 2825 "script.c"
    break;

  case 165: /* function: MULT numarg numarg  */
#line 856 "script.y"
                             { AddFunct(7,1); }
#line 2831 "script.c"
    break;

  case 166: /* function: DIV numarg numarg  */
#line 857 "script.y"
                            { AddFunct(8,1); }
#line 2837 "script.c"
    break;

  case 167: /* function: STRCOPY gstrarg numarg numarg  */
#line 858 "script.y"
                                        { AddFunct(9,1); }
#line 2843 "script.c"
    break;

  case 168: /* function: LAUNCHSCRIPT gstrarg  */
#line 859 "script.y"
                               { AddFunct(10,1); }
#line 2849 "script.c"
    break;

  case 169: /* function: GETSCRIPTFATHER  */
#line 860 "script.y"
                          { AddFunct(11,1); }
#line 2855 "script.c"
    break;

  case 170: /* function: RECEIVFROMSCRIPT numarg  */
#line 861 "script.y"
                                  { AddFunct(12,1); }
#line 2861 "script.c"
    break;

  case 171: /* function: REMAINDEROFDIV numarg numarg  */
#line 862 "script.y"
                                       { AddFunct(13,1); }
#line 2867 "script.c"
    break;

  case 172: /* function: GETTIME  */
#line 863 "script.y"
                  { AddFunct(14,1); }
#line 2873 "script.c"
    break;

  case 173: /* function: GETSCRIPTARG numarg  */
#line 864 "script.y"
                              { AddFunct(15,1); }
#line 2879 "script.c"
    break;

  case 174: /* function: GETFORE numarg  */
#line 865 "script.y"
                         { AddFunct(16,1); }
#line 2885 "script.c"
    break;

  case 175: /* function: GETBACK numarg  */
#line 866 "script.y"
                         { AddFunct(17,1); }
#line 2891 "script.c"
    break;

  case 176: /* function: GETHILIGHT numarg  */
#line 867 "script.y"
                            { AddFunct(18,1); }
#line 2897 "script.c"
    break;

  case 177: /* function: GETSHADOW numarg  */
#line 868 "script.y"
                           { AddFunct(19,1); }
#line 2903 "script.c"
    break;

  case 178: /* function: GETMINVALUE numarg  */
#line 869 "script.y"
                             { AddFunct(20,1); }
#line 2909 "script.c"
    break;

  case 179: /* function: GETMAXVALUE numarg  */
#line 870 "script.y"
                             { AddFunct(21,1); }
#line 2915 "script.c"
    break;

  case 180: /* function: GETPID  */
#line 871 "script.y"
                 { AddFunct(22,1); }
#line 2921 "script.c"
    break;

  case 181: /* function: SENDMSGANDGET gstrarg gstrarg numarg  */
#line 872 "script.y"
                                               { AddFunct(23,1); }
#line 2927 "script.c"
    break;

  case 182: /* function: PARSE gstrarg numarg  */
#line 873 "script.y"
                               { AddFunct(24,1); }
#line 2933 "script.c"
    break;

  case 183: /* function: LASTSTRING  */
#line 874 "script.y"
                     { AddFunct(25,1); }
#line 2939 "script.c"
    break;

  case 184: /* function: GETTEXT gstrarg  */
#line 875 "script.y"
                          { AddFunct(26,1); }
#line 2945 "script.c"
    break;

  case 185: /* args: %empty  */
#line 880 "script.y"
                                { }
#line 2951 "script.c"
    break;

  case 212: /* compare: INF  */
#line 925 "script.y"
                                 { l=1-250000; AddBufArg(&l,1); }
#line 2957 "script.c"
    break;

  case 213: /* compare: INFEQ  */
#line 926 "script.y"
                                 { l=2-250000; AddBufArg(&l,1); }
#line 2963 "script.c"
    break;

  case 214: /* compare: EQUAL  */
#line 927 "script.y"
                                 { l=3-250000; AddBufArg(&l,1); }
#line 2969 "script.c"
    break;

  case 215: /* compare: SUPEQ  */
#line 928 "script.y"
                                 { l=4-250000; AddBufArg(&l,1); }
#line 2975 "script.c"
    break;

  case 216: /* compare: SUP  */
#line 929 "script.y"
                                 { l=5-250000; AddBufArg(&l,1); }
#line 2981 "script.c"
    break;

  case 217: /* compare: DIFF  */
#line 930 "script.y"
                                 { l=6-250000; AddBufArg(&l,1); }
#line 2987 "script.c"
    break;


#line 2991 "script.c"

      default: break;
    }
  /* User semantic actions sometimes alter yychar, and that requires
     that yytoken be updated with the new translation.  We take the
     approach of translating immediately before every use of yytoken.
     One alternative is translating here after every semantic action,
     but that translation would be missed if the semantic action invokes
     YYABORT, YYACCEPT, or YYERROR immediately after altering yychar or
     if it invokes YYBACKUP.  In the case of YYABORT or YYACCEPT, an
     incorrect destructor might then be invoked immediately.  In the
     case of YYERROR or YYBACKUP, subsequent parser actions might lead
     to an incorrect destructor call or verbose syntax error message
     before the lookahead is translated.  */
  YY_SYMBOL_PRINT ("-> $$ =", YY_CAST (yysymbol_kind_t, yyr1[yyn]), &yyval, &yyloc);

  YYPOPSTACK (yylen);
  yylen = 0;

  *++yyvsp = yyval;

  /* Now 'shift' the result of the reduction.  Determine what state
     that goes to, based on the state we popped back to and the rule
     number reduced by.  */
  {
    const int yylhs = yyr1[yyn] - YYNTOKENS;
    const int yyi = yypgoto[yylhs] + *yyssp;
    yystate = (0 <= yyi && yyi <= YYLAST && yycheck[yyi] == *yyssp
               ? yytable[yyi]
               : yydefgoto[yylhs]);
  }

  goto yynewstate;


/*--------------------------------------.
| yyerrlab -- here on detecting error.  |
`--------------------------------------*/
yyerrlab:
  /* Make sure we have latest lookahead translation.  See comments at
     user semantic actions for why this is necessary.  */
  yytoken = yychar == YYEMPTY ? YYSYMBOL_YYEMPTY : YYTRANSLATE (yychar);
  /* If not already recovering from an error, report this error.  */
  if (!yyerrstatus)
    {
      ++yynerrs;
      yyerror (YY_("syntax error"));
    }

  if (yyerrstatus == 3)
    {
      /* If just tried and failed to reuse lookahead token after an
         error, discard it.  */

      if (yychar <= YYEOF)
        {
          /* Return failure if at end of input.  */
          if (yychar == YYEOF)
            YYABORT;
        }
      else
        {
          yydestruct ("Error: discarding",
                      yytoken, &yylval);
          yychar = YYEMPTY;
        }
    }

  /* Else will try to reuse lookahead token after shifting the error
     token.  */
  goto yyerrlab1;


/*---------------------------------------------------.
| yyerrorlab -- error raised explicitly by YYERROR.  |
`---------------------------------------------------*/
yyerrorlab:
  /* Pacify compilers when the user code never invokes YYERROR and the
     label yyerrorlab therefore never appears in user code.  */
  if (0)
    YYERROR;
  ++yynerrs;

  /* Do not reclaim the symbols of the rule whose action triggered
     this YYERROR.  */
  YYPOPSTACK (yylen);
  yylen = 0;
  YY_STACK_PRINT (yyss, yyssp);
  yystate = *yyssp;
  goto yyerrlab1;


/*-------------------------------------------------------------.
| yyerrlab1 -- common code for both syntax error and YYERROR.  |
`-------------------------------------------------------------*/
yyerrlab1:
  yyerrstatus = 3;      /* Each real token shifted decrements this.  */

  /* Pop stack until we find a state that shifts the error token.  */
  for (;;)
    {
      yyn = yypact[yystate];
      if (!yypact_value_is_default (yyn))
        {
          yyn += YYSYMBOL_YYerror;
          if (0 <= yyn && yyn <= YYLAST && yycheck[yyn] == YYSYMBOL_YYerror)
            {
              yyn = yytable[yyn];
              if (0 < yyn)
                break;
            }
        }

      /* Pop the current state because it cannot handle the error token.  */
      if (yyssp == yyss)
        YYABORT;


      yydestruct ("Error: popping",
                  YY_ACCESSING_SYMBOL (yystate), yyvsp);
      YYPOPSTACK (1);
      yystate = *yyssp;
      YY_STACK_PRINT (yyss, yyssp);
    }

  YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
  *++yyvsp = yylval;
  YY_IGNORE_MAYBE_UNINITIALIZED_END


  /* Shift the error token.  */
  YY_SYMBOL_PRINT ("Shifting", YY_ACCESSING_SYMBOL (yyn), yyvsp, yylsp);

  yystate = yyn;
  goto yynewstate;


/*-------------------------------------.
| yyacceptlab -- YYACCEPT comes here.  |
`-------------------------------------*/
yyacceptlab:
  yyresult = 0;
  goto yyreturnlab;


/*-----------------------------------.
| yyabortlab -- YYABORT comes here.  |
`-----------------------------------*/
yyabortlab:
  yyresult = 1;
  goto yyreturnlab;


/*-----------------------------------------------------------.
| yyexhaustedlab -- YYNOMEM (memory exhaustion) comes here.  |
`-----------------------------------------------------------*/
yyexhaustedlab:
  yyerror (YY_("memory exhausted"));
  yyresult = 2;
  goto yyreturnlab;


/*----------------------------------------------------------.
| yyreturnlab -- parsing is finished, clean up and return.  |
`----------------------------------------------------------*/
yyreturnlab:
  if (yychar != YYEMPTY)
    {
      /* Make sure we have latest lookahead translation.  See comments at
         user semantic actions for why this is necessary.  */
      yytoken = YYTRANSLATE (yychar);
      yydestruct ("Cleanup: discarding lookahead",
                  yytoken, &yylval);
    }
  /* Do not reclaim the symbols of the rule whose action triggered
     this YYABORT or YYACCEPT.  */
  YYPOPSTACK (yylen);
  YY_STACK_PRINT (yyss, yyssp);
  while (yyssp != yyss)
    {
      yydestruct ("Cleanup: popping",
                  YY_ACCESSING_SYMBOL (+*yyssp), yyvsp);
      YYPOPSTACK (1);
    }
#ifndef yyoverflow
  if (yyss != yyssa)
    YYSTACK_FREE (yyss);
#endif

  return yyresult;
}

#line 933 "script.y"

