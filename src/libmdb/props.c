/* MDB Tools - A library for reading MS Access database file
 * Copyright (C) 2000 Brian Bruns
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

GPtrArray *
mdb_read_props_list(gchar *kkd, int len)
{
	guint32 record_len;
	int pos = 0;
	gchar *name;
	GPtrArray *names = NULL;
	int i = 0;

	names = g_ptr_array_new();
#ifdef MDB_DEBUG
	buffer_dump(kkd, 0, len - 1);
#endif
	pos = 0;
	while (pos < len) {
		record_len = mdb_get_int16(kkd, pos);
		pos += 2;
#ifdef MDB_DEBUG
		printf("%02d ",i++);
		buffer_dump(kkd, pos - 2, pos + record_len - 1);
#endif
		name = g_malloc(record_len + 1);
		strncpy(name, &kkd[pos], record_len);
		name[record_len] = '\0';
		pos += record_len;
		g_ptr_array_add(names, name);
#ifdef MDB_DEBUG
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
MdbProperties *
mdb_alloc_props()
{
	MdbProperties *props;

	props = g_malloc0(sizeof(MdbProperties));

	return props;
}
MdbProperties * 
mdb_read_props(MdbHandle *mdb, GPtrArray *names, gchar *kkd, int len)
{
	guint32 record_len, name_len;
	int pos = 0;
	int elem, dtype, dsize;
	gchar *name, *value;
	MdbProperties *props;
	int i = 0;

#ifdef MDB_DEBUG
	buffer_dump(kkd, 0, len - 1);
#endif
	pos = 0;

	/* skip the name record */
	record_len = mdb_get_int16(kkd, pos);
	pos += 4;
	name_len = mdb_get_int16(kkd, pos);
	pos += 2;
	props = mdb_alloc_props();
	if (name_len) {
		props->name = g_malloc(name_len + 1);
		strncpy(props->name, &kkd[pos], name_len);
		props->name[name_len]='\0';
	}
	pos += name_len;

	props->hash = g_hash_table_new(g_str_hash, g_str_equal);

	while (pos < len) {
		record_len = mdb_get_int16(kkd, pos);
		elem = mdb_get_int16(kkd, pos + 4);
		dtype = kkd[pos + 3];
		dsize = mdb_get_int16(kkd, pos + 6);
		value = g_malloc(dsize + 1);
		strncpy(value, &kkd[pos + 8], dsize);
		value[dsize] = '\0';
		name = g_ptr_array_index(names,elem);
#ifdef MDB_DEBUG
		printf("%02d ",i++);
		buffer_dump(kkd, pos, pos + record_len - 1);
		printf("elem %d dsize %d dtype %d\n", elem, dsize, dtype);
#endif
		if (dtype == MDB_MEMO) dtype = MDB_TEXT;
		if (dtype == MDB_BOOL) {
			g_hash_table_insert(props->hash, g_strdup(name), g_strdup(kkd[pos + 8] ? "yes" : "no"));
		} else {
			g_hash_table_insert(props->hash, g_strdup(name), g_strdup(mdb_col_to_string(mdb, kkd, pos + 8, dtype, dsize)));
		}
		g_free(value);
		pos += record_len;
	}
	return props;
	
}
