#include "mh.h"
#include "iobuf.h"

#define FROM    13              /* Start of From field          */
#define SFROM   16              /* Length of "        "         */
#define DATE     5              /* Start of Date field          */
#define SDATE    7              /* Length                       */
#define SUBJ    31              /* Start of Subject field       */
#define SSUBJ   (79-SUBJ)       /* Size of Subject field        */
#define BSUBJ   20              /* Room needed in Sub field to  */
				/* add stuff from the body      */
#define MSGN     0              /* Start of msg name field      */
#define SMSGN    3              /* Length                       */
#define FLGS     3              /* Start of flags field         */
#define SFLGS    2              /* Width of flag field          */

struct  iobuf scnout, fout;
int errno;
char scanl[82];

scan(inb, innum, outnum, curflg)
struct iobuf *inb;
int outnum;
{

	char buf[512], name[NAMESZ], tobuf[32], frombuf[32];
	register char *cp;
	int state, subsz, first, compnum;
	static char *myname;

	if(!myname)
		myname = getlogn(getruid());
	tobuf[0] = frombuf[0] = 0;
	errno = first = 0;
	state = FLD;
	compnum = 1;

	for(;;) {

		state = m_getfld(state, name, buf, sizeof buf, inb);
		if(!first++ && state >= FLD && state != FILEEOF) {
		    if(outnum) {
			if(fcreat((cp = m_name(outnum)),&scnout)<0) {
				printf("Error creating msg "); flush();
				perror(cp); exit(-1);
			}
			chmod(cp, m_gmprot());
		    }
		    sfill(scanl, sizeof scanl);
		    scanl[sizeof scanl - 1] = 0;
		    subsz = 0;
		    tobuf[0] = 0;
		}

		switch(state) {

		case FLD:
		case FLDEOF:
		case FLDPLUS:
			compnum++;
			if(uleq(name, "from"))
				frombuf[cpyfrm(buf, frombuf, sizeof frombuf)]=0;
			else if(uleq(name, "date"))
				cpydat(buf, scanl+DATE, SDATE);
			else if(uleq(name, "subject") && scanl[SUBJ] == ' ')
				subsz = cpy(buf, scanl+SUBJ, SSUBJ);
			else if(uleq(name, "to") && !tobuf[0])
				tobuf[cpyfrm(buf, tobuf, sizeof tobuf-1)] = 0;
			else if(uleq(name, "replied"))
				cpy("-", scanl+FLGS+1, 1);
			put(name, buf, &scnout);
			while(state == FLDPLUS) {
				state=m_getfld(state,name,buf,sizeof buf,inb);
				puts(buf, &scnout);
			}
			if(state == FLDEOF)
				goto putscan;
			continue;

		case BODY:
		case BODYEOF:
			compnum = -1;
			if(buf[0] && subsz < SSUBJ - BSUBJ) {
				scanl[SUBJ+subsz+1] = '<';
				scanl[SUBJ+subsz+2] = '<';
				cpy(buf, scanl+SUBJ+subsz+3, SSUBJ-subsz-3);
				subsz = SSUBJ;
			}
			if(buf[0] && scnout.b_fildes) {
				putc('\n', &scnout);
				if(errno) {
					perror("Write error:");exit(-1);
				}
			}
			puts(buf, &scnout);
			while(state == BODY) {
				state=m_getfld(state,name,buf,sizeof buf,inb);
				puts(buf, &scnout);
			}
			if(state == BODYEOF) {
		  putscan:      cpymsgn(m_name(innum), scanl+MSGN, SMSGN);
				if(!frombuf[0] || uleq(frombuf, myname)) {
					cpy("To:", scanl+FROM, 3);
					cpy(tobuf, scanl+FROM+3, SFROM-3);
				} else
					cpy(frombuf, scanl+FROM, SFROM);
				if(curflg)
					cpy("+", scanl+FLGS, SFLGS);
				trim(scanl);
				puts(scanl, &fout);

				if(scnout.b_fildes) {
					fflush(&scnout);
					if(errno) {
						perror("Write error");
						exit(-1);
					}
					close(scnout.b_fildes);
					scnout.b_fildes = 0;
				}
				return(1);
			}
			break;

		case LENERR:
		case FMTERR:
			printf("??Message Format Error ");
			if(outnum) printf("(Message %d) ", outnum);
			if(compnum < 0) printf("in the Body.\n");
			else printf("in Component #%d.\n", compnum);
			goto badret;
		default:
			printf("Getfld returned %d\n", state);
	badret:         if(scnout.b_fildes)
				fflush(&scnout);
			return(-1);
		case FILEEOF:
			return(0);

		}

	}
}


trim(str)
{
	register char *cp;

	cp = str;
	while(*cp) cp++;
	while(*--cp == ' ') ;
	cp++;
	*cp++ = '\n';
	*cp++ = 0;
}

sfill(str, cnt)
{
	register char *cp;
	register int i;

	cp = str;  i = cnt;
	do
		*cp++ = ' ';
	while(--i);
}


put(name, buf, iob)
{
	register struct iobuf *ip;

	ip = iob;
	if(ip->b_fildes) {
		puts(name, iob);
		putc(':', iob);
		if(errno) { perror("Write error");exit(-1);}
		puts(buf, iob);
	}
}


cpy(sfrom, sto, cnt)
{
	register char *from, *to;
	register int c;
	int i;

	from = sfrom;  to = sto;
	i = cnt;
	while(*from == ' ' || *from == '\t' || *from == '\n')
		from++;
	while(i--)
		if(c = *from) {
			if(c == '\t' || c == ' ' || c == '\n') {
				*to++ = ' ';
				do 
					from++;
				while((c= *from)==' '||c=='\t'||c=='\n');
				continue;
			} else
				*to++ = c;
			from++;
		} else
			break;
	return(from - sfrom - 1);
}


cpydat(sfrom, sto, cnt)
{
	register char *from, *cp;
	register int c;
	static int *locvec;
	long now;
	char *to;

	if(!locvec) {
		time(&now);
		locvec = localtime(&now);
	}
	to = sto;
	for(from = sfrom; (c = *from) < '0' || c > '9'; from++)
		if(!c)
			return;
	c = cnt;
	for(cp = from; (*cp >= '0' && *cp <= '9') || *cp == ' '; cp++);
	if(cp = findmonth(cp)) {
		if(!cp[1]) {
			*to++ = ' ';
			c--;
		}
		while(*cp && c--)
			*to++ = *cp++;
		c--;  *to++ = '/';
		if(from[1] == ' ') {
			*to++ = ' ';
			c--;
		}
		while(*from >= '0' && *from <= '9' && c--)
			*to++ = *from++;
		if(c >= 2) {
			while(*from < '0' || *from > '9') from++;
			if((c = atoi(from)) > 1970 && c-1900 < locvec[5]) {
				*to++ = '/';
				*to++ = c - 1970 + '0';
			}
		}
		return;
	}
	if(from[1] == ' ') {
		*to++ = ' ';
		c--;
	}
	while(*from && c--)
		*to++ = *from++;
}


char    *fromp, fromdlm, pfromdlm;

cpyfrm(sfrom, sto, cnt)
{
	register char *to, *cp;
	register int c;

	fromdlm = ' ';
	fromp = sfrom; to = sto;
	cp = frmtok();
	do
		if(c = *cp++)
			*to++ = c;
		else
			break;
	while(--cnt);
	for(;;) {
		if(cnt < 3) break;
		if(*(cp = frmtok()) == 0) break;
		if(*cp == '@' || uleq(cp, "at")) {
			cp = frmtok();
			if(!uleq(cp, "rand-unix")) {
				*to++ = '@';
				cnt--;
				do
					if(c = *cp++)
						*to++ = c;
					else
						break;
				while(--cnt);
			}
		} else if(cnt > 4) {
			cnt--; *to++ = pfromdlm;
			do
				if(c = *cp++)
					*to++ = c;
				else
					break;
			while(--cnt);
		}
	}
	return(to - sto);
}


frmtok()
{
	static tokbuf[64];
	register char *cp;
	register int c;

	pfromdlm = fromdlm;
	cp = tokbuf; *cp = 0;
	while(c = *fromp++) {
		if(c == '\t')
			c = ' ';
		if(c == ' ' && cp == tokbuf)
			continue;
		if(c == ' ' || c == '\n' || c == ',')
			break;
		*cp++ = c;
		*cp = 0;
		if(c == '@' || *fromp == '@' || cp == &tokbuf[63])
			break;
	}
	fromdlm = c;
	return(tokbuf);
}


/*      num specific!         */

cpymsgn(msgnam, addr, len)
char *msgnam, *addr;
{
	register char *cp, *sp;

	sp = msgnam;
	cp = addr + (len - length(sp));
	while(*sp)
		*cp++ = *sp++;
}

char *monthtab[] {
	"jan", "feb", "mar", "apr", "may", "jun",
	"jul", "aug", "sep", "oct", "nov", "dec",
};

findmonth(str)
{
	register char *cp, *sp;
	register int i;
	char buf[4];

	for(cp=str, sp=buf; (*sp++ = *cp++) && sp < &buf[3] && *cp != ' '; );
	*sp = 0;
	for(i = 0; i < 12; i++)
		if(uleq(buf, monthtab[i]))
			return(locv(0, i+1));
	return(0);
}
