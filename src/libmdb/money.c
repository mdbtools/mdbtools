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

#define MAXPRECISION 9
/* 
** these routines are copied from the freetds project which does something
** very similiar
*/

static int multiply_byte(unsigned char *product, int num, unsigned char *multiplier);
static int do_carry(unsigned char *product);
static char *array_to_string(unsigned char *array, int scale, char *s);

char *mdb_money_to_string(MdbHandle *mdb, int start, char *s)
{
unsigned char multiplier[MAXPRECISION], temp[MAXPRECISION];
unsigned char product[MAXPRECISION];
unsigned char *money;
int num_bytes = 8;
int i;
int pos;
int neg=0;

	memset(multiplier,0,MAXPRECISION);
	memset(product,0,MAXPRECISION);
	multiplier[0]=1;

	money = &mdb->pg_buf[start];

	if (money[7] && 0x01) {
		/* negative number -- preform two's complement */
		neg = 1;
		for (i=0;i<num_bytes;i++) {
			money[i] = ~money[i];
		}
		for (i=0; i<num_bytes; i++) {
			money[i] += 1;
			if (money[i]!=0) break;
		}
	}

	money[7]=0;
	for (pos=0;pos<num_bytes;pos++) {
		multiply_byte(product, money[pos], multiplier);

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
int i, top, j, start;

	number[0]=num%10;
	number[1]=(num/10)%10;
	number[2]=(num/100)%10;

	for (top=MAXPRECISION-1;top>=0 && !multiplier[top];top--);
	start=0;
	for (i=0;i<=top;i++) {
		for (j=0;j<3;j++) {
			product[j+start]+=multiplier[i]*number[j];
		}
		do_carry(product);
		start++;
	}
	return 0;
}
static int do_carry(unsigned char *product)
{
int j;

	for (j=0;j<MAXPRECISION;j++) {
		if (product[j]>9) {
			product[j+1]+=product[j]/10;
			product[j]=product[j]%10;
		}
	}
	return 0;
}
static char *array_to_string(unsigned char *array, int scale, char *s)
{
int top, i, j;
	
	for (top=MAXPRECISION-1;top>=0 && top>scale && !array[top];top--);

	if (top == -1)
	{
		s[0] = '0';
		s[1] = '\0';
		return s;
	}

	j=0;
	for (i=top;i>=0;i--) {
		if (top+1-j == scale) s[j++]='.';
		s[j++]=array[i]+'0';
	}
	s[j]='\0';
}
