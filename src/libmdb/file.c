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

static size_t _mdb_read_pg(MdbHandle *mdb, unsigned char *pg_buf, unsigned long pg);

MdbHandle *mdb_open(char *filename)
{
MdbHandle *mdb;
int key[] = {0x86, 0xfb, 0xec, 0x37, 0x5d, 0x44, 0x9c, 0xfa, 0xc6, 0x5e, 0x28, 0xe6, 0x13, 0xb6};
int j,pos;

	mdb = mdb_alloc_handle();
	mdb->filename = (char *) malloc(strlen(filename)+1);
	strcpy(mdb->filename, filename);
	mdb->fd = open(filename,O_RDONLY);

	if (mdb->fd==-1) {
		/* fprintf(stderr,"Couldn't open file %s\n",filename); */
		return NULL;
	}
	if (!mdb_read_pg(mdb, 0)) {
		fprintf(stderr,"Couldn't read first page.\n");
		return NULL;
	}
	mdb->jet_version = mdb_get_int32(mdb, 0x14);
	if (mdb->jet_version == MDB_VER_JET4) {
		mdb->pg_size = 4096;
		mdb->row_count_offset = 0x0c;
	} else {
		mdb->pg_size = 2048;
		mdb->row_count_offset = 0x08;
	}

	/* get the db encryption key and xor it back to clear text */
	mdb->db_key = mdb_get_int32(mdb, 0x3e);
	mdb->db_key ^= 0xe15e01b9;


	/* get the db password located at 0x42 bytes into the file */
	for (pos=0;pos<14;pos++) {
		j = mdb_get_int32(mdb,0x42+pos);
		j ^= key[pos];
		if ( j != 0)
			mdb->db_passwd[pos] = j;
		else
			mdb->db_passwd[pos] = '\0';
        }

	return mdb;
}

void mdb_close(MdbHandle *mdb)
{
	if (mdb->fd > 0) {
		close(mdb->fd);
		if (mdb->filename) {
			free(mdb->filename);
			mdb->filename = NULL;
		}
	}
}

/* 
** mdb_read a wrapper for read that bails if anything is wrong 
*/
size_t mdb_read_pg(MdbHandle *mdb, unsigned long pg)
{
size_t len;

	len = _mdb_read_pg(mdb, mdb->pg_buf, pg);
	/* kan - reset the cur_pos on a new page read */
	mdb->cur_pos = 0; /* kan */
	return len;
}
size_t mdb_read_alt_pg(MdbHandle *mdb, unsigned long pg)
{
size_t len;

	len = _mdb_read_pg(mdb, mdb->alt_pg_buf, pg);
	return len;
}
static size_t _mdb_read_pg(MdbHandle *mdb, unsigned char *pg_buf, unsigned long pg)
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
	len = read(mdb->fd,pg_buf,mdb->pg_size);
	if (len==-1) {
		perror("read");
		return 0;
	}
	else if (len<mdb->pg_size) {
		/* fprintf(stderr,"EOF reached.\n"); */
		return 0;
	} 
	return len;
}
unsigned char mdb_get_byte(MdbHandle *mdb, int offset)
{
unsigned char c;

	c = mdb->pg_buf[offset];
	mdb->cur_pos++;
	return c;
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
float mdb_get_single(MdbHandle *mdb, int offset)
{
float f, f2;
unsigned char *c;
int i;

       if (offset <0 || offset+4 > mdb->pg_size) return -1;

       memcpy(&f, &mdb->pg_buf[offset], 4);

#ifdef WORDS_BIGENDIAN
       f2 = f;
       for (i=0; i<sizeof(f); i++) {
               *(((unsigned char *)&f)+i) =
               *(((unsigned char *)&f2)+sizeof(f)-1-i);
       }
#endif
       mdb->cur_pos+=4;
       return f;
}

double mdb_get_double(MdbHandle *mdb, int offset)
{
double d, d2;
unsigned char *c;
int i;

	if (offset <0 || offset+4 > mdb->pg_size) return -1;

	memcpy(&d, &mdb->pg_buf[offset], 8);

#ifdef WORDS_BIGENDIAN
	d2 = d;
	for (i=0; i<sizeof(d); i++) {
		*(((unsigned char *)&d)+i) =
		*(((unsigned char *)&d2)+sizeof(d)-1-i);
	}
#endif
	mdb->cur_pos+=8;
	return d;

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
