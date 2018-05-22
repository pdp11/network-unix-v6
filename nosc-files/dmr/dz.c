#
/*
module name:
	dz.c

function:
	Device driver for the DEC dz11 8 line asynchronous multiplexor

globals containied:
	dz11	array of tty structures for the dz11 lines
	dzaddrs	array of addresses of the various dz11 controllers
	dzmodem	array specifying which dz ports have modem control
	dzcycling	whether or not modem control is cycling

routines contained:
	dzopen		open system call routine
	dzclose		close system call routine
	dzread		read system call routine
	dzwrite         write system call routine
	dzrint          receive interrupt routine
	dzxint          transmit interrupt routine
	dzstart         I/O startup routine
	dzsgtty         sgtty system call routine
	dzparam         hardare parameter setting routine
	dzstatus	modem control cycling routine

routines referenced:
	ttread, ttwrite, ttyinput, ttystty, ttrstrt
	sleep, wakeup
	timeout
	issig, psig

modules referenced:
	user.h          needed because of the error code definitions
	param.h         contains constants required by user.h
	conf.h          contains the device structure definition
	tty.h           contains definition of tty structure
	options.h       to get system compile time parameters

compile time parameters:
	UCLATTY         define this if you are using the ucla tty.c
			leave this undefined if you are using the standard one

history:
	Designed and coded by Mark Kampe, 9/27/77.
*/
#include "param.h"
#include "conf.h"
#include "user.h"
#include "tty.h"

#define true    0177777
#define false   0000000
#define then    /* */

#ifndef NDZ11
#define NDZ11   8		/* default number of DZ lines */
#endif not NDZ11
#ifndef DZADDRS
#define DZADDRS 0760100		/* default address of dz registers */
#endif not DZADDRS
#ifndef DZMODEM
#define DZMODEM	1, 1, 1, 1, 1, 1, 1, 1	/* default modem control enable */
#endif not DZMODEM
#define DZRATE	5*HZ		/* sample modem status every 5 seconds */

struct  tty     dz11[ NDZ11 ];

struct  dzregs          /* the readable DZ11 registers          */
{       int     dzcsr;          /* control status register      */
	int     dzrcv;          /* receive buffer register      */
	int     dztcr;          /* transmit control register    */
	int     dzmsr;          /* modem status register        */
};

struct                  /* the writeable DZ11 registers         */
{       int     dzcsr;          /* control status register      */
	int     dzlpr;          /* line parameter register      */
	int     dztcr;          /* transmit control register    */
	int     dztdr;          /* transmit data register       */
};

struct  dzregs  *dzaddrs[] { DZADDRS };	/* addresses of DZ registers */

/* specification of which lines have modem control enabled */
char    dzmodem[ NDZ11 ]                /* Setting the i'th entry to a 1  */
{	DZMODEM		};		/* enables modem control on that  */
					/* line.  Although this array is  */
					/* statically initialized, it can */
					/* be changed dynamically.        */

int dzcycling;			/* whether or not the dz has a modem test
				   pending on any modem controlled lines */
extern	ttrstrt();
int	dzstart();

char speedmap[ 16 ]             /* map a Unix speed into a DZ11 equivalent */
{       000,                    /* 0 baud */
	000,                    /* 50 baud */
	001,                    /* 75 baud */
	002,                    /* 110 baud */
	003,                    /* 134.5 baud */
	004,                    /* 150 baud */
	000,                    /* dz doesn't support 200 baud */
	005,                    /* 300 baud */
	006,                    /* 600 baud */
	007,                    /* 1200 baud */
	010,                    /* 1800 baud */
	012,                    /* 2400 baud */
	014,                    /* 4800 baud */
	016,                    /* 9600 baud */
	000,                    /* external 1 not supported */
	000                     /* external 2 not supported */
};


/* definitions for bits in the DZ11 registers */
#define MUXCLR  0000020         /* clear multiplexer    in csr  */
#define SCENABL 0000040         /* scan enable          in csr  */
#define IENABLE 0040100         /* xmt & rcv interrupts in csr  */
#define RINT    0000200         /* receive done bit     in csr  */
#define XINT    0100000         /* transmit done bit    in csr  */
#define LINENUM 0003400         /* line # mask    in rcv & csr  */
#define PERROR  0010000         /* parity error         in rcv  */
#define FRERROR 0020000         /* framing error        in rcv  */
#define OVERRUN 0040000         /* data over run        in rcv  */
#define BITS_6  0000010         /* six bit characters   in lpr  */
#define BITS_7  0000020         /* seven bit characters in lpr  */
#define BITS_8  0000030         /* eight bit characters in lpr  */
#define ONE_SB  0000000         /* one stop bit         in lpr  */
#define TWO_SB  0000040         /* two stop bits        in lpr  */
#define PENABLE 0000100         /* parity enabled       in lpr  */
#define OPAR    0000200         /* odd parity selected  in lpr  */
#define SSPEED  0000007         /* unix notation for 300 baud   */
#define RCV_ON  0010000         /* receiver enable      in lpr  */
#define DEFAULT 0012470         /* initial parameters for  lpr  */

/* name:
	dzopen  and dzclose

function:
	open system call routine for the dz11
	close system call routine for the dz11

algorithm:
open:
	validate the device unit number
	get the controller address
	if the line isn't already open
		initialize the hardware
		if modem control is enabled on that line
			wait for the line to come up
		initialize the software parameters
	call ttyopen on it

close:
	flush any pending output
	mark the line closed
	disable further input from that line

parameters:
	device designation      (major and minor numbers)
	flag                    (whether or not write access is desired)

returns:
	a setting of u.u_error if it fails

globals:
	dz11            tty tables for the dz11 lines
	dzaddrs         pointer to the registers
	dzmodem
	dzcycling

calls:
	ttyopen         to complete the opening operation
	wflushtty       to flush the queues
	dzparam         to set the initial parameters
	dzstatus	to start modem status cycling

called by:
	openi through cdevsw
*/
dzopen( dev , flag )
 int dev, flag;
{       register struct dzregs *dzp;
	register struct tty *tp;
	register int unit;

	unit = dev.d_minor;
	if (unit >= NDZ11)
		then    return( u.u_error = ENXIO );

	dzp = dzaddrs[ (unit >> 3) ];
	tp = &dz11[ unit ];
	unit =& 7;

	if ((tp->t_state & ISOPEN) == 0) then
	{       dzp->dztcr =| (0400 << unit);
		dzp->dzcsr =| (SCENABL|IENABLE);

		if ( dzmodem[dev.d_minor] ) then
		{       spl5();
			dzp->dzlpr = unit|DEFAULT;
			tp->t_state = WOPEN;
			if (~dzcycling)
				then	dzstatus();
			while((tp->t_state & CARR_ON) == 0)
			{       sleep( tp, TTIPRI );
				if (issig()) then
				{       spl0();
					psig();
					return;
				}
			}
			spl0();
		}

		tp->t_state =| SSTART|CARR_ON;
		tp->t_flags = XTABS|ECHO|CRMOD|EVENP|ODDP;
		tp->t_erase = CERASE;
		tp->t_kill = CKILL;
		tp->t_speeds = SSPEED | (SSPEED << 8);
		tp->t_addr = dzstart;

		dzparam( dev.d_minor );
	}

	ttyopen( dev, tp );
}


dzclose( dev, flag )
 int dev, flag;
{       register struct tty *tp;
	register int unit;
	register struct dzregs *dzp;

	unit = dev.d_minor;
	tp = &dz11[ unit ];
	dzp = dzaddrs[ unit >> 3 ];

	wflushtty( tp );

	tp->t_state = 0;
	unit =& 7;
	dzp->dzlpr = unit;
	dzp->dztcr =& ~(0400 << unit);
}
/* name:
	dzread and dzwrite

function:
	read system call routine for the dz11
	write system call routine for the dz11

algorithm:
	convert the minor device designation into a pointer to a tty struct
	call the appropriate device independent routine

parameters:
	major and minor device designations

globals:
	dz11

calls:
	ttread
	ttwrite

called by:
	readi and writei through the cdevsw
*/

dzread( dev )
 int dev;
{       register int unit;
	register struct tty *tp;

	unit = dev.d_minor;
	tp = &dz11[ unit ];
	ttread( tp );
}

dzwrite( dev )
 int dev;
{       register int unit;
	register struct tty *tp;

	unit = dev.d_minor;
	tp = &dz11[ unit ];
	ttwrite( tp );
}
/* name:
	dzrint

function:
	receive interrupt handling routine for the dz11

algorithm:
	get a pointer to the registers for the interrupting dz11
	while there is data in the silo
		get the next character
		if from an illegal line,
			continue
		if a framing error
			say the character was a null
		call ttyinput with the character

parameters:
	dz11 controller number of the interrupting unit

globals:
	dz11    tty structures for the dz11 lines
	dzaddrs address of dz11 controllers

calls:
	ttyinput        to process the received characters

called by:
	trap
*/
dzrint( unit )
 int unit;
{       register struct dzregs *dzp;
	register struct tty *tp;
	register int c;
	int lineno;

	dzp = dzaddrs[ unit ];
	unit =<< 3;

	while((c = dzp->dzrcv) < 0)
	{       lineno = ((c & LINENUM) >> 8) + unit;
		if (lineno >= NDZ11)
			then    continue;

		tp = &dz11[ lineno ];

		if (c & PERROR)
			then    continue;

		if (c & FRERROR)
			then    c = 0;
			else    c =& 0377;

		if (tp->t_state & ISOPEN)
			then    ttyinput( c , tp );
			else    wakeup( tp );
	}
}
/* name:
	dzxint

function:
	dz11 transmit interrupt routine

algorithm:
	get a pointer to the controller that interrupted
	while the dj thinks that I am done with some line
		get the line number
		get the associated tty structure
		if it is timed out, or done disable this line
		get the next character
		if it is a delay indication, schedule the restart
			and disable this line
		else set the character up for output
		if there is a sleeping writer and the queue has drained some
			wake him up

parameters:
	the controller number for the dz11 that interrupted

globals:
	dz11    pointers to the teletype structures for the dz11 lines
	dzaddrs pointers to the dz11 controllers

calls:
	wakeup
	timeout

called by:
	trap
*/
dzxint( unit )
 int unit;
{       register struct dzregs *dzp;
	register struct tty *tp;
	register int lineno;
	int  c;

	dzp = dzaddrs[ unit ];
	unit =<< 3;
	while( dzp->dzcsr&XINT )
	{       lineno = (dzp->dzcsr&LINENUM) >> 8;
		tp = &dz11[ lineno + unit ];

		if ((tp->t_state&TIMEOUT) || ((tp->t_state&ISOPEN) == 0))
			then goto disable;

		if ((c = getc( &tp->t_outq )) < 0 )
			then goto disable;

#ifdef  UCLATTY
		if (c == CESCAPE)
			then c = getc( &tp->t_outq );
			else
#endif
		if (c & 0200)   then
		{       tp->t_state =| TIMEOUT;
			if (c =& 0177)
				then timeout( ttrstrt, tp, c&0177 );
#ifdef  UCLATTY
				else tp->t_state =| STOPOUT;
#endif
				goto disable;
		}

		dzp->dztdr = c;

		if ((tp->t_state&ASLEEP) && (tp->t_outq.c_cc <= TTLOWAT)) then
		{       tp->t_state =& ~ASLEEP;
			wakeup( &tp->t_outq );
		}
		continue;

disable:
		dzp->dztcr =& ~(1 << lineno);
	}

	dzp->dzcsr =| IENABLE;
}
/* name:
	dzstart

function:
	to start an output operation on a dz line

algorithm:
	if the line is timed out, do nothing
	else enable it for transmission

parameters:
	pointer to the tty structure for that line

globals:
	dzaddrs         pointers to addresses of dz11 controllers
	dz11            table of tty structures for dz lines

calls:
	wakeup          to notify a sleeping writer
	timeout         to schedule resuming output

called by:
	dzxint          to continue an output operation
	ttstart         to initiate an output operation
*/


dzstart( atp )
 struct tty *atp;
{       register struct dzregs *dzp;
	register struct tty *tp;
	register int lineno;

	tp = atp;

	if (tp->t_state&TIMEOUT)
		then    return;

	lineno = tp - dz11;
	dzp = dzaddrs[ lineno >> 3 ];
	lineno =& 7;

	dzp->dztcr =| (1 << lineno);
}
/* name:
	dzsgtty

function:
	dz11 sgtty system call routine

algorithm:
	get a pointer to the corresponding tty structure
	call ttystty
	if anything changed, call dzparam

parameters:
	major and minor device designation
	argument vector pointer

globals:
	dzaddrs         addresses of dz11 controllers

calls:
	ttystty         device independent special function routine

called by:
	sgtty           (throught cdevsw)
*/

#ifdef  UCLATTY
dzsgtty( dev )
 int dev;
#endif
#ifndef   UCLATTY
dzsgtty( dev , v )
 int dev, *v;
#endif
{       register struct tty *tp;
	register int unit;

	unit = dev.d_minor;
	tp = &dz11[ unit ];

#ifdef  UCLATTY
	if (ttystty( tp ))
#endif
#ifndef   UCLATTY
	if (ttystty( tp, v ))
#endif
		then    return;
		else    dzparam( unit );
}
/* name:
	dzparam

function:
	DZ11 parameter setting routine

algorithm:
	ascertain what controller the specified unit is on
	get a pointer to its tty structure
	if the speed is set to 0, just turn the line off
	else, compute the parametersd
	and install them for that line

parameters:
	DZ unit number

globals:
	dzaddrs         to get the controller addresses
	dz11            tty structure for the dz lines

called by:
	dzopen          to initialize the line for the first time
	dzsgtty         to change parameters
*/

dzparam( unit )
 int unit;
{       register struct dzregs *dzp;
	register struct tty *tp;
	register int parms;
	int speed;

	dzp = dzaddrs[ unit >> 3 ];
	tp = &dz11[ unit ];
	unit =& 7;

	if (tp->t_speeds == 0) then
	{       dzp->dzlpr = unit;       /* just turn the line off */
		return;
	}
	else
	{       speed = tp->t_speeds&017;
		speed = speedmap[ speed ];
	}
	parms = unit|RCV_ON;
	parms =| (speed << 8);
	if (tp->t_flags & EVENP) then
		if (tp->t_flags & ODDP)
			then    parms =| BITS_8;
			else    parms =| (BITS_7|PENABLE);
		else    parms =| (BITS_7|OPAR|PENABLE);
	if (tp->t_speeds.lobyte == 3)
		then    parms =| TWO_SB;

	dzp->dzlpr = parms;
}
/* name:
	dzstatus

function:
	to see if the lines with modem control enabled still have the
	appropriate signals up

algorithm:
	For all open lines with modem control enabled,
		if the carrier has dropped, send a hangup to the group
	if any open lines still have modem control
		schedule the next call to dzstatus
	else	turn off dzcycling

parameters:
	none

globals:
	dz11
	dzmodem
	dzcycling

calls:
	timeout

called by:
	timeout

*/
dzstatus()
{	register int line;
	register struct tty *tp;
	register struct dzregs *dzp;
	int numlines;

	numlines = 0;
	for( line = 0; line < NDZ11; line++ )
	{	dzp = dzaddrs[ line >> 3 ];
		tp = &dz11[ line ];

		if ((dzmodem[ line ] == 0) || ((tp->t_state&(ISOPEN+WOPEN)) == 0) )
			then	continue;

		numlines++;

		if (dzp->dzmsr & (0400 << (line&7))) then
		{	tp->t_state =| CARR_ON;
			if (tp->t_state&WOPEN)
				then	wakeup( tp );
		}
		else
		{	tp->t_state =& ~CARR_ON;
			if (tp->t_state & ISOPEN) 
				then	signal( tp->t_pgrp, SIGHUP );
		}
	}

	if (numlines) then
	{	timeout( &dzstatus, 0, DZRATE );
		dzcycling = true;
	}
	else	dzcycling = false;
}
