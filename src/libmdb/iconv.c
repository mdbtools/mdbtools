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

int
mdb_unicode2ascii(MdbHandle *mdb, unsigned char *buf, int offset, unsigned int len, char *dest)
{
	unsigned int i;

	if (buf[offset]==0xff && buf[offset+1]==0xfe) {
		strncpy(dest, &buf[offset+2], len-2);
		dest[len-2]='\0';
	} else {
		/* convert unicode to ascii, rather sloppily */
		for (i=0;i<len;i+=2)
			dest[i/2] = buf[offset + i];
		dest[len/2]='\0';
	}
	return len;
}

int
mdb_ascii2unicode(MdbHandle *mdb, unsigned char *buf, int offset, unsigned int len, char *dest)
{
	unsigned int i = 0;

	if (!buf) return 0;

	if (IS_JET3(mdb)) {
		strncpy(dest, &buf[offset], len);
		dest[len]='\0';
		return strlen(dest);
	}

	while (i<strlen(&buf[offset]) && (i*2+2)<len) {
		dest[i*2] = buf[offset+i];
		dest[i*2+1] = 0;
		i++;
	}

	return (i*2);
}
