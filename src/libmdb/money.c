/* MDB Tools - A library for reading MS Access database file
 * Copyright (C) 1998-1999  Brian Bruns
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

#include <stdio.h>
#include "mdbtools.h"

#define MAX_MONEY_PRECISION   20
#define MAX_NUMERIC_PRECISION 40
/* 
** these routines are copied from the freetds project which does something
** very similiar
*/

static int multiply_byte(unsigned char *product, int num, unsigned char *multiplier, size_t len);
static int do_carry(unsigned char *product, size_t len);
static char *array_to_string(unsigned char *array, size_t len, int unsigned scale, int neg);

/**
 * mdb_money_to_string
 * @mdb: Handle to open MDB database file
 * @start: Offset of the field within the current page
 *
 * Returns: the allocated string that has received the value.
 */
char *mdb_money_to_string(MdbHandle *mdb, int start)
{
	const int num_bytes=8, scale=4;
	int i;
	int neg=0;
	unsigned char multiplier[MAX_MONEY_PRECISION] = { 1 };
	unsigned char temp[MAX_MONEY_PRECISION];
	unsigned char product[MAX_MONEY_PRECISION] = { 0 };
	unsigned char bytes[num_bytes];

	memcpy(bytes, mdb->pg_buf + start, num_bytes);

	/* Perform two's complement for negative numbers */
	if (bytes[num_bytes-1] & 0x80) {
		neg = 1;
		for (i=0;i<num_bytes;i++) {
                       bytes[i] = ~bytes[i];
		}
		for (i=0; i<num_bytes; i++) {
                       bytes[i] ++;
                       if (bytes[i]!=0) break;
		}
	}

	for (i=0;i<num_bytes;i++) {
		/* product += multiplier * current byte */
               multiply_byte(product, bytes[i], multiplier, sizeof(multiplier));

		/* multiplier = multiplier * 256 */
               memcpy(temp, multiplier, sizeof(multiplier));
               memset(multiplier, 0, sizeof(multiplier));
		multiply_byte(multiplier, 256, temp, sizeof(multiplier));
	}
       return array_to_string(product, sizeof(product), scale, neg);

}

char *mdb_numeric_to_string(MdbHandle *mdb, int start, int scale, int prec) {
       const int num_bytes = 16;
       int i;
       int neg=0;
       unsigned char multiplier[MAX_NUMERIC_PRECISION] = { 1 };
       unsigned char temp[MAX_NUMERIC_PRECISION];
       unsigned char product[MAX_NUMERIC_PRECISION] = { 0 };
       unsigned char bytes[num_bytes];

       memcpy(bytes, mdb->pg_buf + start + 1, num_bytes);

       /* Negative bit is stored in first byte */
       if (mdb->pg_buf[start] & 0x80) neg = 1;
       for (i=0;i<num_bytes;i++) {
               /* product += multiplier * current byte */
               multiply_byte(product, bytes[12-4*(i/4)+i%4], multiplier, sizeof(multiplier));

               /* multiplier = multiplier * 256 */
               memcpy(temp, multiplier, sizeof(multiplier));
               memset(multiplier, 0, sizeof(multiplier));
               multiply_byte(multiplier, 256, temp, sizeof(multiplier));
       }
       return array_to_string(product, sizeof(product), prec, neg);
}

static int multiply_byte(unsigned char *product, int num, unsigned char *multiplier, size_t len)
{
	unsigned char number[3] = { num % 10, (num/10) % 10, (num/100) % 10 };
	unsigned int i, j;

	for (i=0;i<len;i++) {
		if (multiplier[i] == 0) continue;
		for (j=0;j<3 && i+j<len;j++) {
			if (number[j] == 0) continue;
			product[i+j] += multiplier[i]*number[j];
		}
		do_carry(product, len);
	}
	return 0;
}
static int do_carry(unsigned char *product, size_t len)
{
	unsigned int j;

       for (j=0;j<len-1;j++) {
		if (product[j]>9) {
			product[j+1]+=product[j]/10;
			product[j]%=10;
		}
	}
	if (product[j]>9) {
		product[j]%=10;
	}
	return 0;
}
static char *array_to_string(unsigned char *array, size_t len, unsigned int scale, int neg)
{
	char *s;
	unsigned int top, i, j=0;
	
       for (top=len;(top>0) && (top-1>scale) && !array[top-1];top--);

       /* allocate enough space for all digits + minus sign + decimal point + trailing NULL byte */
       s = g_malloc(len+3);

	if (neg)
		s[j++] = '-';

	if (top == 0) {
		s[j++] = '0';
	} else {
		for (i=top; i>0; i--) {
			if (i == scale) s[j++]='.';
			s[j++]=array[i-1]+'0';
		}
	}
	s[j]='\0';

	return s;
}
