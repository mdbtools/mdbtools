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
#include <inttypes.h>

void mdb_buffer_dump(const void* buf, off_t start, size_t len)
{
	char asc[20];
	size_t j;
    int k = 0;

	memset(asc, 0, sizeof(asc));
	k = 0;
	for (j=0; j<len; j++) {
		unsigned int c = ((const unsigned char *)(buf))[start+j];
		if (k == 0) {
			fprintf(stdout, "%04" PRIu64 "x  ", (uint64_t)(start+j));
		}
		fprintf(stdout, "%02x ", (unsigned char)c);
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
	if (k < 8) {
		fprintf(stdout, " ");
	}
	for (; k<16; k++) {
		fprintf(stdout, "   ");
	}
	fprintf(stdout, "  %s\n", asc);
}
