#
#include "../h/param.h"
#include "../h/netparam.h"
#include "../h/file.h"
#include "../h/buf.h"
#include "../h/netbuf.h"
#include "../h/user.h"
#include "../h/net.h"
#include "../h/ncp.h"
#include "../h/inode.h"
#include "../h/contab.h"

#ifdef NCP                      /* jsq bbn 10/19/78 */

/* SCCS PROGRAM IDENTIFICATION STRING */
/*char id_ncpio[] "~|^`ncpio.c\tV3.9E2\t09Mar78\n";*/
extern imp;
int ncptimo;                    /* flag indicating timeout has occurred */

int wf_send ();
int wf_setup ();
int wf_mod ();
int wf_ready ();
int wf_clean ();
int wf_reset ();
int wf_frlse ();
int wf_stimo ();


/**/
/*NOTE: this struct must match struct kw in kwrite.h, since
data is being passed in it. These will soon be combined into
one .h file used by both kernel and daemon. */
struct {			/*structure data from ncp gets put into*/
	char opcode;		/*chosen from ncp->kernel op codes*/
	char i_index;		/*inode index of particular inode in file*/
	struct netfile *id;	/*file pointer*/
	struct hostlink i_hstlnk;/* host and link */
	char status;		/*various status bits, usually for inode*/
	char bytesize;		/*network connection bytezise*/
	/*following are generated by ncp write routine common preliminaries*/
	char *iptr;		/*network inode pointer for this file and index*/
	} ncprs;		/*ncp read structure*/
/*state variables for ncp io routines*/

int ncpdlost;		/*data heading for ncp was lost. count of lost messages*/
int rootdev;		/* same as rootdev dec in ../systm.h */

/* the queues of data destined for the ncp.  ncpirq is examined first */

struct netbuf *ncprq;		/* non-interrupt traffic for the ncp */
struct netbuf *ncpirq;			/* interrupt (priority) traffic */

/*name:
	ncpopen

function:
	called in response to an open on /dev/ncpkernel
	initializes the kernel net buffer system and the imp interface

algorithm:
	is ncpkernel already open( only one person may have this open )
	determine how many system buffers to initially get( standard is one )
	get them and load them into the free list for usable buffers
	set the state to open
	initialize the imp

parameters:
	none

returns:
	a file descriptor to be used in further communication with ncpkernel

globals:
	ncpopnstate=
	net_b.kb_lowat

calls:
	getkb
	impopen

called by:
	in response to an open on ncpkernel

history:
	initial coding 1/7/75 by S. R. Bunch
	parameters set up for hysteresis buffering 6 Sep77 J.S.Kravitz

*/

ncpopen()		/*ncp open routine*/
{
	if(ncpopnstate){u.u_error = ENCP2;return;} /*already open?*/
		/*The following two lines modified 6Sep77 by J.S.Kravitz
		  for hysteresis buffer freeing algorithm */
	net_b.b_hyster = init_b_hyster;	/*setup net buffer surplus*/
	while(net_b.b_alloc < init_b_hyster) getkb();
	ncpopnstate++;			/* mark us open */
	impopen();			/* bring up the imp */
}


/*name:
	ncpclose

function:
	to make the network go away

algorithm:
	say noone has this open
	if there is anything in the input queues, empty them
	wakeup the input process so it can die.

parameters:
	none

returns:
	nothing

globals:
	ncpopnstate=
	ncpirq=
	ncprq

calls:
	freemsg

called by:
	in response to a close on ncpkernel

history:
	initial coding 1/7/75 by S. R. Bunch
	modified by mark kampe to wake up the input process.
	HMR stuff removed by greep for Rand interface

*/

ncpclose()		/*ncp close routine*/
{
#ifdef RMI
	rawclean();     /* clean up raw connections */
#endif RMI
	spl_imp();                         /* make sure interrupts not doing anything */
	/*flush ncp-destined data. since we're marked closed, none will be subsequently added*/
	if (ncpirq || ncprq)		/* either q nonempty? */
	{
		ncpdlost ++;		/*mark some data lost*/
		freemsg(ncpirq);	/*empty interrupt q*/
		freemsg(ncprq);		/*empty non-int q*/
		ncpirq=ncprq=0;		/*as they should be*/
	}
	spl0();				/* let the world back int */

	ncpopnstate = 0;		/* tell imp daemon we are down */
	wakeup( &imp );
}

/*name:
	ncpread

function:
	to pass data from user processes and the network to the ncpdaemon

algorithm:
	if there is anything in the interrupt loaded queue
	use it otherwise use the non interrupt queue
	( note: since the input side is now driven off a kernel process,
		the distinction between queues can probably be dropped
		see if there is anything on the normal read queue
		if so use that
		else
		wait for something to come in
	transfer a full message into the ncpdaemon space
	delink a message from the queue( for speed )
	copy that message into the ncpdaemon's address space
	if ncpdaemon didnt read a whole message
		signal and error

parameters:
	u.u_base
	u.u_count

returns:
	an instruction for the ncpdaemon to munch on

globals:
	ncpirq
	ncprq

calls:
	sleep
	ifreebuf( delinkbuf )
	catmsg
	bytesout
	freemsg

called by:
	in response to a read on ncpkernel

history:
	initial coding 1/7/75 by S. R. Bunch
	check for timeout added by greep

*/

ncpread()
{
	register struct netbuf *bufptr;
	register *qptr;
	struct netbuf *msgptr;

	msgptr = 0;		/*becomes a logical message*/
check_q:			/*examine the two queues*/

	spl7();                 /* lock out clock and imp interrupts */
	if(ncptimo)             /* check for timeout first */
	{	spl0();
		passc(ncp_timo);        /* send timeout op code */
		ncptimo = 0;            /* clear flag (ignore multiple */
		return;                 /*  timeout requests */
	}

	if(ncpirq)
	{       spl_imp();                 /* can 'reduce' priority to that of imp now */
		qptr = &ncpirq;         /*priority q non-empty*/
	}
	else
	{	if(ncprq)
		{	qptr = &ncprq;	/*regular q non-empty*/
			spl0();		/*don't need to lockout this one*/
		}
		else
		{	sleep(&ncprq,1);	/*wait until nonempty*/
			goto check_q;		/*go try again*/
		}
	}
	/*
	 * may still be at prio imp at this point  -
	 * copy first logical message to a temporary message,
	 * then move that to user space
	 */

	do
	{
		bufptr = (*qptr)->b_qlink;	/* first buffer in message */
		*qptr = ifreebuf(*qptr);	/*remove it from message*/
		msgptr = catmsg(msgptr,bufptr);	/* append to message*/
	}
	while (( bufptr->b_resv&b_eom) == 0 && *qptr); /*while not last buffer*/
	spl0();				/*just in case we were at prio 6*/
	u.u_count = bytesout(&msgptr,u.u_base,u.u_count,u.u_segflg);
	if( msgptr )                    /* is msg empty */
	{
		/*ncp didn't read it all. that's a nono*/
		u.u_error = ENCPIO;	/*io error*/ /*formerly EIO*/
		freemsg(msgptr);	/*reclaim the space remaining*/
	}
}


/*name:
	ncpwrite

function:
	to decode and control instructions sent down from the ncpdaemon
	to embody the ncpdaemons manipulation of the kernel and the network

algorithm:
	determine at least how much the ncpdaemon must always be writing
	is there that much
		no return error
	copy down the header information associated with the instruction
	(see ncprs)
	is the opcode legal? is the count within bounds?
		no return error
	is there a file associated with this instruction
		yes is it in the file table
			no the return error
	is there a specific inode associated with this instruction?
				and 
			   is it there
		no return error
	call the procedure as directed by the daemon( see the wants array above)
	return any error associated with the instruction

parameters:
	u.u_base(cpass)
	u.u_count(cpass)

returns:
	either an error or zero

globals:
	ncprs=
	maxncpwcode
	u.u_count=
	wants
	ncpoptab

calls:
	cpass( sys )
	log_to_ncp	to log error message
	timeout( sys )
	wf_send( thru ncpoptab )
	wf_setup( thru ncpoptab )
	wf_mod	( thru ncpoptab )
	wf_ready( thru ncpoptab )
	wf_clean( thru ncpoptab )
	wf_reset( thru hcpoptab )
	wf_timo ( thru ncpoptab )

called by:
	in response to a write on ncpkernel

history:
	initial coding 1/7/75 by S. R. Bunch
	modified 3/3/76 M. Kampe check ncprs.id = 0
	modified 7/8/76 S. Holmgren to check for ncprs.id & FNET
	modified 1/28/77 by S. M. Abraham to fix bug....
		ncprs.id & FNET change to (ncprs.id->f_flag & FNET) and
		(ncprs.i_index >3) change to (ncprs.i_index > 2)
	modified by greep to read timeout request from daemon
	modified 12Jun77 by JSKravitz to re-arrange ncp-op-table to
		be a array of structure elemements at the suggestion
		of SMAbraham
	long/short leaders jsq bbn 1-23-79 (length for wf_send)
*/

#define want_id	01	/*if set, id parameter is verified before function called*/
#define want_inodep	02	/*if set, presence of an inode pointer at the given index into file  is verified*/
#define maxncpwcode     7       /* # of entries in ncpotab */


/*the function routines called by the op code of a downward command*/

int
	wf_send(),
	wf_setup(),
	wf_mod(),
	wf_ready(),
	wf_clean(),
	wf_reset(),
	wf_frlse(),
	wf_stimo(),
	;

struct ncpoptable {
	int 	(*ncpopproc)();
	int	  ncpopwants;
	int	  ncpopsizes;
} ncpoptable []
{&wf_send,      0,        ihllen  /* send data to the network */
,&wf_setup,	want_id,	6 /* get and inode from the sys and attach it to a file */
,&wf_mod,	want_id
		+want_inodep,	6 /* modify an existing inode/socket */
,&wf_ready,	want_id,	0 /* let a user waiting on a file know something happened */
,&wf_clean,	0,		0 /* clean up an inode/socket */
,&wf_reset,	0,		0 /* clean up everything know about a host */
,&wf_frlse,	want_id,	0 /* ncpdaemon is done using a particular file */
,&wf_stimo,	0,		2 /* set timeout */
};


ncpwrite()
{
	register int i,j;		/*counters, temporaries*/
	register char *p;		/*pointer into ncprs*/

	i = &ncprs.bytesize - &ncprs.opcode;	/*length of common prelude*/
	i++;				/* add one to get right count */
	if(u.u_count < i)		/*is there enough there?*/
	{
freturn:
		u.u_error = ENCPINV;	/*invalid parameter*/ /*was EINVAL*/
		return;
	}
	p = &ncprs;			/*points to first byte of ncprs*/
	for(;i>0;i--)*p++ = cpass();	/*fill ncprs*/
	i = ncprs.opcode;		/*function to perform*/
	if(i<0 || i>maxncpwcode || u.u_count < ncpoptable[i].ncpopsizes)
	{
		log_to_ncp("ncpwrt:id\n");
		goto freturn;		/*is opdode any good?*/
	}
	if(ncpoptable[i].ncpopwants&want_id
	&& ((ncprs.id == 0)
		|| (infiletab(ncprs.id) == 0)
		|| (ncprs.id->f_flag & FNET == 0)))
	{
		log_to_ncp("ncpwrt:index\n");
		goto freturn;		/*he wants the id to be good and it isnot*/
	}
	if(ncpoptable[i].ncpopwants&want_inodep && ((ncprs.i_index > 2)
		|| ((ncprs.iptr=ncprs.id->f_netnode[ncprs.i_index]) == 0)))
		goto freturn;		/*inode or index wrong*/
	/*invoke the guy*/
	u.u_error = (*ncpoptable[i].ncpopproc)();
	return;
}

/*name:
	to_ncp

function:
	when given a vector a length and a message( the first or the last may be
	zero ) it sticks the vector on the front of the message and
	links it into the ncpdaemon's read queue

algorithm:
	is the ncpdaemon still functioning?
		no say data lost free the message, and return error
	if the vector is there and will fit in the front of the message
		move the data back and insert the vector
	otherwise
		build a new message with the vector in it 
	concatonate the newmessage with the one passed
	set the end of message bit in the last buffer of the message
	link it into the correct queue
	wakeup anyone waiting on the queue
	return no error

parameters:
	vec	-	address of 'len' number of chars to add to the front
			to msgpp( may be zero )
	len	-	number of chars in vec ( may be zero only if vec is )
	msgpp	-	pointer to a message to be added to ( may be zero )

returns:
	zero success
	minus one on ncpdaemon not running
	one on resources failure

globals:
	ncpdlost
	ncpirq
	ncprq

calls:
	freemsg
	vectomsg
	catmsg
	wakeup(sys)

called by:
	hh
	siguser
	hh1
	allocate
	netopen
	daecls

history:
	initial coding 1/7/75 by S. R. Bunch

*/

#ifndef BUFMOD

to_ncp(vec,len,msgpp)		/*len and msgpp must not both be zero*/
char *vec;			/* returns : 0 = ok, >0 nospace, 0< = ncp not open */
int len;
struct netbuf *msgpp;
{
	register struct netbuf *msgp;
	register char *p;
	register int l;
	struct netbuf *newmsg;
	int *qmsg;

	msgp = msgpp;

	if(ncpopnstate <= 0)
	{
		ncpdlost++;
		freemsg(msgp);	/* no harm if msgp == 0 */
		return(-1);		/*ncp has closed. flush data*/
	}
	newmsg = 0;			/*will become the data in vec*/
	while(len > 0)			/*copy integrals of net_b_size into
					  a new message, and as many as possible
					  into the first buffer of the old one*/
	{
	    if(msgp)			/*is there a first buffer?*/
	    {
		if(( l = (len+(msgp->b_qlink)->b_len)) <= net_b_size)	/* will it fit in first buffer */
		{
		    (msgp->b_qlink)->b_len =+ len;	/* set new length of buffer */
		    p = (msgp->b_qlink)->b_data;	/* first byte of data in first buffer */
		    for(;l>len;l--)
			p[l-1] = p[l-len-1];		/*move it down*/
		    for(;len>0;len--) *p++ = *vec++;	/*move in vec*/
		    break;
		}
	    }
	    if(vectomsg(vec,l=min(net_b_size,len),&newmsg,1))	/* did it do it? */
	    {
		freemsg(newmsg);	/*not successful. free what it did*/
		return(1);		/*simple failure*/
	    }
	    len =- l;			/*decr. total length*/
	}
	msgp = catmsg(newmsg,msgp);	/*put stuff from vector on front of msg*/
	msgp->b_resv =| b_eom;		/*mark last buffer*/
	qmsg = PS->integ & 0340 ? &ncpirq : &ncprq;	/*appropriate q pointer*/
	if( *qmsg )		/* anything en-queued */
	{
		newmsg = *qmsg;			/* link msgs then */
		p = newmsg->b_qlink;		/* sav addr of first buf */
		newmsg->b_qlink = msgp->b_qlink;	/* point last to first */
		msgp->b_qlink = p;		/* point last to first */
	}
	*qmsg = msgp;		/* update msg queue */
	wakeup(&ncprq);			/*wake up ncp*/
	return(0);			/*alls well that ends well+*/
}

#endif BUFMOD

#ifdef BUFMOD
to_ncp(vec,len,msgpp)		/*len and msgpp must not both be zero*/
char *vec;			/* returns : 0 = ok, >0 nospace, 0< = ncp not open */
int len;
struct netbuf *msgpp;
{

	if(ncpopnstate <= 0)
	{
		ncpdlost++;
		freemsg(msgpp);
		return(-1);		/*ncp has closed. flush data*/
	}
	if(msg_q(PS->integ & 0340 ? &ncpirq : &ncprq,	/*appropriate q pointer*/
		vec, len, msgpp)) return(1);
	wakeup(&ncprq);			/*wake up ncp*/
	return(0);			/*alls well that ends well+*/
}

/*
This code defines the msqq structure, which is just a list of messages strung
together with the b_eom bit set on the last buffer of a message.  This code
optimizes the common case of a short vector being placed in front of a short
message and leaves the other cases to the general-purpose code.
*/

msg_q(qmsg, vec, len, msgpp)	/* prepends vec of length len to message at msgpp, */
struct netbuf **qmsg;		/* then adds it to the queue of messages at qmsg */
char *vec;			/* returns : 0 = ok, >0 nospace */
int len;
struct netbuf *msgpp;
{
	register struct netbuf *msgp;
	register char *p;
	register int l;
	struct netbuf *newmsg;
	int bufloc;

	msgp = msgpp;

	/* do both exist and will vec fit in first buffer of msg? */
	if(len) if(msgp && (l = msgp->b_qlink->b_len+len) <= net_b_size) {
		msgp->b_qlink->b_len =+ len; /* set new length of buffer */
		bufloc = msgp->b_qlink->b_loc;
		while(--l>=len)		/* shift buffer over to give room */
			sbbyte(bufloc, l, fbbyte(bufloc, l-len));
		for(p=0;l-->=0;)	/* move vector in front */
			sbbyte(bufloc, p++, *vec++);

	} else {	/* vector exists; make it a message and put on front */

		newmsg = 0;			/*will become the data in vec*/
		if(vectomsg(vec,len,&newmsg,1)) {	/* did it do it? */
			freemsg(newmsg);	/*not successful. free what it did*/
			return(1);		/*simple failure*/
		}
		msgp = catmsg(newmsg,msgp);	/*put stuff from vector on front of msg*/
	}
	msgp->b_resv =| b_eom;		/*mark last buffer*/
	if( *qmsg )		/* anything en-queued */
	{
		newmsg = *qmsg;			/* link msgs then */
		p = newmsg->b_qlink;		/* sav addr of first buf */
		newmsg->b_qlink = msgp->b_qlink;	/* point last to first */
		msgp->b_qlink = p;		/* point last to first */
	}
	*qmsg = msgp;		/* update msg queue */
	return(0);
}

#endif BUFMOD

/*name:
	log_to_ncp

function:
	Send a null-terminated character string to the NCP daemon
	to be logged.

algorithm:
	Calculate the length of the length of the string (including the
	null at the end) plus one.
	Construct a message with a junk byte at the front.
	If the message construction was unsuccessful
		Clean up
		Say we lost a message to the NCP
	otherwise
		Change the junk byte in the message to the LOG opcode
		(doing it this way avoids a lot of character shuffling).
		Send it to the NCP.

returns:
	I hope so.

globals:
	ncpdlost=	to indicate a message for the NCP was lost.

calls:
	vectomsg	to construct a message from a string
	to_ncp		to send the message to the NCP

called by:
	anybody that has something to say.

history:
	initial coding 8/2/77 by J. G. Noel
*/


log_to_ncp(msg)
char *msg;
{
	register char *msgp;
	struct netbuf *newmsg;

	for(msgp = msg; *msgp++;);	/* find end of string */
	newmsg = 0;
	if(vectomsg(msg-1, (msgp-msg)+1, &newmsg, 1)) {
		freemsg(newmsg);
		ncpdlost++;
	} else {
#ifndef BUFMOD
		(newmsg->b_qlink)->b_data[0] = ncp_rlog;
#endif BUFMOD
#ifdef BUFMOD
		sbbyte((newmsg->b_qlink)->b_loc, 0, ncp_rlog);
#endif
		to_ncp(0, 0, newmsg);
	}
}


#ifdef FANCYLOG

log_to_ncp(msg,x1)
char *msg;
{
	net_print1(&msg);
}

net_print1(adx)
int *adx;
{
	register char *fmt, *msg;
	char *s;
	register c;
	struct netbuf *newmsg;

	newmsg = 0;
	fmt = *adx++;

	/* put into the first byte of the message the op-code */

	if (vectomsg(fmt, 1, &newmsg, 1)) goto ret;
	sbbyte(newmsg->b_loc, 0, ncp_rlog);

	while (1)
		{
		msg = fmt;      /* store beginning to be used later
				 * in the calculation of length of
				 * message
				 */
		for ( ; (*fmt && (*fmt++ != '%')); );
		if (*fmt == 0)
			{
			if (vectomsg(msg, (fmt - msg) + 1, &newmsg, 1))
				goto ret;

/*                      printf("to_ncp called with message containing\n\n");
			p_mes(newmsg);
*/
			to_ncp(0, 0, newmsg);
			return;
			}

		/* at this point a '%' has been encountered */

		if (vectomsg(msg, (fmt - msg) - 1, &newmsg, 1))
			goto ret;
		c = *fmt++;
		if (c == 'o')
			{
			if (net_pchar('0', &newmsg)) goto ret;
			if (net_prn(*adx, 8, &newmsg)) goto ret;
			}
		else
		if (c == 'd')
			{ if (net_prn(*adx, 10, &newmsg)) goto ret; }
		else
		if (c == 's')
			{
			s = *adx;
			while (c = *s++)
				if (net_pchar(c, &newmsg)) goto ret;
			}

		adx++;
		}       /* end of while loop */

ret:    freemsg(newmsg);
	ncpdlost++;

}       /* end of definition of net_print1 */


net_prn(n,b,msgpaddr)
struct netbuf **msgpaddr;
{
	register a;

	if (a = ldiv(n,b))
		if (net_prn(a,b,msgpaddr)) return(1);
	return(net_pchar(lrem(n,b) + '0', msgpaddr));
}

net_pchar(c,msgpaddr)
struct netbuf **msgpaddr;
{
	return(vectomsg(&c, 1, msgpaddr, 1));
}

#endif FANCYLOG

/*name:
	wf_send

function:
	send data to the network ( ncpdaemon takes care of its own leaders )

algorithm:
	call sndnetbytes

parameters:
	u.u_base	-	the address of the data to send
	u.u_count	-	the number of bytes to send

returns:
	zero

globals:
	u.u_base=
	u.u_count=

calls:
	sndnetbytes

called by:
	ncpwrite thru ncpoptab

history:
	initial coding 1/7/75 by S. R. Bunch

*/

wf_send()
{
	/*we enter with u.u_count and u.u_addr set correctly*/

	sndnetbytes( u.u_base,u.u_count,0,0,0 );
	u.u_count = 0;
	return( 0 );
}

/*name:
	wf_reset

function:
	to clean up all table entries and processes referencing
	a specific host.

algorithm:
	thru the whole file table
		if there is an entry and FERR not set
			for each inode in the the entry
				is it in use by the ncp
					and
				either the host is zero meaning all
				or the host matches the one sent down
				by the daemon.
					do an iclean on the inode

parameters:
	ncprs.i_hstlnk.hl_host

returns:
	zero

globals:
	file_tab

calls:
	iclean
	ipcontab
called by:
	ncpwrite thru ncpoptab

history:
	modified 6/25/76 by S. F. Holmgren to check
	for FERR not on in file flag field.
	modified for long hosts jsq bbn 12-20-78
*/

wf_reset()
{
	register int fp,ip,i;
	int fpi;

	for(fpi=0;fpi<FILSIZE;fpi++)	/*once for each possible file*/
	 /* slot in use and FERR not set (uses f_rdnode to pass err info) */
	 if( ((fp=file_tab[fpi]) != 0) && ((fp->f_flag&FERR) == 0) )
	 {
	  for(i=0;i<3;i++)                /*for each possible inode in file*/
	   if((ip=fp->f_netnode[i])    /*check for nonzero inode pointer*/
	    && ip->w_flags&n_ncpiu             /*and ncp using inode*/
	    &&(( ncprs.i_hstlnk.hl_host == 0)  /*and either host is zero*/
	     || (ncprs.i_hstlnk.hl_host == (ipcontab(ip) -> c_host)))) /*or host is same as inde's*/
		     /* do a clean on the inode */
	     {
		     fp->f_flag =| FERR;
		     iclean( fp,i );
	     }
	 }
	return(0);
}


/*name:
	wf_clean

function:
	to either initiate the release or to release the socket( inode )
	denoted by ncprs.id and ncprs.index

algorithm:
	if the user still thinks the inode is usable, advise him that
	he is wrong and send a close to the ncpdaemon.
	otherwise
		destroy the inode

parameters:
	ncprs.id	-	a pointer to a file containing the inode
	ncprs.index	-	index into the file for a pointer to the inode

returns:
	zero

globals:
	none

calls:
	iclean
	iclean calls:
		wakeup
		daedes
		ipcontab
		rmcontab

called by:
	ncpwrite thru ncpoptab ( opcode #4 )

history:
	initial coding 1/7/75 by S. R. Bunch
	modified 3/3/76 M. Kampe check non-zero fp
	call tsrvclean to implement server telnet
	modified by greep to remove connection table entry
	removed call on tsrvclean to delete server telnet test JSK 12Jun77
	changed incontab call to ipcontab call jsq bbn 12-20-78
*/

wf_clean()
{
	return(iclean(ncprs.id,ncprs.i_index));	/*needed it elsewhere also*/
}


iclean(fp,index)
struct netfile *fp;		/*file pointer*/
int index;			/*should be from 0 - 2 */
{
	register struct wrtskt *ip;	/*socket inode pointer*/
#ifndef MSG
	register int *sitp;
#endif MSG

	if( fp && infiletab( fp ) && ( ip = fp->f_netnode[index] ) )
	{
		/* remove entry from appropriate connection table */
		rmcontab( ipcontab( ip ) );
		daecls( fp,index );
		if(ip->w_flags&n_usriu)		/*user still using it?*/
		{
		    fp->f_flag =| FERR;
		    ip->w_flags =&  ~(n_ncpiu |n_open);	/*ncp not using it any more*/
		    ip->w_flags =| n_eof;	/*set end-file bit*/
		    wakeup( ip );

		/* might need to wake up "awaiting" processes, informing
		 * them of the arrival of a CLS.
		 */
#ifndef MSG
		    sitp = ip;
		    if (sitp = *(--sitp)) awake(sitp,0);
#endif MSG
#ifdef MSG
		    if (ip->itab_p) awake(ip->itab_p, 0);
#endif MSG
		}
		else
			/* both thru, destroy it */
			daedes( fp,index );
	}
	return(0);			/*no eror we know of*/
}

/*name:
	wf_ready

function:
	to wakeup anyone waiting on a file and advise them of a change
	in state.

algorithm:
	if there was an error
		is it a legal error
		signal the error
	wakeup anyone waiting on the address

parameters:
	ncprs.id
	ncprs.status

returns:
	zero if any error indication was requested correctly
	otherwise invalid parameter

globals:
	ncprs.id
	ncprs.status

calls:
	wakeup

called by:
	ncpwrite thru ncpoptab( opcode #3 )

history:
	initial coding 1/7/75 by S. R. Bunch
	lines commented out by greep to avoid daemon getting write
	  errors - what are these lines supposed to do?
	14Jun77	Commented out lines re-installed, but added and
		rearranged to make sense. J.S.Kravitz
	14Jun77	FOPEN in file flags set so that sleep in netopen
		can figure out that the wfready happenned. J.S.Kravitz
	20Apr78 net_frlse moved here in err part to correct a bug in
		netopen J.S.Kravitz
	28Jul78 net_frlse line commented out - doesn't appear to be
		correct, A. G. Nemeth
	31Aug78 use the global var "open_err" instead of the f_rdnode
		field to pass open error code, S.Y. Chiu
*/

wf_ready()
{
	register struct netfile *fp;	/* pointer to file struct */
	int retval;

	retval = 0;
	fp = ncprs.id;		/* pointer to net file struct */
	if( ncprs.status ) 	/* was there an error */
	{
		fp->f_flag =| FERR;	/* signal error happened */
		/*
		 * if the read-inode pointer is non-zero, then this
		 * constitutes an error in the state machine of the
		 * file machine in the ncpdaemon. This code correctly
		 * notifies the ncpdaemon of this. In this case, it is
		 * probably undesirable to try to get this file descritor
		 * back to the system, but we will notify the user anyway
		 * JSK
		 */
		if (fp->f_netnode[f_rdnode]) {	/* this should be zero */
			retval = ENCPINV; /*was EINVAL*/
		}else{
			open_err = ncprs.status; /* return err val */
		}
		/*
		 * decrement use count and remove file table entry
		 */
/*		net_frlse (fp);	this line might not work with old daemon */
	}else{
		fp->f_flag=| FOPEN;	/* so user knows we did the wakeup */
	}

	wakeup (fp);		/* cause user to return from open */
	return (retval);	/* tell ncp daemon what happenned */
}

/*name:
	wf_inc_alloc

function:
	internal routine to increment the allocation of an inode
	Can be directly invoked by ncpwrite but isnt

algorithm:
	copy the next six bytes associated with u.u_base & u.u_count
	if it is greater than 4k bytes return error
	is this a net or daemon originated allocation
	( can be net orginated if first allocation comes in over link
	  zero for a socket and it isnt in the connection table yet
	  hence it goes up to the daemon and down through here )
	increment message allocation
	increment the number of bytes allocated( may be in either bits or bytes)

parameters:
	ncprs.iptr

returns:
	invalid param if allocation is over 4k bits
	zero if all is well

globals:
	ncprs.iptr
	ref. socket->r_msgs
	ref. socket->w_falloc
	ref. socket->r_bytes

calls:
	log_to_ncp
	dpadd( sys )
	wakeup( sys )

called by:
	wf_mod
	wf_setup

history:
	initial coding 1/7/75 by S. R. Bunch
	lines added for network file await/capac 7/31/78 S.Y. Chiu

*/

wf_inc_alloc()
{
	register int i;
	register char *p,*ip;
	int alloc[3];
#ifndef MSG
	int *sitp;    /* sitp used exclusively by await/capac */
#endif MSG

	p = &alloc[0];			/*to assemble some bytes into*/
	ip = ncprs.iptr;		/*inode who gets the allocation*/
	for(i=6;i>0;i--) *p++ = cpass(); /*get the bytes into array alloc*/
	ip->r_msgs =+ alloc[0];		/*message alloc*/
	/* net or daemon orginated allocation */
	if( ncprs.i_hstlnk.hl_link & 0200 )
	{				/* net originated - kept in bits */
		ip->w_falloc[0] =+ alloc[1];	/* add high order word */
		dpadd( ip->w_falloc,alloc[2] );	/* add rest */
		wakeup( ip );			/* tell user */
	}
	else
		/* daemon originated pertains to read skt then */
		ip->r_bytes =+ (alloc[2]>>3);

	/* wake up "await" processes if any */
#ifndef MSG
	sitp = ip;
	if (sitp = *(--sitp)) awake(sitp,0);
#endif MSG
#ifdef MSG
	if (ip->itab_p) awake(ip->itab_p, 0);
#endif MSG
	return(0);			/*no error*/
}

/*name:
	wf_mod

function:
	called to modify the state of a socket and to increment
	the socket allocation if applicable.

algorithm:
	set or reset n_toncp or n_open according to ncprs.status
	set the host and link entry of the socket from its contab entry
	set the bytesize
	increment any allocation
	copy any new socket information down

parameters:
	ncprs.iptr
	ncprs.i_hstlnk.hl_host and hl_link
	ncprs.bytesize
	possibly three allocation words passed to wf_inc_alloc
	possibly three socket information words for wf_update_skt

returns:
	0

globals:
	see parameters

calls:
	incontab
	wf_inc_alloc
	wf_update_skt

called by:
	ncpwrite thru ncpoptab (opcode #2)

history:
	initial coding 1/7/75 by S. R. Bunch
	modified 4/16/76 S. F. Holmgren added socket information
	modified for long hosts jsq bbn 12-20-78
*/

wf_mod()
{
	register int *ip,*p;		/*inode pointer, temporary*/

	ip = ncprs.iptr;		/*for speed*/
	/*change flags*/
	ip->w_flags =& ~(n_toncp | n_open);	/*clear accessable bits*/
	ip->w_flags =| (ncprs.status & (n_toncp | n_open));	/*and set them*/
	/*change host and link*/
	/* look in correct connection table for possible error */
	if( (p = ipcontab( ip )) ) {
		p -> c_host = ncprs.i_hstlnk.hl_host;
		p -> c_link = ncprs.i_hstlnk.hl_link;
	}
	/*change bytesize*/
	ip->w_bsize = ncprs.bytesize;
	/*increment allocation*/
	wf_inc_alloc();
	/* modify socket information */
	wf_update_skt( p );
	return( 0 );
}

/*name:
	wf_setup

function:
	ncpwrite write function to initialize a socket inode

algorithm:
	if inode already there or link not 0177 or in contab
		return error
	loop thru the inode table and find a free one
		set ilock so update doesnt write it out to disk
		say two people are using ncpdaemon and user
		say its allocated and a special dev
		set dev and ino to something not usable by sys
	if cant find inode return error
	make entry in connection table
	make entry in file table
	increment count of people that have file open
	initialze fields of socket part to passed params

parameters:
	ncprs.iptr
	ncprs.id=
	ncprs.i_hstlnk.hl_host and hl_link
	ncprs.status
	ncprs.bytesize

returns:
	0

globals:
	see parameters

calls:
	swab
	incontab
	entcontab
	log_to_ncp
	wf_inc_alloc
	wf_update_skt

called by:
	ncpwrite thru ncpoptab( opcode #1 )

history:
	initial coding 1/7/75 by S. R. Bunch
	modified 4/16/76 by S. F. Holmgren to handle socket information
	initialize size1 field for network file await/capac 7/31/78 S.Y. Chiu

*/

wf_setup()
{
	register struct inode *ip;		/*incode pointer */
	register int *i;             /*temporary*/
	long host;
	int link;

	i = &(ncprs.id->f_netnode[ncprs.i_index]);
	host = ncprs.i_hstlnk.hl_host;
	link = ncprs.i_hstlnk.hl_link;
	/*get an inode*/
	for( ip = &inode[0]; ip < &inode[NINODE]; ip++ )
		if( ip->i_count == 0 )
		{				/* inode free take it */
				spl7();
			ip->i_flag = ILOCK;	/* stay in core */
			ip->i_count = 2;	/* set people using */
			ip->i_mode = IALLOC|IFCHR;
			ip->i_nlink = 1;	/* say some using */
			ip->i_dev = ip->i_number = -2;
			ip->i_lastr = -1;

			/* initialize "size1" field for await/capacity use */
			ip->i_size1 = 0;
#ifdef MSG
			ip->i_gid = ip->i_size0 = 0;
#endif MSG
			spl0();
			goto goti;
		}
	/* no available inodes */
	log_to_ncp("Ncp Setup: No free inodes\n");
	return( ENFILE );

	goti:		/* here if we got an inode */

#ifndef MSG
	/* pt at i_addr part for storing network data */
	ip = &ip->i_addr[0];
#endif MSG

	/*fill in file pointer*/
	ncprs.iptr = ncprs.id->f_netnode[ncprs.i_index] = ip;


	/*flags, bytesize,allocation*/
	ip->w_flags = (ncprs.status & (n_open | n_toncp)) | n_ncpiu;
	ip->w_bsize = ncprs.bytesize;
	/* clear messages on down */
	for( i = &ip->r_msgs; i <= &ip->r_hiwat; *i++ = 0 );
	/* see if we can get an entry in the contab */
	if( (ip = entcontab( host, link, ip )) == 0 )
		log_to_ncp( "wf_setup: no connection entries\n");
	wf_inc_alloc();
	wf_update_skt( ip );
	return( 0 );
}

/*name:
	wf_frlse

function:
	to release a network file if everyone is finished with it

algorithm:
	call net_frlse
	return 0

parameters:
	none

returns:
	zero

globals:
	ncprs.id

calls:
	net_frlse (its in nopcls.c)

called by:
	ncpwrite thru ncpoptab (opcode # 6)

history:
	initial coding 7/7/76 by S. F. Holmgren

*/
wf_frlse()
{
	/* see if the file in question can be released */
	net_frlse( ncprs.id );
	return( 0 );
}

/*name:
	wf_stimo

function:
	to request an interrupt after a given time has elapsed

algorithm:
	call timeout
	return 0

parameters:
	none

returns:
	zero

globals:
	&daetim

calls:
	timeout (sys)

called by:
	ncpwrite thru ncpoptab (opcode # 7)

history:
	initial coding by greep

*/
wf_stimo()
{
	int hz;
	int daetim();

	/* see if the file in question can be released */
	hz.lobyte = cpass();
	hz.hibyte = cpass();
	timeout(&daetim,0,hz*60);     /* set up a callout */
	return( 0 );
}

/*name:
	wf_update_skt

function:
	save info concerning local and foreign sockets

algorithm:
	copy down local socket
	copy down foreign socket

parameters:
	address of a connection table entry

returns:
	nothing

globals:
	none

calls:
	cpass

called by:
	wf_setup
	wf_mod

history:
	initial coding 4/16/76 by S. F. Holmgren

*/
wf_update_skt( con )
char *con;
{
	register char *conp;
	register int i;

	conp = &(con->c_localskt);
	i = 6;
	do
		*conp++ = cpass();
	while( --i );
}
/*name:
	daetim

function:
	to send a message to the daemon indicating a timeout has elapsed

algorithm:
	set a flag for ncpread
	wake up daemon if waiting
	note: this does not put a message into the ncp read queue
	   because it is running at priority 6 (from the clock
	   interrupt) so it could interrupt an IMP interrupt routine

parameters:
	none

returns:
	nothing

globals:
	ncptimo

calls:
	wakeup

called by:
	clock(sys) through callout set up by wf_stimo

history:
	initial coding by greep

*/
daetim()
{
	ncptimo++;
	wakeup( &ncprq );
}

#endif NCP
