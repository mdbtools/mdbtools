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
	guint32 pgnum, i, usage_bitlen;
	unsigned char *usage_bitmap;

	pgnum = mdb_get_int32(map, 1);
	usage_bitmap = map + 5;
	usage_bitlen = (map_sz - 5) * 8;

	i = (start_pg >= pgnum) ? start_pg-pgnum+1 : 0;
	for (; i<usage_bitlen; i++) {
		if (usage_bitmap[i/8] & (1 << (i%8))) {
			return pgnum + i;
		}
	}
	/* didn't find anything */
	return 0;
}
static int 
mdb_map_find_next1(MdbHandle *mdb, unsigned char *map, unsigned int map_sz, guint32 start_pg)
{
	guint32 map_ind, max_map_pgs, offset, usage_bitlen;

	/*
	* start_pg will tell us where to (re)start the scan
	* for the next data page.  each usage_map entry points to a
	* 0x05 page which bitmaps (mdb->fmt->pg_size - 4) * 8 pages.
	*
	* map_ind gives us the starting usage_map entry
	* offset gives us a page offset into the bitmap
	*/
	usage_bitlen = (mdb->fmt->pg_size - 4) * 8;
	max_map_pgs = (map_sz - 1) / 4;
	map_ind = (start_pg + 1) / usage_bitlen;
	offset = (start_pg + 1) % usage_bitlen;

	for (; map_ind<max_map_pgs; map_ind++) {
		unsigned char *usage_bitmap;
		guint32 i, map_pg;

		if (!(map_pg = mdb_get_int32(map, (map_ind*4)+1))) {
			continue;
		}
		if(mdb_read_alt_pg(mdb, map_pg) != mdb->fmt->pg_size) {
			fprintf(stderr, "Oops! didn't get a full page at %d\n", map_pg);
			exit(1);
		} 

		usage_bitmap = mdb->alt_pg_buf + 4;
		for (i=offset; i<usage_bitlen; i++) {
			if (usage_bitmap[i/8] & (1 << (i%8))) {
				return map_ind*usage_bitlen + i;
			}
		}
		offset = 0;
	}
	/* didn't find anything */
	return 0;
}
guint32 
mdb_map_find_next(MdbHandle *mdb, unsigned char *map, unsigned int map_sz, guint32 start_pg)
{
	if (map[0] == 0) {
		return mdb_map_find_next0(mdb, map, map_sz, start_pg);
	} else if (map[0] == 1) {
		return mdb_map_find_next1(mdb, map, map_sz, start_pg);
	}

	fprintf(stderr, "Warning: unrecognized usage map type: %d\n", map[0]);
	return -1;
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
