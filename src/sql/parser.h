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
typedef union {
	char *name;
	double dval;
	int ival;
} YYSTYPE;
extern YYSTYPE yylval;
