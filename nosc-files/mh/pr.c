#
/*
 *   print file with headings
 *   precl[2]+head+2+page[56]+endl[5]
 */

/*
 *   NOTE:  Defaults are set in the routine dflts()
 *          and not in these declares.
 */

int     errno;
char   *sys_errlst[];

int     ncol 1;			       /* number of columns            */
char   *header;			       /* header text                  */
int     col;
int     icol;
int     file;
char   *bufp;
#define	BUFS	5120
char    buffer[BUFS];
#define	FF	014
int     line;			       /* current line on page         */
int     linum;			       /* current text line            */
int     ftext;			       /* first text line on page      */
char   *colp[72];
int     nofile;			       /* number of current file       */
char    isclosed[10];
int     peekc;
int     swval;			       /* value for next switch        */
int     fpage;			       /* first page to print          */
int     page;			       /* current page number          */
int     oheadr;			       /* use old header format        */
int     colw;			       /* column width                 */
int     nspace;
int     width 65;		       /* line width                   */
int     pwidth 65;		       /* width of physical line       */
int     length 66;		       /* length of physical page      */
int     plength 61;		       /* last print line              */
int     margin 10;
int     prone 1;		       /* print header on page one     */
int     ntflg;			       /* no header or trailer         */
int     nuflg;			       /* not header line              */
int     mflg;
int     tabc;			       /* print this as column separator */
char   *tty;
int     mode;
int     precl 2;		       /* # blank lines preceding hdr  */
int     endl 5;			       /* min # blank lines end of page */
int     indent;			       /* spaces preceding each line   */
int     ff;			       /* use ff, not blank lines      */
int     wasff;			       /* page break caused by ff      */
int     blankl;			       /* # blank lines between lines  */
int     count;			       /* whether to count linums      */
int     tab2sp;			       /* change tabs => spaces        */
int     queeze;			       /* squeeze blank lines from top */
				       /*   of each page               */
int     jnamflg;		       /* print just file and not path */
int     rapflg;			       /* wrap-around line overflow    */
int	spaftfil 0;			/* space between files */


struct inode
{
    int     dev;
    int     inum;
    int     flags;
    char    nlink;
    char    uid;
    char    gid;
    char    siz0;
    int     size;
    int     ptr[8];
    int     atime[2];
    int     mtime[2];
};

main (argc, argv)
char  **argv;
{
    int     nfdone;
    int     onintr ();
    extern  fout;

    tty = "/dev/ttyx";
    fout = dup (1);
    close (1);
    if ((signal (2, 1) & 01) == 0)
	signal (2, onintr);
    fixtty ();
    dflts (0);
    for (nfdone = 0; argc > 1; argc--)
    {
	argv++;
	if (**argv == '-')
	{
	    swval = 1;		       /* default to on */
    setsw: 
	    switch (*++*argv)
	    {
		case 'b': 
		    blankl = getn (++*argv);
		    continue;

		case 'c': 
		    count = swval;
		    continue;

		case 'd': 
		    dflts (*++*argv);
		    continue;

		case 'e': 
		    endl = getn (++*argv);
		    continue;

		case 'f': 
		    ff = swval;
		    continue;

		case 'h': 
		    if (argc >= 2)
		    {
			header = *++argv;
			argc--;
		    }
		    continue;

		case 'i': 
		    indent = getn (++*argv);
		    continue;

		case 'j': 
		    jnamflg = swval;
		    continue;

		case 'l': 
		    length = getn (++*argv);
		    continue;

		case 'm': 
		    mflg = swval;
		    continue;

		case 'n': 
		    prone = (swval ^ 1);
				       /* Yes => no */
		    continue;

		case 'o': 
		    oheadr = swval;
		    continue;

		case 'p': 
		    precl = getn (++*argv);
		    continue;

		case 'q': 
		    queeze = swval;
		    continue;

		case 'r': 
		    rapflg = swval;
		    continue;

		case 's': 
		    spaftfil = getn( ++*argv);
		    continue;

		case 't': 
		    ntflg = swval;
		    continue;

		case 'u': 
		    nuflg = swval;
		    continue;

		case 'w': 
		    width = getn (++*argv);
		    continue;

		case 'x': 
		    tab2sp = swval;
		    continue;

		case '-': 
		    swval = 0;
		    goto setsw;

		default: 
		    ncol = getn (*argv);
		    continue;
	    }
	}
	else
	    if (**argv == '+')
	    {
		fpage = getn (++*argv);
	    }
	    else
	    {
		col = 0;
		icol = 0;
		line = 0;
		nspace = 0;
		print (*argv, argv);
		nfdone++;
		if (mflg)
		    break;
	    }
    }
    if (nfdone == 0)
	print (0);
    flush ();
    onintr ();
}

onintr ()
{

    chmod (tty, mode);
    exit (0);
}

fixtty ()
{
    struct inode    sbuf;
    extern  fout;

    tty[8] = ttyn (fout);
    fstat (fout, &sbuf);
    mode = sbuf.flags & 0777;
    chmod (tty, 0600);
}

dflts (dfltpkg)
{
    switch (dfltpkg)
    {
	case 'd': 
	    ncol = 1;		       /* number of columns            */
	    width = 65;		       /* line width                   */
	    length = 60;	       /* length of physical page      */
	    plength = 61;	       /* last print line              */
	    margin = 10;
	    precl = 1;		       /* # blank lines preceding hdr  */
	    endl = 5;		       /* min # blank lines end of page */
	    indent = 5;		       /* spaces preceding each line   */
	    ff = 1;		       /* use ff, not blank lines      */
	    blankl = 0;		       /* # blank lines between lines  */
	    count = 0;		       /* whether to count linums      */
	    prone = 0;		       /* print header on page one     */
	    tab2sp = 0;		       /* change tabs into spaces      */
	    queeze = 1;		       /* squeeze beginning of pages   */
	    jnamflg = 0;	       /* print full pathname          */
	    rapflg = 1;		       /* wrap around long lines       */
	    oheadr = 0;		       /* nothin' but the best...      */
	    break;

	case 'l': 
	    ncol = 1;		       /* number of columns            */
	    width = 77;		       /* line width                   */
	    length = 60;	       /* length of physical page      */
	    plength = 61;	       /* last print line              */
	    margin = 10;
	    precl = 2;		       /* # blank lines preceding hdr  */
	    endl = 5;		       /* min # blank lines end of page */
	    indent = 5;		       /* spaces preceding each line   */
	    ff = 1;		       /* use ff, not blank lines      */
	    blankl = 0;		       /* # blank lines between lines  */
	    count = 0;		       /* whether to count linums      */
	    prone = 1;		       /* print header on page one     */
	    tab2sp = 0;		       /* change tabs into spaces      */
	    queeze = 0;		       /* preserve start of each page  */
	    jnamflg = 0;	       /* print full pathname          */
	    rapflg = 1;		       /* wrap around long lines       */
	    oheadr = 0;		       /* nothin' but the best...      */
	    break;

	case 'n': 
	    ncol = 1;		       /* number of columns            */
	    width = 80;		       /* line width                   */
	    length = 60;	       /* length of physical page      */
	    plength = 61;	       /* last print line              */
	    margin = 10;
	    precl = 2;		       /* # blank lines preceding hdr  */
	    endl = 5;		       /* min # blank lines end of page */
	    indent = 5;		       /* spaces preceding each line   */
	    ff = 1;		       /* use ff, not blank lines      */
	    blankl = 0;		       /* # blank lines between lines  */
	    count = 1;		       /* whether to count linums      */
	    prone = 1;		       /* print header on page one     */
	    tab2sp = 0;		       /* don't change tabs into spaces */
	    queeze = 0;		       /* preserve start of each page  */
	    jnamflg = 0;	       /* print full pathname          */
	    rapflg = 1;		       /* wrap around long lines       */
	    oheadr = 0;		       /* nothin' but the best...      */
	    break;

	case 0: 
	default: 
	    ncol = 0;		       /* number of columns            */
	    width = 72;		       /* line width                   */
	    length = 66;	       /* length of physical page      */
	    plength = 61;	       /* last print line              */
	    margin = 10;
	    precl = 2;		       /* # blank lines preceding hdr  */
	    endl = 5;		       /* min # blank lines end of page */
	    indent = 0;		       /* spaces preceding each line   */
	    ff = 0;		       /* use ff, not blank lines      */
	    blankl = 0;		       /* # blank lines between lines  */
	    count = 0;		       /* whether to count linums      */
	    prone = 1;		       /* print header on page one     */
	    tab2sp = 0;		       /* don't change tabs into spaces */
	    queeze = 0;		       /* preserve start of each page  */
	    jnamflg = 1;	       /* don't print full pathname    */
	    rapflg = 0;		       /* don't wrap around long lines */
	    oheadr = 1;		       /* nothin' but the worst...     */
	    break;
    }
}


print (fp, argp)
char   *fp;
char  **argp;
{
    struct inode    sbuf;
    int     tmp1;
    register int    sncol,
                    sheader;
    register char  *cbuf;
    extern  fout;

    linum = 1;
    if (ntflg)
	margin = 0;
    else
	margin = precl + endl + (nuflg ? 0 : 3);
    if (length <= margin)
	length = 66;
    if (width <= 0)
	width = 65;
    if (ncol > 72 || ncol > width)
    {
	write (2, "Very funny.\n", 12);
	exit ();
    }
    if (mflg)
    {
	mopen (argp);
	ncol = nofile;
    }
    if (ncol > 1)
	rapflg = 0;
    colw = width / ncol;
    sncol = ncol;
    sheader = header;
    plength = length - endl;
    pwidth = width + indent;
    if (ntflg)
	plength = length;
    if (--ncol < 0)
	ncol = 0;
    if (mflg)
	fp = 0;
    if (fp)
    {
	file = open (fp, 0);
	if (file < 0)
	{
	    errrpt (fp, sys_errlst[errno]);
	    return;
	}
	fstat (file, &sbuf);
    }
    else
    {
	file = 0;
	time (sbuf.mtime);
    }
    if ((header == 0) && (file != 0))
	header = jnamflg ? fp
	    : getpath (fp);
    cbuf = ctime (sbuf.mtime);
    cbuf[16] = '\0';
    cbuf[24] = '\0';
    page = 1;
    icol = 0;
    colp[ncol] = bufp = buffer;
    if (mflg == 0)
	nexbuf ();
    while (mflg && nofile || (!mflg) && tpgetc (ncol) > 0)
    {
	if (mflg == 0)
	{
	    colp[ncol]--;
	    if (colp[ncol] < buffer)
		colp[ncol] = &buffer[BUFS];
	}
	line = 0;
	if (ntflg == 0)
	{
	    for (tmp1 = precl; tmp1--;)
		put ('\n');
	    if ((nuflg == 0) &&
		    (prone || (page > 1)))
	    {
		for (tmp1 = indent; tmp1--;)
		    put (' ');
		if (!oheadr && header)
		{
		    puts (header);
		    puts ("   ");
		}
		puts (cbuf + 4);
		put (' ');
		puts (cbuf + 20);
		if (oheadr)
		{
		    puts ("  ");
		    if (header)
		    {
			puts (header);
			put (' ');
		    }
		}
		else
		    do
			put (' ');
		    while (col < (pwidth - 9));
		puts ("Page ");
		if (oheadr)
		    putd (page);
		else
		    putrd (page);
		puts ("\n\n\n");
	    }
	}
	ftext = line;		       /* # of first text line */
	putpage ();

	if (ff)
	    put ('\014');
	else
	    if (ntflg == 0)
		while (line < length)
		    put ('\n');
	page++;
    }
   if ( (ntflg != 0) && (spaftfil > 0))
	for (tmp1 = spaftfil; tmp1--;)
		put('\n');
    if (file)
	close (file);
    ncol = sncol;
    header = sheader;
}

mopen (ap)
char  **ap;
{
    register char **p,
                   *p1;

    p = ap;
    while ((p1 = *p++) && p1 != -1)
    {
	isclosed[nofile] = fopen (p1, &buffer[2 * 259 * nofile]);
	if (++nofile >= 10)
	{
	    write (2, "Too many args.\n", 15);
	    exit ();
	}
    }
}

putpage ()
{
    register int    lastcol,
                    i,
                    c;
    int     j;

    if (ncol == 0)
    {
	i = 1;			       /* always starting on new line */
	while (c = pgetc (0))
	{
	    if (i)		       /* new line */
	    {
		if ((queeze) &&
			(c == '\n') &&
			(line == ftext) &&
			(!wasff) &&
			(page > 1)
		    )
		{
		    linum++;
		    continue;
		}

		for (i = indent; i--;)
		    put (' ');
		if (count)
		{
		    putrd (linum);
		    puts ("  ");
		};
		i = 0;
	    };
	    switch (c)
	    {
		case FF: 
		    wasff = 1;
		    return;
		case '\n': 
		    linum++;
		    for ((i = (blankl + 1)); i--;)
		    {
			put ('\n');
			if (line >= plength)
			{
			    wasff = 0;
			    return;
			}
		    }
		    i = 1;	       /* signal new line */
		    break;
		default: 
		    put (c);
	    }

	}
	return;
    }
    colp[0] = colp[ncol];
    if (mflg == 0)
	for (i = 1; i <= ncol; i++)
	{
	    colp[i] = colp[i - 1];
	    for (j = margin; j < length; j++)
		while ((c = tpgetc (i)) != '\n')
		    if (c == 0)
			break;
	}
    while (line < plength)
    {
	for (i = indent; i--;)
	    put (' ');
	lastcol = colw + indent;
	for (i = 0; i < ncol; i++)
	{
	    while ((c = pgetc (i)) && c != '\n')
		if (col < lastcol || tabc != 0)
		    put (c);
	    if (c == 0 && ntflg)
		return;
	    if (tabc)
		put (tabc);
	    else
		while (col < lastcol)
		    put (' ');
	    lastcol =+ colw;
	}
	while ((c = pgetc (ncol)) && c != '\n')
	    put (c);
	put ('\n');
    }
}

nexbuf ()
{
    register int    n;
    register char  *rbufp;

    rbufp = bufp;
    n = &buffer[BUFS] - rbufp;
    if (n > 512)
	n = 512;
    if ((n = read (file, rbufp, n)) <= 0)
	*rbufp = 0376;
    else
    {
	rbufp =+ n;
	if (rbufp >= &buffer[BUFS])
	    rbufp = buffer;
	*rbufp = 0375;
    }
    bufp = rbufp;
}

tpgetc (ai)
{
    register char **p;
    register int    c,
                    i;

    i = ai;
    if (mflg)
    {
	if ((c = getc (&buffer[2 * 259 * i])) < 0)
	{
	    if (isclosed[i] == 0)
	    {
		isclosed[i] = 1;
		if (--nofile <= 0)
		    return (0);
	    }
	    return ('\n');
	}
	if (c == FF && ncol > 0)
	    c = '\n';
	return (c);
    }
loop: 
    c = **(p = &colp[i]) & 0377;
    if (c == 0375)
    {
	nexbuf ();
	c = **p & 0377;
    }
    if (c == 0376)
	return (0);
    (*p)++;
    if (*p >= &buffer[BUFS])
	*p = buffer;
    if (c == 0)
	goto loop;
    return (c);
}

pgetc (i)
{
    register int    c;

    if (peekc)
    {
	c = peekc;
	peekc = 0;
    }
    else
	c = tpgetc (i);
    if (tabc)
	return (c);
    switch (c)
    {

	case '\t': 
	    icol++;
	    if ((icol & 07) != 0)
		peekc = '\t';
	    return (' ');

	case '\n': 
	    icol = 0;
	    break;
    }
    if (c >= ' ')
	icol++;
    return (c);
}

puts (as)
char   *as;
{
    register int    c;
    register char  *s;

    if ((s = as) == 0)
	return;
    while (c = *s++)
	put (c);
}

putrd (an)
{
    register int    mag;

    for (mag = 1000; ((mag > 1) && (mag > an)); mag =/ 10)
	put (' ');
    putd (an);
}


putd (an)
{
    register int    a,
                    n;

    n = an;
    if (a = n / 10)
	putd (a);
    put (n % 10 + '0');
}

put (ac)
{
    register    c;
    int     tspace;

    c = ac;
    if (tabc)
    {
	putcp (c);
	if (c == '\n')
	    line++;
	return;
    }
    switch (c)
    {

	case ' ': 
	    nspace++;
	    col++;
	    return;

    /*     case '\t':                      */
				       /*             c = '~';                
				       */
				       /*             break;                  
				       */
				       /*               nspace =& 0177770;    
				       */
				       /*               nspace =+ 010;        
				       */
				       /*               col    =& 0177770;    
				       */
				       /*               col    =+ 010;        
				       */
				       /*               return;               
				       */

	case '\n': 
	    nspace = 0;
	    line++;
	    col = 0;
	    break;

	case 010: 
	case 033: 
	    if (--col < 0)
		col = 0;
	    if (--nspace < 0)
		nspace = 0;

    }

    if (rapflg && (col >= 84))	       /* overflow             */
    {
	putcp ('\n');
	line++;
	tspace = col - 84;
	col = nspace = 62;	       /* 84 - 22              */
	sppr ();
	putcp ('*');
	putcp ('*');
	col =+ (2 + (nspace = tspace));
    };

    sppr ();

    if (c >= ' ')
	col++;
    putcp (c);
}

sppr ()
{
    register int    c,
                    ns;

    if ((c != '\n') && (c != FF))
	while (nspace)
	{
	    if (
		    (!tab2sp) &&
		    (nspace > 2) &&
		    (col > (ns = ((col - nspace) | 07)))
		)
	    {
		nspace = col - ns - 1;
		putcp ('\t');
	    }
	    else
	    {
		nspace--;
		putcp (' ');
	    }
	}
}

getn (ap)
char   *ap;
{
    register int    n,
                    c;
    register char  *p;

    p = ap;
    n = 0;
    if ((*p < '0') || (*p > '9'))
	errrpt (p, "in parameter is supposed to be a number");

    while ((c = *p++) >= '0' && c <= '9')
	n = n * 10 + c - '0';
    return (n);
}

putcp (c)
{
    if (page >= fpage)
	putchar (c);
}

int     fout;

errrpt (str1, str2)
char   *str1,
       *str2;
{
    int     ofout;

    flush ();
    ofout = fout;
    fout = 2;

    printf ("pr: \"%s\" %s.\n", str1, str2);

    flush ();
    fout = ofout;
};


#define DOT     "."
#define DOTDOT  ".."
#define ROOT    "/"
#define DEBUG

getpath (filestr)
char   *filestr;
{
    int     n;
    int     gpfile;
    char   *fulpath;
    struct statb
    {
	int     devn,
	        inum,
	        i[18];
    }               x;
    struct entry
    {
	int     jnum;
	char    name[16];
    }               y;



    if (*filestr == '/')
    {
	fulpath = filestr;	       /* already a full name      */
	return (fulpath);
    }

    fulpath = alloc (1);	       /* got to start somewhere       */
    *fulpath = 0;
    setexit ();

    if (*fulpath)		       /* been here before ?           */
    {
	chdir (fulpath);	       /* get back to original dir     */
	rplstr (&fulpath,
		dircat ("", fulpath));
	rplstr (&fulpath,
		dircat (fulpath, filestr));
	return (fulpath);
    }

    while (1)
    {
	stat (DOT, &x);
	if ((gpfile = open (DOTDOT, 0)) < 0)
	    reset ();
	do
	{
	    if ((n = read (gpfile, &y, 16)) < 16)
		reset ();
				       /* printf("y.name = %s\n",y.name); */
	}
	while (y.jnum != x.inum);
	close (gpfile);
				       /* printf("y.jnum = %d\n",y.jnum); */
	if (y.jnum == 1)
	    ckroot (y.name, &fulpath);

	rplstr (&fulpath,
		dircat (y.name, fulpath));
				       /* printf("fulpath = %s\n",fulpath); */
	chdir (DOTDOT);
    }
}

ckroot (name, fulpath)
char   *name,
      **fulpath;
{
    int     i,
            n;
    int     gpfile;
    struct statb
    {
	int     devn,
	        inum,
	        i[18];
    }               x;
    struct entry
    {
	int     jnum;
	char    name[16];
    }               y;


    if ((n = stat (name, &x)) < 0)
	reset ();
    i = x.devn;

    if (((n = chdir (ROOT)) < 0) ||
	    ((gpfile = open (ROOT, 0)) < 0))
	reset ();

 /* printf("fulpath = %s\n",*fulpath); */
    while (1)
    {
	if ((n = read (gpfile, &y, 16)) < 16)
	    reset ();
	if (y.jnum == 0)
	    continue;
	if ((n = stat (y.name, &x)) < 0)
	    reset ();
	if (x.devn != i)
	    continue;
	x.i[0] =& 060000;
	if (x.i[0] != 040000)
	    continue;
	if ((strequ (y.name, ".")) ||
		(strequ (y.name, "..")))
	    break;
				       /* printf("y.name = %s\n",y.name); */
	rplstr (fulpath,
		dircat (y.name, *fulpath));
				       /* printf("fulpath = %s\n",*fulpath); */
    }
    reset ();
}


/*                                                              */
/*      Return the length of the given string. This length      */
/*  does not include the terminating null character.            */

pstrlen (string)
char   *string;
{
    register char  *rstring;
    register int    rlength;	       /* length of string     */

    rstring = string;

    for (rlength = 0; *rstring++ != 0; ++rlength);
    return (rlength);
}


/*                                                              */
/*      Copy the given string to the given location.            */
/*                                                              */

strcpy (from, to)
char   *from,
       *to;
{
    register char  *rfrom,
                   *rto;

    rfrom = from;
    rto = to;

    while (*rto++ = *rfrom++);
}

/*                                                                */
/*      Concatenate the first given string the the second         */
/*      given string.                                             */

dircat (prefix, suffix)
char   *prefix,
       *suffix;
{
    register char  *rprefix,
                   *rsuffix,
                   *ptr;
    char   *newstr;

    if ((ptr = newstr =
		alloc (pstrlen (rprefix = prefix) +
		    pstrlen (rsuffix = suffix) + 2))
	    == -1)
    {
	write (1, "No more string storage!\n", 24);
	exit ();
    }
    while (*ptr++ = *rprefix++);
    ptr--;
    if (ptr[-1] != '/')
	*ptr++ = '/';
    while (*ptr++ = *rsuffix++);
    return (newstr);
}

/*                                                              */
/*      Determine if the two given strings are equivalent.      */
/*                                                              */

strequ (str1, str2)
char   *str1,
       *str2;
{
    register char  *rstr1,
                   *rstr2;

    rstr1 = str1;
    rstr2 = str2;

    while (*rstr1 == *rstr2++)
	if (*rstr1++ == 0)
	    return (1);
    return (0);
}

rplstr (oldstr, newstr)
char  **oldstr,
       *newstr;
{
				       /* printf("oldstr = %s\nnewstr =
				          %s\n",*oldstr,newstr); */
    free (*oldstr);
    *oldstr = newstr;
}
