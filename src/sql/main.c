#include <stdio.h>
#ifdef HAVE_READLINE
#include <readline/readline.h>
#endif
#include <string.h>
#include "mdbsql.h"

extern MdbSQL *g_sql;

char *g_input_ptr;

#ifndef HAVE_READLINE
char *readline(char *prompt)
{
char *buf, line[1000];
int i = 0;

	printf("%s",prompt);
	fgets(line,1000,stdin);
	for (i=strlen(line);i>0;i--) {
		if (line[i]=='\n') {
			line[i]='\0';
			break;
		}
	}
	buf = (char *) malloc(strlen(line)+1);
	strcpy(buf,line);

	return buf;
}
add_history(char *s)
{
}
#endif

int mdb_sql_yyinput(char *buf, int need)
{
int cplen, have;

	have = strlen(g_input_ptr);
	cplen = need > have ? have : need;
	
	if (cplen>0) {
		memcpy(buf, g_input_ptr, cplen);
		g_input_ptr += cplen;
	} 
	return cplen;
}

int parse(char *buf)
{
	g_input_ptr = buf;
	if (yyparse()) {
		fprintf(stderr, "Couldn't parse SQL\n");
		return 1;
	} else {
		return 0;
	}
}

main()
{
char *s;
char prompt[20];
int line = 1;
char *mybuf;
int bufsz = 4096;
int done = 0;

	/* initialize the SQL engine */
	g_sql = mdb_sql_init();

	/* give the buffer an initial size */
	bufsz = 4096;
	mybuf = (char *) malloc(bufsz);
	mybuf[0]='\0';

	sprintf(prompt,"1 => ");
	s=readline(prompt);
	while (!done) {
		if (!strcmp(s,"go")) {
			line = 0;
			parse(mybuf);
			mybuf[0]='\0';
		} else if (!strcmp(s,"reset")) {
			line = 0;
			mybuf[0]='\0';
		} else {
			add_history(s);
			strcat(mybuf,s);
			/* preserve line numbering for the parser */
			strcat(mybuf,"\n");
		}
		sprintf(prompt,"%d => ",++line);
		free(s);
		s=readline(prompt);
		if (!strcmp(s,"exit") || !strcmp(s,"quit") || !strcmp(s,"bye")) {
			done = 1;
		}
	}
	mdb_sql_exit(g_sql);	
}
