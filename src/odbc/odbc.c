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

static char  software_version[]   = "$Id: odbc.c,v 1.15 2004/03/13 15:07:19 brianb Exp $";
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

static void bind_columns (struct _hstmt*);

#ifndef MIN
#define MIN(a,b) (a>b ? b : a)
#endif
#define _MAX_ERROR_LEN 255
static char lastError[_MAX_ERROR_LEN+1];

//#define TRACE(x) fprintf(stderr,"Function %s\n", x);
#define TRACE(x)

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
	{"text", SQL_VARCHAR, 255, NULL, NULL, NULL, SQL_TRUE, SQL_TRUE, SQL_TRUE, NULL, SQL_FALSE, SQL_FALSE, NULL, 0, 255, SQL_VARCHAR, NULL, NULL, NULL},
	{"memo", SQL_VARCHAR, 4096, NULL, NULL, NULL, SQL_TRUE, SQL_TRUE, SQL_TRUE, NULL, SQL_FALSE, SQL_FALSE, NULL, 0, 4096, SQL_VARCHAR, NULL, NULL, NULL},
	{"text", SQL_CHAR, 255, NULL, NULL, NULL, SQL_TRUE, SQL_TRUE, SQL_TRUE, NULL, SQL_FALSE, SQL_FALSE, NULL, 0, 255, SQL_CHAR, NULL, NULL, NULL},
	{"numeric", SQL_NUMERIC, 255, NULL, NULL, NULL, SQL_TRUE, SQL_TRUE, SQL_TRUE, NULL, SQL_FALSE, SQL_FALSE, NULL, 0, 255, SQL_NUMERIC, NULL, NULL, NULL},
	{"numeric", SQL_DECIMAL, 255, NULL, NULL, NULL, SQL_TRUE, SQL_TRUE, SQL_TRUE, NULL, SQL_FALSE, SQL_FALSE, NULL, 0, 255, SQL_DECIMAL, NULL, NULL, NULL},
	{"long integer", SQL_INTEGER, 4, NULL, NULL, NULL, SQL_TRUE, SQL_TRUE, SQL_TRUE, NULL, SQL_FALSE, SQL_TRUE, NULL, 0, 4, SQL_INTEGER, NULL, NULL, NULL},
	{"integer", SQL_SMALLINT, 4, NULL, NULL, NULL, SQL_TRUE, SQL_TRUE, SQL_TRUE, NULL, SQL_FALSE, SQL_TRUE, NULL, 0, 4, SQL_SMALLINT, NULL, NULL, NULL},
	{"integer", SQL_SMALLINT, 4, NULL, NULL, NULL, SQL_TRUE, SQL_TRUE, SQL_TRUE, NULL, SQL_FALSE, SQL_TRUE, NULL, 0, 4, SQL_SMALLINT, NULL, NULL, NULL},
	{"single", SQL_REAL, 4, NULL, NULL, NULL, SQL_TRUE, SQL_TRUE, SQL_TRUE, NULL, SQL_FALSE, SQL_TRUE, NULL, 0, 4, SQL_REAL, NULL, NULL, NULL},
	{"double", SQL_DOUBLE, 8, NULL, NULL, NULL, SQL_TRUE, SQL_TRUE, SQL_TRUE, NULL, SQL_FALSE, SQL_TRUE, NULL, 0, 8, SQL_FLOAT, NULL, NULL, NULL},
	{"datetime", SQL_DATETIME, 8, NULL, NULL, NULL, SQL_TRUE, SQL_TRUE, SQL_TRUE, NULL, SQL_FALSE, SQL_TRUE, NULL, 0, 8, SQL_DATETIME, NULL, NULL, NULL}
};

#define NUM_TYPE_INFO_COLS 19
#define MAX_TYPE_INFO 11

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

	TRACE("DriverConnect");

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
	TRACE("BrowseConnect");
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
	TRACE("SQLColumnPrivileges");
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
	TRACE("SQLDescribeParam");
	return SQL_SUCCESS;
}

SQLRETURN SQL_API SQLExtendedFetch(
    SQLHSTMT           hstmt,
    SQLUSMALLINT       fFetchType,
    SQLINTEGER         irow,
    SQLUINTEGER FAR   *pcrow,
    SQLUSMALLINT FAR  *rgfRowStatus)
{
struct _hstmt *stmt = (struct _hstmt *) hstmt;
struct _hdbc *dbc = (struct _hdbc *) stmt->hdbc;
struct _henv *env = (struct _henv *) dbc->henv;

	TRACE("SQLExtendedFetch");
	if (fFetchType!=SQL_FETCH_NEXT) {
		LogError("Fetch type not supported in SQLExtendedFetch");
		return SQL_ERROR;
	}
	if (pcrow)
		*pcrow=1;
	if (rgfRowStatus)
		*rgfRowStatus = SQL_SUCCESS; /* what is the row status value? */
	
	bind_columns(stmt);

	if (mdb_fetch_row(env->sql->cur_table)) {
		stmt->rows_affected++;
		return SQL_SUCCESS;
	} else {
		return SQL_NO_DATA_FOUND;
	}
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
	TRACE("SQLForeignKeys");
	return SQL_SUCCESS;
}

SQLRETURN SQL_API SQLMoreResults(
    SQLHSTMT           hstmt)
{
	TRACE("SQLMoreResults");
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
	TRACE("SQLNativeSql");
	return SQL_SUCCESS;
}

SQLRETURN SQL_API SQLNumParams(
    SQLHSTMT           hstmt,
    SQLSMALLINT FAR   *pcpar)
{
	TRACE("SQLNumParams");
	return SQL_SUCCESS;
}

SQLRETURN SQL_API SQLParamOptions(
    SQLHSTMT           hstmt,
    SQLUINTEGER        crow,
    SQLUINTEGER FAR   *pirow)
{
	TRACE("SQLParamOptions");
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
	TRACE("SQLPrimaryKeys");
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
	TRACE("SQLProcedureColumns");
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
	TRACE("SQLProcedures");
	return SQL_SUCCESS;
}

SQLRETURN SQL_API SQLSetPos(
    SQLHSTMT           hstmt,
    SQLUSMALLINT       irow,
    SQLUSMALLINT       fOption,
    SQLUSMALLINT       fLock)
{
	TRACE("SQLSetPos");
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
	TRACE("SQLTablePrivileges");
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
    SQLUINTEGER        cbColDef,
    SQLSMALLINT        ibScale,
    SQLPOINTER         rgbValue,
    SQLINTEGER         cbValueMax,
    SQLINTEGER FAR    *pcbValue)
{
struct _hstmt *stmt;

	TRACE("SQLBindParameter");
	stmt = (struct _hstmt *) hstmt;
	return SQL_SUCCESS;
}

SQLRETURN SQL_API SQLAllocHandle(    
	SQLSMALLINT HandleType,
    	SQLHANDLE InputHandle,
    	SQLHANDLE * OutputHandle)
{
	TRACE("SQLAllocHandle");
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
	return SQL_ERROR;
}
static SQLRETURN SQL_API _SQLAllocConnect(
    SQLHENV            henv,
    SQLHDBC FAR       *phdbc)
{
struct _henv *env;
ODBCConnection* dbc;

	TRACE("_SQLAllocConnect");
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
	TRACE("SQLAllocConnect");
	return _SQLAllocConnect(henv, phdbc);
}

static SQLRETURN SQL_API _SQLAllocEnv(
    SQLHENV FAR       *phenv)
{
struct _henv *env;

	TRACE("_SQLAllocEnv");
     env = (SQLHENV) g_malloc(sizeof(struct _henv));
     memset(env,'\0',sizeof(struct _henv));
	*phenv=env;
	env->sql = mdb_sql_init();
	return SQL_SUCCESS;
}
SQLRETURN SQL_API SQLAllocEnv(
    SQLHENV FAR       *phenv)
{
	TRACE("SQLAllocEnv");
	return _SQLAllocEnv(phenv);
}

static SQLRETURN SQL_API _SQLAllocStmt(
    SQLHDBC            hdbc,
    SQLHSTMT FAR      *phstmt)
{
struct _hdbc *dbc;
struct _hstmt *stmt;

	TRACE("_SQLAllocStmt");
	dbc = (struct _hdbc *) hdbc;

     stmt = (SQLHSTMT) g_malloc(sizeof(struct _hstmt));
     memset(stmt,'\0',sizeof(struct _hstmt));
	stmt->hdbc=hdbc;
	*phstmt = stmt;

	return SQL_SUCCESS;
}
SQLRETURN SQL_API SQLAllocStmt(
    SQLHDBC            hdbc,
    SQLHSTMT FAR      *phstmt)
{
	TRACE("SQLAllocStmt");
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
	struct _sql_bind_info *cur, *prev, *newitem;

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
   		cur->column_bindlen = cbValueMax;
   		cur->varaddr = (char *) rgbValue;
	} else {
		/* didn't find it create a new one */
		newitem = (struct _sql_bind_info *) malloc(sizeof(struct _sql_bind_info));
		memset(newitem, 0, sizeof(struct _sql_bind_info));
		newitem->column_number = icol;
		newitem->column_bindtype = fCType;
   		newitem->column_bindlen = cbValueMax;
   		newitem->column_lenbind = (int *)pcbValue;
   		newitem->varaddr = (char *) rgbValue;
		/* if there's no head yet */
		if (! stmt->bind_head) {
			stmt->bind_head = newitem;
		} else {
			/* find the tail of the list */
			cur = stmt->bind_head;
			while (cur) {
				prev = cur;
				cur = cur->next;
			}
			prev->next = newitem;
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

	TRACE("SQLConnect");
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
	int namelen, i;
	struct _hstmt *stmt = (struct _hstmt *) hstmt;
	struct _hdbc *dbc = (struct _hdbc *) stmt->hdbc;
	struct _henv *env = (struct _henv *) dbc->henv;
	MdbSQL *sql = env->sql;
	MdbSQLColumn *sqlcol;
	MdbColumn *col;
	MdbTableDef *table;

	TRACE("SQLDescribeCol");
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
	int namelen, i;
	struct _hstmt *stmt;
	struct _hdbc *dbc;
	struct _henv *env;
	MdbSQL *sql;
	MdbSQLColumn *sqlcol;
	MdbColumn *col;
	MdbTableDef *table;

	TRACE("SQLColAttributes");
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

	// fprintf(stderr,"fDescType = %d\n", fDescType);
	switch(fDescType) {
		case SQL_COLUMN_NAME:
		case SQL_COLUMN_LABEL:
			namelen = MIN(cbDescMax,strlen(sqlcol->name));
			strncpy(rgbDesc, sqlcol->name, namelen);
			((char *)rgbDesc)[namelen]='\0';
			break;
		case SQL_COLUMN_TYPE:
			*pfDesc = SQL_CHAR;
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
	TRACE("SQLDisconnect");
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
   
	TRACE("SQLError");
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

	TRACE("_SQLExecute");
   
   /* fprintf(stderr,"query = %s\n",stmt->query); */
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

	TRACE("SQLExecDirect");
	strcpy(stmt->query, szSqlStr);

	return _SQLExecute(hstmt);
}

SQLRETURN SQL_API SQLExecute(
    SQLHSTMT           hstmt)
{
	TRACE("SQLExecute");
   return _SQLExecute(hstmt);
}
static void
bind_columns(struct _hstmt *stmt)
{
struct _hdbc *dbc = (struct _hdbc *) stmt->hdbc;
struct _henv *env = (struct _henv *) dbc->henv;
struct _sql_bind_info *cur;

    if (stmt->rows_affected==0) {
        cur = stmt->bind_head;
        while (cur) {
            if (cur->column_number>0 &&
            cur->column_number <= env->sql->num_columns) {
				mdb_sql_bind_column(env->sql, 
					cur->column_number, cur->varaddr);
				if (cur->column_lenbind)
					mdb_sql_bind_len(env->sql, 
						cur->column_number, cur->column_lenbind);
            } else {
                /* log error ? */
            }
            cur = cur->next;
        }
    }
}
SQLRETURN SQL_API SQLFetch(
    SQLHSTMT           hstmt)
{
struct _hstmt *stmt = (struct _hstmt *) hstmt;
struct _hdbc *dbc = (struct _hdbc *) stmt->hdbc;
struct _henv *env = (struct _henv *) dbc->henv;

	TRACE("SQLFetch");
    /* if we bound columns, transfer them to res_info now that we have one */
	bind_columns(stmt);
    //cur = stmt->bind_head;
    //while (cur) {
    	//if (cur->column_number>0 &&
            //cur->column_number <= env->sql->num_columns) {
			// if (cur->column_lenbind) *(cur->column_lenbind) = 4;
		//}
        //cur = cur->next;
	//}

	if (mdb_sql_fetch_row(env->sql, env->sql->cur_table)) {
		stmt->rows_affected++;
		return SQL_SUCCESS;
	} else {
		return SQL_NO_DATA_FOUND;
	}
}

SQLRETURN SQL_API SQLFreeHandle(    
	SQLSMALLINT HandleType,
    	SQLHANDLE Handle)
{
	TRACE("SQLFreeHandle");
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

	TRACE("_SQLFreeConnect");

   FreeConnectParams(dbc->params);
   g_free(dbc);

   return SQL_SUCCESS;
}
SQLRETURN SQL_API SQLFreeConnect(
    SQLHDBC            hdbc)
{
	TRACE("SQLFreeConnect");
	return _SQLFreeConnect(hdbc);
}

static SQLRETURN SQL_API _SQLFreeEnv(
    SQLHENV            henv)
{
	TRACE("_SQLFreeEnv");
	return SQL_SUCCESS;
}
SQLRETURN SQL_API SQLFreeEnv(
    SQLHENV            henv)
{
	TRACE("SQLFreeEnv");
	return _SQLFreeEnv(henv);
}

static SQLRETURN SQL_API _SQLFreeStmt(
    SQLHSTMT           hstmt,
    SQLUSMALLINT       fOption)
{
	struct _hstmt *stmt=(struct _hstmt *)hstmt;

	TRACE("_SQLFreeStmt");
	if (fOption==SQL_DROP) {
		g_free(stmt);
	} else if (fOption==SQL_CLOSE) {
	} else {
	}
	return SQL_SUCCESS;
}
SQLRETURN SQL_API SQLFreeStmt(
    SQLHSTMT           hstmt,
    SQLUSMALLINT       fOption)
{
	TRACE("SQLFreeStmt");
	return _SQLFreeStmt(hstmt, fOption);
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
    SQLCHAR FAR       *szCursor,
    SQLSMALLINT        cbCursorMax,
    SQLSMALLINT FAR   *pcbCursor)
{
	TRACE("SQLGetCursorName");
	return SQL_SUCCESS;
}

SQLRETURN SQL_API SQLNumResultCols(
    SQLHSTMT           hstmt,
    SQLSMALLINT FAR   *pccol)
{
struct _hstmt *stmt = (struct _hstmt *) hstmt;
struct _hdbc *dbc = (struct _hdbc *) stmt->hdbc;
struct _henv *env = (struct _henv *) dbc->henv;
	
	TRACE("SQLNumResultCols");
	*pccol = env->sql->num_columns;
	return SQL_SUCCESS;
}

SQLRETURN SQL_API SQLPrepare(
    SQLHSTMT           hstmt,
    SQLCHAR FAR       *szSqlStr,
    SQLINTEGER         cbSqlStr)
{
struct _hstmt *stmt=(struct _hstmt *)hstmt;

	TRACE("SQLPrepare");
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

	TRACE("SQLRowCount");
	*pcrow = stmt->rows_affected;
	return SQL_SUCCESS;
}

SQLRETURN SQL_API SQLSetCursorName(
    SQLHSTMT           hstmt,
    SQLCHAR FAR       *szCursor,
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
    SQLUINTEGER        cbParamDef,
    SQLSMALLINT        ibScale,
    SQLPOINTER         rgbValue,
    SQLINTEGER FAR     *pcbValue)
{
	TRACE("SQLSetParam");
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
	TRACE("SQLColumns");
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
    SQLINTEGER         cbValueMax,
    SQLINTEGER FAR    *pcbValue)
{
	struct _hstmt *stmt;
	struct _hdbc *dbc;
	struct _henv *env;
	MdbSQL *sql;
	MdbHandle *mdb;
	MdbSQLColumn *sqlcol;
	MdbColumn *col;
	MdbTableDef *table;
	int i;

	TRACE("SQLGetData");
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
		mdb_col_to_string(mdb, mdb->pg_buf, col->cur_value_start, col->col_type, 
		col->cur_value_len));
	//*((char *)&rgbValue[col->cur_value_len])='\0';
	*pcbValue = col->cur_value_len;
	return 0;
}

static void _set_func_exists(SQLUSMALLINT FAR *pfExists, SQLUSMALLINT fFunction)
{
SQLUSMALLINT FAR *mod;

	mod = pfExists + (fFunction >> 4);
	*mod |= (1 << (fFunction & 0x0f));
}
SQLRETURN SQL_API SQLGetFunctions(
    SQLHDBC            hdbc,
    SQLUSMALLINT       fFunction,
    SQLUSMALLINT FAR  *pfExists)
{

	TRACE("SQLGetFunctions");
	switch (fFunction) {
#if ODBCVER >= 0x0300
		case SQL_API_ODBC3_ALL_FUNCTIONS:
 
/*			for (i=0;i<SQL_API_ODBC3_ALL_FUNCTIONS_SIZE;i++) {
				pfExists[i] = 0xFFFF;
			}
*/
			_set_func_exists(pfExists,SQL_API_SQLALLOCCONNECT);
			_set_func_exists(pfExists,SQL_API_SQLALLOCENV);
			_set_func_exists(pfExists,SQL_API_SQLALLOCHANDLE);
			_set_func_exists(pfExists,SQL_API_SQLALLOCSTMT);
			_set_func_exists(pfExists,SQL_API_SQLBINDCOL);
			_set_func_exists(pfExists,SQL_API_SQLBINDPARAMETER);
			_set_func_exists(pfExists,SQL_API_SQLCANCEL);
			_set_func_exists(pfExists,SQL_API_SQLCLOSECURSOR);
			_set_func_exists(pfExists,SQL_API_SQLCOLATTRIBUTE);
			_set_func_exists(pfExists,SQL_API_SQLCOLUMNS);
			_set_func_exists(pfExists,SQL_API_SQLCONNECT);
			_set_func_exists(pfExists,SQL_API_SQLCOPYDESC);
			_set_func_exists(pfExists,SQL_API_SQLDATASOURCES);
			_set_func_exists(pfExists,SQL_API_SQLDESCRIBECOL);
			_set_func_exists(pfExists,SQL_API_SQLDISCONNECT);
			_set_func_exists(pfExists,SQL_API_SQLENDTRAN);
			_set_func_exists(pfExists,SQL_API_SQLERROR);
			_set_func_exists(pfExists,SQL_API_SQLEXECDIRECT);
			_set_func_exists(pfExists,SQL_API_SQLEXECUTE);
			_set_func_exists(pfExists,SQL_API_SQLFETCH);
			_set_func_exists(pfExists,SQL_API_SQLFETCHSCROLL);
			_set_func_exists(pfExists,SQL_API_SQLFREECONNECT);
			_set_func_exists(pfExists,SQL_API_SQLFREEENV);
			_set_func_exists(pfExists,SQL_API_SQLFREEHANDLE);
			_set_func_exists(pfExists,SQL_API_SQLFREESTMT);
			_set_func_exists(pfExists,SQL_API_SQLGETCONNECTATTR);
			_set_func_exists(pfExists,SQL_API_SQLGETCONNECTOPTION);
			_set_func_exists(pfExists,SQL_API_SQLGETCURSORNAME);
			_set_func_exists(pfExists,SQL_API_SQLGETDATA);
			_set_func_exists(pfExists,SQL_API_SQLGETDESCFIELD);
			_set_func_exists(pfExists,SQL_API_SQLGETDESCREC);
			_set_func_exists(pfExists,SQL_API_SQLGETDIAGFIELD);
			_set_func_exists(pfExists,SQL_API_SQLGETDIAGREC);
			_set_func_exists(pfExists,SQL_API_SQLGETENVATTR);
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
			_set_func_exists(pfExists,SQL_API_SQLSETCONNECTATTR);
			_set_func_exists(pfExists,SQL_API_SQLSETCONNECTOPTION);
			_set_func_exists(pfExists,SQL_API_SQLSETCURSORNAME);
			_set_func_exists(pfExists,SQL_API_SQLSETDESCFIELD);
			_set_func_exists(pfExists,SQL_API_SQLSETDESCREC);
			_set_func_exists(pfExists,SQL_API_SQLSETENVATTR);
			_set_func_exists(pfExists,SQL_API_SQLSETPARAM);
			_set_func_exists(pfExists,SQL_API_SQLSETSTMTATTR);
			_set_func_exists(pfExists,SQL_API_SQLSETSTMTOPTION);
			_set_func_exists(pfExists,SQL_API_SQLSPECIALCOLUMNS);
			_set_func_exists(pfExists,SQL_API_SQLSTATISTICS);
			_set_func_exists(pfExists,SQL_API_SQLTABLES);
			_set_func_exists(pfExists,SQL_API_SQLTRANSACT);

			return SQL_SUCCESS;
			break;
#endif
		case SQL_API_ALL_FUNCTIONS:
			_set_func_exists(pfExists,SQL_API_SQLALLOCCONNECT);
			_set_func_exists(pfExists,SQL_API_SQLALLOCENV);
			_set_func_exists(pfExists,SQL_API_SQLALLOCSTMT);
			_set_func_exists(pfExists,SQL_API_SQLBINDCOL);
			_set_func_exists(pfExists,SQL_API_SQLCANCEL);
			_set_func_exists(pfExists,SQL_API_SQLCOLATTRIBUTES);
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
			_set_func_exists(pfExists,SQL_API_SQLFREESTMT);
			_set_func_exists(pfExists,SQL_API_SQLGETCONNECTOPTION);
			_set_func_exists(pfExists,SQL_API_SQLGETCURSORNAME);
			_set_func_exists(pfExists,SQL_API_SQLGETDATA);
			_set_func_exists(pfExists,SQL_API_SQLGETFUNCTIONS);
			_set_func_exists(pfExists,SQL_API_SQLGETINFO);
			_set_func_exists(pfExists,SQL_API_SQLGETSTMTOPTION);
			_set_func_exists(pfExists,SQL_API_SQLGETTYPEINFO);
			_set_func_exists(pfExists,SQL_API_SQLNUMRESULTCOLS);
			_set_func_exists(pfExists,SQL_API_SQLPARAMDATA);
			_set_func_exists(pfExists,SQL_API_SQLPREPARE);
			_set_func_exists(pfExists,SQL_API_SQLPUTDATA);
			_set_func_exists(pfExists,SQL_API_SQLROWCOUNT);
			_set_func_exists(pfExists,SQL_API_SQLSETCONNECTOPTION);
			_set_func_exists(pfExists,SQL_API_SQLSETCURSORNAME);
			_set_func_exists(pfExists,SQL_API_SQLSETPARAM);
			_set_func_exists(pfExists,SQL_API_SQLSETSTMTOPTION);
			_set_func_exists(pfExists,SQL_API_SQLSPECIALCOLUMNS);
			_set_func_exists(pfExists,SQL_API_SQLSTATISTICS);
			_set_func_exists(pfExists,SQL_API_SQLTABLES);
			_set_func_exists(pfExists,SQL_API_SQLTRANSACT);
			return SQL_SUCCESS;
			break;
		default:
			*pfExists = 1; /* SQL_TRUE */
			return SQL_SUCCESS;
			break;
	}
	return SQL_SUCCESS;
}

SQLRETURN SQL_API SQLGetInfo(
    SQLHDBC            hdbc,
    SQLUSMALLINT       fInfoType,
    SQLPOINTER         rgbInfoValue,
    SQLSMALLINT        cbInfoValueMax,
    SQLSMALLINT FAR   *pcbInfoValue)
{
	TRACE("SQLGetInfo");
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
	struct _hdbc *dbc = (struct _hdbc *) stmt->hdbc;
	struct _henv *env = (struct _henv *) dbc->henv;
	MdbTableDef *ttable;
	MdbSQL *sql = env->sql;
	MdbHandle *mdb = sql->mdb;
	unsigned char *new_pg;
	int row_size;
	unsigned char row_buffer[MDB_PGSIZE];
	unsigned char tmpstr[MDB_BIND_SIZE];
	int i, tmpsiz;
	MdbField fields[NUM_TYPE_INFO_COLS];

	TRACE("SQLGetStmtOption");
	stmt = (struct _hstmt *) hstmt;
	
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

	/* blank out the pg_buf */
	new_pg = mdb_new_data_pg(ttable->entry);
	memcpy(mdb->pg_buf, new_pg, mdb->fmt->pg_size);
	g_free(new_pg);

	for (i=0; i<MAX_TYPE_INFO; i++) {
		if (!fSqlType || fSqlType == type_info[i].data_type) {
			tmpsiz = mdb_ascii2unicode(mdb, type_info[i].type_name, 0, 100, tmpstr);
			mdb_fill_temp_field(&fields[0],tmpstr, tmpsiz, 0,0,0,0);
			mdb_fill_temp_field(&fields[1],&type_info[i].data_type, sizeof(SQLSMALLINT), 0,0,0,1);
			mdb_fill_temp_field(&fields[2],&type_info[i].column_size, sizeof(SQLINTEGER), 0,0,0,1);
			tmpsiz = mdb_ascii2unicode(mdb, type_info[i].literal_prefix, 0, 100, tmpstr);
			mdb_fill_temp_field(&fields[3], tmpstr, tmpsiz, 0,0,0,0);
			tmpsiz = mdb_ascii2unicode(mdb, type_info[i].literal_suffix, 0, 100, tmpstr);
			mdb_fill_temp_field(&fields[4], tmpstr, tmpsiz, 0,0,0,0);
			tmpsiz = mdb_ascii2unicode(mdb, type_info[i].create_params, 0, 100, tmpstr);
			mdb_fill_temp_field(&fields[5], tmpstr, tmpsiz, 0,0,0,0);
			mdb_fill_temp_field(&fields[6],&type_info[i].nullable, sizeof(SQLSMALLINT), 0,0,0,1);
			mdb_fill_temp_field(&fields[7],&type_info[i].case_sensitive, sizeof(SQLSMALLINT), 0,0,0,1);
			mdb_fill_temp_field(&fields[8],&type_info[i].searchable, sizeof(SQLSMALLINT), 0,0,0,1);
			mdb_fill_temp_field(&fields[9],type_info[i].unsigned_attribute, sizeof(SQLSMALLINT), 0,type_info[i].unsigned_attribute ? 0 : 1,0,1);
			mdb_fill_temp_field(&fields[10],&type_info[i].fixed_prec_scale, sizeof(SQLSMALLINT), 0,0,0,1);
			mdb_fill_temp_field(&fields[11],&type_info[i].auto_unique_value, sizeof(SQLSMALLINT), 0,0,0,1);
			tmpsiz = mdb_ascii2unicode(mdb, type_info[i].local_type_name, 0, 100, tmpstr);
			mdb_fill_temp_field(&fields[12], tmpstr, tmpsiz, 0,0,0,0);
			mdb_fill_temp_field(&fields[13],&type_info[i].minimum_scale, sizeof(SQLSMALLINT), 0,0,0,1);
			mdb_fill_temp_field(&fields[14],&type_info[i].maximum_scale, sizeof(SQLSMALLINT), 0,0,0,1);
			mdb_fill_temp_field(&fields[15],&type_info[i].sql_data_type, sizeof(SQLSMALLINT), 0,0,0,1);
			mdb_fill_temp_field(&fields[16],type_info[i].sql_datetime_sub, sizeof(SQLSMALLINT), 0,type_info[i].sql_datetime_sub ? 0 : 1,0,1);
			mdb_fill_temp_field(&fields[17],type_info[i].num_prec_radix, sizeof(SQLSMALLINT), 0,type_info[i].num_prec_radix ? 0 : 1,0,1);
			mdb_fill_temp_field(&fields[18],type_info[i].interval_precision, sizeof(SQLSMALLINT), 0,type_info[i].interval_precision ? 0 : 1,0,1);

			row_size = mdb_pack_row(ttable, row_buffer, NUM_TYPE_INFO_COLS, fields);
			mdb_add_row_to_pg(ttable,row_buffer, row_size);
			ttable->num_rows++;
		}
	}
	sql->kludge_ttable_pg = g_memdup(mdb->pg_buf, mdb->fmt->pg_size);
	sql->cur_table = ttable;
	
	/* return _SQLExecute(hstmt); */
	return SQL_SUCCESS;
}

SQLRETURN SQL_API SQLParamData(
    SQLHSTMT           hstmt,
    SQLPOINTER FAR    *prgbValue)
{
	TRACE("SQLParamData");
	return SQL_SUCCESS;
}

SQLRETURN SQL_API SQLPutData(
    SQLHSTMT           hstmt,
    SQLPOINTER         rgbValue,
    SQLINTEGER         cbValue)
{
	TRACE("SQLPutData");
	return SQL_SUCCESS;
}

SQLRETURN SQL_API SQLSetConnectOption(
    SQLHDBC            hdbc,
    SQLUSMALLINT       fOption,
    SQLUINTEGER        vParam)
{
	TRACE("SQLSetConnectOption");
	return SQL_SUCCESS;
}

SQLRETURN SQL_API SQLSetStmtOption(
    SQLHSTMT           hstmt,
    SQLUSMALLINT       fOption,
    SQLUINTEGER        vParam)
{
	TRACE("SQLSetStmtOption");
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
	TRACE("SQLSpecialColumns");
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
	TRACE("SQLStatistics");
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

	TRACE("SQLTables");
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
	TRACE("SQLDataSources");
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

	return 0;
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
	return 0;
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
	default:
		break;
	}
	return 0;
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
	return -1;
}

