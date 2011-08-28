/* MDB Tools - A library for reading MS Access database files
 * Copyright (C) 2000-2011 Brian Bruns and others
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

#include <ctype.h>
#include <string.h>
#include <stdio.h>

#ifdef DMALLOC
#include "dmalloc.h"
#endif

void mdb_buffer_dump(const void* buf, int start, size_t len)
{
	char asc[20];
	int j, k;

	memset(asc, 0, sizeof(asc));
	k = 0;
	for (j=start; j<start+len; j++) {
		int c = ((const unsigned char *)(buf))[j];
		if (k == 0) {
			fprintf(stdout, "%04x  ", j);
		}
		fprintf(stdout, "%02x ", c);
		asc[k] = isprint(c) ? c : '.';
		k++;
		if (k == 8) {
			fprintf(stdout, " ");
		}
		if (k == 16) {
			fprintf(stdout, "  %s\n", asc);
			memset(asc, 0, sizeof(asc));
			k = 0;
		}
	}
	for (j=k; j<16; j++) {
		fprintf(stdout, "   ");
	}
	if (k < 8) {
		fprintf(stdout, " ");
	}
	fprintf(stdout, "  %s\n", asc);
}
