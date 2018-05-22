#include "mh.h"
#include "stat.h"

/* annotate file component data
 *
 * prepends   Component: data
 *              date stamp
 */

/*extern int inplace;*/            /* preserve links in anno */
int inplace 1;		            /* preserve links in anno */

annotate(file, comp, text)
char *file, *comp, *text;
{
	register int src, tmp;
	register char *cp;
	int cnt;
	char buf[512], *sp, tmpfil[128];
	long now;
	struct inode stbuf;

	if((src = open((cp = file), 2)) == -1) { /* this should be an X-open*/
		printf("Can't open "); flush();
		perror(cp);
		return(1);
	}
	copy(cp, buf);
	sp = cp = buf;
	while(*cp) if(*cp++ == '/') sp = cp;
	if(sp != buf) {
		*sp = 0;
		cp = copy(buf, tmpfil);
	} else
		cp = tmpfil;
	copy(makename("annot",".tmp"), cp);
	fstat(src, &stbuf);
	if((tmp = creat(tmpfil, stbuf.i_mode&0777)) == -1) {
		printf("Can't create "); flush();
		perror(tmpfil);
		return(1);
	}
	cp = comp;
	if(*cp >= 'a' && *cp <= 'z') *cp =- 040;
	type(tmp, cp);
	type(tmp, ": ");
	time(&now);
	cp = cdate(&now);
	cp[9] = ' ';  cp[15] = 0;
	if(*cp == ' ') cp++;
	type(tmp, "<<");
	type(tmp, cp);
	type(tmp, ">>\n");
	cp = text;
	do {
		if(*cp == ' ' || *cp == '\t') cp++;
		sp = cp;
		while(*cp && *cp++ != '\n') ;
		if(cp - sp) {
			type(tmp, comp);
			type(tmp, ": ");
			write(tmp, sp, cp-sp);
		}
	} while(*cp);
	if(cp[-1] != '\n' && cp != text) type(tmp, "\n");
	do
		if((cnt = read(src, buf, sizeof buf)) > 0)
			write(tmp, buf, cnt);
	while(cnt == sizeof buf);
	if(inplace) {
		close(tmp);
		tmp = open(tmpfil, 0);          /* reopen for reading */
		seek(src, 0, 0);
		do
			if((cnt = read(tmp, buf, sizeof buf)) > 0)
				write(src, buf, cnt);
		while(cnt == sizeof buf);
	} else {
	   /*   cp = copy(file, buf);           */
	   /*   *--cp =| 0200;                  */
	   /*   copy(".bak", copy(file, buf));  */
		cp = copy(file, buf);
		cp[1] = 0;
		do
			*cp = cp[-1];
		while(--cp >= buf && *cp != '/');
		*++cp = ',';                    /* New backup convention */
		unlink(buf);
		if(link(file, buf) == -1) {
			printf("Can't rename %s to bak file.\n", file);
			return(1);
		}
		if(unlink(file) == -1) {
			printf("Can't unlink %s\n", file);
			return(1);
		}
		if(link(tmpfil, file) == -1) {
			printf("Can't lnk temp file \"%s\" to %s\n",
			  tmpfil, file);
			return(1);
		}
	}
	close(src);
	close(tmp);
	unlink(tmpfil);
	return(0);
}
