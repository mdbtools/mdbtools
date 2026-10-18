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

#include <errno.h>
#include "mdbtools.h"

#ifndef MIN
#define MIN(a,b) (a>b ? b : a)
#endif

static size_t decompress_unicode(const char *src, size_t slen, char *dst, size_t dlen) {
	unsigned int compress=1;
	size_t tlen = 0;
	while (slen > 0 && tlen < dlen) {
		if (*src == 0) {
			compress = (compress) ? 0 : 1;
			src++;
			slen--;
		} else if (compress) {
			dst[tlen++] = *src++;
			dst[tlen++] = 0;
			slen--;
		} else if (slen >= 2){
			dst[tlen++] = *src++;
			dst[tlen++] = *src++;
			slen-=2;
		} else { // Odd # of bytes
			break;
		}
	}
	return tlen;
}

#ifdef HAVE_ICONV
static size_t decompressed_to_utf8_with_iconv(MdbHandle *mdb, const char *in_ptr, size_t len_in, char *dest, size_t dlen) {
	char *out_ptr = dest;
	size_t len_out = dlen - 1;

	while (len_out) {
		iconv(mdb->iconv_in, (ICONV_CONST char **)&in_ptr, &len_in, &out_ptr, &len_out);
		/* 
		 * Have seen database with odd number of bytes in UCS-2, shouldn't happen but protect against it
		 */
		if (!IS_JET3(mdb) && len_in<=1) {
			//fprintf(stderr, "Detected invalid number of UCS-2 bytes\n");
			break;
		}
		if (!len_in || !len_out || errno == E2BIG) break;
		/* Don't bail if impossible conversion is encountered */
		in_ptr += (IS_JET3(mdb)) ? 1 : 2;
		len_in -= (IS_JET3(mdb)) ? 1 : 2;
		*out_ptr++ = '?';
		len_out--;
	}
	dlen -= len_out + 1;
	dest[dlen] = '\0';
	return dlen;
}
#else
/* Windows-1252 is Latin-1 but for 0x80 to 0x9F. The five bytes it leaves
 * undefined become the C1 controls, as on Windows, so they round trip */
static const unsigned short cp1252_80_9f[32] = {
	0x20AC, 0x0081, 0x201A, 0x0192, 0x201E, 0x2026, 0x2020, 0x2021,
	0x02C6, 0x2030, 0x0160, 0x2039, 0x0152, 0x008D, 0x017D, 0x008F,
	0x0090, 0x2018, 0x2019, 0x201C, 0x201D, 0x2022, 0x2013, 0x2014,
	0x02DC, 0x2122, 0x0161, 0x203A, 0x0153, 0x009D, 0x017E, 0x0178
};

static size_t cp1252_to_utf8_without_iconv(const char *in_ptr, size_t len_in, char *dest, size_t dlen) {
	size_t i, j = 0;
	for (i=0; i<len_in; i++) {
		unsigned int c = (unsigned char)in_ptr[i];
		if (c >= 0x80 && c < 0xA0)
			c = cp1252_80_9f[c - 0x80];
		if (c < 0x80) {
			if (j + 1 >= dlen) break;
			dest[j++] = c;
		} else if (c < 0x800) {
			if (j + 2 >= dlen) break;
			dest[j++] = 0xC0 | (c >> 6);
			dest[j++] = 0x80 | (c & 0x3F);
		} else {
			if (j + 3 >= dlen) break;
			dest[j++] = 0xE0 | (c >> 12);
			dest[j++] = 0x80 | ((c >> 6) & 0x3F);
			dest[j++] = 0x80 | (c & 0x3F);
		}
	}
	dest[j] = '\0';
	return j;
}

static size_t unicode2ascii_locale(mdb_locale_t locale, const char *in_ptr, size_t len_in, char *dest, size_t dlen) {
    size_t i;
    size_t count = 0;
    size_t len_out = dlen - 1;
    wchar_t *w = malloc((len_in/2+1)*sizeof(wchar_t));

    for(i=0; i<len_in/2; i++)
    {
        w[i] = (unsigned char)in_ptr[2*i] + ((unsigned char)in_ptr[2*i+1] << 8);
    }
    w[len_in/2] = '\0';

#if defined(_WIN32) || defined(WIN32) || defined(_WIN64) || defined(WIN64) || defined(WINDOWS)
    count = _wcstombs_l(dest, w, len_out, locale);
#elif defined(HAVE_WCSTOMBS_L)
    count = wcstombs_l(dest, w, len_out, locale);
#else
    locale_t oldlocale = uselocale(locale);
    count = wcstombs(dest, w, len_out);
    uselocale(oldlocale);
#endif
    free(w);
    if (count == (size_t)-1)
        return 0;

    dest[count] = '\0';
	return count;
}

/* The reverse of cp1252_to_utf8_without_iconv(): characters that are not
 * in Windows-1252 become '?' */
static size_t utf8_to_cp1252_without_iconv(const char *in_ptr, size_t len_in, char *dest, size_t dlen) {
	size_t i = 0, j = 0;
	unsigned int k;
	while (i < len_in && j < dlen) {
		unsigned int c = (unsigned char)in_ptr[i++];
		if (c >= 0xC0) {
			int extra = c >= 0xF0 ? 3 : c >= 0xE0 ? 2 : 1;
			c &= 0x3F >> extra;
			while (extra && i < len_in && ((unsigned char)in_ptr[i] & 0xC0) == 0x80) {
				c = (c << 6) | ((unsigned char)in_ptr[i++] & 0x3F);
				extra--;
			}
			if (extra)
				c = '?';
		} else if (c >= 0x80) {
			/* a continuation byte without a lead byte */
			c = '?';
		}
		if ((c >= 0x80 && c < 0xA0) || c > 0xFF) {
			for (k=0; k<32 && cp1252_80_9f[k] != c; k++)
				;
			c = k < 32 ? 0x80 + k : '?';
		}
		dest[j++] = c;
	}
	return j;
}

/* The reverse of unicode2ascii_locale(): convert a multibyte string to
 * UCS-2LE. Characters outside the BMP become '?'. Returns the number of
 * bytes written, or (size_t)-1 if the input isn't valid. */
static size_t ascii2unicode_locale(mdb_locale_t locale, const char *in_ptr, size_t len_in, char *dest, size_t dlen) {
    size_t i, count;
    char *in = g_strndup(in_ptr, len_in);
    wchar_t *w = malloc((len_in+1)*sizeof(wchar_t));

#if defined(_WIN32) || defined(WIN32) || defined(_WIN64) || defined(WIN64) || defined(WINDOWS)
    count = _mbstowcs_l(w, in, len_in+1, locale);
#elif defined(HAVE_MBSTOWCS_L)
    count = mbstowcs_l(w, in, len_in+1, locale);
#else
    locale_t oldlocale = uselocale(locale);
    count = mbstowcs(w, in, len_in+1);
    uselocale(oldlocale);
#endif
    g_free(in);
    if (count == (size_t)-1) {
        free(w);
        return count;
    }
    for (i=0; i<count && 2*i+1<dlen; i++) {
        unsigned long c = (unsigned long)w[i] > 0xFFFF ? '?' : (unsigned long)w[i];
        dest[2*i] = c & 0xFF;
        dest[2*i+1] = c >> 8;
    }
    free(w);
    return 2*i;
}

static size_t decompressed_to_utf8_without_iconv(MdbHandle *mdb, const char *in_ptr, size_t len_in, char *dest, size_t dlen) {
	if (IS_JET3(mdb)) {
		if (mdb->f->code_page == 1252) {
			return cp1252_to_utf8_without_iconv(in_ptr, len_in, dest, dlen);
		}
		int count = 0;
		snprintf(dest, dlen, "%.*s%n", (int)len_in, in_ptr, &count);
		return count;
	}
    return unicode2ascii_locale(mdb->locale, in_ptr, len_in, dest, dlen);
}
#endif

/*
 * This function is used in reading text data from an MDB table.
 * 'dest' will receive a converted, null-terminated string.
 * dlen is the available size of the destination buffer.
 * Returns the length of the converted string, not including the terminator.
 */
int
mdb_unicode2ascii(MdbHandle *mdb, const char *src, size_t slen, char *dest, size_t dlen)
{
	char *tmp = NULL;
	size_t len_in;
	const char *in_ptr = NULL;

	if ((!src) || (!dest) || (!dlen))
		return 0;

	/* Uncompress 'Unicode Compressed' string into tmp */
	if (!IS_JET3(mdb) && (slen>=2)
			&& ((src[0]&0xff)==0xff) && ((src[1]&0xff)==0xfe)) {
		tmp = g_malloc(slen*2);
		len_in = decompress_unicode(src + 2, slen - 2, tmp, slen * 2);
		in_ptr = tmp;
	} else {
		len_in = slen;
		in_ptr = src;
	}

#ifdef HAVE_ICONV
	dlen = decompressed_to_utf8_with_iconv(mdb, in_ptr, len_in, dest, dlen);
#else
	dlen = decompressed_to_utf8_without_iconv(mdb, in_ptr, len_in, dest, dlen);
#endif

	if (tmp) g_free(tmp);
	return dlen;
}

/*
 * This function is used in writing text data to an MDB table.
 * If slen is 0, strlen will be used to calculate src's length.
 */
int
mdb_ascii2unicode(MdbHandle *mdb, const char *src, size_t slen, char *dest, size_t dlen)
{
        size_t len_in, len_out;
        const char *in_ptr = NULL;
        char *out_ptr = NULL;

	if ((!src) || (!dest) || (!dlen))
		return 0;

        in_ptr = src;
        out_ptr = dest;
        len_in = (slen) ? slen : strlen(in_ptr);
        len_out = dlen;

#ifdef HAVE_ICONV
	iconv(mdb->iconv_out, (ICONV_CONST char **)&in_ptr, &len_in, &out_ptr, &len_out);
	//printf("len_in %d len_out %d\n", len_in, len_out);
	dlen -= len_out;
#else
	/* the inverse of decompressed_to_utf8_without_iconv() */
	if (IS_JET3(mdb)) {
		if (mdb->f->code_page == 1252) {
			dlen = utf8_to_cp1252_without_iconv(in_ptr, len_in, out_ptr, len_out);
		} else {
			int count;
			snprintf(out_ptr, len_out, "%.*s%n", (int)len_in, in_ptr, &count);
			dlen = count;
		}
	} else if ((dlen = ascii2unicode_locale(mdb->locale, in_ptr, len_in, out_ptr, len_out)) == (size_t)-1) {
		/* not valid in the locale: copy the bytes */
		unsigned int i;
		slen = MIN(len_in, len_out/2);
		dlen = slen*2;
		for (i=0; i<slen; i++) {
			out_ptr[i*2] = in_ptr[i];
			out_ptr[i*2+1] = 0;
		}
	}
#endif

	/* Unicode Compression */
	if(!IS_JET3(mdb) && (dlen>4)) {
		unsigned char *tmp = g_malloc(dlen);
		unsigned int tptr = 0, dptr = 0;
		int comp = 1;

		tmp[tptr++] = 0xff;
		tmp[tptr++] = 0xfe;
		while((dptr < dlen) && (tptr < dlen)) {
			if (((dest[dptr+1]==0) && (comp==0))
			 || ((dest[dptr+1]!=0) && (comp==1))) {
				/* switch encoding mode */
				tmp[tptr++] = 0;
				comp = (comp) ? 0 : 1;
			} else if (dest[dptr]==0) {
				/* this string cannot be compressed */
				tptr = dlen;
			} else if (comp==1) {
				/* encode compressed character */
				tmp[tptr++] = dest[dptr];
				dptr += 2;
			} else if (tptr+1 < dlen) {
				/* encode uncompressed character */
				tmp[tptr++] = dest[dptr];
				tmp[tptr++] = dest[dptr+1];
				dptr += 2;
			} else {
				/* could not encode uncompressed character
				 * into single byte */
				tptr = dlen;
			}
		}
		if (tptr < dlen) {
			memcpy(dest, tmp, tptr);
			dlen = tptr;
		}
		g_free(tmp);
	}

	return dlen;
}

const char*
mdb_target_charset(MdbHandle *mdb)
{
#ifdef HAVE_ICONV
	const char *iconv_code = getenv("MDBICONV");
	if (!iconv_code)
		iconv_code = "UTF-8";
	return iconv_code;
#else
	if (!IS_JET3(mdb))
		return "ISO-8859-1";
	return NULL; // same as input: unknown
#endif
}

/* See: https://docs.microsoft.com/en-us/windows/win32/Intl/code-page-identifiers */
#ifdef HAVE_ICONV
static const char *mdb_iconv_name_from_code_page(int code_page) {
	const char *jet3_iconv_code = NULL;
	switch (code_page) {
		case 437: jet3_iconv_code="IBM437"; break;
		case 850: jet3_iconv_code="IBM850"; break;
		case 852: jet3_iconv_code="IBM852"; break;
		case 855: jet3_iconv_code="IBM855"; break;
		case 860: jet3_iconv_code="IBM860"; break;
		case 861: jet3_iconv_code="IBM861"; break;
		case 862: jet3_iconv_code="IBM862"; break;
		case 863: jet3_iconv_code="IBM863"; break;
		case 864: jet3_iconv_code="IBM864"; break;
		case 865: jet3_iconv_code="IBM865"; break;
		case 866: jet3_iconv_code="IBM866"; break;
		case 869: jet3_iconv_code="IBM869"; break;
		case 874: jet3_iconv_code="WINDOWS-874"; break;
		case 932: jet3_iconv_code="SHIFT-JIS"; break;
		case 936: jet3_iconv_code="WINDOWS-936"; break;
		case 950: jet3_iconv_code="BIG-5"; break;
		case 951: jet3_iconv_code="BIG5-HKSCS"; break;
		case 1200: jet3_iconv_code="UTF-16LE"; break;
		case 1201: jet3_iconv_code="UTF-16BE"; break;
		case 1250: jet3_iconv_code="WINDOWS-1250"; break;
		case 1251: jet3_iconv_code="WINDOWS-1251"; break;
		case 1252: jet3_iconv_code="WINDOWS-1252"; break;
		case 1253: jet3_iconv_code="WINDOWS-1253"; break;
		case 1254: jet3_iconv_code="WINDOWS-1254"; break;
		case 1255: jet3_iconv_code="WINDOWS-1255"; break;
		case 1256: jet3_iconv_code="WINDOWS-1256"; break;
		case 1257: jet3_iconv_code="WINDOWS-1257"; break;
		case 1258: jet3_iconv_code="WINDOWS-1258"; break;
		case 1361: jet3_iconv_code="CP1361"; break;
		case 12000: jet3_iconv_code="UTF-32LE"; break;
		case 12001: jet3_iconv_code="UTF-32BE"; break;
		case 20866: jet3_iconv_code="KOI8-R"; break;
		case 20932: jet3_iconv_code="EUC-JP"; break;
		case 21866: jet3_iconv_code="KOI8-U"; break;
		case 28591: jet3_iconv_code="ISO-8859-1"; break;
		case 28592: jet3_iconv_code="ISO-8859-2"; break;
		case 28593: jet3_iconv_code="ISO-8859-3"; break;
		case 28594: jet3_iconv_code="ISO-8859-4"; break;
		case 28595: jet3_iconv_code="ISO-8859-5"; break;
		case 28596: jet3_iconv_code="ISO-8859-6"; break;
		case 28597: jet3_iconv_code="ISO-8859-7"; break;
		case 28598: jet3_iconv_code="ISO-8859-8"; break;
		case 28599: jet3_iconv_code="ISO-8859-9"; break;
		case 28503: jet3_iconv_code="ISO-8859-13"; break;
		case 28505: jet3_iconv_code="ISO-8859-15"; break;
		case 51932: jet3_iconv_code="EUC-JP"; break;
		case 51936: jet3_iconv_code="EUC-CN"; break;
		case 51949: jet3_iconv_code="EUC-KR"; break;
		case 65000: jet3_iconv_code="UTF-7"; break;
		case 65001: jet3_iconv_code="UTF-8"; break;
		default: break;
	}
	return jet3_iconv_code;
}
#endif

void mdb_iconv_init(MdbHandle *mdb)
{
	const char *iconv_code;

	/* check environment variable */
	if (!(iconv_code=getenv("MDBICONV"))) {
		iconv_code="UTF-8";
	}

#ifdef HAVE_ICONV
	if (!IS_JET3(mdb)) {
		mdb->iconv_out = iconv_open("UCS-2LE", iconv_code);
		mdb->iconv_in = iconv_open(iconv_code, "UCS-2LE");
	} else {
		/* check environment variable */
		const char *jet3_iconv_code = getenv("MDB_JET3_CHARSET");

		if (!jet3_iconv_code) {
			/* Use code page embedded in the database */
			/* Note that individual columns can override this value,
			 * but per-column code pages are not supported by libmdb */
			jet3_iconv_code = mdb_iconv_name_from_code_page(mdb->f->code_page);
		}
		if (!jet3_iconv_code) {
			jet3_iconv_code = "CP1252";
		}

		mdb->iconv_out = iconv_open(jet3_iconv_code, iconv_code);
		mdb->iconv_in = iconv_open(iconv_code, jet3_iconv_code);
	}
#elif defined(_WIN32) || defined(WIN32) || defined(_WIN64) || defined(WIN64) || defined(WINDOWS)
    mdb->locale = _create_locale(LC_CTYPE, ".65001");
#else
    mdb->locale = newlocale(LC_CTYPE_MASK, "C.UTF-8", NULL);
#endif
}
void mdb_iconv_close(MdbHandle *mdb)
{
#ifdef HAVE_ICONV
    if (mdb->iconv_out != (iconv_t)-1) iconv_close(mdb->iconv_out);
    if (mdb->iconv_in != (iconv_t)-1) iconv_close(mdb->iconv_in);
#elif defined(_WIN32) || defined(WIN32) || defined(_WIN64) || defined(WIN64) || defined(WINDOWS)
    if (mdb->locale) _free_locale(mdb->locale);
#else
    if (mdb->locale) freelocale(mdb->locale);
#endif
}


