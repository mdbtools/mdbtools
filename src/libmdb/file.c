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

#include <inttypes.h>
#include <stddef.h>
#include "mdbtools.h"

MdbFormatConstants MdbJet4Constants = {
	.pg_size = 4096,
    .row_count_offset = 0x0c,
    .tab_num_rows_offset = 16,
    .tab_num_cols_offset = 45,
    .tab_num_idxs_offset = 47,
    .tab_num_ridxs_offset = 51,
    .tab_usage_map_offset = 55,
    .tab_first_dpg_offset = 56,
    .tab_cols_start_offset = 63,
    .tab_ridx_entry_size = 12,
    .col_flags_offset = 15,
    .col_size_offset = 23,
    .col_num_offset = 5,
    .tab_col_entry_size = 25,
    .tab_free_map_offset = 59,
    .tab_col_offset_var = 7,
    .tab_col_offset_fixed = 21,
    .tab_row_col_num_offset = 9
};
MdbFormatConstants MdbJet3Constants = {
	.pg_size = 2048,
    .row_count_offset = 0x08,
    .tab_num_rows_offset = 12,
    .tab_num_cols_offset = 25,
    .tab_num_idxs_offset = 27,
    .tab_num_ridxs_offset = 31,
    .tab_usage_map_offset = 35,
    .tab_first_dpg_offset = 36,
    .tab_cols_start_offset = 43,
    .tab_ridx_entry_size = 8,
    .col_flags_offset = 13,
    .col_size_offset = 16,
    .col_num_offset = 1,
    .tab_col_entry_size = 18,
    .tab_free_map_offset = 39,
    .tab_col_offset_var = 3,
    .tab_col_offset_fixed = 14,
    .tab_row_col_num_offset = 5
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
 * mdb_handle_from_stream:
 * @stream An open file stream
 * @flags MDB_NOFLAGS for read-only, MDB_WRITABLE for read/write
 *
 * Allocates, initializes, and return an MDB handle from a file stream pointing
 * to an MDB file.
 *
 * Return value: The handle on success, NULL on failure
 */
static MdbHandle *mdb_handle_from_stream(FILE *stream, MdbFileFlags flags) {
	MdbHandle *mdb = g_malloc0(sizeof(MdbHandle));
	mdb_set_default_backend(mdb, "access");
    mdb_set_date_fmt(mdb, "%x %X");
    mdb_set_shortdate_fmt(mdb, "%x");
    mdb_set_bind_size(mdb, MDB_BIND_SIZE);
    mdb_set_boolean_fmt_numbers(mdb);
#ifdef HAVE_ICONV
	mdb->iconv_in = (iconv_t)-1;
	mdb->iconv_out = (iconv_t)-1;
#endif
	/* need something to bootstrap with, reassign after page 0 is read */
	mdb->fmt = &MdbJet3Constants;
	mdb->f = g_malloc0(sizeof(MdbFile));
	mdb->f->refs = 1;
	mdb->f->stream = stream;
	if (flags & MDB_WRITABLE) {
		mdb->f->writable = TRUE;
    }

	if (!mdb_read_pg(mdb, 0)) {
		// fprintf(stderr,"Couldn't read first page.\n");
		mdb_close(mdb);
		return NULL;
	}
	if (mdb->pg_buf[0] != 0) {
		mdb_close(mdb);
		return NULL; 
	}
	mdb->f->jet_version = mdb_get_byte(mdb->pg_buf, 0x14);
	switch(mdb->f->jet_version) {
	case MDB_VER_JET3:
		mdb->fmt = &MdbJet3Constants;
		break;
	case MDB_VER_JET4:
	case MDB_VER_ACCDB_2007:
	case MDB_VER_ACCDB_2010:
	case MDB_VER_ACCDB_2013:
	case MDB_VER_ACCDB_2016:
		mdb->fmt = &MdbJet4Constants;
		break;
	default:
		fprintf(stderr,"Unknown Jet version: %x\n", mdb->f->jet_version);
		mdb_close(mdb);
		return NULL; 
	}

    RC4_KEY rc4_key;
    unsigned int tmp_key = 0x6b39dac7;
    RC4_set_key(&rc4_key, 4, (unsigned char *)&tmp_key);
    RC4(&rc4_key, mdb->f->jet_version == MDB_VER_JET3 ? 126 : 128, mdb->pg_buf + 0x18);

	if (mdb->f->jet_version == MDB_VER_JET3) {
		mdb->f->lang_id = mdb_get_int16(mdb->pg_buf, 0x3a);
	} else {
		mdb->f->lang_id = mdb_get_int16(mdb->pg_buf, 0x6e);
	}
	mdb->f->code_page = mdb_get_int16(mdb->pg_buf, 0x3c);
	mdb->f->db_key = mdb_get_int32(mdb->pg_buf, 0x3e);
    if (mdb->f->jet_version == MDB_VER_JET3) {
        /* JET4 needs additional masking with the DB creation date, currently unsupported */
        /* Bug - JET3 supports 20 byte passwords, this is currently just 14 bytes */
        memcpy(mdb->f->db_passwd, mdb->pg_buf + 0x42, sizeof(mdb->f->db_passwd));
    }
    /* write is not supported for encrypted files yet */
    mdb->f->writable = mdb->f->writable && !mdb->f->db_key;

	mdb_iconv_init(mdb);

	return mdb;
}

/**
 * mdb_open_buffer:
 * @buffer A memory buffer containing an MDB file
 * @len Length of the buffer
 *
 * Opens an MDB file in memory and returns an MdbHandle to it.
 *
 * Return value: point to MdbHandle structure.
 */
MdbHandle *mdb_open_buffer(void *buffer, size_t len, MdbFileFlags flags) {
    FILE *file = NULL;
#ifdef HAVE_FMEMOPEN
    file = fmemopen(buffer, len, (flags & MDB_WRITABLE) ? "r+" : "r");
#else
    fprintf(stderr, "mdb_open_buffer requires a platform with support for fmemopen(3)\n");
#endif
    if (file == NULL) {
        fprintf(stderr, "Couldn't open memory buffer\n");
        return NULL;
    }
    return mdb_handle_from_stream(file, flags);
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
    FILE *file;

	char *filepath = mdb_find_file(filename);
	if (!filepath) {
		fprintf(stderr, "File not found\n");
		return NULL; 
	}
#ifdef _WIN32
    char *mode = (flags & MDB_WRITABLE) ? "rb+" : "rb";
#else
    char *mode = (flags & MDB_WRITABLE) ? "r+" : "r";
#endif

    if ((file = fopen(filepath, mode)) == NULL) {
		fprintf(stderr,"Couldn't open file %s\n",filepath);
		g_free(filepath);
		return NULL;
    }

    g_free(filepath);

    return mdb_handle_from_stream(file, flags);
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
			if (mdb->f->stream) fclose(mdb->f->stream);
			g_free(mdb->f);
		}
	}

	mdb_iconv_close(mdb);
    mdb_remove_backends(mdb);

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

	memset(&newmdb->catalog, 0, sizeof(MdbHandle) - offsetof(MdbHandle, catalog));

	newmdb->catalog = g_ptr_array_new();
	for (i=0;i<mdb->num_catalog;i++) {
		entry = g_ptr_array_index(mdb->catalog,i);
		data = g_memdup(entry,sizeof(MdbCatalogEntry));
		data->mdb = newmdb;
		data->props = NULL;
		g_ptr_array_add(newmdb->catalog, data);
	}

	mdb_iconv_init(newmdb);
	mdb_set_default_backend(newmdb, mdb->backend_name);

	if (mdb->f) {
		mdb->f->refs++;
	}

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
	//fprintf(stderr, "read page %ld type %02x\n", pg, mdb->pg_buf[0]);
	mdb->cur_pg = pg;
	/* kan - reset the cur_pos on a new page read */
	mdb->cur_pos = 0; /* kan */
	return len;
}
ssize_t mdb_read_alt_pg(MdbHandle *mdb, unsigned long pg)
{
	return _mdb_read_pg(mdb, mdb->alt_pg_buf, pg);
}
static ssize_t _mdb_read_pg(MdbHandle *mdb, void *pg_buf, unsigned long pg)
{
	ssize_t len;
	off_t offset = pg * mdb->fmt->pg_size;

    if (fseeko(mdb->f->stream, 0, SEEK_END) == -1) {
        fprintf(stderr, "Unable to seek to end of file\n");
        return 0;
    }
    if (ftello(mdb->f->stream) < offset) { 
        fprintf(stderr,"offset %" PRIu64 " is beyond EOF\n",(uint64_t)offset);
        return 0;
    }
	if (mdb->stats && mdb->stats->collect) 
		mdb->stats->pg_reads++;

	if (fseeko(mdb->f->stream, offset, SEEK_SET) == -1) {
        fprintf(stderr, "Unable to seek to page %lu\n", pg);
        return 0;
    }
	len = fread(pg_buf, 1, mdb->fmt->pg_size, mdb->f->stream);
	if (ferror(mdb->f->stream)) {
		perror("read");
		return 0;
	}
    memset(pg_buf + len, 0, mdb->fmt->pg_size - len);
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

	return mdb->fmt->pg_size;
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

int mdb_get_int16(void *buf, int offset)
{
	guint16 l;
	memcpy(&l, (char*)buf + offset, 2);
	return (int)GUINT16_FROM_LE(l);
}
int mdb_pg_get_int16(MdbHandle *mdb, int offset)
{
	if (offset < 0 || offset+2 > mdb->fmt->pg_size) return -1;
	mdb->cur_pos+=2;
	return mdb_get_int16(mdb->pg_buf, offset);
}

long mdb_get_int32_msb(void *buf, int offset)
{
	gint32 l;
	memcpy(&l, (char*)buf + offset, 4);
	return (long)GINT32_FROM_BE(l);
}
long mdb_get_int32(void *buf, int offset)
{
	gint32 l;
	memcpy(&l, (char*)buf + offset, 4);
	return (long)GINT32_FROM_LE(l);
}
long mdb_pg_get_int32(MdbHandle *mdb, int offset)
{
	if (offset <0 || offset+4 > mdb->fmt->pg_size) return -1;
	mdb->cur_pos+=4;
	return mdb_get_int32(mdb->pg_buf, offset);
}

float mdb_get_single(void *buf, int offset)
{
	union {guint32 g; float f;} f;
	memcpy(&f, (char*)buf + offset, 4);
	f.g = GUINT32_FROM_LE(f.g);
	return f.f;
}
float mdb_pg_get_single(MdbHandle *mdb, int offset)
{
       if (offset <0 || offset+4 > mdb->fmt->pg_size) return -1;
       mdb->cur_pos+=4;
       return mdb_get_single(mdb->pg_buf, offset);
}

double mdb_get_double(void *buf, int offset)
{
	union {guint64 g; double d;} d;
	memcpy(&d, (char*)buf + offset, 8);
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
