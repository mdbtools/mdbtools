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

MdbHandle *mdb_open(char *filename)
{
MdbHandle *mdb;

	mdb = mdb_alloc_handle();
	mdb->filename = (char *) malloc(strlen(filename)+1);
	strcpy(mdb->filename, filename);
	mdb->fd = open(filename,O_RDONLY);

	if (mdb->fd==-1) {
		fprintf(stderr,"Couldn't open file %s\n",filename);
		return NULL;
	}

	return mdb;
}

/* 
** mdb_read a wrapper for read that bails if anything is wrong 
*/
size_t mdb_read_pg(MdbHandle *mdb, unsigned long pg)
{
size_t len;
struct stat status;
off_t offset = pg * mdb->pg_size;

        fstat(mdb->fd, &status);
        if (status.st_size < offset) { 
                fprintf(stderr,"offset %lu is beyond EOF\n",offset);
                return 0;
        }
	lseek(mdb->fd, offset, SEEK_SET);
	len = read(mdb->fd,mdb->pg_buf,mdb->pg_size);
	if (len==-1) {
		perror("read");
		return 0;
	}
	else if (len<mdb->pg_size) {
		/* fprintf(stderr,"EOF reached.\n"); */
		return 0;
	}
 	/* kan - reset the cur_pos on a new page read */
        mdb->cur_pos = 0; /* kan */
	return len;
}
int mdb_get_int16(MdbHandle *mdb, int offset)
{
unsigned char *c;
int           i;

	if (offset < 0 || offset+2 > mdb->pg_size) return -1;
	c = &mdb->pg_buf[offset];
	i = c[1]*256+c[0];

	mdb->cur_pos+=2;
	return i;
	
}
long mdb_get_int32(MdbHandle *mdb, int offset)
{
long l;
unsigned char *c;

	if (offset <0 || offset+4 > mdb->pg_size) return -1;
	c = &mdb->pg_buf[offset];
	l =c[3]; l<<=8;
	l+=c[2]; l<<=8;
	l+=c[1]; l<<=8;
	l+=c[0];

	mdb->cur_pos+=4;
	return l;
}
int mdb_set_pos(MdbHandle *mdb, int pos)
{
	if (pos<0 || pos >= mdb->pg_size) return 0;

	mdb->cur_pos=pos;
	return pos;
}
int mdb_get_pos(MdbHandle *mdb)
{
	return mdb->cur_pos;
}
