#include "stdio.h"
#include "ctype.h"
#define	LIBN
#include "netlib.h"
#include "netfiles.h"
/*
 * return name of local host
 */
char *
thisname()
{
	register int c;
	register char *cp;
	register FILE *fp;
	static char buf[NETNAMSIZ+1];

	fp = fopen(THISHOST, "r");
	if (fp == NULL)
		return HOSTNAME;
	cp = buf;
	while (cp < &buf[NETNAMSIZ] && (c = getc(fp)) != EOF && c != '\n')
		*cp++ = islower(c)? toupper(c) : c;
	*cp = '\0';
	fclose(fp);
	return buf;
}
