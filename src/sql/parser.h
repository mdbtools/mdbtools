#define NAME 257
#define PATH 258
#define NUMBER 259
#define STRING 260
#define SELECT 261
#define FROM 262
#define WHERE 263
#define CONNECT 264
#define TO 265
#define LIST 266
#define TABLES 267
#define AND 268
typedef union {
	char *name;
	double dval;
	int ival;
} YYSTYPE;
extern YYSTYPE yylval;
