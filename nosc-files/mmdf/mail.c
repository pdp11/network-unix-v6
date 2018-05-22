#define EVER (;;)
#define TRUE 1
#define FALSE 0

/*  Upgrade to use Submit; 10 Nov. 1978; David H. Crocker       */
/*    1 Dec 78/dhc: add mail-reading, approx like old "mail"    */
/*   15 Dec 78/dhc: add sndmsg & memo & "-" interfaces          */
/*   16 Dec 78/dhc: "-" not generate return address             */

char msgend[] {"\001\001\001\001\n"};
char linetrm[] {"\n\377"};
char locname[] {"UDEE"};
struct iobuf
{   int fildes;
    int nunused;
    char *xfree;
    char buff[512];
};
struct iobuf pipebuf;

long int ff_pos();
long int curnewpos;
int fout;
int childid;
int pipefds[2];
int newfd;
char pwbuf[256];
char *logdir;
char linebuf[256];
char *sbmtargs;
/*char more;    */
char qrychar;
char usertype;

/* **************************  MAIN  ******************************* */

main (argc, argv)
int     argc;
char   *argv[];
{
    fout = dup (1);
    pgminit (argc, argv);

    if (equal (argv[0], "snd", 3) || index ("/snd", argv[0]) != -1)
	usertype = 1;
    else
	if (strequ ("memo", argv[0]) || index ("/memo", argv[0]) != -1)
	    usertype = 2;
	else
	    if (strequ (argv[0], "-"))
	    {                       /* "-" => login shell   */
		usertype = 3;
		prompt ("Send a memo\n");
	    }
    if ((argc == 1 && usertype == 0) ||
	(argc == 2 && *argv[1] == '-'))
	readmail (argc, argv);
    else
	sendmail (argc, argv);
}

pgminit ()
{
    register char *ptr;

    if (getpw (getuid () & 0377, &pwbuf) < 0)
	abort ("Unable to locate user's name.\n");

    for (ptr = &pwbuf; *ptr != ':'; ptr++);
    *ptr++ = '\0';           /* &pwbuf = username    */
    while (*ptr++ != ':');   /* passwd                             */
    while (*ptr++ != ':');   /* user id                            */
    while (*ptr++ != ':');   /* group id                           */
    while (*ptr++ != ':');   /* gcos crud                          */
    for (logdir = ptr, *ptr = '\0'; *ptr != ':'; ptr--);
			     /* initial working directory          */
}

readmail (argc, argv)
    int argc;
    char *argv[];
{
    if (argc == 2)
	qrychar = argv[1][1];

    readinit ();
    while (msgstrt () && showmsg () && savmsg ());
    endread ();
}

readinit ()
{
    if (chdir (logdir) == -1)
	abort ("Unable to go to login directory.\n");

    if ((newfd = ff_open (".mail", 4)) == -1)
	abort ("No new mail.\n");
}

msgstrt ()
{
    register int c;

    for EVER
    {
	curnewpos = ff_pos (newfd);
	if ((c = ff_read (newfd, &linebuf, sizeof linebuf, linetrm)) <= 0)
	    return (FALSE);
	if (!strequ (&linebuf, msgend))
	    break;
     }
     ff_seek (newfd, curnewpos);
     return (TRUE);
}

showmsg()
{
    register int lines;
    register int c;

    for (lines = 0;
	    (c = ff_read (newfd, &linebuf, sizeof linebuf - 1, linetrm)) > 0;)
    {
	linebuf[c] = '\0';
	if (strequ (linebuf, msgend))
	    return (TRUE);
	printf ("%s", &linebuf);
	if (lines++ > 4)
	{
	    flush ();
	    lines = 0;
	}
    }
    return ((c == -1) ? FALSE : TRUE);
}

savmsg ()
{
    static struct iobuf mboxbuf;
    register int c;

    if (!querysav ())
	return (TRUE);

    if (mboxbuf.fildes == 0)
    {
	if ((mboxbuf.fildes = open ("mbox", 1)) == -1 &&
	     (mboxbuf.fildes = creat ("mbox", 0600)) == -1 )
	    abort ("Unable to access/create mbox.\n");
	seek (mboxbuf.fildes, 0, 2);
    }

    for (ff_seek (newfd, curnewpos);
	    (c = ff_read (newfd, &linebuf, sizeof linebuf - 1, linetrm)) > 0;
	    puts (&linebuf, &mboxbuf))
    {
	linebuf[c] = '\0';
	if (strequ (linebuf, msgend))
	    break;
    }
    puts (msgend, &mboxbuf);
    fflush (&mboxbuf);
    return ((c == -1) ? FALSE : TRUE);
}

querysav ()
{
    register char respchar;

    if (qrychar == 0)
    {
	prompt ("\nSave? ");
	if ((respchar =  getchar()) != '\n')
	    while (getchar () != '\n');
    }
    else
	respchar = qrychar;
    if (respchar == 'y' || respchar == 'Y')
	return (1);
    return (0);
}

endread ()
{
    flush ();
    unlink (".mail");
    exit (1);
}
/* *************************  SENDING ****************************** */

sendmail (argc, argv)                  /* Send a message               */
    int argc;
    char *argv[];
{
    extern int     regfdary[];
    char hadfrom;
    register int    i;
    register int pipeout;

    hadfrom = FALSE;

    doargs (argc, argv);        /* any args to be args to submit?       */
    if (pipe(&pipefds) == -1)
	abort ("Unable to create i/o pipes.\n");
    regfdary[0] = pipefds[0];
    pipebuf.fildes = pipeout = pipefds[1];
				  /* parent writing channel             */
#define FRKEXEC 1                 /* run it in lower process            */
#define FRKCIGO 4                 /* Childs signals off before execl    */
    if ((childid = newpgml (FRKEXEC, 0 /*FRKCIGO*/, regfdary,
			"/rand_ms/xsubmit", "Submit",
			(usertype == 3) ? "-lmuxto,cc*" : "-lmrxto,cc*",
			sbmtargs, 0))
				       /* mail, rtn2sndr, snd local,
					  xtract address from to,cc */
	    == -1)
	abort ("Unable to submit mail; please report this error.\n");
    close (pipefds[0]);

    if (usertype == 3)
	putc ('\n', &pipebuf);  /* indicate no return address   */
    dodate ();
    hadfrom = dofrom (argc, argv);
    dosubject (argc, argv);
    if (hadfrom && usertype < 3)
	dosender ("Sender:   ");
    if (!doto (argc, argv) && !docc (argc, argv) && usertype == 0)
	abort ("No addressees specified.\n");
    doid (argc, argv);
    if (usertype > 0)
    {
	doprompt ();            /* get headers          */
	prompt ("Text:\n");
    }
    putc ('\n', &pipebuf);      /* separate body from headers   */
    fflush (&pipebuf);
    dobody ();
    exit (endchild ());
}
/*  *************  ARGUMENT PARSING FOR SENDMAIL  **************  */

doargs (argc, argv)             /* any args to be args to submit?   */
    int argc;
    char *argv[];
{
    register int i;

    for (i = 1; i < argc; i++)
    {                           /* create message header fields */
	if (*argv[i] == '-')
	    switch (argv[i][1])
	    {
	     case '-':
		sbmtargs = &(argv[i][1]);
		continue;
	    }
    }
}

dodate ()
{
    char datbuf[64];

    cnvtdate (3, &datbuf);      /* rfc733 format date   */
    puts ("Date:     ", &pipebuf);
    puts (&datbuf, &pipebuf);
    putc ('\n', &pipebuf);
}

dofrom (argc, argv)
    int argc;
    char *argv[];
{
    char havefrom;
    register int i;

    for (i = 1, havefrom = FALSE; i < argc; i++)
    {                           /* create message header fields */
	if (argv[i][0] == '-' && argv[i][1] == 'f')
	{
	    havefrom = TRUE;
	    puts ("From:     ", &pipebuf);
	    puts (argv[++i], &pipebuf);
	    putc ('\n', &pipebuf);
	    continue;
	}
    }
    if (havefrom)
	return (TRUE);
    if (usertype < 3)   /* not memo     */
	dosender ("From:     ");
    return (FALSE);
}

doid (argc, argv)
    int argc;
    char *argv[];
{
    char datbuf[64];
    register int i;

    for (i = 1; i < argc; i++)
    {                           /* create message header fields */
	if (argv[i][0] == '-' && argv[i][1] == 'i')
	{
	    cnvtdate (1, &datbuf);      /* "Julian"-style date  */
	    puts ("Message-id:   <", &pipebuf);
	    puts (&datbuf, &pipebuf);
	    puts (" @ ", &pipebuf);
	    puts (locname, &pipebuf);
	    puts (">\n", &pipebuf);
	    return;
	}
    }
}

dosubject (argc, argv)
    int argc;
    char *argv[];
{
    register int i;

    for (i = 1; i < argc; i++)
    {                           /* create message header fields */
	if (argv[i][0] == '-' && argv[i][1] == 's')
	{
	    puts ("Subject:  ", &pipebuf);
	    puts (argv[++i], &pipebuf);
	    putc ('\n', &pipebuf);
	    continue;
	}
    }
}

dosender (cmpnt)
    char *cmpnt;
{
    puts (cmpnt, &pipebuf);
    putc (lowtoup (pwbuf[0]), &pipebuf);
    puts (&pwbuf[1], &pipebuf);
    puts (" at ", &pipebuf);
    puts (locname, &pipebuf);
    putc ('\n', &pipebuf);
}

doto (argc, argv)
    int argc;
    char *argv[];
{
    char someto;
    register int i;

    someto = FALSE;
    if (usertype == 0 && argv[1][0] != '-')
    {
	someto = TRUE;
	puts ("To:       ", &pipebuf);
	i = dolist (1, argc, argv);
    }
    else
	i = 1;

    for (; i < argc; i++)
    {                           /* create message header fields */
	if (argv[i][0] == '-' && argv[i][1] == 't')
	{
	    someto = TRUE;
	    puts ("To:       ", &pipebuf);
	    i = dolist (i + 1, argc, argv);
	}
    }
    return (someto ? TRUE : FALSE);
}

docc (argc, argv)
    int argc;
    char *argv[];
{
    char somecc;
    register int i;

    for (somecc = FALSE, i = 1; i < argc; i++)
    {                           /* create message header fields */
	if (argv[i][0] == '-' && argv[i][1] == 'c')
	{
	    somecc = TRUE;
	    puts ("cc:       ", &pipebuf);
	    i = dolist (i + 1, argc, argv);
	    continue;
	}
    }
    return (somecc ? TRUE : FALSE);
}

dolist (ind, argc, argv)
    int ind,
	argc;
    char *argv[];
{
    register int i;

    i = ind;
    if (i < argc && *argv[i] != '-')
	for (puts (argv[i++], &pipebuf); i < argc && *argv[i] != '-';
		puts (", ", &pipebuf), puts (argv[i++], &pipebuf));
    putc ('\n', &pipebuf);
    return (i - 1);
}

doprompt ()
{
     if (usertype == 3)        /* "free" memo          */
	cpyprompt ("From:  ", "From:     ");
     cpyprompt ("To:  ", "To:       ");
     if (usertype == 1)         /* sndmsg               */
	cpyprompt ("cc:  ", "cc:       ");
     cpyprompt ("Subject:  ", "Subject:  ");
}

prompt (str)
    char *str;
{
    printf ("%s", str);
    flush ();
}

cpyprompt (prom, prelim)
    char *prom, *prelim;
{
    prompt (prom);
    copylin (prelim);
}

copylin (prelim)
     char *prelim;
{
     struct iobuf inbuf;
     register int morin;
     register int c;

     for (zero (&inbuf, sizeof inbuf),
	    morin = TRUE; morin;
	    puts (prelim, &pipebuf), prelim = "     ",
		fwrite (&pipebuf, &linebuf, c))
     {
	morin = FALSE;
	switch (c = gcread (&inbuf, &linebuf, sizeof linebuf, linetrm))
	{
	 case -1:
	 case 0:
	    abort ("Input error.\n");
	 case 1:
	    return;
	 default:
	    if (linebuf[c - 2] == '\\')
	    {
		morin = TRUE;
		linebuf[c-- - 2] = '\n';
	    }
	}
     }
}

dobody ()
{
    register char *bufptr;
    register int i;
    register int pipeout;

    for (bufptr = &(pipebuf.buff), pipeout = pipebuf.fildes;
	(i = read (0, bufptr, 512)) > 0; write (pipeout, bufptr, i));
				/* copy body                       */
}

/* *********************  UTILITIES  ***************************  */

abort (msg)
    char *msg;
{
    prompt (msg);
    if (childid)
	endchild ();
    exit (-1);
}

endchild ()
{
    register int i;
    int retval;

    close (pipefds[1]);         /* => eof for submit()              */
    while ((i = wait (&retval)) != childid && i != -1);
    return (retval >> 8);
}

#ifdef NVRCOMPIL
puts (str, bufptr)
    char *str;
    struct iobuf *bufptr;
{
    int len;

    len = strlen (str);
    printf ("%s", str);
    flush ();
    fwrite (bufptr, str, len);
}
#endif

