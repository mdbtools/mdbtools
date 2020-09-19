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
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#ifndef _mdbodbc_h_
#define _mdbodbc_h_

#include <sql.h>
#include <sqlext.h>
#if defined(UNIXODBC)
# include <odbcinst.h>
#elif defined(IODBC)
# include <iodbcinst.h>
#endif

#include "mdbtools.h"
#include "mdbsql.h"
#include "connectparams.h"

#ifdef __cplusplus
extern "C" {
#endif

struct _henv {
	GPtrArray *connections;
    char sqlState[6];
};
struct _hdbc {
	struct _henv *henv;
	MdbSQL *sqlconn;
	ConnectParams* params;
	GPtrArray *statements;
    char lastError[256];
    char sqlState[6];
#ifdef ENABLE_ODBC_W
    iconv_t iconv_in;
    iconv_t iconv_out;
#endif
};
struct _hstmt {
	MdbSQL *sql;
	struct _hdbc *hdbc;
	/* reminder to self: the following is here for testing purposes.
	 * please make dynamic before checking in 
	 */
	char query[4096];
    char lastError[256];
    char sqlState[6];
    char *ole_str;
    size_t ole_len;
	struct _sql_bind_info *bind_head;
	int rows_affected;
	int icol; /* SQLGetData: last column */
	int pos; /* SQLGetData: last position (truncated result) */
};

struct _sql_bind_info {
	int column_number;
	int column_bindtype; /* type/conversion required */
	int column_bindlen; /* size of varaddr buffer */
	int *column_lenbind; /* where to store length of varaddr used */
	char *varaddr;
	struct _sql_bind_info *next;
};

#ifdef __cplusplus
}
#endif
#endif
