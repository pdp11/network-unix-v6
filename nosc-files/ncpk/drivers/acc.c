#
#include "param.h"
#include "user.h"
#include "buf.h"
#include "net_net.h"
#include "net_netbuf.h"
#include "net_acc.h"
#include "net_imp.h"
#include "file.h"
#include "net_ncp.h"
#include "net_contab.h"

#ifdef IMPPRTDEBUG
int impodebug;
#endif	/* JC McMillan */

/*name:

	imp_init
function:
	to initialize the imp interface hardware

algorithm:
	reset the imp interface
	strobe host master ready
	wait for imp to be ready
	enable interrupts
	clear any extended memory address bits

parameters:
	none

returns:
	nothing

globals:
	lbolt		for sleep
	IMPADDR-> status regs

calls:
	imp_reset
	sleep	(sys)


called by:
	impopen		(impio.c)

history:
	initial coding 10/5/75 S. F. Holmgren
	Modified Jan77 for LH-DH-11 by JSK&JCM
	Recoded for modularity 15Feb77 JSK

*/

imp_init()
{	extern lbolt;
	int i;
	
	/* reset the imp interface */
	imp_reset();			/* JSK */
	IMPADDR->istat = ihmasrdy; /* close relay */
	while ( ! (IMPADDR->istat & ihstrdy) )
		sleep( &lbolt, 0 );	/* wait for relay JSK&JCM */
	while( IMPADDR->istat&imntrdy ) 
		sleep( &lbolt, 0 );	/* until imp says is up JSK&JCM */

	/* enable interrupts and enable memory access */
	IMPADDR->istat =| (iienab | iwren);
	IMPADDR->ostat = oienab;

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
	load count & set go

parameters:
	none

returns:
	nothing

globals:
	impotab.d_actf
	impotab.d_active
	(buffer)

calls:
	prt_dbg			prints debugging msg on terminal

called by:
	impstrat	(impio.c)
	imp_odone	(impio.c)


history:
	initial coding 1/7/75 by S. F. Holmgren
	Modified 05Jan77 for LH-DH-11 by JSK

*/

imp_output()
{
	register struct buf *bp;
	register char *p;
	int i;

	
	if((bp = impotab.d_actf) == 0)
		return;			/* return nothing to do */

	impotab.d_active++;		/* something to do set myself active */

	/* set or reset disable endmsg according to user wishes */
	IMPADDR->ostat = oienab;	/* JSK */
	if( bp->b_blkno )	/* is last bit of message in this buf JSK */
		IMPADDR->ostat =& ~ oenlbit;
	else
		IMPADDR->ostat =| oenlbit;


	IMPADDR->spo = bp->b_addr;	/* load start addr */
	IMPADDR->owcnt = -(bp->b_wcount >> 1);		/* load end addr */
	IMPADDR->ostat =& ~ (omrdyerr | otimout);	/* reset latches JSK */
	IMPADDR->ostat =| ogo;		/* go JSK */
	
#	ifdef IMPPRTDEBUG
		if (impodebug) prt_dbg("<send>", bp->b_wcount<15?bp->b_wcount:15, 1, bp->b_addr);
#	endif	/* JC McMillan */
}

/*name:
	impread

function:
	to apply an input area to the input side of the imp interface

algorithm:
	disable interrupts
	plug in address & count
	reset error bits
	set go

parameters:
	addr - start address in which to store imp input data
	size - available number of bytes in which to store said data

returns:
	nothing

globals:
	impi_adr=		stores adr last loaded into dev. reg.
	impi_wcnt=		stores wordcount used for imp-read.
	IMPADDR->...=		set to initiate imp read.

calls:
	printf	(sys)
	spl_imp

called by:
	flushimp	(impio.c)
	ihbget		(impio.c)
	impopen		(impio.c)
	hh		(impio.c)
	ih		(impio.c)

history:
	initial coding 1/7/75 by S. F. Holmgren
	Modified 05Jan77 for LH-DH-11 by JSK

*/

impread( addr,size )		/* start a read from imp into data area */
{
register int imperr;

	spl_imp();			/* make sure im not interrupted */

	IMPADDR->spi = impi_adr = addr;	/* set last-read (impi_adr) as well as dev. reg.--JCM*/
	IMPADDR->iwcnt = -(impi_wcnt = size >> 1);	/* set last-read w-cnt (impi_wcnt) as well as dev.reg.--JCM*/

	if (imperr=			/* the bits which are an error-if-set */
		( IMPADDR->istat & (icomperr|itimout|imntrdy|imrdyerr|igo)) 	/*igo cld be dropped*/
	     |				/* comp(stat) & (the bits which must be set!) */
		(~IMPADDR->istat & (ihstrdy|iwren|idevrdy|iienab|ihmasrdy))
	   )
		printf("\nIMP:Rd(Csri=%o,errs:%o)\n", IMPADDR->istat, imperr);	/*JCM*/

		IMPADDR->istat =& 		/* reset latches JSK&JCM */
			((IMPADDR->istat&(iendmsg|ibfrfull))==(iendmsg|ibfrfull))
			  ? ~(imrdyerr|itimout) : ~(iendmsg|imrdyerr|itimout);

	IMPADDR->istat =| igo;			/* start it up JSK */
}

/*name:
	acc_oint

function:
	This is the imp output interrupt.

algorithm:
	Check for unexpected interrupts from the imp
	Determine if there were errors
	Invoke imp_odone

parameters:
	none

returns:
	nothing

globals:
	impotab.d_active

calls:
	imp_odone	(impio.c)
	printf (sys)

called by:
	Hardware interrupt

history:
	initial coding 1/7/75 by S. F. Holmgren
	Recoded for modularity 15Feb77 JSK

*/

acc_oint()
{
	register struct devtab *iot;
	char errbits;

	errbits = 0;
	iot = &impotab;

	if( iot->d_active == 0 )	/* ignore unexpected errors */
	{	printf("\nIMP:Phantom Intrp(Csro=%o)\n", IMPADDR->ostat);
		return;
	}

#ifdef	IMPPRTDEBUG
	if (iot=		/* the bits which are an error-if-set */
		(IMPADDR->ostat & (ocomperr|otimout|omrdyerr|obusback|ogo)) 	/*ogo cld be dropped*/
	|			/* comp(stat) & (the bits which must be set!) */
		(~ IMPADDR->ostat & (odevrdy|oienab))
	)
		printf("\nIMP:Out(Csro=%o,errs @%o)\n", IMPADDR->ostat, iot);
#endif
	


	if( IMPADDR->ostat & otimout )	/* timeout errror */
	{
		printf("\nIMP:Timeout\n"); /* timeout warning */
		errbits++;
	}
	imp_odone (errbits); /* could say what error some day */
}
/*
name:
	acc_iint

function:
	Wakes up the IMP input process

algorithm:
	calculate device depaendant values of msglen, endmsg, and errors
	wakeup input process

parameters:

	none


returns:
	nothing

globals:
	imp_stat.error=
	imp_stat.inpendmsg=
	imp_stat.inpwcnt=
	IMPADDR-> status regs

calls:
	wakeup (sys)		to start input handling process running
	printf

called by:
	hardware interrupt

history:
	coded by Steve Holmgren 04/28/75
	Recoded for modularity 15Feb77 JSK

*/

acc_iint()
{
	if (imp_stat.error =	/* the bits which are an error-if-set */
		( IMPADDR->istat & (icomperr|itimout|imntrdy|imrdyerr|igo)) 	/*igo cld be dropped*/
	     |			/* comp(stat) & (the bits which must be set!) */
		(~IMPADDR->istat & (ihstrdy|iwren|idevrdy|iienab|ihmasrdy))
	   )
	{
		printf ("\nIMP:In(Csri=%o, errs:%o)\n"
			,IMPADDR->istat
			,imp_stat.error);	/*JC McMillan*/
	}
	imp_stat.inpendmsg = 
		(IMPADDR->istat & (iendmsg|ibfrfull)) == iendmsg;
	imp_stat.inpwcnt = IMPADDR->iwcnt;
	wakeup( &imp );
}
/*
	spl_imp

function:
	centralize priority setting of imp because of variations
	in interface configurations

algorithm:
	invoke correct spl procedure in kernel

parameters:
	none

returns:
	nothing

globals:
	none

calls:
	spl5

called by:
	anyone who fiddles things that get changed by imp interrupts :


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
	Modified 05Jan77 for LH-DH-11 by JSK

*/

spl_imp()
{
	spl5();	/* acc interface interrupts at priority 5 */
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
	IMPADDR-> status regs

calls:
	printf	(sys)

called by:
	imp_dwn		(impio.c)
	imp_init
	impopen		(impio.c)

history:
	initial coding 08 Jan 77 by JSK

*/

imp_reset()
{
	printf("\nIMP:Reset\n");

	IMPADDR->istat = IMPADDR->ostat = 0;
	IMPADDR->ostat = obusback;	/* reset host master ready */
	IMPADDR->ostat = 0;
}
