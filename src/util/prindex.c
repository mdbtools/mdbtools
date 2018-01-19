/* MDB Tools - A library for reading MS Access database file
 * Copyright (C) 2000 Brian Bruns
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include "mdbtools.h"

extern char idx_to_text[];

void walk_index(MdbHandle *mdb, MdbIndex *idx);

int
main(int argc, char **argv)
{
	unsigned int j;
	MdbHandle *mdb;
	MdbTableDef *table;
	MdbIndex *idx;

	if (argc<4) {
		fprintf(stderr,"Usage: %s <file> <table> <index>\n",argv[0]);
		exit(1);
	}
	
	if (!(mdb = mdb_open(argv[1], MDB_NOFLAGS))) {
		fprintf(stderr,"Unable to open database.\n");
		exit(1);
	}

	table = mdb_read_table_by_name(mdb, argv[2], MDB_TABLE);

	if (table) {
		mdb_read_columns(table);
		mdb_read_indices(table);
		for (j=0;j<table->num_idxs;j++) {
			idx = g_ptr_array_index (table->indices, j);
			if (!strcmp(idx->name, argv[3])) {
				walk_index(mdb, idx);
			}
		}
		mdb_free_tabledef(table);
	} else {
		fprintf(stderr,"No table named %s found.\n", argv[2]);
	}

	mdb_close(mdb);
	return 0;
}
char *
page_name(int page_type)
{
	switch(page_type) {
		case 0x00: return "Database Properties"; break;
		case 0x01: return "Data"; break;
		case 0x02: return "Table Definition"; break;
		case 0x03: return "Index"; break;
		case 0x04: return "Index Leaf"; break;
		case 0x05: return "Page Usage"; break;
		default: return "Unknown";
	}
}
void check_row(MdbHandle *mdb, MdbIndex *idx, guint32 pg, int row)
{
	MdbField fields[256];
	unsigned int num_fields, i, j;
	int row_start;
	size_t row_size;
	MdbColumn *col;
	gchar buf[256], key[256];
	int elem;
	MdbTableDef *table = idx->table; 
	
	mdb_read_pg(mdb, pg);
	mdb_find_row(mdb, row, &row_start, &row_size);

	num_fields = mdb_crack_row(table, row_start, row_start + row_size - 1,
		fields);
	for (i=0;i<idx->num_keys;i++) {
		col=g_ptr_array_index(table->columns,idx->key_col_num[i]-1);
		if (col->col_type==MDB_TEXT) {
			mdb_index_hash_text(mdb, buf, key);
		}
		for (j=0;j<num_fields;j++) {
			if (fields[j].colnum+1==idx->key_col_num[i]) {
				elem = j;
				break;
			}
		}
		if (j==num_fields) {
			fprintf(stderr, "ERROR Lost field #%d\n", idx->key_col_num[i]);
			continue;
		}
		//j = idx->key_col_num[i];
		strncpy(buf, fields[elem].value, fields[elem].siz);
		buf[fields[elem].siz]=0;
		//fprintf(stdout, "elem %d %d column %d %s %s\n",elem, fields[elem].colnum, idx->key_col_num[i], col->name, buf);
		if (col->col_type == MDB_TEXT) {
			// fprintf(stdout, "%s = %s \n", buf, key);
		}
	}
}
void
walk_index(MdbHandle *mdb, MdbIndex *idx)
{
	guint32 pg;
	guint16 row;
	MdbHandle *mdbidx;
	MdbIndexChain chain;

#if 0
	MdbTableDef *table = idx->table;
	MdbSarg sarg;

	sarg.op = MDB_EQUAL;
	//strcpy(sarg.value.s, "SP");
	sarg.value.i = 2;
	mdb_add_sarg_by_name(table, "ShipperID", &sarg);
#endif

	memset(&chain, 0, sizeof(MdbIndexChain));
	printf("name %s\n", idx->name);
	printf("root page %lu\n", (long unsigned) idx->first_pg);
	/* clone the handle to search the index, and use the original to read 
	 * the data */
	mdbidx = mdb_clone_handle(mdb);
	mdb_read_pg(mdbidx, idx->first_pg);
	//printf("page type %02x %s\n", mdbidx->pg_buf[0], page_name(mdbidx->pg_buf[0]));
	while (mdb_index_find_next(mdbidx, idx, &chain, &pg, &row)) {
		printf("row = %d pg = %lu\n", row, (long unsigned) pg);
		check_row(mdb, idx, pg, row);
	}
	mdb_close(mdbidx);
}
