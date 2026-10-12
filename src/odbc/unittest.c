/* FreeTDS - Library of routines accessing Sybase and Microsoft databases
 * Copyright (C) 1998-1999  Brian Bruns
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
#include <sql.h>
#include <sqlext.h>

// This test requires the presence of nwind.mdb in the MDBPATH.
// It is stored in a separate repository: https://github.com/mdbtools/mdbtestdata

#define SALES_PERSON_LEN 2
#define STATUS_LEN 6

SQLSMALLINT       sOrderID;
SQLSMALLINT       sCustID;
DATE_STRUCT dsOpenDate;
SQLCHAR       szSalesPerson[SALES_PERSON_LEN] = "D";
SQLCHAR       szStatus[STATUS_LEN];
SQLINTEGER      cbOrderID = 0, cbCustID = 0, cbOpenDate = 0, cbSalesPerson = SQL_NTS,
            cbStatus = SQL_NTS;
SQLRETURN   retcode;
HENV        henv;
HDBC        hdbc;
SQLHSTMT    hstmt;

static void printStatementError(HSTMT hstmt, char *msg)
{
	UCHAR  szSqlState[6];
	UCHAR  szErrorMsg[SQL_MAX_MESSAGE_LENGTH];
	SQLINTEGER dwNativeError;
	SWORD  wErrorMsg;

	SQLError(SQL_NULL_HENV, SQL_NULL_HDBC, hstmt, 
			szSqlState, &dwNativeError, szErrorMsg,
			SQL_MAX_MESSAGE_LENGTH-1, &wErrorMsg);

	fprintf(stderr,"%s\n%s\n%s\n",msg,
		szSqlState, szErrorMsg);
}


static int expect_state(HSTMT hstmt, SQLRETURN ret, SQLRETURN expected_ret,
		const char *expected_state, const char *what)
{
	SQLCHAR state[6] = "";
	SQLCHAR msg[SQL_MAX_MESSAGE_LENGTH] = "";
	SQLINTEGER native;
	SQLSMALLINT len;

	if (ret != expected_ret) {
		fprintf(stderr, "%s: returned %d, expected %d\n", what, ret, expected_ret);
		return 1;
	}
	if (!expected_state)
		return 0;
	SQLError(SQL_NULL_HENV, SQL_NULL_HDBC, hstmt, state, &native, msg, sizeof(msg), &len);
	if (strcmp((char *)state, expected_state) != 0) {
		fprintf(stderr, "%s: SQLSTATE %s, expected %s\n", what, state, expected_state);
		return 1;
	}
	printf("%s: %s %s\n", what, state, msg);
	return 0;
}

/* Statement attributes (issue #434): Qt asks for a static cursor; the
 * driver substitutes a forward-only one and says so with 01S02. */
static int test_stmt_attrs(HDBC hdbc, HSTMT hstmt)
{
	SQLUSMALLINT exists = SQL_FALSE;
	SQLULEN value;
	SQLINTEGER len;
	int i;

	printf("\nTesting statement attributes (issue #434)...\n");
	SQLFreeStmt(hstmt, SQL_CLOSE);
	SQLFreeStmt(hstmt, SQL_UNBIND); /* earlier bindings have gone out of scope */

	SQLGetFunctions(hdbc, SQL_API_SQLSETSTMTATTR, &exists);
	if (exists != SQL_TRUE) {
		fprintf(stderr, "SQLGetFunctions does not report SQLSetStmtAttr\n");
		return 1;
	}

	if (expect_state(hstmt, SQLSetStmtAttr(hstmt, SQL_ATTR_CURSOR_TYPE,
				(SQLPOINTER)SQL_CURSOR_STATIC, SQL_IS_UINTEGER),
				SQL_SUCCESS_WITH_INFO, "01S02", "SQL_ATTR_CURSOR_TYPE=SQL_CURSOR_STATIC"))
		return 1;
	value = SQL_CURSOR_STATIC;
	if (expect_state(hstmt, SQLGetStmtAttr(hstmt, SQL_ATTR_CURSOR_TYPE, &value, 0, &len),
				SQL_SUCCESS, NULL, "get SQL_ATTR_CURSOR_TYPE"))
		return 1;
	if (value != SQL_CURSOR_FORWARD_ONLY) {
		fprintf(stderr, "SQL_ATTR_CURSOR_TYPE is %lu, expected forward-only\n", (unsigned long)value);
		return 1;
	}
	value = SQL_SCROLLABLE;
	SQLGetStmtAttr(hstmt, SQL_ATTR_CURSOR_SCROLLABLE, &value, 0, &len);
	if (value != SQL_NONSCROLLABLE) {
		fprintf(stderr, "SQL_ATTR_CURSOR_SCROLLABLE is %lu, expected nonscrollable\n", (unsigned long)value);
		return 1;
	}
	if (expect_state(hstmt, SQLSetStmtAttr(hstmt, SQL_ATTR_CURSOR_TYPE,
				(SQLPOINTER)SQL_CURSOR_FORWARD_ONLY, SQL_IS_UINTEGER),
				SQL_SUCCESS, NULL, "SQL_ATTR_CURSOR_TYPE=SQL_CURSOR_FORWARD_ONLY"))
		return 1;
	if (expect_state(hstmt, SQLSetStmtAttr(hstmt, SQL_ATTR_CONCURRENCY,
				(SQLPOINTER)SQL_CONCUR_READ_ONLY, SQL_IS_UINTEGER),
				SQL_SUCCESS, NULL, "SQL_ATTR_CONCURRENCY=SQL_CONCUR_READ_ONLY"))
		return 1;
	if (expect_state(hstmt, SQLSetStmtAttr(hstmt, SQL_ATTR_ROW_ARRAY_SIZE,
				(SQLPOINTER)10, SQL_IS_UINTEGER),
				SQL_SUCCESS_WITH_INFO, "01S02", "SQL_ATTR_ROW_ARRAY_SIZE=10"))
		return 1;
	if (expect_state(hstmt, SQLSetStmtOption(hstmt, SQL_CONCURRENCY, SQL_CONCUR_LOCK),
				SQL_SUCCESS_WITH_INFO, "01S02", "SQLSetStmtOption(SQL_CONCURRENCY)"))
		return 1;
	if (expect_state(hstmt, SQLSetStmtAttr(hstmt, SQL_ATTR_ROWS_FETCHED_PTR,
				&value, SQL_IS_POINTER),
				SQL_ERROR, "HYC00", "SQL_ATTR_ROWS_FETCHED_PTR"))
		return 1;

	/* The statement must still work after all that */
	if (expect_state(hstmt, SQLExecDirect(hstmt, (SQLCHAR *)"select * from Shippers", SQL_NTS),
				SQL_SUCCESS, NULL, "SQLExecDirect"))
		return 1;
	for (i = 0; SQLFetch(hstmt) == SQL_SUCCESS; i++)
		;
	if (i != 3) {
		fprintf(stderr, "Fetched %d rows from Shippers, expected 3\n", i);
		return 1;
	}
	SQLFreeStmt(hstmt, SQL_CLOSE);
	printf("Statement attribute test passed.\n");
	return 0;
}

/* SQLPrimaryKeys (issue #431) */
static int check_primary_keys(HSTMT hstmt, const char *table, int nkeys, const char **keys)
{
	SQLCHAR table_name[128], column_name[128], pk_name[128];
	SQLSMALLINT key_seq, ncols = 0;
	SQLLEN len;
	int i;

	retcode = SQLPrimaryKeys(hstmt, NULL, 0, NULL, 0, (SQLCHAR *)table, SQL_NTS);
	if (retcode != SQL_SUCCESS) {
		printStatementError(hstmt, "problem with SQLPrimaryKeys");
		return 1;
	}
	SQLNumResultCols(hstmt, &ncols);
	if (ncols != 6) {
		fprintf(stderr, "SQLPrimaryKeys returned %d columns, expected 6\n", ncols);
		return 1;
	}
	for (i = 0; (retcode = SQLFetch(hstmt)) == SQL_SUCCESS; i++) {
		SQLGetData(hstmt, 3, SQL_C_CHAR, table_name, sizeof(table_name), &len);
		SQLGetData(hstmt, 4, SQL_C_CHAR, column_name, sizeof(column_name), &len);
		SQLGetData(hstmt, 5, SQL_C_SSHORT, &key_seq, sizeof(key_seq), &len);
		SQLGetData(hstmt, 6, SQL_C_CHAR, pk_name, sizeof(pk_name), &len);
		printf("  %s.%s KEY_SEQ=%d PK_NAME=%s\n", table_name, column_name, key_seq, pk_name);
		if (i >= nkeys || strcmp((char *)column_name, keys[i]) != 0 || key_seq != i + 1) {
			fprintf(stderr, "Unexpected primary key column for %s\n", table);
			return 1;
		}
	}
	if (retcode != SQL_NO_DATA_FOUND || i != nkeys) {
		fprintf(stderr, "SQLPrimaryKeys returned %d rows for %s, expected %d\n", i, table, nkeys);
		return 1;
	}
	SQLFreeStmt(hstmt, SQL_CLOSE);
	return 0;
}

static int test_primary_keys(HDBC hdbc, HSTMT hstmt)
{
	const char *order_details[] = { "OrderID", "ProductID" };
	const char *orders[] = { "OrderID" };
	SQLUSMALLINT exists = SQL_FALSE;

	printf("\nTesting SQLPrimaryKeys (issue #431)...\n");
	SQLGetFunctions(hdbc, SQL_API_SQLPRIMARYKEYS, &exists);
	if (exists != SQL_TRUE) {
		fprintf(stderr, "SQLGetFunctions does not report SQLPrimaryKeys\n");
		return 1;
	}
	if (check_primary_keys(hstmt, "Order Details", 2, order_details)
			|| check_primary_keys(hstmt, "orders", 1, orders)
			|| check_primary_keys(hstmt, "No such table", 0, NULL))
		return 1;
	printf("SQLPrimaryKeys test passed.\n");
	return 0;
}

/* Catalog functions: non-ASCII names must come back as UTF-8, and each
 * call must replace the previous result set of the statement */
static int test_catalog(HSTMT hstmt)
{
	SQLCHAR name[128];
	SQLSMALLINT ncols = 0;
	SQLLEN len;
	int found = 0, rows = 0;

	printf("\nTesting catalog functions...\n");
	SQLFreeStmt(hstmt, SQL_CLOSE);
	SQLFreeStmt(hstmt, SQL_UNBIND);

	/* leave a result set open: SQLTables must discard it */
	if (SQLExecDirect(hstmt, (SQLCHAR *)"select * from Shippers", SQL_NTS) != SQL_SUCCESS) {
		printStatementError(hstmt, "problem with SQLExecDirect");
		return 1;
	}
	if (SQLTables(hstmt, NULL, 0, NULL, 0, NULL, 0, NULL, 0) != SQL_SUCCESS) {
		printStatementError(hstmt, "problem with SQLTables");
		return 1;
	}
	SQLNumResultCols(hstmt, &ncols);
	if (ncols != 5) {
		fprintf(stderr, "SQLTables returned %d columns, expected 5\n", ncols);
		return 1;
	}
	while (SQLFetch(hstmt) == SQL_SUCCESS) {
		SQLGetData(hstmt, 3, SQL_C_CHAR, name, sizeof(name), &len);
		if (strcmp((char *)name, "Ums\xc3\xa4tze") == 0)
			found = 1;
	}
	if (!found) {
		fprintf(stderr, "SQLTables did not return Ums\xc3\xa4tze\n");
		return 1;
	}

	if (SQLColumns(hstmt, NULL, 0, NULL, 0, (SQLCHAR *)"Ums\xc3\xa4tze", SQL_NTS, NULL, 0) != SQL_SUCCESS) {
		printStatementError(hstmt, "problem with SQLColumns");
		return 1;
	}
	SQLNumResultCols(hstmt, &ncols);
	if (ncols != 18) {
		fprintf(stderr, "SQLColumns returned %d columns, expected 18\n", ncols);
		return 1;
	}
	while (SQLFetch(hstmt) == SQL_SUCCESS) {
		SQLGetData(hstmt, 3, SQL_C_CHAR, name, sizeof(name), &len);
		if (strcmp((char *)name, "Ums\xc3\xa4tze") != 0) {
			fprintf(stderr, "SQLColumns returned table name '%s'\n", name);
			return 1;
		}
		rows++;
	}
	if (rows == 0) {
		fprintf(stderr, "SQLColumns returned no columns\n");
		return 1;
	}
	SQLFreeStmt(hstmt, SQL_CLOSE);
	printf("Catalog test passed (%d columns).\n", rows);
	return 0;
}

/* Column aliases and qualified names (issues #432, #360) */
static int test_aliases(HSTMT hstmt)
{
	const char *names[] = { "id", "CompanyName", "name2" };
	const char *bases[] = { "ShipperID", "CompanyName", "CompanyName" };
	SQLCHAR name[64], v2[64], v3[64];
	SQLSMALLINT len, type, scale, nullable, ncols = 0, i;
	SQLULEN size;
	SQLLEN ind, num;
	SQLINTEGER id = 0;

	printf("\nTesting column aliases...\n");
	SQLFreeStmt(hstmt, SQL_CLOSE);
	SQLFreeStmt(hstmt, SQL_UNBIND);
	if (SQLExecDirect(hstmt, (SQLCHAR *)"SELECT s.ShipperID AS id, [s].[CompanyName], "
				"\"s\".CompanyName name2 FROM Shippers AS s WHERE s.ShipperID = 2",
				SQL_NTS) != SQL_SUCCESS) {
		printStatementError(hstmt, "problem with SQLExecDirect");
		return 1;
	}
	SQLNumResultCols(hstmt, &ncols);
	if (ncols != 3) {
		fprintf(stderr, "%d result columns, expected 3\n", ncols);
		return 1;
	}
	for (i = 1; i <= ncols; i++) {
		if (SQLDescribeCol(hstmt, i, name, sizeof(name), &len, &type, &size, &scale, &nullable) != SQL_SUCCESS
				|| strcmp((char *)name, names[i-1]) != 0) {
			fprintf(stderr, "SQLDescribeCol(%d) = %s, expected %s\n", i, name, names[i-1]);
			return 1;
		}
		if (SQLColAttributes(hstmt, i, SQL_DESC_LABEL, name, sizeof(name), &len, &num) != SQL_SUCCESS
				|| strcmp((char *)name, names[i-1]) != 0) {
			fprintf(stderr, "SQL_DESC_LABEL(%d) = %s, expected %s\n", i, name, names[i-1]);
			return 1;
		}
		if (SQLColAttributes(hstmt, i, SQL_DESC_BASE_COLUMN_NAME, name, sizeof(name), &len, &num) != SQL_SUCCESS
				|| strcmp((char *)name, bases[i-1]) != 0) {
			fprintf(stderr, "SQL_DESC_BASE_COLUMN_NAME(%d) = %s, expected %s\n", i, name, bases[i-1]);
			return 1;
		}
		printf("  column %d: %s (%s)\n", i, names[i-1], name);
	}
	if (SQLFetch(hstmt) != SQL_SUCCESS) {
		printStatementError(hstmt, "problem with SQLFetch");
		return 1;
	}
	SQLGetData(hstmt, 1, SQL_C_SLONG, &id, sizeof(id), &ind);
	SQLGetData(hstmt, 2, SQL_C_CHAR, v2, sizeof(v2), &ind);
	SQLGetData(hstmt, 3, SQL_C_CHAR, v3, sizeof(v3), &ind);
	if (id != 2 || strcmp((char *)v2, "United Package") != 0 || strcmp((char *)v3, (char *)v2) != 0) {
		fprintf(stderr, "Unexpected row: %d, %s, %s\n", (int)id, v2, v3);
		return 1;
	}
	if (SQLFetch(hstmt) != SQL_NO_DATA_FOUND) {
		fprintf(stderr, "More than one row\n");
		return 1;
	}
	SQLFreeStmt(hstmt, SQL_CLOSE);
	printf("Column alias test passed.\n");
	return 0;
}

int main(int argc, char **argv)
{
int i;

	retcode = SQLAllocEnv(&henv);
	if (retcode != SQL_SUCCESS) {
		fprintf(stderr,"SQLAllocEnv failed: %d\n", retcode);
		return 1;
	}
	
	if (SQLAllocConnect(henv, &hdbc) != SQL_SUCCESS)
	{
		UCHAR  szSqlState[6];
		UCHAR  szErrorMsg[SQL_MAX_MESSAGE_LENGTH];
		SQLINTEGER dwNativeError;
		SWORD  wErrorMsg;

		SQLError(henv, SQL_NULL_HDBC, SQL_NULL_HSTMT, 
			szSqlState, &dwNativeError, szErrorMsg,
			SQL_MAX_MESSAGE_LENGTH-1, &wErrorMsg);

		fprintf(stderr,"problem with SQLAllocConnect\n%s\n%s\n", 
			szSqlState, szErrorMsg);
		return 1;
	}


	retcode = SQLSetConnectOption(hdbc, SQL_ACCESS_MODE, SQL_MODE_READ_ONLY);
	if (retcode != SQL_SUCCESS && retcode != SQL_SUCCESS_WITH_INFO)
	{
		UCHAR  szSqlState[6];
		UCHAR  szErrorMsg[SQL_MAX_MESSAGE_LENGTH];
		SQLINTEGER dwNativeError;
		SWORD  wErrorMsg;

		SQLError(SQL_NULL_HENV, hdbc, SQL_NULL_HSTMT, 
			szSqlState, &dwNativeError, szErrorMsg,
			SQL_MAX_MESSAGE_LENGTH-1, &wErrorMsg);

		fprintf(stderr,"problem with SQLSetConnectOption\n%s\n%s\n",
			szSqlState, szErrorMsg);
		return 1;
	}

	retcode = SQLSetConnectOption(hdbc, SQL_AUTOCOMMIT, SQL_AUTOCOMMIT_ON);
	if (retcode != SQL_SUCCESS && retcode != SQL_SUCCESS_WITH_INFO)
	{
		UCHAR  szSqlState[6];
		UCHAR  szErrorMsg[SQL_MAX_MESSAGE_LENGTH];
		SQLINTEGER dwNativeError;
		SWORD  wErrorMsg;

		SQLError(SQL_NULL_HENV, hdbc, SQL_NULL_HSTMT, 
			szSqlState, &dwNativeError, szErrorMsg,
			SQL_MAX_MESSAGE_LENGTH-1, &wErrorMsg);

		fprintf(stderr,"problem with SQLSetConnectOption\n%s\n%s\n",
			szSqlState, szErrorMsg);
		return 1;
	}


	retcode = SQLDriverConnect(hdbc, NULL,
            (UCHAR *)"DBQ=nwind.mdb", SQL_NTS,
            NULL, 0, NULL, SQL_DRIVER_NOPROMPT);
	if (retcode != SQL_SUCCESS && retcode != SQL_SUCCESS_WITH_INFO)
	{
		UCHAR  szSqlState[6];
		UCHAR  szErrorMsg[SQL_MAX_MESSAGE_LENGTH];
		SQLINTEGER dwNativeError;
		SWORD  wErrorMsg;

		SQLError(SQL_NULL_HENV, hdbc, SQL_NULL_HSTMT, 
			szSqlState, &dwNativeError, szErrorMsg,
			SQL_MAX_MESSAGE_LENGTH-1, &wErrorMsg);

		fprintf(stderr,"problem with SQLConnect\n%s\n%s\n",
			szSqlState, szErrorMsg);
		return 1;
	}


	if (SQLAllocStmt(hdbc, &hstmt)!= SQL_SUCCESS)
	{
		UCHAR  szSqlState[6];
		UCHAR  szErrorMsg[SQL_MAX_MESSAGE_LENGTH];
		SQLINTEGER dwNativeError;
		SWORD  wErrorMsg;

		SQLError(SQL_NULL_HENV, hdbc, SQL_NULL_HSTMT, 
			szSqlState, &dwNativeError, szErrorMsg,
			SQL_MAX_MESSAGE_LENGTH-1, &wErrorMsg);

		fprintf(stderr,"problem with SQLAllocStmt\n%s\n%s\n",
			szSqlState, szErrorMsg);
		return 1;
	}

	/* Prepare the SQL statement with parameter markers. */

	retcode = SQLPrepare(hstmt,
 	       (unsigned char *)"select * from Shippers", 
			SQL_NTS);
			  
	if (retcode == SQL_SUCCESS || retcode == SQL_SUCCESS_WITH_INFO) 
	{
		long id_value;
		UCHAR  szCol1[60];
		SQLLEN length;

		printf("excecuting first statement\n");
		retcode = SQLExecute(hstmt);         
		if (retcode != SQL_SUCCESS) {
			UCHAR  szSqlState[6];
			UCHAR  szErrorMsg[SQL_MAX_MESSAGE_LENGTH];
			SQLINTEGER dwNativeError;
			SWORD  wErrorMsg;

			SQLError(SQL_NULL_HENV, SQL_NULL_HDBC, hstmt, 
				szSqlState, &dwNativeError, szErrorMsg,
				SQL_MAX_MESSAGE_LENGTH-1, &wErrorMsg);

			fprintf(stderr,"problem with SQLExecute\n%s\n%s\n",
				szSqlState, szErrorMsg);
			return 1;
		}		
		/* Column names: SQLDescribeCol reports the length in characters,
		 * SQLColAttributes reports it in bytes (issue #357). For the ANSI
		 * driver these are equal to strlen(name). */
		{
			SQLSMALLINT ncols = 0, icol;
			SQLNumResultCols(hstmt, &ncols);
			for (icol = 1; icol <= ncols; icol++) {
				UCHAR name1[64], name2[64];
				SQLSMALLINT len1 = -1, len2 = -1, type, scale, nullable;
				SQLULEN size;
				SQLLEN num;
				retcode = SQLDescribeCol(hstmt, icol, name1, sizeof(name1), &len1,
						&type, &size, &scale, &nullable);
				if (retcode != SQL_SUCCESS) {
					printStatementError(hstmt, "problem with SQLDescribeCol");
					return 1;
				}
				retcode = SQLColAttributes(hstmt, icol, SQL_COLUMN_NAME, name2, sizeof(name2), &len2, &num);
				if (retcode != SQL_SUCCESS) {
					printStatementError(hstmt, "problem with SQLColAttributes");
					return 1;
				}
				printf("column %d: SQLDescribeCol=\"%s\" (%d) SQLColAttributes=\"%s\" (%d)\n",
						icol, name1, len1, name2, len2);
				if (strcmp((char *)name1, (char *)name2) != 0
						|| len1 != (SQLSMALLINT)strlen((char *)name1)
						|| len2 != (SQLSMALLINT)strlen((char *)name2)) {
					fprintf(stderr, "column name / length mismatch for column %d\n", icol);
					return 1;
				}
			}
		}

		SQLBindCol(hstmt, 1, SQL_C_LONG, &id_value, sizeof(id_value), NULL);
		SQLBindCol(hstmt, 3, SQL_CHAR, szCol1, sizeof(szCol1), &length);
	
		/* Execute statement with first row. */

		i=0;
		while ((retcode = SQLFetch(hstmt)) == SQL_SUCCESS)
		{
			i++;
			printf("%d: id = %ld  szCol1 = %s (%d)\n", i, id_value, szCol1, (int)length);
		}
		if (retcode != SQL_NO_DATA_FOUND)
		{
			printStatementError(hstmt, "problem with SQLFetch");
			return 1;
		}
	}		
	printf("Done\n");

	/* Test for issue #458: MDB_FLOAT should map to SQL_REAL, not SQL_FLOAT
	 * This tests the fix in _odbc_get_client_type() where MDB_FLOAT
	 * columns return SQL_REAL (7) instead of SQL_FLOAT (6).
	 * Single-precision floats in MS Access (Single/MDB_FLOAT) must map to
	 * SQL_REAL for correct value interpretation.
	 */
	printf("\nTesting column type mapping (issue #458)...\n");

	/* Reset statement for new query */
	SQLFreeStmt(hstmt, SQL_CLOSE);

	/* Query Order Details table - the Discount column is typically Single (MDB_FLOAT) */
	retcode = SQLPrepare(hstmt,
		(unsigned char *)"select * from [Order Details]",
		SQL_NTS);

	if (retcode == SQL_SUCCESS || retcode == SQL_SUCCESS_WITH_INFO)
	{
		retcode = SQLExecute(hstmt);
		if (retcode == SQL_SUCCESS || retcode == SQL_SUCCESS_WITH_INFO)
		{
			SQLSMALLINT numCols;
			SQLNumResultCols(hstmt, &numCols);

			printf("Checking column types for Order Details (%d columns):\n", numCols);

			for (i = 1; i <= numCols; i++)
			{
				SQLLEN colType = 0;
				SQLSMALLINT nameLen = 0;
				SQLCHAR colName[128] = {0};

				/* Get column name - use ODBC 2.x SQLColAttributes */
				SQLColAttributes(hstmt, i, SQL_COLUMN_NAME, colName, sizeof(colName), &nameLen, NULL);

				/* Get column type */
				SQLColAttributes(hstmt, i, SQL_COLUMN_TYPE, NULL, 0, NULL, &colType);

				printf("  Column %d (%s): SQL type = %ld", i, colName, (long)colType);

				/* SQL_REAL = 7, SQL_FLOAT = 6, SQL_DOUBLE = 8 */
				if (colType == SQL_REAL) {
					printf(" (SQL_REAL - correct for Single/MDB_FLOAT)\n");
				} else if (colType == SQL_FLOAT) {
					/* This would indicate the bug is present */
					fprintf(stderr, "\nERROR: Column %s returned SQL_FLOAT (%ld) instead of SQL_REAL (%d)!\n",
						colName, (long)colType, SQL_REAL);
					fprintf(stderr, "This indicates issue #458 is not fixed.\n");
					return 1;
				} else if (colType == SQL_DOUBLE) {
					printf(" (SQL_DOUBLE - correct for Double/MDB_DOUBLE)\n");
				} else {
					printf("\n");
				}
			}
			printf("Column type mapping test passed.\n");
		}
		else
		{
			/* Order Details table might not exist in all test databases */
			printStatementError(hstmt, "Note: Order Details query failed (table may not exist)");
			printf("Skipping column type test (Order Details table not available)\n");
		}
	}
	else
	{
		printf("Skipping column type test (prepare failed)\n");
	}

	printf("Done\n");

	if (test_stmt_attrs(hdbc, hstmt))
		return 1;

	if (test_primary_keys(hdbc, hstmt))
		return 1;

	if (test_catalog(hstmt))
		return 1;

	if (test_aliases(hstmt))
		return 1;

	return 0;
}
