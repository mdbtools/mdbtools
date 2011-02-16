/* MDB Tools - A library for reading MS Access database file
 * Copyright (C) 2000-2011 Brian Bruns and others
 *
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

static GPtrArray *
mdb_read_props_list(MdbHandle *mdb, gchar *kkd, int len)
{
	guint32 record_len;
	int pos = 0;
	gchar *name;
	GPtrArray *names = NULL;

	names = g_ptr_array_new();
	int i=0;
#if MDB_DEBUG
	buffer_dump(kkd, 0, len);
#endif
	pos = 0;
	while (pos < len) {
		record_len = mdb_get_int16(kkd, pos);
		pos += 2;
		if (mdb_get_option(MDB_DEBUG_PROPS)) {
			fprintf(stderr, "%02d ",i++);
			buffer_dump(kkd, pos - 2, record_len + 2);
		}
		name = g_malloc(3*record_len + 1); /* worst case scenario is 3 bytes out per byte in */
		mdb_unicode2ascii(mdb, &kkd[pos], record_len, name, 3*record_len);

		pos += record_len;
		g_ptr_array_add(names, name);
#if MDB_DEBUG
		printf("new len = %d\n", names->len);
#endif
	}
	return names;
}
void
mdb_free_props(MdbProperties *props)
{
	if (!props) return;

	if (props->name) g_free(props->name);
	g_free(props);
}

static void
free_names(GPtrArray *names) {
	g_ptr_array_foreach(names, (GFunc)g_free, NULL);
	g_ptr_array_free(names, TRUE);
}
MdbProperties *
mdb_alloc_props()
{
	MdbProperties *props;

	props = g_malloc0(sizeof(MdbProperties));

	return props;
}
static MdbProperties *
mdb_read_props(MdbHandle *mdb, GPtrArray *names, gchar *kkd, int len)
{
	guint32 record_len, name_len;
	int pos = 0;
	int elem, dtype, dsize;
	gchar *name, *value;
	MdbProperties *props;
	int i=0;

#if MDB_DEBUG
	buffer_dump(kkd, 0, len);
#endif
	pos = 0;

	record_len = mdb_get_int16(kkd, pos);
	pos += 4;
	name_len = mdb_get_int16(kkd, pos);
	pos += 2;
	props = mdb_alloc_props();
	if (name_len) {
		props->name = g_malloc(3*name_len + 1);
		mdb_unicode2ascii(mdb, kkd+pos, name_len, props->name, 3*name_len);
		mdb_debug(MDB_DEBUG_PROPS,"prop block named: %s", props->name);
	}
	pos += name_len;

	props->hash = g_hash_table_new(g_str_hash, g_str_equal);

	while (pos < len) {
		record_len = mdb_get_int16(kkd, pos);
		dtype = kkd[pos + 3];
		elem = mdb_get_int16(kkd, pos + 4);
		dsize = mdb_get_int16(kkd, pos + 6);
		value = g_malloc(dsize + 1);
		strncpy(value, &kkd[pos + 8], dsize);
		value[dsize] = '\0';
		name = g_ptr_array_index(names,elem);
		if (mdb_get_option(MDB_DEBUG_PROPS)) {
			fprintf(stderr, "%02d ",i++);
			mdb_debug(MDB_DEBUG_PROPS,"elem %d (%s) dsize %d dtype %d", elem, name, dsize, dtype);
			buffer_dump(value, 0, dsize);
		}
		if (dtype == MDB_MEMO) dtype = MDB_TEXT;
		if (dtype == MDB_BOOL) {
			g_hash_table_insert(props->hash, g_strdup(name),
				g_strdup(kkd[pos + 8] ? "yes" : "no"));
		} else {
			g_hash_table_insert(props->hash, g_strdup(name),
			  mdb_col_to_string(mdb, kkd, pos + 8, dtype, dsize));
		}
		g_free(value);
		pos += record_len;
	}
	return props;
	
}

static void
print_keyvalue(gpointer key, gpointer value, gpointer outfile)
{
		fprintf((FILE*)outfile,"\t%s: %s\n", (gchar *)key, (gchar *)value);
}
void
mdb_dump_props(MdbProperties *props, FILE *outfile, int show_name) {
	if (show_name)
		fprintf(outfile,"name: %s\n", props->name ? props->name : "(none)");
	g_hash_table_foreach(props->hash, print_keyvalue, outfile);
	if (show_name)
		fputc('\n', outfile);
}

GArray*
kkd_to_props(MdbHandle *mdb, void *kkd, size_t len) {
	guint32 record_len;
	guint16 record_type;
	size_t pos;
	GPtrArray *names = NULL;
	MdbProperties *props;

#if MDB_DEBUG
	buffer_dump(kkd, 0, len);
#endif
	mdb_debug(MDB_DEBUG_PROPS,"starting prop parsing of type %s", kkd);
	if (strcmp("KKD", kkd) && strcmp("MR2", kkd)) {
		fprintf(stderr, "Unrecognized format.\n");
		buffer_dump(kkd, 0, len);
		return NULL;
	}

	GArray *result = g_array_new(0, 0, sizeof(MdbProperties*));

	pos = 4;
	while (pos < len) {
		record_len = mdb_get_int32(kkd, pos);
		record_type = mdb_get_int16(kkd, pos + 4);
		mdb_debug(MDB_DEBUG_PROPS,"prop chunk type:0x%04x len:%d", record_type, record_len);
		//buffer_dump(kkd, pos+4, record_len);
		switch (record_type) {
			case 0x80:
				if (names) free_names(names);
				names = mdb_read_props_list(mdb, kkd+pos+6, record_len - 6);
				break;
			case 0x00:
			case 0x01:
				if (!names) {
					fprintf(stderr,"sequence error!\n");
					break;
				}
				props = mdb_read_props(mdb, names, kkd+pos+6, record_len - 6);
				g_array_append_val(result, props);
				//mdb_dump_props(props, stderr, 1);
				break;
			default:
				fprintf(stderr,"Unknown record type %d\n", record_type);
				break;
		}
		pos += record_len;
	}
	if (names) free_names(names);
	return result;
}
