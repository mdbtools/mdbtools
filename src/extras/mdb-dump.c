/* utility program to make a hex dump of a binary file (such as a mdb file) */

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <time.h>
#include <limits.h>
#include <assert.h>
#include <ctype.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <ctype.h>
#include <string.h>

int main(int argc, char **argv)
{
   unsigned long        i=0;
   unsigned int         j;
   unsigned char        data[17];
   FILE                 *in;
   size_t               length;
   int                  pg=0;
   char                 addr[10];
   int                  jet4 = 0;
   int			pg_size = 2048;

   if (argc < 2) {
	fprintf(stderr, "Usage: mdb-dump <filename> [<page number>]\n\n");
	exit(1);
   }
   if (argc>2) {
	   if (!strncmp(argv[2],"0x",2)) {
		   for (i=2;i<strlen(argv[2]);i++) {
			   pg *= 16;
			   //if (islower(argv[2][i]))
				//argv[2][i] += 0x20;
			   printf("char %c\n", (int)argv[2][i]);
		   	   pg += argv[2][i] > '9' ? argv[2][i] - 'a' + 10 : argv[2][i] - '0';
		   }
	   } else {
		   pg = atol(argv[2]);
	   }
   }
   printf("page num %d\n", pg);
   if ((in = fopen(argv[1],"r"))==NULL) {
	fprintf(stderr, "Couldn't open file %s\n", argv[1]);
	exit(1);
   }
   fseek(in,0x14,SEEK_SET);
   length = fread(data,1,1,in);
   if (!length) {
	fprintf(stderr, "fread failed at position 0x14\n");
	exit(1);
   }
   if (data[0]) {
	jet4 = 1;
	pg_size = 4096;
   } 
   fseek(in,(pg*pg_size),SEEK_SET);
   i = 0;
   while ((length = fread(data,1,16,in))) {
      snprintf(addr, sizeof(addr), "%06lx", i);
      //if (!strcmp(&addr[3],"000") || (!jet4 && !strcmp(&addr[3],"800")) &&
		      //pg) break;
      if (!strcmp(&addr[3],"000") || (!jet4 && !strcmp(&addr[3],"800"))) {
	fprintf(stdout,"-- Page 0x%04x (%d) --\n", pg, pg);
	pg++;
      }
      fprintf(stdout,"%s  ", addr);
      i+=length;

      for(j=0; j<length; j++) {
         fprintf(stdout, "%02x ", data[j]);
      }
         
      fprintf(stdout, "  |");

      for(j=0; j<length; j++) {
         fprintf(stdout, "%c", (isprint(data[j])) ? data[j] : '.');
      }
      fprintf(stdout, "|\n");
   }

   exit(0);
} 

