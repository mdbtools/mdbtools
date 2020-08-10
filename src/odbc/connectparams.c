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
#include "connectparams.h"

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

#define max_line 256
static char line[max_line];

static guint HashFunction (gconstpointer key);
#if HAVE_SQLGETPRIVATEPROFILESTRING
#include <sqltypes.h>
extern int  SQLGetPrivateProfileString( LPCSTR lpszSection,
					LPCSTR lpszEntry,
					LPCSTR lpszDefault,
					LPSTR  lpszRetBuffer,
					int    cbRetBuffer,
					LPCSTR lpszFilename);
#else
static GString* GetIniFileName (void);
static int FileExists (const gchar* name);
static int FindSection (FILE* stream, const char* section);
static int GetNextItem (FILE* stream, char** name, char** value);
#endif //HAVE_SQLGETPRIVATEPROFILESTRING

static void visit (gpointer key, gpointer value, gpointer user_data);
static void cleanup (gpointer key, gpointer value, gpointer user_data);

/*
 * Allocate create a ConnectParams object
 */

ConnectParams* NewConnectParams ()
{
   ConnectParams* params = (ConnectParams *) g_malloc(sizeof (ConnectParams));
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

#if !HAVE_SQLGETPRIVATEPROFILESTRING
static int LoadDSN (
   const gchar* iniFileName, const gchar* dsnName, GHashTable* table)
{
   FILE* stream;
   gchar* name;
   gchar* value;


   if ((stream = fopen (iniFileName, "r" )) != NULL )   
   {
      if (!FindSection (stream, dsnName))
      {
	 g_printerr ("Couldn't find DSN %s in %s\n", dsnName, iniFileName);
	 fclose (stream);
         return 0;
      }
      else
      {
         while (GetNextItem (stream, &name, &value))
         {
            g_hash_table_insert (table, g_strdup (name), g_strdup (value));
         }
      }

      fclose( stream );   
   }

   return 1;
}

/*
 * Find the settings for the specified ODBC DSN
 */
gboolean LookupDSN (ConnectParams* params, const gchar* dsnName)
{
   if (!params) {
      fprintf(stderr,"LookupDSN: no parameters, returning FALSE\n");
      return FALSE;
   }
   /*
    * Set the DSN name property
    */
 /*  params->dsnName = g_string_assign (params->dsnName, dsnName); */
   /*
    * Search for the ODBC ini file
    */
   if (!(params->iniFileName = GetIniFileName ())) {
      fprintf(stderr,"LookupDSN: GetIniFileName returned FALSE\n");
      return FALSE;
   }

   if (!LoadDSN (params->iniFileName->str, dsnName, params->table)) {
      fprintf(stderr,"LookupDSN: LoadDSN returned FALSE\n");
      return FALSE;
   }

   return TRUE;
}
/*
 * Get the value of a given ODBC Connection Parameter
 */

gchar* GetConnectParam (ConnectParams* params, const gchar* paramName)
{
   if (!params || !params->table)
      return NULL;

   return g_hash_table_lookup (params->table, paramName);
}

#else
gboolean LookupDSN (ConnectParams* params, const gchar* dsnName)
{
	return TRUE;
}
gchar* GetConnectParam (ConnectParams* params, const gchar* paramName)
{
	static char tmp[FILENAME_MAX];

	/* use old servername */
	tmp[0] = '\0';
	if (SQLGetPrivateProfileString(params->dsnName->str, paramName, "", tmp, FILENAME_MAX, "odbc.ini") > 0) {
		return tmp;
	}
	return NULL;

}
#endif /* !HAVE_SQLGETPRIVATEPROFILESTRING */

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
   char *p, *q, *s;

   if (!params)
      return NULL;
   /*
    * Position ourselves to the beginning of "DSN"
    */
   p = strstr (connectString, "DSN");
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
    * Copy the DSN value to a buffer
    */
   s = line;
   while (*q && *q != ';')
      *s++ = *q++;
   *s = '\0';
   /*
    * Save it as a string in the params object
    */
   params->dsnName = g_string_assign (params->dsnName, line);

   return params->dsnName->str;   
}

gchar* ExtractDBQ (ConnectParams* params, const gchar* connectString)
{
   char *p, *q, *s;

   if (!params)
      return NULL;
   /*
    * Position ourselves to the beginning of "DSN"
    */
   p = strstr (connectString, "DBQ");
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
    * Copy the DSN value to a buffer
    */
   s = line;
   while (*q && *q != ';')
      *s++ = *q++;
   *s = '\0';
   /*
    * Save it as a string in the params object
    */
   params->dsnName = g_string_assign (params->dsnName, line);

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

#if !HAVE_SQLGETPRIVATEPROFILESTRING
static GString* GetIniFileName ()
{
   char* setting;
   GString* iniFileName = g_string_new ("");
   /*
    * First, try the ODBCINI environment variable
    */
   if ((setting = getenv ("ODBCINI")) != NULL)
   {
      g_string_assign (iniFileName, getenv ("ODBCINI"));

      if (FileExists (iniFileName->str))
         return iniFileName;
      
      g_string_assign (iniFileName, "");
   }
   /*
    * Second, try the HOME environment variable
    */
   if ((setting = getenv ("HOME")) != NULL)
   {
      g_string_assign (iniFileName, setting);
      iniFileName = g_string_append (iniFileName, "/.odbc.ini");

      if (FileExists (iniFileName->str))
         return iniFileName;
      
      g_string_assign (iniFileName, "");
   }
   /*
    * As a last resort, try SYS_ODBC_INI
    */
   g_string_assign (iniFileName, SYS_ODBC_INI); 
   if (FileExists (iniFileName->str))
      return iniFileName;
    
   g_string_assign (iniFileName, "");

   return iniFileName;
}

static int FileExists (const gchar* name)
{
   struct stat fileStat;

   return (stat (name, &fileStat) == 0);
}

static int FindSection (FILE* stream, const char* section)
{
   char* s;
   char sectionPattern[max_line];
   int len;

   strcpy (sectionPattern, "[");
   strcat (sectionPattern, section);
   strcat (sectionPattern, "]");

   s = fgets (line, max_line, stream);
   while (s != NULL)
   {
      /*
       * Get rid of the newline character
       */
      len = strlen (line);
      if (len > 0) line[strlen (line) - 1] = '\0';
      /*
       * look for the section header
       */
      if (strcmp (line, sectionPattern) == 0)
         return 1;

      s = fgets (line, max_line, stream);
   }

   return 0;
}

static int GetNextItem (FILE* stream, char** name, char** value)
{
   char* s;
   int len;
   char equals[] = "="; /* used for seperator for strtok */
   char* token;

   if (name == NULL || value == NULL)
   {
      g_printerr ("GetNextItem, invalid parameters");
      return 0;
   }

   s = fgets (line, max_line, stream);
   if (s == NULL)
   {
      //perror ("fgets");
      return 0;
   }
   /*
    * Get rid of the newline character
    */
   len = strlen (line);
   if (len > 0) line[strlen (line) - 1] = '\0';
   /*
    * Extract name from name = value
    */
   if ((token = strtok (line, equals)) == NULL) return 0;

   len = strlen (token);
   while (len > 0 && isspace(token[len-1]))
   {
      len--;
      token[len] = '\0';
   }
   *name = token;
   /*
    * extract value from name = value
    */
   token = strtok (NULL, equals);
   if (token == NULL) return 0;
   while (*token && isspace(token[0]))
      token++;

   *value = token;

   return 1;
}
#endif //!HAVE_SQLGETPRIVATEPROFILESTRING

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


#ifdef UNIXODBC
#include <odbcinstext.h>

int
ODBCINSTGetProperties(HODBCINSTPROPERTY hLastProperty)
{
	hLastProperty->pNext = (HODBCINSTPROPERTY) malloc(sizeof(ODBCINSTPROPERTY));
	hLastProperty = hLastProperty->pNext;
	memset(hLastProperty, 0, sizeof(ODBCINSTPROPERTY));
	hLastProperty->nPromptType = ODBCINST_PROMPTTYPE_FILENAME;
	strncpy(hLastProperty->szName, "Database", INI_MAX_PROPERTY_NAME);
	strncpy(hLastProperty->szValue, "", INI_MAX_PROPERTY_VALUE);
	hLastProperty->pszHelp = (char *) g_strdup("Filename and Path of MDB file to connect to.\n"
						 "Use the full path to the database file.");


	return 1;
}

#endif
