#include "../../h/param.h"
#include "../../h/buf.h"
#include "../../h/user.h"
/* DQ 11aa device driver */

#define not	~	/* so that I will be able to see not on the vistar. */


struct 
{
	int dq_rcsr;	/* receive csr */
	int dq_tcsr;	/* transmit csr */
	int dq_err;	/* err/sel reg */
	int dq_sec;	/* secondary reg */
};

struct
{
	int   dq_state;
		/* the possible values for dq_state are: */

#define DQCLOSED	0	/* closed state */
#define DQREADING	1	/* waiting for data */
#define DQWRITE		2	/* writing data */
#define DQCTSWAIT	3	/* wait for CLEAR to SND inter */

	char *dq_wbuff;
	char *dq_wbfp;
	char *dq_rbuff;
	char *dq_rbfp;
	int   dq_rdcnt;
	int   dq_txcnt;
	int   dq_erreg;
	int   dq_rcvreg;
}dq11;

#define DQADDR		0160050

#define BUFLEN		512	/* the length of read and write buffers */

/* register bit assignments */


/* Rx CSR register alias dq_rcsr */

#define GO		01	/* go bit */
#define STRIPSYNC	02	/* strip sync */
#define PRIMSEC		04	/* primary/secondary in use */
#define HFDUX		010	/* half duplex mode */
#define CHARIE		020	/* character match inter enable */
#define DONEIE		040	/* done enable */
#define SECDONE		0100	/* secondary done */
#define PRIMDONE	0200	/* primary done */
#define CHARDET		07400	/* character detected */
#define RCVACTIVE	010000	/* rcv active */
#define CHARFND		0100000 /* character matched */

/* TX CSR alias dq_tcsr */

#define ERRIE		010	/* err inter enable */
#define DATASETIE	020	/* data set inter enable */
#define REQTOSND	0400	/* request to send */
#define DTATERMRDY	01000	/* data terminal rdy */
#define DTASETRDY	02000	/* data set ready */
#define CARRIER		010000	/* carrier */
#define CLRTOSND	020000	/* clear to send */
#define DTASET		0100000 /* data set interrupt */
#define TRANPRIM	01000	/* transmit primary BA reg */
#define TRANSEC		03000	/* transmit secondary reg */
#define RCVPRIM		00000	/* rcv primary BA reg */
#define RCVSEC		02000	/* rcv secondary BA reg */



/* miscelaneous definitions */

#define MASTERCLR	040	/* master clear */
#define BITS8		04000	/* eight bits per character */

/* REG/ERR alias dq_err */

#define TXCLKLOSS	01	/* transmit clock loss */
#define RXCLKLOSS	02	/* rcv clock loss */
#define TXLATENCY	04	/* transmit latency */
#define RXLATENCY	010	/* rcv latency */
#define TXNXMEM		020	/* transmit non existant mem */
#define RXNXMEM		040	/* rcv non existant mem */
#define RXBCC		0100	/* rcv bcc check */
#define RXVRC		0200	/* rcv vrc check */
#define CHARCNT		0400	/* offset from a BA to get CC reg */
#define DQMISC		05000	/* misc register */
#define DQSYNC		04400	/* dq sync reg */
#define MXWRTIE		010000	/* memory extension wrt enable */
#define ERRIE		0100000 /* error inter enable */

/* a couple of EBCDIC characters */

#define SYN		062	/* synch character */
#define ETB		046	/* end of text character */
#define PAD		0377	/* padding character--generates an interrupt */

#define DQPRI		1	/* priority for sleeping */

dqopen()
{
	register int *dq;
	register char *reg;

	dq = DQADDR;
	if( dq11.dq_state != DQCLOSED )
	{
		u.u_error = ENXIO;
		return;
	}

	/* get buffers */
	if( ++dq11.dq_wbuff == 1  )
	{
		dq11.dq_wbuff = getblk( NODEV );
		dq11.dq_wbfp = dq11.dq_wbuff->b_addr;
		dq11.dq_rbuff = getblk( NODEV );
		dq11.dq_rbfp = dq11.dq_rbuff->b_addr;
	} /* should this } be moved down to the end of the routine? */

	/* initialize read count */
	dq11.dq_rdcnt = -BUFLEN;
	/* master clear the interface */
	dq->dq_err = DQMISC;
	dq->dq_sec =| MASTERCLR;

	/* clear rest of the secondary registers */
	reg = 0;
	while( reg <= DQMISC )
	{
		dq->dq_err = (reg|MXWRTIE);	/* select reg */
		dq->dq_sec = 0;			/* clr selected reg */
		reg =+ CHARCNT;			/* inc to next register */
	}

	/* set bits per character */
	dq->dq_sec = BITS8;			/* 8/per */

	/* set sync register */
	dq->dq_err = DQSYNC;			/* select sync reg */
	dq->dq_sec = (SYN<<8|SYN);

	/* set up read side */
	dqrstart( dq11.dq_rbfp, -BUFLEN );
	/* set data set enable & data term rdy */
	dq->dq_tcsr = (DTATERMRDY | DATASETIE);
	/* take a data set interrupt here
	   since dq_state is not equal to DQREAD, dqturnaround
	   will load up for reading 
	 */
}

dqclose()
{
	register int *dq;

	dq = DQADDR;
	/* master clear things */
	dq->dq_err = DQMISC;
	dq->dq_sec = MASTERCLR;

	/* release buffer */
	if( dq11.dq_wbuff != 0 )
	{
		brelse( dq11.dq_wbuff );
		dq11.dq_wbuff = 0;
		brelse( dq11.dq_rbuff );
		dq11.dq_rbuff = 0;
	}
	dq11.dq_state = DQCLOSED;
}

dqread()
{
	spl5();
	while( dq11.dq_rdcnt == -BUFLEN )
	{
		sleep( &dq11,DQPRI );
	}
	spl0();
	iomove( dq11.dq_rbuff,0,min( u.u_count,(BUFLEN+dq11.dq_rdcnt)),B_READ );
	dq11.dq_rdcnt = -BUFLEN;			/* reset count */
	/* set up read side again */
	dqrstart( dq11.dq_rbfp, -BUFLEN );
}

dqwrite()
{
	if( u.u_count == 0 ) return;
	spl5();
	while( DQADDR->dq_rcsr&RCVACTIVE )
	{
		sleep( &dq11,DQPRI );
	}
	spl0();
	if( u.u_count > BUFLEN ) u.u_count = BUFLEN;
	dq11.dq_txcnt = -u.u_count;
	iomove( dq11.dq_wbuff,0,u.u_count,B_WRITE );
	dqwstart();
}

dqwstart()
{
	register int *dq;

	dq = DQADDR;

	/* select transmit addr reg */
	dq->dq_err = (((dq->dq_tcsr&PRIMSEC) ? TRANSEC : TRANPRIM)|MXWRTIE);
	/* load bus addr reg */
	dq->dq_sec = dq11.dq_wbfp;
	/* select character count */
	dq->dq_err =| CHARCNT;
	/* load CC */
	dq->dq_sec = dq11.dq_txcnt;
	/* set state to waiting for clear to send inter */
	dq11.dq_state = DQCTSWAIT;
	/* set dta set ie & RTS */
	dq->dq_tcsr =| (DATASETIE | REQTOSND);
}

dqrint()	/* Interrupts are generated by transmission errors, a PAD
		 * encountered on input, or a full buffer.
		 */
{
	register int *dq;	/* will point to DQADDR, the first dq11 register */
	register char *bufptr;	/* used to point into the read buffer */

	dq = DQADDR;

	/* perhaps the order of the if tests in this routine could be
	 * altered to take into account the frequency of various events
	 */

	if( dq->dq_rcsr & (PRIMDONE | SECDONE ) ) /* have we transfered BUFLEN bytes? */
	{ /* yes */
		dq11.dq_rdcnt = 0; /* tell this to dqread() */
		wakeup( &dq11 );
	}
	else
	if( dq->dq_err < 0 )			/* err bit on? */
	{
		printf("%o\n",dq->dq_err);
		dq->dq_err =& not (ERRIE|RXBCC|RXVRC|RXNXMEM|TXNXMEM|RXLATENCY|TXLATENCY|RXCLKLOSS
				|TXCLKLOSS);	/* clear possible errors */
		wakeup( &dq11 );
	}
	else	/* no error */
	{ /* we found a PAD char; now check for the end of the block */
		dq->dq_err = dq11.dq_rcvreg;	/* select addr reg */
		bufptr = dq->dq_sec - 3;	/* get addr-3 */
		dq->dq_err =| CHARCNT;		/* select the count reg */

		/* The PAD character, which is at bufptr-1 if it is anywhere, is
		 * not sufficient to insure end of block.  We need to have
		 *
		 *	 ETB crc crc		or	ETB crc crc PAD
		 *
		 * before we can dispatch dqread().
		 * Note that the PAD char will not be in the buffer yet if it
		 * is to be put in a byte at an even address.  This is the
		 * reason for the first sequence above.
		 */

		/* can we wake dqread;  must have read at least 6 characters
		 * before we will entertain any thoughts of ETBs and PADs et.al.
		 */

		if( dq->dq_sec > -506 &&  *bufptr != ETB && *--bufptr != ETB  )
		{
			/*dq->dq_rcsr =& not CHARFND; /* clear only CHARFND */
			bufptr =+ 3;
			if (*bufptr++ != PAD) *bufptr++ = PAD; /* put that damn PAD there */
			dqrstart( bufptr, dq->dq_sec );
		}
		else
		{ /* Yes, we are done with this block */
		   dq->dq_rcsr = 0;	/*clear done,char flgs & all others*/
		   dq11.dq_rdcnt = dq->dq_sec;	/* tell dqread() we're done */
		   wakeup( &dq11 );
		}
	}

}

dqxint()
{
	register int *dq;
	register int *tcsr;
	register int done;

	dq = DQADDR;
	tcsr = &DQADDR->dq_tcsr;

	if( *tcsr < 0 ) 	/* data set flag */
	{
		*tcsr =& not DTASET;	/* turn off data set flag */
		if( *tcsr&CLRTOSND && dq11.dq_state == DQCTSWAIT )
		{
			/* got a clear to send inter so set trans start */
			dq11.dq_state = DQWRITE;
			*tcsr = (GO|ERRIE|DONEIE|DTATERMRDY);
			return;
		}
	}
	if( (done = (*tcsr&( PRIMDONE|SECDONE ))) != 0 )
	{
		*tcsr =& ~(done|DONEIE);
	}
	if( dq->dq_err < 0 )		/* error bit on? */
	{
		printf("%o\n",dq->dq_err);
		DQADDR->dq_err =& not (ERRIE|RXBCC|RXVRC|RXNXMEM|TXNXMEM|RXLATENCY|TXLATENCY|RXCLKLOSS
				|TXCLKLOSS);	/* turn off err bits */
	}

}

dqrstart( addr, count )
int addr,	/* address of buffer (ba) */
    count;	/* minus the number of bytes to transfer */
{
	/* set the interface up for reading */
	register int *dq;

	dq = DQADDR;

	/* save current rcv BA reg */
	dq11.dq_rcvreg = (dq->dq_rcsr&PRIMSEC) ? RCVSEC : RCVPRIM;
	/* select rcv ba reg */
	dq->dq_err = (dq11.dq_rcvreg|MXWRTIE);
	/* set rcv ba reg */
	dq->dq_sec = addr;
	/* set character count */
	dq->dq_err =| CHARCNT;

	/* set err intr enable */
	dq->dq_tcsr =| ERRIE;

	dq->dq_sec = count;
	/* load go, done ie, char ie, half dux, strip syn */
	/* and make sure that Rx active is not cleared in */
	/* case it is on				  */
	/* we may have to "&" a few zeros into dq_rcsr in */
	/* order to be cool.			      */
	dq->dq_rcsr =& not (HFDUX|SECDONE|PRIMDONE|CHARDET|CHARFND);
	dq->dq_rcsr =| (GO|STRIPSYNC|CHARIE|DONEIE);
}

