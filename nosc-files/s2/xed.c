#/*
Module Name:
	xed.c -- Editor
	Copyright 1974, Bell Telephone Laboratories, Incorporated
	Modifications by UCB (the o command)
	Modifications by UCLA (the b,j, & z commands; misc improvements)
	Modifications by U of Illinois misc improvements

Installation:
	if $1x = finalx goto final
	cc xed.c
	mv a.out ../xed
	exit
: final
	cc -O -s -n xed.c
	su cp a.out /usr/bin/xed
	rm a.out ../xed

Synopsis:
	xed [ - ] [ filename ]
*/

#define	SIGHUP	1
#define	SIGINTR	2
#define	SIGQUIT	3
#define	FNSIZE	64
#define	LBSIZE	512
#define	ESIZE	128
#define	GBSIZE	256
#define	NBRA	5
#define	EOF	(-1)
#define CTLA	001
#define NP	014
#define snap(a)	printf("a is %o\n",a);

#define	CBRA	1
#define	CCHR	2
#define	CDOT	4
#define	CCL	6
#define	NCCL	8
#define	CDOL	10
#define	CEOF	11
#define	CKET	12

#define	STAR	01

#define	error	errr()
#define	READ	0
#define	WRITE	1
#define BOOL	char
#define FID	int
#define UNSIGNED	char *

UNSIGNED flushlim 15;	/* number of changed lines before temp updated */
UNSIGNED saveit 15;	/* a very short stack of flushlim values */
UNSIGNED fchanged;
		/* fchanged=0 means internal file = external file */
		/* fchanged=1 means internal file = temp files */
		/* fchanged>1 means internal file differs from temp
		   files by less than fchanged lines */
#define BIGLIM	0177777	/* a very big limit on number of changed lines */
int	peekc;
char	savedfile[FNSIZE];
char	file[FNSIZE];
char	linebuf[LBSIZE];
char	rhsbuf[LBSIZE/2];
char	expbuf[ESIZE+4];
char	genbuf[LBSIZE];
int	*zero;
int	*dot;
int	*dol;
int	*endcore;
int	*fendcore;
int	*addr1;
int	*addr2;
#ifdef LCOUNT
long	lcount;
#endif
#ifndef LCOUNT
int	count[2];
#endif
char	*nextip;
char	*linebp;
int	ninbuf;
FID	io;
struct {
	BOOL	circfl;	/* circumflex flag--circumflex in search string */
	BOOL	pflag;
	BOOL	vflag;
	BOOL	zflag;
	BOOL	hiding;
	char	listf;
}	flags;
struct { int integer[2]; };
FID	tfile -1;
FID	tlpfile;
int	onhup();
/*int	onquit;*/
int	col;
char	*globp;
int	tline;
char	*tfname;
		/* tfname = "etmp.Exxxxx" where xxxxx is process id
		   in decimal, and E is either 'e' for text temporary
		   (maintained by append and delete routines)
		   or 'E' for line index temporary (maintained by
		   flushtmp routine)
		   if for some reason etmp.Exxxxx can't be created on entry,
		   /tmp/Exxxxx is tried.
		*/
char	*loc1;
char	*loc2;
char	*locs;
char	ibuff[512];
int	iblock	-1;
char	obuff[512];
int	oblock	-1;
int	ichanged;
int	nleft;
int	errfunc();
int	names[26];
char	*braslist[NBRA];
char	*braelist[NBRA];

#define RIGHT 1
#define LEFT 0
#define COPY 1
#define NOCOPY 0
#define EOT 004

char *to, *fr;
/*int istty 0;*/
int ttmode[3];
int ttnorm;	/* saves ttmode[2] during 'o' commands */
int zlength 23;

main(argc, argv)
char **argv;
{
	register char *p1, *p2;
	register pid;
	extern int onintr();

/*	onquit = signal(SIGQUIT, 1); */
	signal(SIGQUIT, 1);
	signal(SIGHUP,onhup);
	if(gtty(0,ttmode) != -1) {
/*			istty++;*/
		ttnorm = ttmode[2];
	}
	argv++;
	flags.vflag++;	/* default is verbose mode */
	if (argc > 1 && **argv=='-') {
		/* allow debugging quits? */
		switch ((*argv)[1]) {
		case 'q':
			signal(SIGQUIT, 0);
			break;
		case 0: flags.vflag=0;
			break;
		}
		argv++;
		argc--;
	}
	tfname = "etmp.exxxxx";
	pid = getpid();
	for (p1 = &tfname[11]; p1 > &tfname[6];) {
		*--p1 = (pid%10) + '0';
		pid =/ 10;
	}
	if ((signal(SIGINTR, 1) & 01) == 0)
		signal(SIGINTR, onintr);
	fendcore = sbrk(2) + 2;			/* *(dol+1) = -1 */
						/* see init() */
	init();
	setexit();
	/* All Errors return to this point via reset() */
	if (argc>1) {
		/* read in input; has to be after setexit() */
		argc = 0;
		p1 = *argv;
		p2 = savedfile;
		while(*p2++ = *p1++);
		p1 = *argv;
		p2 = file;
		while (*p2++ = *p1++);
		readfile(zero);
		fchanged = 0;
		}
	commands(0);
	rmfiles();
}

commands(depth)
{
	int getfile(), gettty();
	register *a1, c;
	register char *p;
	int r;
	extern char CRERR[];
	extern char BELL[];
	extern char PROMPT[];

	for (;;) {
	if (flags.pflag) {
		flags.pflag = 0;
		addr1 = addr2 = dot;
		printlines();
	}
	if(flags.vflag && !globp) writef1(PROMPT); /* brute farce */

	addr1 = addr2 = address();
	c = getchar();
	if (addr1) {
		if (c==';') {
			dot = addr1;
			addr2 = address();
			c = getchar();
			}
		else if (c==',') {
			addr2 = address();
			c = getchar();
			}
		}

	switch(c) {

	case 'a':
		setdot();
		newline();
		append(gettty, addr2);
		continue;

	case 'B':
		numout(flushlim);
		saveit = flushlim = addr2-zero;
	case 'b':
		/* update temp */
		numout(flushlim);
		numout(fchanged);
		newline();
		flushtmp();
		continue;

	case 'c':
		setdot();
		newline();
		nonzero();
		delete();
		append(gettty, addr1-1);
		continue;

	case 'd':
		setdot();
		newline();
		nonzero();
		delete();
		continue;

	case 'E':
		fchanged = 0;
	case 'e':
		setnoaddr();
		if ((peekc = getchar()) != ' ')
			error;
		dontgo();
		savedfile[0] = 0;
		filename();
		init();
		readfile(zero);
		fchanged = 0;
		continue;

	case 'f':
		setnoaddr();
		if ((c = getchar()) != '\n') {
			peekc = c;
			savedfile[0] = 0;
			filename();
		}
		puts(savedfile);
		continue;

	case 'G':
		global(2);
		continue;

	case 'g':
		global(1);
		continue;

	case 'i':
		setdot();
		nonzero();
		newline();
		append(gettty, addr2-1);
		continue;

	case 'j':
		setdot();
		nonzero();
		newline();
		join();
		continue;

	case 'k':
		if ((c = getchar()) < 'a' || c > 'z')
			error;
		setdot();
		nonzero();
		newline();
		names[c-'a'] = *addr2 | 01;
		continue;

	case 'm':
		move(0);
		continue;

	case 'n':
		flags.listf = 2;
		goto print;

	case '\n':
		if (addr2==0)
			addr2 = dot+1;
		addr1 = addr2;
		printlines();
		continue;

	case 'l':
		flags.listf = 1;
	print:
	case 'p':
		newline();
		setdot();
		printlines();
		continue;

	case 'o':
		setdot();
		nonzero();
		if ((c=getchar()) != '\n')
		{
			peekc = c;
			ilsubst();
		}
		else {
			ttmode[2] =| 040;
			ttmode[2] =& ~010;
			stty(0,ttmode);
			writef1(BELL);	/* put out a bell */
			ilsubst();
			ttmode[2] = ttnorm;
			stty(0,ttmode);
		}
		continue;

	case 'Q':
		fchanged = 0;
	case 'q':
		setnoaddr();
		if((peekc = getchar()) != '\n')
			error;
	quit:
		dontgo();
		newline();
		rmfiles();
		exit();

	case EOF:		/* I put this here JSK */
		if (depth) return;	/* for global changes */
		setnoaddr();
		dontgo();
		rmfiles();
		exit();

	case 'r':
	caseread:
		filename();
		setall();
		readfile(addr2);
		continue;

	case 's':
		setdot();
		nonzero();
		substitute(globp);
		continue;

	case 't':
		move(1);
		continue;

	case 'v':
		global(0);
		continue;

	case 'W':
	case 'w':
		setall();
		nonzero();
		p = getchar();
		if (p != '-')
		{
			peekc = p;
			p = 0;
		}
		filename();
		io = p ? open(file,1): -1;
		if (io<0)
			io = creat(file,0644);
		if (io < 0) errfunc(CRERR);
		seek(io,0,2);	/* go to end of file */
		putfile();
		exfile();
		fchanged = 0;
		if (c == 'W')
			goto quit;
		continue;

	case 'z':
		getzlen();
		flags.zflag++;
		printlines();
		flags.zflag = flags.pflag = 0;
		continue;

	case '=':
		setall();
		newline();
/*
#ifndef LCOUNT
		count[1] = (addr2-zero)&077777;
#endif
#ifdef LCOUNT
		lcount = (addr2-zero)&077777;
#endif
		putd();
		ptchar('\n');
*/
		numout((addr2-zero)&077777);
		continue;

	case '!':
		unix();
		continue;

	}
	error;
	}
}

address()
{
	register *a1, minus, c;
	int n, relerr;
	extern char PROMPT[];

	minus = 0;
	a1 = 0;
	for (;;) {
		c = getchar();
		if ('0'<=c && c<='9') {
			n = 0;
			do {
				n =* 10;
				n =+ c - '0';
			} while ((c = getchar())>='0' && c<='9');
			peekc = c;
			if (a1==0)
				a1 = zero;
			if (minus<0)
				n = -n;
			a1 =+ n;
			minus = 0;
			continue;
		}
		relerr = 0;
		if (a1 || minus)
			relerr++;
		switch(c) {
		case ' ':
		case '\t':
			continue;

		case '+':
			minus++;
			if (a1==0)
				a1 = dot;
			continue;

		case '-':
		case '^':
			minus--;
			if (a1==0)
				a1 = dot;
			continue;

		case '?':
		case '/':
			compile(c);
			a1 = dot;
			for (;;) {
				if (c=='/') {
					a1++;
					if (a1 > dol)
						a1 = zero;
				} else {
					a1--;
					if (a1 < zero)
						a1 = dol;
				}
				if (execute(0, a1))
					break;
				if (a1==dot)
					error;
			}
			break;

		case '$':
			a1 = dol;
			break;

		case '.':
			a1 = dot;
			break;

		case '\'':
			if ((c = getchar()) < 'a' || c > 'z')
				error;
			for (a1=zero; a1<=dol; a1++)
				if (names[c-'a'] == (*a1|01))
					break;
			break;

		default:
			peekc = c;
			if (a1==0)
				return(0);
			a1 =+ minus;
			if (a1<zero) a1=zero;
			else if (a1>dol) a1=dol;
			return(a1);
		}
		if (relerr)
			error;
	}
}

setdot()
{
	if (addr2 == 0) {
		if (dot>dol) error;
		addr1 = addr2 = dot;
		}
	if (addr1 > addr2)
		error;
}

setall()
{
	if (addr2==0) {
		addr1 = zero+1;
		addr2 = dol;
		if (dol==zero)
			addr1 = zero;
	}
	setdot();
}

setnoaddr()
{
	if (addr2)
		error;
}

nonzero()
{
	if (addr1 > dol) error;
	if (addr1 <= zero) addr1 = zero+1;
	if (addr2 > dol) addr2 = dol;
	if (addr1>addr2) error;
}

newline()
{
	register c;

	if ((c = getchar()) == '\n')
		return;
	flags.pflag++;
	if (c=='l')
		flags.listf = 1;
	else if (c=='p');
	else if (c=='n')
		flags.listf = 2;
	else error;
	if (getchar() != '\n')
		error;
	return;
}

filename()
{
	register char *p1, *p2;
	register c;

#ifndef LCOUNT
	count[1] = 0;
#endif
#ifdef LCOUNT
	lcount.integer[1] = 0;
#endif
	c = getchar();
	if (c=='\n' || c==EOF) {
		p1 = savedfile;
		if (*p1==0)
			error;
		p2 = file;
		while (*p2++ = *p1++);
		return;
	}
	if (c!=' ')
		error;
	while ((c = getchar()) == ' ');
	if (c=='\n')
		error;
	p1 = file;
	do {
		*p1++ = c;
	} while ((c = getchar()) != '\n');
	*p1++ = 0;
	if (savedfile[0]==0) {
		p1 = savedfile;
		p2 = file;
		while (*p1++ = *p2++);
	}
}

exfile()
{
	close(io);
	io = -1;
	if (flags.vflag) {
		putd();
		ptchar('\n');
	}
}

getzlen()
{
	register int c;
	register int zmode;
	register int i;
	for(;;)
	switch(zmode=getchar())
	{
	case '\t':
	case ' ':
		continue;
	case '.':
	case '-':
	case '+':
	case ',':
		c = getchar();
		goto out;
	default:
		c = zmode;
		zmode = '+';
		goto out;
	}
out:
	c =- '0';
	if (c >= 0 && c <= 9) {
		i = c;
		while ((c=getchar()-'0') >= 0 && c <= 9)
			i = i*10 + c;
	}
	else i = zlength;
	peekc = c+'0';
	newline();
	if(i!=0)
		zlength = i;
	if (addr2 == 0) addr1 = dot;
	switch (zmode) {
		case '.': addr1 =- zlength/2;
			break;
		case '-': addr1 =- zlength-1;
			break;
		case ',': addr1 =- 2*(zlength-1);
	}
	addr2 = addr1 + zlength-1;
}

onintr()
{
	signal(SIGINTR, onintr);
	ptchar('\n');
	error;
}

onhup()
{
	extern char WREDHUP[];

	globp = WREDHUP;
	commands(1);
	rmfiles();
	exit(-1);
}

dontgo()
{
	register char c;
	extern char DONTGO[];

	if(!fchanged)
	{
		/*UPDATE*/
		 return;
	}
	fchanged = 0;
	errfunc(DONTGO);
}

errfunc(s)
char *s;
{

	flags.listf = 0;
	puts(s);
#ifndef LCOUNT
	count[0] = 0;
#endif LCOUNT
#ifdef LCOUNT
	lcount = 0;
#endif
	seek(0, 0, 2);
	flags.pflag = 0;
	globp = 0;
	if (io > 0) {
		close(io);
		io = -1;
	}
	peekc = 0;
	ttmode[2] = ttnorm;
	stty(0,ttmode);		/* reset tty if necessary */
				/* and flush all pending input */

	flushlim = saveit;
	reset();
				/* returns to top command level */
}

getchar()
{
	register int c;
	char cc;

	if (c=peekc) {
		peekc = 0;
		return c;
	}
	if (globp) {
		if ((c = *globp++) != 0)
			return c;
		globp = 0;
		return EOF;
	}
	if (read(0,&cc,1) <= 0)
		return EOF;
	return cc;
}

gettty()
{
	register c, gf;
	register char *p;

	p = linebuf;
	gf = globp;
	while ((c = getchar()) != '\n') {
		if (c==EOF) {
			if (gf)
				peekc = c;
			return(c);
		}
		if (c == '\0')
			continue;
		*p++ = c;
		if (p >= &linebuf[LBSIZE-2])
			error;
	}
	*p++ = 0;
	if (linebuf[0]=='.' && linebuf[1]==0)
		return(EOF);
	return(0);
}

getfile()
{
	register c;
	register char *lp, *fp;
	extern char BUFERR[];

	lp = linebuf;
	fp = nextip;
	do {
		if (--ninbuf < 0) {
			if ((ninbuf = read(io, genbuf, LBSIZE)-1) < 0)
				return(EOF);
			fp = genbuf;
		}
		if (lp >= &linebuf[LBSIZE])
			errfunc(BUFERR);
		if ((*lp++ = c = *fp++ & 0177) == 0) {
			lp--;
			continue;
		}
#ifndef LCOUNT
		if (++count[1] == 0)
			++count[0];
#endif
#ifdef LCOUNT
		lcount++;
#endif
	} while (c != '\n');
	*--lp = 0;
	nextip = fp;
	return(0);
}

putfile()
{
	register char *fp, *lp;
	register int i;
	int *a1;
	extern char IOERR[];

	i = 0;
	fp = genbuf;
	a1 = addr1;
	lp = getline(*a1);
	for (;;) {
		/* i = fp-genbuf;
		   fp points to next place to put char to output
		   genbuf[0..i-1] contains chars created to output
		   a1 = line number of current line being moved
		   lp points to next character to move
		   count[0,1] contains total # of bytes moved
		*/
#ifndef LCOUNT
		if (++count[1] == 0)
			++count[0];
#endif
#ifdef LCOUNT
		lcount++;
#endif
		i++;
		if ((*fp++ = *lp++) == 0) {
			fp[-1] = '\n';
			if (a1++ >= addr2) break;
			lp = getline(*a1);
		}
		if ( i == 512) {
			if (write(io,genbuf,i) != i) errfunc(IOERR);
			fp = genbuf;
			i = 0;
		}
	}
	if (write(io,genbuf,i) != i) errfunc(IOERR);
}

append(f, a)
int (*f)();
{
	register *a1, *a2, *rdot;
	int nline, tl;
	extern char NMCERR[];

	nline = 0;
	dot = a;
	while ((*f)() == 0) {
		tl = putline();
		nline++;
		a1 = ++dol;
		rdot = endcore;
		if (dol >= rdot) {
			rdot = (rdot|07777) + 1;	/* 2k bytes */
			if(brk(rdot) < 0)
				errfunc(NMCERR);
			endcore = rdot;
		}
		a2 = a1+1;
		rdot = ++dot;
		while (a1 > rdot)
			*--a2 = *--a1;
		*rdot = tl;
	}
	rdot = (dol|037) + 1;				/* tighten up data */
	if(rdot!=endcore)	brk(endcore);		/* space to nearest */
							/* 64 bytes	    */
							/* possibly put in */
							/* flushtmp */
	return(nline);
}

unix()
{
	register pid, rpid;
	register char	*p;
	int retcode;
	extern char BANG[];
	extern char BUFERR[];
	extern char MC[];
	extern char SH[];
	extern char MSH[];

	setnoaddr();
	p = linebuf;
	while ( (*p = getchar()) != '\n' && *p != EOF )
		if (p++ >= linebuf + LBSIZE) errfunc(BUFERR);
	*p = 0;
	if ((pid = fork()) == 0) {
/*		unneeded:
		signal(SIGHUP, onhup);
		signal(SIGQUIT, onquit);
*/
		execl(SH, MSH, *linebuf?MC:0, linebuf, 0);
		error;
	}
	p = signal(SIGINTR, 1);
	while ((rpid = wait(&retcode)) != pid && rpid != -1);
	signal(SIGINTR, p);
	puts(BANG);
}

delete()
{
	register *a1, *a2;
	register int a3;
	extern int *dol;
	extern int *dot;
	extern int *addr1;
	extern int *addr2;
	extern UNSIGNED flushlim;
	extern UNSIGNED fchanged;
	struct { int* ptr; };

	a1 = addr1;
	a2 = addr2+1;
	a3 = dol-a2+2;

	while(--a3)
		*a1++ = *a2++;

	fchanged =+ a2 - a1;
	a3 = (dol =- a2 - a1);
	a1 = addr1;
	if (a1 > a3.ptr)	
		a1 = a3;
	dot = a1;
	if (fchanged>flushlim)
		flushtmp();
}

join()
{
	register char *a1,*a2;

	a1 = genbuf;
	addr1 = addr2-1;
	if (addr1 <= zero) error;
	a2 = getline(*addr2);
	while (*a1++ = *a2++);
	a1 = getline(*addr1);
	while (*a1++);
	--a1;
	a2 = genbuf;
	while (*a1++ = *a2++);
	*addr2 = putline();
	addr2 = addr1;
	delete();
}

getline(tl)
{
	register char *bp, *lp;
	register nl;

	lp = linebuf;
	bp = getblock(tl, READ);
	nl = nleft;
	tl =& ~0377;
	while (*lp++ = *bp++)
		if (--nl == 0) {
			bp = getblock(tl=+0400, READ);
			nl = nleft;
	}
	return(linebuf);
}

putline()
{
	register char *bp, *lp;
	register nl;
	int tl;

	lp = linebuf;
	tl = tline;
	bp = getblock(tl, WRITE);
	nl = nleft;
	tl =& ~0377;
	while (*bp = *lp++) {
		if (*bp++ == '\n') {
			*--bp = 0;
			linebp = lp;
			break;
		}
		if (--nl == 0) {
			bp = getblock(tl=+0400, WRITE);
			nl = nleft;
		}
	}
	nl = tline;
	tline =+ (((lp-linebuf)+03)>>1)&077776;
	fchanged++;
	if (fchanged>flushlim) flushtmp();
	return(nl);
}

getblock(atl, iof)
{
	register bno, off;
	extern char TMPERR[];
	extern read(), write();

	bno = (atl>>8)&0377;
	off = (atl<<1)&0774;
	if (bno >= 255) errfunc(TMPERR);
	nleft = 512 - off;
	if (bno==iblock) {
		ichanged =| iof;
		return(ibuff+off);
	}
	if (bno==oblock)
		return(obuff+off);
	if (iof==READ) {
		if (ichanged)
			blkio(iblock, ibuff, write);
		ichanged = 0;
		iblock = bno;
		blkio(bno, ibuff, read);
		return(ibuff+off);
	}
	if (oblock>=0)
		blkio(oblock, obuff, write);
	oblock = bno;
	return(obuff+off);
}

flushtmp()
{
	/* this causes temporaries to be updated to most
	   current state.  Then reset fchanged to 1 if
	   fchanged was > 0.
	*/
	register int *a;
	register int i;
	extern read(), write();
	extern char NMCERR[];
	extern char IOERR[];
#ifdef	TELLME
/*%%*/	puts("flushtmp");
#endif
	if (ichanged) blkio(iblock,ibuff,write);
	ichanged = 0;
	blkio(oblock,obuff,write);
	if (tlpfile)
		seek(tlpfile,0,0);	/* go back to start */
	else {
		tfname[5] = 'E';
		tlpfile = creat(tfname, 0600);
		tfname[5] = 'e';
	}
	a = endcore;
	if (dol+1 >= a) {
		a = (a|037) + 1;	/* 64 bytes */
		if(dol<a || brk(a) < 0)
			errfunc(NMCERR);
		endcore = a;
	}
	*(dol+1) = -1;	/* mark eof */
	for (a = zero+1; a < (dol-255); a =+ 256)
		if ( write(tlpfile,a,512) != 512) errfunc(IOERR);
	i = dol - a + 2;	/* length of last write */
	i =* 2;
	if ( write(tlpfile,a,i) != i ) errfunc(IOERR);
	if (fchanged) fchanged=1;
	/* don't set to 0 because 'w' command wasn't issued */
}

blkio(b, buf, iofcn)
int (*iofcn)();
{
	extern char TMPERR[];
	if( seek(tfile, b, 3)<0 || (*iofcn)(tfile, buf, 512) != 512)
/*%%*/		errfunc("IO to tmp");
}

init()
{
	close(tfile);
	rmfiles();
	fchanged = ichanged = 0;
	tline = 0;
	iblock = -1;
	oblock = -1;
	tfname[0] = 'e';
	tfname[4] = '.';
	tfile = creat(tfname,0600);
	if (tfile < 0) {
		/* couldn't make temp in local area, try /tmp */
		tfname[0] = '/';
		tfname[4] = '/';
		tfile = creat(tfname,0600);
		}
	/* must close and open it to get the right access! */
	close(tfile);
	open(tfname, 2);
	brk(fendcore);
	dot = zero = dol = fendcore;
	endcore = fendcore - 4;
}

rmfiles()
{
	tfname[5] = 'E';
	unlink(tfname);
	tfname[5] = 'e';
	unlink(tfname);
	if (tlpfile) close (tlpfile);
	tlpfile = 0;
}

global(k)
{
	register c;
	register int *a1;
	register char *gp;
	char	globuf[GBSIZE];
	extern char BUFERR[];

	if (globp)
		error;
	setall();
	nonzero();
	if ((c=getchar())=='\n')
		error;
	compile(c);
	gp = globuf;
	while ((c = getchar()) != '\n') {
		if (c==EOF)
			error;
		if (c=='\\') {
			c = getchar();
			if (c!='\n')
				*gp++ = '\\';
		}
		*gp++ = c;
		if (gp >= &globuf[GBSIZE-2])
			errfunc(BUFERR);
	}
	*gp++ = '\n';
	*gp++ = 0;
	for (a1=zero; a1<=dol; a1++) {
		*a1 =& ~01;
		if (a1>=addr1 && a1<=addr2 && execute(0, a1)==k)
			*a1 =| 01;
	}
	/* fewer flushtmps during global change */
	saveit = flushlim;
	flushlim = BIGLIM;
	for (a1=zero; a1<=dol; a1++) {
		if (*a1 & 01) {
			*a1 =& ~01;
			dot = a1;
			globp = globuf;
			commands(1);
			a1 = zero;
		}
	}
	flushlim = saveit;
	if(fchanged>flushlim)
		flushtmp ();
}


substitute(inglob)
{
	register gsubf, *a1, nl;
	int getsub();

	gsubf = compsub();
	for (a1 = addr1; a1 <= addr2; a1++) {
		if (execute(0, a1)==0)
			continue;
		inglob =| 01;
		dosub();
		if (gsubf) {
			while (*loc2) {
				if (execute(1)==0)
					break;
				dosub();
			}
		}
		*a1 = putline();
		nl = append(getsub, a1);
		a1 =+ nl;
		addr2 =+ nl;
	}
	if (inglob==0)
		error;
}

compsub()
{
	register seof, c;
	register char *p;
	int gsubf;
	extern char BUFERR[];

	if ((seof = getchar()) == '\n')
		return(0);
	if (seof >= 'a' && seof <= 'z') {
		if (seof != 'g') peekc=seof;
		newline();
		if (seof == 'g') return(1);	/* signal global parm */
		else return(0);
		}
	compile(seof);
	p = rhsbuf;
	for (;;) {
		c = getchar();
		if (c=='\\')
			c = getchar() | 0200;
		if (c=='\n')
			error;
		if (c==seof)
			break;
		*p++ = c;
		if (p >= &rhsbuf[LBSIZE/2])
			errfunc(BUFERR);
	}
	*p++ = 0;
	if ((peekc = getchar()) == 'g') {
		peekc = 0;
		newline();
		return(1);
	}
	newline();
	return(0);
}

getsub()
{
	register char *p1, *p2;

	p1 = linebuf;
	if ((p2 = linebp) == 0)
		return(EOF);
	while (*p1++ = *p2++);
	linebp = 0;
	return(0);
}

dosub()
{
	register char *lp, *sp, *rp;
	int c;
	extern char BUFERR[];

	lp = linebuf;
	sp = genbuf;
	rp = rhsbuf;
	while (lp < loc1)
		*sp++ = *lp++;
	while (c = *rp++) {
		if (c=='&') {
			sp = place(sp, loc1, loc2);
			continue;
		} else if (c<0 && (c =& 0177) >='1' && c < NBRA+'1') {
			sp = place(sp, braslist[c-'1'], braelist[c-'1']);
			continue;
		}
		*sp++ = c&0177;
		if (sp >= &genbuf[LBSIZE])
			errfunc(BUFERR);
	}
	lp = loc2;
	loc2 = sp + linebuf - genbuf;
	while (*sp++ = *lp++)
		if (sp >= &genbuf[LBSIZE])
			errfunc(BUFERR);
	lp = linebuf;
	sp = genbuf;
	while (*lp++ = *sp++);
}

place(asp, al1, al2)
{
	register char *sp, *l1, *l2;
	extern char BUFERR[];

	sp = asp;
	l1 = al1;
	l2 = al2;
	while (l1 < l2) {
		*sp++ = *l1++;
		if (sp >= &genbuf[LBSIZE])
			errfunc(BUFERR);
	}
	return(sp);
}

move(copy)
int copy;
{
	register int *adt, *ad1, *ad2;
	int getcopy();

	setdot();
	nonzero();
	if ((adt = address())==0)
		error;
	newline();
	saveit = flushlim;
	flushlim = BIGLIM;
	if (copy) {
		ad1 = dol+1;
		append(getcopy,dol);
		/* getcopy delivers addr1 thru addr2; append changes dol */
		ad2 = dol+1;
	}
	else {
		ad1 = addr1;
		ad2 = addr2+1;
	}
	if (adt<ad1) {
		dot = adt + (ad2-ad1);
		if ((++adt)==ad1)
			return;
		reverse(adt, ad1);
		reverse(ad1, ad2);
		reverse(adt, ad2);
	} else if (adt >= ad2) {
		dot = adt++;
		reverse(ad1, ad2);
		reverse(ad2, adt);
		reverse(ad1, adt);
	} else
		error;
	fchanged =+ ad1-ad2;
	if(!globp)
		flushtmp();
	flushlim = saveit;
}

reverse(aa1, aa2)
{
	register int *a1, *a2, t;

	a1 = aa1;
	a2 = aa2;
	for (;;) {
		t = *--a2;
		if (a2 <= a1)
			return;
		*a2 = *a1;
		*a1++ = t;
	}
}

getcopy()
{
	if (addr1 > addr2)
		return(EOF);
	getline(*addr1++);
	return(0);
}

compile(aeof)
{
	register eof, c;
	register char *ep;
	char *lastep;
	char bracket[NBRA], *bracketp;
	int nbra;
	int cclcnt;

	ep = expbuf;
	eof = aeof;
	bracketp = bracket;
	nbra = 0;
	if ((c = getchar()) == eof) {
		if (*ep==0)
			error;
		return;
	}
	else if (c == '\n') {
		peekc = c;
		return;
		}
	flags.circfl = 0;
	if (c=='^') {
		c = getchar();
		flags.circfl++;
	}
	if (c=='*')
		goto cerror;
	peekc = c;
	for (;;) {
		if (ep >= &expbuf[ESIZE])
			goto cerror;
		c = getchar();
		if (c==eof) {
			*ep++ = CEOF;
			return;
		}
		if (c!='*')
			lastep = ep;
		switch (c) {

		case '\\':
			if ((c = getchar())=='(') {
				if (nbra >= NBRA)
					goto cerror;
				*bracketp++ = nbra;
				*ep++ = CBRA;
				*ep++ = nbra++;
				continue;
			}
			if (c == ')') {
				if (bracketp <= bracket)
					goto cerror;
				*ep++ = CKET;
				*ep++ = *--bracketp;
				continue;
			}
			if (c=='\n')
				cerror;
			goto defchar;

		case '.':
			*ep++ = CDOT;
			continue;

		case '\n':
			*ep++ = CEOF;
			peekc = c;
			return;

		case '*':
			if (*lastep==CBRA || *lastep==CKET)
				error;
			*lastep =| STAR;
			continue;

		case '$':
			if ((peekc=getchar()) != eof)
				goto defchar;
			*ep++ = CDOL;
			continue;

		case '[':
			*ep++ = CCL;
			*ep++ = 0;
			cclcnt = 1;
			if ((c=getchar()) == '^') {
				c = getchar();
				ep[-2] = NCCL;
			}
			do {
				if (c=='\n')
					goto cerror;
				*ep++ = c;
				cclcnt++;
				if (ep >= &expbuf[ESIZE])
					goto cerror;
			} while ((c = getchar()) != ']');
			lastep[1] = cclcnt;
			continue;

		defchar:
		default:
			*ep++ = CCHR;
			*ep++ = c;
		}
	}
   cerror:
	expbuf[0] = 0;
	error;
}

execute(gf, addr)
int *addr;
{
	register char *p1, *p2, c;

	if (gf) {
		if (flags.circfl)
			return(0);
		p1 = linebuf;
		p2 = genbuf;
		while (*p1++ = *p2++);
		locs = p1 = loc2;
	} else {
		if (addr==zero)
			return(0);
		p1 = getline(*addr);
		locs = 0;
	}
	p2 = expbuf;
	if (flags.circfl) {
		loc1 = p1;
		return(advance(p1, p2));
	}
	/* fast check for first character */
	if (*p2==CCHR) {
		c = p2[1];
		do {
			if (*p1!=c)
				continue;
			if (advance(p1, p2)) {
				loc1 = p1;
				return(1);
			}
		} while (*p1++);
		return(0);
	}
	/* regular algorithm */
	do {
		if (advance(p1, p2)) {
			loc1 = p1;
			return(1);
		}
	} while (*p1++);
	return(0);
}

advance(alp, aep)
{
	register char *lp, *ep, *curlp;
	char *nextep;

	lp = alp;
	ep = aep;
	for (;;) switch (*ep++) {

	case CCHR:
		if (*ep++ == *lp++)
			continue;
		return(0);

	case CDOT:
		if (*lp++)
			continue;
		return(0);

	case CDOL:
		if (*lp==0)
			continue;
		return(0);

	case CEOF:
		loc2 = lp;
		return(1);

	case CCL:
		if (cclass(ep, *lp++, 1)) {
			ep =+ *ep;
			continue;
		}
		return(0);

	case NCCL:
		if (cclass(ep, *lp++, 0)) {
			ep =+ *ep;
			continue;
		}
		return(0);

	case CBRA:
		braslist[*ep++] = lp;
		continue;

	case CKET:
		braelist[*ep++] = lp;
		continue;

	case CDOT|STAR:
		curlp = lp;
		while (*lp++);
		goto star;

	case CCHR|STAR:
		curlp = lp;
		while (*lp++ == *ep);
		ep++;
		goto star;

	case CCL|STAR:
	case NCCL|STAR:
		curlp = lp;
		while (cclass(ep, *lp++, ep[-1]==(CCL|STAR)));
		ep =+ *ep;
		goto star;

	star:
		do {
			lp--;
			if (lp==locs)
				break;
			if (advance(lp, ep))
				return(1);
		} while (lp > curlp);
		return(0);

	default:
		error;
	}
}

cclass(aset, ac, af)
{
	register char *set, c;
	register n;

	set = aset;
	if ((c = ac) == 0)
		return(0);
	n = *set++;
	while (--n)
		if (*set++ == c)
			return(af);
	return(!af);
}

putd()
{
	register r;
	extern ldivr;

#ifndef LCOUNT
	count[1] = ldiv(count[0], count[1], 10);
	count[0] = 0;
	r = ldivr;
	if (count[1])
		putd();
#endif
#ifdef LCOUNT
	lcount.integer[1] = ldiv(lcount.integer[0],lcount.integer[1], 10);
	lcount.integer[0] = 0;
	r = ldivr;
	if (lcount.integer[1])
		putd();
#endif
	ptchar(r + '0');
}

puts(as)
{
	register char *sp;

	sp = as;
	col = 0;
	while (*sp)
		ptchar(*sp++);
	ptchar('\n');
}


char line[70];
char	*linp	line;

ptchar(ac)
{
	register char *lp;
	register c;
	register char i;
	static char linx;

	lp = linp;
	c = ac;
	if (flags.listf == 1) {
		col++;
		if (col >= 72) {
			col = 0;
			*lp++ = '\\';
			*lp++ = '\n';
		}
		if (c=='\t') {
			c = '>';
			goto esc;
		}
		if (c=='\b') {
			c = '<';
		esc:
			*lp++ = '\\';
			col++;
			goto out1;
		}
		if (c == '\\') {
			goto esc;
		}
		if (c<' ' && c!= '\n') {
			*lp++ = '\\';
			/**lp++ = ((c>>6)&3)+'0';*//* would be nice ... */
			*lp++ = (c>>3)+'0';
			*lp++ = (c&7)+'0';
			col =+ 2;
			goto out;
		}
	}
out1:
	*lp++ = c;
out:
	if(c == '\n' || lp >= &line[64]) {
		linp = line;
		write(1, line, lp-line);
		return;
	}
	linp = lp;
}
/*
 * Get process ID routine if system call is unavailable.
getpid()
{
	register f;
	int b[1];

	f = open("/dev/kmem", 0);
	if(f < 0)
		return(-1);
	seek(f, 0140074, 0);
	read(f, b, 2);
	seek(f, b[0]+8, 0);
	read(f, b, 2);
	close(f);
	return(b[0]);
}
 */
ilsubst() {
	register int *a1, nl;
	extern int getsub();
	for(a1=addr1; a1<=addr2; a1++) {
		getline(*a1);
		if(ildo())
		{
			*a1=putline();
			nl=append(getsub,a1);
			a1=+ nl;
			addr2=+ nl;
		}
	}
}


ildo()
{
	register char *t1;
	register int n;
	register int c;
	extern char BELL[];
restart:
	for(t1 = &linebuf[0]; *t1++;);
	t1++;		/* copy '\0' also */
	fr = &genbuf[LBSIZE]; for(n = t1-&linebuf[0]; n--;) *--fr = *--t1;
	to = &genbuf[0];
	for(;;) {
		t1=0; c=ilgetchar();
		if(c=='-')
		{
			t1++;			/* indicate minus */
			c = ilgetchar();
		}
		if(c>='0' && c<='9') {
			n=0;
			do {
				n=* 10;
				n=+ c-'0';
			} while((c=ilgetchar())>='0' && c<='9');
		}
		else if (c=='$') {
			n = &genbuf[LBSIZE]-fr;
			c = ilgetchar();
		}
		else
			n = 1;
		if(t1) n= -n;
		switch(c) {
		case '\b':
			n= -n;
		case ' ':
			if(n<0) while(n++ && ilmove(LEFT,COPY));
			else
			{
				c = n;
				t1 = fr;
				while(n-- && ilmoveR(COPY));
				write(1,t1,fr-t1);
			}
			break;
		case CTLA:
			n= -n;
		case 'd':
			if(n<0) while(n++ && ilmove(LEFT,NOCOPY));
			else
			{
				c = n;
				t1 = fr;
				while(n-- && ilmoveR(NOCOPY));
				write(1,t1,fr-t1);
			}
			break;
		case 'c':
			while(n) {
				c=ilgetchar();
				if(! *fr) break;
				if(c==EOT||c=='') break;/*cntd or esc*/
				if(n>0)
				{
					*fr = c;
					n--;
				}
				else
				{
					*--fr = c;
					n++;
				}
				ilmove(n>=0?RIGHT:LEFT,COPY);
			}
			break;
		case 'r':
			if(n<0) while(n++ && ilmove(LEFT,NOCOPY));
			else
			{
				c = n;
				t1 = fr;
				while(n-- && ilmoveR(NOCOPY));
				write(1,t1,fr-t1);
			}
		case 'i':
			/* while !cntd and !esc */
			while((c=ilgetchar())!=EOT&&c!=''&&c!=EOF) {
				if(to == fr) break;
				if (c == '\b') ilmove(LEFT,NOCOPY);
				else {
					*--fr = c;
					ilmove(RIGHT,COPY);
				}
			}
			break;
		case '@':
			writenl();
			goto restart;
		case 'e':
			while(ilmoveR(COPY));
		case 'f':
			show();
			if((ttmode[2]&050)!=040)	/* if !raw */
				ilgetchar();
			goto finish;
		case '\t':
			show();
			if(n>0)
			{
				writef1(fr);
				while(ilmoveR(COPY));
			}
			else
				while(ilmove(LEFT,COPY));
			break;
		case '\n':
			show();
			writef1(fr);
			while(ilmoveR(COPY));
		finish:
			*to = 0;
			to= &linebuf[0];
			fr= &genbuf[0];
			for(n=0;;)
			{
				if(*fr!=*to)
				{
					*to = *fr;
					n = 1;
				}
				if(*fr=='\0')
				{
					*to = *fr;	/* always */
					break;
				}
				to++; fr++;
			}
			writenl();
			flags.hiding=0;
			return n; /* indicates if line has been modified */
		case 'p':
			show();
			writef1(fr);
			writenl();
			t1 = &genbuf[0];
			if(to>t1)
				write(1,t1,to-t1);
			break;
		case 'l':
			show();
			writef1(fr);
			writenl();
			n = to - &genbuf[0];
			while(n--) *--fr = *--to;
			break;
		case 's':
			show();
			c=ilgetchar();
			if(n<0) while(n++) {
				do
					ilmove(LEFT,COPY);
				while(to != &genbuf[0] && *(to-1) != c);
			} else for(t1 = fr; n--&&*t1!='\0';) {
				for(;;)
				{
					if(*++t1 == '\0' || *t1 == c)
					{
						write(1,fr,t1-fr);
						break;
					}
				}
				while(ilmoveR(COPY) && *fr != c);
			}
			break;
		case 'k':
			c=ilgetchar();
			if(n<0) {
				t1=to-1;
				while(n++) {
					while(t1 != &genbuf[0])
					if(*--t1==c) {
						t1=to-t1;
						while(t1--) ilmove(LEFT,NOCOPY);
						break;
					}
				}
			} else {
				t1 = fr+1;
				while(n--) {
					while(*t1)
					if(*t1==c) {
						t1=- fr;
						hide();
						write(1,fr,t1);
						while(t1--) ilmoveR(NOCOPY);
						break;
					}
					else
						t1++;
				}
			}
			break;
		default: writef1(BELL);
		}
	}
}

char
ilmove(dir,cp) int dir,cp; {
	register char c;

	if(dir==LEFT) {
		if(to == &genbuf[0]) return(0);
		hide();
		c = *--to;
		write(1,to,1);
		if(cp==COPY) *--fr = c;
		return(c);
	} else {
		if(! *fr) return(0);
		if(cp==COPY) show(); else hide();
		write(1,fr,1);
		c = *fr++;
		if(cp==COPY) *to++ = c;
		return(c);
	}
}

char
ilmoveR(cp) int cp; {
	register char c;
	if(! *fr) return(0);
	if(cp==COPY) show(); else hide();
	c = *fr++;
	if(cp==COPY) *to++ = c;
	return(c);
}

ilgetchar() {
	register char c,d;

	c = getchar();
	if(c == '\\') {
		d = getchar();
		if(d=='@' || d=='\b' || d==CTLA || d=='\\')return(d);
		if(ttmode[2] &	04) {
			/* half-ascii terminal escapes */
			if(d >= 'a' && d <= 'z') return(d-'a'+'A');
			switch(d) {
			case '!':	return('|');
			case '\'':	return('`');
			case '^':	return('~');
			case '(':	return('{');
			case ')':	return('}');
			}
		}
		peekc = d;
		/* ASSERT: c == backslash */
	}
	return c;
}

show() {
	extern char BACKSL[];
	if(flags.hiding) {
		writef1(BACKSL);
		flags.hiding=0;
	}
}

hide() {
	extern char BACKSL[];
	if(!flags.hiding) {
		writef1(BACKSL);
		flags.hiding=1;
	}
}

printlines()
{
	register char *p,*s;
	register int *a1;
	nonzero();
	for (a1 = addr1; a1 <= addr2; a1++) {
		if (flags.listf == 2) {
#ifndef LCOUNT
			count[1] = (a1-zero) & 077777;
#endif
#ifdef LCOUNT
			lcount.integer[1] = (a1-zero) & 077777;
#endif
			putd();
			ptchar('\t');
		}
#ifdef DONTLIKEIT
		p = getline(*a1);
		if (flags.zflag) for (s=p; *s; s++) if (*s==NP) {
			*s = 0;
			puts(p);
			dot = a1+1;
			goto exit;
		}
		puts(p);
#endif DONTLIKEIT
#ifndef DONTLIKEIT
		puts(getline(*a1));
#endif
	}
	dot = addr2;
	exit:
	flags.listf = 0;
}

readfile(addr)
{
	extern char OPERR[];
	if ((io = open(file, 0)) < 0) errfunc(OPERR);
	ninbuf = 0;
	/* don't flush during read */
	saveit = flushlim;
	flushlim = BIGLIM;
	append(getfile, addr);
	exfile();
	flushlim = saveit;
}

/*
name:
	writef1

function:
	Write a null terminated string without the terminator

algorithm:
	Determine the length of the string.  Write it out.
 CAVEAT	This call is not buffered, so use it carefully or pay the cost
	of high I/O expense.  It was writted to decrease subroutine linkage
	global variable referencing and module size.  But those gains can
	easily be lost if writef is consistantly called with small strings.
	where buffered i/o would be possible.

parameters:
	*char	pointer to null terminated string.

returns:
	int	value returned by write.

globals:

calls:
	write()

called by:
	lotsa people

history:
	Coded by R. Balocca (CAC) sometime in 1976
*/
writef1(str)	char*	str;
{
	register char *r;
	register char *s;

	s = str;
	r = s;
	while(*s++);

	return write(1, r, --s-r);
}

writenl()
{
	extern char NL[];
	writef1(NL);
}

errr()
{
	extern char Q[];

	errfunc(Q);
}

numout(i)	int i;
{
#ifndef LCOUNT
		count[1] = i;
#endif
#ifdef LCOUNT
		lcount = i;
#endif
		putd();
		ptchar('\n');
}

#ifndef EXTERNAL
char TMPERR[] "Temp file error";
char IOERR[] "Write error";
char BUFERR[] "Buffer overflow";
char NMCERR[] "No more core";
char BELL[] "";			/* ascii bell */
char BACKSL[] "\\";		/* ascii backslash */
char SH[] "/bin/sh";
char MSH[] "-sh";
char CRERR[] "Can't create";
char PROMPT[] "* ";
char WREDHUP[] "w ed-hup\n";
char NL[] "\n";
char OPERR[] "Can't open";
char BANG[] "!";
char Q[] "?";
char MC[] "-c";
char DONTGO[] "Modified file not written out";
#endif
