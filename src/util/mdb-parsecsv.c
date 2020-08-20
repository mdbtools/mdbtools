/* MDB Tools - A library for reading MS Access database file
 * Copyright (C) 2000 Brian Bruns
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

/* this utility converts a CSV from an existing database to a C file */
/* input FOO.txt, output FOO.c */
/* generates an array of type FOO */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define FILENAMESIZE 128
#define BUFFERSIZE 4096
#define LF 10
#define NL 13
#define TRUE 1
#define FALSE 0

void copy_header (FILE *f)
{
 fprintf (f, "/******************************************************************/\n");
 fprintf (f, "/* THIS IS AN AUTOMATICALLY GENERATED FILE.  DO NOT EDIT IT!!!!!! */\n");
 fprintf (f, "/******************************************************************/\n");
}

int
main (int argc, char **argv)
{
  char txt_filename [FILENAMESIZE];
  char c_filename [FILENAMESIZE];
  FILE *txtfile;
  FILE *cfile;
  int count;
  char input [BUFFERSIZE];
  int location;
  int c;
  int instring;
  int lastcomma;
  int i;

  if (argc < 2)
    {
      fprintf (stderr, "Usage: %s <file> (assumed extension .txt)\n",argv[0]);
      exit (1);
    }

  strcpy (txt_filename, argv [1]);
  txtfile = fopen (txt_filename, "r");
  if (!txtfile) {
  	strcat (txt_filename, ".txt");
  	txtfile = fopen (txt_filename, "r");
  	if (!txtfile) exit(1);
  } else {
	/* strip away extension */
	for (i=strlen(txt_filename);i > 0 && txt_filename[i]!='.';i--);
	if (txt_filename[i]=='.') txt_filename[i]='\0';
  }

  strcpy (c_filename, argv [1]);
  strcat (c_filename, ".c");

  cfile = fopen (c_filename, "w");

  copy_header (cfile);
  fprintf (cfile, "\n");
  fprintf (cfile, "#include <stdio.h>\n");
  fprintf (cfile, "#include \"types.h\"\n");
  fprintf (cfile, "#include \"mdbsupport.h\"\n");
  fprintf (cfile, "\n");

  fprintf (cfile, "const %s %s_array [] = {\n", argv [1], argv [1]);

  c = 0;
  count = 0;
  while (c != -1)
    {
      location = 0;
      memset (input, 0, BUFFERSIZE);
      while ((c = fgetc (txtfile)) != NL)
	{
	  if (c == -1)
	    break;
	  input [location++] = c;
	}	
      input [location] = '\0';
      if (location > 0)
	{
	  if (count != 0)
	    {
	      fprintf (cfile, ",\n");
	    }
	  fprintf (cfile, "{\t\t\t\t/* %6d */\n\t", count);
	  instring = FALSE;
	  lastcomma = FALSE;
	  for (i = 0; i < location; i++)
	    {
	      if (instring)
		{
		  if (input [i] == '\\')
		    fprintf (cfile, "\\\\");
		  else if (input [i] == LF)
		    fprintf (cfile, "\\n");
		  else if (input [i] != NL)
		    fprintf (cfile, "%c", input [i]);
		  if (input [i] == '"')
		    {
		      instring = FALSE;
		      lastcomma = FALSE;
		    }
		}
	      else
		{
		  switch (input [i])
		    {
		    case ',':
		      if (lastcomma)
			fprintf (cfile, "\"\"");
		      fprintf (cfile, ",\n\t");
		      lastcomma = TRUE;
		      break;
		    case '"':
		      fprintf (cfile, "%c", input [i]);
		      lastcomma = FALSE;
		      instring = TRUE;
		      break;
		    default:
		      fprintf (cfile, "%c", input [i]);
		      lastcomma = FALSE;
		      break;
		    }
		}
	    }
	  if (lastcomma)
	    fprintf (cfile, "\"\"\n");
	  fprintf (cfile, "\n}");
	  count++;
	}
      c = fgetc (txtfile);
    }

  fprintf (cfile, "\n};\n");
  fprintf (cfile, "\nconst int %s_array_length = %d;\n", argv [1], count);

  fclose (txtfile);
  fclose (cfile);

  fprintf (stdout, "count = %d\n", count);

  return 0;
}
