/* MDB Tools - A library for reading MS Access database file
 * Copyright (C) 2000 Brian Bruns
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include <stdio.h>
#ifdef HAVE_READLINE
#include <readline/readline.h>
#endif
#include <string.h>
#include "mdbsql.h"

#if SQL

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

int parse(MdbSQL *sql, char *buf)
{
	g_input_ptr = buf;
	/* begin unsafe */
	_mdb_sql(sql);
	if (yyparse()) {
		/* end unsafe */
		fprintf(stderr, "Couldn't parse SQL\n");
		mdb_sql_reset(sql);
		return 1;
	} else {
		return 0;
	}
}

main(int argc, char **argv)
{
char *s;
char prompt[20];
int line = 1;
char *mybuf;
int bufsz = 4096;
int done = 0;
MdbSQL *sql;

	/* initialize the SQL engine */
	sql = mdb_sql_init();
	if (argc>1) {
		mdb_sql_open(sql, argv[1]);
	}

	/* give the buffer an initial size */
	bufsz = 4096;
	mybuf = (char *) malloc(bufsz);
	mybuf[0]='\0';

	sprintf(prompt,"1 => ");
	s=readline(prompt);
	while (!done) {
		if (!strcmp(s,"go")) {
			line = 0;
			if (!parse(sql, mybuf) && sql->cur_table) {
				mdbsql_bind_all(sql);
				mdbsql_dump_results(sql);
			}
			mybuf[0]='\0';
		} else if (!strcmp(s,"reset")) {
			line = 0;
			mybuf[0]='\0';
		} else {
			while (strlen(mybuf) + strlen(s) > bufsz) {
				bufsz *= 2;
				mybuf = (char *) realloc(mybuf, bufsz);
			}
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
	mdb_sql_exit(sql);	
}
#else
int main(int argc, char **argv)
{
	fprintf(stderr,"You must configure using --enable-sql to get SQL support\n");
}
#endif
