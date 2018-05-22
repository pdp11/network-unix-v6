#
#include "param.h"
#include "user.h"
#include "buf.h"
#include "net_net.h"
#include "net_netbuf.h"
#include "net_imp.h"
#include "net_hosthost.h"
#include "proc.h"
#include "file.h"
#include "net_ncp.h"
#include "net_contab.h"


#ifdef IMPPRTDEBUG
int	impdebug 0;
#endif	/* JC McMillan */

struct netbuf *rathole;		/* net buffer for discards */

#ifdef SCCSID
/* SCCS PROGRAM IDENTIFICATION STRING */
/*char id_impio[] "~|^`impio.c\tV3.9E1\t25Jan78\n";*/
#endif SCCSID

/* this array is indexed by the above op codes to give the length of the command */

char hhsize[14]
{
	1,		/* nop */
	10,		/* rts */
	10,		/* str */
	9,		/* cls */
	8,		/* all */
	4,		/* gvb */
	8,		/* ret */
	2,		/* inr */
	2,		/* ins */
	2,		/* eco */
	2,		/* erp */
	12,		/* err */
	1,		/* rst */
	1		/* rrp */
};

/* this array holds the rfnm wait bits for sending allocations to hosts */
char host_map[32];


/*name:
	impopen

function:
	imp input process

algorithm:
	fork a kernel process to handle input from net
	child:
		close parents files
	loop:
		init imp interface on first pass thru loop
		and send the noops
		read header
		if no error,
		call parse routine, which will read in rest of message
		else restart interface
		go to loop

parameters:
	none

returns:
	nothing

globals:
	ncpopnstate

calls:
	imp_init
	sleep(sys)
	newproc(sys)
	impread
	imp_input
	imp_reset
	impopen
	impstrat
	spl_imp

called by:
	ncpopen
	(ncpclose	awakens this code)
	(imp_iint	awakens this code)

history:
	initial coding 1/7/75 by S. F. Holmgren
	modified 10/5/75 chopped initialize into imp_init
	and started checking imp_error bit and inited if
	bit was on S. F. Holmgren
	modified 04 Jan 77 for ACC LH-11 by JSK
	modifed 25Jan78 to keep smalldaemon from returning at bottom

*/
impopen()
{
	register int *fp, *p;
	int needinit, inx;

	/*
	 * shouldn't we check to see that we don't get in here twice ?
	 */

	rathole = getbuf();

	if (newproc()) {	 /* fork input process */
		/* release any hold we may have on parents files */
		for( fp = &u.u_ofile[0]; fp < &u.u_ofile[NOFILE]; fp++ )
			if( (p = *fp) != NULL ){
				p->f_count--;
				*fp = 0;
			}

		needinit = 1;	/* flag to force initialization */
		for (;;)	/* body of input process */
		{
			spl_imp();
			if (needinit) {
				needinit = 0;
				imp_init ();
				for ( inx = 0; inx < 3; inx++ ) { /* send 3 noops */
					oimp.o_type = ih_nop;	/* set opcode to h_i_nop */
					impobuf.b_addr = &oimp;	/* set addr for output */
					impobuf.b_wcount = 4;	/* send the four imp leader bytes */
					impstrat( &impobuf );	/* send one */
					iowait( &impobuf );			/* wait for one to fin */
				}
				/* set parameters on input leader */
				imp.nrcv = ncp_rcv;	/* perm receive comm here */
				printf("\nIMP:Init\n");	/*JCM*/
				impread(&imp.type, 8);	/* set up input side of imp interface */
			}
			sleep( &imp,-25 );	/* wait for something */
			if( ncpopnstate == 0 ) break;	/* we down? */
			if (imp_stat.error) {
				printf ("\nIMP: input error, resetting\n");
				imp_reset ();
				needinit++;
			}else{
				imp_input();		/* handle it */
			}
		}
		imp_dwn();		/* clean up kernel data */
		exit ();	/* imp input process never returns */
	}
}

/*name:
	imp_input

function:
	Started in response to a wakeup from the input interrupt
	initiates and switches all transfers received from the imp.

algorithm:
	There are two logical states -
	1. The driver will setup an 8 byte read from the imp when
	   it is in the leader state evidenced by impi_adr == &imp.type
	   even if there is more data to be received from the imp
	   the interface will interrupt and come here for decoding.

	2. After a leader has been received and the endmsg bit of 
	   interface is not sent, the driver falls into a data
	   buffering mode reloaded by ihbget until the endmsg
	   flag is raised.


	if buffering data
		call hh let him handle it
	else
		if leaderread and endmsg means imp to host msg received
			call ih and let him deal with that type
		else
			must be a normal leader with further data waiting in imp
			is imp pad byte != 0
				yes error do stats and flush imp
			else
				legal leader
				if hh protocol msg (imp.link == 0)
					set flag
					continue buffering data
				else
					if host & link number are in conntab
						then set ptr to associated inode
						and continue reading data
					else
						error flush imp 

parameters:
	none

returns:
	nothing

globals:
	struct imp		struct for accessing various imp leader fields
	impi_sockt=		set to addr of user inode
	impi_addr

calls:
	hh			to handle hh protocol msgs
	ih			to handle imp to host protocol msgs
	flushimp		to keep reading data until endmsg is on
	ihbget			to reload imp with fresh imp data buffer
	imphostlink		map host link number in conn tab to sktp ptr
	printf
	prt_dbg			prints debugging msg on terminal
	imp_input
	swab	(sys)


called by:
	imp_open

history:

	initial coding 1/7/75 by Steve Holmgren
	Modified by J.C.McMillan for LH-DH-11 Jan77
	Modified for modularity 04Jan77 JSK
	Suppressed Link # ? message 5Jun77 J.S.Kravitz
*/
imp_input()	/* imp input handler */
{
	register char *c;		/* does unsigned arith & general logic*/
	register char *errtext;		/* pts to 1 of several texts for errmsg */


		/* Each msg begins with a leader of 4 (imp-to-host) or
		**  9 (host-to-host) bytes.  The first read of a msg
		**  (ie, where impi_adr== &imp.type) looks at up-to the first
		**  8 bytes: a completed read indicates an I-to-H
		**  leader, and an incomplete read indicates a H-to-H
		**  where 1 byte (discardable) will be picked up on the
		**  next IMP buffer.

		** On further IMP-reads (ie, when impi_adr!= &imp.type) the
		**  hh() code takes over and concatenates the buffers.
		*/

	if (impi_adr != &imp.type)	/*...if the leader is not being read... */
		hh();		/*    data is being buffered */

		/***************************************************
		**  Prev. line called on hh() to process data     **
		**    which follows leaders:			  **
		**    Lines below process only leaders (IH/HH).   **
		***************************************************/

	else			/*...an 8 byte leader-read is being processed*/
	if (imp_stat.inpendmsg)
		ih();		/* if 'endmsg', a 4 byte IH leader is assumed */

	else			/* ...else a 9 byte HH leader is assumed*/
	if (imp.pad1 != 0)	/* if illegal leader */
	{
		errtext = "Pad error";   /* JC McMillan */
	    iherr:
		impi_sockt = 0;
		imp_stat.i_leaderr++;
		if (errtext)
			printf("\nIMP:%s\n", errtext);
#		ifdef IMPPRTDEBUG
			prt_dbg(" ", 2*(impi_wcnt+imp_stat.inpwcnt), -1, &imp.type);
#		endif	/* JC McMillan */
		flushimp();
	}

	else			/* Pad valid on HH leader */
	if (imp.link == 0)	/* if a HH protocol (link==0) */
	{	if (imp.bsize!=8 || (c=swab(imp.bcnt))>120)
		{	errtext = "HH Ldr format";   /* JC McMillan */
			goto iherr;
		}
		impi_sockt = 0;
	    nxtbuf:
#		ifdef IMPPRTDEBUG
			prt_dbg("HHl", 2*(impi_wcnt+imp_stat.inpwcnt), 3, &imp.type);   /* print 8 bytes of leader*/
#		endif	/* JC McMillan */
		ihbget();
	}

	else	/*... must be HH data msg, and link must be validated */
	if( impi_sockt = imphostlink( imp.link & 0177 ))
		goto nxtbuf;	/* got iptr cont reading */

	else	/*All else has failed! Assume HH data msg w/ invalid link*/
	{
		errtext = 0;	/* JSK 5Jun77 */
		/* errtext = "Link#?"; */
		goto iherr;	/* whos he err */
	}
}
/*name:
	hh

function:
	If data received was not an imp leader to continue buffering
	until endmsg is raised on imp interface, once raised, msg is
	switched either to user or passed to hh1 for further decoding
	and possible switching.

algorithm:
	if endmsg raised
		if flushing imp
			discard data 
			reset flushing switch
			if data for user
				indicate error 
				allow him to run
		else
		not flushing so data is either for ncp daemon or user
				decrement users byte allocation by msg size
				link msg to user msg queue
				increment users msg queue byte total
				let user run
			msg is a host to host msg call hh1 for further decoding
		start another leader read
	else
		endmsg not set so either keep buffering or flushing
		if flushing
			keep flushing
		else
			keep reading

parameters:
	none

returns:
	nothing

globals:
	impi_msg->b_len=
	impi_flush=
	sktp->r_flags=
	sktp->r_bytes=
	sktp->r_msgs=
	sktp->r_msgq
	sktp->r_qtotal=
	imp.nrcv

calls:
	freemsg			to destroy buffer data
	wakeup (sys)		to allow user to run
	rmovepad		to remove imp padding and set length
	to_ncp			to pass data to ncpdaemon
	hh1			for further decoding and switching of data
	impread			to start another leader read
	flushimp		to continue flushing data from imp
	ihbget			continue bufferin data
	prt_dbg			prints debugging msg on terminal
	catmsg			assemble incoming message
called by:
	imp_input

history:

	initial coding 1/7/75 by Steve Holmgren
	check for something in impi_msg 8/16/76 S. Holmgren
	Modified Jan77 for LH-DH-11 by JSK& JC McMillan 
	server telnet test stuff deleted 12Jun77 JSKravitz
*/

hh()
{
	register struct rdskt *sktp;
	register char *src;
	register char *dest;
	struct netbuf *bufp;
	int cnt;

	sktp = impi_sockt;
	

	if( impi_msg )		/* if there is anything in the msg */
		impi_msg->b_len = net_b_size;
			/* The above sets a maximum bound on the number of
			**	characters in the buffer... rmovepad will
			**	more accurately set this length.        */

	if (imp_stat.inpendmsg)
	{
		if( impi_flush ) 		/* finished flushing */
		{
#			ifdef IMPPRTDEBUG
				prt_dbg("   ^", net_b_size+2*imp_stat.inpwcnt, -1, rathole);
#			endif	/* JC McMillan */

			freemsg( impi_msg );	/* destroy any bfred data */
			impi_msg = 0;		/* get rid of previous msg */
			impi_flush = 0;	/* reset flushing */
			if (sktp)		/* was user proc involved */
			{
				sktp->r_flags =| n_eof;	/* set err */
				wakeup( sktp );
			}
		}
		else
		{
			/* destroy the first data byte, its part of the leader */
			bufp = impi_msg->b_qlink;
			bufp->b_len--;
			src = dest = &bufp->b_data[0];
			src++;
			cnt = net_b_size+1;
			while( --cnt )	*dest++ = *src++;

			rmovepad();		/* remove padding and set length */

			if (sktp)		/* if (sktp!=0) then its a user-lvl msg*/
			{
#				ifdef IMPPRTDEBUG
					prt_dbg("Usr<", (src=bufp->b_len) > 15 ? 15:src, 1, &bufp->b_data[0]);
#				endif	/* JC McMillan */
				if( sktp->r_flags&n_toncp)	/* data to ncp? */
					to_ncp(&imp.nrcv,5,impi_msg);
				else	/* no give to user and wakeup */
				{
					/* dec msgs allocated */
					sktp->r_msgs--;
					sktp->r_msgq = catmsg( sktp->r_msgq,impi_msg );
					sktp->r_qtotal =+ impi_mlen;
					wakeup( sktp );
				}
			}
			else	/* nope hh msg */
			{
				hh1();		/* let him handle this */
			}
		}
		impi_msg = 0;
		impread(&imp.type, 8);	/* start another leader read */
	}
	else
	if( imp_stat.inpwcnt == 0 )	/* filled our buffer? JSK */
	{
		/* not end msg - keep flushing or reading */
		if( impi_flush )	/* flushing? */
		{
#			ifdef IMPPRTDEBUG
				prt_dbg("   +", net_b_size, -1, rathole);
#			endif	/* JC McMillan */

			flushimp();	/* yes - keep flushing */
		}
		else
			ihbget();	/* no cont reading data */
	
	}
	else
		printf("\nIMP:Phantom Intrp(Csri=%o)\n", imp_stat.inpstat);
}



/*name:
	ih

function:
	To look at all imp to host messages and handle rfnms, incomplete
	transmissions, and nops

algorithm:
	if imp.type is request for next message (rfnm)
		tell user rfnm arrived
	if imp.type is nop
		start another leader read
	if imp.type is error_in_data or incomplete_transmission
		tell user last data he sent is in error or imp couldnt handle
	if imp.type is non of the above
		send imp leader to ncpdaemon for processing

	in any case start another leader read

parameters:
	none

returns:
	nothing

globals:
	imp.type
	imp.nrcv
calls:
	siguser			to let user see rfnms, incomptrans or err data
	to_ncp			to ship leaders to ncpdaemon
	impread			to start another leader read
	prt_dbg			prints debugging msg on terminal

called by:
	imp_input

history:

	initial coding 1/7/75 by Steve Holmgren
*/

ih()		/*  handles all imp to host messages */
{
	register char *p;
	
#	ifdef IMPPRTDEBUG
		prt_dbg("IH ", 8+2*imp_stat.inpwcnt, 3, &imp.type);   /* print 'N' bytes of leader (us.==4)*/
#	endif	/* JC McMillan */

	switch( imp.type )
	{
		case ih_rfnm:	siguser( n_rfnmwt );	/* tell user rfnm arrived  */
		case ih_nop:	break;		/* ignore nops */

		case ih_edata:
		case ih_ictrans:siguser( n_xmterr );	/* tell user about his problem   */
		default:	to_ncp( &imp.nrcv,5,0 );	/* send leader to ncp */

	}
	impi_msg = 0;
	impread(&imp.type, 8);		/* start another leader read */
	
}


/*name:
	siguser

function:
	ostensibly to or the parameter into the users flag field and
	let him run. If the socket is closed send a close command
	to the ncp daemon he was waiting for a msg to clear the imp sys
	If the ncp daemon is monitoring all data going to a user send
	the rfnm to him
	if this is a host to host protocol rfnm let the ncp daemon
	know he can now send further protocol msgs to that host.


algorithm:
	If imp.link!=0 andif the host link entry is in the conn tab
		If conn closed
			send close to ncpdaemon
		if data for ncpdaemon
			send leader to ncpdaemon
		else
			or param into flag field
			allow user to run
	else
		host to host protocol return response so
		allow ncpdaemon to process 

parameters:
	sigflg		bits to be ored into the users flag field

returns:
	nothing

globals:
	imp.link
	imp.nrcv
	conentry->w_flags=

calls:
	imphostlink		to relate host link to socket entry
	wakeup (sys)		to allow user to run
	to_ncp			to send data to ncp daemon
	siguser
	biton
	reset_bit

called by:
	ih

history:

	initial coding 1/7/75 by Steve Holmgren
	removed check for rfnm so incomplete transmissions 
	will also reset the host rfnm bit 8/20/76 S. Holmgren
*/

siguser( sigflg )		/* called when user needs waking up. the
				   passed flag is ored into his flag field
				   and he is awakened */
{
	register conentry;
	
	if( imp.link && (conentry = imphostlink( imp.link | 0200 )) )
	{
			if( conentry->w_flags & n_toncp )
				to_ncp( &imp.nrcv,5,0 );	/* give to ncp */
			else
			{
				conentry->w_flags =| sigflg;	/* set flags */
				wakeup( conentry );
			}
	}
	else
	{
		/* user waiting for control link rfnm? */
		if( bit_on( host_map,imp.host ) != 0 )
		{
			/* say rfnm here */
			reset_bit( host_map,imp.host );
			/* let any users battle it out */
			wakeup( host_map+imp.host );
		}
		else
			/* no users waiting ship to daemon */
			to_ncp( &imp.nrcv,5,0 );
	
	}
}


/*name:
	prt_dbg

function:
	To print debugging data in a consistent manner.


parameters:
	str -- A string of length <= 3, used as first text on line.
	cnt -- The count of the number of bytes to print.
	typ -- The extended interpretation type:
		1 - general data
		2 - HH protocol data
		3 - imp leader data
		Negative # indicates MANDATORY printf, regardless of impdebug flag
	adr -- The address from which to start printing bytes.

returns:
	nothing

globals:
	impdebug		 iff, print debugging information 

calls:
	printf

called by:
	ih
	hh
	imp_output
	prt_odbg
	imp_input

history:

	Coded 1/12/76 by JC McMillan to facilitate testing ACC imp-interface
*/

#ifdef IMPPRTDEBUG

char *prt_ops [] {"nop","rts","str","cls","all","gvb","ret","inr"
		 ,"ins","eco","erp","err","rst","rrp","???" };

char *prt_types[] {
	 "Reglr","ErLdr","GoDwn","UnCtl","No-Op","RfNM ","DHost","D Imp"
	,"ErDat","IncMs","Reset","RefTA","RefWt","RefST","Ready","?"};

prt_dbg(str, cnt, typ, adr)
char *str, *adr;
int   cnt,typ;
{	register int count, bytex, newlin;
	char *address;		/* holds init value of adr */

	if (typ < 0)	/* if typ<0, msg MUST be printed */
	{	typ = -typ;
		printf("\nIMP ");
	} else {		/* handle optional msgs */
		if (!impdebug) return;
		printf("\n");
	}

	printf("%s:", str);
	if (!cnt) goto prtcrlf;
	if (cnt<0) cnt=512;

	cnt++;
	newlin = count = 0;
	address = adr;
	while (--cnt)
	{	bytex = *adr & 0377;
		printf(" %o", bytex);
		if (typ==2 && !count--)
		{	printf("=%s"
				,prt_ops [ bytex = (bytex>=0 && bytex<14 )
					? bytex:14]
				);
			count = (bytex==14 ? -1 : hhsize[bytex]-1);
		}
		if (((newlin++ & 017)==017) && (cnt!=1)) printf("\n   +");
		adr++;
	}
	if (typ == 3)
		printf("<%s", prt_types [*address&017] );
    prtcrlf:
	printf("\n");
}
#endif
/*name:
	flushimp

function:
	increment number of imp flushes and reload imp interface with
	buffer to dump data into

algorithm:
	set impi_flush flag
	handle statistics
	reload imp interface with black hole

parameters:
	none

returns:
	nothing

globals:
	impi_flush=
	imp_stat.i_flushes=

calls:
	impread			to load imp interface with address of hole

called by:
	imp_input
	ihbget
	hh

history:

	initial coding 1/7/75 by Steve Holmgren
	Modified jan/77 by JC McMillan to imbed debug-aid for ACC intrfc
*/

flushimp()
{		/* repeatedly called when we want imp interface cleaned out */
	impi_flush = 1;

	imp_stat.i_flushes++;
	impread( rathole,net_b_size );
}
/*name:
	ihbget

function:
	To reload the imp interface with a buffer to store data into

algorithm:
	If a buffer is available set impi_msg to address
		call impread to reload imp interface registers
	else
		couldnt get a buffer start flushing data

parameters:
	none

returns:
	nothing

globals:
	impi_msg=

calls:
	appendb			to add another buffer to impi_msg
	impread			to load imp interface registers
	flushimp		to flush imp data if couldnt get buffer
	printf	(sys)

called by:
	imp_input
	hh

history:

	initial coding 1/7/75 by Steve Holmgren
*/

ihbget()	/* stands for i                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                   