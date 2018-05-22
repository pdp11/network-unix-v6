#include "mh.h";
#include "iobuf.h"
#include "stat.h"

#define DEBUG 1			  /* Comment out normally */
#define TIMJUL  1
#define TIMSECS 2
#define TIMREG  3

/* Include a -msgid switch to .mh_defs to cause a
 * Message-Id component to be added to outgoing mail.
 */

char   *anoyes[],		  /* Std no/yes gans array        */
        draft[],
	locname[],		/* local host name */
	sbmitloc[],		/* location (pathname) of submit process */
	sbmitnam[];		/* name of submit process */
struct swit switches[]
{
    "debug", -1,		  /* 0 */
    "draft", 0,			  /* 1 */
    "format", 0,		  /* 2 */
    "noformat", 0,		  /* 3 */
    "msgid", 0,			  /* 4 */
    "nomsgid", 0,		  /* 5 */
    "verbose", 1,		  /* 6 *//* DBG dhc */
    "noverbose", 0,		  /* 7 */
    "help", 4,			  /* 8 */
    0, 0
};
struct iobuf    in,
                pin,
                pout,
                fout,
                fccout;
int     errno;
int     format 0;		  /* re-formatting is the default */
int     sbmitfd[2];
int     sbchild;

char   *logn,
       *parptr,
       *nmsg,
        fccfile[256],
        sberrline[256];

main (argc, argv)
char   *argv[];
{
    int     debug;
    register char  *cp;
    register int    state;
    char   *to,
           *cc,
           *bcc,
           *dist_to,
           *dist_cc,
           *dist_bcc;
    char   *tolist,
           *cclist,
           *adrlist,
           *sender,
	*fieldptr,
           *fcc,
           *dist_fcc;
    char    msg[128];
    char    name[NAMESZ],
            field[512];
    int     specfld,
            dist,
            fcconly;
    int     from,
            loc,
            net,
            verbose,
            msgid;
    char   *ap,
	inp;
    char   *arguments[50],
          **argp;

    fout.b_fildes = dup (1);
    msgid = from = loc = net = to = cc = bcc = dist = fcc = dist_fcc =
	dist_to = dist_cc = dist_bcc = fcconly = debug = 0;

    loc = net = 1;  /* DBG during testing of new submit */

    state = FLD;
    msg[0] = 0;
	ap = cp = argv[0];
	while(*cp)
		if(*cp++ == '/')
			ap = cp;
	inp = ap;
    copyip (argv + 1, arguments);
    argp = arguments;
    while (cp = *argp++)
    {
	if (*cp == '-')
	    switch (smatch (++cp, switches))
	    {
		case -2: 
		    ambigsw (cp, switches);
				  /* ambiguous */
		    leave ("Illegal switch setting: \"%s\"", --cp);
				  /* unknown */
		case -1: 
		    leave ("-%s unknown", cp);
		case 0: 
		    verbose++;
		    debug++;
		    continue;     /* -debug */
		case 1: 
		    copy (m_maildir (draft), msg);
		    continue;     /* -draft  */
		case 2: 
/* disabled for now format = 1;                         */
		    continue;     /* -format */
		case 3: 
		    format = 0;
		    continue;     /* -noformat */
		case 4: 
		    msgid = 1;
		    continue;     /* -msgid */
		case 5: 
		    msgid = 0;
		    continue;     /* -nomsgid */
		case 6: 
		    loc = 1;
		    net = 1;
		    continue;     /* -verbose */
		case 7: 
		    loc = 0;
		    net = 0;
		    continue;     /* -noverbose */
		case 8: 
  		  help (concat( inp, " [file] [switches]", 0),
			    switches);
		    leave (0);
	    }
	if (msg[0])
	    leave ("Only one message at a time!");
	else
	    copy (cp, msg);
    }
    if (msg[0] == 0)
    {
	copy (m_maildir (draft), msg);
	if (stat (msg, &field) == -1)
	{
	    printf ("Draft file: %s doesn't exist.\n", msg);
	    flush ();
	    exit (1);
	}
	/* cp = concat ("Use \"", msg, "\"? ", 0);
	if (!gans (cp, anoyes))
	    exit (0); */
    }
    if (fopen (msg, &in) < 0)
	leave ("Can't open \"%s\" for reading.", msg);

    state = FLD;

    if (!loc && !net)
        printf ("Message being processed...\n");
    flush ();
    for (;;)
	switch (state = m_getfld (state, name, field, sizeof field, &in))
	{

	    case FLD: 
	    case FLDEOF: 
	    case FLDPLUS: 
		if (!dist && field[0] != 'n' &&  uleq (name, "to"))
		    to = adrlist = add (field, adrlist);
		else
		    if (!dist && field[0] != 'n' && uleq (name, "cc"))
			cc = adrlist = add (field, adrlist);
		    else
			if (!dist && uleq (name, "bcc"))
			    adrlist = add (field, adrlist);
			else
			    if (!dist && uleq (name, "fcc"))
				fcc = add (field, fcc);
			    else
				if (field[0] != 'n' && uleq (name, "distribute-to"))
				{
				    dist++;
				  /* use presence of field as flag */
				    to = adrlist = add (field, adrlist);
				}
				else
				    if (field[0] != 'n' && uleq (name, "distribute-cc"))
					cc = adrlist = add (field, adrlist);
				    else
					if (field[0] != 'n' && uleq (name, "distribute-bcc"))
					    adrlist = add (field, adrlist);
					else
					    if (uleq (name, "distribute-fcc"))
						dist_fcc = add (field, dist_fcc);
					    else
						if (uleq (name, "from"))
						    from++;

		if (state != FLDEOF)
		    continue;
	    case BODY: 
	    case BODYEOF: 
		goto done;

	    default: 
		leave ("getfld returned %d", state);
	}
done: 
    if (dist)
    {
/*      to = dist_to;                                                   */
/*      cc = dist_cc;                                                   */
/*      bcc = dist_bcc;                                                 */
	fcc = dist_fcc;
    }
    if (!(to || cc))
	if (!fcc)
	    leave ("Message %s has no addresses!!", msg);
	else
	    fcconly++;
     /****/ pin.b_fildes = dup (2);
/**** pout.b_fildes = 3;*/

/*  if ((to && parse (to, 'c')) ||                                      */
/*      (cc && parse (cc, 'c')) ||                                      */
/*      (bcc && parse (bcc, 'c'))                                       */
/*          leavit ();                                                  */

/*  parptr = adrlist;                                                   */
/*  compress ();                                                        */
/*  adrlist = parptr;                                                   */
    parptr = 0;
    if (verbose)
    {
	printf ("Address List:\n%s", adrlist);
	flush ();
    }

/*  if (to && parse (to, 'r', dist ? "Distribute-To: " : "To: "))       */
/*          leave (0);                                                  */
/*  tolist = parptr;                                                    */
/*  parptr = 0;                                                         */
/*  if (cc && parse (cc, 'r', dist ? "Distribute-cc: " : "cc: "))       */
/*          leave (0);                                                  */
/*  cclist = parptr;                                                    */
/*  parptr = 0;                                                         */
/*                                                                      */
/*  if (verbose)                                                        */
/*  {                                                                   */
/*      if (tolist)                                                     */
/*          printf (tolist);                                            */
/*      if (cclist)                                                     */
/*          printf (cclist);                                            */
/*      flush ();                                                       */
/*  }                                                                   */
    if (fcc)
    {
	if (*fcc == ' ')
	    fcc++;
	for (cp = fcc; *cp && *cp != '\n'; cp++);
	*cp = 0;
	if (debug)
	    printf ("fcc: \"%s\"\n", fcc);
	flush ();
	if ((fccout.b_fildes = filemsg (fcc)) == -1)
	    leave ("Problem beginning fcc message transfer");
    }

/*  if (parse (field, 'r', dist ? "Distributed-By: "                    */
/*               : (from ? "Sender: " : "From: ")))                     */
/*      leave (0);                                                      */

    sender = add ((dist) ? "Distributed-By: "
	    : (from) ? "Sender: "
	    : "From: ", sender);
fieldptr = copy ((logn = getlogn (getruid ())), field);
fieldptr = copy (" at ", fieldptr);
fieldptr = copy ( locname, fieldptr);
copy ( "\n", fieldptr);
    sender = add (field, sender);
/*    parptr = 0;                                                       */
    seek (in.b_fildes, 0, 0);
/* ***   INVOKE SUBMIT, AT LAST   ***  */
    ap = arguments;
    *ap++ = '-';		  /* indicate a switch sequence               
				  */
    /* *ap++ = 'd';	/*DBG have submit output messages */
    *ap++ = 'm';		  /* mail, not screen-notice, yet             
				  */
/*  cputc ('s', &pout); */
/* ******* During initial testing, mail always sent immediately, ****** */
/*         so that users can verify delivery...                         */
/*         Can't trust new software these days...                ****** */

/*  if (loc)                    */
	*ap++ = 'l';              /* send local mail now        */
    if (net)
	*ap++ = 'n';              /* send net mail now          */
    if (net || loc)
	*ap++ = 'w';              /* user will watch deliveries */

/* *******  * * * * * * * * * * * * * * * * * * * * * * * * * *  ****** */

    *ap++ = 'r';		  /* return messages go to sender             
				  */
    *ap = '\0';

    if (debug)
	pout.b_fildes = 1;	  /* Send msg to std output for debugging */
    else
	if (fcconly)
	    pout.b_fildes = 0;    /* Flush send output if only an fcc */
	else
	{
	    if ((sbchild = sbmitinit (arguments, sbmitfd)) == -1)
		leave ("Unable to invoke submit");
	    pout.b_fildes = sbmitfd[1];
	}

    puts (adrlist, &pout);
    puts ("!\n", &pout);	  /* end the address list                 */
    puts2 (sender, &pout);
    puts2 (dist ? "Distribution-Date: " : "Date: ", &pout);
    puts2 (cnvtdate (TIMREG), &pout);
    puts2 ("\n", &pout);
    if (msgid)
    {
	puts2 (dist ? "Distribution-ID: " : "Message-ID: ", &pout);
	puts2 ("<", &pout);
	puts2 (cnvtdate (TIMJUL), &pout);
	puts2 (".", &pout);
	puts2 (locv (0, getpid ()), &pout);
	puts2 (" at ", &pout);
	puts2 (locname, &pout);
	puts2 ("\n", &pout);
    }
    seek (in.b_fildes, 0, 0);
    in.b_nleft = in.b_nextp = 0;
    state = FLD;
    for (;;)
	switch (state = m_getfld (state, name, field, sizeof field, &in))
	{
	    case FLD: 
	    case FLDEOF: 
	    case FLDPLUS: 
		specfld = 0;
		if (format && uleq (name, dist ? "distribute-to" : "to"))
		{
		    specfld++;
		    if (tolist)
			puts2 (tolist, &pout);
		}
		else
		    if (format && uleq (name, dist ? "distribute-cc" : "cc"))
		    {
			specfld++;
			if (cclist)
			    puts2 (cclist, &pout);
		    }
		    else
			if (uleq (name, dist ? "distribute-bcc" : "bcc") ||
				uleq (name, dist ? "distributed-by" : "sender") ||
				uleq (name, dist ? "distribution-date" : "date") ||
				uleq (name, dist ? "distribution-id" : "message-id") ||
				uleq (name, dist ? "distribution-fcc" : "fcc"))
			    specfld++;
				  /* Ignore these if present */
			else
			{
			    puts2 (name, &pout);
			    puts2 (":", &pout);
			    puts2 (field, &pout);
			}
		while (state == FLDPLUS)
		{		  /* read rest of field */
		    state = m_getfld (state, name, field, sizeof field, &in);
		    if (specfld)
			continue;
/*                  puts2(name, &pout); puts2(":", &pout);  */
		    puts2 (field, &pout);
		}
		if (state == FLDEOF)
		    goto endit;
		continue;
	    case BODY: 
	    case BODYEOF: 
		if (field[0])
		{
		    puts2 ("\n", &pout);
		    puts2 (field, &pout);
		}

		while (state == BODY)
		{
		    state = m_getfld (state, name, field, sizeof field, &in);
		    puts2 (field, &pout);
		}
		if (state == BODYEOF)
		    goto endit;
	    default: 
		leave ("Error from getfld=%d", state);
	}

endit: 
    errno = 0;
    if (pout.b_fildes)
	if (fflush (&pout) <  0 || errno != 0)
	    leave ("Problem writing data to Submit.");
    if (fccout.b_fildes)
    {
	fflush (&fccout);
	close (fccout.b_fildes);
    }
    if (!debug && !fcconly)
    {
/*      if ((state = read (2, &field, sizeof field)) != 1 || field[0])  */
	if (loc || net)
	{
	    printf ("Delivery being attempted...\n");
	    flush ();
	}
	if (sbresp (&sbmitfd) != 0)
	    leave ("Mail submission program ended abnormally");
    }
    if (fccout.b_fildes)
	printf ("Filed: %s:%s\n", fcc, nmsg);
    if (!debug)
    {
	if (!fcconly)
	{
	    printf ("Message processed.\n");
/*	    printf ("Message %s processed.\n", msg);*/
	    flush ();
	}
	cp = copy (msg, field);
				  /*  for(cp = field; *cp++; ) ;  */
	cp[1] = 0;
	do
	    *cp = cp[-1];
	while (--cp >= field && *cp != '/');
	*++cp = ',';		  /* New backup convention */
	unlink (field);
	if (link (msg, field) == -1 || unlink (msg) == -1)
	    printf ("Can't rename %s to %s\n", msg, field);
    }
    m_update ();
/*    childend ();  */
    flush ();
    exit (0);
}

leave (form, a1, a2, a3)
char   *form,
       *a1,
       *a2,
       *a3;
{
    if (form != 0)
    {
	printf (form, a1, a2, a3);
        putchar ('\n');
    }
if (sbchild != 0)
    kill (sbchild, 9);
    printf ("[ Message NOT Delivered! ]\n");
    if (fccout.b_fildes)
	unlink (fccfile);
    m_update ();
    flush ();
    exit (1);
}

parse (ptr, type, fldname)
char   *fldname;
{
    register int    i;
    register int    l;
    char    line[128];

    errno = 0;
    putc (type, &pout);
    puts ("\n", &pout);
    puts (ptr, &pout);
    if (fflush (&pout) < 0 || errno != 0)
	leave ("Problem sending data to Submit.");
    write (pout.b_fildes, "", 1);
    l = 0;

    while ((i = getl (&pin, line, (sizeof line) - 1)) > 0)
    {
	line[i] = 0;
	if (line[0] == 0)
	{
	    if (type == 'r')
	    {
		if (l > 0)
		    parptr = add ("\n", parptr);
	    }
	    return (0);
	}
	else
	    if (line[0] == '?')
	    {
		printf ("Submit returned: %s", line);
		return (1);
	    }
	if (type == 'r')
	{
	    line[--i] = 0;
	    if (l + i > 70)
	    {
		parptr = add ("\n", parptr);
		l = 0;
	    }
	    if (l == 0)
	    {
		parptr = add (fldname, parptr);
		l =+ length (fldname);
	    }
	    else
	    {
		parptr = add (", ", parptr);
		l =+ 2;
	    }
	    parptr = add (line + 1, parptr);
	    l =+ i;
	}
	else
	    parptr = add (line, parptr);
    }
    printf ("Error from Submit.\n");
    return (1);
}

cnvtdate (flag)
int     flag;			  /* date format option value     */
{
    static char datbuf[128];
    extern int  daylight;
    int     tvec[2];
    long int    seconds;
    register int
                   *i;
    register char
                   *p,
                   *t;

    time (tvec);
    i = localtime (tvec);
    t = ctime (tvec);
    p = datbuf;

    switch (flag)
    {
	case TIMJUL: 		  /* Julian-oriented for msg-ids  */
	    for (itoa (i[5], p); *p; p++);
	    *p++ = '.';
	    for (itoa (i[7], p); *p; p++);
	    *p++ = '.';
	    seconds = i[2];
	    seconds = i[0] + (i[1] * 60) + (seconds * 3600);
	    litoa (seconds, p);   /* seconds since midnight     */
	    break;
	case TIMREG: 		  /* RFC 733 standard time string */
	default: 		  /* "Sat, 21 Jan 76 14:30-PDT"   */
	    *p++ = t[0];
	    *p++ = t[1];
	    *p++ = t[2];
	    *p++ = ',';
	    *p++ = ' ';
	    *p++ = t[8];	  /* day of month                 */
	    *p++ = t[9];
	    *p++ = ' ';
	    *p++ = t[4];	  /* month abbreviation eg: "JAN" */
	    *p++ = t[5];
	    *p++ = t[6];
	    *p++ = ' ';
	    *p++ = t[22];	  /* year eg: "76"              */
	    *p++ = t[23];
	    *p++ = ' ';
	    *p++ = t[11];	  /* hours & minutes eg: "14:30" */
	    *p++ = t[12];
	    *p++ = ':';
	    *p++ = t[14];
	    *p++ = t[15];
	    *p++ = '-';
	    *p++ = 'P';		  /* time zone eg: "PDT"          */
	    *p++ = (i[8]) ? 'D' : 'S';
	    *p++ = 'T';
	    *p++ = '\000';
    }
    free (i);
    free (t);
    return (datbuf);
}

getl (iob, buf, size)
{
    register char  *cp;
    register int    cnt,
                    c;

    for (cp = buf, cnt = size; (c = getc (iob)) >= 0;)
    {
	*cp++ = c;
	--cnt;
	switch (c)
	{
	    case '\0': 
	    case '\n': 
		break;
	    default: 
		continue;
	}
	break;
    }
    return (size - cnt);
}

compress ()
{
    register char  *f1,
                   *f2,
                   *f3;

#ifdef DEBUG
    printf ("compress:\n%s-----\n", parptr);
#endif
    for (f1 = parptr; *f1;)
    {
	for (f2 = f1; *f2++ != '\n';);
	while (*f2)
	{
	    if (eqqq (f1, f2))
	    {
		for (f3 = f2; *f3++ != '\n';);
		copy (f3, f2);
	    }
	    else
		while (*f2++ != '\n');
	}
	while (*f1++ != '\n');
    }
}

eqqq (s1, s2)
{
    register char  *c1,
                   *c2;

    c1 = s1;
    c2 = s2;
    while (*c1 != '\n' && *c2 != '\n')
	if (*c1++ != *c2++)
	    return (0);
    return (*c1 == *c2);
}
filemsg (folder)
char   *folder;
{
    register int    i;
    register char  *fp;
    struct inode    stbuf;
    struct msgs *mp;

    fp = m_maildir (folder);
    if (stat (fp, &stbuf) < 0)
    {
/*	nmsg = concat ("Create folder \"",
		fp, "\"? ", 0);
	if (!gans (nmsg, anoyes))
	    return (-1); */
	if (!makedir (fp))
	{
	    printf ("Can't create folder.\n");
	    return (-1);
	}
    }
    if (chdir (fp) < 0)
    {
	perror (concat ("Can't chdir to ", fp, 0));
	return (-1);
    }
    if (!(mp = m_gmsg ()))
    {
	printf ("Can't read folder %s\n", folder);
	return (-1);
    }
    nmsg = m_name (mp -> hghmsg + 1);
    copy (nmsg, copy ("/", copy (m_maildir (fp), fccfile)));
    if ((i = creat (fccfile, m_gmprot ())) == -1)
	printf ("Can't create %s\n", fccfile);
    return (i);
}

puts2 (str, iob)
{
    puts (str, &fccout);
     errno = 0;
    puts (str, iob);
    if (fflush (iob) < 0 || errno != 0)
	leave ("Problem writing out data");
}
/* subroutines to interface to submit   */

#define CLOSEFD -1		  /* if equals fdarray[i], close (i);     */
#define PUREXEC 0		  /* simply exec over current process     */
#define FRKEXEC 1		  /* run it in lower process              */
#define SPNEXEC 2		  /* separate new process from old        */
#define FRKWAIT 1		  /* wait for FRKEXEC to complete         */
#define FRKPIGO 2		  /* Parent's signals off during FRKWAIT  */
#define FRKCIGO 4		  /* Childs signals off before execl      */
/*#define EXECR   8               ** Use execr, instead of execv          */
#define HIGHFD  16
#define NUMTRY  30

struct pstruct
{
    int     prd;
    int     pwrt;
};
int     highfd
{
    HIGHFD
};
int     regfdary[]
{
    0, 1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, 1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1
};

sbmitinit (args, ioary)
char   *args;
int    *ioary;
{

    struct pstruct  sbpout;
    int     sbmitid;
    char    linebuf[256];
    char   *p;

    if (pipe (&sbpout) == -1)
	return (-1);
    regfdary[0] = sbpout.prd;
    regfdary[1] = fout.b_fildes;

    if ((sbmitid = newpgml (FRKEXEC, 0, regfdary,
		    sbmitloc, sbmitnam, args, 0)) == -1)
    {
flush ();
	close (sbpout.prd);
	close (sbpout.pwrt);
	return (-1);
    }

dbglog ("Ready to close");
    close (sbpout.prd);
    ioary[1] = sbpout.pwrt;
    return (sbmitid);
}

#ifdef COMMNET
sbmitadr (name, ioary)
char   *name;
int    *ioary;
{
    int     tmp;
    char    linebuf[256];
    char   *from,
           *to;

    for (from = name, to = linebuf; *to = *from++; to++);
    for (from = "\n"; *to++ = *from++;);
    tmp = to - &linebuf;

    if (write (ioary[1], linebuf, tmp) == -1)
	return (-1);

    return (sbresp (ioary));
}
#endif

sbresp (ioary)
int    *ioary;
{
    return (childend ());
}

newpgml (proctyp, pgmflags, fdarray, pgm, pgmparm)
int     proctyp,
        pgmflags,
        fdarray[];
char   *pgm,
       *pgmparm;
{
	int retval;
	char *parmptr;
				  /* printf ("newpgml: calling newpgmv\n"); */
    parmptr = &pgmparm;
    retval = newpgmv (proctyp, pgmflags, fdarray, pgm, parmptr);
    return (retval);
}

newpgmv (proctyp, pgmflags, fdarray, pgm, pgmparm)
int     proctyp,
        pgmflags,
        fdarray[];
char   *pgm,
       *pgmparm[];
{
    register int    tmp;
    int     tmp2;
    int     childid;
    int     tried;
    int     osig[3];

    if (proctyp != PUREXEC)
    {
				  /* printf ("This is a forking call.\n"); */
	for (tried = NUMTRY; (childid = fork ()) == -1 && tried--; sleep (1));
	if (childid == -1)
	    return (-1);
				  /* printf ("Successful fork\n"); */
	if (childid != 0)
	{			  /* parent process               */
	    if (pgmflags & FRKPIGO)
	    {			  /* disable parent signals       */
				  /* printf ("Parent to be
				     non-interruptible\n"); */
		osig[0] = signal (1, 1);
		osig[1] = signal (2, 1);
		osig[2] = signal (3, 1);
	    }
	    if ((proctyp == FRKEXEC) && (pgmflags & FRKWAIT))
	    {
				  /* printf ("Parent is to wait\n");      */
		while ((tmp = wait (&tmp2)) != childid && tmp != -1);
				  /* printf ("Parent done waiting\n");    */
		if (pgmflags & FRKPIGO)
		{
		    signal (1, osig[0]);
		    signal (2, osig[1]);
		    signal (3, osig[2]);
		}
		return (tmp2);
	    }
	    return (childid);
	}
	if (proctyp == SPNEXEC)
	{			  /* want it to be a spawn        */
				  /* printf ("This is a spawn\n");    */
	    for (tried = NUMTRY; (tmp = fork ()) < 0 && tried--;
		    sleep (1));
	    if (tmp != 0)	  /* split the grandparent from the   */
		exit (tmp < 0 ? -2 : 0);
				  /* grandchild: kill middle proc     */
	}
    }
    if (fdarray)
    {				  /* re-align fd array list           */
	fout.b_fildes = fdarray[1];
	for (tmp = 0; tmp < highfd; tmp++)
	{			  /* first do the re-positions        */
	    if (fdarray[tmp] != CLOSEFD && fdarray[tmp] != tmp)
	    {
				   dbglog ("Closing %2d\n", tmp);     
flush ();
		close (tmp);
		while ((tmp2 = dup (fdarray[tmp])) < tmp && tmp2 != -1);
		   dbglog ("Last dup'd %d into %d\n", fdarray[tmp], tmp2);     
flush ();
				  /* did we get right fd?     */
	    }
	}
	for (tmp = 0; tmp < highfd; tmp++)
	    if (fdarray[tmp] == CLOSEFD)
	    {
				   dbglog ("Closing %2d\n", tmp);     
flush ();
		close (tmp);      /* get rid of unwanted ones */
	    }
    }
fout.b_fildes = 1;

    if (pgmflags & FRKCIGO)
    {
				  /* printf ("Child's interrupts to be
				     disabled\n");      */
	signal (1, 1);
	signal (2, 1);
	signal (3, 1);
    }

 /* printf ("Execing %s\n", pgm);    */
/*  if (pgmflags & EXECR)       */
/*      execr (pgm, pgmparm);   */
/*  else                        */
    execv (pgm, pgmparm);
				  /* printf ("Exec not successful\n"); */
    if (proctyp == PUREXEC)
	return (-1);
    fout.b_fildes = 1;
    printf ("Unable to exec \"%s\"\n", pgm);
    flush ();
    for (tmp = 0; tmp < 20 && pgmparm[tmp][0] != 0 ; tmp++)
    {
	printf ("Parm %d = \"%s\"\n", tmp, pgmparm[tmp]); flush ();
    }
    exit (-3);
}

childend ()
{
    int     tmp,
            tmp2;

    /* close (sbmitfd[0]); */
    close (sbmitfd[1]);
    while ((tmp = wait (&tmp2)) != sbchild && tmp != -1);
    return (tmp2 >> 8);
}
dbglog (str)
{
return;
printf(str);
}
