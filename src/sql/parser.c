#ifndef lint
static char yysccsid[] = "@(#)yaccpar	1.9 (Berkeley) 02/21/93";
#endif
#define YYBYACC 1
#define YYMAJOR 1
#define YYMINOR 9
#define yyclearin (yychar=(-1))
#define yyerrok (yyerrflag=0)
#define YYRECOVERING (yyerrflag!=0)
#define YYPREFIX "yy"
#line 2 "parser.y"
#include "mdbsql.h"

MdbSQL *g_sql;
#line 7 "parser.y"
typedef union {
	char *name;
	double dval;
	int ival;
} YYSTYPE;
#line 22 "y.tab.c"
#define NAME 257
#define PATH 258
#define NUMBER 259
#define STRING 260
#define SELECT 261
#define FROM 262
#define WHERE 263
#define CONNECT 264
#define DISCONNECT 265
#define TO 266
#define LIST 267
#define TABLES 268
#define AND 269
#define DESCRIBE 270
#define TABLE 271
#define LTEQ 272
#define GTEQ 273
#define LIKE 274
#define YYERRCODE 256
short yylhs[] = {                                        -1,
    0,    0,    0,    0,    0,    3,    3,    5,    5,    6,
    6,    7,    7,    7,    7,    7,    7,    8,    8,    4,
    4,    2,    1,    1,    1,    9,
};
short yylen[] = {                                         2,
    5,    3,    1,    3,    2,    0,    2,    1,    3,    3,
    3,    1,    1,    1,    1,    1,    1,    1,    1,    1,
    1,    1,    1,    1,    3,    1,
};
short yydefred[] = {                                      0,
    0,    0,    3,    0,    0,    0,   26,   23,    0,    0,
    0,    5,    0,    0,    0,   21,   20,    2,   22,    4,
    0,   25,    0,    1,    0,   18,   19,    7,    0,    0,
   15,   16,   17,   12,   13,   14,    0,    0,    0,   10,
    9,   11,
};
short yydgoto[] = {                                       6,
    9,   20,   24,   18,   28,   29,   37,   30,   10,
};
short yysindex[] = {                                   -254,
  -42, -263,    0, -250, -252,    0,    0,    0, -242,  -27,
 -256,    0, -236, -236,  -42,    0,    0,    0,    0,    0,
 -241,    0, -245,    0,  -56,    0,    0,    0, -246,  -56,
    0,    0,    0,    0,    0,    0, -251, -245, -233,    0,
    0,    0,
};
short yyrindex[] = {                                      0,
    0,    0,    0,    0,    0,    0,    0,    0,    0, -237,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
   26,    0,    0,    0,    0,    0,    0,    0,   27,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,
};
short yygindex[] = {                                      0,
   13,   15,    0,    0,   -8,    0,    1,   -5,    0,
};
#define YYTABLESIZE 218
short yytable[] = {                                       8,
   16,   17,   11,   36,   34,   35,    1,   26,   27,    2,
    3,   25,    4,   26,   27,    5,   15,   12,   13,   14,
   19,   23,   38,   42,   24,    6,    8,   22,   21,   41,
   39,   40,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    7,   31,   32,   33,
};
short yycheck[] = {                                      42,
  257,  258,  266,   60,   61,   62,  261,  259,  260,  264,
  265,  257,  267,  259,  260,  270,   44,  268,  271,  262,
  257,  263,  269,  257,  262,    0,    0,   15,   14,   38,
   30,   37,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
   -1,   -1,   -1,   -1,  257,  272,  273,  274,
};
#define YYFINAL 6
#ifndef YYDEBUG
#define YYDEBUG 0
#endif
#define YYMAXTOKEN 274
#if YYDEBUG
char *yyname[] = {
"end-of-file",0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
0,0,0,0,0,0,0,0,"'*'",0,"','",0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,"'<'","'='","'>'",0,
0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,"NAME","PATH",
"NUMBER","STRING","SELECT","FROM","WHERE","CONNECT","DISCONNECT","TO","LIST",
"TABLES","AND","DESCRIBE","TABLE","LTEQ","GTEQ","LIKE",
};
char *yyrule[] = {
"$accept : query",
"query : SELECT column_list FROM table where_clause",
"query : CONNECT TO database",
"query : DISCONNECT",
"query : DESCRIBE TABLE table",
"query : LIST TABLES",
"where_clause :",
"where_clause : WHERE sarg_list",
"sarg_list : sarg",
"sarg_list : sarg AND sarg_list",
"sarg : NAME operator constant",
"sarg : constant operator NAME",
"operator : '='",
"operator : '>'",
"operator : '<'",
"operator : LTEQ",
"operator : GTEQ",
"operator : LIKE",
"constant : NUMBER",
"constant : STRING",
"database : PATH",
"database : NAME",
"table : NAME",
"column_list : '*'",
"column_list : column",
"column_list : column ',' column_list",
"column : NAME",
};
#endif
#ifdef YYSTACKSIZE
#undef YYMAXDEPTH
#define YYMAXDEPTH YYSTACKSIZE
#else
#ifdef YYMAXDEPTH
#define YYSTACKSIZE YYMAXDEPTH
#else
#define YYSTACKSIZE 500
#define YYMAXDEPTH 500
#endif
#endif
int yydebug;
int yynerrs;
int yyerrflag;
int yychar;
short *yyssp;
YYSTYPE *yyvsp;
YYSTYPE yyval;
YYSTYPE yylval;
short yyss[YYSTACKSIZE];
YYSTYPE yyvs[YYSTACKSIZE];
#define yystacksize YYSTACKSIZE
#define YYABORT goto yyabort
#define YYREJECT goto yyabort
#define YYACCEPT goto yyaccept
#define YYERROR goto yyerrlab
int
yyparse()
{
    register int yym, yyn, yystate;
#if YYDEBUG
    register char *yys;
    extern char *getenv();

    if (yys = getenv("YYDEBUG"))
    {
        yyn = *yys;
        if (yyn >= '0' && yyn <= '9')
            yydebug = yyn - '0';
    }
#endif

    yynerrs = 0;
    yyerrflag = 0;
    yychar = (-1);

    yyssp = yyss;
    yyvsp = yyvs;
    *yyssp = yystate = 0;

yyloop:
    if (yyn = yydefred[yystate]) goto yyreduce;
    if (yychar < 0)
    {
        if ((yychar = yylex()) < 0) yychar = 0;
#if YYDEBUG
        if (yydebug)
        {
            yys = 0;
            if (yychar <= YYMAXTOKEN) yys = yyname[yychar];
            if (!yys) yys = "illegal-symbol";
            printf("%sdebug: state %d, reading %d (%s)\n",
                    YYPREFIX, yystate, yychar, yys);
        }
#endif
    }
    if ((yyn = yysindex[yystate]) && (yyn += yychar) >= 0 &&
            yyn <= YYTABLESIZE && yycheck[yyn] == yychar)
    {
#if YYDEBUG
        if (yydebug)
            printf("%sdebug: state %d, shifting to state %d\n",
                    YYPREFIX, yystate, yytable[yyn]);
#endif
        if (yyssp >= yyss + yystacksize - 1)
        {
            goto yyoverflow;
        }
        *++yyssp = yystate = yytable[yyn];
        *++yyvsp = yylval;
        yychar = (-1);
        if (yyerrflag > 0)  --yyerrflag;
        goto yyloop;
    }
    if ((yyn = yyrindex[yystate]) && (yyn += yychar) >= 0 &&
            yyn <= YYTABLESIZE && yycheck[yyn] == yychar)
    {
        yyn = yytable[yyn];
        goto yyreduce;
    }
    if (yyerrflag) goto yyinrecovery;
#ifdef lint
    goto yynewerror;
#endif
yynewerror:
    yyerror("syntax error");
#ifdef lint
    goto yyerrlab;
#endif
yyerrlab:
    ++yynerrs;
yyinrecovery:
    if (yyerrflag < 3)
    {
        yyerrflag = 3;
        for (;;)
        {
            if ((yyn = yysindex[*yyssp]) && (yyn += YYERRCODE) >= 0 &&
                    yyn <= YYTABLESIZE && yycheck[yyn] == YYERRCODE)
            {
#if YYDEBUG
                if (yydebug)
                    printf("%sdebug: state %d, error recovery shifting\
 to state %d\n", YYPREFIX, *yyssp, yytable[yyn]);
#endif
                if (yyssp >= yyss + yystacksize - 1)
                {
                    goto yyoverflow;
                }
                *++yyssp = yystate = yytable[yyn];
                *++yyvsp = yylval;
                goto yyloop;
            }
            else
            {
#if YYDEBUG
                if (yydebug)
                    printf("%sdebug: error recovery discarding state %d\n",
                            YYPREFIX, *yyssp);
#endif
                if (yyssp <= yyss) goto yyabort;
                --yyssp;
                --yyvsp;
            }
        }
    }
    else
    {
        if (yychar == 0) goto yyabort;
#if YYDEBUG
        if (yydebug)
        {
            yys = 0;
            if (yychar <= YYMAXTOKEN) yys = yyname[yychar];
            if (!yys) yys = "illegal-symbol";
            printf("%sdebug: state %d, error recovery discards token %d (%s)\n",
                    YYPREFIX, yystate, yychar, yys);
        }
#endif
        yychar = (-1);
        goto yyloop;
    }
yyreduce:
#if YYDEBUG
    if (yydebug)
        printf("%sdebug: state %d, reducing by rule %d (%s)\n",
                YYPREFIX, yystate, yyn, yyrule[yyn]);
#endif
    yym = yylen[yyn];
    yyval = yyvsp[1-yym];
    switch (yyn)
    {
case 1:
#line 21 "parser.y"
{
			mdb_sql_select(g_sql);	
		}
break;
case 2:
#line 24 "parser.y"
{ 
			mdb_sql_open(g_sql, yyvsp[0].name); free(yyvsp[0].name); 
		}
break;
case 3:
#line 27 "parser.y"
{ 
			mdb_sql_close(g_sql);
		}
break;
case 4:
#line 30 "parser.y"
{ 
			mdb_sql_describe_table(g_sql); 
		}
break;
case 5:
#line 33 "parser.y"
{ 
			mdb_sql_listtables(g_sql); 
		}
break;
case 10:
#line 49 "parser.y"
{ 
				mdb_sql_add_sarg(g_sql, yyvsp[-2].name, yyvsp[-1].ival, yyvsp[0].name);
				free(yyvsp[-2].name);
				free(yyvsp[0].name);
				}
break;
case 11:
#line 54 "parser.y"
{
				mdb_sql_add_sarg(g_sql, yyvsp[0].name, yyvsp[-1].ival, yyvsp[-2].name);
				free(yyvsp[-2].name);
				free(yyvsp[0].name);
				}
break;
case 12:
#line 62 "parser.y"
{ yyval.ival = MDB_EQUAL; }
break;
case 13:
#line 63 "parser.y"
{ yyval.ival = MDB_GT; }
break;
case 14:
#line 64 "parser.y"
{ yyval.ival = MDB_LT; }
break;
case 15:
#line 65 "parser.y"
{ yyval.ival = MDB_LTEQ; }
break;
case 16:
#line 66 "parser.y"
{ yyval.ival = MDB_GTEQ; }
break;
case 17:
#line 67 "parser.y"
{ yyval.ival = MDB_LIKE; }
break;
case 18:
#line 70 "parser.y"
{ yyval.name = yyvsp[0].name; }
break;
case 19:
#line 71 "parser.y"
{ yyval.name = yyvsp[0].name; }
break;
case 22:
#line 79 "parser.y"
{ mdb_sql_add_table(g_sql, yyvsp[0].name); free(yyvsp[0].name); }
break;
case 23:
#line 83 "parser.y"
{ mdb_sql_all_columns(g_sql); }
break;
case 26:
#line 89 "parser.y"
{ mdb_sql_add_column(g_sql, yyvsp[0].name); free(yyvsp[0].name); }
break;
#line 427 "y.tab.c"
    }
    yyssp -= yym;
    yystate = *yyssp;
    yyvsp -= yym;
    yym = yylhs[yyn];
    if (yystate == 0 && yym == 0)
    {
#if YYDEBUG
        if (yydebug)
            printf("%sdebug: after reduction, shifting from state 0 to\
 state %d\n", YYPREFIX, YYFINAL);
#endif
        yystate = YYFINAL;
        *++yyssp = YYFINAL;
        *++yyvsp = yyval;
        if (yychar < 0)
        {
            if ((yychar = yylex()) < 0) yychar = 0;
#if YYDEBUG
            if (yydebug)
            {
                yys = 0;
                if (yychar <= YYMAXTOKEN) yys = yyname[yychar];
                if (!yys) yys = "illegal-symbol";
                printf("%sdebug: state %d, reading %d (%s)\n",
                        YYPREFIX, YYFINAL, yychar, yys);
            }
#endif
        }
        if (yychar == 0) goto yyaccept;
        goto yyloop;
    }
    if ((yyn = yygindex[yym]) && (yyn += yystate) >= 0 &&
            yyn <= YYTABLESIZE && yycheck[yyn] == yystate)
        yystate = yytable[yyn];
    else
        yystate = yydgoto[yym];
#if YYDEBUG
    if (yydebug)
        printf("%sdebug: after reduction, shifting from state %d \
to state %d\n", YYPREFIX, *yyssp, yystate);
#endif
    if (yyssp >= yyss + yystacksize - 1)
    {
        goto yyoverflow;
    }
    *++yyssp = yystate;
    *++yyvsp = yyval;
    goto yyloop;
yyoverflow:
    yyerror("yacc stack overflow");
yyabort:
    return (1);
yyaccept:
    return (0);
}
