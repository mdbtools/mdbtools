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

#ifdef UNIXODBC
#include <sql.h>
#include <sqlext.h>
#else
#include "isql.h"
#include "isqlext.h"
#endif

#include <mdbodbc.h>

#include <string.h>
#include <stdio.h>

#include "connectparams.h"

static char  software_version[]   = "$Id: odbc.c,v 1.5 2002/04/03 23:02:54 brianb Exp $";
static void *no_unused_var_warn[] = {software_version,
                                     no_unused_var_warn};

static SQLSMALLINT _odbc_get_client_type(int srv_type);
static int _odbc_fix_literals(struct _hstmt *stmt);
static int _odbc_get_server_type(int clt_type);
static int _odbc_get_string_size(int size, char *str);
static SQLRETURN SQL_API _SQLAllocConnect(SQLHENV henv, SQLHDBC FAR *phdbc);
static SQLRETURN SQL_API _SQLAllocEnv(SQLHENV FAR *phenv);
static SQLRETURN SQL_API _SQLAllocStmt(SQLHDBC hdbc, SQLHSTMT FAR *phstmt);
static SQLRETURN SQL_API _SQLFreeConnect(SQLHDBC hdbc);
static SQLRETURN SQL_API _SQLFreeEnv(SQLHENV henv);
static SQLRETURN SQL_API _SQLFreeStmt(SQLHSTMT hstmt, SQLUSMALLINT fOption);

#define MIN(a,b) (a>b ? b : a)
#define _MAX_ERROR_LEN 255
static char lastError[_MAX_ERROR_LEN+1];

/* The SQL engine is presently non-reenterrant and non-thread safe.  
   See _SQLExecute for details.
*/

static void LogError (const char* error)
{
   /*
    * Someday, I might make this store more than one error.
    */
   strncpy (lastError, error, _MAX_ERROR_LEN);
   lastError[_MAX_ERROR_LEN] = '\0'; /* in case we had a long message */
}

/*
 * Driver specific connectionn information
 */

typedef struct
{
   struct _hdbc hdbc;
   ConnectParams* params;
} ODBCConnection;

static SQLRETURN do_connect (
   SQLHDBC hdbc,
   SQLCHAR FAR *database
)
{
   struct _hdbc *dbc = (struct _hdbc *) hdbc;
   struct _henv *env = (struct _henv *) dbc->henv;

   if (mdb_sql_open(env->sql,database)) {
   	return SQL_SUCCESS;
   } else {
     return SQL_ERROR;
   }
}

SQLRETURN SQL_API SQLDriverConnect(
    SQLHDBC            hdbc,
    SQLHWND            hwnd,
    SQLCHAR FAR       *szConnStrIn,
    SQLSMALLINT        cbConnStrIn,
    SQLCHAR FAR       *szConnStrOut,
    SQLSMALLINT        cbConnStrOutMax,
    SQLSMALLINT FAR   *pcbConnStrOut,
    SQLUSMALLINT       fDriverCompletion)
{
   SQLCHAR FAR* dsn = NULL;
   SQLCHAR FAR* database = NULL;
   ConnectParams* params;
   SQLRETURN ret;

   strcpy (lastError, "");

   params = ((ODBCConnection*) hdbc)->params;

   if (!(dsn = ExtractDSN (params, szConnStrIn)))
   {
      LogError ("Could not find DSN in connect string");
      return SQL_ERROR;
   }
   else if (!LookupDSN (params, dsn))
   {
      LogError ("Could not find DSN in odbc.ini");
      return SQL_ERROR;
   }
   else 
   {
      SetConnectString (params, szConnStrIn);

      if (!(database = GetConnectParam (params, "Database")))
      {
	 LogError ("Could not find Database parameter");
	 return SQL_ERROR;
      }
   }
   ret = do_connect (hdbc, database);
   return ret;
}

SQLRETURN SQL_API SQLBrowseConnect(
    SQLHDBC            hdbc,
    SQLCHAR FAR       *szConnStrIn,
    SQLSMALLINT        cbConnStrIn,
    SQLCHAR FAR       *szConnStrOut,
    SQLSMALLINT        cbConnStrOutMax,
    SQLSMALLINT FAR   *pcbConnStrOut)
{
	return SQL_SUCCESS;
}

SQLRETURN SQL_API SQLColumnPrivileges(
    SQLHSTMT           hstmt,
    SQLCHAR FAR       *szCatalogName,
    SQLSMALLINT        cbCatalogName,
    SQLCHAR FAR       *szSchemaName,
    SQLSMALLINT        cbSchemaName,
    SQLCHAR FAR       *szTableName,
    SQLSMALLINT        cbTableName,
    SQLCHAR FAR       *szColumnName,
    SQLSMALLINT        cbColumnName)
{
	return SQL_SUCCESS;
}

SQLRETURN SQL_API SQLDescribeParam(
    SQLHSTMT           hstmt,
    SQLUSMALLINT       ipar,
    SQLSMALLINT FAR   *pfSqlType,
    SQLUINTEGER FAR   *pcbParamDef,
    SQLSMALLINT FAR   *pibScale,
    SQLSMALLINT FAR   *pfNullable)
{
	return SQL_SUCCESS;
}

SQLRETURN SQL_API SQLExtendedFetch(
    SQLHSTMT           hstmt,
    SQLUSMALLINT       fFetchType,
    SQLINTEGER         irow,
    SQLUINTEGER FAR   *pcrow,
    SQLUSMALLINT FAR  *rgfRowStatus)
{
	return SQL_SUCCESS;
}

SQLRETURN SQL_API SQLForeignKeys(
    SQLHSTMT           hstmt,
    SQLCHAR FAR       *szPkCatalogName,
    SQLSMALLINT        cbPkCatalogName,
    SQLCHAR FAR       *szPkSchemaName,
    SQLSMALLINT        cbPkSchemaName,
    SQLCHAR FAR       *szPkTableName,
    SQLSMALLINT        cbPkTableName,
    SQLCHAR FAR       *szFkCatalogName,
    SQLSMALLINT        cbFkCatalogName,
    SQLCHAR FAR       *szFkSchemaName,
    SQLSMALLINT        cbFkSchemaName,
    SQLCHAR FAR       *szFkTableName,
    SQLSMALLINT        cbFkTableName)
{
	return SQL_SUCCESS;
}

SQLRETURN SQL_API SQLMoreResults(
    SQLHSTMT           hstmt)
{
	return SQL_SUCCESS;
}

SQLRETURN SQL_API SQLNativeSql(
    SQLHDBC            hdbc,
    SQLCHAR FAR       *szSqlStrIn,
    SQLINTEGER         cbSqlStrIn,
    SQLCHAR FAR       *szSqlStr,
    SQLINTEGER         cbSqlStrMax,
    SQLINTEGER FAR    *pcbSqlStr)
{
	return SQL_SUCCESS;
}

SQLRETURN SQL_API SQLNumParams(
    SQLHSTMT           hstmt,
    SQLSMALLINT FAR   *pcpar)
{
	return SQL_SUCCESS;
}

SQLRETURN SQL_API SQLParamOptions(
    SQLHSTMT           hstmt,
    SQLUINTEGER        crow,
    SQLUINTEGER FAR   *pirow)
{
	return SQL_SUCCESS;
}

SQLRETURN SQL_API SQLPrimaryKeys(
    SQLHSTMT           hstmt,
    SQLCHAR FAR       *szCatalogName,
    SQLSMALLINT        cbCatalogName,
    SQLCHAR FAR       *szSchemaName,
    SQLSMALLINT        cbSchemaName,
    SQLCHAR FAR       *szTableName,
    SQLSMALLINT        cbTableName)
{
	return SQL_SUCCESS;
}

SQLRETURN SQL_API SQLProcedureColumns(
    SQLHSTMT           hstmt,
    SQLCHAR FAR       *szCatalogName,
    SQLSMALLINT        cbCatalogName,
    SQLCHAR FAR       *szSchemaName,
    SQLSMALLINT        cbSchemaName,
    SQLCHAR FAR       *szProcName,
    SQLSMALLINT        cbProcName,
    SQLCHAR FAR       *szColumnName,
    SQLSMALLINT        cbColumnName)
{
	return SQL_SUCCESS;
}

SQLRETURN SQL_API SQLProcedures(
    SQLHSTMT           hstmt,
    SQLCHAR FAR       *szCatalogName,
    SQLSMALLINT        cbCatalogName,
    SQLCHAR FAR       *szSchemaName,
    SQLSMALLINT        cbSchemaName,
    SQLCHAR FAR       *szProcName,
    SQLSMALLINT        cbProcName)
{
	return SQL_SUCCESS;
}

SQLRETURN SQL_API SQLSetPos(
    SQLHSTMT           hstmt,
    SQLUSMALLINT       irow,
    SQLUSMALLINT       fOption,
    SQLUSMALLINT       fLock)
{
	return SQL_SUCCESS;
}

SQLRETURN SQL_API SQLTablePrivileges(
    SQLHSTMT           hstmt,
    SQLCHAR FAR       *szCatalogName,
    SQLSMALLINT        cbCatalogName,
    SQLCHAR FAR       *szSchemaName,
    SQLSMALLINT        cbSchemaName,
    SQLCHAR FAR       *szTableName,
    SQLSMALLINT        cbTableName)
{
	return SQL_SUCCESS;
}

SQLRETURN SQL_API SQLDrivers(
    SQLHENV            henv,
    SQLUSMALLINT       fDirection,
    SQLCHAR FAR       *szDriverDesc,
    SQLSMALLINT        cbDriverDescMax,
    SQLSMALLINT FAR   *pcbDriverDesc,
    SQLCHAR FAR       *szDriverAttributes,
    SQLSMALLINT        cbDrvrAttrMax,
    SQLSMALLINT FAR   *pcbDrvrAttr)
{
	return SQL_SUCCESS;
}
SQLRETURN SQL_API SQLSetEnvAttr (
    SQLHENV EnvironmentHandle,
    SQLINTEGER Attribute,
    SQLPOINTER Value,
    SQLINTEGER StringLength)
{
	return SQL_SUCCESS;
}


SQLRETURN SQL_API SQLBindParameter(
    SQLHSTMT           hstmt,
    SQLUSMALLINT       ipar,
    SQLSMALLINT        fParamType,
    SQLSMALLINT        fCType,
    SQLSMALLINT        fSqlType,
    SQLUINTEGER        cbColDef,
    SQLSMALLINT        ibScale,
    SQLPOINTER         rgbValue,
    SQLINTEGER         cbValueMax,
    SQLINTEGER FAR    *pcbValue)
{
struct _hstmt *stmt;

	stmt = (struct _hstmt *) hstmt;
	return SQL_SUCCESS;
}

SQLRETURN SQL_API SQLAllocHandle(    
	SQLSMALLINT HandleType,
    	SQLHANDLE InputHandle,
    	SQLHANDLE * OutputHandle)
{
	switch(HandleType) {
		case SQL_HANDLE_STMT:
			return _SQLAllocStmt(InputHandle,OutputHandle);
			break;
		case SQL_HANDLE_DBC:
			return _SQLAllocConnect(InputHandle,OutputHandle);
			break;
		case SQL_HANDLE_ENV:
			return _SQLAllocEnv(OutputHandle);
			break;
	}
}
static SQLRETURN SQL_API _SQLAllocConnect(
    SQLHENV            henv,
    SQLHDBC FAR       *phdbc)
{
struct _henv *env;
ODBCConnection* dbc;

	env = (struct _henv *) henv;
        dbc = (SQLHDBC) malloc (sizeof (ODBCConnection));
        memset(dbc,'\0',sizeof (ODBCConnection));
	dbc->hdbc.henv=env;
	
     dbc->params = NewConnectParams ();
	*phdbc=dbc;

	return SQL_SUCCESS;
}
SQLRETURN SQL_API SQLAllocConnect(
    SQLHENV            henv,
    SQLHDBC FAR       *phdbc)
{
	return _SQLAllocConnect(henv, phdbc);
}

static SQLRETURN SQL_API _SQLAllocEnv(
    SQLHENV FAR       *phenv)
{
struct _henv *env;

     env = (SQLHENV) malloc(sizeof(struct _henv));
     memset(env,'\0',sizeof(struct _henv));
	*phenv=env;
	env->sql = mdb_sql_init();
	return SQL_SUCCESS;
}
SQLRETURN SQL_API SQLAllocEnv(
    SQLHENV FAR       *phenv)
{
	return _SQLAllocEnv(phenv);
}

static SQLRETURN SQL_API _SQLAllocStmt(
    SQLHDBC            hdbc,
    SQLHSTMT FAR      *phstmt)
{
struct _hdbc *dbc;
struct _hstmt *stmt;

	dbc = (struct _hdbc *) hdbc;

     stmt = (SQLHSTMT) malloc(sizeof(struct _hstmt));
     memset(stmt,'\0',sizeof(struct _hstmt));
	stmt->hdbc=hdbc;
	*phstmt = stmt;

	return SQL_SUCCESS;
}
SQLRETURN SQL_API SQLAllocStmt(
    SQLHDBC            hdbc,
    SQLHSTMT FAR      *phstmt)
{
	return _SQLAllocStmt(hdbc,phstmt);
}

SQLRETURN SQL_API SQLBindCol(
    SQLHSTMT           hstmt,
    SQLUSMALLINT       icol,
    SQLSMALLINT        fCType,
    SQLPOINTER         rgbValue,
    SQLINTEGER         cbValueMax,
    SQLINTEGER FAR    *pcbValue)
{
struct _hstmt *stmt = (struct _hstmt *) hstmt;
struct _hdbc *dbc = (struct _hdbc *) stmt->hdbc;
struct _henv *env = (struct _henv *) dbc->henv;

	mdbsql_bind_column(env->sql, icol, rgbValue);

	return SQL_SUCCESS;
}

SQLRETURN SQL_API SQLCancel(
    SQLHSTMT           hstmt)
{
	return SQL_SUCCESS;
}

SQLRETURN SQL_API SQLConnect(
    SQLHDBC            hdbc,
    SQLCHAR FAR       *szDSN,
    SQLSMALLINT        cbDSN,
    SQLCHAR FAR       *szUID,
    SQLSMALLINT        cbUID,
    SQLCHAR FAR       *szAuthStr,
    SQLSMALLINT        cbAuthStr)
{
   SQLCHAR FAR* database = NULL;
   ConnectParams* params;
   SQLRETURN ret;

   strcpy (lastError, "");

   params = ((ODBCConnection*) hdbc)->params;

   if (!LookupDSN (params, szDSN))
   {
      LogError ("Could not find DSN in odbc.ini");
      return SQL_ERROR;
   }
   else if (!(database = GetConnectParam (params, "Database")))
   {
      LogError ("Could not find Database parameter");
      return SQL_ERROR;
   }

   ret = do_connect (hdbc, database);
      return ret;
}

SQLRETURN SQL_API SQLDescribeCol(
    SQLHSTMT           hstmt,
    SQLUSMALLINT       icol,
    SQLCHAR FAR       *szColName,
    SQLSMALLINT        cbColNameMax,
    SQLSMALLINT FAR   *pcbColName,
    SQLSMALLINT FAR   *pfSqlType,
    SQLUINTEGER FAR   *pcbColDef, /* precision */
    SQLSMALLINT FAR   *pibScale,
    SQLSMALLINT FAR   *pfNullable)
{
int cplen, namelen, i;
struct _hstmt *stmt = (struct _hstmt *) hstmt;
struct _hdbc *dbc = (struct _hdbc *) stmt->hdbc;
struct _henv *env = (struct _henv *) dbc->henv;
MdbSQL *sql = env->sql;
MdbSQLColumn *sqlcol;
MdbColumn *col;
MdbTableDef *table;

	if (icol<1 || icol>sql->num_columns) {
		return SQL_ERROR;
	}
     sqlcol = g_ptr_array_index(sql->columns,icol - 1);
	table = sql->cur_table;
     for (i=0;i<table->num_cols;i++) {
          col=g_ptr_array_index(table->columns,i);
          if (!strcasecmp(sqlcol->name, col->name)) {
			break;
          }
     }

	if (szColName) {
		namelen = MIN(cbColNameMax,strlen(sqlcol->name));
		strncpy(szColName, sqlcol->name, namelen);
		szColName[namelen]='\0';
	}
	if (pfSqlType) {
		*pfSqlType = _odbc_get_client_type(col->col_type);
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

	return SQL_SUCCESS;
}

SQLRETURN SQL_API SQLColAttributes(
    SQLHSTMT           hstmt,
    SQLUSMALLINT       icol,
    SQLUSMALLINT       fDescType,
    SQLPOINTER         rgbDesc,
    SQLSMALLINT        cbDescMax,
    SQLSMALLINT FAR   *pcbDesc,
    SQLINTEGER FAR    *pfDesc)
{
int cplen, len = 0;
int namelen, i;
struct _hstmt *stmt;
struct _hdbc *dbc;
struct _henv *env;
MdbSQL *sql;
MdbSQLColumn *sqlcol;
MdbColumn *col;
MdbTableDef *table;

	stmt = (struct _hstmt *) hstmt;
	dbc = (struct _hdbc *) stmt->hdbc;
	env = (struct _henv *) dbc->henv;
	sql = env->sql;

	/* dont check column index for these */
	switch(fDescType) {
		case SQL_COLUMN_COUNT:
			return SQL_SUCCESS;
			break;
	}

	if (icol<1 || icol>sql->num_columns) {
		return SQL_ERROR;
	}

	/* find the column */
	sqlcol = g_ptr_array_index(sql->columns,icol - 1);
	table = sql->cur_table;
	for (i=0;i<table->num_cols;i++) {
		col=g_ptr_array_index(table->columns,i);
		if (!strcasecmp(sqlcol->name, col->name)) {
			break;
          	}
	}

	switch(fDescType) {
		case SQL_COLUMN_NAME:
		case SQL_COLUMN_LABEL:
			namelen = MIN(cbDescMax,strlen(sqlcol->name));
			strncpy(rgbDesc, sqlcol->name, namelen);
			*((char *)&rgbDesc[namelen])='\0';
			break;
		case SQL_COLUMN_TYPE:
			*pcbDesc = SQL_CHAR;
			break;
		case SQL_COLUMN_LENGTH:
			break;
		//case SQL_COLUMN_DISPLAY_SIZE:
		case SQL_DESC_DISPLAY_SIZE:
			switch(_odbc_get_client_type(col->col_type)) {
				case SQL_CHAR:
				case SQL_VARCHAR:
					*pfDesc = col->col_size;	
					break;
				case SQL_INTEGER:
					*pfDesc = 8;
					break;
				case SQL_SMALLINT:
					*pfDesc = 6;
					break;
				case SQL_TINYINT:
					*pfDesc = 4;
					break;
				default:
					//fprintf(stderr,"\nUnknown type %d\n", _odbc_get_client_type(col->col_type));
					break;
			}
			break;
	}
	return SQL_SUCCESS;
}


SQLRETURN SQL_API SQLDisconnect(
    SQLHDBC            hdbc)
{
	return SQL_SUCCESS;
}

SQLRETURN SQL_API SQLError(
    SQLHENV            henv,
    SQLHDBC            hdbc,
    SQLHSTMT           hstmt,
    SQLCHAR FAR       *szSqlState,
    SQLINTEGER FAR    *pfNativeError,
    SQLCHAR FAR       *szErrorMsg,
    SQLSMALLINT        cbErrorMsgMax,
    SQLSMALLINT FAR   *pcbErrorMsg)
{
   SQLRETURN result = SQL_NO_DATA_FOUND;
   
   if (strlen (lastError) > 0)
   {
      strcpy (szSqlState, "08001");
      strcpy (szErrorMsg, lastError);
      if (pcbErrorMsg)
	 *pcbErrorMsg = strlen (lastError);
      if (pfNativeError)
	 *pfNativeError = 1;

      result = SQL_SUCCESS;
      strcpy (lastError, "");
   }

   return result;
}

static SQLRETURN SQL_API _SQLExecute( SQLHSTMT hstmt)
{
struct _hstmt *stmt = (struct _hstmt *) hstmt;
struct _hdbc *dbc = (struct _hdbc *) stmt->hdbc;
struct _henv *env = (struct _henv *) dbc->henv;
int ret;
   
   fprintf(stderr,"query = %s\n",stmt->query);
   _odbc_fix_literals(stmt);

   mdb_sql_reset(env->sql);

   /* calls to yyparse would need to be serialized for thread safety */

   /* begin unsafe */
   g_input_ptr = stmt->query;
   _mdb_sql(env->sql);
   if (yyparse()) {
   /* end unsafe */
        LogError("Couldn't parse SQL\n");
        mdb_sql_reset(env->sql);
        return SQL_ERROR;
   } else {
        return SQL_SUCCESS;
   }
}

SQLRETURN SQL_API SQLExecDirect(
    SQLHSTMT           hstmt,
    SQLCHAR FAR       *szSqlStr,
    SQLINTEGER         cbSqlStr)
{
struct _hstmt *stmt = (struct _hstmt *) hstmt;
int ret;

   strcpy(stmt->query, szSqlStr);

   return _SQLExecute(hstmt);
}

SQLRETURN SQL_API SQLExecute(
    SQLHSTMT           hstmt)
{
   return _SQLExecute(hstmt);
}

SQLRETURN SQL_API SQLFetch(
    SQLHSTMT           hstmt)
{
struct _hstmt *stmt = (struct _hstmt *) hstmt;
struct _hdbc *dbc = (struct _hdbc *) stmt->hdbc;
struct _henv *env = (struct _henv *) dbc->henv;

	if (mdb_fetch_row(env->sql->cur_table)) {
		stmt->row_affected++;
		return SQL_SUCCESS;
	} else {
		return SQL_NO_DATA_FOUND;
	}
}

SQLRETURN SQL_API SQLFreeHandle(    
	SQLSMALLINT HandleType,
    	SQLHANDLE Handle)
{
	switch(HandleType) {
		case SQL_HANDLE_STMT:
			_SQLFreeStmt(Handle,SQL_DROP);
			break;
		case SQL_HANDLE_DBC:
			_SQLFreeConnect(Handle);
			break;
		case SQL_HANDLE_ENV:
			_SQLFreeEnv(Handle);
			break;
	}
   return SQL_SUCCESS;
}

static SQLRETURN SQL_API _SQLFreeConnect(
    SQLHDBC            hdbc)
{
   ODBCConnection* dbc = (ODBCConnection*) hdbc;


   FreeConnectParams (dbc->params);
   free (dbc);

   return SQL_SUCCESS;
}
SQLRETURN SQL_API SQLFreeConnect(
    SQLHDBC            hdbc)
{
	return _SQLFreeConnect(hdbc);
}

static SQLRETURN SQL_API _SQLFreeEnv(
    SQLHENV            henv)
{
	return SQL_SUCCESS;
}
SQLRETURN SQL_API SQLFreeEnv(
    SQLHENV            henv)
{
	return _SQLFreeEnv(henv);
}

static SQLRETURN SQL_API _SQLFreeStmt(
    SQLHSTMT           hstmt,
    SQLUSMALLINT       fOption)
{
struct _hstmt *stmt=(struct _hstmt *)hstmt;

   if (fOption==SQL_DROP) {
   	free (hstmt);
   } else if (fOption==SQL_CLOSE) {
   } else {
   }
   return SQL_SUCCESS;
}
SQLRETURN SQL_API SQLFreeStmt(
    SQLHSTMT           hstmt,
    SQLUSMALLINT       fOption)
{
	return _SQLFreeStmt(hstmt, fOption);
}

SQLRETURN SQL_API SQLGetStmtAttr (
    SQLHSTMT StatementHandle,
    SQLINTEGER Attribute,
    SQLPOINTER Value,
    SQLINTEGER BufferLength,
    SQLINTEGER * StringLength)
{
   return SQL_SUCCESS;
}

SQLRETURN SQL_API SQLGetCursorName(
    SQLHSTMT           hstmt,
    SQLCHAR FAR       *szCursor,
    SQLSMALLINT        cbCursorMax,
    SQLSMALLINT FAR   *pcbCursor)
{
	return SQL_SUCCESS;
}

SQLRETURN SQL_API SQLNumResultCols(
    SQLHSTMT           hstmt,
    SQLSMALLINT FAR   *pccol)
{
struct _hstmt *stmt = (struct _hstmt *) hstmt;
struct _hdbc *dbc = (struct _hdbc *) stmt->hdbc;
struct _henv *env = (struct _henv *) dbc->henv;
	
	*pccol = env->sql->num_columns;
	return SQL_SUCCESS;
}

SQLRETURN SQL_API SQLPrepare(
    SQLHSTMT           hstmt,
    SQLCHAR FAR       *szSqlStr,
    SQLINTEGER         cbSqlStr)
{
struct _hstmt *stmt=(struct _hstmt *)hstmt;

   if (cbSqlStr!=SQL_NTS) {
	strncpy(stmt->query, szSqlStr, cbSqlStr);
	stmt->query[cbSqlStr]='\0';
   } else {
   	strcpy(stmt->query, szSqlStr);
   }

   return SQL_SUCCESS;
}

SQLRETURN SQL_API SQLRowCount(
    SQLHSTMT           hstmt,
    SQLINTEGER FAR    *pcrow)
{
struct _hstmt *stmt=(struct _hstmt *)hstmt;

	*pcrow = stmt->row_affected;
	return SQL_SUCCESS;
}

SQLRETURN SQL_API SQLSetCursorName(
    SQLHSTMT           hstmt,
    SQLCHAR FAR       *szCursor,
    SQLSMALLINT        cbCursor)
{
	return SQL_SUCCESS;
}

SQLRETURN SQL_API SQLTransact(
    SQLHENV            henv,
    SQLHDBC            hdbc,
    SQLUSMALLINT       fType)
{
	return SQL_SUCCESS;
}


SQLRETURN SQL_API SQLSetParam(            /*      Use SQLBindParameter */
    SQLHSTMT           hstmt,
    SQLUSMALLINT       ipar,
    SQLSMALLINT        fCType,
    SQLSMALLINT        fSqlType,
    SQLUINTEGER        cbParamDef,
    SQLSMALLINT        ibScale,
    SQLPOINTER         rgbValue,
    SQLINTEGER FAR     *pcbValue)
{
	return SQL_SUCCESS;
}

SQLRETURN SQL_API SQLColumns(
    SQLHSTMT           hstmt,
    SQLCHAR FAR       *szCatalogName,
    SQLSMALLINT        cbCatalogName,
    SQLCHAR FAR       *szSchemaName,
    SQLSMALLINT        cbSchemaName,
    SQLCHAR FAR       *szTableName,
    SQLSMALLINT        cbTableName,
    SQLCHAR FAR       *szColumnName,
    SQLSMALLINT        cbColumnName)
{
	return SQL_SUCCESS;
}

SQLRETURN SQL_API SQLGetConnectOption(
    SQLHDBC            hdbc,
    SQLUSMALLINT       fOption,
    SQLPOINTER         pvParam)
{
	return SQL_SUCCESS;
}

SQLRETURN SQL_API SQLGetData(
    SQLHSTMT           hstmt,
    SQLUSMALLINT       icol,
    SQLSMALLINT        fCType,
    SQLPOINTER         rgbValue,
    SQLINTEGER         cbValueMax,
    SQLINTEGER FAR    *pcbValue)
{
struct _hstmt *stmt;
struct _hdbc *dbc;
struct _henv *env;
unsigned char *src;
int srclen;
MdbSQL *sql;
MdbHandle *mdb;
MdbSQLColumn *sqlcol;
MdbColumn *col;
MdbTableDef *table;
int i;

	stmt = (struct _hstmt *) hstmt;
	dbc = (struct _hdbc *) stmt->hdbc;
	env = (struct _henv *) dbc->henv;
	sql = env->sql;
	mdb = sql->mdb;

	if (icol<1 || icol>sql->num_columns) {
		return SQL_ERROR;
	}

	sqlcol = g_ptr_array_index(sql->columns,icol - 1);
	table = sql->cur_table;
	for (i=0;i<table->num_cols;i++) {
		col=g_ptr_array_index(table->columns,i);
		if (!strcasecmp(sqlcol->name, col->name)) {
			break;
		}
	}
	
	strcpy(rgbValue,
		mdb_col_to_string(mdb, col->cur_value_start, col->col_type, 
		col->cur_value_len));
	//*((char *)&rgbValue[col->cur_value_len])='\0';
	*pcbValue = col->cur_value_len;
	return 0;
}

SQLRETURN SQL_API SQLGetFunctions(
    SQLHDBC            hdbc,
    SQLUSMALLINT       fFunction,
    SQLUSMALLINT FAR  *pfExists)
{
	return SQL_SUCCESS;
}

SQLRETURN SQL_API SQLGetInfo(
    SQLHDBC            hdbc,
    SQLUSMALLINT       fInfoType,
    SQLPOINTER         rgbInfoValue,
    SQLSMALLINT        cbInfoValueMax,
    SQLSMALLINT FAR   *pcbInfoValue)
{
	return SQL_SUCCESS;
}

SQLRETURN SQL_API SQLGetStmtOption(
    SQLHSTMT           hstmt,
    SQLUSMALLINT       fOption,
    SQLPOINTER         pvParam)
{
	return SQL_SUCCESS;
}

SQLRETURN SQL_API SQLGetTypeInfo(
    SQLHSTMT           hstmt,
    SQLSMALLINT        fSqlType)
{
struct _hstmt *stmt;

	stmt = (struct _hstmt *) hstmt;
	if (!fSqlType) {
   		strcpy(stmt->query, "SELECT * FROM tds_typeinfo");
	} else {
			sprintf(stmt->query, "SELECT * FROM tds_typeinfo WHERE SQL_DATA_TYPE = %d", fSqlType);
		}
		
		return _SQLExecute(hstmt);
	}

	SQLRETURN SQL_API SQLParamData(
	    SQLHSTMT           hstmt,
	    SQLPOINTER FAR    *prgbValue)
	{
		return SQL_SUCCESS;
	}

	SQLRETURN SQL_API SQLPutData(
	    SQLHSTMT           hstmt,
	    SQLPOINTER         rgbValue,
	    SQLINTEGER         cbValue)
	{
		return SQL_SUCCESS;
	}

	SQLRETURN SQL_API SQLSetConnectOption(
	    SQLHDBC            hdbc,
	    SQLUSMALLINT       fOption,
	    SQLUINTEGER        vParam)
	{
		return SQL_SUCCESS;
	}

	SQLRETURN SQL_API SQLSetStmtOption(
	    SQLHSTMT           hstmt,
	    SQLUSMALLINT       fOption,
	    SQLUINTEGER        vParam)
	{
		return SQL_SUCCESS;
	}

	SQLRETURN SQL_API SQLSpecialColumns(
	    SQLHSTMT           hstmt,
	    SQLUSMALLINT       fColType,
	    SQLCHAR FAR       *szCatalogName,
	    SQLSMALLINT        cbCatalogName,
	    SQLCHAR FAR       *szSchemaName,
	    SQLSMALLINT        cbSchemaName,
	    SQLCHAR FAR       *szTableName,
	    SQLSMALLINT        cbTableName,
	    SQLUSMALLINT       fScope,
	    SQLUSMALLINT       fNullable)
	{
		return SQL_SUCCESS;
	}

	SQLRETURN SQL_API SQLStatistics(
	    SQLHSTMT           hstmt,
	    SQLCHAR FAR       *szCatalogName,
	    SQLSMALLINT        cbCatalogName,
	    SQLCHAR FAR       *szSchemaName,
	    SQLSMALLINT        cbSchemaName,
	    SQLCHAR FAR       *szTableName,
	    SQLSMALLINT        cbTableName,
	    SQLUSMALLINT       fUnique,
	    SQLUSMALLINT       fAccuracy)
	{
		return SQL_SUCCESS;
	}

	SQLRETURN SQL_API SQLTables(
	    SQLHSTMT           hstmt,
	    SQLCHAR FAR       *szCatalogName,
	    SQLSMALLINT        cbCatalogName,
	    SQLCHAR FAR       *szSchemaName,
	    SQLSMALLINT        cbSchemaName,
	    SQLCHAR FAR       *szTableName,
	    SQLSMALLINT        cbTableName,
	    SQLCHAR FAR       *szTableType,
	    SQLSMALLINT        cbTableType)
	{
	char *query, *p;
	char *sptables = "exec sp_tables ";
	int querylen, clen, slen, tlen, ttlen;
	int first = 1;
	struct _hstmt *stmt;

		stmt = (struct _hstmt *) hstmt;

		clen = _odbc_get_string_size(cbCatalogName, szCatalogName);
		slen = _odbc_get_string_size(cbSchemaName, szSchemaName);
		tlen = _odbc_get_string_size(cbTableName, szTableName);
		ttlen = _odbc_get_string_size(cbTableType, szTableType);

		querylen = strlen(sptables) + clen + slen + tlen + ttlen + 40; /* a little padding for quotes and commas */
		query = (char *) malloc(querylen);
		p = query;

		strcpy(p, sptables);
		p += strlen(sptables);

		if (tlen) {
			*p++ = '"';
			strncpy(p, szTableName, tlen); *p+=tlen;
			*p++ = '"';
			first = 0;
		}
		if (slen) {
			if (!first) *p++ = ',';
			*p++ = '"';
			strncpy(p, szSchemaName, slen); *p+=slen;
			*p++ = '"';
			first = 0;
		}
		if (clen) {
			if (!first) *p++ = ',';
			*p++ = '"';
			strncpy(p, szCatalogName, clen); *p+=clen;
			*p++ = '"';
			first = 0;
		}
		if (ttlen) {
			if (!first) *p++ = ',';
			*p++ = '"';
			strncpy(p, szTableType, ttlen); *p+=ttlen;
			*p++ = '"';
			first = 0;
		}
		*p++ = '\0';
		// fprintf(stderr,"\nquery = %s\n",query);

	strcpy(stmt->query, query);
	return _SQLExecute(hstmt);
}


SQLRETURN SQL_API SQLDataSources(
    SQLHENV            henv,
    SQLUSMALLINT       fDirection,
    SQLCHAR FAR       *szDSN,
    SQLSMALLINT        cbDSNMax,
    SQLSMALLINT FAR   *pcbDSN,
    SQLCHAR FAR       *szDescription,
    SQLSMALLINT        cbDescriptionMax,
    SQLSMALLINT FAR   *pcbDescription)
{
	return SQL_SUCCESS;
}

static int _odbc_fix_literals(struct _hstmt *stmt)
{
char tmp[4096],begin_tag[11];
char *s, *d, *p;
int i, quoted = 0, find_end = 0;
char quote_char;

        s=stmt->query;
        d=tmp;
        while (*s) {
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
				strncpy(begin_tag, s, i);
				begin_tag[i] = '\0';
				/* printf("begin tag %s\n", begin_tag); */
				s += i;
				find_end = 1;
			}
		} else {
			*d++=*s++;	
		}
        }
	*d='\0';
	strcpy(stmt->query,tmp);
}

static int _odbc_get_string_size(int size, char *str)
{
	if (!str) {
		return 0;
	}
	if (size==SQL_NTS) {
		return strlen(str);
	} else {
		return size;
	}
}
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
	}
}
static SQLSMALLINT _odbc_get_client_type(int srv_type)
{
	switch (srv_type) {
		case MDB_BOOL:
			return SQL_BIT;
			break;
		case MDB_BYTE:
			return SQL_TINYINT;
			break;
		case MDB_INT:
			return SQL_SMALLINT;
			break;
		case MDB_LONGINT:
			return SQL_INTEGER;
			break;
		case MDB_FLOAT:
			return SQL_FLOAT;
			break;
		case MDB_DOUBLE:
			return SQL_DOUBLE;
			break;
		case MDB_TEXT:
			return SQL_VARCHAR;
			break;
		default:
			// fprintf(stderr,"Unknown type %d\n",srv_type);
			break;
	}
}

