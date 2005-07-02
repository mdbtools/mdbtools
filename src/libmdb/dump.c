#include <ctype.h>
#include <string.h>
#include <stdio.h>

#ifdef DMALLOC
#include "dmalloc.h"
#endif

void buffer_dump(const void* buf, int start, size_t len)
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
