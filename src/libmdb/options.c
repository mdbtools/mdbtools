/* MDB Tools - A library for reading MS Access database file
 * Copyright (C) 2004 Brian Bruns
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
#include <string.h>
#include <stdarg.h>
#include <stdlib.h>

#include <mdbtools.h>

#ifdef DMALLOC
#include "dmalloc.h"
#endif

#define DEBUG 1

static unsigned long opts;
static int optset;

static void load_options();

void
mdb_debug(int klass, char *fmt, ...)
{
#ifdef DEBUG
	va_list ap;

	if (!optset) load_options();
	if (klass & opts) {	
    	va_start(ap, fmt);
    	vfprintf (stdout,fmt, ap);
    	va_end(ap);
    	fprintf(stdout,"\n");
	}
#endif
}

static void
load_options()
{
	char *opt;
	char *s;

    if (!optset && (s=getenv("MDBOPTS"))) {
		opt = strtok(s, ":");
		do {
        	if (!strcmp(opt, "use_index")) opts |= MDB_USE_INDEX;
        	if (!strcmp(opt, "debug_like")) opts |= MDB_DEBUG_LIKE;
        	if (!strcmp(opt, "debug_write")) opts |= MDB_DEBUG_WRITE;
        	if (!strcmp(opt, "debug_usage")) opts |= MDB_DEBUG_USAGE;
        	if (!strcmp(opt, "debug_ole")) opts |= MDB_DEBUG_OLE;
        	if (!strcmp(opt, "debug_row")) opts |= MDB_DEBUG_ROW;
        	if (!strcmp(opt, "debug_all")) {
				opts |= MDB_DEBUG_LIKE;
				opts |= MDB_DEBUG_WRITE;
				opts |= MDB_DEBUG_USAGE;
				opts |= MDB_DEBUG_OLE;
				opts |= MDB_DEBUG_ROW;
			}
			opt = strtok(NULL,":");
		} while (opt);
    }
	optset = 1;
}
int
mdb_get_option(unsigned long optnum)
{
	if (!optset) load_options();
	return ((opts & optnum) > 0);
}
