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

#ifdef DMALLOC
#include "dmalloc.h"
#endif

/*
typedef struct {
	int		pg_size;
	guint16		row_count_offset; 
	guint16		tab_num_rows_offset;
	guint16		tab_num_cols_offset;
	guint16		tab_num_idxs_offset;
	guint16		tab_num_ridxs_offset;
	guint16		tab_usage_map_offset;
	guint16		tab_first_dpg_offset;
	guint16		tab_cols_start_offset;
	guint16		tab_ridx_entry_size;
	guint16		col_fixed_offset;
	guint16		col_size_offset;
	guint16		col_num_offset;
	guint16		tab_col_entry_size;
	guint16         tab_free_map_offset;
	guint16         tab_col_offset_var;
	guint16         tab_col_offset_fixed;
	guint16         tab_row_col_num_offset;
} MdbFormatConstants; 
*/
MdbFormatConstants MdbJet4Constants = {
	4096, 0x0c, 16, 45, 47, 51, 55, 56, 63, 12, 15, 23, 5, 25, 59, 7, 21, 9
};
MdbFormatConstants MdbJet3Constants = {
	2048, 0x08, 12, 25, 27, 31, 35, 36, 43, 8, 13, 16, 1, 18, 39, 3, 14, 5
};

static ssize_t _mdb_read_pg(MdbHandle *mdb, unsigned char *pg_buf, unsigned long pg);

/**
 * mdb_find_file:
 * @filename: path to MDB (database) file
 *
 * Finds and returns the absolute path to an MDB file.  Function will first try
 * to fstat file as passed, then search through the $MDBPATH if not found.
 *
 * Return value: gchar pointer to absolute path. Caller is responsible for
 * freeing.
 **/

static gchar *mdb_find_file(const char *file_name)
{
	struct stat status;
	gchar *mdbpath, **dir, *tmpfname;
	unsigned int i = 0;

	/* try the provided file name first */
	if (!stat(file_name, &status)) {
		return g_strdup(file_name);
	}
	
	/* Now pull apart $MDBPATH and try those */
	mdbpath = (gchar *) getenv("MDBPATH");
	/* no path, can't find file */
	if (!mdbpath || !strlen(mdbpath)) return NULL;

	dir = g_strsplit(mdbpath, ":", 0); 
	while (dir[i]) {
		if (!strlen(dir[i])) continue;
		tmpfname = g_strconcat(dir[i++], "/", file_name, NULL);
		if (!stat(tmpfname, &status)) {
			g_strfreev(dir);
			return tmpfname;
		}
		g_free(tmpfname);
	}
	g_strfreev(dir);
	return NULL;
}
/**
 * mdb_open:
 * @filename: path to MDB (database) file
 * @flags: MDB_NOFLAGS for read-only, MDB_WRITABLE for read/write
 *
 * Opens an MDB file and returns an MdbHandle to it.  MDB File may be relative
 * to the current directory, a full path to the file, or relative to a 
 * component of $MDBPATH.
 *
 * Return value: pointer to MdbHandle structure.
 **/
MdbHandle *mdb_open(const char *filename, MdbFileFlags flags)
{
	MdbHandle *mdb;
	int open_flags;

	mdb = (MdbHandle *) g_malloc0(sizeof(MdbHandle));
	mdb_set_default_backend(mdb, "access");
#ifdef HAVE_ICONV
	mdb->iconv_in = (iconv_t)-1;
	mdb->iconv_out = (iconv_t)-1;
#endif
	/* need something to bootstrap with, reassign after page 0 is read */
	mdb->fmt = &MdbJet3Constants;
	mdb->f = (MdbFile *) g_malloc0(sizeof(MdbFile));
	mdb->f->refs = 1;
	mdb->f->fd = -1;
	mdb->f->filename = (char *) mdb_find_file(filename);
	if (!mdb->f->filename) { 
		fprintf(stderr, "Can't alloc filename\n");
		mdb_close(mdb);
		return NULL; 
	}
	if (flags & MDB_WRITABLE) {
		mdb->f->writable = TRUE;
		open_flags = O_RDWR;
	} else {
		open_flags = O_RDONLY;
	}

#ifdef _WIN32
	open_flags |= O_BINARY;
#endif

	mdb->f->fd = open(mdb->f->filename, open_flags);

	if (mdb->f->fd==-1) {
		fprintf(stderr,"Couldn't open file %s\n",mdb->f->filename); 
		mdb_close(mdb);
		return NULL;
	}
	if (!mdb_read_pg(mdb, 0)) {
		fprintf(stderr,"Couldn't read first page.\n");
		mdb_close(mdb);
		return NULL;
	}
	if (mdb->pg_buf[0] != 0) {
		mdb_close(mdb);
		return NULL; 
	}
	mdb->f->jet_version = mdb_get_int32(mdb->pg_buf, 0x14);
	if (IS_JET4(mdb)) {
		mdb->fmt = &MdbJet4Constants;
	} else if (IS_JET3(mdb)) {
		mdb->fmt = &MdbJet3Constants;
	} else {
		fprintf(stderr,"Unknown Jet version.\n");
		mdb_close(mdb);
		return NULL; 
	}
	mdb_iconv_init(mdb);

	return mdb;
}

/**
 * mdb_close:
 * @mdb: Handle to open MDB database file
 *
 * Dereferences MDB file, closes if reference count is 0, and destroys handle.
 *
 **/
void 
mdb_close(MdbHandle *mdb)
{
	if (!mdb) return;	
	mdb_free_catalog(mdb);
	g_free(mdb->stats);
	g_free(mdb->backend_name);

	if (mdb->f) {
		if (mdb->f->refs > 1) {
			mdb->f->refs--;
		} else {
			if (mdb->f->fd != -1) close(mdb->f->fd);
			g_free(mdb->f->filename);
			g_free(mdb->f);
		}
	}

	mdb_iconv_close(mdb);

	g_free(mdb);
}
/**
 * mdb_clone_handle:
 * @mdb: Handle to open MDB database file
 *
 * Clones an existing database handle.  Cloned handle shares the file descriptor
 * but has its own page buffer, page position, and similar internal variables.
 *
 * Return value: new handle to the database.
 */
MdbHandle *mdb_clone_handle(MdbHandle *mdb)
{
	MdbHandle *newmdb;
	MdbCatalogEntry *entry, *data;
	unsigned int i;

	newmdb = (MdbHandle *) g_memdup(mdb, sizeof(MdbHandle));
	newmdb->stats = NULL;
	newmdb->catalog = g_ptr_array_new();
	for (i=0;i<mdb->num_catalog;i++) {
		entry = g_ptr_array_index(mdb->catalog,i);
		data = g_memdup(entry,sizeof(MdbCatalogEntry));
		g_ptr_array_add(newmdb->catalog, data);
	}
	mdb->backend_name = NULL;
	if (mdb->f) {
		mdb->f->refs++;
	}
	mdb_iconv_init(mdb);

	return newmdb;
}

/* 
** mdb_read a wrapper for read that bails if anything is wrong 
*/
ssize_t mdb_read_pg(MdbHandle *mdb, unsigned long pg)
{
	ssize_t len;

	if (pg && mdb->cur_pg == pg) return mdb->fmt->pg_size;

	len = _mdb_read_pg(mdb, mdb->pg_buf, pg);
	//fprintf(stderr, "read page %d type %02x\n", pg, mdb->pg_buf[0]);
	mdb->cur_pg = pg;
	/* kan - reset the cur_pos on a new page read */
	mdb->cur_pos = 0; /* kan */
	return len;
}
ssize_t mdb_read_alt_pg(MdbHandle *mdb, unsigned long pg)
{
	ssize_t len;

	len = _mdb_read_pg(mdb, mdb->alt_pg_buf, pg);
	return len;
}
static ssize_t _mdb_read_pg(MdbHandle *mdb, unsigned char *pg_buf, unsigned long pg)
{
	ssize_t len;
	struct stat status;
	off_t offset = pg * mdb->fmt->pg_size;

        fstat(mdb->f->fd, &status);
        if (status.st_size < offset) { 
                fprintf(stderr,"offset %lu is beyond EOF\n",offset);
                return 0;
        }
	if (mdb->stats && mdb->stats->collect) 
		mdb->stats->pg_reads++;

	lseek(mdb->f->fd, offset, SEEK_SET);
	len = read(mdb->f->fd,pg_buf,mdb->fmt->pg_size);
	if (len==-1) {
		perror("read");
		return 0;
	}
	else if (len<mdb->fmt->pg_size) {
		/* fprintf(stderr,"EOF reached %d bytes returned.\n",len, mdb->fmt->pg_size); */
		return 0;
	} 
	return len;
}
void mdb_swap_pgbuf(MdbHandle *mdb)
{
char tmpbuf[MDB_PGSIZE];

	memcpy(tmpbuf,mdb->pg_buf, MDB_PGSIZE);
	memcpy(mdb->pg_buf,mdb->alt_pg_buf, MDB_PGSIZE);
	memcpy(mdb->alt_pg_buf,tmpbuf,MDB_PGSIZE);
}


/* really stupid, just here for consistancy */
unsigned char mdb_get_byte(unsigned char *buf, int offset)
{
	return buf[offset];
}
unsigned char mdb_pg_get_byte(MdbHandle *mdb, int offset)
{
	if (offset < 0 || offset+1 > mdb->fmt->pg_size) return -1;
	mdb->cur_pos++;
	return mdb->pg_buf[offset];
}

int mdb_get_int16(unsigned char *buf, int offset)
{
	return buf[offset+1]*256+buf[offset];
}
int mdb_pg_get_int16(MdbHandle *mdb, int offset)
{
	if (offset < 0 || offset+2 > mdb->fmt->pg_size) return -1;
	mdb->cur_pos+=2;
	return mdb_get_int16(mdb->pg_buf, offset);
}

gint32 mdb_pg_get_int24_msb(MdbHandle *mdb, int offset)
{
	gint32 l = 0;
	if (offset <0 || offset+3 > mdb->fmt->pg_size) return -1;
	mdb->cur_pos+=3;
	memcpy(&l, &(mdb->pg_buf[offset]), 3);
	return GINT32_FROM_BE(l);
}
gint32 mdb_get_int24(unsigned char *buf, int offset)
{
	gint32 l = 0;
	memcpy(&l, &buf[offset], 3);
	return GINT32_FROM_LE(l);
}
gint32 mdb_pg_get_int24(MdbHandle *mdb, int offset)
{
	if (offset <0 || offset+3 > mdb->fmt->pg_size) return -1;
	mdb->cur_pos+=3;
	return mdb_get_int24(mdb->pg_buf, offset);
}

long mdb_get_int32(unsigned char *buf, int offset)
{
	guint32 l;
	memcpy(&l, &buf[offset], 4);
	return (long)GINT32_FROM_LE(l);
}
long mdb_pg_get_int32(MdbHandle *mdb, int offset)
{
	if (offset <0 || offset+4 > mdb->fmt->pg_size) return -1;
	mdb->cur_pos+=4;
	return mdb_get_int32(mdb->pg_buf, offset);
}

float mdb_get_single(unsigned char *buf, int offset)
{
	union {guint32 g; float f;} f;
	memcpy(&f, &buf[offset], 4);
	f.g = GUINT32_FROM_LE(f.g);
	return f.f;
}
float mdb_pg_get_single(MdbHandle *mdb, int offset)
{
       if (offset <0 || offset+4 > mdb->fmt->pg_size) return -1;
       mdb->cur_pos+=4;
       return mdb_get_single(mdb->pg_buf, offset);
}

double mdb_get_double(unsigned char *buf, int offset)
{
	union {guint64 g; double d;} d;
	memcpy(&d, &buf[offset], 8);
	d.g = GUINT64_FROM_LE(d.g);
	return d.d;
}
double mdb_pg_get_double(MdbHandle *mdb, int offset)
{
	if (offset <0 || offset+8 > mdb->fmt->pg_size) return -1;
	mdb->cur_pos+=8;
	return mdb_get_double(mdb->pg_buf, offset);
}


int 
mdb_set_pos(MdbHandle *mdb, int pos)
{
	if (pos<0 || pos >= mdb->fmt->pg_size) return 0;

	mdb->cur_pos=pos;
	return pos;
}
int mdb_get_pos(MdbHandle *mdb)
{
	return mdb->cur_pos;
}
