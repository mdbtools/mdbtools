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

#ifdef DMALLOC
#include "dmalloc.h"
#endif

/**
 * mdb_stats_on:
 * @mdb: Handle to the (open) MDB file to collect stats on.
 *
 * Begins collection of statistics on an MDBHandle.
 *
 * Statistics in LibMDB will track the number of reads from the MDB file.  The
 * collection of statistics is started and stopped with the mdb_stats_on and
 * mdb_stats_off functions.  Collected statistics are accessed by reading the
 * MdbStatistics structure or calling mdb_dump_stats.
 * 
 */
void
mdb_stats_on(MdbHandle *mdb)
{
	if (!mdb->stats) 
		mdb->stats = g_malloc0(sizeof(MdbStatistics));

	mdb->stats->collect = TRUE;
}
/**
 * mdb_stats_off:
 * @mdb: pointer to handle of MDB file with active stats collection.
 *
 * Turns off statistics collection.
 *
 * If mdb_stats_off is not called, statistics will be turned off when handle
 * is freed using mdb_close.
 **/
void
mdb_stats_off(MdbHandle *mdb)
{
	if (!mdb->stats) return;

	mdb->stats->collect = FALSE;
}
/**
 * mdb_dump_stats:
 * @mdb: pointer to handle of MDB file with active stats collection.
 *
 * Dumps current statistics to stdout.
 **/
void
mdb_dump_stats(MdbHandle *mdb)
{
	if (!mdb->stats) return;

	fprintf(stdout, "Physical Page Reads: %lu\n", mdb->stats->pg_reads);
}
