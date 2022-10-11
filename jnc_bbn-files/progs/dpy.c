#
/*
 * dpy [mlfn] [filename]
 *
 * Maintain system status display on a VT52 screen.
 * Reads system process table, displays pertinent information,
 * then sleeps for a period which is determined in this manner:
 * The system idle percentage is estimated from the sum of the
 * `cpu' factors of the individual processes.  This is then decreased
 * by twice the current `nice' number, and the sleep duration is
 * computed on a hyperbolic scale with (basically):
 * 		sleep  =  <CONST> / idle_percent
 * The total duration of the sleep is bounded by a reasonable number,
 * so that an idle of zero won't cause you to lose too badly.
 *
 * Options:
 *
 * m - display only processes owned by me
 * l - display all processes
 * f - dump output into file specified by next arg
 * n - new system, so recompute location of globals in `/dev/kmem'
 *
 */

#include "../h/param.h"
#include "../h/proc.h"
#include "../h/tty.h"
#include "../h/user.h"

#define reg	register
#define procptr	struct proc*

#define MAXID	256
char*	Users[MAXID];

char*	_proc;				/* system locations  */
char*	_swapdev;
char*	_coremap;
char*	_swapmap;
char	partab[1];			/* so it's not undefined */

struct proc proc[NPROC];		/* copy of processs array */
struct tty  tty;			/* used for looking at tty table */
struct user u;				/* per process user table */

char	*eol	"\n";		/* end of line */

int	vflg;		/* VT52 flag */
int	bflg;		/* Ann arbor flag */
int	aflg  1;
int	lflg;
int	fflg;
int	nflg;
int	rflg;

int	mynice;				/* nice-factor of this process  */
int	me;				/* process ID of this process  */

int	mem;				/* file descriptors for mem and swap */
int	swap;

struct					/* buffered output  */
    {	int	fildes;
	int	nunused;
	char*	xfree;
	char	outbuf[512];
    }
    obuf;

char*	filname;
int     stbuf[257];                     /* random block size buffer */
int	uid;				/* number of devices found, our uid */

				/* ===== device info ===== */
int	ndev;				/* number of devices */
char	devc[65];			/* table of teletype names */
int	devl[65];			/* inodes for teletypes */
char	swapdev[60];			/* name of swap device */

struct ibuf
    {	char	idevmin, idevmaj;
	int	inum;
	int	iflags;
	char	inl;
	char	iuid;
	char	igid;
	char	isize0;
	int	isize;
	int	iaddr[8];
	char*	ictime[2];
	char*	imtime[2];
	int	fill;
    };

reset ()
    {
	signal(16,reset);
    }

main (argc, argv)
		int	argc;
		char	*argv[];
    {
	if (signal(16,reset) & 01)
		signal(16,1);
	obuf.fildes = 1;		/* output to standard output device */
	Fill(Users, "/etc/passwd");
	if (argc > 1)
		rd_options(argv[1]);
	if (fflg && argv[2][0]!='/')
		done("Output file name must begin with /\n");
	filname = argv[2];
	if (chdir("/dev") < 0)		/* opens goto /dev */
		done("Cannot change to /dev\n");
	get_syms ();
	mem = open("/dev/mem", 0);	/* open core for reading tables */
	if (mem < 0)
		done("No mem\n");
	uid = getuid() & 0377;		/* find out who we are */
	me  = getpid();
	seek(mem, _swapdev, 0);		/* find out swapping device */
	read(mem, &_swapdev, 2);
	getdev();			/* fill in device table once */

	if (rflg)
		for (;;)
			display();
	else
		display();
	exit();
    }


Fill (Tbl, File)	/* Fill User (Group) Table from User (Group) File  */
	char*	Tbl[];		/* The Table Itself  */
		char*	File;		/* The Name of the File  */
    {		char	A[400];		/* Buffer for Reading a Line  */
	reg	char*	P;		/* Pointer into the Buffer  */
		int	B[260];		/* Buffer for `getc()' Routine  */
	reg	int	i;		/* An Integer  */
		int	length;		/* length of the user-name  */

	if (fopen(File, B) == -1)
		return;

	while ((A[0] = getc(B)) != -1)
	    {
		P = &A[1];
		while ((*P = getc(B)) != '\n')
			if (*P++ == -1)
				return;
		P = &A[0];
		while (*P++ != ':');
		P[-1] = 0;
		length = P - A;
		while (*P++ != ':');
		if (*P == ':')
			continue;
		i = atoi(P);
		if (i < 0 || i >= MAXID || Tbl[i] != 0)
			continue;
		Tbl[i] = sbrk(length);
		smove(Tbl[i], A);
	    }
	close(B[0]);
	return;
    }


display ()
    {	reg	int	i;
	reg	int	sum;
	reg	procptr	p;
		int	idletime;
		int	effidle;

	if (fflg)
		if (fcreat(filname, &obuf) == -1)
			done();
	if (vflg)
		printf("\033H\033J");
	else if (bflg)
		printf("\014");
	else
		printf("\n\n\n");
	sum = summap(_coremap, CMAPSIZ);
	sum = sum*5/16;			/* this is number of 100 wrd blocks */
	prtime();
	printf("  Free Core = %d.%1dK", sum/10, sum%10);
	sum = summap(_swapmap, SMAPSIZ);
	printf(", Swap = %l blks", sum);

	seek(mem, _proc, 0);		/* get a copy of the process array */
	read(mem, proc, sizeof proc);
	idletime = 100;
	effidle = 100;
	i = 0;
	for (p=proc; p < &proc[NPROC]; p++)
		if (p->p_stat != 0)
		    {
			i++;
			if (p->p_pgrp != 0)
			    {
				sum = ((p->p_cpu&0377)*100)/255;
				idletime =- sum;
				if (p->p_nice > 10)
					sum =>> 1;
				effidle =- sum;
			    }
		    }
	printf(", Idle = %d%%",idletime);
	printf("  %d/%d Procs", i, NPROC);
	printf(eol);

	printf ("user    tty pid  np s(k)   idle  cputime  %%  status        command%s",eol);

	for (p=proc; p < &proc[NPROC]; p++)	/* loop through processes */
		if (check(p) && p->p_ppid == 1)
			dpy_process (p , 0);
	if (fflg) close(obuf.fildes);	/* close file so next open will
					   start from beginning  */

	i = effidle;		/* === compute length of our nap === */
	i =- mynice*2;			/* skew it other way for nice people */
	if (i<15) i = 15;		/* put ceiling on our nap time  */
	i = 900/i;			/* nap increases with system load  */

	if (rflg)
		printf("(Sleep=%d)",i);

	fflush(&obuf);			/* dump buffer */
	if (rflg)
		sleep(i);
    }


prtime ()

	{register char *ap;
	int tvec[2];

	time (tvec);
	ap = ctime(tvec);	/* get the time in ascii format */
	ap[16] = 0;
	printf ("%s",ap);
	}

int check (p)			/* return true if process is displayable */
	struct proc *p;	/* arg is pointer to process array */
    {
	if (p->p_pid == me) mynice = p->p_nice;
	if (p->p_stat == 0) return (0);	/* process isn't active */
	if (lflg) return(1);
	if (p->p_pgrp == 0) return(0);  /* process doesn't have a tty */
	if (p->p_ppid == 1 && p->p_uid == 0) return(0); /* init */
	if (uid != (p->p_uid&0377)) return(aflg);/* he's someone else */
	return(1);				/* otherwise, he's OK */
    }

dpy_process(ap, level)			/* puts process onto screen */
	struct proc *ap;	/* arg is pointer to process table and level */

	{int h,i,j,k;
	double cpu;
	register struct proc *p, *q;
	register char *s;
	struct {
		long lint;
	};

	p = ap;
	if (level>0)			/* at higher levels use indentation */
		for (i=0; i<9; i++)
			printf (i==level ? "*": " ");
	else				/* at zero level use name  */
		printf ("%-9.9s",Users[p->p_uid]);

	i = 0; j = 0;			/* calculate address of user table */
	if (p->p_flag&SLOAD)	     /* process loaded -> use core address */
		{i = p->p_addr; k = mem;}	/* read from memory */
	else
		{j = p->p_addr; k = swap;}	/* otherwise from swap area */
	j =+ i>>3;				/* this is block address */
	i = (i & 07) << 6;			/* this is byte offset */
	seek(k,j,3); seek(k,i,1);		/* seek to correct byte */
	if (read(k,&u,sizeof u) != sizeof u) goto noread;
	for (i=0; i<ndev; i++)		/* now figure out teletype name */
		if (devl[i] == u.u_ttyd) /* see if we already know the name */
			{printf("%c",devc[i]);  /* if so print it and quit */
			goto out;
			}
	printf("?");			/* can't figure out tty name */

out:	printf("%6l",p->p_pid);		/* next is process id */
	printf("%3d",p->p_nice);
	prsize (p->p_size, "%3d.%1d");

	if ((cpu = 0 /* p->p_sleep */)==0) printf("        ");
	else { 	j = cpu/60; i = cpu-60*j; printf("%4d:%d%d ",j%1000,i/10,i%10); };

/*************/
	cpu = u.u_utime->lint + u.u_stime->lint +
		u.u_cutime->lint + u.u_cstime->lint; /* this is total cpu tm */
	k = cpu/216000.;			/* hours */
	j = fcalc(cpu,216000.,1./3600.);	/* minutes */
	i = fcalc(cpu,3600.,1/60.);		/* seconds */
	h = fcalc(cpu,60.,5./3.);		/* hundredths */
	if (k)
		printf("%2d:%d%d:%d%d",k,(j%100)/10,j%10,i/10,i%10);
	else
		printf("%2d:%d%d.%d%d",j%100,i/10,i%10,h/10,h%10);

	k = ((p->p_cpu&0377)*100)/255;
	if (!k) printf("   "); else printf("%3d",k);

	status(p);			/* followed by process status */
	if (p->p_stat == 5) printf(" <defunct>");  /* finally command name */
	else prcom(p);			/* use the real thing if we have it */

noread:	printf(eol);				/* that's all there is! */
	p->p_stat = 0;			/* remember that we did this one */
	for (q=proc; q < &proc[NPROC]; q++) /* print out all our inferiors */
	   if (q->p_ppid == p->p_pid) dpy_process(q,level+1);
	}

getdev()

	{int fd;

	if (!nflg) fd = open ("/tmp/wdev", 0);
	else fd = -1;
	if (fd >= 0)
		{read (fd, &ndev, 2);
		read (fd, devc, ndev);
		read (fd, devl, ndev*2);
		read (fd, swapdev, 60);
		close (fd);
		swap = open (swapdev, 0);
		}
	if (fd < 0)
		{rddev ();
		fd = creat ("/tmp/wdev", 0666);
		if (fd >= 0)
			{write (fd, &ndev, 2);
			write (fd, devc, ndev);
			write (fd, devl, ndev*2);
			write (fd, swapdev, 60);
			close (fd);
			}
		}
	}

rddev()

	{register struct { int dir_ino; char dir_n[14]; } *p;
	register i, c; int f;
	char dbuf[512];
	int sbuf[20];

	f = open ("/dev", 0);
	if (f < 0) {
		printf("cannot open /dev\n");
		done();
		}
	swap = -1;
	c = 0;

	for (;;)
		{if ((i = read (f, dbuf, 512)) <= 0)
			{close (f);
			if(swap < 0)
				{printf("no swap device\n");
				done();
				}
			ndev = c;
			return;
			}
		while (i < 512)	dbuf[i++] = 0;
		for (p = dbuf; p < dbuf+512; p++) {
			if(p->dir_ino == 0) continue;
			if(p->dir_n[0] == 't' &&
			   p->dir_n[1] == 't' &&
			   p->dir_n[2] == 'y' &&
			   p->dir_n[4] == 0 &&
			   p->dir_n[3] != 0) {
				if(stat(p->dir_n, sbuf) < 0)
					continue;
				devc[c] = p->dir_n[3];
				devl[c] = sbuf->iaddr[0];
				c++;
				continue;
			}
			if(swap >= 0) continue;
			if(stat(p->dir_n, sbuf) < 0) continue;
			if((sbuf->iflags & 060000) != 060000) continue;
			if(sbuf->iaddr[0] == _swapdev)
				{swap = open(p->dir_n, 0);
				if (swap >= 0)
					smove (swapdev, p->dir_n);
				}
			}
		}
	}

smove (p, s)
	char *p, *s;
	{while (*p++ = *s++);}

get_syms ()

   {struct { char name[8]; int type; char *value; } nl[5];
   int fd, n;

   if (!nflg) fd = open ("/tmp/w", 0);
   else fd = -1;
   if (fd >= 0)
	{n = read (fd, &_proc, 8);
	close (fd);
	if (n < 8) fd = -1;
	}

   if (fd < 0)
	{smove(&nl[0],"_proc");		/* setup symbols we want from system */
	smove(&nl[1],"_swapdev");
	smove(&nl[2],"_coremap");
	smove(&nl[3],"_swapmap");
	smove(&nl[4],"");

	nlist("/unix",nl);		/* go find out their values */
	if (nl[0].type == 0)		/* complain if we can't find them */
		{printf("No namelist\n"); done();}
	_proc = nl[0].value;
	_swapdev = nl[1].value;
	_coremap = nl[2].value;
	_swapmap = nl[3].value;

	fd = creat ("/tmp/w", 0666);
	if (fd >= 0)
		{write (fd, &_proc, 8);
		close (fd);
		}
	}
   }

prcom(p)
	struct proc *p;

	{int baddr, laddr, mf;
	register int *ip;
	register char *cp, *cp1;
	int c, nbad;

	baddr = 0;
	laddr = 0;
	if (p->p_flag&SLOAD) {
		laddr = p->p_addr;
		mf = mem;
	} else {
		baddr = p->p_addr;
		mf = swap;
	}
	laddr =+ p->p_size - 8;
	baddr =+ laddr>>3;
	laddr = (laddr&07)<<6;
	seek(mf, baddr, 3);
	seek(mf, laddr, 1);
	if (read(mf, stbuf, 512) != 512)
		return(0);
	for (ip = &stbuf[256]; ip > &stbuf[0];) {
		if (*--ip == -1) {
			cp = ip+1;
			if (*cp==0)
				cp++;
			nbad = 0;
			for (cp1 = cp; cp1 < &stbuf[256]; cp1++) {
				c = *cp1;
				if (c==0)
					*cp1 = ' ';
				else if (c < ' ' || c > 0176) {
					if (++nbad >= 5) {
						*cp1++ = ' ';
						break;
					}
					*cp1 = '?';
				}
			}
			while (*--cp1==' ')
				*cp1 = 0;
			printf("%.32s",cp);
			return(1);
		}
	}
	return(0);
}

done (format, a, b, c, d, e)
    {
	printf(format, a, b, c, d, e);
	fflush(&obuf);
	exit(4);
    }

int fcalc(a,b,f)		/* calculates fmod(a,b)*f */
	double a,b,f;

	{register int temp; double t;
	t = a/b; temp = t;
	if (temp > t) temp =- 1;
	return ((a - temp*b)*f);
	}

prsize (size, format)
  char *format;
  {int i;
  i = size*5/16;	/* number of K times 10 */
  if (16*i != 5*size) ++i;	/* round up */
  printf (format, i/10, i%10);
  }

rd_options (ap)
	char *ap;

	{register char c, *s;
	s = ap;

	while (c = *s++) switch (c) {	/* loop thru characters */
	case 'm': aflg=0; break;	/* he wants only himself */
	case 'a': bflg=1; break;	/* ann arbor output */
	case 'l': lflg++; break;	/* include processes running INIT */
	case 'n': nflg++; break;	/* new system - recompute sys stuff */
	case 'f': fflg++; break;	/* maintain file output */
	case 'v': vflg++; break;	/* VT52 output */
	case 'r': rflg++; break;	/* continuous display */
		}
	}

putchar(c)
{
	putc(c,&obuf);
}

status(p)
 struct proc *p;
{ 	char *s;

	if (!(p->p_flag&077)) s = "swapped";
	else switch (p->p_stat)
	 { case 0:	s = "nonexistent"; break;

	   case 1:	switch (p->p_pri)
			{ case -100:	s = "being swapped"; break;
			  case  -90:	s = "inode"; break;
			  case  -50:	s = "block i/o"; break;
			  case	  1:	s = "pipe i/o"; break;
			  case   10:	s = "tty input"; break;
			  case   20:	s = "tty output"; break;
			  case   40:	s = "waiting"; break;
			  case   90:	s = "snoozing"; break;
			  default:	s = "sleeping"; break;
			}; break;

	   case 2:	s = "abandoned"; break;
	   case 3:	printf(" p=%-4d c=%-3d ",p->p_pri,p->p_cpu&0377); return;
	   case 4:	s = "being created"; break;
	   case 5:	s = "terminated"; break;
	   case 6:	s = "suspended"; break;
	   default:	s = "who knows?"; break;
	 };
	printf(" %-13s",s);
}

summap(amap, asize)
{
	register char *sum;
	register int i, nb;

	asize =* 2;                              /* convert to bytes */
	sum = 0;
	do {
		seek(mem, amap, 0);
		nb = read(mem, stbuf, ((asize<512)?asize:512));
		asize =- nb;
		nb =/ 2;                        /* convert to words */
		for (i = 0; i<nb && stbuf[i]!=0; i =+ 2)
			sum =+ stbuf[i];
	} while (asize > 0);
	return(sum);
}
