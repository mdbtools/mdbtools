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

static SQLSMALLINT _odbc_get_client_type(MdbColumn *col);
static const char * _odbc_get_client_type_name(MdbColumn *col);
static int _odbc_fix_literals(struct _hstmt *stmt);
//static int _odbc_get_server_type(int clt_type);
static int _odbc_get_string_size(int size, SQLCHAR *str);

static void unbind_columns (struct _hstmt*);

#define FILL_FIELD(f,v,s) mdb_fill_temp_field(f,v,s,0,0,0,0)

typedef struct {
	SQLCHAR *type_name;
	SQLSMALLINT data_type;
	SQLINTEGER column_size;
	SQLCHAR *literal_prefix;
	SQLCHAR *literal_suffix;
	SQLCHAR *create_params;
	SQLSMALLINT nullable;
	SQLSMALLINT case_sensitive;
	SQLSMALLINT searchable;
	SQLSMALLINT *unsigned_attribute;
	SQLSMALLINT fixed_prec_scale;
	SQLSMALLINT auto_unique_value;
	SQLCHAR *local_type_name;
	SQLSMALLINT minimum_scale;
	SQLSMALLINT maximum_scale;
	SQLSMALLINT sql_data_type;
	SQLSMALLINT *sql_datetime_sub;
	SQLSMALLINT *num_prec_radix;
	SQLSMALLINT *interval_precision;
} TypeInfo;

TypeInfo type_info[] = {
	{(SQLCHAR*)"text", SQL_VARCHAR, 255, NULL, NULL, NULL, SQL_TRUE, SQL_TRUE, SQL_TRUE, NULL, SQL_FALSE, SQL_FALSE, NULL, 0, 255, SQL_VARCHAR, NULL, NULL, NULL},
	{(SQLCHAR*)"memo", SQL_VARCHAR, 4096, NULL, NULL, NULL, SQL_TRUE, SQL_TRUE, SQL_TRUE, NULL, SQL_FALSE, SQL_FALSE, NULL, 0, 4096, SQL_VARCHAR, NULL, NULL, NULL},
	{(SQLCHAR*)"ole", SQL_VARCHAR, MDB_BIND_SIZE, NULL, NULL, NULL, SQL_TRUE, SQL_TRUE, SQL_TRUE, NULL, SQL_FALSE, SQL_FALSE, NULL, 0, MDB_BIND_SIZE, SQL_VARCHAR, NULL, NULL, NULL},
	{(SQLCHAR*)"text", SQL_CHAR, 255, NULL, NULL, NULL, SQL_TRUE, SQL_TRUE, SQL_TRUE, NULL, SQL_FALSE, SQL_FALSE, NULL, 0, 255, SQL_CHAR, NULL, NULL, NULL},
	{(SQLCHAR*)"numeric", SQL_NUMERIC, 255, NULL, NULL, NULL, SQL_TRUE, SQL_TRUE, SQL_TRUE, NULL, SQL_FALSE, SQL_FALSE, NULL, 0, 255, SQL_NUMERIC, NULL, NULL, NULL},
	{(SQLCHAR*)"numeric", SQL_DECIMAL, 255, NULL, NULL, NULL, SQL_TRUE, SQL_TRUE, SQL_TRUE, NULL, SQL_FALSE, SQL_FALSE, NULL, 0, 255, SQL_DECIMAL, NULL, NULL, NULL},
	{(SQLCHAR*)"long integer", SQL_INTEGER, 4, NULL, NULL, NULL, SQL_TRUE, SQL_TRUE, SQL_TRUE, NULL, SQL_FALSE, SQL_TRUE, NULL, 0, 4, SQL_INTEGER, NULL, NULL, NULL},
	{(SQLCHAR*)"integer", SQL_SMALLINT, 4, NULL, NULL, NULL, SQL_TRUE, SQL_TRUE, SQL_TRUE, NULL, SQL_FALSE, SQL_TRUE, NULL, 0, 4, SQL_SMALLINT, NULL, NULL, NULL},
	{(SQLCHAR*)"integer", SQL_SMALLINT, 4, NULL, NULL, NULL, SQL_TRUE, SQL_TRUE, SQL_TRUE, NULL, SQL_FALSE, SQL_TRUE, NULL, 0, 4, SQL_SMALLINT, NULL, NULL, NULL},
	{(SQLCHAR*)"single", SQL_REAL, 4, NULL, NULL, NULL, SQL_TRUE, SQL_TRUE, SQL_TRUE, NULL, SQL_FALSE, SQL_TRUE, NULL, 0, 4, SQL_REAL, NULL, NULL, NULL},
	{(SQLCHAR*)"double", SQL_DOUBLE, 8, NULL, NULL, NULL, SQL_TRUE, SQL_TRUE, SQL_TRUE, NULL, SQL_FALSE, SQL_TRUE, NULL, 0, 8, SQL_FLOAT, NULL, NULL, NULL},
	{(SQLCHAR*)"datetime", SQL_DATETIME, 8, NULL, NULL, NULL, SQL_TRUE, SQL_TRUE, SQL_TRUE, NULL, SQL_FALSE, SQL_TRUE, NULL, 0, 8, SQL_DATETIME, NULL, NULL, NULL}
};

#define NUM_TYPE_INFO_COLS 19
#define MAX_TYPE_INFO 11

static void LogHandleError(struct _hdbc* dbc, const char* format, ...)
{
    va_list argp;
    va_start(argp, format);
    vsnprintf(dbc->lastError, sizeof(dbc->lastError), format, argp);
    va_end(argp);
}

static void LogStatementError(struct _hstmt* stmt, const char* format, ...)
{
    va_list argp;
    va_start(argp, format);
    vsnprintf(stmt->lastError, sizeof(stmt->lastError), format, argp);
    va_end(argp);
}

static SQLRETURN do_connect (
   SQLHDBC hdbc,
   char *database)
{
	struct _hdbc *dbc = (struct _hdbc *) hdbc;

	if (mdb_sql_open(dbc->sqlconn, database))
	{
		// ODBC requires ISO format dates, see
		// https://docs.microsoft.com/en-us/sql/relational-databases/native-client-odbc-date-time/datetime-data-type-conversions-odbc?view=sql-server-ver15
		mdb_set_date_fmt( dbc->sqlconn->mdb, "%F %H:%M:%S" );
		mdb_set_shortdate_fmt( dbc->sqlconn->mdb, "%F" );
		// Match formatting of REPID type values to MS Access ODBC driver
		mdb_set_repid_fmt( dbc->sqlconn->mdb, MDB_NOBRACES_4_2_2_2_6 );
		return SQL_SUCCESS;
	}
	else
		return SQL_ERROR;
}

size_t _mdb_odbc_ascii2unicode(struct _hdbc* dbc, const char *_in, size_t _in_len, SQLWCHAR *_out, size_t _out_count){
    wchar_t *w = malloc(_out_count * sizeof(wchar_t));
    size_t count = 0, i;
#if defined(_WIN32) || defined(WIN32) || defined(_WIN64) || defined(WIN64) || defined(WINDOWS)
    count = _mbstowcs_l(w, _in, _out_count, dbc->locale);
#elif defined(HAVE_MBSTOWCS_L)
    count = mbstowcs_l(w, _in, _out_count, dbc->locale);
#else
    locale_t oldlocale = uselocale(dbc->locale);
    count = mbstowcs(w, _in, _out_count);
    uselocale(oldlocale);
#endif
    for (i=0; i<count; i++) {
        _out[i] = (SQLWCHAR)w[i];
    }
    free(w);
    if (count < _out_count)
        _out[count] = '\0';
    return count;
}

SQLRETURN SQL_API SQLDriverConnect(
    SQLHDBC            hdbc,
    SQLHWND            hwnd,
    SQLCHAR        *szConnStrIn,
    SQLSMALLINT        cbConnStrIn,
    SQLCHAR        *szConnStrOut,
    SQLSMALLINT        cbConnStrOutMax,
    SQLSMALLINT    *pcbConnStrOut,
    SQLUSMALLINT       fDriverCompletion)
{
	char* database = NULL;
	ConnectParams* params;
	SQLRETURN ret;

	TRACE("SQLDriverConnect");
	strcpy(((struct _hdbc*)hdbc)->lastError, "");

	params = ((struct _hdbc*) hdbc)->params;

	if (ExtractDSN(params, (gchar*)szConnStrIn)) {
		SetConnectString (params, (gchar*)szConnStrIn);
		if (!(database = GetConnectParam (params, "Database"))){
			LogHandleError(hdbc, "Could not find Database parameter in '%s'", szConnStrIn);
			return SQL_ERROR;
		}
		ret = do_connect (hdbc, database);
		return ret;
	}
	if ((database = ExtractDBQ (params, (gchar*)szConnStrIn))) {
		ret = do_connect (hdbc, database);
		return ret;
	}
	LogHandleError(hdbc, "Could not find DSN nor DBQ in connect string '%s'", szConnStrIn);
	return SQL_ERROR;
}

SQLRETURN SQL_API SQLBrowseConnect(
    SQLHDBC            hdbc,
    SQLCHAR           *szConnStrIn,
    SQLSMALLINT        cbConnStrIn,
    SQLCHAR           *szConnStrOut,
    SQLSMALLINT        cbConnStrOutMax,
    SQLSMALLINT       *pcbConnStrOut)
{
	TRACE("SQLBrowseConnect");
	return SQL_SUCCESS;
}

SQLRETURN SQL_API SQLColumnPrivileges(
    SQLHSTMT           hstmt,
    SQLCHAR           *szCatalogName,
    SQLSMALLINT        cbCatalogName,
    SQLCHAR           *szSchemaName,
    SQLSMALLINT        cbSchemaName,
    SQLCHAR           *szTableName,
    SQLSMALLINT        cbTableName,
    SQLCHAR           *szColumnName,
    SQLSMALLINT        cbColumnName)
{
	TRACE("SQLColumnPrivileges");
	return SQL_SUCCESS;
}

SQLRETURN SQL_API SQLDescribeParam(
    SQLHSTMT           hstmt,
    SQLUSMALLINT       ipar,
    SQLSMALLINT       *pfSqlType,
    SQLULEN           *pcbParamDef,
    SQLSMALLINT       *pibScale,
    SQLSMALLINT       *pfNullable)
{
	TRACE("SQLDescribeParam");
	return SQL_SUCCESS;
}

SQLRETURN SQL_API SQLExtendedFetch(
    SQLHSTMT           hstmt,
    SQLUSMALLINT       fFetchType,
    SQLLEN             irow,
    SQLULEN           *pcrow,
    SQLUSMALLINT      *rgfRowStatus)
{
	struct _hstmt *stmt = (struct _hstmt *) hstmt;
	struct _sql_bind_info *cur = stmt->bind_head;

	TRACE("SQLExtendedFetch");
	if (fFetchType!=SQL_FETCH_NEXT) {
		LogStatementError(stmt, "Fetch type not supported in SQLExtendedFetch");
		return SQL_ERROR;
	}
	if (pcrow)
		*pcrow=1;
	if (rgfRowStatus)
		*rgfRowStatus = SQL_SUCCESS; /* what is the row status value? */
	
	if (mdb_fetch_row(stmt->sql->cur_table)) {
		SQLRETURN final_retval = SQL_SUCCESS;
		while (cur && (final_retval == SQL_SUCCESS || final_retval == SQL_SUCCESS_WITH_INFO)) {
			/* log error ? */
			SQLLEN lenbind = 0;
			SQLRETURN this_retval = SQLGetData(hstmt, cur->column_number, cur->column_bindtype,
					cur->varaddr, cur->column_bindlen, &lenbind);
			if (cur->column_lenbind)
				*(cur->column_lenbind) = lenbind;
			if (this_retval != SQL_SUCCESS)
				final_retval = this_retval;
			cur = cur->next;
		}
		stmt->rows_affected++;
		return final_retval;
	} else {
		return SQL_NO_DATA_FOUND;
	}
}

SQLRETURN SQL_API SQLForeignKeys(
    SQLHSTMT           hstmt,
    SQLCHAR           *szPkCatalogName,
    SQLSMALLINT        cbPkCatalogName,
    SQLCHAR           *szPkSchemaName,
    SQLSMALLINT        cbPkSchemaName,
    SQLCHAR           *szPkTableName,
    SQLSMALLINT        cbPkTableName,
    SQLCHAR           *szFkCatalogName,
    SQLSMALLINT        cbFkCatalogName,
    SQLCHAR           *szFkSchemaName,
    SQLSMALLINT        cbFkSchemaName,
    SQLCHAR           *szFkTableName,
    SQLSMALLINT        cbFkTableName)
{
	TRACE("SQLForeignKeys");
	return SQL_SUCCESS;
}

SQLRETURN SQL_API SQLMoreResults(
    SQLHSTMT           hstmt)
{
	TRACE("SQLMoreResults");
	return SQL_NO_DATA;
}

SQLRETURN SQL_API SQLNativeSql(
    SQLHDBC            hdbc,
    SQLCHAR           *szSqlStrIn,
    SQLINTEGER         cbSqlStrIn,
    SQLCHAR           *szSqlStr,
    SQLINTEGER         cbSqlStrMax,
    SQLINTEGER        *pcbSqlStr)
{
	TRACE("SQLNativeSql");
	return SQL_SUCCESS;
}

SQLRETURN SQL_API SQLNumParams(
    SQLHSTMT           hstmt,
    SQLSMALLINT       *pcpar)
{
	TRACE("SQLNumParams");
	return SQL_SUCCESS;
}

SQLRETURN SQL_API SQLParamOptions(
    SQLHSTMT           hstmt,
    SQLULEN            crow,
    SQLULEN           *pirow)
{
	TRACE("SQLParamOptions");
	return SQL_SUCCESS;
}

SQLRETURN SQL_API SQLPrimaryKeys(
    SQLHSTMT           hstmt,
    SQLCHAR           *szCatalogName,
    SQLSMALLINT        cbCatalogName,
    SQLCHAR           *szSchemaName,
    SQLSMALLINT        cbSchemaName,
    SQLCHAR           *szTableName,
    SQLSMALLINT        cbTableName)
{
	TRACE("SQLPrimaryKeys");
	return SQL_SUCCESS;
}

SQLRETURN SQL_API SQLProcedureColumns(
    SQLHSTMT           hstmt,
    SQLCHAR           *szCatalogName,
    SQLSMALLINT        cbCatalogName,
    SQLCHAR           *szSchemaName,
    SQLSMALLINT        cbSchemaName,
    SQLCHAR           *szProcName,
    SQLSMALLINT        cbProcName,
    SQLCHAR           *szColumnName,
    SQLSMALLINT        cbColumnName)
{
	TRACE("SQLProcedureColumns");
	return SQL_SUCCESS;
}

SQLRETURN SQL_API SQLProcedures(
    SQLHSTMT           hstmt,
    SQLCHAR           *szCatalogName,
    SQLSMALLINT        cbCatalogName,
    SQLCHAR           *szSchemaName,
    SQLSMALLINT        cbSchemaName,
    SQLCHAR           *szProcName,
    SQLSMALLINT        cbProcName)
{
	TRACE("SQLProcedures");
	return SQL_SUCCESS;
}

SQLRETURN SQL_API SQLSetPos(
    SQLHSTMT           hstmt,
    SQLSETPOSIROW      irow,
    SQLUSMALLINT       fOption,
    SQLUSMALLINT       fLock)
{
	TRACE("SQLSetPos");
	return SQL_SUCCESS;
}

SQLRETURN SQL_API SQLTablePrivileges(
    SQLHSTMT           hstmt,
    SQLCHAR           *szCatalogName,
    SQLSMALLINT        cbCatalogName,
    SQLCHAR           *szSchemaName,
    SQLSMALLINT        cbSchemaName,
    SQLCHAR           *szTableName,
    SQLSMALLINT        cbTableName)
{
	TRACE("SQLTablePrivileges");
	return SQL_SUCCESS;
}

SQLRETURN SQL_API SQLDrivers(
    SQLHENV            henv,
    SQLUSMALLINT       fDirection,
    SQLCHAR           *szDriverDesc,
    SQLSMALLINT        cbDriverDescMax,
    SQLSMALLINT       *pcbDriverDesc,
    SQLCHAR           *szDriverAttributes,
    SQLSMALLINT        cbDrvrAttrMax,
    SQLSMALLINT       *pcbDrvrAttr)
{
	TRACE("SQLDrivers");
	return SQL_SUCCESS;
}
SQLRETURN SQL_API SQLSetEnvAttr (
    SQLHENV EnvironmentHandle,
    SQLINTEGER Attribute,
    SQLPOINTER Value,
    SQLINTEGER StringLength)
{
	TRACE("SQLSetEnvAttr");
	return SQL_SUCCESS;
}

SQLRETURN SQL_API SQLBindParameter(
    SQLHSTMT           hstmt,
    SQLUSMALLINT       ipar,
    SQLSMALLINT        fParamType,
    SQLSMALLINT        fCType,
    SQLSMALLINT        fSqlType,
    SQLULEN            cbColDef,
    SQLSMALLINT        ibScale,
    SQLPOINTER         rgbValue,
    SQLLEN             cbValueMax,
    SQLLEN            *pcbValue)
{
	/*struct _hstmt *stmt;*/

	TRACE("SQLBindParameter");
	/*stmt = (struct _hstmt *) hstmt;*/
	return SQL_SUCCESS;
}

SQLRETURN SQL_API SQLAllocConnect(
    SQLHENV            henv,
    SQLHDBC           *phdbc)
{
struct _henv *env;
struct _hdbc* dbc;

	TRACE("SQLAllocConnect");
	env = (struct _henv *) henv;
	dbc = g_malloc0(sizeof(struct _hdbc));
	dbc->henv=env;
	g_ptr_array_add(env->connections, dbc);
	dbc->params = NewConnectParams ();
	dbc->statements = g_ptr_array_new();
	dbc->sqlconn = mdb_sql_init();
#if defined(_WIN32) || defined(WIN32) || defined(_WIN64) || defined(WIN64) || defined(WINDOWS)
    dbc->locale = _create_locale(LC_CTYPE, ".65001");
#else
    dbc->locale = newlocale(LC_CTYPE_MASK, "C.UTF-8", NULL);
#endif
	*phdbc=dbc;

	return SQL_SUCCESS;
}

SQLRETURN SQL_API SQLAllocEnv(
    SQLHENV           *phenv)
{
struct _henv *env;

	TRACE("SQLAllocEnv");
	env = g_malloc0(sizeof(struct _henv));
	env->connections = g_ptr_array_new();
	*phenv=env;
	return SQL_SUCCESS;
}

SQLRETURN SQL_API SQLAllocStmt(
    SQLHDBC            hdbc,
    SQLHSTMT          *phstmt)
{
	struct _hdbc *dbc;
	struct _hstmt *stmt;

	TRACE("SQLAllocStmt");
	dbc = (struct _hdbc *) hdbc;

	stmt = g_malloc0(sizeof(struct _hstmt));
	stmt->hdbc=dbc;
	g_ptr_array_add(dbc->statements, stmt);
	stmt->sql = mdb_sql_init();
	stmt->sql->mdb = mdb_clone_handle(dbc->sqlconn->mdb);

	*phstmt = stmt;
	return SQL_SUCCESS;
}

SQLRETURN SQL_API SQLAllocHandle(
    SQLSMALLINT HandleType,
    SQLHANDLE InputHandle,
    SQLHANDLE * OutputHandle)
{
    SQLRETURN retval = SQL_ERROR;
	TRACE("SQLAllocHandle");
	switch(HandleType) {
		case SQL_HANDLE_STMT:
			retval = SQLAllocStmt(InputHandle,OutputHandle);
			break;
		case SQL_HANDLE_DBC:
			retval = SQLAllocConnect(InputHandle,OutputHandle);
			break;
		case SQL_HANDLE_ENV:
			retval = SQLAllocEnv(OutputHandle);
			break;
	}
	return retval;
}

SQLRETURN SQL_API SQLBindCol(
    SQLHSTMT           hstmt,
    SQLUSMALLINT       icol,
    SQLSMALLINT        fCType,
    SQLPOINTER         rgbValue,
    SQLLEN             cbValueMax,
    SQLLEN            *pcbValue)
{
	struct _hstmt *stmt = (struct _hstmt *) hstmt;
	struct _sql_bind_info *cur, *newitem;

	TRACE("SQLBindCol");
	/* find available item in list */
	cur = stmt->bind_head;
	while (cur) {
		if (cur->column_number==icol) 
			break;
		cur = cur->next;
	}
	/* if this is a repeat */
	if (cur) {
		cur->column_bindtype = fCType;
   		cur->column_lenbind = pcbValue;
   		cur->column_bindlen = cbValueMax;
   		cur->varaddr = (char *) rgbValue;
	} else {
		/* didn't find it create a new one */
		newitem = g_malloc0(sizeof(struct _sql_bind_info));
		newitem->column_number = icol;
		newitem->column_bindtype = fCType;
   		newitem->column_bindlen = cbValueMax;
   		newitem->column_lenbind = pcbValue;
   		newitem->varaddr = (char *) rgbValue;
		/* if there's no head yet */
		if (! stmt->bind_head) {
			stmt->bind_head = newitem;
		} else {
			/* find the tail of the list */
			cur = stmt->bind_head;
			while (cur->next) {
				cur = cur->next;
			}
			cur->next = newitem;
		}
	}

	return SQL_SUCCESS;
}

SQLRETURN SQL_API SQLCancel(
    SQLHSTMT           hstmt)
{
	TRACE("SQLCancel");
	return SQL_SUCCESS;
}

SQLRETURN SQL_API SQLConnect(
    SQLHDBC            hdbc,
    SQLCHAR           *szDSN,
    SQLSMALLINT        cbDSN,
    SQLCHAR           *szUID,
    SQLSMALLINT        cbUID,
    SQLCHAR           *szAuthStr,
    SQLSMALLINT        cbAuthStr)
{
	char* database = NULL;
	ConnectParams* params;
	SQLRETURN ret;

	TRACE("SQLConnect");
	strcpy(((struct _hdbc*)hdbc)->lastError, "");

	params = ((struct _hdbc*) hdbc)->params;

	params->dsnName = g_string_assign (params->dsnName, (gchar*)szDSN);

	if (!(database = GetConnectParam (params, "Database")))
	{
		LogHandleError(hdbc, "Could not find Database parameter in '%s'", szDSN);
		return SQL_ERROR;
	}

	ret = do_connect (hdbc, database);
	return ret;
}

SQLRETURN SQL_API SQLDescribeCol(
    SQLHSTMT           hstmt,
    SQLUSMALLINT       icol,
    SQLCHAR           *szColName,
    SQLSMALLINT        cbColNameMax,
    SQLSMALLINT       *pcbColName,
    SQLSMALLINT       *pfSqlType,
    SQLULEN           *pcbColDef, /* precision */
    SQLSMALLINT       *pibScale,
    SQLSMALLINT       *pfNullable)
{
	int i;
	struct _hstmt *stmt = (struct _hstmt *) hstmt;
	MdbSQL *sql = stmt->sql;
	MdbSQLColumn *sqlcol;
	MdbColumn *col;
	MdbTableDef *table;
	SQLRETURN ret;

	TRACE("SQLDescribeCol");
	if (icol<1 || icol>sql->num_columns) {
		strcpy(stmt->sqlState, "07009"); // Invalid descriptor index
		return SQL_ERROR;
	}
	sqlcol = g_ptr_array_index(sql->columns,icol - 1);
	table = sql->cur_table;
	for (i=0;i<table->num_cols;i++) {
		col=g_ptr_array_index(table->columns,i);
		if (!g_ascii_strcasecmp(sqlcol->name, col->name)) {
			break;
		}
	}
	if (i==table->num_cols) {
		fprintf(stderr, "Column %s lost\n", (char*)sqlcol->name);
		strcpy(stmt->sqlState, "07009"); // Invalid descriptor index
		return SQL_ERROR;
	}

	ret = SQL_SUCCESS;
	if (pcbColName)
		*pcbColName=strlen(sqlcol->name);
	if (szColName) {
		if (cbColNameMax < 0) {
			strcpy(stmt->sqlState, "HY090"); // Invalid string or buffer length
			return SQL_ERROR;
		}
		if (snprintf((char *)szColName, cbColNameMax, "%s", sqlcol->name) + 1 > cbColNameMax) {
			strcpy(stmt->sqlState, "01004"); // String data, right truncated
			ret = SQL_SUCCESS_WITH_INFO;
		}
	}
	if (pfSqlType) {
		*pfSqlType = _odbc_get_client_type(col);
	}
	if (pcbColDef) {
		*pcbColDef = col->col_size;
	}
	if (pibScale) {
		/* FIX ME */
		*pibScale = 0;
	}
	if (pfNullable) {
		*pfNullable = !col->is_fixed;
	}

	return ret;
}

SQLRETURN SQL_API SQLColAttributes(
    SQLHSTMT           hstmt,
    SQLUSMALLINT       icol,
    SQLUSMALLINT       fDescType,
    SQLPOINTER         rgbDesc,
    SQLSMALLINT        cbDescMax,
    SQLSMALLINT       *pcbDesc,
    SQLLEN            *pfDesc)
{
	int i;
	struct _hstmt *stmt;
	MdbSQL *sql;
	MdbSQLColumn *sqlcol;
	MdbColumn *col;
	MdbTableDef *table;
	SQLRETURN ret;

	TRACE("SQLColAttributes");
	stmt = (struct _hstmt *) hstmt;
	sql = stmt->sql;

	/* dont check column index for these */
	switch(fDescType) {
		case SQL_COLUMN_COUNT:
		case SQL_DESC_COUNT:
			*pfDesc = stmt->sql->num_columns;
			return SQL_SUCCESS;
			break;
	}

	if (icol<1 || icol>sql->num_columns) {
		strcpy(stmt->sqlState, "07009"); // Invalid descriptor index
		return SQL_ERROR;
	}

	/* find the column */
	sqlcol = g_ptr_array_index(sql->columns,icol - 1);
	table = sql->cur_table;
	for (i=0;i<table->num_cols;i++) {
		col=g_ptr_array_index(table->columns,i);
		if (!g_ascii_strcasecmp(sqlcol->name, col->name)) {
			break;
          	}
	}
	if (i==table->num_cols) {
		strcpy(stmt->sqlState, "07009"); // Invalid descriptor index
		return SQL_ERROR;
	}

	// fprintf(stderr,"fDescType = %d\n", fDescType);
	ret = SQL_SUCCESS;
	switch(fDescType) {
		case SQL_COLUMN_NAME: case SQL_DESC_NAME:
		case SQL_COLUMN_LABEL: /* = SQL_DESC_LABEL */
			if (cbDescMax < 0) {
				strcpy(stmt->sqlState, "HY090"); // Invalid string or buffer length
				return SQL_ERROR;
			}
			if (snprintf(rgbDesc, cbDescMax, "%s", sqlcol->name) + 1 > cbDescMax) {
				strcpy(stmt->sqlState, "01004"); // String data, right truncated
				ret = SQL_SUCCESS_WITH_INFO;
			}
			break;
		case SQL_COLUMN_TYPE: /* =SQL_DESC_CONCISE_TYPE */
			//*pfDesc = SQL_CHAR;
			*pfDesc = _odbc_get_client_type(col);
			break;
		case SQL_COLUMN_TYPE_NAME:
		{
			const char *type_name = _odbc_get_client_type_name(col);
			if (type_name)
				*pcbDesc = snprintf(rgbDesc, cbDescMax, "%s", type_name);
			break;
		}
		case SQL_COLUMN_LENGTH:
			break;
		case SQL_COLUMN_DISPLAY_SIZE: /* =SQL_DESC_DISPLAY_SIZE */
			*pfDesc = mdb_col_disp_size(col);
			break;
		case SQL_DESC_UNSIGNED:
			switch(col->col_type) {
				case MDB_INT:
				case MDB_LONGINT:
				case MDB_FLOAT:
				case MDB_DOUBLE:
				case MDB_NUMERIC:
					*pfDesc = SQL_FALSE;
					break;
				case MDB_BYTE:
				default: // Everything else returns true per MSDN
					*pfDesc = SQL_TRUE;
					break;
			}
			break;
        case SQL_DESC_UPDATABLE:
            *pfDesc = SQL_ATTR_READONLY;
            break;
		default:
			strcpy(stmt->sqlState, "HYC00"); // 	Driver not capable
			ret = SQL_ERROR;
			break;
	}
	return ret;
}

SQLRETURN SQL_API SQLDisconnect(
    SQLHDBC            hdbc)
{
	struct _hdbc *dbc;

	TRACE("SQLDisconnect");

	dbc = (struct _hdbc *) hdbc;
	// Automatically close all statements:
	while (dbc->statements->len)
		SQLFreeStmt(g_ptr_array_index(dbc->statements, 0), SQL_DROP);
	mdb_sql_close(dbc->sqlconn);

	return SQL_SUCCESS;
}

SQLRETURN SQL_API SQLError(
    SQLHENV            henv,
    SQLHDBC            hdbc,
    SQLHSTMT           hstmt,
    SQLCHAR           *szSqlState,
    SQLINTEGER        *pfNativeError,
    SQLCHAR           *szErrorMsg,
    SQLSMALLINT        cbErrorMsgMax,
    SQLSMALLINT       *pcbErrorMsg)
{
	SQLRETURN result = SQL_NO_DATA_FOUND;
   
	TRACE("SQLError");
	//if(pfNativeError)fprintf(stderr,"NativeError %05d\n", *pfNativeError);
    char *src = NULL;
    char *state = NULL;
    if (hstmt) {
        src = ((struct _hstmt *)hstmt)->lastError;
        state = ((struct _hstmt *)hstmt)->sqlState;
    } else if (hdbc) {
        src = ((struct _hdbc *)hdbc)->lastError;
        state = ((struct _hdbc *)hdbc)->sqlState;
    } else if (henv) {
        state = ((struct _henv *)henv)->sqlState;
    }

    if (state)
        strcpy((char*)szSqlState, state);

    if (src && src[0]) {
        int l = snprintf((char*)szErrorMsg, cbErrorMsgMax, "%s", src);
        if (pcbErrorMsg)
            *pcbErrorMsg = l;
        if (pfNativeError)
            *pfNativeError = 1;

        result = SQL_SUCCESS;
        strcpy(src, "");
    }

	return result;
}

SQLRETURN SQL_API SQLExecute(SQLHSTMT hstmt)
{
	struct _hstmt *stmt = (struct _hstmt *) hstmt;

	TRACE("SQLExecute");
   
	/* fprintf(stderr,"query = %s\n",stmt->query); */
	_odbc_fix_literals(stmt);

	mdb_sql_reset(stmt->sql);

	mdb_sql_run_query(stmt->sql, stmt->query);
	if (mdb_sql_has_error(stmt->sql)) {
		LogStatementError(stmt, "Couldn't parse SQL\n");
		mdb_sql_reset(stmt->sql);
		return SQL_ERROR;
	} else {
		return SQL_SUCCESS;
	}
}

SQLRETURN SQL_API SQLExecDirect(
    SQLHSTMT           hstmt,
    SQLCHAR           *szSqlStr,
    SQLINTEGER         cbSqlStr)
{
	SQLPrepare(hstmt, szSqlStr, cbSqlStr);
	return SQLExecute(hstmt);
}

static void
unbind_columns(struct _hstmt *stmt)
{
	struct _sql_bind_info *cur, *next;

	TRACE("unbind_columns");

	//Free the memory allocated for bound columns
	cur = stmt->bind_head;
	while(cur) {
		next = cur->next;
		g_free(cur);
		cur = next;
	}
	stmt->bind_head = NULL;
}

SQLRETURN SQLFetch(
    SQLHSTMT           hstmt)
{
	struct _hstmt *stmt = (struct _hstmt *) hstmt;
	struct _sql_bind_info *cur = stmt->bind_head;
	TRACE("SQLFetch");

	if ( stmt->sql->limit >= 0 && stmt->rows_affected == stmt->sql->limit ) {
		return SQL_NO_DATA_FOUND;
	}
	if (mdb_fetch_row(stmt->sql->cur_table)) {
		SQLRETURN final_retval = SQL_SUCCESS;
		while (cur && (final_retval == SQL_SUCCESS || final_retval == SQL_SUCCESS_WITH_INFO)) {
			/* log error ? */
			SQLLEN lenbind = 0;
			SQLRETURN this_retval = SQLGetData(hstmt, cur->column_number, cur->column_bindtype,
					cur->varaddr, cur->column_bindlen, &lenbind);
			if (cur->column_lenbind)
				*(cur->column_lenbind) = lenbind;
			if (this_retval != SQL_SUCCESS)
				final_retval = this_retval;
			cur = cur->next;
		}
		stmt->rows_affected++;
		stmt->pos=0;
		return final_retval;
	} else {
		return SQL_NO_DATA_FOUND;
	}
}

SQLRETURN SQL_API SQLFreeConnect(
    SQLHDBC            hdbc)
{
	struct _hdbc* dbc = (struct _hdbc*) hdbc;
	struct _henv* env;

	TRACE("SQLFreeConnect");

	env = dbc->henv;

	if (dbc->statements->len) {
		// Function sequence error
		strcpy(dbc->sqlState, "HY010");
		return SQL_ERROR;
	}
	if (!g_ptr_array_remove(env->connections, dbc))
		return SQL_INVALID_HANDLE;

	FreeConnectParams(dbc->params);
	g_ptr_array_free(dbc->statements, TRUE);
	mdb_sql_exit(dbc->sqlconn);
#if defined(_WIN32) || defined(WIN32) || defined(_WIN64) || defined(WIN64) || defined(WINDOWS)
    if (dbc->locale) _free_locale(dbc->locale);
#else
    if (dbc->locale) freelocale(dbc->locale);
#endif
	g_free(dbc);

	return SQL_SUCCESS;
}

SQLRETURN SQL_API SQLFreeEnv(
    SQLHENV            henv)
{
	struct _henv* env = (struct _henv*)henv;

	TRACE("SQLFreeEnv");

	if (env->connections->len) {
		// Function sequence error
		strcpy(env->sqlState, "HY010");
		return SQL_ERROR;
	}
	g_ptr_array_free(env->connections, TRUE);
	g_free(env);

	return SQL_SUCCESS;
}

SQLRETURN SQL_API SQLFreeStmt(
    SQLHSTMT           hstmt,
    SQLUSMALLINT       fOption)
{
	struct _hstmt *stmt=(struct _hstmt *)hstmt;
	struct _hdbc *dbc = (struct _hdbc *) stmt->hdbc;

	TRACE("SQLFreeStmt");
	free(stmt->ole_str);
	stmt->ole_str = NULL;
	if (fOption==SQL_DROP) {
		if (!g_ptr_array_remove(dbc->statements, stmt))
			return SQL_INVALID_HANDLE;
		mdb_sql_exit(stmt->sql);
		unbind_columns(stmt);
		g_free(stmt);
	} else if (fOption==SQL_CLOSE) {
		stmt->rows_affected = 0;
	} else if (fOption==SQL_UNBIND) {
		unbind_columns(stmt);
	} else if (fOption==SQL_RESET_PARAMS) {
		/* Bound parameters not currently implemented */
	} else {
	}
	return SQL_SUCCESS;
}

SQLRETURN SQL_API SQLFreeHandle(
    SQLSMALLINT HandleType,
    SQLHANDLE Handle)
{
    SQLRETURN retval = SQL_ERROR;
	TRACE("SQLFreeHandle");
	switch(HandleType) {
		case SQL_HANDLE_STMT:
			retval = SQLFreeStmt(Handle,SQL_DROP);
			break;
		case SQL_HANDLE_DBC:
			retval = SQLFreeConnect(Handle);
			break;
		case SQL_HANDLE_ENV:
			retval = SQLFreeEnv(Handle);
			break;
	}
   return retval;
}

SQLRETURN SQL_API SQLGetStmtAttr (
    SQLHSTMT StatementHandle,
    SQLINTEGER Attribute,
    SQLPOINTER Value,
    SQLINTEGER BufferLength,
    SQLINTEGER * StringLength)
{
	TRACE("SQLGetStmtAttr");
	return SQL_SUCCESS;
}

SQLRETURN SQL_API SQLGetCursorName(
    SQLHSTMT           hstmt,
    SQLCHAR           *szCursor,
    SQLSMALLINT        cbCursorMax,
    SQLSMALLINT       *pcbCursor)
{
	TRACE("SQLGetCursorName");
	return SQL_SUCCESS;
}

SQLRETURN SQL_API SQLNumResultCols(
    SQLHSTMT           hstmt,
    SQLSMALLINT       *pccol)
{
	struct _hstmt *stmt = (struct _hstmt *) hstmt;
	
	TRACE("SQLNumResultCols");
	*pccol = stmt->sql->num_columns;
	return SQL_SUCCESS;
}

SQLRETURN SQL_API SQLPrepare(
    SQLHSTMT           hstmt,
    SQLCHAR           *szSqlStr,
    SQLINTEGER         cbSqlStr)
{
	struct _hstmt *stmt=(struct _hstmt *)hstmt;
	int sqllen = _odbc_get_string_size(cbSqlStr, szSqlStr);

	TRACE("SQLPrepare");

	snprintf(stmt->query, sizeof(stmt->query), "%.*s", sqllen, (char*)szSqlStr);

	return SQL_SUCCESS;
}

SQLRETURN SQL_API SQLRowCount(
    SQLHSTMT           hstmt,
    SQLLEN            *pcrow)
{
struct _hstmt *stmt=(struct _hstmt *)hstmt;

	TRACE("SQLRowCount");
	*pcrow = stmt->rows_affected;
	return SQL_SUCCESS;
}

SQLRETURN SQL_API SQLSetCursorName(
    SQLHSTMT           hstmt,
    SQLCHAR           *szCursor,
    SQLSMALLINT        cbCursor)
{
	TRACE("SQLSetCursorName");
	return SQL_SUCCESS;
}

SQLRETURN SQL_API SQLTransact(
    SQLHENV            henv,
    SQLHDBC            hdbc,
    SQLUSMALLINT       fType)
{
	TRACE("SQLTransact");
	return SQL_SUCCESS;
}


SQLRETURN SQL_API SQLSetParam(            /*      Use SQLBindParameter */
    SQLHSTMT           hstmt,
    SQLUSMALLINT       ipar,
    SQLSMALLINT        fCType,
    SQLSMALLINT        fSqlType,
    SQLULEN            cbParamDef,
    SQLSMALLINT        ibScale,
    SQLPOINTER         rgbValue,
    SQLLEN            *pcbValue)
{
	TRACE("SQLSetParam");
	return SQL_SUCCESS;
}

SQLRETURN SQL_API SQLColumns(
    SQLHSTMT           hstmt,
    SQLCHAR           *szCatalogName,
    SQLSMALLINT        cbCatalogName,
    SQLCHAR           *szSchemaName,
    SQLSMALLINT        cbSchemaName,
    SQLCHAR           *szTableName,
    SQLSMALLINT        cbTableName,
    SQLCHAR           *szColumnName,
    SQLSMALLINT        cbColumnName)
{
	struct _hstmt *stmt = (struct _hstmt *) hstmt;
	MdbSQL *sql = stmt->sql;
	MdbHandle *mdb = sql->mdb;
	MdbTableDef *ttable;
	MdbField fields[18];
	unsigned char row_buffer[MDB_PGSIZE];
	int row_size;
	unsigned int i, j, k;
	MdbCatalogEntry *entry;
	MdbTableDef *table;
	MdbColumn *col;
	unsigned int ts2, ts3, ts5;
	unsigned char t2[MDB_BIND_SIZE],
	              t3[MDB_BIND_SIZE],
	              t5[MDB_BIND_SIZE];
	SQLSMALLINT nullable;  /* SQL_NULLABLE or SQL_NO_NULLS */
	SQLSMALLINT datatype;  /* For datetime, use concise data type */
	SQLSMALLINT sqldatatype;  /* For datetime, use nonconcise data type */
	SQLINTEGER ordinal;

	TRACE("SQLColumns");

	mdb_read_catalog(mdb, MDB_ANY);

	ttable = mdb_create_temp_table(mdb, "#columns");
	mdb_sql_add_temp_col(sql, ttable, 0, "TABLE_CAT", MDB_TEXT, 128, 0);
	mdb_sql_add_temp_col(sql, ttable, 1, "TABLE_SCHEM", MDB_TEXT, 128, 0);
	mdb_sql_add_temp_col(sql, ttable, 2, "TABLE_NAME", MDB_TEXT, 128, 0);
	mdb_sql_add_temp_col(sql, ttable, 3, "COLUMN_NAME", MDB_TEXT, 128, 0);
	mdb_sql_add_temp_col(sql, ttable, 4, "DATA_TYPE", MDB_INT, 0, 1);
	mdb_sql_add_temp_col(sql, ttable, 5, "TYPE_NAME", MDB_TEXT, 128, 0);
	mdb_sql_add_temp_col(sql, ttable, 6, "COLUMN_SIZE", MDB_LONGINT, 0, 1);
	mdb_sql_add_temp_col(sql, ttable, 7, "BUFFER_LENGTH", MDB_LONGINT, 0, 1);
	mdb_sql_add_temp_col(sql, ttable, 8, "DECIMAL_DIGITS", MDB_INT, 0, 1);
	mdb_sql_add_temp_col(sql, ttable, 9, "NUM_PREC_RADIX", MDB_INT, 0, 1);
	mdb_sql_add_temp_col(sql, ttable, 10, "NULLABLE", MDB_INT, 0, 1);
	mdb_sql_add_temp_col(sql, ttable, 11, "REMARKS", MDB_TEXT, 254, 0);
	mdb_sql_add_temp_col(sql, ttable, 12, "COLUMN_DEF", MDB_TEXT, 254, 0);
	mdb_sql_add_temp_col(sql, ttable, 13, "SQL_DATA_TYPE", MDB_INT, 0, 1);
	mdb_sql_add_temp_col(sql, ttable, 14, "SQL_DATETIME_SUB", MDB_INT, 0, 1);
	mdb_sql_add_temp_col(sql, ttable, 15, "CHAR_OCTET_LENGTH", MDB_LONGINT, 0, 1);
	mdb_sql_add_temp_col(sql, ttable, 16, "ORDINAL_POSITION", MDB_LONGINT, 0, 1);
	mdb_sql_add_temp_col(sql, ttable, 17, "IS_NULLABLE", MDB_TEXT, 254, 0);
	mdb_temp_columns_end(ttable);

	for (i=0; i<mdb->num_catalog; i++) {
     		entry = g_ptr_array_index(mdb->catalog, i);
		/* TODO: Do more advanced matching */
		if (entry->object_type != MDB_TABLE || g_ascii_strcasecmp((char*)szTableName, entry->object_name) != 0)
			continue;
		table = mdb_read_table(entry);
		if ( !table )
		{
			LogStatementError(stmt, "Could not read table '%s'", szTableName);
			return SQL_ERROR;
		}
		if (!mdb_read_columns(table)) {
			LogStatementError(stmt, "Could not read columns of table '%s'", szTableName);
			return SQL_ERROR;
		}
		for (j=0; j<table->num_cols; j++) {
			col = g_ptr_array_index(table->columns, j);

			ts2 = mdb_ascii2unicode(mdb, table->name, 0, (char*)t2, sizeof(t2));
			ts3 = mdb_ascii2unicode(mdb, col->name, 0, (char*)t3, sizeof(t3));
			ts5 = mdb_ascii2unicode(mdb, _odbc_get_client_type_name(col), 0,  (char*)t5, sizeof(t5));

			nullable = SQL_NO_NULLS;
			datatype = _odbc_get_client_type(col);
			sqldatatype = _odbc_get_client_type(col);
			ordinal = j+1;

			/* Set all fields to NULL */
			for (k=0; k<18; k++) {
				FILL_FIELD(&fields[k], NULL, 0);
			}

			FILL_FIELD(&fields[2], t2, ts2);
			FILL_FIELD(&fields[3], t3, ts3);
			FILL_FIELD(&fields[4], &datatype, 0);
			FILL_FIELD(&fields[5], t5, ts5);
			FILL_FIELD(&fields[10], &nullable, 0);
			FILL_FIELD(&fields[13], &sqldatatype, 0);
			FILL_FIELD(&fields[16], &ordinal, 0);

			row_size = mdb_pack_row(ttable, row_buffer, 18, fields);
			mdb_add_row_to_pg(ttable, row_buffer, row_size);
			ttable->num_rows++;
		}
		mdb_free_tabledef(table);
	}
	sql->cur_table = ttable;

	return SQL_SUCCESS;
}

SQLRETURN SQL_API SQLGetConnectOption(
    SQLHDBC            hdbc,
    SQLUSMALLINT       fOption,
    SQLPOINTER         pvParam)
{
	TRACE("SQLGetConnectOption");
	return SQL_SUCCESS;
}

SQLRETURN SQL_API SQLGetData(
    SQLHSTMT           hstmt,
    SQLUSMALLINT       icol,
    SQLSMALLINT        fCType,
    SQLPOINTER         rgbValue,
    SQLLEN             cbValueMax,
    SQLLEN             *pcbValue)
{
	struct _hstmt *stmt;
	MdbSQL *sql;
	MdbHandle *mdb;
	MdbSQLColumn *sqlcol;
	MdbColumn *col;
	MdbTableDef *table;
	int i, intValue;

	TRACE("SQLGetData");
	stmt = (struct _hstmt *) hstmt;
	sql = stmt->sql;
	mdb = sql->mdb;

	if (icol<1 || icol>sql->num_columns) {
		strcpy(stmt->sqlState, "07009");
		return SQL_ERROR;
	}

	sqlcol = g_ptr_array_index(sql->columns,icol - 1);
	table = sql->cur_table;
	for (i=0;i<table->num_cols;i++) {
		col=g_ptr_array_index(table->columns,i);
		if (!g_ascii_strcasecmp(sqlcol->name, col->name)) {
			break;
		}
	}
	if (i==table->num_cols)
		return SQL_ERROR;

	if (icol!=stmt->icol) {
		stmt->icol=icol;
		stmt->pos=0;
	}

	if (!rgbValue) {
		strcpy(stmt->sqlState, "HY009");
	 	return SQL_ERROR;
	}

	if (col->col_type == MDB_BOOL) {
		// bool cannot be null
		if (fCType == SQL_C_CHAR) {
			((SQLCHAR *)rgbValue)[0] = col->cur_value_len ? '0' : '1';
			((SQLCHAR *)rgbValue)[1] = '\0';
			if (pcbValue)
				*pcbValue = sizeof(SQLCHAR);
		} else if (fCType == SQL_C_WCHAR) {
			((SQLWCHAR *)rgbValue)[0] = col->cur_value_len ? '0' : '1';
			((SQLWCHAR *)rgbValue)[1] = '\0';
			if (pcbValue)
				*pcbValue = sizeof(SQLWCHAR);
		} else {
			*(BOOL*)rgbValue = col->cur_value_len ? 0 : 1;
			if (pcbValue)
				*pcbValue = 1;
	        }
		return SQL_SUCCESS;
	}
	if (col->cur_value_len == 0) {
		/* When NULL data is retrieved, non-null pcbValue is
		   required */
		if (!pcbValue) {
			strcpy(stmt->sqlState, "22002");
			return SQL_ERROR;
		}
		*pcbValue = SQL_NULL_DATA;
		return SQL_SUCCESS;
	}

	if (fCType==SQL_ARD_TYPE) {
		// Get _sql_bind_info
		struct _sql_bind_info *cur;
		for (cur = stmt->bind_head; cur; cur=cur->next) {
			if (cur->column_number == icol) {
				fCType = cur->column_bindtype;
				goto found_bound_type;
			}
		}
		strcpy(stmt->sqlState, "07009");
		return SQL_ERROR;
	}
	found_bound_type:
	if (fCType==SQL_C_DEFAULT)
		fCType = _odbc_get_client_type(col);
	if (fCType == SQL_C_CHAR || fCType == SQL_C_WCHAR)
		goto to_c_char;
	switch(col->col_type) {
		case MDB_BYTE:
			intValue = (int)mdb_get_byte(mdb->pg_buf, col->cur_value_start);
			goto to_integer_type;
		case MDB_INT:
			intValue = mdb_get_int16(mdb->pg_buf, col->cur_value_start);
			goto to_integer_type;
		case MDB_LONGINT:
			intValue = mdb_get_int32(mdb->pg_buf, col->cur_value_start);
			goto to_integer_type;
		to_integer_type:
			switch (fCType) {
			case SQL_C_UTINYINT:
				if (intValue<0 || intValue>UCHAR_MAX) {
					strcpy(stmt->sqlState, "22003"); // Numeric value out of range
					return SQL_ERROR;
				}
				*(SQLCHAR*)rgbValue = (SQLCHAR)intValue;
				if (pcbValue)
					*pcbValue = sizeof(SQLCHAR);
				break;
			case SQL_C_TINYINT:
			case SQL_C_STINYINT:
				if (intValue<SCHAR_MIN || intValue>SCHAR_MAX) {
					strcpy(stmt->sqlState, "22003"); // Numeric value out of range
					return SQL_ERROR;
				}
				*(SQLSCHAR*)rgbValue = (SQLSCHAR)intValue;
				if (pcbValue)
					*pcbValue = sizeof(SQLSCHAR);
				break;
			case SQL_C_USHORT:
			case SQL_C_SHORT:
				if (intValue<0 || intValue>USHRT_MAX) {
					strcpy(stmt->sqlState, "22003"); // Numeric value out of range
					return SQL_ERROR;
				}
				*(SQLSMALLINT*)rgbValue = (SQLSMALLINT)intValue;
				if (pcbValue)
					*pcbValue = sizeof(SQLSMALLINT);
				break;
			case SQL_C_SSHORT:
				if (intValue<SHRT_MIN || intValue>SHRT_MAX) {
					strcpy(stmt->sqlState, "22003"); // Numeric value out of range
					return SQL_ERROR;
				}
				*(SQLSMALLINT*)rgbValue = (SQLSMALLINT)intValue;
				if (pcbValue)
					*pcbValue = sizeof(SQLSMALLINT);
				break;
			case SQL_C_ULONG:
				if (intValue<0 || intValue>UINT_MAX) {
					strcpy(stmt->sqlState, "22003"); // Numeric value out of range
					return SQL_ERROR;
				}
				*(SQLUINTEGER*)rgbValue = (SQLINTEGER)intValue;
				if (pcbValue)
					*pcbValue = sizeof(SQLINTEGER);
				break;
			case SQL_C_LONG:
			case SQL_C_SLONG:
				if (intValue<INT_MIN || intValue>INT_MAX) {
					strcpy(stmt->sqlState, "22003"); // Numeric value out of range
					return SQL_ERROR;
				}
				*(SQLINTEGER*)rgbValue = intValue;
				if (pcbValue)
					*pcbValue = sizeof(SQLINTEGER);
				break;
			default:
				strcpy(stmt->sqlState, "HYC00"); // Not implemented
				return SQL_ERROR;
			}
			break;
		// case MDB_MONEY: TODO
		case MDB_FLOAT:
			switch (fCType) {
			case SQL_C_FLOAT:
				*(float*)rgbValue = mdb_get_single(mdb->pg_buf, col->cur_value_start);
				if (pcbValue)
					*pcbValue = sizeof(float);
				break;
			case SQL_C_DOUBLE:
				*(double*)rgbValue = (double)mdb_get_single(mdb->pg_buf, col->cur_value_start);
				if (pcbValue)
					*pcbValue = sizeof(double);
				break;
			case SQL_C_CHAR:
			case SQL_C_WCHAR:
			case SQL_C_STINYINT:
			case SQL_C_UTINYINT:
			case SQL_C_TINYINT:
			case SQL_C_SBIGINT:
			case SQL_C_UBIGINT:
			case SQL_C_SSHORT:
			case SQL_C_USHORT:
			case SQL_C_SHORT:
			case SQL_C_SLONG:
			case SQL_C_ULONG:
			case SQL_C_LONG:
			case SQL_C_NUMERIC:
			// case SQL_C_FLOAT:
			// case SQL_C_DOUBLE:
			case SQL_C_BIT:
			case SQL_C_BINARY:
			case SQL_C_INTERVAL_YEAR_TO_MONTH:
			case SQL_C_INTERVAL_DAY_TO_HOUR:
			case SQL_C_INTERVAL_DAY_TO_MINUTE:
			case SQL_C_INTERVAL_DAY_TO_SECOND:
			case SQL_C_INTERVAL_HOUR_TO_MINUTE:
			case SQL_C_INTERVAL_HOUR_TO_SECOND:
				strcpy(stmt->sqlState, "HYC00"); // Not implemented
				return SQL_ERROR;
			default:
				strcpy(stmt->sqlState, "07006"); // Not allowed
				return SQL_ERROR;
			}
			break;
		case MDB_DOUBLE:
			switch (fCType) {
			case SQL_C_DOUBLE:
				*(double*)rgbValue = mdb_get_double(mdb->pg_buf, col->cur_value_start);
				if (pcbValue)
					*pcbValue = sizeof(double);
				break;
			case SQL_C_CHAR:
			case SQL_C_WCHAR:
			case SQL_C_STINYINT:
			case SQL_C_UTINYINT:
			case SQL_C_TINYINT:
			case SQL_C_SBIGINT:
			case SQL_C_UBIGINT:
			case SQL_C_SSHORT:
			case SQL_C_USHORT:
			case SQL_C_SHORT:
			case SQL_C_SLONG:
			case SQL_C_ULONG:
			case SQL_C_LONG:
			case SQL_C_NUMERIC:
			case SQL_C_FLOAT:
			// case SQL_C_DOUBLE:
			case SQL_C_BIT:
			case SQL_C_BINARY:
			case SQL_C_INTERVAL_YEAR_TO_MONTH:
			case SQL_C_INTERVAL_DAY_TO_HOUR:
			case SQL_C_INTERVAL_DAY_TO_MINUTE:
			case SQL_C_INTERVAL_DAY_TO_SECOND:
			case SQL_C_INTERVAL_HOUR_TO_MINUTE:
			case SQL_C_INTERVAL_HOUR_TO_SECOND:
				strcpy(stmt->sqlState, "HYC00"); // Not implemented
				return SQL_ERROR;
			default:
				strcpy(stmt->sqlState, "07006"); // Not allowed
				return SQL_ERROR;
			}
			break;
#if ODBCVER >= 0x0300
		// returns text if old odbc
		case MDB_DATETIME:
		{
			struct tm tmp_t = { 0 };
			mdb_date_to_tm(mdb_get_double(mdb->pg_buf, col->cur_value_start), &tmp_t);

			if (mdb_col_is_shortdate(col)) {
				DATE_STRUCT sql_dt;
				sql_dt.year     = tmp_t.tm_year + 1900;
				sql_dt.month    = tmp_t.tm_mon + 1;
				sql_dt.day      = tmp_t.tm_mday;
				*(DATE_STRUCT*)rgbValue = sql_dt;
				if (pcbValue)
					*pcbValue = sizeof(DATE_STRUCT);
			} else {
				TIMESTAMP_STRUCT sql_ts;
				sql_ts.year     = tmp_t.tm_year + 1900;
				sql_ts.month    = tmp_t.tm_mon + 1;
				sql_ts.day      = tmp_t.tm_mday;
				sql_ts.hour     = tmp_t.tm_hour;
				sql_ts.minute   = tmp_t.tm_min;
				sql_ts.second   = tmp_t.tm_sec;
				sql_ts.fraction = 0;

				*(TIMESTAMP_STRUCT*)rgbValue = sql_ts;
				if (pcbValue)
					*pcbValue = sizeof(TIMESTAMP_STRUCT);
			}
			break;
		}
#endif
        case MDB_OLE:
			if (cbValueMax < 0) {
				strcpy(stmt->sqlState, "HY090"); // Invalid string or buffer length
				return SQL_ERROR;
			}
			if (stmt->pos == 0) {
				if (stmt->ole_str) {
					free(stmt->ole_str);
				}
				stmt->ole_str = mdb_ole_read_full(mdb, col, &stmt->ole_len);
			}
			if (stmt->pos >= stmt->ole_len) {
				return SQL_NO_DATA;
			}
			if (pcbValue) {
				*pcbValue = stmt->ole_len - stmt->pos;
			}
			if (cbValueMax == 0) {
				return SQL_SUCCESS_WITH_INFO;
			}
			/* if the column type is OLE, then we don't add terminators
			   see https://docs.microsoft.com/en-us/sql/odbc/reference/syntax/sqlgetdata-function?view=sql-server-ver15
			   and https://www.ibm.com/support/knowledgecenter/SSEPEK_11.0.0/odbc/src/tpc/db2z_fngetdata.html

			   "The buffer that the rgbValue argument specifies contains nul-terminated values, unless you retrieve
			   binary data, or the SQL data type of the column is graphic (DBCS) and the C buffer type is SQL_C_CHAR."
			   */
			const int totalSizeRemaining = stmt->ole_len - stmt->pos;
			const int partsRemain = cbValueMax < totalSizeRemaining;
			const int sizeToReadThisPart = partsRemain ? cbValueMax : totalSizeRemaining;
			memcpy(rgbValue, stmt->ole_str + stmt->pos, sizeToReadThisPart);

			if (partsRemain) {
				stmt->pos += cbValueMax;
				strcpy(stmt->sqlState, "01004"); // truncated
				return SQL_SUCCESS_WITH_INFO;
			}
			stmt->pos = stmt->ole_len;
			free(stmt->ole_str);
			stmt->ole_str = NULL;
			break;
		default: /* FIXME here we assume fCType == SQL_C_CHAR || fCType == SQL_C_WCHAR */
		to_c_char:
		{
			if (cbValueMax < 0) {
				strcpy(stmt->sqlState, "HY090"); // Invalid string or buffer length
				return SQL_ERROR;
			}
			char *str = NULL;
			SQLWCHAR *wstr = NULL;
			if (col->col_type == MDB_NUMERIC) {
				str = mdb_numeric_to_string(mdb, col->cur_value_start,
						col->col_scale, col->col_prec);
			} else {
				str = mdb_col_to_string(mdb, mdb->pg_buf,
						col->cur_value_start, col->col_type, col->cur_value_len);
			}
			size_t len = strlen(str);
			size_t charsize = 1;
			if (fCType == SQL_C_WCHAR) {
				wstr = calloc(len+1, charsize = sizeof(SQLWCHAR));
				len = _mdb_odbc_ascii2unicode(((struct _hstmt *)hstmt)->hdbc, str, len, wstr, len+1);
			}

			if (stmt->pos >= len) {
				free(wstr); wstr = NULL;
				free(str); str = NULL;
				return SQL_NO_DATA;
			}
			if (pcbValue) {
				*pcbValue = (len - stmt->pos) * charsize;
			}
			if (cbValueMax == 0) {
				free(wstr); wstr = NULL;
				free(str); str = NULL;
				return SQL_SUCCESS_WITH_INFO;
			}

			const int totalCharactersRemaining = len - stmt->pos;
			const int partsRemain = cbValueMax/charsize - 1 < totalCharactersRemaining;
			const int charactersToReadThisPart = partsRemain ? cbValueMax/charsize - 1 : totalCharactersRemaining;

			if (wstr) {
				memcpy(rgbValue, wstr + stmt->pos, charactersToReadThisPart * sizeof(SQLWCHAR));
				((SQLWCHAR *)rgbValue)[charactersToReadThisPart] = '\0';
			} else {
				memcpy(rgbValue, str + stmt->pos, charactersToReadThisPart);
				((SQLCHAR *)rgbValue)[charactersToReadThisPart] = '\0';
			}

			free(wstr); wstr = NULL;
			free(str); str = NULL;

			if (partsRemain) {
				stmt->pos += charactersToReadThisPart;
				strcpy(stmt->sqlState, "01004"); // truncated
				return SQL_SUCCESS_WITH_INFO;
			}
			stmt->pos = len;
			break;
		}
	}
	return SQL_SUCCESS;
}

static void _set_func_exists(SQLUSMALLINT *pfExists, SQLUSMALLINT fFunction)
{
	SQLUSMALLINT     *mod;
	mod = pfExists + (fFunction >> 4);
	*mod |= (1 << (fFunction & 0x0f));
}

SQLRETURN SQL_API SQLGetFunctions(
    SQLHDBC            hdbc,
    SQLUSMALLINT       fFunction,
    SQLUSMALLINT      *pfExists)
{

	TRACE("SQLGetFunctions");
	switch (fFunction) {
#if ODBCVER >= 0x0300
		case SQL_API_ODBC3_ALL_FUNCTIONS:
			memset(pfExists, 0, SQL_API_ODBC3_ALL_FUNCTIONS_SIZE);
			_set_func_exists(pfExists,SQL_API_SQLALLOCCONNECT);
			_set_func_exists(pfExists,SQL_API_SQLALLOCENV);
			_set_func_exists(pfExists,SQL_API_SQLALLOCHANDLE);
			_set_func_exists(pfExists,SQL_API_SQLALLOCSTMT);
			_set_func_exists(pfExists,SQL_API_SQLBINDCOL);
			_set_func_exists(pfExists,SQL_API_SQLBINDPARAMETER);
			_set_func_exists(pfExists,SQL_API_SQLCANCEL);
			//_set_func_exists(pfExists,SQL_API_SQLCLOSECURSOR);
			_set_func_exists(pfExists,SQL_API_SQLCOLATTRIBUTE);
			_set_func_exists(pfExists,SQL_API_SQLCOLUMNS);
			_set_func_exists(pfExists,SQL_API_SQLCONNECT);
			//_set_func_exists(pfExists,SQL_API_SQLCOPYDESC);
			_set_func_exists(pfExists,SQL_API_SQLDATASOURCES);
			_set_func_exists(pfExists,SQL_API_SQLDESCRIBECOL);
			_set_func_exists(pfExists,SQL_API_SQLDISCONNECT);
			//_set_func_exists(pfExists,SQL_API_SQLENDTRAN);
			_set_func_exists(pfExists,SQL_API_SQLERROR);
			_set_func_exists(pfExists,SQL_API_SQLEXECDIRECT);
			_set_func_exists(pfExists,SQL_API_SQLEXECUTE);
			_set_func_exists(pfExists,SQL_API_SQLFETCH);
			//_set_func_exists(pfExists,SQL_API_SQLFETCHSCROLL);
			_set_func_exists(pfExists,SQL_API_SQLFREECONNECT);
			_set_func_exists(pfExists,SQL_API_SQLFREEENV);
			_set_func_exists(pfExists,SQL_API_SQLFREEHANDLE);
			_set_func_exists(pfExists,SQL_API_SQLFREESTMT);
			//_set_func_exists(pfExists,SQL_API_SQLGETCONNECTATTR);
			_set_func_exists(pfExists,SQL_API_SQLGETCONNECTOPTION);
			_set_func_exists(pfExists,SQL_API_SQLGETCURSORNAME);
			_set_func_exists(pfExists,SQL_API_SQLGETDATA);
			//_set_func_exists(pfExists,SQL_API_SQLGETDESCFIELD);
			//_set_func_exists(pfExists,SQL_API_SQLGETDESCREC);
			//_set_func_exists(pfExists,SQL_API_SQLGETDIAGFIELD);
			//_set_func_exists(pfExists,SQL_API_SQLGETDIAGREC);
			//_set_func_exists(pfExists,SQL_API_SQLGETENVATTR);
			_set_func_exists(pfExists,SQL_API_SQLGETFUNCTIONS);
			_set_func_exists(pfExists,SQL_API_SQLGETINFO);
			_set_func_exists(pfExists,SQL_API_SQLGETSTMTATTR);
			_set_func_exists(pfExists,SQL_API_SQLGETSTMTOPTION);
			_set_func_exists(pfExists,SQL_API_SQLGETTYPEINFO);
			_set_func_exists(pfExists,SQL_API_SQLNUMRESULTCOLS);
			_set_func_exists(pfExists,SQL_API_SQLPARAMDATA);
			_set_func_exists(pfExists,SQL_API_SQLPREPARE);
			_set_func_exists(pfExists,SQL_API_SQLPUTDATA);
			_set_func_exists(pfExists,SQL_API_SQLROWCOUNT);
			//_set_func_exists(pfExists,SQL_API_SQLSETCONNECTATTR);
			_set_func_exists(pfExists,SQL_API_SQLSETCONNECTOPTION);
			_set_func_exists(pfExists,SQL_API_SQLSETCURSORNAME);
			//_set_func_exists(pfExists,SQL_API_SQLSETDESCFIELD);
			//_set_func_exists(pfExists,SQL_API_SQLSETDESCREC);
			_set_func_exists(pfExists,SQL_API_SQLSETENVATTR);
			_set_func_exists(pfExists,SQL_API_SQLSETPARAM);
			//_set_func_exists(pfExists,SQL_API_SQLSETSTMTATTR);
			_set_func_exists(pfExists,SQL_API_SQLSETSTMTOPTION);
			_set_func_exists(pfExists,SQL_API_SQLSPECIALCOLUMNS);
			_set_func_exists(pfExists,SQL_API_SQLSTATISTICS);
			_set_func_exists(pfExists,SQL_API_SQLTABLES);
			_set_func_exists(pfExists,SQL_API_SQLTRANSACT);
			break;
#endif
		case SQL_API_ALL_FUNCTIONS:
			memset(pfExists, 0, 100); // 100 by spec
			_set_func_exists(pfExists,SQL_API_SQLALLOCCONNECT);
			_set_func_exists(pfExists,SQL_API_SQLALLOCENV);
			_set_func_exists(pfExists,SQL_API_SQLALLOCSTMT);
			_set_func_exists(pfExists,SQL_API_SQLBINDCOL);
			_set_func_exists(pfExists,SQL_API_SQLCANCEL);
			_set_func_exists(pfExists,SQL_API_SQLCOLATTRIBUTE);
			_set_func_exists(pfExists,SQL_API_SQLCOLUMNS);
			_set_func_exists(pfExists,SQL_API_SQLCONNECT);
			_set_func_exists(pfExists,SQL_API_SQLDATASOURCES);
			_set_func_exists(pfExists,SQL_API_SQLDESCRIBECOL);
			_set_func_exists(pfExists,SQL_API_SQLDISCONNECT);
			_set_func_exists(pfExists,SQL_API_SQLERROR);
			_set_func_exists(pfExists,SQL_API_SQLEXECDIRECT);
			_set_func_exists(pfExists,SQL_API_SQLEXECUTE);
			_set_func_exists(pfExists,SQL_API_SQLFETCH);
			_set_func_exists(pfExists,SQL_API_SQLFREECONNECT);
			_set_func_exists(pfExists,SQL_API_SQLFREEENV);
			_set_func_exists(pfExists,SQL_API_SQLFREEHANDLE);
			_set_func_exists(pfExists,SQL_API_SQLFREESTMT);
			_set_func_exists(pfExists,SQL_API_SQLGETCONNECTOPTION);
			_set_func_exists(pfExists,SQL_API_SQLGETCURSORNAME);
			_set_func_exists(pfExists,SQL_API_SQLGETDATA);
			_set_func_exists(pfExists,SQL_API_SQLGETFUNCTIONS);
			_set_func_exists(pfExists,SQL_API_SQLGETINFO);
			_set_func_exists(pfExists,SQL_API_SQLGETSTMTATTR);
			_set_func_exists(pfExists,SQL_API_SQLGETSTMTOPTION);
			_set_func_exists(pfExists,SQL_API_SQLGETTYPEINFO);
			_set_func_exists(pfExists,SQL_API_SQLNUMRESULTCOLS);
			_set_func_exists(pfExists,SQL_API_SQLPARAMDATA);
			_set_func_exists(pfExists,SQL_API_SQLPREPARE);
			_set_func_exists(pfExists,SQL_API_SQLPUTDATA);
			_set_func_exists(pfExists,SQL_API_SQLROWCOUNT);
			_set_func_exists(pfExists,SQL_API_SQLSETCONNECTOPTION);
			_set_func_exists(pfExists,SQL_API_SQLSETCURSORNAME);
			_set_func_exists(pfExists,SQL_API_SQLSETENVATTR);
			_set_func_exists(pfExists,SQL_API_SQLSETPARAM);
			_set_func_exists(pfExists,SQL_API_SQLSETSTMTOPTION);
			_set_func_exists(pfExists,SQL_API_SQLSPECIALCOLUMNS);
			_set_func_exists(pfExists,SQL_API_SQLSTATISTICS);
			_set_func_exists(pfExists,SQL_API_SQLTABLES);
			_set_func_exists(pfExists,SQL_API_SQLTRANSACT);
			break;

		case SQL_API_SQLALLOCCONNECT:
		case SQL_API_SQLALLOCENV:
		case SQL_API_SQLALLOCSTMT:
		case SQL_API_SQLBINDCOL:
		case SQL_API_SQLCANCEL:
		case SQL_API_SQLCOLATTRIBUTE:
		case SQL_API_SQLCOLUMNS:
		case SQL_API_SQLCONNECT:
		case SQL_API_SQLDATASOURCES:
		case SQL_API_SQLDESCRIBECOL:
		case SQL_API_SQLDISCONNECT:
		case SQL_API_SQLERROR:
		case SQL_API_SQLEXECDIRECT:
		case SQL_API_SQLEXECUTE:
		case SQL_API_SQLFETCH:
		case SQL_API_SQLFREECONNECT:
		case SQL_API_SQLFREEENV:
		case SQL_API_SQLFREEHANDLE:
		case SQL_API_SQLFREESTMT:
		case SQL_API_SQLGETCONNECTOPTION:
		case SQL_API_SQLGETCURSORNAME:
		case SQL_API_SQLGETDATA:
		case SQL_API_SQLGETFUNCTIONS:
		case SQL_API_SQLGETINFO:
		case SQL_API_SQLGETSTMTATTR:
		case SQL_API_SQLGETSTMTOPTION:
		case SQL_API_SQLGETTYPEINFO:
		case SQL_API_SQLNUMRESULTCOLS:
		case SQL_API_SQLPARAMDATA:
		case SQL_API_SQLPREPARE:
		case SQL_API_SQLPUTDATA:
		case SQL_API_SQLROWCOUNT:
		case SQL_API_SQLSETCONNECTOPTION:
		case SQL_API_SQLSETCURSORNAME:
		case SQL_API_SQLSETENVATTR:
		case SQL_API_SQLSETPARAM:
		case SQL_API_SQLSETSTMTOPTION:
		case SQL_API_SQLSPECIALCOLUMNS:
		case SQL_API_SQLSTATISTICS:
		case SQL_API_SQLTABLES:
		case SQL_API_SQLTRANSACT:
			*pfExists = 1; /* SQL_TRUE */
			break;

		default:
			*pfExists = 0; /* SQL_FALSE */
			break;
	}
	return SQL_SUCCESS;
}

SQLRETURN SQL_API SQLGetInfo(
    SQLHDBC            hdbc,
    SQLUSMALLINT       fInfoType,
    SQLPOINTER         rgbInfoValue,
    SQLSMALLINT        cbInfoValueMax,
    SQLSMALLINT       *pcbInfoValue)
{
	TRACE("SQLGetInfo");
	switch (fInfoType) {
	case SQL_MAX_STATEMENT_LEN:
		if (rgbInfoValue)
			*((SQLUINTEGER *)rgbInfoValue) = (SQLUINTEGER)65000;
		if (pcbInfoValue)
			*pcbInfoValue = sizeof(SQLUINTEGER);
	break;
	case SQL_SCHEMA_USAGE:
		if (rgbInfoValue)
			*((SQLSMALLINT *)rgbInfoValue) = (SQLSMALLINT)0;
		if (pcbInfoValue)
			*pcbInfoValue = sizeof(SQLSMALLINT);
	break;
	case SQL_CATALOG_NAME_SEPARATOR:
		if (rgbInfoValue)
			memcpy(rgbInfoValue, ".", 1);
		if (pcbInfoValue)
			*pcbInfoValue = 1;
	break;
	case SQL_CATALOG_LOCATION:
		if (rgbInfoValue)
			*((SQLSMALLINT *)rgbInfoValue) = (SQLSMALLINT)1;
		if (pcbInfoValue)
			*pcbInfoValue = sizeof(SQLSMALLINT);
	break;
	case SQL_IDENTIFIER_QUOTE_CHAR:
		if (rgbInfoValue)
			memcpy(rgbInfoValue, "\"", 1);
		if (pcbInfoValue)
			*pcbInfoValue = 1;
	break;
	case SQL_DBMS_NAME:
		if (rgbInfoValue)
			snprintf(rgbInfoValue, cbInfoValueMax, "%s", "MDBTOOLS");
		if (pcbInfoValue)
			*pcbInfoValue = sizeof("MDBTOOLS");
	break;
	case SQL_DBMS_VER:
		if (rgbInfoValue)
			snprintf(rgbInfoValue, cbInfoValueMax, "%s", VERSION);
		if (pcbInfoValue)
			*pcbInfoValue = sizeof(VERSION);
	break;
	default:
		if (pcbInfoValue)
			*pcbInfoValue = 0;
		strcpy(((struct _hdbc *)hdbc)->sqlState, "HYC00");
		return SQL_ERROR;
	}

	return SQL_SUCCESS;
}

SQLRETURN SQL_API SQLGetStmtOption(
    SQLHSTMT           hstmt,
    SQLUSMALLINT       fOption,
    SQLPOINTER         pvParam)
{
	TRACE("SQLGetStmtOption");
	return SQL_SUCCESS;
}

SQLRETURN SQL_API SQLGetTypeInfo(
    SQLHSTMT           hstmt,
    SQLSMALLINT        fSqlType)
{
	struct _hstmt *stmt = (struct _hstmt *) hstmt;
	MdbTableDef *ttable;
	MdbSQL *sql = stmt->sql;
	MdbHandle *mdb = sql->mdb;
	int row_size;
	unsigned char row_buffer[MDB_PGSIZE];
	unsigned int ts0, ts3, ts4, ts5, ts12;
	unsigned char t0[MDB_BIND_SIZE],
	              t3[MDB_BIND_SIZE],
	              t4[MDB_BIND_SIZE],
	              t5[MDB_BIND_SIZE],
	              t12[MDB_BIND_SIZE];
	int i;
	MdbField fields[NUM_TYPE_INFO_COLS];

	TRACE("SQLGetTypeInfo");
	
	ttable = mdb_create_temp_table(mdb, "#typeinfo");
	mdb_sql_add_temp_col(sql, ttable, 0, "TYPE_NAME", MDB_TEXT, 30, 0);
	mdb_sql_add_temp_col(sql, ttable, 1, "DATA_TYPE", MDB_INT, 0, 1);
	mdb_sql_add_temp_col(sql, ttable, 2, "COLUMN_SIZE", MDB_LONGINT, 0, 1);
	mdb_sql_add_temp_col(sql, ttable, 3, "LITERAL_PREFIX", MDB_TEXT, 30, 0);
	mdb_sql_add_temp_col(sql, ttable, 4, "LITERAL_SUFFIX", MDB_TEXT, 30, 0);
	mdb_sql_add_temp_col(sql, ttable, 5, "CREATE_PARAMS", MDB_TEXT, 30, 0);
	mdb_sql_add_temp_col(sql, ttable, 6, "NULLABLE", MDB_INT, 0, 1);
	mdb_sql_add_temp_col(sql, ttable, 7, "CASE_SENSITIVE", MDB_INT, 0, 1);
	mdb_sql_add_temp_col(sql, ttable, 8, "SEARCHABLE", MDB_INT, 0, 1);
	mdb_sql_add_temp_col(sql, ttable, 9, "UNSIGNED_ATTRIBUTE", MDB_INT, 0, 1);
	mdb_sql_add_temp_col(sql, ttable, 10, "FIXED_PREC_SCALE", MDB_INT, 0, 1);
	mdb_sql_add_temp_col(sql, ttable, 11, "AUTO_UNIQUE_VALUE", MDB_INT, 0, 1);
	mdb_sql_add_temp_col(sql, ttable, 12, "LOCAL_TYPE_NAME", MDB_TEXT, 30, 0);
	mdb_sql_add_temp_col(sql, ttable, 13, "MINIMUM_SCALE", MDB_INT, 0, 1);
	mdb_sql_add_temp_col(sql, ttable, 14, "MAXIMUM_SCALE", MDB_INT, 0, 1);
	mdb_sql_add_temp_col(sql, ttable, 15, "SQL_DATA_TYPE", MDB_INT, 0, 1);
	mdb_sql_add_temp_col(sql, ttable, 16, "SQL_DATETIME_SUB", MDB_INT, 0, 1);
	mdb_sql_add_temp_col(sql, ttable, 17, "NUM_PREC_RADIX", MDB_INT, 0, 1);
	mdb_sql_add_temp_col(sql, ttable, 18, "INTERVAL_PRECISION", MDB_INT, 0, 1);
	mdb_temp_columns_end(ttable);

	for (i=0; i<MAX_TYPE_INFO; i++) {
		if (fSqlType && (fSqlType != type_info[i].data_type))
			continue;

		ts0 = mdb_ascii2unicode(mdb, (char*)type_info[i].type_name, 0, (char*)t0, sizeof(t0));
		ts3 = mdb_ascii2unicode(mdb, (char*)type_info[i].literal_prefix, 0, (char*)t3, sizeof(t3));
		ts4 = mdb_ascii2unicode(mdb, (char*)type_info[i].literal_suffix, 0, (char*)t4, sizeof(t4));
		ts5 = mdb_ascii2unicode(mdb, (char*)type_info[i].create_params, 0, (char*)t5, sizeof(t5));
		ts12 = mdb_ascii2unicode(mdb, (char*)type_info[i].local_type_name, 0, (char*)t12, sizeof(t12));

		FILL_FIELD(&fields[0], t0, ts0);
		FILL_FIELD(&fields[1],&type_info[i].data_type, 0);
		FILL_FIELD(&fields[2],&type_info[i].column_size, 0);
		FILL_FIELD(&fields[3], t3, ts3);
		FILL_FIELD(&fields[4], t4, ts4);
		FILL_FIELD(&fields[5], t5, ts5);
		FILL_FIELD(&fields[6],&type_info[i].nullable, 0);
		FILL_FIELD(&fields[7],&type_info[i].case_sensitive, 0);
		FILL_FIELD(&fields[8],&type_info[i].searchable, 0);
		FILL_FIELD(&fields[9],type_info[i].unsigned_attribute, 0);
		FILL_FIELD(&fields[10],&type_info[i].fixed_prec_scale, 0);
		FILL_FIELD(&fields[11],&type_info[i].auto_unique_value, 0);
		FILL_FIELD(&fields[12], t12, ts12);
		FILL_FIELD(&fields[13],&type_info[i].minimum_scale, 0);
		FILL_FIELD(&fields[14],&type_info[i].maximum_scale, 0);
		FILL_FIELD(&fields[15],&type_info[i].sql_data_type, 0);
		FILL_FIELD(&fields[16],type_info[i].sql_datetime_sub, 0);
		FILL_FIELD(&fields[17],type_info[i].num_prec_radix, 0);
		FILL_FIELD(&fields[18],type_info[i].interval_precision, 0);

		row_size = mdb_pack_row(ttable, row_buffer, NUM_TYPE_INFO_COLS, fields);
		mdb_add_row_to_pg(ttable,row_buffer, row_size);
		ttable->num_rows++;
	}
	sql->cur_table = ttable;
	
	/* return SQLExecute(hstmt); */
	return SQL_SUCCESS;
}

SQLRETURN SQL_API SQLParamData(
    SQLHSTMT           hstmt,
    SQLPOINTER        *prgbValue)
{
	TRACE("SQLParamData");
	return SQL_SUCCESS;
}

SQLRETURN SQL_API SQLPutData(
    SQLHSTMT           hstmt,
    SQLPOINTER         rgbValue,
    SQLLEN             cbValue)
{
	TRACE("SQLPutData");
	return SQL_SUCCESS;
}

SQLRETURN SQL_API SQLSetConnectOption(
    SQLHDBC            hdbc,
    SQLUSMALLINT       fOption,
    SQLULEN            vParam)
{
	TRACE("SQLSetConnectOption");
	return SQL_SUCCESS;
}

SQLRETURN SQL_API SQLSetStmtOption(
    SQLHSTMT           hstmt,
    SQLUSMALLINT       fOption,
    SQLULEN            vParam)
{
	TRACE("SQLSetStmtOption");
	return SQL_SUCCESS;
}

SQLRETURN SQL_API SQLSpecialColumns(
    SQLHSTMT           hstmt,
    SQLUSMALLINT       fColType,
    SQLCHAR           *szCatalogName,
    SQLSMALLINT        cbCatalogName,
    SQLCHAR           *szSchemaName,
    SQLSMALLINT        cbSchemaName,
    SQLCHAR           *szTableName,
    SQLSMALLINT        cbTableName,
    SQLUSMALLINT       fScope,
    SQLUSMALLINT       fNullable)
{
	TRACE("SQLSpecialColumns");
	return SQL_SUCCESS;
}

SQLRETURN SQL_API SQLStatistics(
    SQLHSTMT           hstmt,
    SQLCHAR           *szCatalogName,
    SQLSMALLINT        cbCatalogName,
    SQLCHAR           *szSchemaName,
    SQLSMALLINT        cbSchemaName,
    SQLCHAR           *szTableName,
    SQLSMALLINT        cbTableName,
    SQLUSMALLINT       fUnique,
    SQLUSMALLINT       fAccuracy)
{
	TRACE("SQLStatistics");
	return SQL_SUCCESS;
}

SQLRETURN SQL_API SQLTables( //sz* not used, so Unicode API not required.
    SQLHSTMT           hstmt,
    SQLCHAR           *szCatalogName,
    SQLSMALLINT        cbCatalogName,
    SQLCHAR           *szSchemaName,
    SQLSMALLINT        cbSchemaName,
    SQLCHAR           *szTableName,
    SQLSMALLINT        cbTableName,
    SQLCHAR           *szTableType,
    SQLSMALLINT        cbTableType)
{
	struct _hstmt *stmt = (struct _hstmt *) hstmt;
	MdbSQL *sql = stmt->sql;
	MdbHandle *mdb = sql->mdb;
	MdbTableDef *ttable;
	MdbField fields[5];
	MdbCatalogEntry *entry;
	unsigned char row_buffer[MDB_PGSIZE];
	char *table_types[] = {"TABLE", "SYSTEM TABLE", "VIEW"};
	unsigned int i, j, row_size, ttype;
	unsigned int ts2, ts3;
	unsigned char t2[MDB_BIND_SIZE],
	              t3[MDB_BIND_SIZE];

	TRACE("SQLTables");

	mdb_read_catalog(mdb, MDB_ANY);

	ttable = mdb_create_temp_table(mdb, "#tables");
	mdb_sql_add_temp_col(sql, ttable, 0, "TABLE_CAT", MDB_TEXT, 128, 0);
	mdb_sql_add_temp_col(sql, ttable, 1, "TABLE_SCHEM", MDB_TEXT, 128, 0);
	mdb_sql_add_temp_col(sql, ttable, 2, "TABLE_NAME", MDB_TEXT, 128, 0);
	mdb_sql_add_temp_col(sql, ttable, 3, "TABLE_TYPE", MDB_TEXT, 128, 0);
	mdb_sql_add_temp_col(sql, ttable, 4, "REMARKS", MDB_TEXT, 254, 0);
	mdb_temp_columns_end(ttable);

	/* TODO: Sort the return list by TYPE, CAT, SCHEM, NAME */
	for (i=0; i<mdb->num_catalog; i++) {
     		entry = g_ptr_array_index(mdb->catalog, i);

		if (mdb_is_user_table(entry))
			ttype = 0;
		else if (mdb_is_system_table(entry))
			ttype = 1;
		else if (entry->object_type == MDB_QUERY)
			ttype = 2;
		else
			continue;

		/* Set all fields to NULL */
		for (j=0; j<5; j++) {
			FILL_FIELD(&fields[j], NULL, 0);
		}

		ts2 = mdb_ascii2unicode(mdb, entry->object_name, 0, (char*)t2, sizeof(t2));
		ts3 = mdb_ascii2unicode(mdb, table_types[ttype], 0, (char*)t3, sizeof(t3));

		FILL_FIELD(&fields[2], t2, ts2);
		FILL_FIELD(&fields[3], t3, ts3);
		
		row_size = mdb_pack_row(ttable, row_buffer, 5, fields);
		mdb_add_row_to_pg(ttable, row_buffer, row_size);
		ttable->num_rows++;
	}
	sql->cur_table = ttable;

	return SQL_SUCCESS;
}


SQLRETURN SQL_API SQLDataSources(
    SQLHENV            henv,
    SQLUSMALLINT       fDirection,
    SQLCHAR           *szDSN,
    SQLSMALLINT        cbDSNMax,
    SQLSMALLINT       *pcbDSN,
    SQLCHAR           *szDescription,
    SQLSMALLINT        cbDescriptionMax,
    SQLSMALLINT       *pcbDescription)
{
	TRACE("SQLDataSources");
	return SQL_SUCCESS;
}

static int _odbc_fix_literals(struct _hstmt *stmt)
{
	char tmp[4096];
	char *s, *d, *p;
	int i, quoted = 0, find_end = 0;
	char quote_char;

	s=stmt->query;
	d=tmp;
	while (*s && d<tmp+sizeof(tmp)) {
		if (!quoted && (*s=='"' || *s=='\'')) {
			quoted = 1;
			quote_char = *s;
		} else if (quoted && *s==quote_char) {
			quoted = 0;
		}
		if (!quoted && find_end && *s=='}') {
			s++; /* ignore the end of tag */
		} else if (!quoted && *s=='{') {
			for (p=s,i=0;*p && *p!=' ';p++) i++;
			if (i>10) {
				/* garbage */
				*d++=*s++;
			} else {
				/* printf("begin tag %.*s\n", i, s); */
				s += i;
				find_end = 1;
			}
		} else {
			*d++=*s++;	
		}
	}

	snprintf(stmt->query, sizeof(stmt->query), "%.*s", (int)(d-tmp), tmp);

	return 0;
}

static int _odbc_get_string_size(int size, SQLCHAR *str)
{
	if (!str) {
		return 0;
	}
	if (size==SQL_NTS) {
		return strlen((char*)str);
	} else {
		return size;
	}
	return 0;
}
/*
static int _odbc_get_server_type(int clt_type)
{
	switch (clt_type) {
	case SQL_CHAR:
	case SQL_VARCHAR:
	case SQL_BIT:
	case SQL_TINYINT:
	case SQL_SMALLINT:
	case SQL_INTEGER:
	case SQL_DOUBLE:
	case SQL_DECIMAL:
	case SQL_NUMERIC:
	case SQL_FLOAT:
	default:
		break;
	}
	return 0;
}*/
static SQLSMALLINT _odbc_get_client_type(MdbColumn *col)
{
	switch (col->col_type) {
		case MDB_BOOL:
			return SQL_BIT;
		case MDB_BYTE:
			return SQL_TINYINT;
		case MDB_INT:
			return SQL_SMALLINT;
		case MDB_LONGINT:
			return SQL_INTEGER;
		case MDB_MONEY:
			return SQL_DECIMAL;
		case MDB_FLOAT:
			return SQL_FLOAT;
		case MDB_DOUBLE:
			return SQL_DOUBLE;
		case MDB_DATETIME: ;
#if ODBCVER >= 0x0300
			const char *format = mdb_col_get_prop(col, "Format");
			if (format && !strcmp(format, "Short Date"))
				return SQL_TYPE_DATE;
			else
				return SQL_TYPE_TIMESTAMP;
#endif // returns text otherwise
		case MDB_TEXT:
		case MDB_MEMO:
			return SQL_VARCHAR;
		case MDB_OLE:
			return SQL_LONGVARBINARY;
		default:
			// fprintf(stderr,"Unknown type %d\n",srv_type);
			break;
	}
	return -1;
}

static const char * _odbc_get_client_type_name(MdbColumn *col)
{
	switch (col->col_type) {
		case MDB_BOOL:
			return "BOOL";
		case MDB_BYTE:
			return "BYTE";
		case MDB_INT:
			return "INT";
		case MDB_LONGINT:
			return "LONGINT";
		case MDB_MONEY:
			return "MONEY";
		case MDB_FLOAT:
			return "FLOAT";
		case MDB_DOUBLE:
			return "DOUBLE";
		case MDB_DATETIME:
			return "DATETIME";
		case MDB_TEXT:
			return "TEXT";
		case MDB_BINARY:
			return "BINARY";
		case MDB_MEMO:
			return "MEMO";
		case MDB_OLE:
			return "OLE";
		case MDB_REPID:
			return "REPID";
		case MDB_NUMERIC:
			return "NUMERIC";
		case MDB_COMPLEX:
			return "COMPLEX";
		default:
			fprintf(stderr,"Unknown type for column %s: %d\n",col->name,col->col_type);
			break;
	}
	return NULL;
}

/** @}*/
