#/*
Module Name:
	ac.c -- login accounting

Installation:
	if $1e = finale goto finale
	if -r a.out rm -f a.out
	cc ac.c
	if ! -r a.out -o $1x != testx exit
	echo testing....
	a.out -p -d
	exit
: finale
	cc -O -s ac.c
	if ! -r a.out exit
	su cp a.out /usr/bin/ac
	rm -f a.out

Synopsis:
	acct [ -w wtmp ] [ -d ] [ -p people ]

Function:

Restrictions:

Diagnostics:

Files:

See Also:

Bugs:

Compile time parameters and effects:

Module History:
	Original from Bell Labs.
	Modified Nov78 by Greg Noel to use long arithmetic instead of
	    floating point.
*/
#define	TSIZE	(10+26+26)
#define	USIZE	200

struct {
	char	name[8];
	char	tty;
	char	fill1;
	long	time;
	int	fill2;
} ibuf;

struct ubuf {
	char	name[8];
	long	utime;
} ubuf[USIZE];

struct tbuf {
	struct	ubuf	*userp;
	long	ttime;
} tbuf[TSIZE];

char	*wtmp;
int	pflag, byday;
long	dtime;
long	midnight;
long	lastime;
long	day;
long	day1;	/* 1.5*day */
int	pcount;
char	**pptr;

main(argc, argv) 
char **argv;
{
	int c, fl;
	register i;
	register char *ip;
	extern fin;
	int f;

	day = 24;	day =* 60*60;
	day1 = 36;	day1 =* 60*60;
	wtmp = "/usr/adm/wtmp";
	while (--argc > 0 && **++argv == '-')
	switch(*++*argv) {
	case 'd':
		byday++;
		continue;

	case 'w':
		if (--argc>0)
			wtmp = *++argv;
		continue;

	case 'p':
		pflag++;
		continue;
	}
	pcount = argc;
	pptr = argv;
	if (fopen(wtmp, &fin) < 0) {
		printf("No %s\n", wtmp);
		return;
	}
	for(;;) {
		ip = &ibuf;
		for (i=0; i<16; i++) {
			if ((c=getc(&fin)) < 0)
				goto brk;
			*ip++ = c;
		}
		if (ibuf.tty == 0) goto skip;
		fl = 0;
		for (i=0; i<8; i++) {
			c = ibuf.name[i];
			if ('0'<=c&&c<='9'||'a'<=c&&c<='z'||'A'<=c&&c<='Z') {
				if (fl)
					goto skip;
				continue;
			}
			if (c==' ' || c=='\0') {
				fl++;
				ibuf.name[i] = '\0';
			} else
				goto skip;
		}
		loop();
    skip:;
	}
    brk:
	ibuf.name[0] = '\0';
	ibuf.tty = '~';
	time(&ibuf.time);
	loop();
	print();
}

loop()
{
	register i;
	register struct tbuf *tp;
	register struct ubuf *up;

	if (ibuf.fill1||ibuf.fill2)
		return;
	if(ibuf.tty == '|') {
		dtime = ibuf.time;
		return;
	}
	if(ibuf.tty == '}') {
		if(dtime == 0)
			return;
		for(tp = tbuf; tp < &tbuf[TSIZE]; tp++)
			tp->ttime =+ ibuf.time-dtime;
		dtime = 0;
		return;
	}
	if (lastime>ibuf.time || lastime+day1<ibuf.time)
		midnight = 0;
	if (midnight==0)
		newday();
	lastime = ibuf.time;
	if (byday && ibuf.time > midnight) {
		upall(1);
		print();
		newday();
		for (up=ubuf; up < &ubuf[USIZE]; up++)
			up->utime = 0;
	}
	if (ibuf.tty == '~') {
		ibuf.name[0] = '\0';
		upall(0);
		return;
	}
	if ((i = ibuf.tty) >= 'A' && i <= 'Z')
		i =+ 'z' - 'A' + 1;
	if (i >= 'a')
		i =+ '9' - 'a' + 1;
	i =- '0';
	if (i<0 || i>=TSIZE)
		i = TSIZE-1;
	tp = &tbuf[i];
	update(tp, 0);
}

print()
{
	int i;
	long ttime, t;

	ttime = 0;
	for (i=0; i<USIZE; i++) {
		if(!among(i))
			continue;
		t = ubuf[i].utime;
		if (t > 0) {
			ttime =+ t;
			if (pflag) {
				printf("\t%-8.8s%s\n", ubuf[i].name,
				    ptime(&ubuf[i].utime));
			}
		}
	}
	if (ttime > 0) {
		pdate();
		printf("\ttotal   %s\n\n", ptime(&ttime));
	}
}

upall(f)
{
	register struct tbuf *tp;

	for (tp=tbuf; tp < &tbuf[TSIZE]; tp++)
		update(tp, f);
}

char ttys[] "0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ?";

update(tp, f)
struct tbuf *tp;
{
	int i, j;
	struct ubuf *up;
	long t, t1;

	if (f)
		t = midnight;
	else
		t = ibuf.time;
	if (tp->userp) {
		t1 = t - tp->ttime;
		if (t1>0 && t1 < day1)
			tp->userp->utime =+ t1;
	}
	tp->ttime = t;
	if (f)
		return;
/*
if(tp->userp)
printf("logoff tty%c %-8.8s used %s\n", ttys[tp-tbuf], tp->userp->name,
    ptime(&t1));
*/
	if (ibuf.name[0]=='\0') {
		tp->userp = 0;
		return;
	}
	for (up=ubuf; up < &ubuf[USIZE]; up++) {
		if (up->name[0] == '\0')
			break;
		for (j=0; j<8 && up->name[j]==ibuf.name[j]; j++);
		if (j>=8)
			break;
	}
	for (j=0; j<8; j++)
		up->name[j] = ibuf.name[j];
	tp->userp = up;
/*
printf("login  tty%c %-8.8s at %s", ttys[tp-tbuf], ibuf.name,
    ctime(&ibuf.time));
*/
}

among(i)
{
	register j, k;
	register char *p;

	if (pcount==0)
		return(1);
	for (j=0; j<pcount; j++) {
		p = pptr[j];
		for (k=0; k<8; k++) {
			if (*p == ubuf[i].name[k]) {
				if (*p++ == '\0')
					return(1);
			} else
				break;
		}
	}
	return(0);
}

newday()
{
	register int *p;

	if(midnight == 0) {		/* Re-calculate midnight? */
		p = localtime(&ibuf.time);	/* Get current offsets */
		midnight = ibuf.time;	/* Starting point */
		midnight =- *p++;	/* Remove any seconds */
		midnight =- *p++*60;	/* Remove any minutes */
		while(--*p >= 0)	/* Remove any hours */
			midnight =- 60*60;
	}
	while (midnight <= ibuf.time)
		midnight =+ day;
}

char ans[] "0000.00";

ptime(val)
long *val;
{
	register int i, t, *p;

	*val =+ 18;	/* round off to nearest tick */
	t = (*val%3600)/36;
	ans[6] = t%10 + '0';
	ans[5] = t/10 + '0';
	t = *val/3600;
	for(i = 3; i >= 0; i--) {
		ans[i] = t%10 + '0';
		t =/ 10;
	}
	for(i = 0; i < 4 && ans[i] == '0'; i++)
		ans[i] = ' ';
	return(ans);
}

pdate()
{
	long priorday;

	if (byday==0)
		return;
	priorday = midnight - 1;
	printf("%-6.6s", ctime(&priorday)+4);
}
