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
mdb_unicode2ascii(MdbHandle *mdb, unsigned char *buf, int offset, unsigned int len, char *dest, unsigned int dest_sz)
{
        unsigned int i, ret;
        int len_in, len_out;
        unsigned char *in_ptr, *out_ptr;

        in_ptr = &buf[offset];
        out_ptr = dest;
        len_in = len;
        len_out = dest_sz;

	if (in_ptr[0]==0xff && in_ptr[1]==0xfe) {
		len_in -= 2;
        	in_ptr += 2;
                ret = iconv(mdb->iconv_compress, (char **)&in_ptr, &len_in, (char **)&out_ptr, &len_out);
                dest[dest_sz - len_out]='\0';
                return dest_sz - len_out;
		//strncpy(dest, in_ptr+2, len-2);
		//dest[len-2]='\0';
	} else {
#ifdef HAVE_ICONV
        if (mdb->iconv_in) {
                //printf("1 len_in %d len_out %d\n",len_in, len_out);
                ret = iconv(mdb->iconv_in, (char **)&in_ptr, &len_in, (char **)&out_ptr, &len_out);
                //printf("2 len_in %d len_out %d\n",len_in, len_out);
                dest[dest_sz - len_out]='\0';
                //printf("dest %s\n",dest);
                return dest_sz - len_out;
        }
#endif

		/* convert unicode to ascii, rather sloppily */
		for (i=0;i<len;i+=2)
			dest[i/2] = in_ptr[i];
		dest[len/2]='\0';
	}
	return len;
}

int
mdb_ascii2unicode(MdbHandle *mdb, unsigned char *buf, int offset, unsigned int len, char *dest, unsigned int dest_sz)
{
        unsigned int i = 0, ret;
        size_t len_in, len_out, len_orig;
        char *in_ptr, *out_ptr;

        in_ptr = &buf[offset];
        out_ptr = dest;
        len_orig = strlen(in_ptr);
        len_in = len_orig;
        len_out = dest_sz;

        if (!buf) return 0;

#ifdef HAVE_ICONV
        if (mdb->iconv_out) {
                ret = iconv(mdb->iconv_out, &in_ptr, &len_in, &out_ptr, &len_out);
                //printf("len_in %d len_out %d\n",len_in, len_out);
                dest[dest_sz - len_out]='\0';
                dest[dest_sz - len_out + 1]='\0';
                return dest_sz - len_out;
	}
#endif

	if (IS_JET3(mdb)) {
		strncpy(dest, in_ptr, len);
		dest[len]='\0';
		return strlen(dest);
	}

	while (i<strlen(in_ptr) && (i*2+2)<len) {
		dest[i*2] = in_ptr[i];
		dest[i*2+1] = 0;
		i++;
	}

	return (i*2);
}
void mdb_iconv_init(MdbHandle *mdb)
{
	char *iconv_code;

	/* check environment variable */
	if (!(iconv_code=(char *)getenv("MDB_ICONV"))) {
		iconv_code="UTF-8";
	}

#ifdef HAVE_ICONV
        if (IS_JET4(mdb)) {
                mdb->iconv_out = iconv_open("UCS-2LE", iconv_code);
                mdb->iconv_in = iconv_open(iconv_code, "UCS-2LE");
                mdb->iconv_compress = iconv_open(iconv_code, "ISO8859-1");
        } else {
                /* XXX - need to determine character set from file */
                mdb->iconv_out = iconv_open("ISO8859-1", iconv_code);
                mdb->iconv_in = iconv_open(iconv_code, "ISO8859-1");
                mdb->iconv_compress = (iconv_t)-1;
        }
#endif
}
void mdb_iconv_close(MdbHandle *mdb)
{
#ifdef HAVE_ICONV
        if (mdb->iconv_out != (iconv_t)-1) iconv_close(mdb->iconv_out);
        if (mdb->iconv_in != (iconv_t)-1) iconv_close(mdb->iconv_in);
        if (mdb->iconv_compress != (iconv_t)-1) iconv_close(mdb->iconv_compress);
#endif
}
