/* A Bison parser, made by GNU Bison 3.8.2.  */

/* Bison interface for Yacc-like parsers in C

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

/* DO NOT RELY ON FEATURES THAT ARE NOT DOCUMENTED in the manual,
   especially those whose name start with YY_ or yy_.  They are
   private implementation details that can be changed or removed.  */

#ifndef YY_YY_SCRIPT_H_INCLUDED
# define YY_YY_SCRIPT_H_INCLUDED
/* Debug traces.  */
#ifndef YYDEBUG
# define YYDEBUG 0
#endif
#if YYDEBUG
extern int yydebug;
#endif

/* Token kinds.  */
#ifndef YYTOKENTYPE
# define YYTOKENTYPE
  enum yytokentype
  {
    YYEMPTY = -2,
    YYEOF = 0,                     /* "end of file"  */
    YYerror = 256,                 /* error  */
    YYUNDEF = 257,                 /* "invalid token"  */
    STR = 258,                     /* STR  */
    GSTR = 259,                    /* GSTR  */
    VAR = 260,                     /* VAR  */
    FONT = 261,                    /* FONT  */
    NUMBER = 262,                  /* NUMBER  */
    WINDOWTITLE = 263,             /* WINDOWTITLE  */
    WINDOWLOCALETITLE = 264,       /* WINDOWLOCALETITLE  */
    WINDOWSIZE = 265,              /* WINDOWSIZE  */
    WINDOWPOSITION = 266,          /* WINDOWPOSITION  */
    USEGETTEXT = 267,              /* USEGETTEXT  */
    FORECOLOR = 268,               /* FORECOLOR  */
    BACKCOLOR = 269,               /* BACKCOLOR  */
    SHADCOLOR = 270,               /* SHADCOLOR  */
    LICOLOR = 271,                 /* LICOLOR  */
    COLORSET = 272,                /* COLORSET  */
    OBJECT = 273,                  /* OBJECT  */
    INIT = 274,                    /* INIT  */
    PERIODICTASK = 275,            /* PERIODICTASK  */
    QUITFUNC = 276,                /* QUITFUNC  */
    MAIN = 277,                    /* MAIN  */
    END = 278,                     /* END  */
    PROP = 279,                    /* PROP  */
    TYPE = 280,                    /* TYPE  */
    SIZE = 281,                    /* SIZE  */
    POSITION = 282,                /* POSITION  */
    VALUE = 283,                   /* VALUE  */
    VALUEMIN = 284,                /* VALUEMIN  */
    VALUEMAX = 285,                /* VALUEMAX  */
    TITLE = 286,                   /* TITLE  */
    SWALLOWEXEC = 287,             /* SWALLOWEXEC  */
    ICON = 288,                    /* ICON  */
    FLAGS = 289,                   /* FLAGS  */
    WARP = 290,                    /* WARP  */
    WRITETOFILE = 291,             /* WRITETOFILE  */
    LOCALETITLE = 292,             /* LOCALETITLE  */
    HIDDEN = 293,                  /* HIDDEN  */
    NOFOCUS = 294,                 /* NOFOCUS  */
    NORELIEFSTRING = 295,          /* NORELIEFSTRING  */
    CENTER = 296,                  /* CENTER  */
    LEFT = 297,                    /* LEFT  */
    RIGHT = 298,                   /* RIGHT  */
    CASE = 299,                    /* CASE  */
    SINGLECLIC = 300,              /* SINGLECLIC  */
    DOUBLECLIC = 301,              /* DOUBLECLIC  */
    BEG = 302,                     /* BEG  */
    POINT = 303,                   /* POINT  */
    EXEC = 304,                    /* EXEC  */
    HIDE = 305,                    /* HIDE  */
    SHOW = 306,                    /* SHOW  */
    CHFONT = 307,                  /* CHFONT  */
    CHFORECOLOR = 308,             /* CHFORECOLOR  */
    CHBACKCOLOR = 309,             /* CHBACKCOLOR  */
    CHCOLORSET = 310,              /* CHCOLORSET  */
    CHWINDOWTITLE = 311,           /* CHWINDOWTITLE  */
    CHWINDOWTITLEFARG = 312,       /* CHWINDOWTITLEFARG  */
    KEY = 313,                     /* KEY  */
    GETVALUE = 314,                /* GETVALUE  */
    GETMINVALUE = 315,             /* GETMINVALUE  */
    GETMAXVALUE = 316,             /* GETMAXVALUE  */
    GETFORE = 317,                 /* GETFORE  */
    GETBACK = 318,                 /* GETBACK  */
    GETHILIGHT = 319,              /* GETHILIGHT  */
    GETSHADOW = 320,               /* GETSHADOW  */
    CHVALUE = 321,                 /* CHVALUE  */
    CHVALUEMAX = 322,              /* CHVALUEMAX  */
    CHVALUEMIN = 323,              /* CHVALUEMIN  */
    ADD = 324,                     /* ADD  */
    DIV = 325,                     /* DIV  */
    MULT = 326,                    /* MULT  */
    GETTITLE = 327,                /* GETTITLE  */
    GETOUTPUT = 328,               /* GETOUTPUT  */
    STRCOPY = 329,                 /* STRCOPY  */
    NUMTOHEX = 330,                /* NUMTOHEX  */
    HEXTONUM = 331,                /* HEXTONUM  */
    QUIT = 332,                    /* QUIT  */
    LAUNCHSCRIPT = 333,            /* LAUNCHSCRIPT  */
    GETSCRIPTFATHER = 334,         /* GETSCRIPTFATHER  */
    SENDTOSCRIPT = 335,            /* SENDTOSCRIPT  */
    RECEIVFROMSCRIPT = 336,        /* RECEIVFROMSCRIPT  */
    GET = 337,                     /* GET  */
    SET = 338,                     /* SET  */
    SENDSIGN = 339,                /* SENDSIGN  */
    REMAINDEROFDIV = 340,          /* REMAINDEROFDIV  */
    GETTIME = 341,                 /* GETTIME  */
    GETSCRIPTARG = 342,            /* GETSCRIPTARG  */
    GETPID = 343,                  /* GETPID  */
    SENDMSGANDGET = 344,           /* SENDMSGANDGET  */
    PARSE = 345,                   /* PARSE  */
    LASTSTRING = 346,              /* LASTSTRING  */
    GETTEXT = 347,                 /* GETTEXT  */
    IF = 348,                      /* IF  */
    THEN = 349,                    /* THEN  */
    ELSE = 350,                    /* ELSE  */
    FOR = 351,                     /* FOR  */
    TO = 352,                      /* TO  */
    DO = 353,                      /* DO  */
    WHILE = 354,                   /* WHILE  */
    BEGF = 355,                    /* BEGF  */
    ENDF = 356,                    /* ENDF  */
    EQUAL = 357,                   /* EQUAL  */
    INFEQ = 358,                   /* INFEQ  */
    SUPEQ = 359,                   /* SUPEQ  */
    INF = 360,                     /* INF  */
    SUP = 361,                     /* SUP  */
    DIFF = 362                     /* DIFF  */
  };
  typedef enum yytokentype yytoken_kind_t;
#endif
/* Token kinds.  */
#define YYEMPTY -2
#define YYEOF 0
#define YYerror 256
#define YYUNDEF 257
#define STR 258
#define GSTR 259
#define VAR 260
#define FONT 261
#define NUMBER 262
#define WINDOWTITLE 263
#define WINDOWLOCALETITLE 264
#define WINDOWSIZE 265
#define WINDOWPOSITION 266
#define USEGETTEXT 267
#define FORECOLOR 268
#define BACKCOLOR 269
#define SHADCOLOR 270
#define LICOLOR 271
#define COLORSET 272
#define OBJECT 273
#define INIT 274
#define PERIODICTASK 275
#define QUITFUNC 276
#define MAIN 277
#define END 278
#define PROP 279
#define TYPE 280
#define SIZE 281
#define POSITION 282
#define VALUE 283
#define VALUEMIN 284
#define VALUEMAX 285
#define TITLE 286
#define SWALLOWEXEC 287
#define ICON 288
#define FLAGS 289
#define WARP 290
#define WRITETOFILE 291
#define LOCALETITLE 292
#define HIDDEN 293
#define NOFOCUS 294
#define NORELIEFSTRING 295
#define CENTER 296
#define LEFT 297
#define RIGHT 298
#define CASE 299
#define SINGLECLIC 300
#define DOUBLECLIC 301
#define BEG 302
#define POINT 303
#define EXEC 304
#define HIDE 305
#define SHOW 306
#define CHFONT 307
#define CHFORECOLOR 308
#define CHBACKCOLOR 309
#define CHCOLORSET 310
#define CHWINDOWTITLE 311
#define CHWINDOWTITLEFARG 312
#define KEY 313
#define GETVALUE 314
#define GETMINVALUE 315
#define GETMAXVALUE 316
#define GETFORE 317
#define GETBACK 318
#define GETHILIGHT 319
#define GETSHADOW 320
#define CHVALUE 321
#define CHVALUEMAX 322
#define CHVALUEMIN 323
#define ADD 324
#define DIV 325
#define MULT 326
#define GETTITLE 327
#define GETOUTPUT 328
#define STRCOPY 329
#define NUMTOHEX 330
#define HEXTONUM 331
#define QUIT 332
#define LAUNCHSCRIPT 333
#define GETSCRIPTFATHER 334
#define SENDTOSCRIPT 335
#define RECEIVFROMSCRIPT 336
#define GET 337
#define SET 338
#define SENDSIGN 339
#define REMAINDEROFDIV 340
#define GETTIME 341
#define GETSCRIPTARG 342
#define GETPID 343
#define SENDMSGANDGET 344
#define PARSE 345
#define LASTSTRING 346
#define GETTEXT 347
#define IF 348
#define THEN 349
#define ELSE 350
#define FOR 351
#define TO 352
#define DO 353
#define WHILE 354
#define BEGF 355
#define ENDF 356
#define EQUAL 357
#define INFEQ 358
#define SUPEQ 359
#define INF 360
#define SUP 361
#define DIFF 362

/* Value type.  */
#if ! defined YYSTYPE && ! defined YYSTYPE_IS_DECLARED
union YYSTYPE
{
#line 433 "script.y"
  char *str;
          int number;
       

#line 286 "script.h"

};
typedef union YYSTYPE YYSTYPE;
# define YYSTYPE_IS_TRIVIAL 1
# define YYSTYPE_IS_DECLARED 1
#endif


extern YYSTYPE yylval;


int yyparse (void);


#endif /* !YY_YY_SCRIPT_H_INCLUDED  */
