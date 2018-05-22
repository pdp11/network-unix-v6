#
/*
 *  CD-11 Card Reader Driver
 *
 *	History: written by Steve Holmgren (CAC) ?
 *		Mods by R. Balocca (CAC) 77Nov30
 */

#include	"../../h/param.h"
#include	"../../h/conf.h"
#include	"../../h/user.h"

#define CDADDR   0772460

#define GO		     01
#define DATAPACK	     02
#define HOPCHECK	     04
#define	ONLINE		    010	/* set by the TRANSITION to online */
#define XBA		    060
#define IENABLE		   0100
#define READY		   0200
#define POWCLR		   0400
#define NOMEM		  01000
#define DATALATE	  02000
#define DATAERR		  04000
#define	REREAD		  07000	/* any of these bits, and no data came in */
#define OFFLINE		 010000
#define	NOTUSED		 020000	/* hardware eof button option */
#define RDRCHECK	 040000
#define ERROR		0100000

/*
 * these defines are on cd.cdstate
 */
#define CLOSED		0
#define CD_OPEN		1
#define EOFSEEN		2

#define CDSIZE   80
#define EOFCHAR  052

#define CDPRI    10

struct {
	int	cdcsr;
	int	cdcc;
	int	cdba;
	int	cddb;
};


struct {
	char	*curcol;
	char	*endcol;
	char	cdstate;
	char	cdbuf[CDSIZE + 1];
} cd;


/*
 * Note the sleep in this start routine.  This does not
 * follow the `philosophy' of Unix xxstart routines.
 * The reason this works is because there is no concurrency
 * in this driver.  If double buffering or some such should
 * be used, this sleep should be moved out of here.
 */
cdstart()
{
	CDADDR -> cdcsr =| IENABLE;

	/* N.B.: wait for online */
	while( CDADDR->cdcsr&OFFLINE )
		sleep( &cd, CDPRI );

	CDADDR -> cdba = &cd.cdbuf [0];		/* load mem addr reg */
	CDADDR -> cdcc = -CDSIZE;		/* load col count reg*/
	CDADDR -> cdcsr =| DATAPACK|GO;		/* packed format and go */
}

cdopen(dev,flag)
{
	if( cd.cdstate != CLOSED ) {	/* still open */
		u.u_error = ENXIO;
		return;
	}
	cd.cdstate = CD_OPEN;
	cd.curcol = cd.endcol = 0;
	CDADDR -> cdcsr =| POWCLR;	/* reset reader */
}

cdclose(dev,flag)
{
	CDADDR -> cdcsr =| POWCLR;	/* reset reader */
	cd.cdstate = CLOSED;
}

/*
 * This routine is inverted: there probably should be a routine for
 * reading a char and cdread should be something like:
 *
 *		while( (c=getch())>0 && passc(c)>0);
 *
 *		--or at least this is my style...
 */

cdread()
{

	register char *curp;
	register char *endp;

	if(cd.cdstate == EOFSEEN)
		return;

	curp = cd.curcol;
	endp = cd.endcol;
	do
	{
		/* is this the end of this buf */
		if( curp == endp )
		{
			endp = cdget();	/* get another card */
			/* check for eof */
			curp = &cd.cdbuf[0];
			if( curp == endp )	/* indicates EOFSEEN */
				return;
		}
	}
	while( passc( *curp++ ) >= 0 );

	/* reset pointers for next time in */
	cd.curcol = curp;
	cd.endcol = endp;
}

cdget()
{
	register char * p;

	for(;;)
	{
		cdclear();
		spl4();
		cdstart();
		while( (CDADDR->cdcsr&READY) == 0 )
			sleep( &cd, CDPRI );		/* wait for card */
		spl0();
		/* ^^ we apparantly can get an interrupt due to
		 * transition to online at this point.  Does
		 * my solution of putting cd_online() in cdstart
		 * work (and fix this?)?
		 */
		if( CDADDR->cdcsr >= 0 )		/* Error free? */
			return cdtran();
		else
		{
			if( CDADDR->cdcsr & REREAD )
			{
				printf("Reread last card\n");
			}
			else
			{
				p = cdtran();
				if(cd.cdstate == EOFSEEN)
					return p;
			}
			printf("CD %o %o\n",CDADDR->cdcsr,CDADDR->cdba);
		}
	}
}

/*
 * This routine should perhaps use the queue stuff and create a canonical
 * queue.  This would speed the reader up (essentially multi-buffering would
 * be possible).  It would however mean that it would take more cpu
 * to handle the card reader... (But it would be charged to the user, on the
 * Illinois system...  Oh, hell, it isn't worth it.
 */
cdtran()
{

	register char *endp;
	register char *startp;

	startp = &cd.cdbuf[0];
	/* strip off trailing blanks */
	for(endp = startp+80; *--endp == 0 && endp >= startp; );

	if(endp == startp && *startp == EOFCHAR)
	{
		cd.cdstate = EOFSEEN;
		return startp;
	}

	/* translate characters */
	while( startp <= endp )
		*startp++ = cdascii( *startp );
	*startp++ = '\n';
	return startp;	/* stick in a newline */
}

/* yes fans this really is the interrupt */
cdint()
{
	wakeup (&cd);
}

char cdtab[]
{
	 ' '   ,'1'   ,'2'   ,'3'   ,'4'   ,'5'   ,'6'   ,'7' 
	,'8'   ,' '   ,':'   ,'#'   ,'@'   ,'\''  ,'='   ,'"' 
	,'9'   ,'0'   ,'/'   ,'S'   ,'T'   ,'U'   ,'V'   ,'W' 
	,'X'   ,'Y'   ,' '   ,']'   ,','   ,'%'   ,'_'   ,'>' 
	,'?'   ,'Z'   ,'-'   ,'J'   ,'K'   ,'L'   ,'M'   ,'N' 
	,'O'   ,'P'   ,'Q'   ,' '   ,'!'   ,'$'   ,'*'   ,')' 
	,';'   ,'\\'  ,'R'   ,'&'   ,'A'   ,'B'   ,'C'   ,'D' 
	,'E'   ,'F'   ,'G'   ,'H'   ,' '   ,'['   ,'.'   ,'<' 
	,'('   ,'+'   ,'|'   ,'I'
};

/*
 * This anonymous piece of magic converts from hollerith to
 * ascii.  Note that it does not use the full code: no lower
 * case or ascii compatible control characters (i.e. vt, nl,
 * etc.)
 */
cdascii(c)
char c;
{
	register c1, c2;

	c1 = c & 0377;
	if (c1>=0200)
		c1 =- 040;
	c2 = c1;
	while ((c2 =- 040) >= 0)
		c1 =- 017;
	if (c1 >= sizeof cdtab)
		return ' ';
	else
		return cdtab[c1];
}

cdclear()
{
	register char * p;

	for(p = &cd.cdbuf[0]; p<&cd.cdbuf[CDSIZE+1]; )
		*p++ = 0;
}
