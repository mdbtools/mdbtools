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

#include "mdbprivate.h"

typedef struct
{
	unsigned char state[256];
	unsigned char x;
	unsigned char y;
} RC4_KEY;

#define swap_byte(x,y) t = *(x); *(x) = *(y); *(y) = t


static void RC4_set_key(RC4_KEY *key, int key_data_len, unsigned char *key_data_ptr)
{
	unsigned char t;
	unsigned char index1;
	unsigned char index2;
	unsigned char* state;
	short counter;

	state = &key->state[0];
	for(counter = 0; counter < 256; counter++)
		state[counter] = counter;
	key->x = 0;
	key->y = 0;
	index1 = 0;
	index2 = 0;
	for(counter = 0; counter < 256; counter++) {
		index2 = (key_data_ptr[index1] + state[counter] + index2) % 256;
		swap_byte(&state[counter], &state[index2]);
		index1 = (index1 + 1) % key_data_len;
	}
}

/*
 * this algorithm does 'encrypt in place' instead of inbuff/outbuff
 * note also: encryption and decryption use same routine
 * implementation supplied by (Adam Back) at <adam at cypherspace dot org>
 */
static void RC4(RC4_KEY *key, int buffer_len, unsigned char * buff)
{
	unsigned char t;
	unsigned char x;
	unsigned char y;
	unsigned char* state;
	unsigned char xorIndex;
	short counter;

	x = key->x;
	y = key->y;
	state = &key->state[0];
	for(counter = 0; counter < buffer_len; counter++) {
		x = (x + 1) % 256;
		y = (state[x] + y) % 256;
		swap_byte(&state[x], &state[y]);
		xorIndex = (state[x] + state[y]) % 256;
		buff[counter] ^= state[xorIndex];
	}
	key->x = x;
	key->y = y;
}

void mdbi_rc4(unsigned char *key, guint32 key_len, unsigned char *buf, guint32 buf_len) {
    RC4_KEY rc4_key;
    RC4_set_key(&rc4_key, key_len, key);
    RC4(&rc4_key, buf_len, buf);
}