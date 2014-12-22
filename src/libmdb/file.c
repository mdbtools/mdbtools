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
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
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
	guint16		col_flags_offset;
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

typedef struct _RC4_KEY
{
	unsigned char state[256];
	unsigned char x;
	unsigned char y;
} RC4_KEY;

#define swap_byte(x,y) t = *(x); *(x) = *(y); *(y) = t

static ssize_t _mdb_read_pg(MdbHandle *mdb, void *pg_buf, unsigned long pg);

static void RC4_set_key(RC4_KEY *key, int key_data_len, unsigned char *key_data_ptr)
{
	unsigned char t;
	unsigned char index1;
	unsigned char index2;
	unsigned char* state;
	short counter;

	state = &key->state[0];
	for(counter = 0; counter < 256; counter++)
		state[counter] = counter;
	key->x = 0;
	key->y = 0;
	index1 = 0;
	index2 = 0;
	for(counter = 0; counter < 256; counter++) {
		index2 = (key_data_ptr[index1] + state[counter] + index2) % 256;
		swap_byte(&state[counter], &state[index2]);
		index1 = (index1 + 1) % key_data_len;
	}
}

/*
 * this algorithm does 'encrypt in place' instead of inbuff/outbuff
 * note also: encryption and decryption use same routine
 * implementation supplied by (Adam Back) at <adam at cypherspace dot org>
 */

static void RC4(RC4_KEY *key, int buffer_len, unsigned char * buff)
{
	unsigned char t;
	unsigned char x;
	unsigned char y;
	unsigned char* state;
	unsigned char xorIndex;
	short counter;

	x = key->x;
	y = key->y;
	state = &key->state[0];
	for(counter = 0; counter < buffer_len; counter++) {
		x = (x + 1) % 256;
		y = (state[x] + y) % 256;
		swap_byte(&state[x], &state[y]);
		xorIndex = (state[x] + state[y]) % 256;
		buff[counter] ^= state[xorIndex];
	}
	key->x = x;
	key->y = y;
}


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

static char *mdb_find_file(const char *file_name)
{
	struct stat status;
	gchar *mdbpath, **dir, *tmpfname;
	unsigned int i = 0;

	/* try the provided file name first */
	if (!stat(file_name, &status)) {
		char *result;
		result = g_strdup(file_name);
		if (!result)
			fprintf(stderr, "Can't alloc filename\n");
		return result;
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
	int key[] = {0x86, 0xfb, 0xec, 0x37, 0x5d, 0x44, 0x9c, 0xfa, 0xc6, 0x5e, 0x28, 0xe6, 0x13, 0xb6};
	int j, pos;
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
	mdb->f->filename = mdb_find_file(filename);
	if (!mdb->f->filename) { 
		fprintf(stderr, "File not found\n");
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
	switch(mdb->f->jet_version) {
	case MDB_VER_JET3:
		mdb->fmt = &MdbJet3Constants;
		break;
	case MDB_VER_JET4:
	case MDB_VER_ACCDB_2007:
	case MDB_VER_ACCDB_2010:
		mdb->fmt = &MdbJet4Constants;
		break;
	default:
		fprintf(stderr,"Unknown Jet version.\n");
		mdb_close(mdb);
		return NULL; 
	}
	mdb->f->db_key = mdb_get_int32(mdb->pg_buf, 0x3e);
	/* I don't know if this value is valid for some versions?
	 * it doesn't seem to be valid for the databases I have
	 *
	 * f->db_key ^= 0xe15e01b9;
	 */
	mdb->f->db_key ^= 0x4ebc8afb;
	/* fprintf(stderr, "Encrypted file, RC4 key seed= %d\n", mdb->f->db_key); */
	if (mdb->f->db_key) {
		/* write is not supported for encrypted files yet */
		mdb->f->writable = FALSE;
		/* that should be enought, but reopen the file read only just to be
		 * sure we don't write invalid data */
		close(mdb->f->fd);
		open_flags = O_RDONLY;
#ifdef _WIN32
		open_flags |= O_BINARY;
#endif
		mdb->f->fd = open(mdb->f->filename, open_flags);
		if (mdb->f->fd==-1) {
			fprintf(stderr, "Couldn't ropen file %s in read only\n", mdb->f->filename);
			mdb_close(mdb);
			return NULL;
		}
	}

	/* get the db password located at 0x42 bytes into the file */
	for (pos=0;pos<14;pos++) {
		j = mdb_get_int32(mdb->pg_buf, 0x42+pos);
		j ^= key[pos];
		if ( j != 0)
			mdb->f->db_passwd[pos] = j;
		else
			mdb->f->db_passwd[pos] = '\0';
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
static ssize_t _mdb_read_pg(MdbHandle *mdb, void *pg_buf, unsigned long pg)
{
	ssize_t len;
	struct stat status;
	off_t offset = pg * mdb->fmt->pg_size;

        fstat(mdb->f->fd, &status);
        if (status.st_size < offset) { 
                fprintf(stderr,"offset %jd is beyond EOF\n",offset);
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
	/*
	 * unencrypt the page if necessary.
	 * it might make sense to cache the unencrypted data blocks?
	 */
	if (pg != 0 && mdb->f->db_key != 0)
	{
		RC4_KEY rc4_key;
		unsigned int tmp_key = mdb->f->db_key ^ pg;
		RC4_set_key(&rc4_key, 4, (unsigned char *)&tmp_key);
		RC4(&rc4_key, mdb->fmt->pg_size, pg_buf);
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


unsigned char mdb_get_byte(void *buf, int offset)
{
	return ((unsigned char *)(buf))[offset];
}
unsigned char mdb_pg_get_byte(MdbHandle *mdb, int offset)
{
	if (offset < 0 || offset+1 > mdb->fmt->pg_size) return -1;
	mdb->cur_pos++;
	return mdb->pg_buf[offset];
}

int mdb_get_int16(unsigned char *buf, int offset)
{
	guint16 l;
	memcpy(&l, buf + offset, 2);
	return (int)GUINT16_FROM_LE(l);
}
int mdb_pg_get_int16(MdbHandle *mdb, int offset)
{
	if (offset < 0 || offset+2 > mdb->fmt->pg_size) return -1;
	mdb->cur_pos+=2;
	return mdb_get_int16(mdb->pg_buf, offset);
}

long mdb_get_int32_msb(unsigned char *buf, int offset)
{
	gint32 l;
	memcpy(&l, buf + offset, 4);
	return (long)GINT32_FROM_BE(l);
}
long mdb_get_int32(unsigned char *buf, int offset)
{
	gint32 l;
	memcpy(&l, buf + offset, 4);
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
	memcpy(&f, buf + offset, 4);
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
	memcpy(&d, buf + offset, 8);
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
