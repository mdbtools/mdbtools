/* A Bison parser, made by GNU Bison 3.7.1.  */

/* Bison interface for Yacc-like parsers in C

   Copyright (C) 1984, 1989-1990, 2000-2015, 2018-2020 Free Software Foundation,
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
   along with this program.  If not, see <http://www.gnu.org/licenses/>.  */

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

#ifndef YY_YY_PARSER_H_INCLUDED
# define YY_YY_PARSER_H_INCLUDED
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
    IDENT = 258,                   /* IDENT  */
    NAME = 259,                    /* NAME  */
    PATH = 260,                    /* PATH  */
    STRING = 261,                  /* STRING  */
    NUMBER = 262,                  /* NUMBER  */
    SELECT = 263,                  /* SELECT  */
    FROM = 264,                    /* FROM  */
    WHERE = 265,                   /* WHERE  */
    CONNECT = 266,                 /* CONNECT  */
    DISCONNECT = 267,              /* DISCONNECT  */
    TO = 268,                      /* TO  */
    LIST = 269,                    /* LIST  */
    TABLES = 270,                  /* TABLES  */
    AND = 271,                     /* AND  */
    OR = 272,                      /* OR  */
    NOT = 273,                     /* NOT  */
    LIMIT = 274,                   /* LIMIT  */
    COUNT = 275,                   /* COUNT  */
    STRPTIME = 276,                /* STRPTIME  */
    DESCRIBE = 277,                /* DESCRIBE  */
    TABLE = 278,                   /* TABLE  */
    TOP = 279,                     /* TOP  */
    PERCENT = 280,                 /* PERCENT  */
    LTEQ = 281,                    /* LTEQ  */
    GTEQ = 282,                    /* GTEQ  */
    LIKE = 283,                    /* LIKE  */
    IS = 284,                      /* IS  */
    NUL = 285,                     /* NUL  */
    EQ = 286,                      /* EQ  */
    LT = 287,                      /* LT  */
    GT = 288                       /* GT  */
  };
  typedef enum yytokentype yytoken_kind_t;
#endif

/* Value type.  */
#if ! defined YYSTYPE && ! defined YYSTYPE_IS_DECLARED
union YYSTYPE
{
#line 55 "parser.y"

	char *name;
	double dval;
	int ival;

#line 103 "parser.h"

};
typedef union YYSTYPE YYSTYPE;
# define YYSTYPE_IS_TRIVIAL 1
# define YYSTYPE_IS_DECLARED 1
#endif

/* Location type.  */
#if ! defined YYLTYPE && ! defined YYLTYPE_IS_DECLARED
typedef struct YYLTYPE YYLTYPE;
struct YYLTYPE
{
  int first_line;
  int first_column;
  int last_line;
  int last_column;
};
# define YYLTYPE_IS_DECLARED 1
# define YYLTYPE_IS_TRIVIAL 1
#endif



int yyparse (struct sql_context* parser_ctx);

#endif /* !YY_YY_PARSER_H_INCLUDED  */
