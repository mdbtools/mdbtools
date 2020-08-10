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

#ifndef _connectparams_h_
#define _connectparams_h_

#ifdef HAVE_GLIB
#include <glib.h>
#else
#include <mdbfakeglib.h>
#endif

typedef struct
{
   GString* dsnName;
   GString* iniFileName;
   GHashTable* table;
} ConnectParams;

ConnectParams* NewConnectParams (void);
void FreeConnectParams (ConnectParams* params);

gboolean LookupDSN        (ConnectParams* params, const gchar* dsnName);
gchar*   GetConnectParam  (ConnectParams* params, const gchar* paramName);
void     SetConnectString (ConnectParams* params, const gchar* connectString);
void     DumpParams       (ConnectParams* params, FILE* output);

gchar*   ExtractDSN (ConnectParams* params, const gchar* connectString);
gchar*   ExtractDBQ (ConnectParams* params, const gchar* connectString);

#endif
