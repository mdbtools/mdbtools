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
   int length;

   in = fopen(argv[1],"r");
   while (length = fread(data,1,16,in)) {
      fprintf(stdout, "%06x  ", i);
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

