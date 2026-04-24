#
#include "../h/param.h"
#include "../h/netparam.h"
#include "../h/user.h"
#include "../h/buf.h"
#include "../h/net.h"
#include "../h/netbuf.h"
#include "../h/imp.h"
#include "../h/imp11a.h"
#include "../h/hosthost.h"
#include "../h/proc.h"
#include "../h/file.h"
#include "../h/ncp.h"
#include "../h/contab.h"

#ifdef NETWORK  /* jsq bbn 10/19/78 */
extern int localnet;
#ifdef IMPPRTDEBUG
int	impdebug 0;
#endif IMPPRTDEBUG /* JC McMillan */

/* SCCS PROGRAM IDENTIFICATION STRING */
/*char id_impio[] "~|^`impio.c\tV3.9E1\t25Jan78\n";*/

/* this array is indexed by the above op codes to give the length of the command */

#ifdef NCP              /* jsq bbn 10/19/78 */
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
#endif NCP

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
	implread        (imp.h)
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
	added line to clear flags in NOP buffer before each use. jsq bbn101978
	fixed nop length jsq BBN 3-20-79
*/
impopen()
{
	register int *fp, *p;
	int needinit, inx;

	/*
	 * shouldn't we check to see that we don't get in here twice ?
	 */
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
#ifndef SHORT
/* tell imp we want long leaders */     oimp.o_nff = NFF;
/* always zero */                       oimp.o_dnet = 0;
/* nothing here this time */            oimp.o_lflgs = 0;
/* */                                   oimp.o_htype = 0;
/* */                                   oimp.o_host = 0;
/* */                                   oimp.o_imp = 0;
/* */                                   oimp.o_link = 0;
/* no padding after the i-h leader */   oimp.o_subtype = 0;
/* no message length in this one */     oimp.o_mlength = 0;
#endif SHORT
/* set opcode to h_i_nop */             oimp.o_type = ih_nop;

#ifndef BUFMOD
/* especially not BUSY */               impobuf.b_flags = 0;
/* set addr for output */               impobuf.b_addr = &oimp;
/* send the imp leader bytes */         impobuf.b_wcount = impllen;
/* send one */                          impstrat( &impobuf );
					iowait( &impobuf );			/* wait for one to fin */
#endif BUFMOD
#ifdef BUFMOD
					sndnetbytes(&oimp,impllen,0,0,1);
#endif

				}
				/* set parameters on input leader */
#ifdef NCP
				imp.nrcv = ncp_rcv;	/* perm receive comm here */
#endif NCP
				printf("\nIMP:Init\n");	/*JCM*/
				implread();             /* set up input side of imp interface */
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
	   buffering mode until the endmsg flag is raised.

	There are three orthogonal conditional compilation flags -
	NCP -   Include code for distributing to the ncpkernel
		This affects the most code.
	RMI -   Distribute to the rawkernel
		This puts in hooks to catch things before the ncp gets them.
	SHORT - Use short leaders (or long ones if not defined)
		This affects only the length and address of the leader buffer
		and the way the host number is gotten from the leader.

	There are three daemon states -
	ncpopnstate == 0 -      No daemon up; this should never occur here.
	ncpopnstate == 1 -      ncpdaemon up; either rmi or ncp can claim
		messages (also depending on compilation flags).
	ncpopnstate == -1 -     only the rmi can claim messages.

	if buffering data
		if this message is raw, or no ncp, or rawdaemon up,
			rawhh handles the buffering
		else call hh let him handle it
		in either case, return; there's nothing more to do.

	....we are reading a leader....
	get host number from leader according to leader style
	if endmsg, means imp to host msg received
		call ih and let him deal with that type
		(there is code in ih to catch ones for rmi)
	else if there is rmi and this is a raw message
		buffer the leader and start reading the rest of it
	else if there is rmi and no ncp or if the rawdaemon is up
		flush the message, there's no place else for it to go.
		return

	....the rest of the routine is only used by the ncp....
	else    must be a normal leader with further data waiting in imp
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
	struct imp (imp.h)      struct for accessing various imp leader fields
	implilob   (imp.h)      pointer to the beginning of imp struct
	impi_host  (imp.h)      put host number from leader here
	impi_sockt (imp.h)      set to addr of user inode
	impi_con   (imp.h)      addr of rawentry
	imp_stat.inpendmsg      to see if the end of the message was read
	ncpopnstate(netparam.h) to see if rawdaemon is up

calls:
	hh			to handle hh protocol msgs
	ih			to handle imp to host protocol msgs
	flushimp		to keep reading data until endmsg is on
	ihbget			to reload imp with fresh imp data buffer
	imphostlink		map host link number in conn tab to sktp ptr
	printf
	prt_dbg			prints debugging msg on terminal
	swab	(sys)
	rawhh                   (rawnet.c)
	rawhostmsg             (rawnet.c)
	rawlget                 (rawnet.c)
	stolhost                (btest.c)

called by:
	imp_open

history:

	initial coding 1/7/75 by Steve Holmgren
	Modified by J.C.McMillan for LH-DH-11 Jan77
	Modified for modularity 04Jan77 JSK
	Suppressed Link # ? message 5Jun77 J.S.Kravitz
	26Sep78 added calls on rawhh, rawhostmsg, rawlget jsq bbn
	3-?-79  long hosts jsq BBN
	3-21-79 RMI input is demultiplexed on connection (impi_con)
		rather than socket (impi_sockt) jsq BBN 3-21-79
	comments and code straightened somewhat jsq BBN 3-21-79
*/
imp_input()	/* imp input handler */
{
	register char *c;		/* does unsigned arith & general logic*/
	register char *errtext;		/* pts to 1 of several texts for errmsg */

#ifndef BUFMOD
	if (impi_adr != implilob) {    /*...if the leader is not being read... */
				       /*    data is being buffered */
#endif BUFMOD
#ifdef BUFMOD
	if ( (impi_adr != ( (implilob >> 6) & 01777 ) )    /*...if the leader is not being read... */
	   || (impi_offset != (implilob & 077)) ) {
#endif BUFMOD

#ifdef RMI
#ifdef NCP
		/* want everything if rawkernel open or no ncp */
		if ((impi_msg && (impi_msg -> b_resv & b_raw)) ||
		    (ncpopnstate < 0))
#endif NCP
		{
			rawhh();  /* rawmessage */
			return;
		}
#endif RMI
#ifdef NCP
			hh();    /* regular */
#endif NCP
		return;
	}
				/*...an 8 byte leader-read is being processed*/
	/*get host from leader now so we don't have to every time we want it*/
#ifndef SHORT
	impi_host = 0;
	impi_host.h_imp0 = imp.imp.hibyte;
	impi_host.h_imp1 = imp.imp.lobyte;    /* swap imp field */
	impi_host.h_hoi = imp.host;          /* use host directly */
	if (impi_host) impi_host.h_net = localnet;
#endif SHORT
#ifdef SHORT
	impi_host = stolhost(imp.host);      /* convert */
#endif SHORT
	if (imp_stat.inpendmsg) {
		ih();		/* if 'endmsg', a 4 byte IH leader is assumed */
		return;
	}
#ifdef RMI
	if (impi_con = rawhostmsg ()) {  /* maybe is a raw message */
		rawlget();
		return;
	}
#ifdef NCP
	if (ncpopnstate < 0) /* if rawkernel open or no ncp, flush */
#endif NCP
	{
		printf("imp: leader?\n");
		flushimp();
		return;
	}
#endif RMI

#ifdef NCP      /* the rest of the routine is only used by the ncp */
	if (imp.pad1 != 0 || (imp.type =& 0137))	/* if illegal leader */
	{
		errtext = "Pad or type error";   /* JC McMillan */
	    iherr:
		impi_sockt = 0;
		imp_stat.i_leaderr++;
		if (errtext)
			printf("\nIMP:%s\n", errtext);
#		ifdef IMPPRTDEBUG
			prt_dbg(" ", 2*(impi_wcnt+imp_stat.inpwcnt), -1, &imp.type);
#               endif IMPPRTDEBUG  /* JC McMillan */
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
#               endif IMPPRTDEBUG  /* JC McMillan */
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
#endif NCP
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
	implread   (imp.h)      to start another leader read
	flushimp		to continue flushing data from imp
	ihbget			continue bufferin data
	prt_dbg			prints debugging msg on terminal
	emerbuf			debug output of flushed data
	catmsg			assemble incoming message
called by:
	imp_input

history:

	initial coding 1/7/75 by Steve Holmgren
	check for something in impi_msg 8/16/76 S. Holmgren
	Modified Jan77 for LH-DH-11 by JSK& JC McMillan
	server telnet test stuff deleted 12Jun77 JSKravitz
	lines added for network file await/capacity 7/31/78 S.Y. Chiu
	changed for long or short leaders by jsq bbn 12-6-78
*/
#ifdef NCP
hh()
{
	register struct rdskt *sktp;
	register char *src;
	register char *dest;
	struct netbuf *bufp;
	int cnt;
#ifndef MSG
	int *sitp;         /* sitp used exclusively by await/capac */
#endif MSG
	char *ii;
	int ll;
#ifdef BUFMOD
	int dummy;
#endif

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
				prt_dbg("   ^", net_b_size+2*imp_stat.inpwcnt, -1, emerbuf());
#                       endif IMPPRTDEBUG  /* JC McMillan */

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
#ifndef BUFMOD
			bufp = impi_msg->b_qlink;
			bufp->b_len--;
			src = dest = &bufp->b_data[0];
			src++;
			cnt = net_b_size+1;
			while( --cnt )	*dest++ = *src++;
#endif BUFMOD
#ifdef BUFMOD
			bytesout(&impi_msg,&dummy,1,1);
#endif BUFMOD

			rmovepad();		/* remove padding and set length */

			if (sktp)		/* if (sktp!=0) then its a user-lvl msg*/
			{
#				ifdef IMPPRTDEBUG
					prt_dbg("Usr<", (src=bufp->b_len) > 15 ? 15:src, 1, &bufp->b_data[0]);
#                               endif IMPPRTDEBUG  /* JC McMillan */
				if( sktp->r_flags&n_toncp)	/* data to ncp? */
					to_ncp(&imp.nrcv,toncpll,impi_msg);
				else	/* no give to user and wakeup */
				{
					/* dec msgs allocated */
					sktp->r_msgs--;
					sktp->r_msgq = catmsg( sktp->r_msgq,impi_msg );
					sktp->r_qtotal =+ impi_mlen;
					wakeup( sktp );
					/* there might be an await
					 * on this read socket, so call awake
					 */
#ifndef MSG
					sitp = sktp;
					if (sitp = *(--sitp)) awake(sitp,0);
#endif MSG
#ifdef MSG
					if (sktp->itab_p) awake(sktp->itab_p, 0);
#endif MSG
				}
			}
			else	/* nope hh msg */
			{
				hh1();		/* let him handle this */
			}
		}
		impi_msg = 0;
		implread();             /* start another leader read */
	}
	else
	if( imp_stat.inpwcnt == 0 )	/* filled our buffer? JSK */
	{
		/* not end msg - keep flushing or reading */
		if( impi_flush )	/* flushing? */
		{
#			ifdef IMPPRTDEBUG
				prt_dbg("   +", net_b_size, -1, emerbuf());
#                       endif IMPPRTDEBUG  /* JC McMillan */

			flushimp();	/* yes - keep flushing */
		}
		else
			ihbget();	/* no cont reading data */
	
	}
	else
		printf("\nIMP:Phantom Intrp(Csri=%o)\n", imp_stat.inpstat);
}

#endif NCP

/*name:
	ih

function:
	To look at all imp to host messages and handle rfnms, incomplete
	transmissions, and nops

algorithm:
	if rawih completely processes it as a rawmessage leader, return

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
	implread (imp.h)        to start another leader read
	prt_dbg			prints debugging msg on terminal

called by:
	imp_input

history:

	initial coding 1/7/75 by Steve Holmgren
	26Sep78 added call on rawih jsq bbn
	12-6-78 changed for short or long leaders jsq bbn
	pick up local host from nop jsq bbn 2-9-79
*/

ih()		/*  handles all imp to host messages */
{
	register char *p;
	
#	ifdef IMPPRTDEBUG
		prt_dbg("IH ", 8+2*imp_stat.inpwcnt, 3, &imp.type);   /* print 'N' bytes of leader (us.==4)*/
#       endif IMPPRTDEBUG  /* JC McMillan */

	if ((imp.type & 0137) == ih_nop) {
		netparam.net_lhost = impi_host;
	}
#ifdef RMI
	if (rawih()) return;
#endif RMI
#ifdef NCP
	switch( imp.type =& 0137 )
	{
		case ih_rfnm:	siguser( n_rfnmwt );	/* tell user rfnm arrived  */
		case 15:			/* ignore new style header */
		case ih_nop:	break;		/* ignore nops */

		case ih_edata:
		case ih_ictrans:siguser( n_xmterr );	/* tell user about his problem   */
		default:        to_ncp( &imp.nrcv,toncpll,0 );/* send leader to ncp */

	}
#endif NCP
	impi_msg = 0;
	implread();                      /* start another leader read */
	
}


/*name:
	siguser

function:
	ostensibly to or the parameter into the users flag field and
	let him run. If the socket is closed send a close command
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
	reset_host

called by:
	ih

history:

	initial coding 1/7/75 by Steve Holmgren
	removed check for rfnm so incomplete transmissions
	will also reset the host rfnm bit 8/20/76 S. Holmgren
	changed to_ncp arg 12-6-78 jsq bbn
*/

#ifdef NCP
siguser( sigflg )		/* called when user needs waking up. the
				   passed flag is ored into his flag field
				   and he is awakened */
{
	register conentry;
	
	if( imp.link && (conentry = imphostlink( imp.link | 0200 )) )
	{
			if( conentry->w_flags & n_toncp )
				to_ncp( &imp.nrcv,toncpll,0);/* give to ncp */
			else
			{
				conentry->w_flags =| sigflg;	/* set flags */
				wakeup( conentry );
			}
	}
	else
	{
		/* user waiting for control link rfnm? */
		if( host_on( host_map,impi_host ) != 0 )
		{
			/* say rfnm here */
			reset_host( host_map,impi_host );
			/* let any users battle it out */
			host_wakeup( host_map, impi_host );
		}
		else
			/* no users waiting ship to daemon */
			to_ncp( &imp.nrcv,toncpll,0 );
	
	}
}
#endif NCP

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
#endif IMPPRTDEBUG
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
	emerbuf			to get address of black hole
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
#ifdef BUFMOD
	register char *temp;
#endif BUFMOD

	impi_flush = 1;

	imp_stat.i_flushes++;
#ifndef BUFMOD
	impread( emerbuf(),net_b_size );
#endif BUFMOD
#ifdef BUFMOD
	temp = emerbuf();
	impread( ( (temp >> 6) & 01777 ),
		 ( temp & 077 ),
		 net_b_size);
#endif BUFMOD
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

ihbget()	/* stands for imp host buffer getter */
{
	register struct netbuf *bp;
	/* called when there is data to buffer from the imp */
	/* appendb returns 0 if cant get buffer */
	
	if( bp=appendb( impi_msg ) )
	{
		impi_msg = bp;
#ifndef BUFMOD
		impread( bp->b_data, net_b_size);
#endif BUFMOD
#ifdef BUFMOD
		impread( bp->b_loc, 0, net_b_size);
#endif BUFMOD
	}
	else
	{
		printf("\nIMP:Flush(No Bfrs)\n");
		flushimp();
	}
	
}

/*name:
	hh1

function:
	To search through host to host protocol messages strip out any
	that apply to the user ( allocates for now )

algorithm:
	Looks through each buffer of the message on host to host
	protocol boundaries for and allocate protocol msg. When
	one is found, copies the msg into hhmsg and overwrites
	with host to host nops. Calls allocate with the allocate
	protocol command with deals with it at the user level.
	once all the msg has been passed through, it as well as
	the imp to host leader is passed to the ncpdaemon for further
	processing.

parameters:
	none

returns:
	nothing

globals:
	impi_msg
	impi_mlen
	hhsize[]
	imp.nrcv

calls:
	allocate		to deal with allocate commands rcvd from net
	to_ncp			to send uninteresting protocol to ncpdaemon
	prt_dbg			to note all flush calls if in debug mode
	bytesout
	catmsg
	imphostlink
	vectomsg

called by:
	hh

history:

	initial coding 1/7/75 by Steve Holmgren
	modified 1/1/77 by Steve Holmgren to simplify and correct bug
	related to hh protocal crossing buffer boundary
	changed to_ncp arg jsq bbn 12-6-78
*/
#ifdef NCP
hh1()
{

	register int hhcom;
	register char *sktp;
	register cnt;
	static char *daemsg, hhproto [96];

	daemsg = 0;
	while( impi_mlen > 0  ) {	/* while things in msg */
#ifndef BUFMOD
		hhcom = (impi_msg->b_qlink)->b_data[0] & 0377;
#endif BUFMOD
#ifdef BUFMOD
		hhcom = fbbyte(impi_msg->b_qlink->b_loc, 0);
#endif BUFMOD
		if( hhcom > NUMHHOPS ) {
			daemsg = catmsg( daemsg,impi_msg );
			goto fordaemon;
		}

		cnt = hhsize[ hhcom ];	/* get bytes in this command */
		impi_mlen =- cnt;	/* decrement impi_mlen */

		if( bytesout( &impi_msg,&hhproto,cnt,1 ))	/* msg not long enough */
			impi_mlen = 0;			/* force msg empty */
		else
		if( hhcom == hhall && (sktp=imphostlink( hhproto[1]|0200 )) ) {
			allocate( sktp,&hhproto );
			continue;
		}
		else
		if( hhcom == hhins && (sktp=imphostlink( hhproto[1]&0177 )) ) {
			if( (sktp->r_rdproc) &&
			    ((sktp -> r_rdproc) -> p_stat) ) { /* there ? */
#ifdef MSG
				sktp->INS_cnt =+ 1;     /* increment count */
				if (sktp->itab_p) awake(sktp->itab_p, 0);  /* awake awaiting process */
#endif MSG
				psignal( sktp->r_rdproc,SIGINR );
			}
			continue;
		}
		else
		if( hhcom == hhinr && (sktp=imphostlink( hhproto[1]|0200 )) ) {
			if( (sktp->r_rdproc) &&
			    ((sktp -> r_rdproc) -> p_stat) ) {
#ifdef MSG
				sktp->INR_cnt =+ 1;     /* increment count */
				if (sktp->itab_p) awake(sktp->itab_p, 0);  /* awake awaiting process */
#endif MSG
				psignal (sktp -> w_wrtproc, SIGINR);
			}
			continue;
		}
		else
		if( hhcom == hhnop )
			continue;

		vectomsg( &hhproto,cnt,&daemsg,1 );	/* got here then give it to daemon */
	}

	if( daemsg != 0 )		/* something in msg */
fordaemon:
		to_ncp (&imp.nrcv, toncpll, daemsg);  /* send to daemon */
}
#endif NCP
/*name:
	allocate
function:
	To look over host to host allocate protocol messages .
	determine whether they are going to ncpdaemon or user
	send off to ncpdaemon, inc appropriate user fields.

algorithm:
	if host_link in conn tab
		if socket flags say to ncpdaemon
			send to ncpdaemon with imp to host leader
		else
			update num of messages alocated
			update number of bits allocated
			tell user allocate came in
			let user run

parameters:
	allocp 		pointer to a host host allocate

returns:
	nothing

globals:
	imp.nrcv
	sktp->w_msgs=
	sktp->w_falloc=
	sktp->w_flags=

calls:
	vectomsg		to build a msg from vec passed to send to ncp daemon
	to_ncp			to ship the allocate off to the ncp daemon
	dpadd (sys)		to add two double precision words
	wakeup (sys)		to let the user run

called by:
	hh1

history:

	initial coding 1/7/75 by Steve Holmgren
	modified to awake "awaiting" processes 8/14/78 S.Y. Chiu
	changed to_ncp arg jsq bbn 12-6-78
*/
#ifdef NCP
allocate( skt_ptr,allocp )
struct wrtskt *skt_ptr;
{

	/* called from hh1 when a hh allocate is received */
	register char *ap;
	register struct wrtskt *sktp;
	struct netbuf *msgp;
#ifndef MSG
	int *sitp;
#endif MSG

	sktp = skt_ptr;
	ap = allocp;
	msgp = 0;
	if (sktp->w_flags & n_toncp)
	{
		vectomsg( allocp,8,&msgp,1 );
		to_ncp( &imp.nrcv,toncpll,msgp );
	}
	else
	{
		sktp->w_msgs =+ swab( ap->a_msgs );
		sktp->w_falloc[0] =+ swab(ap->a_bitshi);
		dpadd(sktp->w_falloc,swab(ap->a_bitslo));
		sktp->w_flags =| n_allocwt;
		wakeup( sktp );
		/* wake up "await" processes, if any */
#ifndef MSG
		sitp = sktp;
		if (sitp = *(--sitp)) awake(sitp,0);
#endif MSG
#ifdef MSG
		if (sktp->itab_p) awake(sktp->itab_p, 0);
#endif MSG
	}
	
}
#endif NCP
/*name:
	rmovepad

function:
	To remove the padding attached to every host host protocol msg
	and standard message by the imp

algorithm:
	given a bytesize in number of bytes in that size, calculate
	the number of 8 bit bytes.
	set the message length to that size
	run through the message a buffer at a time subtracting
	the buffer length from the calculated size. eventually
	it will go negative, since the number of bytes calculated
	is less that the actual number of bytes in the msg
	subtract the number of pad bytes from the last buffer in
	the message to setthe number of actual number of data bytes
	then free any remaining buffers.
	set impi_msg to the new last buffer.

parameters:
	none

returns:
	nothing

globals:
	impi_mlen=
	impi_msg=
	impi_msg->b_qlink

calls:
	swab (sys)		to switch top and bottom bytes
	freebuf			to release the last buffer from the message

called by:
	hh

history:

	initial coding 1/7/75 by Steve Holmgren
	added bytesizes multiple of 8 01/27/78 S. F. Holmgren
	modified so that impi_mlen is correctly set 8/11/78 S.Y. Chiu
*/
#ifdef NCP
rmovepad()
{
	/*  calculates number of bytes from impleader then runs through impsg buffers
	    adding up counts until number calculated or end of msg is reached.
	    sets impcnt tothe min of amt in msg and calculated and discards
	    any excess bytes  */

	register struct netbuf *bfrp;
	register cnt;

	impi_mlen = swab( imp.bcnt );		/* get # bytesize bytes */
	impi_mlen = cnt = (impi_mlen * imp.bsize)/8;        /* turn into 8-bit bytes */

	impi_msg = bfrp = impi_msg->b_qlink;	/* pt at first bfr in msg */
	while(((cnt =- (bfrp->b_len & 0377)) > 0 )	/*get to last bfr with valid data */
		&& (bfrp->b_qlink != impi_msg))	/* protect against wrapping around -- JC McMillan */
	    bfrp = bfrp->b_qlink;

	if (cnt>0)	/* jcm -- note errors */
	{	printf(" \nIMP:Missing %d B\n", cnt);
		impi_mlen =- (( cnt*8 ) / impi_sockt->r_bsize );
	}

	if (cnt < 0)			/* cnt has -(# bytes to discard) */
		bfrp->b_len = (bfrp->b_len & 0377) + cnt;	/* discard extra bytes this buffer */
	while( bfrp->b_qlink != impi_msg )	/* while not pting at 1st bfr */
		freebuf(bfrp);
	impi_msg = bfrp;				/* set new handle on msg */
}
#endif NCP
/*name:
	imphostlink

function:
	Checks to see if the current host and link are in the connection
	table (conn tab). Checks to see if the socket is open, if so
	returns a pointer to the socket. If not in conn tab or socket
	not open returns zero

algorithm:
	if host link in conn tab
		if skt open
			return socket ptr

	return zero

parameters:
	link			link to be checked with current host

returns:
	zero 			if not in conn tab or socket not open
	socket ptr		if in conn tab and socket open

globals:
	imp.host
	sktp->r_flags

calls:
	incontab		to see if host link is in conn tab

called by:
	hh1
	imp_input
	siguser

history:

	initial coding 1/7/75 by Steve Holmgren
*/
#ifdef NCP
imphostlink( link )
char link;
{
	register sktp;
	if( sktp = incontab( impi_host, (link & 0377), 0 ))
	{
		sktp = sktp->c_siptr;		/* contab returns ptr to entry
						   must get skt pointer
						*/
			return( sktp );
	}
	return( 0 );
}
#endif NCP
/*name:
	imp_dwn

function:
	clean up ncp data structures so that an ncpdaemon restart will
	work correctly.

algorithm:
	reset the imp interface
	free any input message
	clean up input variables
	clear out all messages queued for the output side
	reset any rfnm bits
	let the kernel buffer code clean up its own

parameters:
	none

returns:
	nothing

globals:
	impi_msg =
	impi_mlen =
	impi_sockt =
	impi_con
	impi_flush =
	impotab (forward and backward links)
	host_map[-] =

calls:
	freemsg
	ncp_bfrdwn
	imp_reset
	printf
	host_clean

called by:
	imp_open

history:
	initial coding 6/22/76 by S. F. Holmgren
	modified 4/1/77, S.M. Abraham to fix bug in loop that frees
	all msgs in output queue. It wasn't resetting the msg ptr
	to the next msg in the queue.
	printf changed to 'NCP' from 'IMP' 31AUG77 JSK
	long host mods jsq bbn 1-30-79
	clear impi_con jsq BBN 3-21-79
*/
imp_dwn()
{
	register char *p;
	register int *q;

	/* reset the imp interface */
	imp_reset();		/* JSK */

	/* cleanup the input side */
	freemsg( impi_msg );
	impi_msg = impi_mlen = impi_sockt = impi_con = impi_flush =  0;

	/* clean up the output side */
	impotab.d_active = 0;
	/* get rid of messages waiting to be output */
#ifndef BUFMOD
	p = impotab.d_actf;
	while (p)
	{
		if( p->b_flags & B_SPEC )	/* this a net message */
			freemsg( p->b_dev );	/* free it */
		else
			iodone( p );		/* sys buffer say done */
		p = p -> b_forw;		/* pt to next msg */
	}
#endif BUFMOD
#ifdef BUFMOD
	freemsg(impotab.d_actf);
#endif BUFMOD
	impotab.d_actf = impotab.d_actl = 0;

#ifdef NCP
	/* clear out any waiting rfnm bits */
	host_clean(&host_map);
#endif NCP

	printf("\nNCP:Down!\n");	/*JCM & JSK*/

	/* let the net message software clean up */
	ncp_bfrdwn();
}

/*name:
	impstrat

function:
	Used to link output buffers into the imp output queue
	for transmission to the network.

algorithm:
	if queue empty put buffer at front.
	else
	link buffer to end
	if imp output is not active start it up

parameters:
	abp	-	pointer to the buffer to be sent

returns:
	nothing

globals:
	impotab.d_actf=
	impotab.d_actl=
	bp->b_forw=

calls:
	imp_output
	prt_odbg			prints debugging msg on terminal
	spl_imp

called by:
	sndnetbytes	(kerbuf.c)
	impopen

history:
	initial coding 1/7/75 by S. F. Holmgren
	Modified 05Jan77 for LH-DH-11 by JSK

*/

#ifndef BUFMOD
impstrat( abp )
struct buf *abp;
{

	register struct buf *bp;

	bp = abp;
	

	bp->b_forw = 0;		/* null forward pointer this buffer */
	spl_imp();
	if( impotab.d_actf == 0 )	/* anything in list ?? */
		impotab.d_actf = bp;	/* no make this the first */
	else
		impotab.d_actl->b_forw = bp;	/* make this new last */
	/* set last to this */
	impotab.d_actl = bp;

	if (impotab.d_active == 0)		/* do we need to start the imp? */
	{	imp_output();		/* yep kick it over */
#		ifdef IMPPRTDEBUG
			prt_odbg();		/* if impdebug set, scan & print leader */
#               endif IMPPRTDEBUG  /* JC McMillan */
	}
	

}
#endif BUFMOD

#ifdef BUFMOD
impstrat( abp )
struct netbuf *abp;
{
	register struct netbuf *bp;

	bp = abp;
	spl_imp();
	if (impotab.d_actf == 0)
		impotab.d_actf = bp->b_qlink;
	msg_q(&impotab.d_actl, 0, 0, bp);
	if (impotab.d_active == 0)
		imp_output();
}
#endif

/*name:
	prt_odbg

function:
	aid in debugging output stream to imp

algorithm:
	interprete data in a minor way, then use prt_dbg to print.

parameters:
	none	-- uses same presumptions regarding globals as does imp_output

returns:
	nothing

globals:
	impotab.d_actf
	impdebug

calls:
	prt_dbg			prints debugging msg on terminal

called by:
	impstrat
	imp_oint

history:
	coded jan 77 by JCM

*/
#ifdef IMPPRTDEBUG
int impodebug 0;

prt_odbg()
{
	register struct buf *bp;
	register char *cnt;
	register char *ldr;
	extern impdebug;

	
	if ((!impdebug) || (impodebug))  return;

	if((bp = impotab.d_actf) == 0)
		return;			/* return nothing to do */

	ldr = bp->b_addr;
	cnt = bp->b_wcount;
	prt_dbg(cnt<5 ? " HI":"<HH", cnt>9 ? 9:cnt, 3, ldr);
	if ((cnt>9) & (ldr->o_link==0)) prt_dbg("pHH", cnt-9>128? 128:cnt-9, 2, ldr+9);
}

#endif IMPPRTDEBUG /* JC McMillan */
/*name:
	imp_odone

function:
	Check conditions of last output transfer and start
	next output to interface

algorithm:
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
	imp output interrupt routine

history:
	initial coding 1/7/75 by S. F. Holmgren
	recoded for modularity 15Feb77 JSK

*/
#ifndef BUFMOD
imp_odone (errbits) char errbits;
{
	register char *buffer;
	register struct devtab *iot;
	register char *msg;

	iot = &impotab;

	buffer = iot->d_actf;		/* pt at first buffer in list */

	if( errbits )	/* timeout errror */
	{
		buffer->b_flags =| B_ERROR;
		buffer->b_error = errbits;
	}
	else	/* if kernel msg and has multi buffers send next buffer */
	if(( buffer->b_flags & B_SPEC )
			&&
		/* this last bfr in msg */
		(buffer->b_blkno & 01))
	{
		/* no we disabled endmsg last time must send some more */
		
		msg = buffer->av_forw;
		/* set next buffer address into header */
		buffer->b_addr = msg->b_data;
		/* set length of that buffer into length field */
		buffer->b_wcount = ( msg->b_len & 0377 );
		/* if this is not the last buffer then disable endmsg */
		buffer->b_blkno = (msg == buffer->b_dev) ? 0 : 1;

		/* update next msg buffer field */
		buffer->av_forw = msg = msg->b_qlink;

		/* send to imp */
		imp_output();

		
		return;
	}
	iot->d_actf = buffer->b_forw;		/* set top to next */
	/* give msg back -- ptr put in by sndnetbytes */
	if( buffer->b_flags&B_SPEC )
		freemsg( buffer->b_dev );
	else
		iodone( buffer );	/* say done with buffer */

	if( iot->d_actf == 0 )		/* done for awhile?? */
	{
		iot->d_actl = 0;	/* remove queue links */
		iot->d_active = 0;	/* say we are done */
	}
	else
	{	imp_output();		/* do some more */
#		ifdef IMPPRTDEBUG
			prt_odbg();		/* if impdebug set, scan & print leader */
#               endif IMPPRTDEBUG  /* JC McMillan */
	}
	
}
#endif

#ifdef BUFMOD
imp_odone (errbits)
char errbits;
{
	register char *nbp;
	register struct devtab *iot;
	register char *msg;

	/* free UNIBUS map */
	mapfree(&i11a_obuf);

	iot = &impotab;
	nbp = iot->d_actf;

	iot->d_actl = freebuf(iot->d_actl);

	if (iot->d_actl == 0)
		iot->d_actf = iot->d_active = 0;
	else
		{
		iot->d_actf = ( (iot->d_actl) -> b_qlink );
		imp_output();
		}
}
#endif BUFMOD


#endif NETWORK
