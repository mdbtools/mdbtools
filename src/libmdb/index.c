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

GPtrArray *mdb_read_indices(MdbTableDef *table)
{
MdbHandle *mdb = table->entry->mdb;
MdbIndex idx, *pidx;
int len, i;
int cur_pos;
int name_sz;

/* FIX ME -- doesn't handle multipage table headers */

        table->indices = g_ptr_array_new();

        cur_pos = table->index_start;

	for (i=0;i<table->num_real_idxs;i++) {
		memset(&idx, '\0', sizeof(MdbIndex));
		idx.index_num = i;
		cur_pos += 34;
		idx.first_pg = mdb_get_int32(mdb, cur_pos);
		cur_pos += 5;
		mdb_append_index(table->indices, &idx);
	}

	for (i=0;i<table->num_idxs;i++) {
		pidx = g_ptr_array_index (table->indices, i);
		cur_pos += 19;
		if (mdb->pg_buf[cur_pos++]==0x01) 
			pidx->primary_key=1;
	}

	for (i=0;i<table->num_idxs;i++) {
		pidx = g_ptr_array_index (table->indices, i);
		name_sz=mdb->pg_buf[cur_pos++];
		memcpy(pidx->name, &mdb->pg_buf[cur_pos], name_sz);
		pidx->name[name_sz]='\0';		
		//fprintf(stderr, "index name %s\n", pidx->name);
		cur_pos += name_sz;
	}
}
void mdb_index_dump(MdbIndex *idx)
{
int i;

	fprintf(stdout,"index number     %d\n", idx->index_num);
	fprintf(stdout,"index name       %s\n", idx->name);
	fprintf(stdout,"index first page %d\n", idx->first_pg);
	if (idx->primary_key) fprintf(stdout,"index is a primary key\n");
}
