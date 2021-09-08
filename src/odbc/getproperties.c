/* MDB Tools - A library for reading MS Access database file
 * Copyright (C) 2000-2004 Brian Bruns
 *
 * portions based on FreeTDS, Copyright (C) 1998-1999 Brian Bruns
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

#include <stdlib.h>
#include <string.h>
#include <odbcinstext.h>

int
ODBCINSTGetProperties(HODBCINSTPROPERTY hLastProperty)
{
	hLastProperty->pNext = malloc(sizeof(ODBCINSTPROPERTY));
	hLastProperty = hLastProperty->pNext;
	memset(hLastProperty, 0, sizeof(ODBCINSTPROPERTY));
	hLastProperty->nPromptType = ODBCINST_PROMPTTYPE_FILENAME;
	strncpy(hLastProperty->szName, "Database", INI_MAX_PROPERTY_NAME);
	strncpy(hLastProperty->szValue, "", INI_MAX_PROPERTY_VALUE);
	hLastProperty->pszHelp = strdup("Filename and Path of MDB file to connect to.\n"
						 "Use the full path to the database file.");

	return 1;
}
