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

void dump_results(MdbSQL *sql);
void dump_results_pp(MdbSQL *sql);

#if SQL

int headers = 1;
int footers = 1;
int pretty_print = 1;
char *delimiter;

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

void
do_set_cmd(MdbSQL *sql, char *s)
{
	char *level1, *level2;
	level1 = strtok(s, " \t\n");
	if (!strcmp(level1,"stats")) {
		level2 = strtok(NULL, " \t");
		if (!strcmp(level2,"on")) {
			mdb_stats_on(sql->mdb);
		} else if (!strcmp(level2,"off")) {
			mdb_stats_off(sql->mdb);
			mdb_dump_stats(sql->mdb);
		} else {
			printf("Unknown stats option %s\n", level2);
		}
	} else {
		printf("Unknown set command %s\n", level1);
	}
}

void 
run_query(MdbSQL *sql, char *mybuf)
{
	if (!parse(sql, mybuf) && sql->cur_table) {
		mdbsql_bind_all(sql);
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
int j;
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
			fprintf(stdout, "%d Rows retrieved\n", row_count);
	}
	mdb_sql_reset(sql);
}

void 
dump_results_pp(MdbSQL *sql)
{
int j;
MdbSQLColumn *sqlcol;
unsigned long row_count = 0;

	/* print header */
	if (headers) {
		for (j=0;j<sql->num_columns;j++) {
			sqlcol = g_ptr_array_index(sql->columns,j);
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
			fprintf(stdout, "%d Rows retrieved\n", row_count);
	}

	/* clean up */
	for (j=0;j<sql->num_columns;j++) {
		if (sql->bound_values[j]) free(sql->bound_values[j]);
	}

	/* the column and table names are no good now */
	mdb_sql_reset(sql);
}

void myexit(int r)
{
	free(delimiter);
	exit(r);
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
int opt;
FILE *in = NULL, *out = NULL;


	if (!isatty(fileno(stdin))) {
		in = stdin;
	}
	while ((opt=getopt(argc, argv, "hfpd:i:o:"))!=-1) {
		switch (opt) {
		        case 'd':
				delimiter = malloc(strlen(optarg)+1);
				strcpy(delimiter, optarg);
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
		delimiter = strdup("\t");
	}
	/* initialize the SQL engine */
	sql = mdb_sql_init();
	if (argc>optind) {
		mdb_sql_open(sql, argv[optind]);
	}

	/* give the buffer an initial size */
	bufsz = 4096;
	mybuf = (char *) malloc(bufsz);
	mybuf[0]='\0';

	if (in) {
		s=malloc(256);
		if (!fgets(s, 256, in)) myexit(0);
	} else {
		sprintf(prompt,"1 => ");
		s=readline(prompt);
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
		free(s);
		if (in) {
			s=malloc(256);
			if (!fgets(s, 256, in)) {
				/* if we have something in the buffer, run it */
				if (strlen(mybuf)) 
					run_query(sql, mybuf);
				myexit(0);
			}
			if (s[strlen(s)-1]=='\n') s[strlen(s)-1]=0;
		} else {
			sprintf(prompt,"%d => ",++line);
			s=readline(prompt);
		}
		if (!strcmp(s,"exit") || !strcmp(s,"quit") || !strcmp(s,"bye")) {
			done = 1;
		}
	}
	mdb_sql_exit(sql);	

	myexit(0);
}
#else
int main(int argc, char **argv)
{
	fprintf(stderr,"You must configure using --enable-sql to get SQL support\n");

	exit(-1);
}
#endif
