#include <ctype.h>
#include <string.h>
#include <stdio.h>

#ifdef DMALLOC
#include "dmalloc.h"
#endif

void buffer_dump(const unsigned char* buf, int start, int end)
{
	char asc[20];
	int j, k;

	memset(asc, 0, sizeof(asc));
	k = 0;
	for (j=start; j<=end; j++) {
		if (k == 0) {
			fprintf(stdout, "%04x  ", j);
		}
		fprintf(stdout, "%02x ", buf[j]);
		asc[k] = isprint(buf[j]) ? buf[j] : '.';
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
