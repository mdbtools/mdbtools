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
char idx_to_text[] = {
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* 0-7     0x00-0x07 */
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* 8-15    0x09-0x0f */
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* 16-23   0x10-0x17 */
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* 24-31   0x19-0x1f */
' ',  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* 32-39   0x20-0x27 */
0x00, 0x00, 0x00, 0x00, 0x00, ' ',  ' ',  0x00, /* 40-47   0x29-0x2f */
'V',  'W',  'X',  'Y',  'Z',  '[',  '\\', ']',  /* 48-55   0x30-0x37 */
'^',  '_',  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* 56-63   0x39-0x3f */
0x00, '`',  'a',  'b',  'd',  'f',  'g',  'h',  /* 64-71   0x40-0x47 */
'i',  'j',  'k',  'l',  'm',  'o',  'p',  'r',  /* 72-79   0x49-0x4f  H */
's',  't',  'u',  'v',  'w',  'x',  'z',  '{',  /* 80-87   0x50-0x57  P */
'|',  '}',  '~',  '5',  '6',  '7',  '8',  '9',  /* 88-95   0x59-0x5f */
0x00, '`',  'a',  'b',  'd',  'f',  'g',  'h',  /* 96-103  0x60-0x67 */
'i',  'j',  'k',  'l',  'm',  'o',  'p',  'r',  /* 014-111 0x69-0x6f  h */
's',  't',  'u',  'v',  'w',  'x',  'z',  '{',  /* 112-119 0x70-0x77  p */
'|',  '}',  '~',  0x00, 0x00, 0x00, 0x00, 0x00, /* 120-127 0x78-0x7f */
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* 128-135 0x80-0x87 */
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* 0x88-0x8f */
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* 0x90-0x97 */
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* 0x98-0x9f */
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* 0xa0-0xa7 */
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* 0xa8-0xaf */
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* 0xb0-0xb7 */
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* 0xb8-0xbf */
0x00, 0x00, 0x00, 0x00, 0x00, '`',  0x00, 0x00, /* 0xc0-0xc7 */
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* 0xc8-0xcf */
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* 0xd0-0xd7 */
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* 0xd8-0xdf */
0x00, '`',  0x00, '`',  '`',  '`',  0x00, 0x00, /* 0xe0-0xe7 */
'f',  'f',  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* 0xe8-0xef */
0x00, 0x00, 0x00, 'r',  0x00, 0x00, 'r',  0x00, /* 0xf0-0xf7 */
0x81, 0x00, 0x00, 0x00, 'x',  0x00, 0x00, 0x00, /* 0xf8-0xff */
};

GPtrArray *
mdb_read_indices(MdbTableDef *table)
{
MdbHandle *mdb = table->entry->mdb;
MdbIndex idx, *pidx;
int len, i, j;
int idx_num, key_num, col_num;
int cur_pos;
int name_sz;

/* FIX ME -- doesn't handle multipage table headers */

        table->indices = g_ptr_array_new();

        cur_pos = table->index_start + 39 * table->num_real_idxs;

	for (i=0;i<table->num_idxs;i++) {
		memset(&idx, '\0', sizeof(MdbIndex));
		idx.table = table;
		idx.index_num = mdb_get_int16(mdb, cur_pos);
		cur_pos += 19;
		idx.index_type = mdb->pg_buf[cur_pos++]; 
		mdb_append_index(table->indices, &idx);
	}

	for (i=0;i<table->num_idxs;i++) {
		pidx = g_ptr_array_index (table->indices, i);
		name_sz=mdb->pg_buf[cur_pos++];
		memcpy(pidx->name, &mdb->pg_buf[cur_pos], name_sz);
		pidx->name[name_sz]='\0';		
		//fprintf(stderr, "index name %s\n", pidx->name);
		cur_pos += name_sz;
	}

	cur_pos = table->index_start;
	idx_num=0;
	for (i=0;i<table->num_real_idxs;i++) {
		do {
			pidx = g_ptr_array_index (table->indices, idx_num++);
		} while (pidx && pidx->index_type==2);

		/* if there are more real indexes than index entries left after
		   removing type 2's decrement real indexes and continue.  Happens
		   on Northwind Orders table.
		*/
		if (!pidx) {
			table->num_real_idxs--;
			continue;
		}

		pidx->num_rows = mdb_get_int32(mdb, 43+(i*8) );

		key_num=0;
		for (j=0;j<MDB_MAX_IDX_COLS;j++) {
			col_num=mdb_get_int16(mdb,cur_pos);
			cur_pos += 2;
			if (col_num != 0xFFFF) {
				/* set column number to a 1 based column number and store */
				pidx->key_col_num[key_num]=col_num + 1;
				if (mdb->pg_buf[cur_pos]) {
					pidx->key_col_order[key_num]=MDB_ASC;
				} else {
					pidx->key_col_order[key_num]=MDB_DESC;
				}
				key_num++;
			}
			cur_pos++;
		}
		pidx->num_keys = key_num;
		cur_pos += 4;
		pidx->first_pg = mdb_get_int32(mdb, cur_pos);
		cur_pos += 4;
		pidx->flags = mdb->pg_buf[cur_pos++];
	}
}
void
mdb_index_hash_text(guchar *text, guchar *hash)
{
	int k;

	for (k=0;k<strlen(text);k++) {
		hash[k] = idx_to_text[text[k]];
		if (!(hash[k])) fprintf(stderr, 
				"No translation available for %02x %d\n", 
				text[k],text[k]);
	}
	hash[strlen(text)]=0;
}
guint32
mdb_index_swap_int32(guint32 l)
{
	unsigned char *c, *c2;
	guint32 l2;

	c = &l;
	c2 = &l2;
	c2[0]=c[3];
	c2[1]=c[2];
	c2[2]=c[1];
	c2[3]=c[0];

	return l2;
}
void mdb_index_cache_sarg(MdbColumn *col, MdbSarg *sarg, MdbSarg *idx_sarg)
{
	guint32 cache_int;
	unsigned char *c;

	switch (col->col_type) {
		case MDB_TEXT:
		mdb_index_hash_text(sarg->value.s, idx_sarg->value.s);
		break;

		case MDB_LONGINT:
		idx_sarg->value.i = mdb_index_swap_int32(sarg->value.i);
		//cache_int = sarg->value.i * -1;
		c = &(idx_sarg->value.i);
		c[0] |= 0x80;
		//printf("int %08x %02x %02x %02x %02x\n", sarg->value.i, c[0], c[1], c[2], c[3]);
		break;	

		case MDB_INT:
		break;	

		default:
		break;	
	}
}
int
mdb_index_test_sargs(MdbHandle *mdb, MdbIndex *idx, int offset, int len)
{
	int i, j;
	MdbColumn *col;
	MdbTableDef *table = idx->table;
	MdbSarg *idx_sarg;
	MdbSarg *sarg;
	int c_offset = 0, c_len;

	for (i=0;i<idx->num_keys;i++) {
		c_offset++; /* the per column null indicator/flags */
		col=g_ptr_array_index(table->columns,idx->key_col_num[i]-1);
		/*
		 * This will go away eventually
		 */
		if (col->col_type==MDB_TEXT) {
			c_len = strlen(&mdb->pg_buf[offset + c_offset]);
		} else {
			c_len = col->col_size;
			//fprintf(stderr,"Only text types currently supported.  How did we get here?\n");
		}
		/*
		 * If we have no cached index values for this column, 
		 * create them.
		 */
		if (col->num_sargs && !col->idx_sarg_cache) {
			col->idx_sarg_cache = g_ptr_array_new();
			for (j=0;j<col->num_sargs;j++) {
				sarg = g_ptr_array_index (col->sargs, j);
				idx_sarg = g_memdup(sarg,sizeof(MdbSarg));
				//printf("calling mdb_index_cache_sarg\n");
				mdb_index_cache_sarg(col, sarg, idx_sarg);
				g_ptr_array_add(col->idx_sarg_cache, idx_sarg);
			}
		}

		for (j=0;j<col->num_sargs;j++) {
			sarg = g_ptr_array_index (col->idx_sarg_cache, j);
			if (!mdb_test_sarg(mdb, col, sarg, offset + c_offset, c_len)) {
			/* sarg didn't match, no sense going on */
			return 0;
			}
		}
	}
	return 1;
}
/*
 * find the next entry on a page (either index or leaf). Uses state information
 * stored in the MdbIndexPage across calls.
 */
int
mdb_index_find_next_on_page(MdbHandle *mdb, MdbIndexPage *ipg)
{
	do {
		//fprintf(stdout, "%d %d\n", ipg->mask_bit, ipg->mask_byte);
		ipg->mask_bit++;
		if (ipg->mask_bit==8) {
			ipg->mask_bit=0;
			ipg->mask_pos++;
		}
		ipg->mask_byte = mdb->pg_buf[ipg->mask_pos];
		ipg->len++;
	} while (ipg->mask_pos <= 0xf8 && 
			!((1 << ipg->mask_bit) & ipg->mask_byte));

	if (ipg->mask_pos>=0xf8) 
		return 0;
	
	return ipg->len;
}
void mdb_index_page_init(MdbIndexPage *ipg)
{
	memset(ipg, 0, sizeof(MdbIndexPage));
	ipg->offset = 0xf8; /* start byte of the index entries */
	ipg->mask_pos = 0x16; 
	ipg->mask_bit=0;
	ipg->len = 0; 
}
/*
 * find the next leaf page if any given a chain. Assumes any exhausted leaf 
 * pages at the end of the chain have been peeled off before the call.
 */
MdbIndexPage *
mdb_find_next_leaf(MdbHandle *mdb, MdbIndexChain *chain)
{
	MdbIndexPage *ipg, *newipg;
	guint32 pg;
	guint passed = 0;

	ipg = &(chain->pages[chain->cur_depth - 1]);

	/*
	 * If we are at the first page deep and it's not an index page then
	 * we are simply done. (there is no page to find
	 */

	mdb_read_pg(mdb, ipg->pg);
	if (mdb->pg_buf[0]==MDB_PAGE_LEAF) 
		return ipg;

	/*
	 * apply sargs here, currently we don't
	 */
	do {
		ipg->len = 0;
		//printf("finding next on pg %lu\n", ipg->pg);
		if (!mdb_index_find_next_on_page(mdb, ipg))
			return 0;
		pg = mdb_get_int24_msb(mdb, ipg->offset + ipg->len - 3);
		//printf("Looking at pg %lu at %lu %d\n", pg, ipg->offset, ipg->len);
		ipg->offset += ipg->len;

		/*
		 * add to the chain and call this function
		 * recursively.
		 */
		chain->cur_depth++;
		if (chain->cur_depth > MDB_MAX_INDEX_DEPTH) {
			fprintf(stderr,"Error! maximum index depth of %d exceeded.  This is probably due to a programming bug, If you are confident that your indexes really are this deep, adjust MDB_MAX_INDEX_DEPTH in mdbtools.h and recompile.\n");
			exit(1);
		}
		newipg = &(chain->pages[chain->cur_depth - 1]);
		mdb_index_page_init(newipg);
		newipg->pg = pg;
		newipg = mdb_find_next_leaf(mdb, chain);
		//printf("returning pg %lu\n",newipg->pg);
		return newipg;
	} while (!passed);
	/* no more pages */
	return NULL;

}
/*
 * the main index function.
 * caller provides an index chain which is the current traversal of index
 * pages from the root page to the leaf.  Initially passed as blank, 
 * mdb_index_find_next will store it's state information here. Each invocation
 * then picks up where the last one left off, allowing us to scroll through
 * the index one by one.
 *
 * Sargs are applied here but also need to be applied on the whole row b/c
 * text columns may return false positives due to hashing and non-index
 * columns with sarg values can't be tested here.
 */
int
mdb_index_find_next(MdbHandle *mdb, MdbIndex *idx, MdbIndexChain *chain, guint32 *pg, guint16 *row)
{
	MdbIndexPage *ipg;
	int passed = 0;

	/*
	 * if it's new use the root index page (idx->first_pg)
	 */
	if (!chain->cur_depth) {
		ipg = &(chain->pages[0]);
		mdb_index_page_init(ipg);
		chain->cur_depth = 1;
		ipg->pg = idx->first_pg;
		if (!(ipg = mdb_find_next_leaf(mdb, chain)))
			return 0;
	} else {
		ipg = &(chain->pages[chain->cur_depth - 1]);
		ipg->len = 0; 
	}

	mdb_read_pg(mdb, ipg->pg);

	/*
	 * loop while the sargs don't match
	 */
	do {
		ipg->len = 0;
		/*
		 * if no more rows on this leaf, try to find a new leaf
		 */
		if (!mdb_index_find_next_on_page(mdb, ipg)) {
			//printf("page %lu finished\n",ipg->pg);
			if (chain->cur_depth==1)
				return 0;
			/* 
			 * unwind the stack until we find something or reach 
			 * the top.
			 */
			while (chain->cur_depth>1) {
				chain->cur_depth--;
				if (!(ipg = mdb_find_next_leaf(mdb, chain)))
					return 0;
				mdb_index_find_next_on_page(mdb, ipg);
			}
			if (chain->cur_depth==1)
				return 0;
		}
		*row = mdb->pg_buf[ipg->offset + ipg->len - 1];
		*pg = mdb_get_int24_msb(mdb, ipg->offset + ipg->len - 4);

		passed = mdb_index_test_sargs(mdb, idx, ipg->offset, ipg->len);

		ipg->offset += ipg->len;
	} while (!passed);

	//fprintf(stdout,"len = %d pos %d\n", ipg->len, ipg->mask_pos);
	//buffer_dump(mdb->pg_buf, ipg->offset, ipg->offset+ipg->len-1);

	return ipg->len;
}
void mdb_index_walk(MdbTableDef *table, MdbIndex *idx)
{
MdbHandle *mdb = table->entry->mdb;
int cur_pos = 0;
unsigned char marker;
MdbColumn *col;
int i;

	if (idx->num_keys!=1) return;

	mdb_read_pg(mdb, idx->first_pg);
	cur_pos = 0xf8;
	
	for (i=0;i<idx->num_keys;i++) {
		marker = mdb->pg_buf[cur_pos++];
		col=g_ptr_array_index(table->columns,idx->key_col_num[i]-1);
		printf("column %d coltype %d col_size %d (%d)\n",i,col->col_type, mdb_col_fixed_size(col), col->col_size);
	}
}
void 
mdb_index_dump(MdbTableDef *table, MdbIndex *idx)
{
	int i;
	MdbColumn *col;

	fprintf(stdout,"index number     %d\n", idx->index_num);
	fprintf(stdout,"index name       %s\n", idx->name);
	fprintf(stdout,"index first page %d\n", idx->first_pg);
	fprintf(stdout,"index rows       %d\n", idx->num_rows);
	if (idx->index_type==1) fprintf(stdout,"index is a primary key\n");
	for (i=0;i<idx->num_keys;i++) {
		col=g_ptr_array_index(table->columns,idx->key_col_num[i]-1);
		fprintf(stdout,"Column %s(%d) Sorted %s Unique: %s\n", 
			col->name,
			idx->key_col_num[i], 
			idx->key_col_order[i]==MDB_ASC ? "ascending" : "descending",
			idx->flags & MDB_IDX_UNIQUE ? "Yes" : "No"
			);
	}
	mdb_index_walk(table, idx);
}
