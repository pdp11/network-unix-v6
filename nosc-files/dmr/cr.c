#/*
Module Name:
	cr.c -- card reader driver for Unix

Installation:

Function:
	Provide the interface necessary to handle the CR-11 card reader.

Globals contained:
	cr11	control info and wait channel address

Routines contained:
	cropen	process-level open request
	crclose	process-level close request
	crread	process-level read request
	crint	interrupt handler

Modules referenced:
	param.h		miscellaneous parameters
	conf.h		configuration options
	user.h		defination of the user table
	m40.s		for getc and putc

Modules referencing:
	c.c and l.s

Compile time parameters and effects:
	CRPRI	Waiting priority for the card reader.  Should be positive.
		Since the card reader is a delayable device, this needn't
		be particularly high (i.e., a low number).
	CRLOWAT	The low water mark.  If the card reader has been stopped
		because the process code has not been taking the data, this
		is the point at which to restart it.
	CRHIWAT	The high water mark.  If the queued data exceeds this limit
		at a card boundry, the reader will be temporarily stopped.

Module History:
	Created 30Jan78 by Greg Noel, based upon the version from the
		Children's Museum, which didn't work on our system.
		(The original was Copyright 1974; reproduced by permission.)
*/
#include	"param.h"
#include	"conf.h"
#include	"user.h"

#define	CRPRI	10
#define	CRLOWAT	50
#define	CRHIWAT	160

#define	NLCHAR	0140	/* internal value used for end-of-line */
#define	EOFCHAR	07417	/* end-of-file character (not currently used) */

		/* the device registers */

struct {
	int crcsr;
	int crbuf1;
	int crbuf2;
};
#define	CRADDR	0177160

		/* values defined on the crcsr field */

#define     RDRENB	0000001
#define     EJECT	0000002
#define     IENABLE	0000100
#define     COLDONE	0000200
#define     READY	0000400
#define     BUSY	0001000
#define     ONLINE	0002000
#define     TIMERR	0004000
#define     RDCHECK	0010000
#define     PICKCHECK	0020000
#define     CARDDONE	0040000
#define     ERROR	0100000

		/* card reader status */

struct {
	int crstate;
	struct {
		int cc;
		int cf;
		int cl;
	} crin;
} cr11;

		/* values defined over the crstate field */

#define	CLOSED	0
#define	WAITING	1
#define	READING	2
#define	EOF	3

char asctab[]
{
	' ','1','2','3','4','5','6','7',
	'8',' ',':','#','@','`','=','"',
	'9','0','/','s','t','u','v','w',
	'x','y',' ',']',',','%','_','>',
	'?','z','-','j','k','l','m','n',
	'o','p','q',' ','!','$','*',')',
	';','\\','r','&','a','b','c','d',
	'e','f','g','h',' ','[','.','<',
	'(','+','^','i',' ',' ',' ',' '
};

cropen(dev,flag)
{
	extern lbolt;

	if (flag!=0 || cr11.crstate!=CLOSED) {
	err:	u.u_error = ENXIO;
		return;
	}
	while(CRADDR->crcsr & (ERROR|PICKCHECK|RDCHECK|TIMERR|READY|BUSY)) {
		CRADDR->crcsr = 0;
		sleep(&lbolt,CRPRI);
		if(cr11.crstate != CLOSED) goto err; /* somebody else got it */
	}
	CRADDR->crcsr = IENABLE|RDRENB;
	cr11.crstate = WAITING;
}

crclose(dev,flag)
{
	spl6();
	CRADDR->crcsr = 0;
	while (getc(&cr11.crin) >= 0);
	cr11.crstate = CLOSED;
	spl0();
}

crread()
{
	register int c;

	do {
		spl6();
		while((c = getc(&cr11.crin))<0) {
			if(cr11.crstate == EOF)
				goto out;
			if((CRADDR->crcsr & CARDDONE) && (cr11.crin.cc<CRHIWAT))
				CRADDR->crcsr =| IENABLE|RDRENB;
			sleep(&cr11.crin,CRPRI);
		}
		spl0();
	} while (passc(asciicon(c)) >= 0);

out:
	spl0();
}

crint()
{
	if (cr11.crstate == WAITING) {
		if(CRADDR->crcsr & ERROR) {
			CRADDR->crcsr = IENABLE|RDRENB;
			return;
		}
		cr11.crstate = READING;
	}
	if(cr11.crstate == READING) {
		if (CRADDR->crcsr & ERROR)
			/*
			   This code is not really smart enough.  The acutal
			   EOF condition is indicated by a simultaneous
			   PICKCHECK and CARDDONE; anything else is a real
			   error.  In the event of a real error we should
			   discard the current card image, wait for the
			   operator to fix it, and continue.  This is very
			   hard to do.....
			*/
			cr11.crstate = EOF;
		else {
			if ((CRADDR->crcsr&CARDDONE) == 0) {
				putc(CRADDR->crbuf2,&cr11.crin);
				return;
			} else {
				cr11.crstate = WAITING;
				if (cr11.crin.cc < CRHIWAT)
					CRADDR->crcsr = IENABLE|RDRENB;
			}
		}
		putc(NLCHAR,&cr11.crin);   /* card end or EOF -- insert CR */
		wakeup(&cr11.crin);
	}
}

asciicon(c)
{
	register c1, c2;

	c1 = c&0377;
	if (c1 == NLCHAR)
		return('\n');
	if (c1>=0200)
		c1 =- 040;
	c2 = c1;
	while ((c2 =- 040) >= 0)
		c1 =- 017;
	if(c1 > 0107)
		c1 = ' ';
	else c1 = asctab[c1];
	return(c1);
}
