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

#ifndef MDBPRIVATE_H
#define MDBPRIVATE_H

#include "mdbtools.h"

/*
 * This header is for stuff lacking a MDB_ or mdb_ something, or functions only
 * used within mdbtools so they won't be exported to calling programs.
 */

#ifndef HAVE_G_MEMDUP2
#define g_memdup2 g_memdup
#endif

#ifdef __cplusplus
  extern "C" {
#endif

void mdbi_rc4(unsigned char *key, guint32 key_len, unsigned char *buf, guint32 buf_len);
MdbBackend *mdbi_register_backend2(MdbHandle *mdb, char *backend_name, guint32 capabilities,
        const MdbBackendType *backend_type,
        const MdbBackendType *type_shortdate,
        const MdbBackendType *type_autonum,
        const char *short_now, const char *long_now,
        const char *date_fmt, const char *shortdate_fmt,
        const char *charset_statement, const char *create_table_statement,
        const char *drop_statement, const char *constaint_not_empty_statement,
        const char *column_comment_statement, const char *per_column_comment_statement,
        const char *table_comment_statement, const char *per_table_comment_statement,
        gchar* (*quote_schema_name)(const gchar*, const gchar*),
        gchar* (*normalise_case)(const gchar*));

#ifdef __cplusplus
  }
#endif

#endif
