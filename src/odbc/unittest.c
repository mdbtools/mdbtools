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
		SQLBindCol(hstmt, 3, SQL_CHAR, szCol1, sizeof(szCol1), &length);
		//SQLBindCol(hstmt, 1, SQL_CHAR, szCol1, 60, NULL);
	
		/* Execute statement with first row. */

		i=0;
		while ((retcode = SQLFetch(hstmt)) == SQL_SUCCESS)
		{
			i++;
			printf("%d: szCol1 = %s (%d)\n", i, szCol1, (int)length);
		}
		if (retcode != SQL_NO_DATA_FOUND)
		{
			printStatementError(hstmt, "problem with SQLFetch");
			return 1;
		}
	}		
	printf("Done\n");

	return 0;
}
