/* MDB Tools - A library for reading MS Access database file
 * Copyright (C) 2000-2004 Brian Bruns
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
#ifdef HAVE_LIBMSWSTR
#include <mswstr/mswstr.h>
#endif

#ifdef DMALLOC
#include "dmalloc.h"
#endif

MdbIndexPage *mdb_index_read_bottom_pg(MdbHandle *mdb, MdbIndex *idx, MdbIndexChain *chain);
MdbIndexPage *mdb_chain_add_page(MdbHandle *mdb, MdbIndexChain *chain, guint32 pg);

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

/* This table doesn't really work accurately, as it is missing
 * a lot of special processing, therefore do not use!
 * This is just some kind of fallback if MSWSTR cannot be used
 * for whatever reason and may not work for most indexes, i.e. 
 * those containing hyphens etc.
 */
char idx_to_text_ling[] = {
0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,  /* 0-7      0x00-0x07 */
0x01, 0x08, 0x08, 0x08, 0x08, 0x08, 0x01, 0x01,  /* 8-15     0x08-0x0F */
0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,  /* 16-23    0x10-0x17 */
0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,  /* 24-31    0x18-0x1F */
0x07, 0x09, 0x0A, 0x0C, 0x0E, 0x10, 0x12, 0x01,  /* 32-39    0x20-0x27 */
0x14, 0x16, 0x18,  ',', 0x1A, 0x01, 0x1C, 0x1E,  /* 40-47    0x28-0x2F */
 '6',  '8',  ':',  '<',  '>',  '@',  'B',  'D',  /* 48-55    0x30-0x37 */
 'F',  'H',  ' ',  '"',  '.',  '0',  '2',  '$',  /* 56-63    0x38-0x3F */
 '&',  'J',  'L',  'M',  'O',  'Q',  'S',  'U',  /* 64-71    0x40-0x47 */
 'W',  'Y',  '[',  '\\', '^',  '`',  'b',  'd',  /* 72-79    0x48-0x4F */
 'f',  'h',  'i',  'k',  'm',  'o',  'q',  's',  /* 80-87    0x50-0x57 */
 'u',  'v',  'x',  '\'', ')',  '*',  '+',  '+',  /* 88-95    0x58-0x5F */
 '+',  'J',  'L',  'M',  'O',  'Q',  'S',  'U',  /* 96-103   0x60-0x67 */
 'W',  'Y',  '[',  '\\', '^',  '`',  'b',  'd',  /* 104-111  0x68-0x6F */
 'f',  'h',  'i',  'k',  'm',  'o',  'q',  's',  /* 112-119  0x70-0x77 */
 'u',  'v',  'x',  '+',  '+',  '+',  '+', 0x01,  /* 120-127  0x78-0x7F */
0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,  /* 128-135  0x80-0x87 */
0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,  /* 136-143  0x88-0x8F */
0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,  /* 144-151  0x90-0x97 */
0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,  /* 152-159  0x98-0x9F */
0x08,  '+',  '4',  '4',  '4',  '4',  '+',  '4',  /* 160-167  0xA0-0xA7 */
 '+',  '4',  'J',  '3',  '4', 0x01,  '4',  '+',  /* 168-175  0xA8-0xAF */
 '4',  '3',  ':',  '<',  '+',  '4',  '4',  '4',  /* 176-183  0xB0-0xB7 */
 '+',  '8',  'd',  '3',  '7',  '7',  '7',  '+',  /* 184-191  0xB8-0xBF */
 'J',  'J',  'J',  'J',  'J',  'J',  'J',  'M',  /* 192-199  0xC0-0xC7 */
 'Q',  'Q',  'Q',  'Q',  'Y',  'Y',  'Y',  'Y',  /* 200-207  0xC8-0xCF */
 'O',  'b',  'd',  'd',  'd',  'd',  'd',  '3',  /* 208-215  0xD0-0xD7 */
 'd',  'o',  'o',  'o',  'o',  'v',  'm',  'k',  /* 216-223  0xD8-0xDF */
 'J',  'J',  'J',  'J',  'J',  'J',  'J',  'M',  /* 224-231  0xE0-0xE7 */
 'Q',  'Q',  'Q',  'Q',  'Y',  'Y',  'Y',  'Y',  /* 232-239  0xE8-0xEF */
 'O',  'b',  'd',  'd',  'd',  'd',  'd',  '3',  /* 240-247  0xF0-0xF7 */
 'd',  'o',  'o',  'o',  'o',  'v',  'm',  'v',  /* 248-255  0xF8-0xFF */
};

/* JET Red (v4) Index definition byte layouts
 *
 * Based on:
 *
 * http://jabakobob.net/mdb/table-page.html
 * https://github.com/jahlborn/jackcess
 *
 * plus inspection of JET (Red) 4 databases.  (JET 3 format has fewer
 * fields -- some of the ones below omitted, and others narrower.)
 *
 * See also JET Blue (Extensible Storage Engine) format information:
 *
 * https://github.com/libyal/libesedb/blob/master/documentation/Extensible%20Storage%20Engine%20%28ESE%29%20Database%20File%20%28EDB%29%20format.asciidoc
 *
 * which is a later Microsoft embedded database format with the same
 * early base format.
 *
 * ----------------------------------------------------------------------
 * Index Column Definitions:
 * - for each "non foreign key" index (ie pidx->index_type!=2), a list
 *   of columns indexed
 *
 * Repeated table->num_real_idxs times:
 *
 * Offset	Bytes 	Meaning
 * 0x0000	4	UNKNOWN; seems to be type marker, usually 1923 or 0
 *
 * 0x0004       2       Column 1 ID
 * 0x0006       1       Column 1 Flags
 * 0x0007       2       Column 2 ID
 * 0x0009       1       Column 2 Flags
 * 0x000A       2       Column 3 ID
 * 0x000C       1       Column 3 Flags
 * 0x000D       2       Column 4 ID
 * 0x000F       1       Column 4 Flags
 * 0x0010       2       Column 5 ID
 * 0x0012       1       Column 5 Flags
 * 0x0013       2       Column 6 ID
 * 0x0015       1       Column 6 Flags
 * 0x0016       2       Column 7 ID
 * 0x0018       1       Column 7 Flags
 * 0x0019       2       Column 8 ID
 * 0x001B       1       Column 8 Flags
 * 0x001C       2       Column 9 ID
 * 0x001E       1       Column 9 Flags
 * 0x001F       2       Column 10 ID
 * 0x0021       1       Column 10 Flags
 *
 * 0x0022	1	Usage Map row
 * 0x0023	3	Usage Map page (24-bit)
 * 0x0026	4	First index page
 * 0x002A	4	UNKNOWN
 * 0x002E	2	Index Flags
 * 0x0030	4	UNKNOWN; seems to always be 0
 * 0x0034
 *
 * Column ID of 0xFFFF (-1) means "not used" or "end of used columns".
 * Column Flags:
 * - 0x01 = Ascending
 *
 * Index Flags:
 * - 0x0001 = Unique index
 * - 0x0002 = Ignore NULLs
 * - 0x0008 = Required Index
 *
 * ----------------------------------------------------------------------
 * Index Definitions
 * - for each index (normal, primary key, foreign key), details on the
 *   index.
 *
 * - this appears to be the union of information required for normal/
 *   primary key indexes, and the information required for foreign key
 *   indexes.
 *
 * Repeated table->num_idxs times:
 *
 * Offset	Bytes 	Meaning
 * 0x0000	4	UNKNOWN; apparently a type marker, usually 1625 or 0
 * 0x0004	4	Logical Index Number
 * 0x0008	4	Index Column Definition Entry
 * 0x000C	1	FK Index Type
 * 0x000D	4	FK Index Number
 * 0x0011	4	FK Index Table Page Number
 * 0x0015	1	Flags: Update Action
 * 0x0016	1	Flags: Delete Action
 * 0x0017	1	Index Type
 * 0x0018	4	UNKNNOWN; seems to always be 0
 * 0x001B
 *
 * Where Index Type is:
 * 0x01 = normal
 * 0x01 = primary key
 * 0x02 = foreign key index reference
 */

/* Debugging helper to dump out raw hex values of index definition */
/*
static void hexdump(unsigned char *tmpbuf, int size) {
	mdb_buffer_dump(tmpbuf, 0, size);
}
*/


GPtrArray *
mdb_read_indices(MdbTableDef *table)
{
	MdbCatalogEntry *entry = table->entry;
	MdbHandle *mdb = entry->mdb;
	MdbFormatConstants *fmt = mdb->fmt;
	MdbIndex *pidx;
	unsigned int i, j, k;
	int key_num, col_num, cleaned_col_num;
	int cur_pos, name_sz, idx2_sz, type_offset;
	int index_start_pg = mdb->cur_pg;
	gchar *tmpbuf;

	table->indices = g_ptr_array_new();

	if (IS_JET3(mdb)) {
		cur_pos = table->index_start + 39 * table->num_real_idxs;
		idx2_sz = 20;
		type_offset = 19;
	} else {
		cur_pos = table->index_start + 52 * table->num_real_idxs;
		idx2_sz = 28;
		type_offset = 23;
	}

	/* Read in the definitions of table indexes, into table->indices */

	/* num_real_idxs should be the number of indexes other than type 2.
	 * It's not always the case. Happens on Northwind Orders table.
	 */
	//fprintf(stderr, "num_idxs:%d num_real_idxs:%d\n", table->num_idxs, table->num_real_idxs);

	table->num_real_idxs = 0;
	tmpbuf = (gchar *) g_malloc(idx2_sz);
	for (i=0;i<table->num_idxs;i++) {
		read_pg_if_n(mdb, tmpbuf, &cur_pos, idx2_sz);
                //fprintf(stderr, "Index defn: ");
                //hexdump((unsigned char *)tmpbuf, idx2_sz);
		pidx = (MdbIndex *) g_malloc0(sizeof(MdbIndex));
		pidx->table = table;
		pidx->index_num = mdb_get_int16(tmpbuf, 4);
		pidx->index_type = tmpbuf[type_offset];
		g_ptr_array_add(table->indices, pidx);
		/*
		{
			gint32 idx_marker = mdb_get_int32(tmpbuf, 0);
		        gint32 index_col_def_num = mdb_get_int16(tmpbuf, 8);
			gint8 rel_idx_type = tmpbuf[0x0c];
			gint32 rel_idx_number = mdb_get_int32(tmpbuf, 0x0d);
			gint32 rel_idx_page = mdb_get_int32(tmpbuf, 0x11);
			gint8 update_action_flags = tmpbuf[0x15];
			gint8 delete_action_flags = tmpbuf[0x16];
			fprintf(stderr, "idx #%d: num2:%d num3:%d type:%d\n", i, pidx->index_num, index_col_def_num, pidx->index_type);
			fprintf(stderr, "idx #%d: %d %d %d %d %d/%d\n", i, idx_marker, rel_idx_type, rel_idx_number, rel_idx_page, update_action_flags, delete_action_flags);
		}*/
		if (pidx->index_type!=2)
			table->num_real_idxs++;
	}
	//fprintf(stderr, "num_idxs:%d num_real_idxs:%d\n", table->num_idxs, table->num_real_idxs);
	g_free(tmpbuf);

	/* Pick up the names of each index */
	for (i=0;i<table->num_idxs;i++) {
		pidx = g_ptr_array_index (table->indices, i);
		if (IS_JET3(mdb)) {
			name_sz=read_pg_if_8(mdb, &cur_pos);
		} else {
			name_sz=read_pg_if_16(mdb, &cur_pos);
		}
		tmpbuf = g_malloc(name_sz);
		read_pg_if_n(mdb, tmpbuf, &cur_pos, name_sz);
		mdb_unicode2ascii(mdb, tmpbuf, name_sz, pidx->name, MDB_MAX_OBJ_NAME); 
		g_free(tmpbuf);
		//fprintf(stderr, "index %d type %d name %s\n", pidx->index_num, pidx->index_type, pidx->name);
	}

	/* Pick up the column definitions for normal/primary key indexes */
	/* NOTE: Match should possibly be by index_col_def_num, rather
         * than index_num; but in files encountered both seem to be the
         * same (so left with index_num until a counter example is found).
         */
	mdb_read_alt_pg(mdb, entry->table_pg);
	mdb_read_pg(mdb, index_start_pg);
	cur_pos = table->index_start;
	for (i=0;i<table->num_real_idxs;i++) {
		/*	Debugging print out, commented out
		{
			gchar *tmpbuf = (gchar *) g_malloc(0x34);
			int now_pos = cur_pos;
			read_pg_if_n(mdb, tmpbuf, &now_pos, 0x34);
			fprintf(stderr, "Index defn: ");
			hexdump((unsigned char *)tmpbuf, 0x34);
			g_free(tmpbuf);
		}*/

		if (!IS_JET3(mdb)) cur_pos += 4;
		/* look for index number i */
		for (j=0; j<table->num_idxs; ++j) {
			pidx = g_ptr_array_index (table->indices, j);
			if (pidx->index_type!=2 && (unsigned int)pidx->index_num==i)
				break;
		}
		if (j==table->num_idxs) {
			fprintf(stderr, "ERROR: can't find index #%d.\n", i);
			continue;
		}
		//fprintf(stderr, "index %d #%d (%s) index_type:%d\n", i, pidx->index_num, pidx->name, pidx->index_type);

		pidx->num_rows = mdb_get_int32(mdb->alt_pg_buf, 
				fmt->tab_cols_start_offset +
				(pidx->index_num*fmt->tab_ridx_entry_size));
		/*
		fprintf(stderr, "ridx block1 i:%d data1:0x%08x data2:0x%08x\n",
			i,
			(unsigned int)mdb_get_int32(mdb->pg_buf,
				fmt->tab_cols_start_offset + pidx->index_num * fmt->tab_ridx_entry_size),
			(unsigned int)mdb_get_int32(mdb->pg_buf,
				fmt->tab_cols_start_offset + pidx->index_num * fmt->tab_ridx_entry_size +4));
		fprintf(stderr, "pidx->num_rows:%d\n", pidx->num_rows);*/

		/* Read columns in each index */
		key_num=0;
		for (j=0;j<MDB_MAX_IDX_COLS;j++) {
			col_num=read_pg_if_16(mdb,&cur_pos);
			if (col_num == 0xFFFF) {
				cur_pos++;
				continue;
			}
			/* here we have the internal column number that does not
			 * always match the table columns because of deletions */
			cleaned_col_num = -1;
			for (k=0; k<table->num_cols; k++) {
				MdbColumn *col = g_ptr_array_index(table->columns,k);
				if (col->col_num == col_num) {
					cleaned_col_num = k;
					break;
				}
			}
			if (cleaned_col_num==-1) {
				fprintf(stderr, "CRITICAL: can't find column with internal id %d in index %s\n",
					col_num, pidx->name);
				cur_pos++;
				continue;
			}
			/* set column number to a 1 based column number and store */
			pidx->key_col_num[key_num] = cleaned_col_num + 1;
			pidx->key_col_order[key_num] =
				(read_pg_if_8(mdb, &cur_pos)) ? MDB_ASC : MDB_DESC;
			//fprintf(stderr, "component %d using column #%d (internally %d)\n", j, cleaned_col_num,  col_num);
			key_num++;
		}
		pidx->num_keys = key_num;

		if (0)  // DEBUGGING ONLY
		{
			gint32 usage_map = read_pg_if_32(mdb, &cur_pos);
			fprintf(stderr, "pidx->unknown_pre_first_pg:0x%08x\n", usage_map);
		} else {
			cur_pos += 4;    // Skip Usage map information
		}
		pidx->first_pg = read_pg_if_32(mdb, &cur_pos);

		if (!IS_JET3(mdb)) cur_pos += 4;

		pidx->flags = read_pg_if_8(mdb, &cur_pos);
		//fprintf(stderr, "pidx->first_pg:%d pidx->flags:0x%02x\n",	pidx->first_pg, pidx->flags);
		if (!IS_JET3(mdb)) cur_pos += 5;
	}
	return NULL;
}
void
mdb_index_hash_text(MdbHandle *mdb, char *text, char *hash)
{
	unsigned int k, len=strlen(text);
	char *transtbl=NULL;

	if (!IS_JET3(mdb))
	{
#ifdef __MSWSTR_H__
		char *out_ptr = alloca((len+1)*2);
		int i;
		// mdb_ascii2unicode doesn't work, we don't want unicode compression!
		for (i=0; i<len+1; i++) {
			out_ptr[i*2] = text[i];
			out_ptr[i*2+1] = 0;
		}
        if (!(k=DBLCMapStringW(MAKELCID(MAKELANGID(LANG_ENGLISH, SUBLANG_DEFAULT), 0),
                LCMAP_LINGUISTIC_CASING | LCMAP_SORTKEY | NORM_IGNORECASE | NORM_IGNOREKANATYPE | NORM_IGNOREWIDTH,
                (WCHAR*)out_ptr, len, (LPBYTE)hash, len*2)))
		{
			len++;
#endif
		transtbl = idx_to_text_ling;
#ifdef __MSWSTR_H__
		}
#endif
	}
	else
	{
		transtbl = idx_to_text;
	}
	if (transtbl)
	{
		for (k=0;k<len;k++) {
			unsigned char c = ((unsigned char *)(text))[k];
			hash[k] = transtbl[c];
			if (!(hash[k])) fprintf(stderr, 
					"No translation available for %02x %d\n", c, c);
		}
		hash[len]='\0';
	}
	//printf ("mdb_index_hash_text %s -> %s (%d -> %d)\n", text, hash, len, k);
}
/*
 * reverse the order of the column for hashing
 */
void
mdb_index_swap_n(unsigned char *src, int sz, unsigned char *dest)
{
	int i, j = 0;

	for (i = sz-1; i >= 0; i--) {
		dest[j++] = src[i];
	}
}
void 
mdb_index_cache_sarg(MdbColumn *col, MdbSarg *sarg, MdbSarg *idx_sarg)
{
	//guint32 cache_int;
	unsigned char *c;

	switch (col->col_type) {
		case MDB_TEXT:
		mdb_index_hash_text(col->table->mdbidx, sarg->value.s, idx_sarg->value.s);
		break;

		case MDB_LONGINT:
		idx_sarg->value.i = GUINT32_SWAP_LE_BE(sarg->value.i);
		//cache_int = sarg->value.i * -1;
		c = (unsigned char *) &(idx_sarg->value.i);
		c[0] |= 0x80;
		//printf("int %08x %02x %02x %02x %02x\n", sarg->value.i, c[0], c[1], c[2], c[3]);
		break;	

		case MDB_INT:
		break;	

		default:
		break;	
	}
}
#if 0
int 
mdb_index_test_sarg(MdbHandle *mdb, MdbColumn *col, MdbSarg *sarg, int offset, int len)
{
char tmpbuf[256];
int lastchar;

	switch (col->col_type) {
		case MDB_BYTE:
			return mdb_test_int(sarg, mdb_pg_get_byte(mdb, offset));
			break;
		case MDB_INT:
			return mdb_test_int(sarg, mdb_pg_get_int16(mdb, offset));
			break;
		case MDB_LONGINT:
			return mdb_test_int(sarg, mdb_pg_get_int32(mdb, offset));
			break;
		case MDB_TEXT:
			strncpy(tmpbuf, &mdb->pg_buf[offset],255);
			lastchar = len > 255 ? 255 : len;
			tmpbuf[lastchar]='\0';
			return mdb_test_string(sarg, tmpbuf);
		default:
			fprintf(stderr, "Calling mdb_test_sarg on unknown type.  Add code to mdb_test_sarg() for type %d\n",col->col_type);
			break;
	}
	return 1;
}
#endif
int
mdb_index_test_sargs(MdbHandle *mdb, MdbIndex *idx, char *buf, int len)
{
	unsigned int i, j;
	MdbColumn *col;
	MdbTableDef *table = idx->table;
	MdbSarg *idx_sarg;
	MdbSarg *sarg;
	MdbField field;
	MdbSargNode node;
	//int c_offset = 0, 
	int c_len;

	//fprintf(stderr,"mdb_index_test_sargs called on ");
	//mdb_buffer_dump(buf, 0, len);
	//fprintf(stderr,"\n");
	for (i=0;i<idx->num_keys;i++) {
		//c_offset++; /* the per column null indicator/flags */
		col=g_ptr_array_index(table->columns,idx->key_col_num[i]-1);
		/*
		 * This will go away eventually
		 */
		if (col->col_type==MDB_TEXT) {
			//c_len = strlen(&mdb->pg_buf[offset + c_offset]);
			c_len = strlen(buf);
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
			/* XXX - kludge */
			node.op = sarg->op;
			node.value = sarg->value;
			//field.value = &mdb->pg_buf[offset + c_offset];
			field.value = buf;
		       	field.siz = c_len;
		       	field.is_null = FALSE;
			/* In Jet 4 Index text hashes don't need to be converted from Unicode */
			if (!IS_JET3(mdb) && col->col_type == MDB_TEXT)
			{
				if (!mdb_test_string(&node, buf)) return 0;
			}
			else if (!mdb_test_sarg(mdb, col, &node, &field)) {
				/* sarg didn't match, no sense going on */
				return 0;
			}
		}
	}
	return 1;
}
/*
 * pack the pages bitmap
 */
int
mdb_index_pack_bitmap(MdbHandle *mdb, MdbIndexPage *ipg)
{
	int mask_bit = 0;
	int mask_pos = IS_JET3(mdb)?0x16:0x1b;
	int mask_byte = 0;
	int elem = 0;
	int len, start, i;

	start = ipg->idx_starts[elem++];

	while (start) {
		//fprintf(stdout, "elem %d is %d\n", elem, ipg->idx_starts[elem]);
		len = ipg->idx_starts[elem] - start;
		//fprintf(stdout, "len is %d\n", len);
		for (i=0; i < len; i++) {
			mask_bit++;
			if (mask_bit==8) {
				mask_bit=0;
				mdb->pg_buf[mask_pos++] = mask_byte;
				mask_byte = 0;
			}
			/* upon reaching the len, set the bit */
		}
		mask_byte = (1 << mask_bit) | mask_byte;
		//fprintf(stdout, "mask byte is %02x at %d\n", mask_byte, mask_pos);
		start = ipg->idx_starts[elem++];
	}
	/* flush the last byte if any */
	mdb->pg_buf[mask_pos++] = mask_byte;
	/* remember to zero the rest of the bitmap */
	for (i = mask_pos; i < 0xf8; i++) {
		mdb->pg_buf[mask_pos++] = 0;
	}
	return 0;
}
/*
 * unpack the pages bitmap
 */
int
mdb_index_unpack_bitmap(MdbHandle *mdb, MdbIndexPage *ipg)
{
	int mask_bit = 0;
	int mask_pos = IS_JET3(mdb)?0x16:0x1b;
	int mask_byte;
	int jet_start = IS_JET3(mdb)?0xf8:0x1e0;
	int start = jet_start;
	int elem = 0;
	int len = 0;

	ipg->idx_starts[elem++]=start;

	//fprintf(stdout, "Unpacking index page %lu\n", ipg->pg);
	do {
		len = 0;
		do {
			mask_bit++;
			if (mask_bit==8) {
				mask_bit=0;
				mask_pos++;
			}
			mask_byte = mdb->pg_buf[mask_pos];
			len++;
		} while (mask_pos <= jet_start && !((1 << mask_bit) & mask_byte));
		//fprintf(stdout, "%d %d %d %d\n", mask_pos, mask_bit, mask_byte, len);

		start += len;
		if (mask_pos < jet_start) ipg->idx_starts[elem++]=start;

	} while (mask_pos < jet_start);

	/* if we zero the next element, so we don't pick up the last pages starts*/
	ipg->idx_starts[elem]=0;

	return elem;
}
/*
 * find the next entry on a page (either index or leaf). Uses state information
 * stored in the MdbIndexPage across calls.
 */
int
mdb_index_find_next_on_page(MdbHandle *mdb, MdbIndexPage *ipg)
{
	if (!ipg->pg) return 0;

	/* if this page has not been unpacked to it */
	if (!ipg->idx_starts[0]){
		//fprintf(stdout, "Unpacking page %d\n", ipg->pg);
		mdb_index_unpack_bitmap(mdb, ipg);
	}

	
	if (ipg->idx_starts[ipg->start_pos + 1]==0) return 0; 
	ipg->len = ipg->idx_starts[ipg->start_pos+1] - ipg->idx_starts[ipg->start_pos];
	ipg->start_pos++;
	//fprintf(stdout, "Start pos %d\n", ipg->start_pos);

	return ipg->len;
}
void mdb_index_page_reset(MdbHandle *mdb, MdbIndexPage *ipg)
{
	ipg->offset = IS_JET3(mdb)?0xf8:0x1e0; /* start byte of the index entries */
	ipg->start_pos=0;
	ipg->len = 0; 
	ipg->idx_starts[0]=0;
}
void mdb_index_page_init(MdbHandle *mdb, MdbIndexPage *ipg)
{
	memset(ipg, 0, sizeof(MdbIndexPage));
	mdb_index_page_reset(mdb, ipg);
}
/*
 * find the next leaf page if any given a chain. Assumes any exhausted leaf 
 * pages at the end of the chain have been peeled off before the call.
 */
MdbIndexPage *
mdb_find_next_leaf(MdbHandle *mdb, MdbIndex *idx, MdbIndexChain *chain)
{
	MdbIndexPage *ipg, *newipg;
	guint32 pg;
	guint passed = 0;

	ipg = mdb_index_read_bottom_pg(mdb, idx, chain);

	/*
	 * If we are at the first page deep and it's not an index page then
	 * we are simply done. (there is no page to find
	 */

	if (mdb->pg_buf[0]==MDB_PAGE_LEAF) {
		/* Indexes can have leaves at the end that don't appear
		 * in the upper tree, stash the last index found so
		 * we can follow it at the end.  */
		chain->last_leaf_found = ipg->pg;
		return ipg;
	}

	/*
	 * apply sargs here, currently we don't
	 */
	do {
		ipg->len = 0;
		//printf("finding next on pg %lu\n", ipg->pg);
		if (!mdb_index_find_next_on_page(mdb, ipg)) {
			//printf("find_next_on_page returned 0\n");
			return 0;
		}
		pg = mdb_get_int32_msb(mdb->pg_buf, ipg->offset + ipg->len - 3) >> 8;
		//printf("Looking at pg %lu at %lu %d\n", pg, ipg->offset, ipg->len);
		ipg->offset += ipg->len;

		/*
		 * add to the chain and call this function
		 * recursively.
		 */
		newipg = mdb_chain_add_page(mdb, chain, pg);
		newipg = mdb_find_next_leaf(mdb, idx, chain);
		//printf("returning pg %lu\n",newipg->pg);
		return newipg;
	} while (!passed);
	/* no more pages */
	return NULL;

}
MdbIndexPage *
mdb_chain_add_page(MdbHandle *mdb, MdbIndexChain *chain, guint32 pg)
{
	MdbIndexPage *ipg;

	chain->cur_depth++;
	if (chain->cur_depth > MDB_MAX_INDEX_DEPTH) {
		fprintf(stderr,"Error! maximum index depth of %d exceeded.  This is probably due to a programming bug, If you are confident that your indexes really are this deep, adjust MDB_MAX_INDEX_DEPTH in mdbtools.h and recompile.\n", MDB_MAX_INDEX_DEPTH);
		exit(1);
	}
	ipg = &(chain->pages[chain->cur_depth - 1]);
	mdb_index_page_init(mdb, ipg);
	ipg->pg = pg;

	return ipg;
}
/*
 * returns the bottom page of the IndexChain, if IndexChain is empty it 
 * initializes it by reading idx->first_pg (the root page)
 */
MdbIndexPage *
mdb_index_read_bottom_pg(MdbHandle *mdb, MdbIndex *idx, MdbIndexChain *chain)
{
	MdbIndexPage *ipg;

	/*
	 * if it's new use the root index page (idx->first_pg)
	 */
	if (!chain->cur_depth) {
		ipg = &(chain->pages[0]);
		mdb_index_page_init(mdb, ipg);
		chain->cur_depth = 1;
		ipg->pg = idx->first_pg;
		if (!(ipg = mdb_find_next_leaf(mdb, idx, chain)))
			return 0;
	} else {
		ipg = &(chain->pages[chain->cur_depth - 1]);
		ipg->len = 0; 
	}

	mdb_read_pg(mdb, ipg->pg);

	return ipg;
}
/*
 * unwind the stack and search for new leaf node
 */
MdbIndexPage *
mdb_index_unwind(MdbHandle *mdb, MdbIndex *idx, MdbIndexChain *chain)
{
	MdbIndexPage *ipg;

	//printf("page %lu finished\n",ipg->pg);
	if (chain->cur_depth==1) {
		//printf("cur_depth == 1 we're out\n");
		return NULL;
	}
	/* 
	* unwind the stack until we find something or reach 
	* the top.
	*/
	ipg = NULL;
	while (chain->cur_depth>1 && ipg==NULL) {
		//printf("chain depth %d\n", chain->cur_depth);
		chain->cur_depth--;
		ipg = mdb_find_next_leaf(mdb, idx, chain);
		if (ipg) mdb_index_find_next_on_page(mdb, ipg);
	}
	if (chain->cur_depth==1) {
		//printf("last leaf %lu\n", chain->last_leaf_found);
		return NULL;
	}
	return ipg;
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
	int idx_sz;
	int idx_start = 0;
	unsigned short compress_bytes;
	MdbColumn *col;
	guint32 pg_row;

	ipg = mdb_index_read_bottom_pg(mdb, idx, chain);

	/*
	 * loop while the sargs don't match
	 */
	do {
		ipg->len = 0;
		/*
		 * if no more rows on this leaf, try to find a new leaf
		 */
		if (!mdb_index_find_next_on_page(mdb, ipg)) {
			if (!chain->clean_up_mode) {
				if (ipg->rc==1 || !(ipg = mdb_index_unwind(mdb, idx, chain)))
					chain->clean_up_mode = 1;
			}
			if (chain->clean_up_mode) {
				//fprintf(stdout,"in cleanup mode\n");

				if (!chain->last_leaf_found) return 0;
				mdb_read_pg(mdb, chain->last_leaf_found);
				chain->last_leaf_found = mdb_get_int32(
					mdb->pg_buf, 0x0c);
				//printf("next leaf %lu\n", chain->last_leaf_found);
				mdb_read_pg(mdb, chain->last_leaf_found);
				/* reuse the chain for cleanup mode */
				chain->cur_depth = 1;
				ipg = &chain->pages[0];
				mdb_index_page_init(mdb, ipg);
				ipg->pg = chain->last_leaf_found;
				//printf("next on page %d\n",
				if (!mdb_index_find_next_on_page(mdb, ipg))
					return 0;
			}
		}
		pg_row = mdb_get_int32_msb(mdb->pg_buf, ipg->offset + ipg->len - 4);
		*row = pg_row & 0xff;
		*pg = pg_row >> 8;
		//printf("row = %d pg = %lu ipg->pg = %lu offset = %lu len = %d\n", *row, *pg, ipg->pg, ipg->offset, ipg->len);
		col=g_ptr_array_index(idx->table->columns,idx->key_col_num[0]-1);
		idx_sz = mdb_col_fixed_size(col);
		/* handle compressed indexes, single key indexes only? */
		if (idx_sz<0) idx_sz = ipg->len - (ipg->start_pos==1?5:4); // Length from Index - the 4 trailing bytes (data page/row), Skip flags on first page
		compress_bytes = mdb_get_int16(mdb->pg_buf, IS_JET3(mdb)?0x14:0x18);
		if (idx->num_keys==1 && idx_sz>0 && compress_bytes > 1 && ipg->start_pos>1 /*ipg->len - 4 < idx_sz*/) {
			//printf("short index found\n");
			//mdb_buffer_dump(ipg->cache_value, 0, idx_sz);
			memcpy(&ipg->cache_value[compress_bytes-1], &mdb->pg_buf[ipg->offset], ipg->len);
			//mdb_buffer_dump(ipg->cache_value, 0, idx_sz);
		} else {
			idx_start = ipg->offset + (ipg->len - 4 - idx_sz);
			memcpy(ipg->cache_value, &mdb->pg_buf[idx_start], idx_sz);
		}

		//idx_start = ipg->offset + (ipg->len - 4 - idx_sz);
		passed = mdb_index_test_sargs(mdb, idx, (char *)(ipg->cache_value), idx_sz);
		if (passed) ipg->rc=1; else if (ipg->rc) return 0;

		ipg->offset += ipg->len;
	} while (!passed);

	//fprintf(stdout,"len = %d pos %d\n", ipg->len, ipg->mask_pos);
	//mdb_buffer_dump(mdb->pg_buf, ipg->offset, ipg->len);

	return ipg->len;
}
/*
 * XXX - FIX ME
 * This function is grossly inefficient.  It scans the entire index building 
 * an IndexChain to a specific row.  We should be checking the index pages 
 * for matches against the indexed fields to find the proper leaf page, but
 * getting it working first and then make it fast!
 */
int 
mdb_index_find_row(MdbHandle *mdb, MdbIndex *idx, MdbIndexChain *chain, guint32 pg, guint16 row)
{
	MdbIndexPage *ipg;
	int passed = 0;
	guint32 pg_row = (pg << 8) | (row & 0xff);
	guint32 datapg_row;

	ipg = mdb_index_read_bottom_pg(mdb, idx, chain);

	do {
		ipg->len = 0;
		/*
		 * if no more rows on this leaf, try to find a new leaf
		 */
		if (!mdb_index_find_next_on_page(mdb, ipg)) {
			/* back to top? We're done */
			if (chain->cur_depth==1)
				return 0;

			/* 
			 * unwind the stack until we find something or reach 
			 * the top.
			 */
			while (chain->cur_depth>1) {
				chain->cur_depth--;
				if (!(ipg = mdb_find_next_leaf(mdb, idx, chain)))
					return 0;
				mdb_index_find_next_on_page(mdb, ipg);
			}
			if (chain->cur_depth==1)
				return 0;
		}
		/* test row and pg */
		datapg_row = mdb_get_int32_msb(mdb->pg_buf, ipg->offset + ipg->len - 4);
		if (pg_row == datapg_row) {
			passed = 1;
		}
		ipg->offset += ipg->len;
	} while (!passed);

	/* index chain from root to leaf should now be in "chain" */
	return 1;
}

void mdb_index_walk(MdbTableDef *table, MdbIndex *idx)
{
/*
	MdbHandle *mdb = table->entry->mdb;
	int cur_pos = 0;
	unsigned char marker;
	MdbColumn *col;
	unsigned int i;

	if (idx->num_keys!=1) return;

	mdb_read_pg(mdb, idx->first_pg);
	cur_pos = 0xf8;
	
	for (i=0;i<idx->num_keys;i++) {
		marker = mdb->pg_buf[cur_pos++];
		col=g_ptr_array_index(table->columns,idx->key_col_num[i]-1);
		//printf("column %d coltype %d col_size %d (%d)\n",i,col->col_type, mdb_col_fixed_size(col), col->col_size);
	}
*/
}
void 
mdb_index_dump(MdbTableDef *table, MdbIndex *idx)
{
	unsigned int i;
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
/*
 * compute_cost tries to assign a cost to a given index using the sargs 
 * available in this query.
 *
 * Indexes with no matching sargs are assigned 0
 * Unique indexes are preferred over non-uniques
 * Operator preference is equal, like, isnull, others 
 */
int mdb_index_compute_cost(MdbTableDef *table, MdbIndex *idx)
{
	unsigned int i;
	MdbColumn *col;
	MdbSarg *sarg = NULL;
	int not_all_equal = 0;

	if (!idx->num_keys) return 0;
	if (idx->num_keys > 1) {
		for (i=0;i<idx->num_keys;i++) {
			col=g_ptr_array_index(table->columns,idx->key_col_num[i]-1);
			if (col->sargs) sarg = g_ptr_array_index (col->sargs, 0);
			if (!sarg || sarg->op != MDB_EQUAL) not_all_equal++;
		}
	}

	col=g_ptr_array_index(table->columns,idx->key_col_num[0]-1);
	/* 
	 * if this is the first key column and there are no sargs,
	 * then this index is useless.
	 */
	if (!col->num_sargs) return 0;

	sarg = g_ptr_array_index (col->sargs, 0);

	/*
	 * a like with a wild card first is useless as a sarg */
	if (sarg->op == MDB_LIKE && sarg->value.s[0]=='%')
		return 0;

	/*
	 * this needs a lot of tweaking.
	 */
	if (idx->flags & MDB_IDX_UNIQUE) {
		if (idx->num_keys == 1) {
			//printf("op is %d\n", sarg->op);
			switch (sarg->op) {
				case MDB_EQUAL:
					return 1; break;
				case MDB_LIKE:
					return 4; break;
				case MDB_ISNULL:
					return 12; break;
				default:
					return 8; break;
			}
		} else {
			switch (sarg->op) {
				case MDB_EQUAL:
					if (not_all_equal) return 2; 
					else return 1;
					break;
				case MDB_LIKE:
					return 6; break;
				case MDB_ISNULL:
					return 12; break;
				default:
					return 9; break;
			}
		}
	} else {
		if (idx->num_keys == 1) {
			switch (sarg->op) {
				case MDB_EQUAL:
					return 2; break;
				case MDB_LIKE:
					return 5; break;
				case MDB_ISNULL:
					return 12; break;
				default:
					return 10; break;
			}
		} else {
			switch (sarg->op) {
				case MDB_EQUAL:
					if (not_all_equal) return 3; 
					else return 2;
					break;
				case MDB_LIKE:
					return 7; break;
				case MDB_ISNULL:
					return 12; break;
				default:
					return 11; break;
			}
		}
	}
	return 0;
}
/*
 * choose_index runs mdb_index_compute_cost for each available index and picks
 * the best.
 *
 * Returns strategy to use (table scan, or index scan)
 */
MdbStrategy 
mdb_choose_index(MdbTableDef *table, int *choice)
{
	unsigned int i;
	MdbIndex *idx;
	int cost = 0;
	int least = 99;

	*choice = -1;
	for (i=0;i<table->num_idxs;i++) {
		idx = g_ptr_array_index (table->indices, i);
		cost = mdb_index_compute_cost(table, idx);
		//printf("cost for %s is %d\n", idx->name, cost);
		if (cost && cost < least) {
			least = cost;
			*choice = i;
		}
	}
	/* and the winner is: *choice */
	if (least==99) return MDB_TABLE_SCAN;
	return MDB_INDEX_SCAN;
}
void
mdb_index_scan_init(MdbHandle *mdb, MdbTableDef *table)
{
	int i;

	if (mdb_get_option(MDB_USE_INDEX) && mdb_choose_index(table, &i) == MDB_INDEX_SCAN) {
		table->strategy = MDB_INDEX_SCAN;
		table->scan_idx = g_ptr_array_index (table->indices, i);
		table->chain = g_malloc0(sizeof(MdbIndexChain));
		table->mdbidx = mdb_clone_handle(mdb);
		mdb_read_pg(table->mdbidx, table->scan_idx->first_pg);
		//printf("best index is %s\n",table->scan_idx->name);
	}
	//printf("TABLE SCAN? %d\n", table->strategy);
}
void 
mdb_index_scan_free(MdbTableDef *table)
{
	if (table->chain) {
		g_free(table->chain);
		table->chain = NULL;
	}
	if (table->mdbidx) {
		mdb_close(table->mdbidx);
		table->mdbidx = NULL;
	}
}

void mdb_free_indices(GPtrArray *indices)
{
	guint i;

	if (!indices) return;
	for (i=0; i<indices->len; i++)
		g_free (g_ptr_array_index(indices, i));
	g_ptr_array_free(indices, TRUE);
}
