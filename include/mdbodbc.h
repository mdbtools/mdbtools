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
#ifndef _mdbodbc_h_
#define _mdbodbc_h_

#include <mdbtools.h>
#include <mdbsql.h>

#include <sql.h>
#include <sqlext.h>
#if defined(UNIXODBC)
# include <odbcinst.h>
#elif defined(IODBC)
# include <iodbcinst.h>
#endif


#ifdef __cplusplus
extern "C" {
#endif

static char  rcsid_sql_h [ ] =
         "$Id: mdbodbc.h,v 1.5 2005/01/15 05:02:13 calvinrsmith Exp $";
static void *no_unused_sql_h_warn[]={rcsid_sql_h, no_unused_sql_h_warn};

struct _henv {
	MdbSQL *sql;	
};
struct _hdbc {
	struct _henv *henv;
};
struct _hstmt {
	struct _hdbc *hdbc;
	/* reminder to self: the following is here for testing purposes.
	 * please make dynamic before checking in 
	 */
	char query[4096];
	struct _sql_bind_info *bind_head;
	int rows_affected;
};

struct _sql_bind_info {
	int column_number;
	int column_bindtype;
	int column_bindlen;
	int *column_lenbind;
	char *varaddr;
	struct _sql_bind_info *next;
};

#ifdef __cplusplus
}
#endif
#endif
