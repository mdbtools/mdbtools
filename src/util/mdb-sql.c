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
#include <config.h>

#include <stdio.h>

#ifdef HAVE_LIBREADLINE
#  if defined(HAVE_READLINE_READLINE_H)
#    include <readline/readline.h>
#  elif defined(HAVE_READLINE_H)
#    include <readline.h>
#  else /* !defined(HAVE_READLINE_H) */
extern char *readline ();
#  endif /* !defined(HAVE_READLINE_H) */
char *cmdline = NULL;
#else /* !defined(HAVE_READLINE_READLINE_H) */
/* no readline */
#endif /* HAVE_LIBREADLINE */

#ifdef HAVE_READLINE_HISTORY
#  if defined(HAVE_READLINE_HISTORY_H)
#    include <readline/history.h>
#  elif defined(HAVE_HISTORY_H)
#    include <history.h>
#  else /* !defined(HAVE_HISTORY_H) */
extern void add_history ();
extern int write_history ();
extern int read_history ();
#  endif /* defined(HAVE_READLINE_HISTORY_H) */
/* no history */
#endif /* HAVE_READLINE_HISTORY */

#include <string.h>
#include "mdbsql.h"

#ifdef DMALLOC
#include "dmalloc.h"
#endif

void dump_results(MdbSQL *sql);
void dump_results_pp(MdbSQL *sql);
int yyparse(void);

#if SQL

int headers = 1;
int footers = 1;
int pretty_print = 1;
char *delimiter;
int showplan = 0;
int noexec = 0;

#define HISTFILE ".mdbhistory"

#ifndef HAVE_LIBREADLINE
char *readline(char *prompt)
{
char *buf, line[1000];
int i = 0;

	printf("%s",prompt);
	if (! fgets(line,1000,stdin)) {
		return NULL;
	}
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
void add_history(char *s)
{
}
void read_history(char *s)
{
}
void write_history(char *s)
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

void
do_set_cmd(MdbSQL *sql, char *s)
{
	char *level1, *level2;
	level1 = strtok(s, " \t\n");
	if (!strcmp(level1,"stats")) {
		level2 = strtok(NULL, " \t");
		if (!level2) {
			printf("Usage: set stats [on|off]\n");
			return;
		}
		if (!strcmp(level2,"on")) {
			mdb_stats_on(sql->mdb);
		} else if (!strcmp(level2,"off")) {
			mdb_stats_off(sql->mdb);
			mdb_dump_stats(sql->mdb);
		} else {
			printf("Unknown stats option %s\n", level2);
		}
	} else if (!strcmp(level1,"showplan")) {
		level2 = strtok(NULL, " \t");
		if (!level2) {
			printf("Usage: set showplan [on|off]\n");
			return;
		}
		if (!strcmp(level2,"on")) {
			showplan=1;
		} else if (!strcmp(level2,"off")) {
			showplan=0;
		} else {
			printf("Unknown showplan option %s\n", level2);
		}
	} else if (!strcmp(level1,"noexec")) {
		level2 = strtok(NULL, " \t");
		if (!level2) {
			printf("Usage: set noexec [on|off]\n");
			return;
		}
		if (!strcmp(level2,"on")) {
			noexec=1;
		} else if (!strcmp(level2,"off")) {
			noexec=0;
		} else {
			printf("Unknown showplan option %s\n", level2);
		}
	} else {
		printf("Unknown set command %s\n", level1);
	}
}

int
read_file(char *s, int line, unsigned int *bufsz, char *mybuf)
{
	char *fname;
	FILE *in;
	char buf[256];
	unsigned int cursz = 0;	
	int lines = 0;	

	fname = s;
	while (*fname && *fname==' ') fname++;

	if (! (in = fopen(fname, "r"))) {
		fprintf(stderr,"Unable to open file %s\n", fname);
		mybuf[0]=0;
		return 0;
	}
	while (fgets(buf, 255, in)) {
		cursz += strlen(buf) + 1;
		if (cursz > (*bufsz)) {
			(*bufsz) *= 2;
			mybuf = (char *) realloc(mybuf, *bufsz);
		}	
		strcat(mybuf, buf);
		/* don't record blank lines */
		if (strlen(buf)) add_history(buf);
		strcat(mybuf, "\n");
		lines++;
		printf("%d => %s",line+lines, buf);
	}
	return lines;
}
void 
run_query(MdbSQL *sql, char *mybuf)
{
	MdbTableDef *table;

	if (!parse(sql, mybuf) && sql->cur_table) {
		if (showplan) {
			table = sql->cur_table;
			if (table->sarg_tree) mdb_sql_dump_node(table->sarg_tree, 0);
			if (sql->cur_table->strategy == MDB_TABLE_SCAN)
				printf("Table scanning %s\n", table->name);
			else 
				printf("Index scanning %s using %s\n", table->name, table->scan_idx->name);
		}
		if (noexec) {
			mdb_sql_reset(sql);
			return;
		}
		mdb_sql_bind_all(sql);
		if (pretty_print)
			dump_results_pp(sql);
		else
			dump_results(sql);
	}
}

void print_value(char *v, int sz, int first)
{
int i;
int vlen;

	if (first) {
		fprintf(stdout,"|");
	}
	vlen = strlen(v);
	for (i=0;i<sz;i++) {
		fprintf(stdout,"%c",i >= vlen ? ' ' : v[i]);
	}
	fprintf(stdout,"|");
}
static void print_break(int sz, int first)
{
int i;
	if (first) {
		fprintf(stdout,"+");
	}
	for (i=0;i<sz;i++) {
		fprintf(stdout,"-");
	}
	fprintf(stdout,"+");
}
void
dump_results(MdbSQL *sql)
{
	unsigned int j;
	MdbSQLColumn *sqlcol;
	unsigned long row_count = 0;

	if (headers) {
		for (j=0;j<sql->num_columns-1;j++) {
			sqlcol = g_ptr_array_index(sql->columns,j);
			fprintf(stdout, "%s%s", sqlcol->name, delimiter);
		}
		sqlcol = g_ptr_array_index(sql->columns,sql->num_columns-1);
		fprintf(stdout, "%s", sqlcol->name);
		fprintf(stdout,"\n");
	}
	while(mdb_fetch_row(sql->cur_table)) {
		row_count++;
  		for (j=0;j<sql->num_columns-1;j++) {
			sqlcol = g_ptr_array_index(sql->columns,j);
			fprintf(stdout, "%s%s", sql->bound_values[j], delimiter);
		}
		sqlcol = g_ptr_array_index(sql->columns,sql->num_columns-1);
		fprintf(stdout, "%s", sql->bound_values[sql->num_columns-1]);
		fprintf(stdout,"\n");
	}
	if (footers) {
		if (!row_count) 
			fprintf(stdout, "No Rows retrieved\n");
		else if (row_count==1)
			fprintf(stdout, "1 Row retrieved\n");
		else 
			fprintf(stdout, "%lu Rows retrieved\n", row_count);
	}
	mdb_sql_reset(sql);
}

void 
dump_results_pp(MdbSQL *sql)
{
	unsigned int j;
	MdbSQLColumn *sqlcol;
	unsigned long row_count = 0;
	/*
	int rows, rc, i;
	MdbHandle *mdb = sql->mdb;
	MdbFormatConstants *fmt = mdb->fmt;
	*/

	/* print header */
	if (headers) {
		for (j=0;j<sql->num_columns;j++) {
			sqlcol = g_ptr_array_index(sql->columns,j);
			if (strlen(sqlcol->name)>sqlcol->disp_size)
				sqlcol->disp_size = strlen(sqlcol->name);
			print_break(sqlcol->disp_size, !j);
		}
		fprintf(stdout,"\n");
		for (j=0;j<sql->num_columns;j++) {
			sqlcol = g_ptr_array_index(sql->columns,j);
			print_value(sqlcol->name,sqlcol->disp_size,!j);
		}
		fprintf(stdout,"\n");
	}

	for (j=0;j<sql->num_columns;j++) {
		sqlcol = g_ptr_array_index(sql->columns,j);
		print_break(sqlcol->disp_size, !j);
	}
	fprintf(stdout,"\n");

	/* print each row */
	while(mdb_fetch_row(sql->cur_table)) {
		row_count++;
  		for (j=0;j<sql->num_columns;j++) {
			sqlcol = g_ptr_array_index(sql->columns,j);
			print_value(sql->bound_values[j],sqlcol->disp_size,!j);
		}
		fprintf(stdout,"\n");
	}

	/* footer */
	for (j=0;j<sql->num_columns;j++) {
		sqlcol = g_ptr_array_index(sql->columns,j);
		print_break(sqlcol->disp_size, !j);
	}
	fprintf(stdout,"\n");
	if (footers) {
		if (!row_count) 
			fprintf(stdout, "No Rows retrieved\n");
		else if (row_count==1)
			fprintf(stdout, "1 Row retrieved\n");
		else 
			fprintf(stdout, "%lu Rows retrieved\n", row_count);
	}

	/* clean up */
	for (j=0;j<sql->num_columns;j++) {
		g_free(sql->bound_values[j]);
	}

	/* the column and table names are no good now */
	mdb_sql_reset(sql);
}

void myexit(int r)
{
	g_free(delimiter);
	exit(r);
}
int
main(int argc, char **argv)
{
char *s;
char prompt[20];
int line = 1;
char *mybuf;
unsigned int bufsz = 4096;
int done = 0;
MdbSQL *sql;
int opt;
FILE *in = NULL, *out = NULL;
char *home = getenv("HOME");
char *histpath;


	if (home) {
		histpath = (char *) g_strconcat(home, "/", HISTFILE, NULL);
		read_history(histpath);
		g_free(histpath);
	}
	if (!isatty(fileno(stdin))) {
		in = stdin;
	}
	while ((opt=getopt(argc, argv, "HFpd:i:o:"))!=-1) {
		switch (opt) {
		        case 'd':
				delimiter = (char *) g_strdup(optarg);
				break;
		        case 'p':
				pretty_print=0;
				break;
		        case 'H':
				headers=0;
				break;
		        case 'F':
				footers=0;
				break;
		        case 'i':
				if (!strcmp(optarg, "stdin"))
					in = stdin;
				else if (!(in = fopen(optarg, "r"))) {
					fprintf(stderr,"Unable to open file %s\n", optarg);
					exit(1);
				}
				break;
		        case 'o':
				if (!(out = fopen(optarg, "w"))) {
					fprintf(stderr,"Unable to open file %s\n", optarg);
					exit(1);
				}
				break;
			default:
				fprintf(stdout,"Unknown option.\nUsage: %s [-HFp] [-d <delimiter>] [-i <file>] [-o <file>] [<database>]\n", argv[0]);
				exit(1);
		}
	}
	if (!delimiter) {
		delimiter = (char *) g_strdup("\t");
	}
	/* initialize the SQL engine */
	sql = mdb_sql_init();
	if (argc>optind) {
		mdb_sql_open(sql, argv[optind]);
	}

	/* give the buffer an initial size */
	bufsz = 4096;
	mybuf = (char *) g_malloc(bufsz);
	mybuf[0]='\0';

	if (in) {
		s=malloc(256);
		if (!fgets(s, 256, in)) {
			if (s) free (s);
			g_free (mybuf);
			myexit(0);
		}
	} else {
		sprintf(prompt,"1 => ");
		s=readline(prompt);
		if (!s) done = 1;
		if (s && (!strcmp(s,"exit") || !strcmp(s,"quit") || !strcmp(s,"bye")))
			done = 1;
	}
	while (!done) {
		if (line==1 && !strncmp(s,"set ", 4)) {
			do_set_cmd(sql, &s[4]);
			line = 0;
		} else if (!strcmp(s,"go")) {
			line = 0;
			run_query(sql, mybuf);
			mybuf[0]='\0';
		} else if (!strcmp(s,"reset")) {
			line = 0;
			mybuf[0]='\0';
		} else if (!strncmp(s,":r",2)) {
			line += read_file(&s[2], line, &bufsz, mybuf);
		} else {
			while (strlen(mybuf) + strlen(s) > bufsz) {
				bufsz *= 2;
				mybuf = (char *) g_realloc(mybuf, bufsz);
			}
			/* don't record blank lines */
			if (strlen(s)) add_history(s);
			strcat(mybuf,s);
			/* preserve line numbering for the parser */
			strcat(mybuf,"\n");
		}
		if (s) free(s);
		if (in) {
			s=malloc(256);
			if (!fgets(s, 256, in)) {
				/* if we have something in the buffer, run it */
				if (strlen(mybuf)) 
					run_query(sql, mybuf);
				if (s) free (s);
				g_free (mybuf);
				myexit(0);
			}
			if (s[strlen(s)-1]=='\n') s[strlen(s)-1]=0;
		} else {
			sprintf(prompt,"%d => ",++line);
			s=readline(prompt);
			if (!s) done = 1;
		}
		if( s && (!strcmp(s,"exit") || !strcmp(s,"quit") || !strcmp(s,"bye"))) {
			done = 1;
		}
	}
	mdb_sql_exit(sql);	

	g_free(mybuf);
	if (s) free(s);

	if (home) {
		histpath = (char *) g_strconcat(home, "/", HISTFILE, NULL);
		write_history(histpath);
		g_free(histpath);
	}

	myexit(0);

	return 0; /* make gcc -Wall happy */
}
#else
int main(int argc, char **argv)
{
	fprintf(stderr,"You must configure using --enable-sql to get SQL support\n");

	exit(-1);
}
#endif
