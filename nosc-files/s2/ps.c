#/*
Installation:
	if $1e = finale goto final
	cc ps.c -lj
	if ! -r a.out exit
	chmod 2755 a.out
	su chgrp system a.out
	exit
: final
	cc -s -O ps.c -lj
	if ! -r a.out exit
	su rm -f /bin/ps
	su cp a.out /bin/ps
	su chmod 2555 /bin/ps
	su chown bin /bin/ps
	su chgrp system /bin/ps
	rm -f a.out

 * ps -bB must be run once per reboot or modification
 *   to /dev/tty?
 *
 * Derived from the orignal ps program (BTL ?) by J.S.Kravitz at
 * University of Illinois. This program is somewhat faster than the
 * original since scan of /dev for swapdev and tty correspondence
 * is performed only once per re-boot.
 * ps -b should be run as part of the start up procedure.
 * With the -s and -k switches, ps builds a new internal table each time.
 * The switches -t and -n * have been added.
 *	-tx where is is a tty letter gets you the process for that terminal.
 *	-nxxx where xxx is a process id gets you the process info for
 *		 that process.
 *
 * added 30Jun77 -p flag.. lets you set nice value for process
 * added  8Jul77 -c flag... lets you see children of a process
 * added 27Sep77 fout cludge to speed it up RJB
 * added 30Sep77 consistency check on U structure RJB
 * added 30Sep77 Corrected time print RJB
 * added 12Dec77 Replaced heuristic to get arg list RJB
 * added 15Dec77 Fixed psinfo to have _PROC and swapdev name
 * added 14Jan78 Merged ps.h ps.c psinfo.h, added -b(uild) flag
 */
#define MAJ_KL11  0	/* major device number of KL11 */
/*  We don't have a DC11....
#define MAJ_DC11  3	/* major device number of DC11 */
#define MAJ_DH11  2	/* major device number of DH11 */
#define MAJ_PTY   8	/* major device number of psuedo-TTYs */

#include "../h/param.h"
#define NO_U X
#include "../h/user.h"
#include "../h/proc.h"
#include "../h/tty.h"
/* to keep stupid compiler happy with tty.h */
char partab[1];

struct statbuf
{
	char s_minor;
	char s_major;
	int s_inumber;
	int s_flags;
	char s_nlinks;
	char s_uid;
	char s_gid;
	char s_size0;
	int s_size1;
	int s_addr[8];
	long s_actime;
	long s_modtime;
};

struct syment
{
	char name[8];
	int type;
	int value;
};


#define NTTYI	62
struct psinfo
{
	struct ttyinf
	{
		int devname;
		char *pgrpaddr;
		int pgrpval;
		int devno;
	}ttyinfo[NTTYI];
	char	swapname [28];
	char	*procbase;
} psinfo;

int
	 ua[256]
	,uid
	,lflg
	,cflg
	,pgrpcnt
#define PGRPMAX	25
	,pgrpinx[PGRPMAX]
	,pgrp[PGRPMAX]
	,bflg
	,Bflg
	,xflg
	,aflg
	,wflg
	,nflg
	,pflg
	,qflg
	,mem
	,pcount
	,headed
	,swmem
	,swap
	,psfid
	;
char
	 *coref		"/dev/mem"
	,*symref	"/unix"
	;


char infoname[]	"/etc/psinfo1";

main(ac, av)
char **av;
{
	int n, b;
	register int i, c;
	register char *ap;
	int infofid;
	int argc; char **argv;

	extern int fout;
	fout = dup(1);

	argc = ac;  argv = av;

	while (argc > 1)
	{
		argc--;
		ap = *++argv;
		while (*ap) switch (*ap++)
		{
		case 'a':	/* all uid's */
			aflg++;
			break;

		case 'b':	/* build flag */
			bflg++;
			break;

		case 'B':
			Bflg++;		/* save built info */
			break;

		case 'c':	/* specific child of parent  */
			cflg = atoi(ap);
			ap = scan(ap);
			aflg++;
			xflg++;
			break;

		case 'k':	/* use sysdump for core */
			coref = "sysdump";
			break;

		case 'K':	/* use specified file for core */
			if(argc < 1) break;
			argc--;
			coref = *++argv;
			break;

		case 'l':	/* long format */
			lflg++;
			break;

		case '0':	/* number -- assume specific pid */
		case '1':	case '2':	case '3':
		case '4':	case '5':	case '6':
		case '7':	case '8':	case '9':
			ap--;
		case 'n':	/* specific pid */
			nflg = atoi(ap);
			ap = scan(ap);
			aflg++;
			xflg++;
			break;

		case 'p':	/* poke process's nice value */
			pflg = atoi(ap);
			ap = scan(ap);
			break;

		case 'q':	/* suppress headers (quiet) */
			qflg++;
			break;

		case 's':	/* use specified file for symbol table */
			if(argc < 1) break;
			argc--;
			symref = *++argv;
			bflg++;
			break;

		case 't':	/* for the specified tty only */
			c = maptty (*ap);
			if (c < 0)
				continue;
			ap++;
			pgrpinx [pgrpcnt++] = c;
			if (pgrpcnt >= PGRPMAX)
			{
				printf ("Too may 't' arguments\n");
				done (0);
			}
			aflg++;
			break;

		case 'w':	/* print out symbol names */
			wflg++;
			lflg++;
			break;

		case 'x':	/* include procs with no process group */
			xflg++;
			break;

		default:
			printf("Unknown flag: %c\n", ap[-1]);
		case '-':	/* ignore this */
			break;

		}
	}

	if ((mem = open(coref, pflg?3:0)) < 0)
	{
		printf("No mem\n");
		done (0);
	}
	swmem = open(coref, 0);
	if (bflg
	|| ( (psfid = open (infoname, 0)) < 0 )
	|| ( read (psfid, &psinfo, sizeof psinfo) != sizeof psinfo )
	) {
		getpsinfo ();
	}
	else
	{
		close (psfid);
	}
	chdir ("/");
	for (i = 0; i < pgrpcnt; i++)
	{
		seek (mem, psinfo.ttyinfo[pgrpinx[i]].pgrpaddr, 0);
		read (mem, &pgrp[i], 2);
	}
	swap = open (psinfo.swapname, 0);
	if (swap < 0)
	{
		printf ("Can't open %s for swap device\n"
			,psinfo.swapname);
		done (0);
	}
	if (wflg)
		syminit();
	/*
	 * Read in proc table
	 */
	if(pflg)
	{
		if ( nflg == 0)
		{
			printf ("Must specify process id\n");
			done (0);
		}
		procprint (mappid (nflg), 1);
		setprio (psinfo.procbase
			,&proc[0]
			,&proc[mappid (nflg)].p_nice
			,mem
			,pflg
			);
		procprint (mappid (nflg), 1);
		done (0);
	}
	uid = getuid() & 0377;
	if (nflg)
	{
		procprint (mappid (nflg), lflg);
		done (0);
	}
	seek(mem, psinfo.procbase, 0);
	read(mem, proc, sizeof proc);
	cprint(0);
	if (headed)
		printf ("\n%d used,  %d unused\n", pcount, NPROC-pcount);
	done (0);
}

cprint(parent)
int parent;
{
	register int i;

	for(i = 0; i < NPROC; i++) {
		if(proc[i].p_stat == 0)
			continue;
		if(proc[i].p_ppid == parent) {
			pcount++;
			if(okprint(i)) procprint(i, lflg);
			if(proc[i].p_pid) cprint(proc[i].p_pid);
		}
	}
}

okprint(pid)
{
	register int i, j;

	i = pid;
	if (proc[i].p_stat==0)
		return 0;
	if (pgrpcnt)
	{
		for (j = 0; ; j++)
		{
			if (j >= pgrpcnt)
				return 0;
			if (pgrp[j] == proc[i].p_pgrp)
				break;
		}
	}
	else
	{
		if (	(proc[i].p_pgrp==0 && xflg==0)
		||	(nflg && nflg != proc[i].p_pid)
		||	(cflg && cflg != proc[i].p_ppid)
		   )	return 0;
	}
	if (uid != proc[i].p_uid & 0377 && aflg==0)
		return 0;
	return 1;	/* say OK */
}

procprint (pid, verbose)
{
	register i;

	seek (mem, psinfo.procbase+(pid * sizeof proc[0]), 0);
	read (mem, &proc[pid], sizeof proc[0]);
	if(!okprint(i = pid)) return;	/* validate that it hasn't changed */

	if (!qflg && !headed)
	{
		headed++;
		if(verbose)
		printf(
" F S UID  PPID NCE CPU PRI  ADDR  SZ  WCHAN%s  PID TTY TIME COMMAND\n"
			,wflg?"     ":"");
		else
			printf(
" PID TTY TIME COMMAND\n"		);
	}
	if (verbose)
	{
		printf(proc[i].p_flag?"%3o":"   ",proc[i].p_flag);
		printf("%c%4d%6l"
			,"0SWRIZT"[proc[i].p_stat]
			,proc[i].p_uid & 0377
			,proc[i].p_ppid
			);
		printf("%4d%4d%4d%6d%4d"
			,proc[i].p_nice
			,proc[i].p_cpu&0377
			,proc[i].p_pri
			,proc[i].p_addr
			,(proc[i].p_size+7)>>3
			);
		if (proc[i].p_wchan)
		{
			if (wflg)
			{
				symbol (proc[i].p_wchan);
			}
			else
			{
				printf("%7o", proc[i].p_wchan);
			}
		}
		else
		{
			printf (wflg?"            ":"       ");
		}
		putchar(' ');
	}
	printf("%5l", proc[i].p_pid);
	if (proc[i].p_stat==SZOMB)
		printf(" <defunct>");
	else
		prcom(i, verbose);
	printf("\n");
	flush();
}


prcom(procnum, verbose)
{
#define UASIZE sizeof ua
#define XSIZE 1024
	int baddr, laddr, mf;
	register int *ip;
	register char *cp, *cp1;
	int c, nbad;
	long
		clong,
		clong1,
		;
	struct { int integer[]; };

	baddr = 0;
	laddr = 0;
	if (proc[procnum].p_flag&SLOAD)
	{
		laddr = proc[procnum].p_addr;
		mf = swmem;
	}
	else
	{
		baddr = proc[procnum].p_addr;
		mf = swap;
	}
	baddr =+ laddr>>3;
	laddr = (laddr&07)<<6;
	seek(mf, baddr, 3);
	seek(mf, laddr, 1);
	/*
	 *  The following expression is logically a series of ors, but
	 *  the V6 C compiler gets an expression overflow if we don't
	 *  hack it this way.
	 */
	if (read(mf, ua, UASIZE) != UASIZE) goto nope;
	if (ua->u_dsize+ua->u_ssize+USIZE != proc[procnum].p_size) goto nope;
	if (ua->u_tsize<0 || ua->u_dsize<0 || ua->u_ssize<0) goto nope;
	if (ua->u_tsize>XSIZE || ua->u_dsize>XSIZE || ua->u_ssize>XSIZE)
	{
	nope:
		printf(" <unavailable>");
		return 0;
	}
	printf(" %c", gettty());
	clong.integer[0] = ua->u_utime[0];
	clong.integer[1] = ua->u_utime[1];
	clong1.integer[0] = ua->u_stime[0];
	clong1.integer[1] = ua->u_stime[1];
	clong1 =+ clong;
	if(clong1!=0)
		prclock(clong1);
	else
		printf("     ");
	if(procnum == 0)
		goto sched;
	c = proc[procnum].p_size - 8;
	laddr =+ (c&07)<<6;
	baddr =+ c>>3;
	seek(mf, baddr, 3);
	seek(mf, laddr, 1);
	if (read(mf, ua, sizeof ua) != sizeof ua)
		return(0);
	for (ip = &ua[256]; ip > &ua[0]; )
	{
		if ((*--ip)&0100200) 	/* nonascii? */
		{
			cp = ip+1;
			while (*cp==0)
				cp++;
			nbad = 0;
			for (cp1 = cp; cp1 < &ua[256]; cp1++)
			{
				c = *cp1;
				if (c==0)
					*cp1 = ' ';
				else
				if (c < ' ' || c > 0176)
				{
					if (++nbad >= 5)
					{
						*cp1++ = ' ';
						break;
					}
					*cp1 = '?';
				}
			}
			while (*--cp1==' ')
				*cp1 = 0;
sched:
			printf(verbose?" %.16s":" %.64s"
				,procnum?cp:"scheduler");
			return(1);
		}
	}
	return(0);
}

gettty()
{
	register i;

	if (ua->u_ttyp==0)
		return(' ');
	for (i=0; i<=NTTYI; i++)
		if (	(psinfo.ttyinfo[i].devno == ua[0].u_ttyd)
		&&	 psinfo.ttyinfo[i].devname )
			return(psinfo.ttyinfo[i].devname);
	return('?');
}

#define HSIZE 16	/* executable file header size */
int buf[HSIZE];
int	sym;
struct syment *symtab;
struct syment *maxsym;

syminit()
{
	register int i, j;
	sym = open(symref,0);
	if (sym < 0)
	{
		printf("Can't open %s\n",symref);
		done(1);
	}
	else
	{
		read(sym,buf,HSIZE);
		if (buf[0]!=0407 && buf[0]!=0410 && buf[0]!=0411)
		{
			printf("Format error in %s\n",symref);
			done(1);
		}
		seek(sym,buf[1],1);
		seek(sym,buf[2],1);
		if (buf[7] != 1)
		{
			seek(sym,buf[1],1);
			seek(sym,buf[2],1);
		}
		i = buf[4];
		i = buf[4]/12;
		j = i*12;
		if (i == 0)
		{
			printf("No namelist in %s\n",symref);
			done(1);
		}
		symtab = sbrk(j);
		seek (sym, HSIZE + (buf[7]?1:2)*(buf[1]+buf[2]), 0);
		read(sym,symtab,j);
		maxsym = symtab + i;
		close(sym);
	}
}


symbol(val)
char	*val;
{
	register char *v;
	register struct syment *i;
	register char *mindif;
	char	*minptr;
	char	*thisdif;

	v = val;
	mindif = 077777;
	minptr = 0;
	for(i=symtab; i<maxsym; i++)
	{
		if (((i->type&077) != 043) && ((i->type&077) != 044)) continue;
		if (i->value > v) continue;
		thisdif = v - i->value;
		if (thisdif >= mindif) continue;
		mindif = thisdif;
		minptr = i;
	}
	if (minptr == 0) return(0);
	printf(
		mindif>0?
		"%5o+"
		: "      "
		, mindif
		);
	printf ("%-6.6s"
		, minptr->name+1
		);
	return(minptr);
}

scan(pp)	char * pp;
{
	register char * p;
	register char c;

	for(p = pp;	(c = *p++) >= '0' && c <= '9'; );
	return (--p);
}

done(i)	int i;
{
	flush();
	exit(i);
}

/*
 *	setprio (kerprocbase, psprocbase, psniceaddr, memfile, niceval)
 *		lets the super-user change the priority of a job
 */

setprio (kerprocbase, psprocbase, psniceaddr, memfile, niceval)
char *kerprocbase, *psprocbase, *psniceaddr;
int niceval;
{
	extern int errorr;
	register char *niceaddr;

	if (getuid () != 0)
	{
		printf ("Forbidden\n");
		done (0);
	}
	niceaddr = kerprocbase + (psniceaddr - psprocbase);

	if ( store(niceaddr, niceval & 0177, memfile) < 0 )
		return (-1);

	if (errorr == -1)
		return (-1);

	return (sizeof niceval);
}

char *
load (addr, memfile)
char * addr;
{
	extern int errorr;
	char * val;

	errorr = 0;
	return (
		seek(memfile,addr,0)<0
		|| sizeof val != read(memfile,&val,sizeof val)
		?
			(errorr = -1)
			:
			val
	);
}

store(addr,contents, memfile)
char * addr;
char contents;
{
	extern int errorr;

	errorr = 0;

	return (
		seek(memfile,addr,0)<0
		|| sizeof contents != write(memfile,&contents,sizeof contents)
		?
			(errorr = -1)
			:
			contents
	);
}

prclock(l)	long l;
{
	if(l<0)
	{
		printf("-");
		l = -l;
	}
	l =/ HZ;	/* make it seconds */
#define SEC_MIN	60
#define MIN_HR	60
	if(l/(SEC_MIN*MIN_HR)>0)	/* greater than one hour? */
	{
		printf("%D:%02D:%02D"
			,l/(SEC_MIN*MIN_HR) 	/* hours */
			,(l/MIN_HR)%SEC_MIN	/* minutes */
			,l % MIN_HR		/* seconds */
			);
	}
	else
	{
		printf("%2D:%02D"
			,(l/MIN_HR)%SEC_MIN	/* minutes */
			,l % SEC_MIN		/* seconds */
			);
	}
}
int errorr;

mappid (mappedid)
{
	register int i;

	if( (i = mappedid & 0377) < NPROC) return i;
	printf("Error, %l is not a legal process id\n", mappedid);
	done(1);
}

/*
 * Build the tables required
 *
 */

#include "../h/conf.h"
/* to keep stupid compiler happy with conf.h */
struct bdevsw bdevsw[1];
struct cdevsw cdevsw[1];



struct syment nl [6];

#define	_KL11		0
#define	_DC11		1
#define	_DH11		2
#define	_PTY		3
#define	_PROC		4
#define	_SWAPDEV	5


struct tty tty;

int	swapdev;


getpsinfo ()
{
	register int infofid;
	register int symfid;


	setup (&nl[_KL11], "_kl11");
	setup (&nl[_DC11], "_dc11");
	setup (&nl[_DH11], "_dh11");
	setup (&nl[_PTY], "_pty");
	setup (&nl[_PROC], "_proc");
	setup (&nl[_SWAPDEV], "_swapdev");
	nlist (symref, nl);
	if (nl[_KL11].type==0)
	{
		printf("No namelist\n");
		done (0);
	}
	psinfo.procbase = nl[_PROC].value;
	symfid = open (symref, 0);
	if (symfid  < 0)
	{
		printf ("Can't open %s (symbols and swap #)\n", symref);
		done (0);
	}
	seek (symfid, nl[_SWAPDEV].value + HSIZE, 0);
	read (symfid, &swapdev, 2);
	getdev ();
	close (symfid);
	if (Bflg)
	{
		if ((infofid = creat (infoname, 0644)) < 0)
			printf ("Can't create psinfo\n");
		else
			write (infofid, &psinfo, sizeof psinfo);
		done(0);
	}
}

getdev()
{
	register struct
	{
		 int dir_ino;
		 char dir_n[14];
	} *p;
	register i;
	register char c;
	struct ttyinf *ttyp;
	char *charp;
	int swapfound;
	int f;
	char dbuf[512];
	struct statbuf statbuf;

	swapfound = -1;
	if (chdir ("/dev") < 0)
	{
		printf ("Can't chdir to /dev\n");
		done (0);
	}
	f = open("", 0);
	if(f < 0)
	{
		printf("Can't open /dev\n");
		done (0);
	}

	if (Bflg)
		printf ("Procbase is %6o\n", psinfo.procbase);
loop:
	i = read(f, dbuf, sizeof dbuf);
	if (i <= 0)
	{
		close(f);
		return;
	}
	while (i < sizeof dbuf)
		dbuf[i++] = 0;
	for(p = dbuf; p < dbuf+sizeof dbuf; p++)
	{
		if(p->dir_ino == 0)
			continue;
		if(p->dir_n[0] == 't' &&
		   p->dir_n[1] == 't' &&
		   p->dir_n[2] == 'y' &&
		   p->dir_n[4] == 0 &&
		   p->dir_n[3] != 0)
		{
			if(stat(p->dir_n, &statbuf) < 0)
				continue;
			c = maptty (p->dir_n[3]);
			ttyp = &psinfo.ttyinfo [c];
			ttyp->devname = p->dir_n[3];
			ttyp->devno = statbuf.s_addr[0];
			switch (ttyp->devno.d_major)
			{
#ifdef MAJ_KL11
			case MAJ_KL11:
				ttyp->pgrpaddr = nl[_KL11].value;
				break;
#endif
#ifdef MAJ_DC11
			case MAJ_DC11:
				ttyp->pgrpaddr = nl[_DC11].value;
				break;
#endif
#ifdef MAJ_DH11
			case MAJ_DH11:
				ttyp->pgrpaddr = nl[_DH11].value;
				break;
#endif
#ifdef MAJ_PTY
			case MAJ_PTY:
				ttyp->pgrpaddr = nl[_PTY].value;
				break;
#endif
			}
			ttyp->pgrpaddr =+ (((sizeof tty)*(ttyp->devno.d_minor+1))-2);
			if (Bflg)
				printf ("tty%c %2d %2d %6o\n"
					,ttyp->devname
					,ttyp->devno.d_major
					,ttyp->devno.d_minor
					,ttyp->pgrpaddr
					);
			continue;
		}
		else
		if (swapfound < 0)
		{
			if(stat(p->dir_n, &statbuf) < 0)
				continue;
			if((statbuf.s_flags & 060000) != 060000) /* IFBLK */
				continue;
			if(statbuf.s_addr[0] == swapdev)
			{
				charp = setup (psinfo.swapname, "/dev/");
				setup (charp, p->dir_n);
				swapfound++;
				if (Bflg)
					printf ("Swapdev is %s\n", psinfo.swapname);
			}
		}
	}
	goto loop;
}


setup (p, s)
char *p, *s;
{
	while (*p++ = *s++);
	return (--p);
}

maptty (cc) char cc;
{
	register int c;

	c = cc;
	if ((c >= 'A') && (c <= 'Z'))
		c =- 'A'-26;
	else
	if ((c >= 'a') && (c <= 'z'))
		c =- 'a';
	else
	if ((c >= '0') && (c <= '9'))
		c =- '0'-52;
	else
	if (c=='.')
		c = maptty(ttyn(0));
	else
	{
		printf ("tty%c ?\n", c);
		c = -1;
	}
	return (c);
}
