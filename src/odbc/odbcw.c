/* MDB Tools - A library for reading MS Access database file
 * Copyright (C) 2000-2021 Brian Bruns and others
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

/* For a full list of functions that could be implemented, see:
 * https://docs.microsoft.com/en-us/sql/odbc/reference/develop-app/unicode-function-arguments?view=sql-server-ver15 */

#define SQL_NOUNICODEMAP
#define UNICODE

#include <sql.h>
#include <sqlext.h>
#include <string.h>
#include <stdio.h>
#include <limits.h>
#include "mdbodbc.h"

/** \addtogroup odbc
 *  @{
 */

//#define TRACE(x) fprintf(stderr,"Function %s\n", x);
#define TRACE(x)

static size_t unicode2ascii(struct _hdbc* dbc, const SQLWCHAR *_in, size_t _in_count, SQLCHAR *_out, size_t _out_len){
    wchar_t *w = malloc((_in_count + 1) * sizeof(wchar_t));
    size_t i;
    size_t count = 0;
    for (i=0; i<_in_count; i++) {
        w[i] = _in[i]; // wchar_t might be larger than SQLWCHAR
    }
    w[_in_count] = '\0';

#if defined(_WIN32) || defined(WIN32) || defined(_WIN64) || defined(WIN64) || defined(WINDOWS)
    count = _wcstombs_l((char *)_out, w, _out_len, dbc->locale);
#elif defined(HAVE_WCSTOMBS_L)
    count = wcstombs_l((char *)_out, w, _out_len, dbc->locale);
#else
    locale_t oldlocale = uselocale(dbc->locale);
    count = wcstombs((char *)_out, w, _out_len);
    uselocale(oldlocale);
#endif
    free(w);
    if (count == (size_t)-1)
        return 0;

    if (count < _out_len)
        _out[count] = '\0';

    return count;
}

static int sqlwlen(SQLWCHAR *p){
	int r=0;
	for(;*p;r++)
		p++;
	return r;
}

SQLRETURN SQL_API SQLDriverConnectW(
    SQLHDBC            hdbc,
    SQLHWND            hwnd,
    SQLWCHAR           *szConnStrIn,
    SQLSMALLINT        cbConnStrIn,
    SQLWCHAR           *szConnStrOut,
    SQLSMALLINT        cbConnStrOutMax,
    SQLSMALLINT       *pcbConnStrOut,
    SQLUSMALLINT       fDriverCompletion)
{
	TRACE("SQLDriverConnectW");
	if(cbConnStrIn==SQL_NTS)cbConnStrIn=sqlwlen(szConnStrIn);
	{
		size_t l = cbConnStrIn*4;
		SQLCHAR *tmp = malloc(l+1);
		SQLRETURN ret;
		l = unicode2ascii((struct _hdbc *)hdbc, szConnStrIn, cbConnStrIn, tmp, l);
		ret = SQLDriverConnect(hdbc,hwnd,tmp,SQL_NTS,NULL,0,pcbConnStrOut,fDriverCompletion);
		free(tmp);
		if (szConnStrOut && cbConnStrOutMax>0)
			szConnStrOut[0] = 0;
		if (pcbConnStrOut)
			*pcbConnStrOut = 0;
		return ret;
	}
}

SQLRETURN SQL_API SQLConnectW(
    SQLHDBC            hdbc,
    SQLWCHAR           *szDSN,
    SQLSMALLINT        cbDSN,
    SQLWCHAR           *szUID,
    SQLSMALLINT        cbUID,
    SQLWCHAR           *szAuthStr,
    SQLSMALLINT        cbAuthStr)
{
	TRACE("SQLConnectW");
	if(cbDSN==SQL_NTS)cbDSN=sqlwlen(szDSN);
	if(cbUID==SQL_NTS)cbUID=sqlwlen(szUID);
	if(cbAuthStr==SQL_NTS)cbAuthStr=sqlwlen(szAuthStr);
	{
		size_t l1=cbDSN*4;
		size_t l2=cbUID*4;
		size_t l3=cbAuthStr*4;
		SQLCHAR *tmp1=calloc(l1,1),*tmp2=calloc(l2,1),*tmp3=calloc(l3,1);
		SQLRETURN ret;
		l1 = unicode2ascii((struct _hdbc *)hdbc, szDSN, cbDSN, tmp1, l1);
		l2 = unicode2ascii((struct _hdbc *)hdbc, szUID, cbUID, tmp2, l2);
		l3 = unicode2ascii((struct _hdbc *)hdbc, szAuthStr, cbAuthStr, tmp3, l3);
		ret = SQLConnect(hdbc, tmp1, l1, tmp2, l2, tmp3, l3);
		free(tmp1),free(tmp2),free(tmp3);
		return ret;
	}
}

SQLRETURN SQL_API SQLDescribeColW(
    SQLHSTMT           hstmt,
    SQLUSMALLINT       icol,
    SQLWCHAR           *szColName,
    SQLSMALLINT        cbColNameMax,
    SQLSMALLINT       *pcbColName,
    SQLSMALLINT       *pfSqlType,
    SQLULEN           *pcbColDef, /* precision */
    SQLSMALLINT       *pibScale,
    SQLSMALLINT       *pfNullable)
{
	TRACE("SQLDescribeColW");
	if(cbColNameMax==SQL_NTS)cbColNameMax=sqlwlen(szColName);
	{
		size_t l=cbColNameMax*4+1;
		SQLCHAR *tmp=calloc(l,1);
		SQLRETURN ret = SQLDescribeCol(hstmt, icol, tmp, l, (SQLSMALLINT*)&l, pfSqlType, pcbColDef, pibScale, pfNullable);
		*pcbColName = _mdb_odbc_ascii2unicode(((struct _hstmt*)hstmt)->hdbc, (char*)tmp, l, szColName, cbColNameMax);
		free(tmp);
		return ret;
	}
}

SQLRETURN SQL_API SQLColAttributesW(
    SQLHSTMT           hstmt,
    SQLUSMALLINT       icol,
    SQLUSMALLINT       fDescType,
    SQLPOINTER         rgbDesc,
    SQLSMALLINT        cbDescMax,
    SQLSMALLINT       *pcbDesc,
    SQLLEN            *pfDesc)
{
	TRACE("SQLColAttributesW");
	if (fDescType!=SQL_COLUMN_NAME && fDescType!=SQL_COLUMN_LABEL)
		return SQLColAttributes(hstmt,icol,fDescType,rgbDesc,cbDescMax,pcbDesc,pfDesc);
	else{
		size_t l=cbDescMax*4+1;
		SQLCHAR *tmp=calloc(l,1);
		SQLRETURN ret=SQLColAttributes(hstmt,icol,fDescType,tmp,l,(SQLSMALLINT*)&l,pfDesc);
		*pcbDesc = _mdb_odbc_ascii2unicode(((struct _hstmt *)hstmt)->hdbc, (char*)tmp, l, (SQLWCHAR*)rgbDesc, cbDescMax);
		free(tmp);
		return ret;
	}
}

SQLRETURN SQL_API SQLErrorW(
    SQLHENV            henv,
    SQLHDBC            hdbc,
    SQLHSTMT           hstmt,
    SQLWCHAR          *szSqlState,
    SQLINTEGER        *pfNativeError,
    SQLWCHAR          *szErrorMsg,
    SQLSMALLINT        cbErrorMsgMax,
    SQLSMALLINT       *pcbErrorMsg)
{
	SQLCHAR szSqlState8[6];
	SQLCHAR szErrorMsg8[3*cbErrorMsgMax+1];
	SQLSMALLINT pcbErrorMsg8;
	SQLRETURN result;

	TRACE("SQLErrorW");

	result = SQLError(henv, hdbc, hstmt, szSqlState8, pfNativeError, szErrorMsg8, 3*cbErrorMsgMax+1, &pcbErrorMsg8);
	if (result == SQL_SUCCESS) {
        struct _hdbc *dbc = hstmt ? ((struct _hstmt *)hstmt)->hdbc : hdbc;
		size_t pcb;
		_mdb_odbc_ascii2unicode(dbc, (char*)szSqlState8, sizeof(szSqlState8), szSqlState, sizeof(szSqlState8));
		pcb = _mdb_odbc_ascii2unicode(dbc, (char*)szErrorMsg8, pcbErrorMsg8, szErrorMsg, cbErrorMsgMax);
		if (pcbErrorMsg) *pcbErrorMsg = pcb;
	}
	return result;
}

SQLRETURN SQL_API SQLExecDirectW(
    SQLHSTMT           hstmt,
    SQLWCHAR           *szSqlStr,
    SQLINTEGER         cbSqlStr)
{
	TRACE("SQLExecDirectW");
	if(cbSqlStr==SQL_NTS)cbSqlStr=sqlwlen(szSqlStr);
	{
		size_t l=cbSqlStr*4;
		SQLCHAR *tmp=calloc(l,1);
		SQLRETURN ret;
		l = unicode2ascii(((struct _hstmt *)hstmt)->hdbc, szSqlStr, cbSqlStr, tmp, l);
		ret = SQLExecDirect(hstmt, tmp, l);
		TRACE("SQLExecDirectW end");
		free(tmp);
		return ret;
	}
}

SQLRETURN SQL_API SQLColumnsW(
    SQLHSTMT           hstmt,
    SQLWCHAR           *szCatalogName,
    SQLSMALLINT        cbCatalogName,
    SQLWCHAR           *szSchemaName,
    SQLSMALLINT        cbSchemaName,
    SQLWCHAR           *szTableName,
    SQLSMALLINT        cbTableName,
    SQLWCHAR           *szColumnName,
    SQLSMALLINT        cbColumnName)
{
	if(cbTableName==SQL_NTS)cbTableName=sqlwlen(szTableName);
	{
		size_t l=cbTableName*4;
		SQLCHAR *tmp=calloc(l,1);
		SQLRETURN ret;
		l = unicode2ascii(((struct _hstmt* )hstmt)->hdbc, szTableName, cbTableName, tmp, l);
		ret = SQLColumns(hstmt, NULL, 0, NULL, 0, tmp, l, NULL, 0);
		free(tmp);
		return ret;
	}
}

SQLRETURN SQL_API SQLGetInfoW(
    SQLHDBC            hdbc,
    SQLUSMALLINT       fInfoType,
    SQLPOINTER         rgbInfoValue,
    SQLSMALLINT        cbInfoValueMax,
    SQLSMALLINT   *pcbInfoValue)
{
	TRACE("SQLGetInfoW");

	if(fInfoType==SQL_MAX_STATEMENT_LEN||fInfoType==SQL_SCHEMA_USAGE||fInfoType==SQL_CATALOG_LOCATION)
		return SQLGetInfo(hdbc,fInfoType,rgbInfoValue,cbInfoValueMax,pcbInfoValue);

	size_t l=cbInfoValueMax*4+1;
	SQLCHAR *tmp=calloc(l,1);
	SQLRETURN ret = SQLGetInfo(hdbc, fInfoType, tmp, l, (SQLSMALLINT*)&l);
	size_t pcb = _mdb_odbc_ascii2unicode((struct _hdbc *)hdbc, (char*)tmp, l, (SQLWCHAR*)rgbInfoValue, cbInfoValueMax);
	if(pcbInfoValue)*pcbInfoValue=pcb;
	free(tmp);
	return ret;
}

/** @}*/
