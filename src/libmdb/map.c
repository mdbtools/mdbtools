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

#include "mdbtools.h"

#ifdef DMALLOC
#include "dmalloc.h"
#endif

static guint32 
mdb_map_find_next0(MdbHandle *mdb, unsigned char *map, unsigned int map_sz, guint32 start_pg)
{
	unsigned int pgnum, i, bitn;

	pgnum = mdb_get_int32(map,1);
	/* the first 5 bytes of the usage map mean something */
	for (i=5;i<map_sz;i++) {
		for (bitn=0;bitn<8;bitn++) {
			if ((map[i] & (1 << bitn)) && (pgnum > start_pg)) {
				return pgnum;
			}
			pgnum++;
		}
	}
	/* didn't find anything */
	return 0;
}
static int 
mdb_map_find_next1(MdbHandle *mdb, unsigned char *map, unsigned int map_sz, guint32 start_pg)
{
	guint32 pgnum, i, j, bitn, map_pg;

	pgnum = 0;
	//printf("map size %ld\n", table->map_sz);
	for (i=1;i<map_sz-1;i+=4) {
		map_pg = mdb_get_int32(map, i);
		//printf("loop %d pg %ld %02x%02x%02x%02x\n",i, map_pg,table->usage_map[i],table->usage_map[i+1],table->usage_map[i+2],table->usage_map[i+3]);

		if (!map_pg) continue;

		if(mdb_read_alt_pg(mdb, map_pg) != mdb->fmt->pg_size) {
			fprintf(stderr, "Oops! didn't get a full page at %d\n", map_pg);
			exit(1);
		} 
		//printf("reading page %ld\n",map_pg);
		for (j=4;j<mdb->fmt->pg_size;j++) {
			for (bitn=0;bitn<8;bitn++) {
				if (mdb->alt_pg_buf[j] & 1 << bitn && pgnum > start_pg) {
					return pgnum;
				}
				pgnum++;
			}
		}
	}
	/* didn't find anything */
	//printf("returning 0\n");
	return 0;
}
guint32 
mdb_map_find_next(MdbHandle *mdb, unsigned char *map, unsigned int map_sz, guint32 start_pg)
{
	int map_type;

	map_type = map[0];
	if (map_type==0) {
		return mdb_map_find_next0(mdb, map, map_sz, start_pg);
	} else if (map_type==1) {
		return mdb_map_find_next1(mdb, map, map_sz, start_pg);
	} else {
		fprintf(stderr,"Warning: unrecognized usage map type: %d, defaulting to brute force read\n",map[0]);
	}
	return 0;
}
guint32
mdb_alloc_page(MdbTableDef *table)
{
	printf("Allocating new page\n");
	return 0;
}
guint32 
mdb_map_find_next_freepage(MdbTableDef *table, int row_size)
{
	MdbCatalogEntry *entry = table->entry;
	MdbHandle *mdb = entry->mdb;
	guint32 pgnum;
	guint32 cur_pg = 0;
	int free_space;

	do {
		pgnum = mdb_map_find_next(mdb, 
				table->free_usage_map, 
				table->freemap_sz, cur_pg);
		printf("looking at page %d\n", pgnum);
		if (!pgnum) {
			/* allocate new page */
			pgnum = mdb_alloc_page(table);
			return pgnum;
		}
		cur_pg = pgnum;

		mdb_read_pg(mdb, pgnum);
		free_space = mdb_pg_get_freespace(mdb);
		
	} while (free_space < row_size);

	printf("page %d has %d bytes left\n", pgnum, free_space);

	return pgnum;
}
