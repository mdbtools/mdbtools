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

#define MAXPRECISION 20
/* 
** these routines are copied from the freetds project which does something
** very similiar
*/

static int multiply_byte(unsigned char *product, int num, unsigned char *multiplier);
static int do_carry(unsigned char *product);
static char *array_to_string(unsigned char *array, int unsigned scale, char *s);

/**
 * mdb_money_to_string
 * @mdb: Handle to open MDB database file
 * @start: Offset of the field within the current page
 * @s: String that will receieve the value
 *
 * Returns: the string that has received the value.
 */
char *mdb_money_to_string(MdbHandle *mdb, int start, char *s)
{
	int num_bytes = 8;
	int i;
	int neg=0;
	unsigned char multiplier[MAXPRECISION], temp[MAXPRECISION];
	unsigned char product[MAXPRECISION];
	unsigned char money[num_bytes];

	memset(multiplier,0,MAXPRECISION);
	memset(product,0,MAXPRECISION);
	multiplier[0]=1;
	memcpy(money, mdb->pg_buf + start, num_bytes);

	/* Perform two's complement for negative numbers */
	if (money[7] & 0x80) {
		neg = 1;
		for (i=0;i<num_bytes;i++) {
			money[i] = ~money[i];
		}
		for (i=0; i<num_bytes; i++) {
			money[i] ++;
			if (money[i]!=0) break;
		}
	}

	for (i=0;i<num_bytes;i++) {
		/* product += multiplier * current byte */
		multiply_byte(product, money[i], multiplier);

		/* multiplier = multiplier * 256 */
		memcpy(temp, multiplier, MAXPRECISION);
		memset(multiplier,0,MAXPRECISION);
		multiply_byte(multiplier, 256, temp);
	}
	if (neg) {
		s[0]='-';
		array_to_string(product, 4, &s[1]);
	} else {
		array_to_string(product, 4, s);
	}
	return s;
}
static int multiply_byte(unsigned char *product, int num, unsigned char *multiplier)
{
	unsigned char number[3];
	unsigned int i, j;

	number[0]=num%10;
	number[1]=(num/10)%10;
	number[2]=(num/100)%10;

	for (i=0;i<MAXPRECISION;i++) {
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

	for (j=0;j<MAXPRECISION-1;j++) {
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
static char *array_to_string(unsigned char *array, unsigned int scale, char *s)
{
	unsigned int top, i, j=0;
	
	for (top=MAXPRECISION;(top>0) && (top-1>scale) && !array[top-1];top--);

	if (top == 0) {
		s[j++] = '0';
	} else {
		for (i=top; i>0; i--) {
			if (j == top-scale) s[j++]='.';
			s[j++]=array[i-1]+'0';
		}
	}
	s[j]='\0';

	return s;
}
