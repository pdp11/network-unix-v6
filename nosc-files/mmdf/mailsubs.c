#include "mailer.h"

int fout;
char logfile[];
struct iobuf logbuf;

initlog (handle)
    int handle;
{
    long ltime;
    int tmp;
    int *tpt;

    if (handle <= 0)
	return (FALSE);
    zero (&logbuf, sizeof logbuf);
    flush ();
    tmp = fout;
    time (&ltime);
    tpt = localtime (&ltime);
    logbuf.fildes = handle;
    seek (handle, 0, 2);
    fout = handle;
    printf ("\n*** %2d/%2d %2d:%2d:%2d - LOG OPENED ***\n",
		tpt[4] + 1, tpt[3], tpt[2], tpt[1], tpt[0]);
    flush ();
    fout = tmp;
    return (TRUE);
}

getlogfd ()
{
    return (logbuf.fildes);
}
clslog ()
{
    if (logbuf.fildes > 0)
    {
	fflush (&logbuf);
	close (logbuf.fildes);
	logbuf.fildes = 0;
    }
}

log (format, a1, a2, a3, a4, a5, a6, a7)
    char *format, *a1, *a2, *a3, *a4, *a5, *a6, *a7;
{
    extern int *localtime();
    long ltime;
    int tmp;
    int *tpt;
    char numstr[6];
    char logherald;
    register int *ap;
    register char   c, *fmt;

    time (&ltime);
    tpt = localtime (&ltime);

    logherald = FALSE;
    switch (logbuf.fildes)
    {case NOTOK:
	return (FALSE);
     case 0:
	if (logfile[0] == '\0')
	{
	    logbuf.fildes = NOTOK;
	    return (FALSE);
	}

	if  ((tmp = open (&logfile, 1)) <= OK)
	{
	    logbuf.fildes = NOTOK;
	    if (logfile[0] != '\0')
		printx ("Unable to open log file; continuing...\n");
	    return (FALSE);
	}
	initlog (tmp);
    }

    if (*(fmt = format) == 0)
    {
	fflush (&logbuf);
	return;
    }

    flush ();
    tmp = fout;
    fout = logbuf.fildes;
    printf ("%2d:%2d ", tpt[1], tpt[0]);
    printf (format, a1, a2, a3, a4, a5, a6, a7);
    putchar ('\n');
    flush ();
    fout = tmp;

#ifdef NVRCOMPIL
    for (;;)
    {
	for (ap = &a1; (c = *fmt++) != '%'; putc (c, &logbuf))
	{
	    if (c == '\0')
	    {
/* * */         fflush (&logbuf);
		return;
	    }
/*            if (c == '\\')    */
/*                c = *fmt++;   */
	}
	for (tmp = 0; digit (c = *fmt++); tmp =* 10, tmp =+ c - '0');
	switch (c)
	{
	 case 'c':
	    putc (*ap++, &logbuf);
	    continue;
	 case 'd':
	    itoa (*ap++, &numstr);
	    for (tmp = tmp - strlen (numstr); tmp-- > 0;
		putc (' ', &logbuf));
	    puts (numstr, &logbuf);
	    continue;
	 case 's':
	    puts (*ap++, &logbuf);
	    continue;
	}
    }
#endif
}
char dbgmode;

dbglog (a, b, c, d, e, f)
char   *a,
       *b,
       *c,
       *d,
       *e,
       *f;
{
    if (dbgmode)
	log (a, b, c, d, e, f);
}

perror (a, b, c, d, e, f)
char   *a,
       *b,
       *c,
       *d,
       *e,
       *f;
{
    extern int  sys_nerr;
    extern char *sys_errlist[];
    extern int  errno;

    log (a, b, c, d, e, f);

    log ("\t[ (%d) %s]", errno, errno >= 0 && errno <= sys_nerr
				    ?    sys_errlist[errno]
				    :    " <- Illegal errno");
}

int domsg;

printx (a, b, c, d,e,f)
char    a[],
	b[],
	c[],
	d[];
char    e[],
	f[];
{
    if (domsg)
	printf (a, b, c, d,e,f);
    flush ();                     /* DBG */
}

int savenv;

timeout ()
{
    signal (SIGCLK, &timeout);
    log ("Timeout\n");
    envrest (&savenv);
}

sig1 ()
{
    perror ("Dying on signal 1");
    abort ();
}
sig2 ()
{
    perror ("Dying on signal 2");
    abort ();
}
sig4 ()
{
    perror ("Dying on signal 4");
    abort ();
}
sig10 ()
{
    perror ("Dying on signal 10");
    abort ();
}
sig12 ()
{
    perror ("Dying on signal 12");
    abort ();
}
sig13 ()
{
    perror ("Dying on signal 13");
    abort ();
}

siginit ()
{
    signal (1, &sig1);
    signal (2, &sig2);
    signal (4, &sig4);
    signal (10, &sig10);
    signal (12, &sig12);
    signal (13, &sig13);
}
