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


/*
** Note: This code is mostly garbage right now...just a test to parse out the
**       KKD structures.
*/

GArray *mdb_get_column_props(MdbCatalogEntry *entry, int start)
{
int pos, cnt=0;
int len, tmp, cplen;
MdbColumnProp prop;
MdbHandle *mdb = entry->mdb;

	entry->props = g_array_new(FALSE,FALSE,sizeof(MdbColumnProp));
	len = mdb_pg_get_int16(mdb,start);
	pos = start + 6;
	while (pos < start+len) {
		tmp = mdb_pg_get_int16(mdb,pos); /* length of string */
		pos += 2;
		cplen = tmp > MDB_MAX_OBJ_NAME ? MDB_MAX_OBJ_NAME : tmp;
		g_memmove(prop.name,&mdb->pg_buf[pos],cplen);
		prop.name[cplen]='\0';
		pos += tmp; 
		g_array_append_val(entry->props, prop.name);
		cnt++;
	}
	entry->num_props = cnt;
	return entry->props;
}

GHashTable *mdb_get_column_def(MdbCatalogEntry *entry, int start)
{
GHashTable *hash = NULL;
MdbHandle *mdb = entry->mdb;
MdbColumnProp prop;
int tmp, pos, col_num, val_len, i;
int len, col_type;
unsigned char c;
int end;

	fprintf(stdout,"\n data\n");
	fprintf(stdout,"-------\n");
	len = mdb_pg_get_int16(mdb,start);
	fprintf(stdout,"length = %3d\n",len);
	pos = start + 6;
	end = start + len;
	while (pos < end) {
		fprintf(stdout,"pos = %3d\n",pos);
		start = pos;
		tmp = mdb_pg_get_int16(mdb,pos); /* length of field */
		pos += 2;
		col_type = mdb_pg_get_int16(mdb,pos); /* ??? */
		pos += 2;
		col_num = 0;
		if (col_type) {
			col_num = mdb_pg_get_int16(mdb,pos); 
			pos += 2;
		}
		val_len = mdb_pg_get_int16(mdb,pos);
		pos += 2;
		fprintf(stdout,"length = %3d %04x %2d %2d ",tmp, col_type, col_num, val_len);
		for (i=0;i<val_len;i++) {
			c = mdb->pg_buf[pos+i];
			if (isprint(c))
				fprintf(stdout,"  %c",c);
			else 
				fprintf(stdout," %02x",c);

		}
		pos = start + tmp; 
		prop = g_array_index(entry->props,MdbColumnProp,col_num);
		fprintf(stdout," Property %s",prop.name); 
		fprintf(stdout,"\n");
	}
	return hash;
}
void mdb_kkd_dump(MdbCatalogEntry *entry)
{
int rows;
int kkd_start, kkd_end;
int i, tmp, pos, row_type, datapos=0;
MdbColumnProp prop;
MdbHandle *mdb = entry->mdb;
int rowid = entry->kkd_rowid;


	mdb_read_pg(mdb, entry->kkd_pg);
	rows = mdb_pg_get_int16(mdb,8);
	fprintf(stdout,"number of rows = %d\n",rows);
	kkd_start = mdb_pg_get_int16(mdb,10+rowid*2);
	fprintf(stdout,"kkd start = %d %04x\n",kkd_start,kkd_start);
	kkd_end = mdb->fmt->pg_size;
	for (i=0;i<rows;i++) {
		tmp = mdb_pg_get_int16(mdb, 10+i*2);
		if (tmp < mdb->fmt->pg_size &&
		    tmp > kkd_start &&
		    tmp < kkd_end) {
			kkd_end = tmp;
		}
	}
	fprintf(stdout,"kkd end = %d %04x\n",kkd_end,kkd_end);
	pos = kkd_start + 4; /* 4 = K K D \0 */
	while (pos < kkd_end) {
		tmp = mdb_pg_get_int16(mdb,pos);
		row_type = mdb_pg_get_int16(mdb,pos+4);
		fprintf(stdout,"row size = %3d type = 0x%02x\n",tmp,row_type);
		if (row_type==0x80)  {
			fprintf(stdout,"\nColumn Properties\n");
			fprintf(stdout,"-----------------\n");
			mdb_get_column_props(entry,pos);
			for (i=0;i<entry->num_props;i++) {
				prop = g_array_index(entry->props,MdbColumnProp,i);
				fprintf(stdout,"%3d %s\n",i,prop.name); 
			}
		}
		if (row_type==0x01) datapos = pos;
		pos += tmp;
	}
	
	if (datapos) {
		mdb_get_column_def(entry, datapos);
	}
}

