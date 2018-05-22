#
#include "param.h"
#include "user.h"
#include "buf.h"
#include "net_net.h"
#include "net_netbuf.h"
#include "net_ill.h"
#include "net_imp.h"
#include "file.h"
#include "net_ncp.h"
#include "net_contab.h"



/*name:
	imp_init
function:
	to initialize the imp interface hardware

algorithm:
	reset the imp interface
	start strobing the host master ready bit
	enable interrupts

parameters:
	none

returns:
	nothing

globals:
	lbolt
	HMR_interval=
	IMPADDR-> status regs

calls:
	set_HMR
	sleep	(sys)

called by:
	impopen	(impio.c)

history:
	initial coding 10/5/75 S. F. Holmgren
	Recoded for modularity 15Feb77 JSK

*/

imp_init()
{
	int i;
	extern lbolt;
	

	IMPADDR->omaint =| omreset;
	/* reset any input extended memory bits */
	IMPADDR->imaint = IMPADDR->omaint = 0;
	HMR_interval = 60;
	set_HMR ();	/* start strobing host master ready */
	while( IMPADDR->imaint&imimprdy == 0)
		sleep (&lbolt, 0);	/* until imp says is up */

	/* enable interrupts */
	IMPADDR->istat = isintenb;
	IMPADDR->ostat = osintenb;


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
	impotab.d_active
	IMPADDR-> output regs
	(buffer)

calls:
	printf		(sys)

called by:
	impstrat	(impio.c)
	imp_odone	(impio.c)

history:
	initial coding 1/7/75 by S. F. Holmgren

*/

#ifdef IMPPRTDEBUG
int imp_o_debug;
# endif

imp_output()
{
	register struct buf *bp;
	register char *endaddr;
	register char *addr;

	
	if((bp = impotab.d_actf) == 0)
		return;			/* return nothing to do */

	impotab.d_active++;		/* something to do set myself active */

	/* set or reset disable endmsg according to user wishes */
	IMPADDR->ostat = osintenb | (bp->b_blkno ? osdiseom : 0) ;

	endaddr = bp->b_addr + bp->b_wcount;	/* generate second address */
	/*
	 * imp interface expects address of last word to be transferred.
	 * if low order bit is on, only 1 byte of last word is sent. Blech !
	 * NOTE: bp->b_wcount is really a byte count
	 */
	endaddr = endaddr&01 ? endaddr : endaddr-2;

	IMPADDR->spo = bp->b_addr;	/* load start addr */
	IMPADDR->epo = endaddr;		/* load endaddr and go */

#ifdef	IMPPRTDEBUG
	if (imp_o_debug) {
		printf ("IMP: ");	/* print first 24 bytes in olctal */
		addr = bp->b_addr;
		endaddr = bp->b_addr + bp->b_wcount-1;
		if (endaddr > addr+24)
			endaddr = addr+24;
		for (;addr<endaddr;addr++)
			printf ("%o ", *addr & 0377);
		printf ("\n");
	}
#endif
	
}



/*name:
	impread

function:
	to apply an input area to the input side of the imp interface

algorithm:
	inhibit interrupts
	calculate transfer parameters and save in globals
	load interface with saved parameters
	load end input register with address+size passed

parameters:
	addr - start address in which to store imp input data
	size - available number of bytes in which to store said data

returns:
	nothing

globals:
	impi_adr=
	impi_wcnt=

calls:
	spl_imp

called by:
	flushimp	(impio.c)
	ihbget		(impio.c)
	impopen		(impio.c)
	hh		(impio.c)
	ih		(impio.c)

history:
	initial coding 1/7/75 by S. F. Holmgren

*/

impread( addr,size )		/* start a read from imp into data area */
{
	spl_imp ();			/* not to be interrupted, please */
	impi_adr = addr;
	impi_wcnt = size>>1;
	IMPADDR->spi = addr;
	IMPADDR->epi = addr + size;
	

}

/*name:
	ill_oint

function:
	This is the imp output interrupt.

algorithm:
	Check for unexpected interrupts from the imp
	Check for errors
	call impodone

parameters:
	none

returns:
	nothing

globals:
	impotab.d_active
	IMPADDR->ostat

calls:
	imp_odone	(impio.c)
	printf		(sys)

called by:
	hardware interrupt

history:
	initial coding 1/7/75 by S. F. Holmgren
	recoded 15Feb77 for modularity JSK

*/

ill_oint()
{
	char errbits;

	if (impotab.d_active == 0) /* ignore unexpected interrupts */
		return;

	errbits = 0;
	if( IMPADDR->ostat & ostimout )	/* timeout errror */
	{
		errbits++;
		printf ("IMP: Timeout\n");
	}
	imp_odone (errbits);
}
/*
name:
	imp_iint

function:
	Wakes up the input process

algorithm:
	calulate device dependant values of msglen end of msg and errors
	wakes up input process

parameters:

	none


returns:
	nothing

globals:
	imp_stat.error=
	imp_stat.inpendmsg=
	imp_stat.inpwcnt=

calls:
	wakeup		to start input handling process running

called by:
	hardware interrupt

history:
	coded by Steve Holmgren 04/28/75
	Recoded for modularity 15Feb77 JSK


*/

ill_iint()
{
	imp_stat.error = IMPADDR->istat & iserr; /* currently not filled in */
	imp_stat.inpendmsg = IMPADDR->istat & isendmsg;
	/* delvelop remainder word count, ala dec interface */
	imp_stat.inpwcnt = (IMPADDR->istat & isbufful) ?
		0 : ((IMPADDR->spi - impi_adr) >>1) - impi_wcnt;
	wakeup( &imp );
}
/*name:

/*name:
	set_HMR

function:
	To repeatedly tickle the host master ready bit of the imp interface

algorithm:
	if someone hasnt set HMR_interval to zero ( should i go away?? )
		set the host master ready bit
		setup timeout so that i am called again
	else
		wakeup the guy who reset HMR_interval

parameters:
	none

returns:
	nothing

globals:
	HMR_interval
	IMPADDR->omaint=	(host master ready)

calls:
	timeout(sys)

called by:
	imp_init
	timeout(sys)

history:
	initial coding 1/7/75 by S. F. Holmgren

*/

set_HMR()
{

	if( HMR_interval )
	{
		IMPADDR->omaint =| omhstrdy;
		timeout( &set_HMR,0,HMR_interval);	/* restart */
	}else{
		wakeup (&HMR_interval);
	}
}

/*
	spl_imp

function:
	centralize priority setting of imp because of variations
	in interface configurations

algorithm:
	call correct kernel procedure

parameters:
	none

returns:
	nothing

globals:
	none

calls:
	spl?

called by:
	anyone who fiddles with things that get changed by imp interrupts :

spl_imp	acc.c	impread
	
	contab.c entconta
	ill.c	impread
	
	impio.c	impopen
		impstrat
	kerbuf.c bytesout    
		catmsg
		freeb
		freebuf
		freekb
		getbuf
		getkb
		linkbuf
	ncpio.c	ncpclose
		ncpread	{      
		wf_setup
	nopcls.c netopen
	nrdwr.c	netread
		netsleep
		netwrite
		sendallo


history:
	Added 1 Feb 77 for compatibility with other imp-drivers,
		by JC McMillan

*/

spl_imp()
{
	spl4();	/* just high enuf to disalbe interrupts from imp iface */
}
/*name:
	imp_reset

function:
	reset interface

algorithm:
	reset host_master ready. Reset all other bits. Wait
	on HMR_interval insures that host_master_ready does not
	get reset by the clock routine

parameters:
	none

returns:
	nothing

globals:
	HMR_interval=
	IMPADDR->omant=

calls:
	printf	(sys)
	sleep	(sys)

called by:
	imp_dwn		(impio.c)
	impopen		(impio.c)

history:
	initial coding 08 Jan 77 by JSK

*/

imp_reset()
{
	printf("\nIMP:Reset\n");

	spl6 ();
	HMR_interval = 0;
	sleep (&HMR_interval);
	IMPADDR->omaint =| omreset;
}
