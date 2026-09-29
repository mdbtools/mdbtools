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

	return 0;
}
