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
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include "mdbtools.h"
#include <locale.h>

#ifdef DMALLOC
#include "dmalloc.h"
#endif

/**
 * mdb_init:
 *
 * Initializes the LibMDB library.  This function should be called exactly once
 * by calling program and prior to any other function.
 *
 **/
void mdb_init()
{
	mdb_init_backends();
}

/**
 * mdb_exit:
 *
 * Cleans up the LibMDB library.  This function should be called exactly once
 * by the calling program prior to exiting (or prior to final use of LibMDB 
 * functions).
 *
 **/
void mdb_exit()
{
	mdb_remove_backends();
}
