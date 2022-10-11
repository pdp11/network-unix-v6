#
#include "../h/param.h"
#include "../h/netparam.h"
#include "../h/user.h"
#include "../h/buf.h"
#include "../h/net.h"
#include "../h/netbuf.h"
#include "../h/imp11a.h"
#include "../h/imp.h"
#include "../h/file.h"
#include "../h/ncp.h"
#include "../h/contab.h"

#ifdef NETWORK          /* jsq bbn 10/19/78 */
#define TRYING 3
/*name:
	imp_init
function:
	to initialize the imp interface hardware, and various software
	parameters.

algorithm:
	reset the imp interface
	start strobing the host master ready bit
	wait for imp to be ready
	wait for 3 NOPs to be sent

parameters:
	none

returns:
	an initialized imp

globals:
	HMR_interval
	imp_trying

calls:
	impreset
	set_HMR

called by:
	impopen

history:
	initial coding 10/5/75 S. F. Holmgren
	Modified for imp11a by Bob Walton, Lincoln Lab Jun 78
	Comments corrected by Alan Nemeth, BBN 28 Jul 78
*/

imp_init()
{
	extern lbolt;
	

	imp_reset ();
	HMR_interval = 60;
	set_HMR ();	/* start the clock */
	IMPADDR->istat = hmasrdy;
	while( IMPADDR->istat&impnotrdy)
		sleep (&lbolt, 0);	/* until imp says is up */
	imp_trying = TRYING;    /* ignore imnotrdy till 3 nops received */
}
/*name:
	imp_output

function:
	Takes buffers( see buf.h ) linked into the impotab active
	queue and applies them to the imp interface.

algorithm:
	check to see if the queue is empty if so return
	set the active bit
	enable output interrupts and the endmsg bit( imp interface manual)
	calculate then end address and perform the mod 2 function
	on the calculated address( again see imp interface manual)
	load the start output address register
	load the end output address register which is also an implied
	go

parameters:
	none

returns:
	nothing

globals:
	impotab.d_actf
	(buffer)

calls:
	nothing

called by:
	impstrat
	imp_oint

history:
	initial coding 1/7/75 by S. F. Holmgren

*/

#ifndef BUFMOD

imp_output()
{
	register struct buf *bp;

	
	if((bp = impotab.d_actf) == 0)
		return;			/* return nothing to do */

	impotab.d_active++;		/* something to do set myself active */

	IMPADDR->spo = bp->b_addr;	/* load start addr */
	IMPADDR->owcnt = - ((bp->b_wcount + 1) >> 1);
	if (bp->b_blkno)	IMPADDR->ostat = oienab + ogo;
	else			IMPADDR->ostat = oendmsg + oienab + ogo;

	
	if (impdiag >= 8) printf ("\nIMP OUT i=%o h=%o\n",
		IMPADDR->istat, bp->b_addr->integ);
}

#endif BUFMOD

#ifdef BUFMOD

imp_output()
{
	register struct netbuf *nbp;
	register addr;
	register struct buf *bf;

	if ( (nbp = impotab.d_actf) == 0 )
		return;

	impotab.d_active = 1;

	bf = &i11a_obuf;

	addr = nbp->b_loc;
	bf->b_xmem = ( (addr >> 10) & 077 );
	bf->b_addr = (addr << 6);
	bf->b_wcount = -( (nbp->b_len + 1) >> 1);
	mapalloc(bf);

	IMPADDR->spo = bf->b_addr;
	IMPADDR->owcnt = bf->b_wcount;
	if (nbp->b_resv & b_eom)
		IMPADDR->ostat = ( oendmsg + oienab + (bf->b_xmem << 4) + ogo);
	else
		IMPADDR->ostat = ( oienab + (bf->b_xmem << 4) + ogo);
}

#endif BUFMOD

/*name:
	impread

function:
	to apply an input area to the input side of the imp interface

algorithm:
	say not doing leader read
	load start input register with address passed
	load end input register with address+size passed

parameters:
	addr - start address in which to store imp input data
	size - available number of bytes in which to store said data

returns:
	nothing

globals:
	impleader=

calls:
	nothing

called by:
	flushimp
	ihbget

history:
	initial coding 1/7/75 by S. F. Holmgren

*/

#ifndef BUFMOD

impread( addr,size )		/* start a read from imp into data area */
{
	spl_imp ();			/* not to be interrupted, please */
	impi_adr = addr;
	impi_wcnt = size>>1;
	IMPADDR->spi = addr;
	IMPADDR->iwcnt = -impi_wcnt;
	IMPADDR->istat = hmasrdy + iienab + igo;
	

}

#endif BUFMOD

#ifdef BUFMOD

impread( addr, offset, size)
char *addr, *offset;
{
	register char *ad;
	register struct buf *bf;

	ad = addr;

	bf = &i11a_ibuf;

	bf->b_addr = ( (ad << 6) | offset );

	if ( bf->b_xmem =  ( (ad >> 10) & 077 ) ) {
		bf->b_wcount = -(size >> 1);
		mapalloc(bf);
	}

	spl_imp();
	impi_adr = ad;
	impi_offset = offset;
	impi_wcnt = size >> 1;

	IMPADDR->spi = bf->b_addr;
	IMPADDR->iwcnt = -impi_wcnt;
	IMPADDR->istat = hmasrdy + iienab + (bf->b_xmem << 4) + igo;
}

#endif BUFMOD


/*name:
	imp_oint

function:
	This is the imp output interrupt.
	Handles both system type buffers( buf.h ) and the network msgs
	If it finds that a net message is being sent, steps to the
	next buffer in the message and transmits that.
	If all buffers of a net message or a standard system buffer
	has been encountered, it is freed and the output side is
	started once again.

algorithm:
	Check for unexpected interrupts from the imp
	Get the first buffer from the output queue
	If there was an error in output, it is indicated, and the
	buffer returned.
	If the buffer embodies a network message( B_SPEC ) and
	the last buffer of that message has not been sent,
		Get to next buffer in net message
	        Update w_count for that buffer
		Update next buffer pointer(av_forw) so next
		time come through things will be easier.
		Decide whether to set endmsg bit on not.
		Start up output side
	otherwise
		get next buffer in output queue
		if this was a net message give it back to the system
		otherwise say that the system buffer( buf.h ) is done
	if there is nothing to do clean up a little
	otherwise
	start up the output side again.

parameters:
	none

returns:
	nothing

globals:
	impotab.d_active
	buffer->b_flags=
	buffer->b_error=
	buffer->b_addr=
	buffer->b_wcount=
	buffer->av_forw=
	buffer->b_blkno=
	impotab->d_actf=

calls:
	imp_output
	freemsg
	iodone

called by:
	system

history:
	initial coding 1/7/75 by S. F. Holmgren

*/

i11a_oint()
{
	register int errbits;

	if (impotab.d_active == 0) { /* ignore unexpected interrupts */
		if (impdiag) printf ("\nIMP OUT PHANTOM: o=%o i=%o\n",
			IMPADDR->ostat, IMPADDR->istat);
		return;
	}

	errbits = 0;
	if( IMPADDR->ostat < 0 )
	{
		errbits++;
		printf ("\nIMP OUT ERROR: o=%o i=%o\n",
			IMPADDR->ostat, IMPADDR->istat);
	}
	imp_odone (errbits);
}
/*
name:
	imp_iint

function:
	Simply wakes up the input handling process to tell something
	has arrived from the network

algorithm:
	calls wakeup

parameters:

	none


returns:
	i hope so

globals:
	imp_proc

calls:
	wakeup		to start input handling process running

called by:
	hardware

history:
	coded by Steve Holmgren 04/28/75


*/

i11a_iint()
{
	register int trying;
#ifdef BUFMOD
	register struct buf *bf;
#endif BUFMOD
#ifndef BUFMOD
	if (impdiag >= 8) printf ("\nIMP IN i=%o h=%o\n",
		IMPADDR->istat, impi_adr->integ);
#endif BUFMOD

	if ((IMPADDR->istat & iienab) == 0) return;

#ifdef BUFMOD
	/* free UNIBUS map */
	bf = &i11a_ibuf;
	if ( (impi_adr >> 10) & 077 )
		mapfree(bf);
#endif BUFMOD
	trying = imp_trying;

	if (imp_stat.inpendmsg = IMPADDR->istat & iendmsg)
	{
		if (trying) --imp_trying;
	}
	if ((imp_stat.error = (IMPADDR->istat & (ierror | irdyerr))) ||
		trying >= TRYING)
	{
		if (trying)
		{
			IMPADDR->istat = hmasrdy + irst;
#ifdef BUFMOD
			impread (impi_adr, impi_offset, impi_wcnt << 1);
#endif BUFMOD
#ifndef BUFMOD
			impread (impi_adr, impi_wcnt << 1);
#endif BUFMOD

		}
#ifndef BUFMOD
		if (impdiag) printf ("\nIMP IN ERROR: i=%o h=%o\n",
			IMPADDR->istat, impi_adr->integ);
#endif BUFMOD
		if (trying) return;
	}
	imp_stat.inpwcnt = IMPADDR->iwcnt;
	IMPADDR->istat = hmasrdy;
	/* iienab bit is on only if interrupt actually expected */
	wakeup( &imp );
}
/*name:
	set_HMR

function:
	To repeatedly tickle the host master ready bit of the imp interface

algorithm:
	if someone hasnt set HMR_interval to zero ( should i go away?? )
		check if hung in read with imp dead
		setup timeout so that i am called again

parameters:
	none

returns:
	nothing

globals:
	HMR_interval
	hostmaster ready bit=

calls:
	timeout(sys)

called by:
	impopen
	timeout(sys)

history:
	initial coding 1/7/75 by S. F. Holmgren

*/

set_HMR()
{

	if( HMR_interval )
	{
		if (IMPADDR->istat & irdyerr && IMPADDR->istat & iienab)
			i11a_iint();
		timeout( &set_HMR,0,HMR_interval);	/* restart */
	}else{
		wakeup (&HMR_interval);
	}
}

/*name:
	imp_reset

function:
	reset interface

algorithm:
	reset host_master ready. Reset all other bits

parameters:
	none

returns:
	nothing

globals:
	none

calls:
	nobody

called by:
	imp_dwn
	imp_init

history:
	initial coding 08 Jan 77 by JSK
	gave the sleep a priority and added spl0 to reset priority
	jsq bbn 10-18-78
*/

imp_reset()
{
	register char *impadd;
	printf("\nIMP:Reset\n");

	spl6 ();
	if (HMR_interval) {
		HMR_interval = 0;
		sleep (&HMR_interval, NETPRI);
	}
	impadd = IMPADDR;
	impadd -> istat = irst;
	impadd -> istat = 0;
	impadd -> ostat = orst;
	impadd -> ostat = 0;
	spl0();
}

#endif NETWORK

