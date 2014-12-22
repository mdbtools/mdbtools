/* MDB Tools - A library for reading MS Access database files
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

#include "mdbtools.h"

MDB_DEPRECATED(void,
mdb_init())
{
	fprintf(stderr, "mdb_init() is DEPRECATED and does nothing. Stop calling it.\n");
}

MDB_DEPRECATED(void,
mdb_exit())
{
	fprintf(stderr, "mdb_exit() is DEPRECATED and does nothing. Stop calling it.\n");
}

/* glib - to allow static linking of glib in mdbtools */
void	 mdb_g_free	          (gpointer	 mem) { g_free(mem); }
gpointer mdb_g_malloc         (gsize	 n_bytes) { return g_malloc(n_bytes); }
gpointer mdb_g_malloc0        (gsize	 n_bytes) { return g_malloc0(n_bytes); }
