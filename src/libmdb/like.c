/* MDB Tools - A library for reading MS Access database file
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

#include <stdio.h>
#include <string.h>
#include <mdbtools.h>

#ifdef DMALLOC
#include "dmalloc.h"
#endif

/**
 * mdb_like_cmp
 * @s: String to search within.
 * @r: Search pattern.
 *
 * Tests the string @s to see if it matches the search pattern @r.  In the
 * search pattern, a percent sign indicates matching on any number of
 * characters, and an underscore indicates matching any single character.
 *
 * Returns: 1 if the string matches, 0 if the string does not match.
 */
int mdb_like_cmp(char *s, char *r)
{
	unsigned int i;
	int ret;

	mdb_debug(MDB_DEBUG_LIKE, "comparing %s and %s", s, r);
	switch (r[0]) {
		case '\0':
			if (s[0]=='\0') {
				return 1;
			} else {
				return 0;
			}
		case '_':
			/* skip one character */
			return mdb_like_cmp(&s[1],&r[1]);
		case '%':
			/* skip any number of characters */
			/* the strlen(s)+1 is important so the next call can */
			/* if there are trailing characters */
			for(i=0;i<strlen(s)+1;i++) {
				if (mdb_like_cmp(&s[i],&r[1])) {
					return 1;
				}
			}
			return 0;
		default:
			for(i=0;i<strlen(r);i++) {
				if (r[i]=='_' || r[i]=='%') break;
			}
			if (strncmp(s,r,i)) {
				return 0;
			} else {
				mdb_debug(MDB_DEBUG_LIKE, "at pos %d comparing %s and %s", i, &s[i], &r[i]);
				ret = mdb_like_cmp(&s[i],&r[i]);
				mdb_debug(MDB_DEBUG_LIKE, "returning %d (%s and %s)", ret, &s[i], &r[i]);
				return ret;
			}
	}
}
