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


/*
** Note: This code is mostly garbage right now...just a test to parse out the
**       KKD structures.
*/

void mdb_kkd_dump(MDB_HANDLE *mdb, int rowid)
{
int rows;
int kkd_start, kkd_end;
int i, j, tmp, pos, row_type, hdrpos=0, datapos=0;
int len;
int col_type, col_num, val_len;
int start;
unsigned char c;

	rows = mdb_get_int16(mdb,8);
	fprintf(stdout,"number of rows = %d\n",rows);
	kkd_start = mdb_get_int16(mdb,10+rowid*2);
	fprintf(stdout,"kkd start = %d %04x\n",kkd_start,kkd_start);
	kkd_end = MDB_PGSIZE;
	for (i=0;i<rows;i++) {
		tmp = mdb_get_int16(mdb, 10+i*2);
		if (tmp<MDB_PGSIZE &&
		    tmp>kkd_start &&
		    tmp<kkd_end) {
			kkd_end = tmp;
		}
	}
	fprintf(stdout,"kkd end = %d %04x\n",kkd_end,kkd_end);
	pos = kkd_start + 4; /* 4 = K K D \0 */
	while (pos < kkd_end) {
		tmp = mdb_get_int16(mdb,pos);
		row_type = mdb_get_int16(mdb,pos+4);
		fprintf(stdout,"row size = %3d type = 0x%02x\n",tmp,row_type);
		if (row_type==0x80) hdrpos = pos;
		if (row_type==0x01) datapos = pos;
		pos += tmp;
	}
	
	if (hdrpos) {
		j=0;
		fprintf(stdout,"\nheaders\n");
		fprintf(stdout,"-------\n");
		len = mdb_get_int16(mdb,hdrpos);
		pos = hdrpos + 6;
		while (pos < hdrpos+len) {
			fprintf(stdout,"%3d ",j++);
			tmp = mdb_get_int16(mdb,pos); /* length of string */
			pos += 2;
			for (i=0;i<tmp;i++) 
				fprintf(stdout,"%c",mdb->pg_buf[pos+i]);
			fprintf(stdout,"\n");
			pos += tmp; 
		}
	}
	if (datapos) {
		fprintf(stdout,"\n data\n");
		fprintf(stdout,"-------\n");
		len = mdb_get_int16(mdb,datapos);
		pos = datapos + 6;
		while (pos < datapos+len) {
			start = pos;
			tmp = mdb_get_int16(mdb,pos); /* length of field */
			pos += 2;
			col_type = mdb_get_int16(mdb,pos); /* ??? */
			pos += 2;
			col_num = 0;
			if (col_type) {
				col_num = mdb_get_int16(mdb,pos); 
				pos += 2;
			}
			val_len = mdb_get_int16(mdb,pos);
			pos += 2;
			fprintf(stdout,"length = %3d %04x %2d %2d ",tmp, col_type, col_num, val_len);
			for (i=0;i<val_len;i++) {
				c = mdb->pg_buf[pos+i];
				if (isprint(c))
					fprintf(stdout,"  %c",c);
				else 
					fprintf(stdout," %02x",c);
	
			}
			fprintf(stdout,"\n");
			pos = start + tmp; 
		}
	}
}
main(int argc, char **argv)
{
int rows;
int i;
unsigned char buf[2048];
MDB_HANDLE *mdb;
MDB_CATALOG_ENTRY entry;


	if (argc<2) {
		fprintf(stderr,"Usage: prtable <file> <table>\n");
		exit(1);
	}
	
	mdb = mdb_open(argv[1]);

	mdb_read_pg(mdb, MDB_CATALOG_PG);
	rows = mdb_catalog_rows(mdb);

	for (i=0;i<rows;i++) {
  		if (mdb_catalog_entry(mdb, i, &entry)) {
			if (!strcmp(entry.object_name,argv[2])) {
				mdb_read_pg(mdb, entry.kkd_pg);
				mdb_kkd_dump(mdb, entry.kkd_rowid);
  			}
		}
	}

	mdb_free_handle(mdb);
}

