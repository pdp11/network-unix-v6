#define MAXCOL  71		       /* for CR if tty reaches this col */


/* load - execute cc on argument files - compiling those which need it.
	for each arg "file" - if file.c was modified since file.o
	was created, the arg passed is file.c, else file.o.  Args
	preceded by - are left alone */


char    svfile[] "/tmp/load.xxxxxxxxxx";
				       /* x's will be user name */
extern int  fout;		       /* for putchar                  */

main (nargs, argary)
int     nargs;
char   *argary[];
{
    char    ch,
            pwf[100];
    char  **a,
           *strptr;
    char   *args[300];
    int     i,
            j,
            oc,
            col;
    int     new0,
            new1,
            standout;		       /* standard output              */
    long int    buf[9],		       /* to check dates between .c    */
                obuf[9];	       /*    and .o files              */

    getpw (getuid (), pwf);	       /* find entry in passwd file    */
    for (i = 12; pwf[i - 12] != ':'; i++)
	svfile[i] = pwf[i - 12];       /* copy up to colon     */
    svfile[i] = 0;		       /* terminate with a zero        */
    fout = standout = dup (1);


    if (nargs == 1)		       /* then read from /tmp file     */
    {
	new0 = dup (0);
	close (0);

	if (open (svfile, 0) < 0)
	{
	    printf ("load: no args given\n");
	    flush ();
	    exit (0);
	}

	i = j = 0;		       /* character and line counters   */
	args[0] = alloc (21);	       /* 20-char increments            */

	while ((ch = getchar ()) && (ch != '\n'))
	{
	    col++;
	    if (ch == ' ')	       /* end of argument           */
	    {
		args[i][j] = '\0';     /* end str with null         */
		if (col >= MAXCOL - 30)
		{
		    putchar ('\n');
		    col = 0;
		}
		printf ("%s ", args[i++]);
		if (i > sizeof args)
		{
		    printf ("\nload: too many arguments.\n");
		    flush ();
		    exit (1);
		}
		args[i] = alloc (21);
		args[i][j = 0] = 0;    /* to test emptiness         */
	    }
	    else
	    {
		args[i][j++] = ch;
		if (j % 20 == 0)       /* get more space       */
		{
		    args[i][j] = '\0';
		    strptr = alloc (j + 21);
				       /* 20 char increment   */
		    strcpy (args[i], strptr);
		    free (args[i]);
		    args[i] = strptr;
		}
	    }
	}
	if (j != 0)
	{
	    args[i][j] = 0;	       /* terminate the last one       */
	    printf ("%s", args[i++]);
	}

	putchar ('\n');
	nargs = i;

	close (0);		       /* reset standard input         */
	dup (new0);
	close (new0);
    }
    else
    {
	flush ();
	if ((fout = creat (svfile, 0644)) < 0)
	{			       /* make a trace file   */
	    fout = standout;
	    printf ("load: unable to create \"memory\" of this run.\n");
	    flush ();
	    for (i = 0; i < nargs; i++)/* setup calling array  */
		args[i] = argary[i];
	}
	else
	{
	    for (i = 0; i < nargs; i++)/* make the trace       */
		printf ("%s ", args[i] = argary[i]);
				       /* setup calling array  */
	    putchar ('\n');
	    flush ();
	    fout = standout;
	}
    }

    a = alloc (2 * (nargs + 1));
    a[nargs] = 0;

    for (i = 1; i < nargs; i++)
    {
	if (*args[i] == '-')
	    a[i] = args[i];
	else
	{
	    if (stat (a[i] = cat (args[i], ".c"), buf) != -1)
	    {			       /* there's a .c file            */
		if ((stat (strptr = cat (args[i], ".o"), obuf) != -1) &&
			(buf[8] < obuf[8]))
		    a[i] = strptr;     /* has .o file which is younger */
	    }
	    else
		if (stat (strptr = cat (args[i], ".o"), buf) != -1)
		    a[i] = strptr;     /* will settle for .o file      */
		else
		    if (stat (args[i], buf) != -1)
			a[i] = args[i];/* take named file if any       */
		    else
			printf ("load: file \"%s\" not found.\n", args[i]);
	}
    }
    a[0] = "cc";

    col = 0;
    for (i = 0; i < nargs; i++)	       /* tell user the real calling  */
    {				       /* arguments                   */
	col =+ strlen (a[i]) + 1;
	if (col > MAXCOL)
	{
	    putchar ('\n');
	    col = strlen (a[i]) + 1;
	}

	printf ("%s ", a[i]);
    }
    putchar ('\n');
    flush ();

    unlink ("a.out");		       /* if it is there               */

    if (!strcomp (args[0], "ex") || (fork () == 0))
    {
	execvsrh (a);		       /* get cc from appropriate dir  */
	flush ();
	exit (-1);
    }
				       /* If got here, then did a fork */
    wait (&oc);			       /* wait for the compilation to be
				          completed */

    if (stat ("a.out", buf) != -1)     /* then a.out exists           */
    {
	printf ("ex: executing a.out\n");
	flush ();
	execl ("a.out", "a.out", 0);
    }
    exit (0);			       /* for some reason omitting     */
				       /*  this gives bus error        */
}


char   *cat (a, b)		       /* assumes 2-char 2nd arg       */
char   *a,
       *b;
{
    char   *bb;
    register char  *aa;

    aa = bb = alloc (3 + strlen (a));  /* 2 longer than orig string   */

    while (*aa++ = *a++);	       /* copy 1st source into new place */
    aa--;
    while (*aa++ = *b++);	       /* concatenate 2nd to end        */

    return (bb);		       /* return address of result     */
}


strcomp (s, t)
char   *s,
       *t;
{
    while (*s == *t++)
	if (!*s++)
	    return (1);
    return (0);
}


strcpy (ss, tt)
char   *ss,
       *tt;
{
    register char  *s,
                   *t;

    s = ss;
    t = tt;
    while (*t++ = *s++);
}

char   *SRHLIST[]		       /* upper case!!                 */
{
    "",
    "%",
    "/bin/",
    "/usr/bin/",
    0
};
char  **srhlist SRHLIST;

execvsrh (argv)
char  **argv;
{
    extern  errno;
    register char **r1,
                   *r2,
                   *r3;
    static char command[50];

    for (r1 = srhlist; r2 = *r1++;)
    {
	r3 = command;
	flush ();
	if (*r2 == '%')
	{
	    r2 = getpath (getluid () & 0377);
	    flush ();
	    while (*r3++ = *r2++);
	    --r3;
	    r2 = "/bin/";
	}
	while (*r3++ = *r2++);
	--r3;

	r2 = argv[0];
	while (*r3++ = *r2++);
	if ((execv (command, argv) == -1) &&
		(errno != 2))
	    break;
    }
    printf ("Unable to run \"%s\" because:  %s\n",
	    argv[0],
	    (errno == 7) ? "arg list too long" : "not found");
}

/*
 * Inode structure as returned by
 * system call stat.
 */
struct inode
{
    int     i_dev;
    int     i_number;
    int     i_mode;
    char    i_nlink;
    char    i_uid;
    char    i_gid;
    char    i_size0;
    char   *i_size1;
    int     i_addr[8];
            long i_atime;
            long i_mtime;
};

/* advisable to use this define to generate owner as 16-bit num */
#define IOWNER(i_n_buf) (((i_n_buf).i_uid<<8)|(i_n_buf).i_gid&0377)

/* modes */
#define	IALLOC	0100000
#define	IFMT	060000
#define IFDIR   040000
#define IFCHR   020000
#define IFBLK   060000
#define	ILARG	010000
#define	ISUID	04000
#define	ISGID	02000
#define ISVTX	01000
#define	IREAD	0400
#define	IWRITE	0200
#define	IEXEC	0100

struct
{
    char    d_minor,
            d_major;
};


/* this subr returns owner of tty attached to file handle # 2   */
/* on harv/unix, this may not be the real uid, because          */
/* we have the command "alias" which spawns a sub-shell with    */
/* a new real uid.  luid (login uid) is used by the harvard shell */
/* to find the "save.txt" file (via getpath(getluid())) for $a - $z */
/* shell macros, and elsewhere by certain accounting routines.  */

getluid ()
{
    struct inode    ibuf;

    if (fstat (2, &ibuf) < 0 &&
	    fstat (1, &ibuf) < 0)
	return (getuid () & 0377);

    if ((ibuf.i_mode & IFMT) != IFCHR)
	return (getuid () & 0377);

    return (ibuf.i_uid & 0377);
}

int     last_uid;		       /* used to check if info already in
				          u_buf */
char    u_buf[120];		       /* buffer used by get*.c                
				       */
int     pich;			       /* global file handle used by get*.c    
				       */
#define NULSTR -1		       /* guaranteed to be a null string       
				       */

getpath (uid)
{
    char   *path;
    register char  *pp,
                   *bp;
    register int    n;

    if (getubuf (uid) <= 0)
	return (NULSTR);

    path = alloc (50);
    pp = path;
    bp = u_buf;

    for (n = 0; n < 5; n++)
	while (*bp++ != ':');
    while ((*pp++ = *bp++) != ':');
    *--pp = '\0';

    return (path);
}


getubuf (uid)
{
    if ((uid != last_uid) || (uid == 0))
	if (getpw (uid, u_buf))
	    return (0);
    last_uid = uid;
    return (1);
}

/*                                                                      */
/*      Return the length of the given string.  This length does not    */
/*  include the terminating null character.                             */

strlen (string)
char   *string;
{
    register char  *rstring;
    register int    rlength;	       /* length of string             */

    rstring = string;

    for (rlength = 0; *rstring++ != 0; ++rlength);
    return (rlength);
}
