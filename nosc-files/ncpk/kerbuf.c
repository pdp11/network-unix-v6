#
#include "param.h"
#include "buf.h"
#include "net_netbuf.h"
#include "net_net.h"

#ifdef SCCSID
/* SCCS PROGRAM IDENTIFICATION STRING */
char id_kerbuf[] "~|^`kerbuf.c\tV3.9E1\t9Mar78\n";
#endif


/*name:
	appendb

function:
	return some space at the end of a message to put some data in

algorithm:
	is there space at the end of the last buffer in the present
	message 
		return that
	if we could get a buffer then link it to the message and return
	linkbuf otherwise zero

parameters:
	msg	-	pointer to a message( may be zero )
			MESSAGE DEFINITION
				A net message is a circularly linked
				ring of 64 bytes buffers.  The handle
				on the message is a pointer to the 
				most recently added buffer in the ring.

returns:
	if it could find some space a possibly updated pointer to the
	buffer containing the space.  The buffer is linked to the message

globals:
	net_b_size

calls:
	getbuf
	linkbuf

called by:
	ihbget		(impio.c)
	sndnetbytes
	vectomsg

history:
	initial coding 1/7/75 by S. R. Bunch
	initial debugging 1/7/75 S. F. Holmgren


*/

appendb(msg)
struct netbuf *msg;			/*pointer to message being extended*/
{

	register struct netbuf *msgp;	/*just for a teensy bit more speed maybe*/
	register struct netbuf *bp;	/* for holding on to new buffer addr */

	msgp = msg;
	if( msgp && msgp->b_len < net_b_size) return( msgp ); /* already space there */
	return( (bp=getbuf()) ? linkbuf( bp,msgp ) : 0 );
}

/*name:
	bcopyin

function:
	copy bytes from user to kernel space

algorithm:
	if there are bytes to move
		if anyone of src, dest, count are odd
			move it a byte at a time
		else
			copyin

parameters:
	src	-	address to get bytes from
	dest	-	address to put bytes
	count	-	how many

returns:
	nothing

globals:
	none

calls:
	fubyte( sys )
	copyin( sys )

called by:
	vectomsg
	netopen		(nopcls.c)

history:
	initial coding 1/7/75 by S. R. Bunch
	initial debugging 1/7/75 S. F. Holmgren
	Not used by UCBUFMODs 19Feb78 Greg Noel

*/
#ifndef UCBUFMOD
bcopyin(src,dst,count)
char *src,*dst;		/*source and dest addresses, resp*/
int count;			/*byte count*/
{
	register char *s,*d;	/*counterparts of above*/
	register int c;		/*ditto*/

	s = src;
	d = dst;
	c = count;
	
	if(c > 0)
	{
		if((s|d|c)&01)	/*source,dest,or count odd?*/
		    for(;c>0;c--)*d++ = fubyte(s++);	/*yes, do bytes*/
		else
		    copyin(s,d,c);			/*no, do words*/
	}
	
}
#endif UCBUFMOD
/*name:
	bcopyout

function:
	copies bytes from kernel to user space

algorithm:
	if there is data to move
		if anyone of source, dest, and count are odd
			move it a byte at a time
		else
			copyout

parameters:
	src	-	address to get bytes from
	dst	-	address to put bytes
	count	-	number of bytes to move

returns:
	nothing

globals:
	none

calls:
	subyte( sys )
	copyout( sys )

called by:
	bytesout

history:
	initial coding 1/7/75 by S. R. Bunch
	initial debugging 1/7/75 S. F. Holmgren
	Not used by UCBUFMODs 19Feb78 Greg Noel

*/
#ifndef UCBUFMOD
bcopyout(src,dst,count)
char *src,*dst;		/*see comments in bcopyin*/
int count;
{
	register char *s,*d;
	register int c;

	s = src;
	d = dst;
	c = count;
	
	if(c > 0)
	{
		if((s|d|c)&01)
		    for(;c>0;c--)subyte(d++,*s++);	/* store *s at d */
		else
		    copyout(s,d,c);
	}
	
}
#endif UCBUFMOD
/*name:
	bytesout

function:
	copy bytes from a message into a vector

algorithm:
	while the msgptr passed points to something and user count non zero
		point at first buffer
		copy data from that buffer into space denoted by segflg
		update count in buffer
		possibly free buffer and update msgpointer
		update count

parameters:
	msgpaddr	-	pointer to a message pointer fakes call by name
	vec		-	address to put data
	count		-	how much to take out of msg
	segflg		-	1 = if vec in kernel space
				0 = if vec in user space

returns:
	some data in the vector passed

globals:
	none

calls:
	bcopyout
	freebuf
	min
	spl_imp

called by:
	ncpread		(ncpio.c)
	netread		(nrdwr.c)
	hh1		(impio.c)

history:
	initial coding 1/7/75 by S. R. Bunch
	initial debugging 1/7/75 S. F. Holmgren
	UCBUFMODs added 19Feb78 Greg Noel

*/

bytesout(msgpaddr,vec,count,segflg)
struct netbuf **msgpaddr;	/*pointer to a message pointer so we can
				  modify it as needed. fakes a call by name*/
int segflg;			/*0=user space. !=0 = kernel space*/
char *vec;			/*vector of bytes. can be in kernel or user
				  space, as per segflg*/
int count;			/*number of bytes to move, if possible*/
{

	register char *d,*p;	/*pointer to data,anything, resp.*/
	int size,sps;		/*size to move this time, save ps*/
	register int s;		/*for fast loops and to preserve size*/
#ifdef UCBUFMOD
	int o;			/*holds offset during copy*/
#endif UCBUFMOD

	
	sps = PS->integ;	/*save processor priority*/
	while(*msgpaddr != 0 && count > 0)
	{
	    spl_imp();		/*disallow interrupts for the critical part*/
	    p = (*msgpaddr)->b_qlink;	/*pointer to first buf in msg*/
#ifndef UCBUFMOD
	    d = &p->b_data[0];		/*pointer to data part of msg*/
#endif UCBUFMOD
#ifdef UCBUFMOD
	    d = 0;			/*offset in data part of msg*/
#endif UCBUFMOD
	    size = min( count,(p->b_len & 0377) );	/* amt of data to move */
	    if(segflg)			/*find out where to put it*/
	    {
#ifndef UCBUFMOD
		for(s = size;s>0;s--)*vec++ = *d++;   /*kernel to kernel*/
#endif UCBUFMOD
#ifdef UCBUFMOD
		for(s=size;s>0;s--)		/*buffer to kernel*/
			*vec++ = fbbyte(p->b_loc, d++);
#endif UCBUFMOD
	    }
	    else
	    {
#ifndef UCBUFMOD
		bcopyout(d,vec,size);	/*kernel to user*/
		d =+ size;		/*update addresses*/
		vec =+ size;
#endif UCBUFMOD
#ifdef UCBUFMOD
		for(s=size;s>0;s--)	/*buffer to kernel*/
		    subyte(vec++, fbbyte(p->b_loc, d++));
#endif UCBUFMOD
	    }
	    if((s=(p->b_len - size)) > 0)	/*data left in buffer?*/
	    {
		p->b_len = s;		/*update data length*/
#ifndef UCBUFMOD
		p = &p->b_data[0];	/*dest of data*/
		for(;s>0;s--)*p++ = *d++; /*move data up*/
#endif UCBUFMOD
#ifdef UCBUFMOD
		for(o=0;o<s;)	/*shift data over in buffer*/
		    sbbyte(p->b_loc, o++, fbbyte(p->b_loc, d++));
#endif UCBUFMOD
	    }
	    else *msgpaddr = freebuf(*msgpaddr);
					/*just release the buffer*/
	    count =- size;		/*update amount to go*/
	    PS->integ = sps;	/*restore ps*/
	}
	return(count);
	
}

/*name:
	catmsg

function:
	concatenate two message in the order given and return the resulting
	message.

algorithm:
	if msg2 is zero return msg1
	if msg1 is zero return msg2
	while the first buffer of message 2 will fit in the last buffer of msg1
		copy buffer for msg2 to last of msg1
		release buffer
		is message two now gone
			reset ps
			return message one
	save address of first buffer in msg1
	point last buffer in msg1 to first buffer in msg2
	point last buffer in msg2 to first buffer in msg1

parameters:
	msgp1	-	address of a buffer containing the latest data added
	msgp2	-	address of a buffer containing the latest data added

returns:
	a pointer to the concatenated message

globals:
	none

calls:
	freebuf
	spl_imp
	vectomsg

called by:
	ncpread		(ncpio.c)
	to_ncp		(ncpio.c)
	hh		(impio.c)
	hh1		(impio.c)

history:
	initial coding 1/7/75 by S. R. Bunch
	initial debugging 1/7/75 S. F. Holmgren
	added copy of first of two to last of one 10/12/76 S. F. Holmgren
	UCBUFMODs added 19Feb78 Greg Noel

*/

catmsg(msgp1,msgp2)
struct netbuf *msgp1,*msgp2;
{
	register struct netbuf *msg1,*msg2;
	register char *p;	/*a temporary*/
	int sps;		/* store ps */

	msg1 = msgp1;		msg2 = msgp2;

	sps = PS->integ;			/* save PS */
	if(msg2==0) return(msg1);		/*check for easy ones*/
	if(msg1==0) return(msg2);
	spl_imp();

	/* while there is room in last of one for all of first of two */
	while( (net_b_size - msg1->b_len) >= (msg2->b_qlink)->b_len )
	{
		/*  copy all of first of two into last of one */
#ifndef UCBUFMOD
		vectomsg( &(msg2->b_qlink)->b_data[0],(msg2->b_qlink)->b_len,
			  &msgp1,1
			);
#endif ifndef UCBUFMOD
#ifdef UCBUFMOD
		for(p = 0; p < msg2->b_qlink->b_len;)
			sbbyte(msg1->b_loc, msg1->b_len++,
				fbbyte(msg2->b_qlink->b_loc, p++));
#endif ifdef UCBUFMOD

		/* release first of two */
		msg2 = freebuf( msg2 );

		/* is two now empty */
		if( msg2 == 0 )
		{
			PS->integ = sps;
			return( msg1 );
		}
	}

	p = msg1->b_qlink;			/*first buf in msg1*/
	msg1->b_qlink = msg2->b_qlink;		/*first buffer in msg2*/
	msg2->b_qlink = p;			/*point last of 2 to first of 1*/
	PS->integ = sps;			/* restore ps */
	return(msg2);				/*return catenation*/
}

/*name:
	freeb

function:
	add a free buffer to the free list

algorithm:
	link the buffer onto the freelist
	if anyone needs a buffer let him know one has been made available
	return the number of small buffers pertaining to a particular
	large buffer( buf.h ) that are currently in the free list

parameters:
	buffer to be freed

returns:
	see algorithm

globals:
	net_b.b_freel=
	net_b.b_need=

calls:
	linkbuf
	spl_imp
	wakeup

called by:
	freebuf
	getkb

history:
	initial coding 1/7/75 by S. R. Bunch
	initial debugging 1/7/75 S. F. Holmgren
	Increment free count moved here 15Mar78 Greg Noel

*/

freeb(bufpp)
struct netbuf *bufpp;			/*buffer to be freed*/
{
	register struct netbuf *bufp;	/*same as bufpp*/
	register int index;		/*index into kernel buffer array of
					  this buffer*/
	register int sps;		/* save ps */

	
	bufp = bufpp;
	sps = PS->integ;			/* save ps */
	spl_imp();					/* set to non-interrupt */
	index = bufp->b_resv =& b_memberof;	/* clean up this field */
	net_b.b_freel = linkbuf(bufp,net_b.b_freel);
						/*add to free list*/
	if( net_b.b_need & proc_need )	/* a process needs a buffer */
	    wakeup(&net_b.b_freel);
	net_b.b_need = 0;		/*in any event, reset the need*/
	
	/*increment free buffer count for this kernel buffer*/
	index = ++(net_b.kb_[index]->b_freect);
	net_b.b_cntfree++;	/* inc free list count */
	PS->integ = sps;
	return( index );
}

/*name:
	freebuf

function:
	buffer freeing primitive

algorithm:
	get the addres of the buffer being freed ( passed is the buffer
	previous to the one freed )
	delink the buffer for it's present ring
	add the buffer to the free list
	possibly free a kernel buffer

parameters:
	prevbp	-	address of the buffer previous to the one to be
			freed in the present hierarchy of the ring.
			note: Things work all right if this is a pointer
			      to itself as in the case of a one buffer msg.

returns:
	zero if this was the last buffer in a message
	the address passed

globals:
	none

calls:
	ifreebuf
	freeb
	freekb
	spl_imp

called by:
	rmovepad	(impio.c)
	bytesout
	catmsg
	freemsg

history:
	initial coding 1/7/75 by S. R. Bunch
	initial debugging 1/7/75 S. F. Holmgren
	count in free list added 6Sep77 J.S.Kravitz
	parameter removed from freekb 6Set77 J.S.Kravitz
	count in free list moved to freeb 15Mar78

*/

freebuf( prevbp )
struct netbuf *prevbp;
{
	register struct netbuf *freedbuf;	/* buffer getting freed */
	register int bufno,sps;			/* kernel buffer index, old ps */

	if(!prevbp)				/* RJB 1978Mar31 */
		return 0;			/* RJB why is is this needed?*/
	sps = PS->integ;			/* save old ps */
	
	spl_imp();				/* will be messing with free list */
	freedbuf = prevbp->b_qlink;		/* buffer being liberated */
	prevbp = ifreebuf(prevbp);		/* delink freedbuf,reset prevbp */
	bufno = freedbuf->b_resv & b_memberof;	/* cleanup b_resv, get value */
	freeb(freedbuf);			/* add freedbuf to freelist */
	freekb ();				/* maybe free kerned buffer */
	PS->integ = sps;			/* restore priority */
	
	return( prevbp );			/* updated maybe 0 */
}

/*name:
	freekb

function:
	free kernel buffer primitive

algorithm:
	if all small network buffers associated with the large system
	buffer in question are currently in the freelist, and we have
	more kernel buffers than we really need then lets free the thing
	( possibly some more intelligent games could be played here
	  in terms of deciding when to release a kernel buffer, due
	  to the possibility of getting in a state where the net is
	  constantly procuring and releasing these large system buffers.
	)
	find all net buffers in the free list and free them
	re initialize the freelist
	call brelse and give the kernel buff back to the sys
	make his place available in the kernel buff holding table
	decrement the number of kernel buffers we have in our
	possession.

parameters:
	effectively the kernel buffer in which a small net buffer has
	been released.

returns:
	nothing

globals:
	net_b.kb=
	net_b.b_alloc=
	net_b.freeli
	net_b_per_k_b

calls:
	ifreebuf
	brelse( sys )

called by:
	freebuf

history:
	initial coding 1/7/75 by S. R. Bunch
	initial debugging 1/7/75 S. F. Holmgren
	modifed 1Apr77 with suggestion from S.M.Abraham (JSK)
		to correctly set new freelist ptr
	modified 3Jun77 by J. S. Kravitz to correct algorithm which checks
	for all net bufs being in free list. Old algorithm would sometimes
	miss a free block, and then the kernel buffer would stay in use
	forever.  Panic added. This entire mechanism for checking
	should be removed at the point that the free count in the
	buffer header is trusted.
	spl's removed 3Jun77 by J.S.Kravitz, since spl_imp was
	done in calling routine
	Test to determine if we shoud release buffers changed to hysteresis
	mechanism (parameter deleted) 6Sep77 J.S.Kravitz
	Code revised and improved 16Feb78 by Greg Noel
	Code for hysteresis release re-arranged for readability 12Mar78 JSK

*/

freekb ()
{
    register struct netbuf *bufp,*prev;	/*this buffer. its predecessor*/
    int found;		/* counter */
    int kb_index;
    char *newfreelist;		/* to point to new freelist */

    /*
     * if there are more free little buffers than are required by
     * hyster-value (which is in big buffer units), then we try to
     * find a big buffer to release. This may not be possible, since
     * we might still have 1 or more buffers in use from each of the
     * big buffers allocated
     */
    
    if (net_b.b_cntfree  > (net_b.b_hyster * net_b_per_k_b)) /* should we? */
    {
	for (kb_index = 0; kb_index < kb_hiwat; kb_index++)	/* try! */
	{
	    if ( (net_b.kb_[kb_index])				/* can we? */
	    &&   (net_b.kb_[kb_index]->b_freect == net_b_per_k_b)  )
	    {
		/*
		 * Yes, we can free this one -- find and
		 * remove all the netbufs in this kbuf
		 * from the free list
		 */
		prev = net_b.b_freel;		/*last buf in msg*/
		bufp = prev->b_qlink;		/*successor to last buf*/
		for(found=net_b_per_k_b;found>0;)	/*till all are found*/
		{
		    if( (bufp->b_resv&b_memberof)==kb_index )
		    {
			bufp->b_len = 0;
			newfreelist = ifreebuf(prev);	/*free it if so*/
			found--;		/*one down, found to go*/
		    }
		    else
		    {
			 prev = bufp;	/*advance both pointers*/
		    }
		    bufp = prev->b_qlink;	/*reset this in either case*/
		}	/* end of for all netbufs in this buffer */
		net_b.b_freel = newfreelist; /* point to free list, if any */
		brelse(net_b.kb_[kb_index]);	/*free kernel buffer*/
		net_b.kb_[kb_index] = 0;	/*reset pointer to 0*/
		net_b.b_alloc--;		/*dec count of kbufs*/
		/*
		 * decrement count by what we took out
		 */
		net_b.b_cntfree =- net_b_per_k_b;
	    }	/* if there are no buffers in use */
	}	/* for each buffer/buffer-slot */
    }	/* if we should try to release a buffer */
}

/*name:
	freemsg

function:
	totally reclaim a message

algorithm:
	keep freeing buffs until freebuf returns zero

parameters:
	msgp	-	message to be freed

returns:
	nothing

globals:
	none

calls:
	freebuf

called by:
	ncpclose	(ncpio.c)
	ncpread		(ncpio.c)
	to_ncp		(ncpio.c)
	hh		(impio.c)
	imp_dwn		(impio.c)
	daedes		(nopcls.c)
	impodone	(impio.c)

history:
	initial coding 1/7/75 by S. R. Bunch
	initial debugging 1/7/75 S. F. Holmgren

*/

freemsg(msgp)
struct netbuf *msgp;
{
	register struct netbuf *msg;

	msg = msgp;

	while(msg)msg=freebuf(msg);
}

/*name:
	getbuf

function:
	get a buffer

algorithm:
	if the free list is empty
		see if we can get another kernel buffer 
		if we cant then say we are waiting and wait for
		one of the small netbufs to be freed by someone.

	take first buffer in freelist 
	say that piece of a larger kernel buffer is no longer in free
	delink that buffer from the free list
	set its length to zero
	point it at itself
	return it

parameters:
	none

returns:
	buffer pointed at itself

globals:
	net.b_b_freel
	net_b.kb

calls:
	getkb
	sleep( sys )
	ifreebuf
	spl_imp

called by:
	appendb

history:
	initial coding 1/7/75 by S. R. Bunch
	initial debugging 1/7/75 S. F. Holmgren
	modified 8Jun77 by J.S.Kravitz to delete test of hi-wat...
	the intent of the change is the strategy is now to
	wait for a buffer if we have reached the high water mark
	note that the VDH driver must be modified to never come
	in here from an interrupt.	spurious comments deleted.
	Free list counter update added 6Sep77 J.S.Kravitz

*/

getbuf()
{
	int sps;			/*to save ps in*/
	register struct netbuf *freedbuf;
					/*pointer to obtained buffer*/

	sps = PS->integ;		/*save priority*/
	spl_imp();			/*lock out clock and below*/
	
	for (;;)
	/*
	 * there is a mild race here that we must allow for.
	 * if we sleep on the free list, we have no guarantee
	 * that we will get the buffer that someone was signaling about.
	 */
	{
	    if(net_b.b_freel == 0)	/*empty free list?*/
	    {
		getkb();		/*maybe get a kernel buffer*/
		if (net_b.b_freel)	/* if we have a buffer */
			break; 		/* go use it */
		net_b.b_need =| proc_need;  /*note that a process needs space*/
		sleep(&net_b.b_freel,NETPRI);/*sit and hope*/
	    }
	    else break;			/*we have one*/
	}
	/* note we're still at imp-priority */
	freedbuf = net_b.b_freel->b_qlink; /*addr of first buffer on free list*/
	/*decrement free buffer count for this buffer*/
	net_b.kb_[freedbuf->b_resv]->b_freect--;
	net_b.b_cntfree--;	/* decrement counter */
	net_b.b_freel = ifreebuf(net_b.b_freel);	/* chop off that buffer */
	PS->integ = sps;		/*restore prio*/
	freedbuf->b_len = 0;		/*setup buffer to look like a zero-length
					  one-buffer message*/
	freedbuf->b_qlink = freedbuf;	/*point at self*/
	
	return(freedbuf);
}

/*name:
	getkb

function:
	get a kernel buffer and chop it up into net smaller network
	buffers linked into the free list

algorithm:
	have we taken as many kernel buffers from the system as we should
		yes return zero
	increment the number of kernel buffers we have
	get a kernel buffer may have to sleep until one is available
	none of his buffers are in the free list
	find a hole in net_b.kb to keep around the address of the large buffer
	stick it in there 
	loop around chopping it into smaller netbuffers and linking those
	onto the free list

parameters:
	none

returns:
	0 if have taken max number of ker bufs already
	-1 if got another kernel buffer

globals:
	net_b.b_alloc=
	net_b.kb=
	<and the freelist thru freeb>

calls:
	getblk( sys )
	freeb
	spl_imp

called by:
	ncpopen		(ncpio.c)
	getbuf

history:
	initial coding 1/7/75 by S. R. Bunch
	initial debugging 1/7/75 S. F. Holmgren
	modified 8Jun77 by J.S.Kravitz to correct race condition:
		since increment of # allocated was done after
		the getblk, and since getblk could put us to sleep,
		it was possible to be asleep in getkb twice, and
		this made it possible for the search for hole in
		the buffer pointer table to fail, but the failure
		wasn't noticed and the free list pointer (next loc
		of memory) was stepped on. Table search reorganized
		to prevent error. Panic added, tho it should never
		be reached.
	modified 6Sep77 by J.S.Kravitz to keep free list counter
	changed dbufp to a pointer to a netbuf (that's what it is!) so that
	    address arithmetic is simplified (removed use of b_overhead and
	    net_b_size) 11Feb78 by Greg Noel
	UCBUFMODs added 19Feb78 Greg Noel
	Free count maintence moved to freeb 15Mar78 Greg Noel

*/

getkb()
{
	register struct buf *bufp;	/*points to buffers as we free them*/
	register index;			/*index of this kbuf in kb_ */
	register struct netbuf *dbufp;	/*data part of a buffer pointer*/
	int k;				/*just a counter*/
	int sps;			/* save priority */

	sps = PS->integ;		/* save priority */
	
	if(net_b.b_alloc >= kb_hiwat)
		return(0);			/*can't do it*/
	net_b.b_alloc++;		/*inc count of bufffers we now have*/
	spl0();				/* set it down for this */
	bufp = getblk(NODEV,0);		/*get a kernel buffer. may sleep*/
	spl_imp();			/* go back to non interrupt for this */
	bufp->b_freect = 0;		/*zero of his buffers are on free list*/
	for(index=0;index<kb_hiwat;index++) {	/*find hole in kb_*/
	    if(net_b.kb_[index]==0) {		/*got one*/
		net_b.kb_[index] = bufp;	/*store our buffer addr in hole*/
#ifndef UCBUFMOD
		dbufp = bufp->b_addr;	/*addr of data part of buffer*/
#endif UCBUFMOD
#ifdef UCBUFMOD
		dbufp = bufhead[index];	/*assign set of buffer headers*/
#endif UCBUFMOD
		for(k=0;k < net_b_per_k_b; k++)	/*once per netbuf*/
		{
		    dbufp->b_resv = index;	/*member of buffer "index" */
#ifdef UCBUFMOD
		    dbufp->b_loc = net_b.kb_[index]->b_paddr + k;
#endif UCBUFMOD
		    freeb(dbufp);		/*add to free list*/
		    dbufp =+ 1;			/*to next buffer*/
		}
		PS->integ = sps;		/* fix things */
		return(-1);			/*success code*/
	    }
	}
	panic ("getkb");
}

/*name:
	ifreebuf

function:
	delink a buffer 

algorithm:
	point at the buffer to be delinked
	update the the link of the buffer previous to the delinked
	point the buffer at itself
	if the previous is the same as the freed then return 0
	else the previous

parameters:
	prevbp	-	address of the buffer containing a qlink with
			the address of the buffer to be freed. This is 
			so that previous link can be easily updated 
			without having to loop thru the whole ring
			checking qlinks to find the previous.

returns:
	zero	if the buffer passed pointed to itself
	the address of the buffer passed

globals:
	none

calls:
	nothing

called by:
	ncpread		(ncpio.c)
	freebuf
	freekb
	getbuf
	imp_input	(impio.c)

history:
	initial coding 1/7/75 by S. R. Bunch
	initial debugging 1/7/75 S. F. Holmgren

*/

ifreebuf(prevbp)
struct netbuf *prevbp;
{
	register struct netbuf *p;		/*buf being removed*/


	/* should we check to see if the the ncpd is down? */
	if(!prevbp)
		return 0;				/* RJB 1978Mar31 */
	p = prevbp->b_qlink;			/*buffer getting freed*/
	prevbp->b_qlink = p->b_qlink;		/*point around p*/
	p->b_qlink = p;				/* pt freed bfr at itself */
	if(prevbp==p)return(0);			/*only one buf. now none*/
	return(prevbp);
}

/*name:
	linkbuf

function:
	link a buffer onto a message

algorithm:
	if the message is zero return the buffer address
	if buff is non zero
		point buffs qlink at the first buffer in the message
		point previous last buffers qlink at new buff
		point msg at new last buffer
	return new last buffer address

parameters:
	address of buffer to be linked to the end of the message
	address of the last buffer of a message( may be zero )

returns:
	pointer to the new last buffer of the message

globals:
	none

calls:
	spl_imp

called by:
	freeb
	appendb

history:
	initial coding 1/7/75 by S. R. Bunch
	initial debugging 1/7/75 S. F. Holmgren

*/

linkbuf(bufpp,msgpp)
struct netbuf *bufpp,*msgpp;

{
	register struct netbuf *bufp,*msgp;
	register sps;

	msgp = msgpp;
	bufp = bufpp;
	sps = PS->integ;

	spl_imp();
	if(msgp == 0) msgp = bufp;
	if(bufp)
	{
		bufp->b_qlink = msgp->b_qlink;
		msgp->b_qlink = bufp;
		msgp = bufp;
	}
	PS->integ = sps;
	return(msgp);
}

/*name:
	vectomsg

function:
	copy bytes from a vector into a message

algorithm:
	while there is data to move
		get a buffer
			if cant get buffer return zero
		update the message pointer to the one just procured
		calculate address and count of data to be added to
		this buffer
		update count of valid data in this buffer
		copy the data into the buffer
		update the amount of data left to copyin

parameters:
	vec	-	addre to get data from or param to data proc
	count	-	number of bytes to copyin from vec
	msgpaddr-	address of a word that contains the pointer to a 
			message	( essentially so can fake a call by name )
	dtaproc	-	0 means data from user
			non zero this is the address of a procedure
			to call with vec as the parameter.  That procedure
			will return a byte to be copied in.

returns:
	non zero	is number of bytes it couldnt transfer
	zero		transferred all bytes

globals:
	none

calls:
	appendb
	bcopyin

called by:
	to_ncp		(ncpio.c)
	allocate	(impio.c)
	sndnetbytes
	catmsg
	hh1		(impio.c)

history:
	initial coding 1/7/75 by S. R. Bunch
	initial debugging 1/7/75 S. F. Holmgren
	added dtaproc capability 8/27/76 S. F. Holmgren
	put in UCBUFMOD additions 18Feb78 Greg Noel.  Also fixed glitch in
	    dtaproc capability; vec addr wasn't incremented.  dtaproc not
	    used anywhere; why is in included?

*/

vectomsg(vec,count,msgpaddr,segflg)
int segflg;
char *vec;		/*vector being copied from. can be in user or
			  kernel space, as per value of dtaproc      */
int count;		/*count of bytes to move from vec           */
struct netbuf **msgpaddr;
			/*pointer to a word that is a pointer to a
			  message.  (i.e., pass the pointer address
			  so i can fake a call by name)		    */
{

	int size;	/*size to move in a chunk*/
	int (*dtaproc)();	/* to get chars from */
#ifdef UCBUFMOD
	int fubyte(), fkbyte();	/* potential choices for dtaproc */
#endif
	register char *p,*d; /*pointer to anything,pointer to data,resp.*/
	register int s; /*loop counter so its fast and size doesn't change*/

#ifndef UCBUFMOD
	dtaproc = segflg;
#endif UCBUFMOD
#ifdef UCBUFMOD
	dtaproc = (segflg==0) ? &fubyte	/*vector in user space*/
		: (segflg&01) ? &fkbyte	/*vector in kernel space*/
		: segflg;		/*user-defined space*/
#endif UCBUFMOD
	while(count > 0)
	{
	    if((p=appendb(*msgpaddr))==0)return(count); /*cant get space*/
	    *msgpaddr = p;		/*got one, update msg pointer*/
#ifndef UCBUFMOD
	    d = &p->b_data[0] + p->b_len;	/*address where data goes*/
#endif UCBUFMOD
#ifdef UCBUFMOD
	    d = p->b_len;			/*offset where data goes*/
#endif
	    size = min( (net_b_size - (p->b_len & 0377)),count );
					/*amount to move this time*/
	    p->b_len =+ size;		/* update amt valid data in bfr */
#ifndef UCBUFMOD
	    if( segflg )		/* special get dta proc? */
	    {
		for(s=size;s>0;s--)	/* copy from kernel space */
		    *d++ = (segflg&01) ? *vec++ : (*dtaproc)(vec);
	    }
	    else
	    {
		bcopyin(vec,d,size);	/* copy from user space */
		vec =+ size;		/* update addresses */
		d =+ size;
	    }
#endif UCBUFMOD
#ifdef UCBUFMOD
	    for(s=size;s>0;s--)		/* copy input vector to buffer */
		sbbyte(p->b_loc, d++, (*dtaproc)(vec++));
#endif UCBUFMOD
	    count =- size;		/* lower count left */
	}
	
	return(0);
}


/*name:
	sndnetbytes

function:
	to take a vector of bytes and format them so they can be
	dealt with by the imp output driver (this includes padding
	the message to an even number of bytes)

algorithm:
	get a buffer to build a buffer header( see buf.h )
	initialize the buffer header
	if sktp is non-zero
		then build imp-to-host and host-to-host leaders
		from socket information and link
		update the number of bytes to be sent
		and the number of bytes in the message
	decide how much to send from the first buffer
	copy the data into the rest building a message as needed
	if length of last buffer is odd, pad it with a byte (and update length)
	(	if only one buffer, update both b_len and b_wcount	)
	store away a pointer to the second buffer for imp output driver
	store away the message pointer so imp can release msg when done
	if more than a one buffer message set disable end msg
	link the completed buffer header into the imp output queue
	return zero

parameters:
	uaddr	-	address from which to get the bytes
	ucnt	-	number of bytes to get from uadddr
	sktp	-	address of a read socket to get host number and
			bytesize for the construction of net leaders.
			may be zero if have already constructed own leaders.
	link	-	if a leader is to be constructed, the link over which
			it is to be sent.
	flg	-	if non zero the data is gotten from kernel space
			zero the data is gotten from user space

returns:
	zero


globals:
	none

calls:
	appendb
	min
	vectomsg
	impstrat

called by:
	netwrite	(nrdwr.c)
	wf_send		(ncpio.c)
	sendalloc	(nrdwr.c)
	sendins		(nrdwr.c)

history:
	initial coding 1/7/75 by S. R. Bunch
	initial debugging 1/7/75 S. F. Holmgren
	padding to even length by M. Kampe, 3/76
	print changed to log_to_ncp Sep77 Greg Noel
	calculation of first byte of data area changed to be more meaningful
	    (b_overhead removed) 11Feb78 by Greg Noel
	UCBUFMODs added 19Feb78 Greg Noel

*/
#ifndef UCBUFMOD
sndnetbytes( uaddr,ucnt,sktp,link,flg )
char *uaddr;
int   ucnt;
int   sktp;
char  link;
{

	register char *bp;
	register char *ldrp;
	register char *firstbuf;
	char *msg;

	struct netleader
	{
		char l_type;
		char l_host;
		char l_link;
		char l_subtype;
		char l_pad1;
		char l_bsize;
		int  l_bcnt;
		char l_pad2;
	};

	/* make copy of standard UNIX buffer */
	
	bp = 0;			/* use bp as a counter for a bit */
	while( (firstbuf=appendb(0)) == 0 )
		if( bp++ == 5 )	/* have we tried five times */
		{
			log_to_ncp("No network buffers");
			return(0);
		}
	firstbuf->b_len = BUFSIZE;	/* say i/o buffer in there */

	bp = &firstbuf->b_data[0];
	bp->b_flags = B_SPEC;
	bp->b_addr = ldrp = bp + BUFSIZE;
	bp->b_wcount = 0;
	/* if need imp host and host host leaders make them */
	if (sktp)
	{
		firstbuf->b_len = BUFSIZE + 9;
		ldrp->l_type = 0;
		ldrp->l_host = (sktp->w_hostlink>>8);
		ldrp->l_link = link;
		ldrp->l_subtype = 0;
		ldrp->l_pad1 = 0;
		ldrp->l_bsize = link == 0 ? 8 : sktp->w_bsize;
		ldrp->l_bcnt = swab( ucnt/(ldrp->l_bsize>>3) );
		ldrp->l_pad2 = 0;
		bp->b_wcount = 9;
	}

	/* set amount to send in first buf */
	bp->b_wcount =+ min( (net_b_size - firstbuf->b_len),ucnt );

	/* move in data */
	msg = firstbuf;
	vectomsg( uaddr,ucnt,&msg,flg );

	/*	pad the last buffer of the message to an even length	*/
	if (msg->b_len & 01)
		msg->b_data[ msg->b_len++ ] = '\000';
	/*	in a one buffer message, the odd length is also in b_wcount */
	if (bp->b_wcount & 01)
		bp->b_wcount++;

	firstbuf = msg->b_qlink;	/* point at first bfr again */

	/* pt to second buffer in msg */
	bp->av_forw = firstbuf->b_qlink;
	/* leave around msg ptr so imp can release */
	bp->b_dev = msg;

	/* if multi buffer msg set disable endmsg flag */
	bp->b_blkno = ( firstbuf == msg ) ? 0 : 1;

	impstrat( bp );
	
	return( 0 );
}
#endif UCBUFMOD
/*
Instead of constructing a queue of message buffers, we just construct a
message queue.  This permits the I/O driver to do whatever it wants with
the stuff, besides being a more consistant mechanism.
*/
#ifdef UCBUFMOD
sndnetbytes( uaddr,ucnt,sktp,link,flg )
char *uaddr;
int   ucnt;
int   sktp;
char  link;
{
	char *msg;

	struct netleader
	{
		char l_type;
		char l_host;
		char l_link;
		char l_subtype;
		char l_pad1;
		char l_bsize;
		int  l_bcnt;
		char l_pad2;
	} netleader;

	msg = 0;	/* start with a null message */

	/* if need imp host and host host leaders make them */
	if (sktp)
	{
		netleader.l_type = 0;
		netleader.l_host = (sktp->w_hostlink>>8);
		netleader.l_link = link;
		netleader.l_subtype = 0;
		netleader.l_pad1 = 0;
		netleader.l_bsize = link == 0 ? 8 : sktp->w_bsize;
		netleader.l_bcnt = swab( ucnt/(netleader.l_bsize>>3) );
		netleader.l_pad2 = 0;
		vectomsg(&netleader, 9/*sizeof netleader*/, &msg, 1);
		/* sizeof netleader rounds to the next even byte.... */
	}

	/* move in data */
	vectomsg( uaddr,ucnt,&msg,flg );

	/*	pad the last buffer of the message to an even length	*/
	if (msg->b_len & 01)
		sbbyte(msg->b_loc, msg->b_len++, '\000');

	imp_output(msg);
	
	return( 0 );
}
#endif UCBUFMOD
/*name:
	ncp_bfrdwn

function:
	clean up the net buffer structures when the net software
	is going down

algorithm:
	release all kernel buffers
	stomp on relavent net_b words

parameters:
	none

returns:
	nothing

globals:
	net_b =

calls:
	brelse

called by:
	imp_dwn

history:
	initial coding 6/22/76 by S. F. Holmgren
	loop rewritten 03Jun77 by J. S. Kravitz, since the original
	didn't realize that there could be holes in the allocated
	buffer table
	Clear free list counter 6Sep77 J.S.Kravitz

*/

ncp_bfrdwn()
{
	int i;

	/* stomp on netbuffer freelist */
	net_b.b_freel = 0;

	net_b.b_need = 0;	/* noone needs anything */

	net_b.b_alloc = 0;

	net_b.b_cntfree = 0;	/* Show what we did */

	/* free all kernel buffers in use */
	for (i = 0; i < kb_hiwat; i++) {
		if (net_b.kb_[i]) {	/* if slot is in use */
			brelse (net_b.kb_[i]);
			net_b.kb_[i] = 0;
		}
	}
}
