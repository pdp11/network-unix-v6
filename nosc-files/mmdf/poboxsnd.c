#
#include "mailer.h"

extern struct adr  adr;
extern int timeout ();
extern int errno;
extern char eosstr[];

struct iobuf usribuf;
int savenv;
int     domsg;
int     uid;
int     gid;
int     infd;                     /* handle on source text        */
char *sender;
char   *txtname;
char   *userdir;
char    errline[100];

main (argc, argv)
int     argc;
char   *argv[];
/* if run in user mode:                                              */
/* [name,] infd, namebuf.fildes, resbuf.fildes, logbuf.files,txtfile */
{                       /* pass msgs thru standard i/o          */
    int result;

    signal (SIGCLK, &timeout);
    initcom (argc, argv);

    if ((result = getusr ()) < OK ||
	(result = privpgm (argc, argv)) != TRYMAIL)
	exit (result);
snsbstrt:
    for EVER
    {                          /* iterate thru mail to be sent      */
	switch (getname())
	{                      /* 1st addr has mode info   */
	 case NOTOK:           /* no more messages         */
	 case DONE:            /* end of adr list          */
	    exit (OK);
	 case OK:
	    break;
	 default:               /* illegal getname return       */
	    exit (NOTOK);
	}
	domsg = (adr.adr_hand == 'w');
	sbmitinit ();
	for EVER
	{
	    procadr ();
	    result = getresp ();
	    putresp ((result == OK) ? T_OK : result, errline);
	    switch (getname())
	    {
	     case DONE:                     /* null => end of adrlist   */
		write (1, "!\n", 2);
		switch (msgxfer ())
		{case NOTOK:
		    putresp (NOTOK, "error reading message file");
		    goto snsbstrt;
		 case HOSTDEAD:
		    putresp (HOSTDEAD, "it done disappeared");
		    exit (HOSTDEAD);
		 case OK:
		    write (1, "\0", 1); /* signal eof   */
		    result = getresp ();
		    if (result == DONE)
			result = OK;
		    putresp (result, (result == OK) ? 0 : errline);
		    break;                  /* real validation of xfer  */
		 default:
		    exit (NOTOK);
		}
		if ((result = newfile ()) != OK)
		    exit (result);      /* must be "!=" and not "<"     */
		goto snsbstrt;
	     case OK:
		continue;
	     default:
	     case NOTOK:
		exit (NOTOK);               /* whoops                   */
	    }
	    break;
	}
    }
}
/* *************  PRIVATE PROTOCOL HANDLING  ************************ */

getusr ()
{
    char pwline[200];
    register char  *p,
		   *q;

    if (getpw (getuid () & 0377, &pwline) == -1)
	return (NOTOK);
    for (q= pwline; *q++ != ':'; );      /* user name    */
    while (*q++ != ':');                 /* skip a field */
    uid = atoi (q);                      /* get uid      */
    while (*q++ != ':');                 /* skip over it */
    gid = atoi (q);                      /* get gid      */
    while (*q++ != ':');                 /* skip over it]*/
    for (p = q; *p != ':' && *p != '\n'; p++);
    *p = '\0';                           /* login directory     */
    userdir = strdup (q);
    return (OK);
}

privpgm ()
{
    extern char userpo[];
    register char  *p,
		   *q;
    register int    result;
    int filesiz;
    int     i;
    int     temp;
    int     status;
    char    *userprog;
    int     f;

    userprog = strcat (userdir, userpo);
    if (access (userprog, 1))      /* Is it executable? */
    {
	free (userprog);
	return (TRYMAIL);
    }
    log("[POBOX]");
    switch (f = fork ())
    {
     case 0:
	if (infd != 0)
	{
	    close (0);          /* NO!  */
	    dup (infd);         /*  "   */
	    close (infd);       /*  "   */
	}
	setgid (gid);
	setuid (uid);
	execl (userprog, userpo, adr.adr_delv, txtname, sender, 0);
	error ("can't exec program");
	exit (TRYAGN);
     case -1:
	error ("can't fork");
	free (userprog);
	return (TRYAGN);
    }
    free (userprog);
    if (envsave (&savenv))
    {
	filesiz = infdsize ();
    /*  alarm (filesiz * 10 + 30);      */
	while ((temp = waita (&status)) != f && temp != -1);
	alarm (0);
    }
    else
    {
	result = TIMEOUT;
	error ("user program timed out");
	kill (f, 9);
    }
    if (temp == -1)		  /* This shouldn't happen */
	result = TRYAGN;
    else
    {
	result = (status >> 8) & 0377;
	if (result < MINSTAT || result > MAXSTAT)
	    result = HOSTERR;
    }
    if (result < OK)
  /*    msg ("delivered")*/;
    return (result);
}

getresp ()
{
    char linebuf[LINESIZE];
    int respenv;
    register int     c;
    register char resp;

    respenv = savenv;
    if (!envsave (&savenv))
	return (TIMEOUT);

/*  alarm (30); */
    if ((c = gcread (&usribuf, &linebuf, sizeof linebuf - 1, "\000\n\377"))
	    <= 0)
	return (HOSTDEAD);
    alarm (0);
    savenv = respenv;
    linebuf[0] =- 060;       /* convert it to binary           */
    linebuf[c] = '\0';
    if (linebuf[0] < OK)
	strcpy (&linebuf[1], errline);
    return (linebuf[0]);
}

sbmitinit ()
{
    int result;
    register char *p;

    p = nmultcat ("     ",
		    (adr.adr_delv == DELMAIL) ? "m"
		    : (adr.adr_delv == DELTTY)   ? "a"
		    : "s",
/*          "fForwarded by Rand-Unix to U of Delaware", */
/*          "\n", 0);                                   */
/*  if ((result = write (1, p, strlen (p))) != -1)      */
/*  {                                                   */
/*      free (p);                                       */
/*      p = nmultcat (sender, " at Rand-Unix", "\n", 0);*/
		  sender, /*" at Rand-Unix", */"\n", 0);
	result = write (1, p, strlen (p));
/*  }   */
    free (p);
    return ((result == -1) ? NOTOK : OK);
}
/* *************  ADDRESS & MESSAGE PROCESSING  ********************* */

procadr ()
{                               /* specify addr to remote host */
    char linebuf[LINESIZE];
    register char *p;

/* BUG!!  there is a lie built in, currently, since the address
 *        list is sent before the message.  An OK for an address
 *        does not mean that a message for it has been adquately
 *        queued.  If we go down before completing xmission of
 *        the ENTIRE list AND text, some addrs, at the source,
 *        will show a completed status, when they haven't really been.
 */
			       /* already in arpanet form           */
			       /* build submit adr spec line        */
/*    for (p = adr.adr_name; *p++ != ' '; ); DN: send both host and username */
      p = adr.adr_name; /* DN: send both host and user names */
    strcpy (p, linebuf);
    p = linebuf + strlen(linebuf);
/*  printx ("%s: ", linebuf);   */
    if (adr.adr_name[strlen (adr.adr_name) - 1] != '\n')
	*p++ = '\n';
    *p = '\0';

    write (1, linebuf, strlen (linebuf));
			    /* send the address             */
    return (OK);
}

msgxfer ()
{
    int count;
    char buf[512];
    char lastchar;

    for (seek (infd, 0, 0); (count = read (infd, buf, sizeof buf)) > 0;
		lastchar = buf[count - 1])
	if (write (1, buf, count) < 0)
	{
	    error ("error writing to mailbox");
	    return (TRYAGN);
	}
    if (lastchar != '\n')
	write (1, "\n", 1);
    return (OK);
}
