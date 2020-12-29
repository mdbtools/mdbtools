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
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <stdlib.h>
#include "mdbtools.h"

#define DEBUG 1

static __thread unsigned long opts;
static __thread int optset;

static void load_options(void);

void
mdb_debug(int klass, char *fmt, ...)
{
#ifdef DEBUG
	va_list ap;

	if (!optset) load_options();
	if (klass & opts) {	
    	va_start(ap, fmt);
    	vfprintf (stderr,fmt, ap);
    	va_end(ap);
    	fprintf(stderr,"\n");
	}
#endif
}

static void
load_options()
{
	char *opt;
	char *s;
    char *ctx;

    if (!optset && (s=getenv("MDBOPTS"))) {
		opt = strtok_r(s, ":", &ctx);
		while (opt) {
			if (!strcmp(opt, "use_index")) {
#ifdef HAVE_LIBMSWSTR
				opts |= MDB_USE_INDEX;
#else
				fprintf(stderr, "The 'use_index' argument was supplied to MDBOPTS environment variable. However, this feature requires the libmswstr library, which was not found when libmdb was compiled. As a result, the 'use_index' argument will be ignored.\n\nTo enable indexes, you will need to download libmswstr from https://github.com/leecher1337/libmswstr and then recompile libmdb. Note that the 'use_index' feature is largely untested, and may have unexpected results.\n\nTo suppress this warning, run the program again after removing the 'use_index' argument from the MDBOPTS environment variable.\n");
#endif
			}
        	if (!strcmp(opt, "no_memo")) opts |= MDB_NO_MEMO;
        	if (!strcmp(opt, "debug_like")) opts |= MDB_DEBUG_LIKE;
        	if (!strcmp(opt, "debug_write")) opts |= MDB_DEBUG_WRITE;
        	if (!strcmp(opt, "debug_usage")) opts |= MDB_DEBUG_USAGE;
        	if (!strcmp(opt, "debug_ole")) opts |= MDB_DEBUG_OLE;
        	if (!strcmp(opt, "debug_row")) opts |= MDB_DEBUG_ROW;
        	if (!strcmp(opt, "debug_props")) opts |= MDB_DEBUG_PROPS;
        	if (!strcmp(opt, "debug_all")) {
				opts |= MDB_DEBUG_LIKE;
				opts |= MDB_DEBUG_WRITE;
				opts |= MDB_DEBUG_USAGE;
				opts |= MDB_DEBUG_OLE;
				opts |= MDB_DEBUG_ROW;
				opts |= MDB_DEBUG_PROPS;
			}
			opt = strtok_r(NULL,":", &ctx);
		}
    }
	optset = 1;
}
int
mdb_get_option(unsigned long optnum)
{
	if (!optset) load_options();
	return ((opts & optnum) > 0);
}
