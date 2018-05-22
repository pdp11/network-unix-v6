#

/* pg filter--relies on the kludge that file descriptor 2 is controlling tty
 *	and it can be written to as well as read from.  1976 November 30.
 *
 *	Usage:	prog|pg -N|prog2
 *
 *	where N is the pagesize desired.  Currently 23 is the default.
 *	Compile with	cc -O -n -s pg.c; chmod 1555 pg; cp pg /usr/bin
 *
 *	pg could trail one more char behind its input, looking for eof
 *	so that "More? " won't appear just before eof.
 *	There is a whole sh*t load of options that only will power prevent
 *	me from adding.
 *
 *	Author: Richard Balocca, the Center for Advanced Computation,
 *		University of Illinois at Urbana-Champaign
 */


#include "/sys/netsys/util/etch.c"
char	shortbuf[100];
char	msg[] "More? ";
int pagesize	23;

main(argc,argv)		int argc;	char **argv;
{
    register int lcnt, c;
    register char *ptr;
    static int eoflag;


    if(argc>=2)
    {
	pagesize = atoi(&argv[1][0]);

	if(pagesize<0)	pagesize = -pagesize;
    }

    while( (c=getch())>=0 )
    {
	putch(c);

	if( c == '\n' )
	{
	    if(++lcnt >= pagesize && !eoflag)
	    {
		lcnt = 0;

		for(ptr = &msg[0]; *ptr != '\0';)/* write out question */
		{
		    putch(*ptr++);
		}

		fflush(&obuf);

		if( read(2,&shortbuf,sizeof shortbuf)<=0 )  /* read a line from 2 */
		{
		    eoflag++;
		    putch('\n');	/* so that control-d looks good */
		}
		else
		{
		    if(shortbuf[0] == 'n')	return;
		}
	    }
	}
    }

    fflush(&obuf);
}
