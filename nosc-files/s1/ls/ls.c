#include "../h/iobuf.h"

#define GROUPS 1              /* define for std bell groups */
/*
 * list file or directory
 */

#define NUIDB   50      /* 50 uid name slots */

struct	suids	{
	int	f_uid;
	int	f_count;
	char    f_lnam[10];
} uidbuf[NUIDB];

struct	iobuf inf;

struct { char low; char high; };

struct ibuf {
	int	idev;
	int	inum;
	int	iflags;
	char	inl;
	char	iuid;
	char	igid;
	char	isize0;
	int	isize;
	int	iaddr[8];
	long	iatime;
	long	imtime;
};

struct lbuf {
	char	lname[14];
	int	lnum;
	int	lflags;
#ifdef GROUPS
	int     uid:8;
	int     gid:8;
#endif
#ifndef GROUPS
	int     uid;
#endif
	char	lnl;
	char	lsize0;
	int	lsize;
	long	lmtime;
} bentry;

struct lbufx {
	char	*namep;
};

int     aflg, bflg, dflg, fflg, gflg, iflg, lflg;
int     sflg, tflg, uflg, xflg, dotflg;
int	hflgx, hflg 1;
int	bsbflg, bsbsiz, bsbnum, bsbwhf, namsiz, lnlsiz;
int     fout,   dirflg;
int	rflg	1;
int	flags;
int	tblocks, tblox;
int	statreq;
struct	lbuf	*lastp	&end;
struct	lbuf	*rlastp	&end;
char	*dotp	".";
long	nowtime;

#define	IFMT	060000
#define	DIR	0100000
#define	CHR	020000
#define	BLK	040000
#define STICKY  IFMT
#define	ISARG	01000
#define	LARGE	010000
#define	SUID	04000
#define	SGID	02000
#define	ROWN	0400
#define	WOWN	0200
#define	XOWN	0100
#define	RGRP	040
#define	WGRP	020
#define	XGRP	010
#define	ROTH	04
#define	WOTH	02
#define	XOTH	01

main(argc, argv)
char **argv;
{
	int i, j, k, l, m;
	register struct lbuf *ep, *ep1;
	register struct lbuf *slastp;
	struct lbuf lb;
	char *cp, myname;
	int compar();
	extern end;

	time(&nowtime);
	fout = dup(1);
	bsbnum = 4;
	bsbflg++;
	/* get first char of name called by; d--> '-l' implied */
	cp = argv[0];
	myname = *cp;
	while (*cp)
		if (*cp++ == '/') myname = *cp;
	if (myname == 'd') {
		lflg++;	/* called via 'directory' */
		statreq++;
	}
	if (--argc > 0 && *argv[1] == '-') {
		argv++;
		while (*++*argv) switch (**argv) {
		case '.':
			dotflg++;
			continue;

		case 'a':
			aflg++;
			continue;

		case 'b':
			bflg++;
			statreq++;
			continue;

		case 'd':
			dflg++;
			continue;

		case 'f':
			fflg++;
			continue;

		case 'g':
			gflg++;
			continue;

		case 'h':
			hflg = -hflg;
			continue;

		case 'i':
			iflg++;
			continue;

		case 'l':
			lflg++;
			statreq++;
			continue;

		case 'p':
			bsbflg = 0;
			continue;

		case 'r':
			rflg = -1;
			continue;

		case 's':
			sflg++;
			statreq++;
			continue;

		case 't':
			tflg++;
			statreq++;
			continue;

		case 'u':
			uflg++;
			continue;

		case 'x':
			xflg = xflg? -xflg: -1;
			continue;

		default:
			printf("Unknown switch: %c\n", **argv);
			goto leave;
		}
		argc--;
	}
	if (fflg) {
		aflg++;
		lflg = 0;
		sflg = 0;
		tflg = 0;
		statreq = 0;
	}
	if(iflg || sflg || lflg) bsbflg = 0;
	if (argc==0) {
		argc++;
		argv = &dotp - 1;
	}
	namsiz = 4;
	for (i=0; i < argc; i++) {
		if ((ep = gstat(*++argv, 1))==0)
			continue;
		ep->namep = *argv;
		ep->lflags =| ISARG;
		if(length(*argv) > bsbsiz)
			bsbsiz = length(*argv);
	}
	qsort(&end, lastp - &end, 28, compar);
	if(!dirflg && bsbflg) {
		blkdsp(&end, lastp);
		goto leave;
	}
	slastp = lastp;
	for (ep = &end; ep<slastp; ep++) {
		if (ep->lflags&DIR && dflg==0 || fflg) {
			if (argc>1)
				printf("\n%s:\n", ep->namep);
			namsiz = 4;
			hflgx = 0;
			lnlsiz = 0;
			bsbsiz = 0;
			lastp = slastp;
			tblocks = nblock(ep->lsize0, ep->lsize);
			readdir(ep->namep);
			if (fflg==0)
				qsort(slastp, lastp - slastp, 28, compar);
			if (statreq)
				printf("total %d\n", tblocks);
			if(!bsbflg)
				for (ep1=slastp; ep1<lastp; ep1++)
					pentry(ep1);
			else
				blkdsp(slastp, lastp);
		} else {
			if(argc == 1)
				if(hflg == 1)
					hflg = 0;
				else
					hflg = 1;
			pentry(ep);
		}
	}
leave:	flush();
}

pentry(ap)
struct lbuf *ap;
{
	char tbuf[16];
	struct { char dminor, dmajor;};
	register struct lbuf *p;
	register char *cp;
	long longtime;

/*      longtime = 60l*60l*24l*30l*10l;  /* 10 months of seconds */
	longtime = 25920000l;
	p = ap;
	if (p->lnum == -1)
		return;
	if(hflg > 0 && !hflgx)
		header();
	if (iflg)
		printf("%5d ", p->lnum);
	if (lflg) {
		pmode(p->lflags);
		if(lnlsiz)
			printf("%*d", lnlsiz+1, p->lnl&0377);
		if(cp = getl(p->uid))
			printf(" %-*.15s", namsiz, cp);
		else
			printf(" %-*d", namsiz, p->uid);
#ifdef GROUPS
		if(gflg)
			printf(" %3d", p->gid);
#endif
		if (p->lflags&IFMT && (p->lflags&IFMT) != IFMT) {
			printf("%3d,%3d", p->lsize.dmajor&0377,
			    p->lsize.dminor&0377);
			if(sflg)
				printf("  (0)");
		} else {
			printf("%7s", locv(p->lsize0, p->lsize));
			if (sflg)
				printf("%5d", nblock(p->lsize0, p->lsize));
		}
		cp = ctime(&p->lmtime)+4;
		if((nowtime - p->lmtime) > longtime)
			printf(" %-6.6s '%-4.2s ",cp,cp+18);
		else
			printf(" %-12.12s ",cp);
	}
	if (p->lflags&ISARG)
		printf("%s\n", p->namep);
	else
		printf("%.14s\n", p->lname);
}

nblock(size0, size)
char *size0, *size;
{
	register int n;

	n = ldiv(size0, size, 512);
	if (size&0777)
		n++;
	if (n>8)
		n =+ (n+255)/256;
	return(n);
}

int     m0[] { 4, STICKY, 'S', DIR, 'd', BLK, 'b', CHR, 'c', '-'};
int	m1[] { 1, ROWN, 'r', '-' };
int	m2[] { 1, WOWN, 'w', '-' };
int	m3[] { 2, SUID, 's', XOWN, 'x', '-' };
#ifdef GROUPS
int	m4[] { 1, RGRP, 'r', '-' };
int	m5[] { 1, WGRP, 'w', '-' };
int     m6[] { 2, SGID, 's', XGRP, 'x', '-' };
#endif
int	m7[] { 1, ROTH, 'r', '-' };
int	m8[] { 1, WOTH, 'w', '-' };
int	m9[] { 1, XOTH, 'x', '-' };

int	*m[] { m0, m1, m2, m3,
#ifdef GROUPS
		m4, m5, m6,
#endif
		m7, m8, m9};

pmode(aflag)
{
	register int **mp;

	flags = aflag;
	for (mp = &m[0]; mp < &m + 1;)
		select(*mp++);
}

select(pairp)
int *pairp;
{
	register int n, *ap;

	ap = pairp;
	n = *ap++;
	while (--n >= 0 && (flags&*ap) != *ap++)
		ap++;
	putchar(*ap);
}

makename(dir, file)
char *dir, *file;
{
	static char dfile[100];
	register char *dp, *fp;
	register int i;

	dp = dfile;
	fp = dir;
	while (*fp)
		*dp++ = *fp++;
	*dp++ = '/';
	fp = file;
	for (i=0; i<14; i++)
		*dp++ = *fp++;
	*dp = 0;
	return(dfile);
}

readdir(dir)
char *dir;
{
	static struct {
		int	dinode;
		char	dname[14];
	} dentry;
	register char *p;
	register int j;
	register struct lbuf *ep;

	if (fopen(dir, &inf) < 0) {
		printf("%s unreadable\n", dir);
		return;
	}
	for(;;) {
		p = &dentry;
		for (j=0; j<16; j++)
			*p++ = getc(&inf);
		if (dentry.dinode==0
		 || aflg==0 && dentry.dname[0]=='.')
			continue;
		if (dentry.dinode == -1)
			break;
		ep = gstat(makename(dir, dentry.dname), xflg);
		if((xflg < 0 && (ep->lflags&DIR)==0) ||
		   (xflg > 0 && (ep->lflags&DIR)) ||
		   (dotflg && any('.', dentry.dname))) {
			lastp--;
			tblocks =- tblox;
			continue;
		}
		if (ep->lnum != -1)
			ep->lnum = dentry.dinode;
		for (j=0; j<14; j++)
			if(!(ep->lname[j] = dentry.dname[j])) {
				if(bsbsiz < j) bsbsiz = j;
				goto fiddlestix;
			}
			bsbsiz = 14;
fiddlestix: ;
	}
	close(inf.b_fildes);
}

gstat(file, argfl)
char *file;
{
	struct ibuf statb;
	register int i;
	register struct lbuf *rep;

	if (lastp+1 >= rlastp) {
		sbrk(512);
		rlastp.idev =+ 512;
	}
	rep = lastp;
	lastp++;
	rep->lflags = 0;
	rep->lnum = 0;
	if (argfl || statreq) {
		if (stat(file, &statb)<0) {
			printf("%s not found\n", file);
			statb.inum = -1;
			statb.isize0 = 0;
			statb.isize = 0;
			statb.iflags = 0;
			if (argfl) {
				lastp--;
				return(0);
			}
		}
		rep->lnum = statb.inum;
		rep->lflags = statb.iflags& ~(IFMT|DIR|ISARG);
		switch(statb.iflags& (IFMT|ISARG)) {
		case 040000: rep->lflags =| DIR; dirflg++; break;
		case 020000: rep->lflags =| CHR; break;
		case 060000: rep->lflags =| BLK; break;
		case 001000: rep->lflags =| STICKY; break;
		}
#ifndef GROUPS
		rep->uid.high = statb.iuid;
		rep->uid.low = statb.igid;
#endif
#ifdef GROUPS
		rep->uid = statb.iuid;
		rep->gid = statb.igid;
#endif
	/*      getl(rep->uid);         /* get name lengths */
		namsiz = 8;             /* rand hack !!!!!!!*/
		rep->lnl = statb.inl;
		if((i = decsiz(statb.inl&0377)) > lnlsiz && statb.inl != 1)
			lnlsiz = i;
		rep->lsize0 = statb.isize0;
		rep->lsize = statb.isize;
		if (rep->lflags&IFMT && (rep->lflags&IFMT) != IFMT && lflg)
			rep->lsize = statb.iaddr[0];
		rep->lmtime = statb.imtime;
		if(uflg) {
			rep->lmtime = statb.iatime;
		}
		tblox = nblock(statb.isize0, statb.isize);
		tblocks =+ tblox;
	}
	return(rep);
}

compar(ap1, ap2)
struct lbuf *ap1, *ap2;
{
	register struct lbuf *p1, *p2;
	register int i;
	int j, lim;
	struct { char *charp;};

	p1 = ap1;
	p2 = ap2;
	if (dflg==0) {
		if ((p1->lflags&(DIR|ISARG)) == (DIR|ISARG)) {
			if ((p2->lflags&(DIR|ISARG)) != (DIR|ISARG))
				return(1);
		} else {
			if ((p2->lflags&(DIR|ISARG)) == (DIR|ISARG))
				return(-1);
		}
	}
	if (tflg) {
		i = 0;
		if(p2->lmtime > p1->lmtime)
			i++;
		else if(p2->lmtime < p1->lmtime)
			i--;
		return(i*rflg);
	}
	if(bflg)
		return(sizcmp(p1,p2)*rflg);
	if(p1->lflags & ISARG) {
		p1 = p1->namep;  p2 = p2->namep;
		lim = 256;
	} else {
		p1 = p1->lname;  p2 = p2->lname;
		lim = 14;
	}
	for (i=0; i<lim; i++)
		if ((j = *p1.charp++ - *p2.charp++) || p1.charp[-1]==0)
			return(rflg*j);
	return(0);
}


getl(uid)
{
	register struct suids *u, *small;
	register int i;
	char *cp, *np;

	small = &uidbuf[0];
	for(u = &uidbuf; u < &uidbuf[NUIDB]; u++) {
		if(u->f_uid == 0) {
			small = u;
			break;
		}
		if(u->f_uid == uid+1) {
			u->f_count++;
			i = length(u->f_lnam);
			if(i > namsiz)
				namsiz = i;
			return(u->f_lnam);
		}
		if(u->f_count < small->f_count)
			small = u;
	}
	cp = getlogn(uid);
	if(*cp) {
		np = small->f_lnam;
		i = 0;
		while(*np++ = *cp++)
			i++;
		if(i > namsiz)
			namsiz = i;
		small->f_uid = uid+1;
		small->f_count = 1;
		return(small->f_lnam);
	}
	return(0);
}

decsiz(num)
{
	register int i;

	i = 1;
	while(num =/ 10) i++;
	return(i);
}

header()
{
	hflgx++;
	if(iflg) printf("Inode ");
	if(lflg) {
		printf("%-*s", sizeof m/sizeof m[0], "t prot");
		if(lnlsiz) printf("%*s", lnlsiz+1, "ln");
		printf(" %-*s", namsiz+1, "owner");
#ifdef GROUPS
		if(gflg)
			printf("grp ");
#endif
		printf(" bytes ");
		if(sflg)
			printf("blks ");
		if(!uflg)
			printf("  modified   ");
		else
			printf("  accessed   ");
	}
	if(iflg || lflg || sflg)
		printf("name\n");
}

length(str)
{
	register char *cp;
	register int i;

	cp = str;
	i = 0;
	while(*cp++) ++i;
	return(i);
}


blkdsp(base, last)
struct lbuf *base, *last;
{
	register struct lbuf *ep1;
	int i,j,k,l,mmm;

	bsbnum = 79/(bsbsiz+1);
/*      bsbnum = (sgbuf.sg_clim?
		 (sgbuf.sg_clim&0377)-1:71)
		 /(bsbsiz+1);
 */     ep1 = base;
	i = (last-base);
	l = (i + bsbnum -1) / bsbnum;
	for(j = 0; j < l; j++) {
	    for(mmm = 0; mmm < bsbnum; mmm++) {
		k = (mmm * l) + j;
		if(k >= i)
		    continue;
		if((mmm+1)*l+j >= i)
		    printf("%s",gname(&ep1[k]));
		else
		    printf("%-*s ",bsbsiz,gname(&ep1[k]));
	    }
	    printf("\n");
	}
}

gname(lbp)
struct lbuf *lbp;
{
	register struct lbuf *lp;

	lp = lbp;
	if(lp->lflags & ISARG)
		return(lp->namep);
	lp->lname[14] = 0;
	return(&lp->lname[0]);
}


sizcmp(p1,p2)
struct lbuf *p1, *p2;
{
	long l1, l2;
	struct { int long0, long1; };

	l1.long0 = p1->lsize0 & 0377;
	l1.long1 = p1->lsize;
	l2.long0 = p2->lsize0 & 0377;
	l2.long1 = p2->lsize;
	if(l1 > l2)
		return(-1);
	if(l1 < l2)
		return(1);
	return(0);
}
