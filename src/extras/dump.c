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

int main(int argc, char **argv)
{
   long                 i=0;
   int                  j;
   unsigned char        data[17];
   FILE                 *in;
   int                  length;
   int                  pg=0;
   char                 addr[10];

   in = fopen(argv[1],"r");
   while (length = fread(data,1,16,in)) {
      sprintf(addr, "%06x", i);
      if (!strcmp(&addr[3],"000") || ! strcmp(&addr[3],"800")) {
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

