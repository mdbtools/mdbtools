#include <stdio.h>

int likecmp(char *s, char *r)
{
int i, ret;

	switch (r[0]) {
		case '\0':
			if (s[0]=='\0') {
				return 1;
			} else {
				return 0;
			}
		case '_':
			/* skip one character */
			return likecmp(&s[1],&r[1]);
		case '%':
			/* skip any number of characters */
			/* the strlen(s)+1 is important so the next call can */
			/* if there are trailing characters */
			for(i=0;i<strlen(s)+1;i++) {
				if (likecmp(&s[i],&r[1])) {
					return 1;
				}
			}
			return 0;
		default:
			for(i=0;i<strlen(r);i++) {
				if (r[i]=='_' || r[i]=='%') break;
			}
			if (strncmp(s,r,i)) {
				return 0;
			} else {
				ret = likecmp(&s[i],&r[i]);
				return ret;
			}
	}
}
