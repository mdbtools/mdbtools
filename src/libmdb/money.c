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
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include <stdio.h>
#include "mdbtools.h"

#ifdef DMALLOC
#include "dmalloc.h"
#endif

#define MAX_NUMERIC_PRECISION 28
/* 
** these routines are copied from the freetds project which does something
** very similiar
*/

static int multiply_byte(unsigned char *product, int num, unsigned char *multiplier);
static int do_carry(unsigned char *product);
static char *array_to_string(unsigned char *array, int unsigned scale, int neg);

/**
 * mdb_money_to_string
 * @mdb: Handle to open MDB database file
 * @start: Offset of the field within the current page
 *
 * Returns: the allocated string that has received the value.
 */
char *mdb_money_to_string(MdbHandle *mdb, int start)
{
       int num_bytes=8, prec=19, scale=4;
	int i;
	int neg=0;
       unsigned char multiplier[MAX_NUMERIC_PRECISION], temp[MAX_NUMERIC_PRECISION];
       unsigned char product[MAX_NUMERIC_PRECISION];
       unsigned char bytes[num_bytes];

       memset(multiplier,0,MAX_NUMERIC_PRECISION);
       memset(product,0,MAX_NUMERIC_PRECISION);
	multiplier[0]=1;
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
               multiply_byte(product, bytes[i], multiplier);

		/* multiplier = multiplier * 256 */
               memcpy(temp, multiplier, MAX_NUMERIC_PRECISION);
               memset(multiplier, 0, MAX_NUMERIC_PRECISION);
		multiply_byte(multiplier, 256, temp);
	}
       return array_to_string(product, scale, neg);

}

char *mdb_numeric_to_string(MdbHandle *mdb, int start, int prec, int scale) {
       int num_bytes = 16;
       int i;
       int neg=0;
       unsigned char multiplier[MAX_NUMERIC_PRECISION], temp[MAX_NUMERIC_PRECISION];
       unsigned char product[MAX_NUMERIC_PRECISION];
       unsigned char bytes[num_bytes];

       memset(multiplier,0,MAX_NUMERIC_PRECISION);
       memset(product,0,MAX_NUMERIC_PRECISION);
       multiplier[0]=1;
       memcpy(bytes, mdb->pg_buf + start + 1, num_bytes);

       /* Perform two's complement for negative numbers */
       if (mdb->pg_buf[start] & 0x80) neg = 1;
       for (i=0;i<num_bytes;i++) {
               /* product += multiplier * current byte */
               multiply_byte(product, bytes[12-4*(i/4)+i%4], multiplier);

               /* multiplier = multiplier * 256 */
               memcpy(temp, multiplier, MAX_NUMERIC_PRECISION);
               memset(multiplier, 0, MAX_NUMERIC_PRECISION);
               multiply_byte(multiplier, 256, temp);
       }
       return array_to_string(product, scale, neg);
}

static int multiply_byte(unsigned char *product, int num, unsigned char *multiplier)
{
	unsigned char number[3];
	unsigned int i, j;

	number[0]=num%10;
	number[1]=(num/10)%10;
	number[2]=(num/100)%10;

       for (i=0;i<MAX_NUMERIC_PRECISION;i++) {
		if (multiplier[i] == 0) continue;
		for (j=0;j<3;j++) {
			if (number[j] == 0) continue;
			product[i+j] += multiplier[i]*number[j];
		}
		do_carry(product);
	}
	return 0;
}
static int do_carry(unsigned char *product)
{
	unsigned int j;

       for (j=0;j<MAX_NUMERIC_PRECISION-1;j++) {
		if (product[j]>9) {
			product[j+1]+=product[j]/10;
			product[j]=product[j]%10;
		}
	}
	if (product[j]>9) {
		product[j]=product[j]%10;
	}
	return 0;
}
static char *array_to_string(unsigned char *array, unsigned int scale, int neg)
{
	char *s;
	unsigned int top, i, j=0;
	
       for (top=MAX_NUMERIC_PRECISION;(top>0) && (top-1>scale) && !array[top-1];top--);

       /* allocate enough space for all digits + minus sign + decimal point + trailing NULL byte */
       s = (char *) g_malloc(MAX_NUMERIC_PRECISION+3);

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
