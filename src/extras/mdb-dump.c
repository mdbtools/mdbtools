/* utility program to make a hex dump of a binary file (such as a mdb file) */
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <netdb.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
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
   long                 i=0;
   int                  j;
   unsigned char        data[17];
   FILE                 *in;
   int                  length;
   int                  pg=0;
   char                 addr[10];
   int                  jet4 = 0;

   if (argc < 1) {
	fprintf(stderr, "Usage: mdb-dump <filename>\n\n");
	exit(1);
   }
   if ((in = fopen(argv[1],"r"))==NULL) {
	fprintf(stderr, "Couldn't open file %s\n", argv[1]);
	exit(1);
   }
   fseek(in,0x14,SEEK_SET);
   fread(data,1,1,in);
   if (data[0]==0x01) {
	jet4 = 1;
   } 
   fseek(in,0,SEEK_SET);
   while (length = fread(data,1,16,in)) {
      sprintf(addr, "%06x", i);
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
} 

