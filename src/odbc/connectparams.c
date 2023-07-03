/* MDB Tools - A library for reading MS Access database file
 * Copyright (C) 2000-2004 Brian Bruns
 *
 * portions based on FreeTDS, Copyright (C) 1998-1999 Brian Bruns
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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <ctype.h>
#if defined(HAVE_ODBCINST_H)
#include <odbcinst.h>
#elif defined(HAVE_IODBCINST_H)
#include <iodbcinst.h>
#endif /* HAVE_ODBCINST_H */
#include "connectparams.h"


/** \addtogroup odbc
 *  @{
 */

/*
 *  * Last resort place to check for INI file. This is usually set at compile time
 *   * by build scripts.
 *    */
#ifndef SYS_ODBC_INI
#define SYS_ODBC_INI "/etc/odbc.ini"
#endif

#if defined(FILENAME_MAX) && FILENAME_MAX < 512
#undef FILENAME_MAX
#define FILENAME_MAX 512
#endif

static guint HashFunction (gconstpointer key);

static void visit (gpointer key, gpointer value, gpointer user_data);
static void cleanup (gpointer key, gpointer value, gpointer user_data);

/*
 * Allocate create a ConnectParams object
 */

ConnectParams* NewConnectParams ()
{
   ConnectParams* params = g_malloc(sizeof(ConnectParams));
   if (!params)
      return params;
   
   params->dsnName = g_string_new ("");
   params->iniFileName = NULL;
   params->table = g_hash_table_new (HashFunction, g_str_equal);

   return params;
}

/*
 * Destroy a ConnectParams object
 */

void FreeConnectParams (ConnectParams* params)
{
   if (params)
   {
      if (params->dsnName)
         g_string_free (params->dsnName, TRUE);
      if (params->iniFileName)
         g_string_free (params->iniFileName, TRUE);
      if (params->table)
      {
         g_hash_table_foreach (params->table, cleanup, NULL);
         g_hash_table_destroy (params->table);
      }
      g_free(params);
   }
}

gchar* GetConnectParam (ConnectParams* params, const gchar* paramName)
{
	static TLS char tmp[FILENAME_MAX];

	/* use old servername */
	tmp[0] = '\0';
	if (SQLGetPrivateProfileString(params->dsnName->str, paramName, "", tmp, sizeof(tmp), "odbc.ini") > 0) {
		return tmp;
	}
	return NULL;

}

/*
 * Apply a connection string to the ODBC Parameter Settings
 */

void SetConnectString (ConnectParams* params, const gchar* connectString)
{
   int end;
   char *cs, *s, *p, *name, *value;
   gpointer key;
   gpointer oldvalue;

   if (!params) 
      return;
   /*
    * Make a copy of the connection string so we can modify it
    */
   cs = (char *) g_strdup(connectString);
   s = cs;
   /*
    * Loop over ';' seperated name=value pairs
    */
   p = strchr (s, '=');
   while (p)
   {
      if (p) *p = '\0';
      /*
       * Extract name
       */
      name = s;
      if (p) s = p + 1;
      /*
       * Extract value
       */
      p = strchr (s, ';');
      if (p) *p = '\0';
      value = s;
      if (p) s = p + 1;
      /*
       * remove trailing spaces from name
       */
      end = strlen (name) - 1;
      while (end > 0 && isspace(name[end]))
	 name[end--] = '\0';
      /*
       * remove leading spaces from value
       */
      while (isspace(*value))
	 value++;

      if (g_hash_table_lookup_extended (params->table, name, &key, &oldvalue))
      {
	 /* 
	  * remove previous value 
	  */
	 g_hash_table_remove (params->table, key);
	 /* 
	  * cleanup strings 
	  */
	 g_free (key);
	 g_free (oldvalue);
      }
      /*
       * Insert the name/value pair into the hash table.
       *
       * Note that these g_strdup allocations are freed in cleanup,
       * which is called by FreeConnectParams.
       */
      g_hash_table_insert (params->table, g_strdup (name), g_strdup (value));

      p = strchr (s, '=');
   }  

   g_free (cs);
}

/*
 * Dump all the ODBC Connection Paramters to a file (e.g. stdout)
 */

void DumpParams (ConnectParams* params, FILE* output)
{
   if (!params)
   {
      g_printerr ("NULL ConnectionParams pointer\n");
      return;
   }

   if (params->dsnName)
      g_printerr ("Parameter values for DSN: %s\n", params->dsnName->str);
      
   if (params->iniFileName)
      g_printerr ("Ini File is %s\n", params->iniFileName->str);

   g_hash_table_foreach (params->table, visit, output);
}

/*
 * Return the value of the DSN from the conneciton string 
 */

gchar* ExtractDSN (ConnectParams* params, const gchar* connectString)
{
   char *p, *q;

   if (!params)
      return NULL;
   /*
    * Position ourselves to the beginning of "DSN"
    */
   p = strcasestr (connectString, "DSN");
   if (!p) return NULL;
   /*
    * Position ourselves to the "="
    */
   q = strchr (p, '=');
   if (!q) return NULL;
   /*
    * Skip over any leading spaces
    */
   q++;
   while (isspace(*q))
     q++;
   /*
    * Save it as a string in the params object
    */
   char **components = g_strsplit(q, ";", 2);
   params->dsnName = g_string_assign(params->dsnName, components[0]);
   g_strfreev(components);

   return params->dsnName->str;   
}

gchar* ExtractDBQ (ConnectParams* params, const gchar* connectString)
{
   char *p, *q;

   if (!params)
      return NULL;
   /*
    * Position ourselves to the beginning of "DBQ"
    */
   p = strcasestr (connectString, "DBQ");
   if (!p) return NULL;
   /*
    * Position ourselves to the "="
    */
   q = strchr (p, '=');
   if (!q) return NULL;
   /*
    * Skip over any leading spaces
    */
   q++;
   while (isspace(*q))
     q++;
   /*
    * Save it as a string in the params object
    */
   char **components = g_strsplit(q, ";", 2);
   params->dsnName = g_string_assign(params->dsnName, components[0]);
   g_strfreev(components);

   return params->dsnName->str;
}

/*
 * Begin local function definitions
 */

/*
 * Make a hash from all the characters
 */
static guint HashFunction (gconstpointer key)
{
   guint value = 0;
   const char* s = key;

   while (*s) value += *s++;

   return value;
}

static void visit (gpointer key, gpointer value, gpointer user_data)
{
   FILE* output = (FILE*) user_data;
   fprintf(output, "Parameter: %s, Value: %s\n", (char*)key, (char*)value);
}

static void cleanup (gpointer key, gpointer value, gpointer user_data)
{
   g_free (key);
   g_free (value);
}

/** @}*/
