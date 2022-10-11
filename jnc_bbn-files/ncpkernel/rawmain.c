#
#include "../h/param.h"
#include "../h/netparam.h"
#include "../h/user.h"
#include "../h/file.h"
#include "../h/inode.h"
#include "../h/reg.h"
#include "../h/buf.h"
#include "../h/net.h"
#include "../h/netbuf.h"
#include "../h/imp.h"
#include "../h/rawnet.h"

#ifdef RMI
extern int localnet;
long ZERO0L 0;          /* fake a long constant zero */
/*
 *  system call routines:
 *      rawopen, rawclose, rawread, rawwrite
 *
 *  used by the above:
 *      rawrel, rawinode, rawrw
 *
 *  imp input interrupt routines:
 *      rawlget, rawhh, rawhostmsg, rawih, rawstrat
 *
 */

/*

name:   rawopen

function:
	to implement the open system call on /dev/net/rawmsg
algorithm:
	if the file being opened is not /dev/net/rawmsg
		if rawkernel is open
			give error to keep netopen from working
			release inode, and return -1
		return 0
	give /dev/net/rawmsg inode back to system
	error if ncp is not up (because it initializes imp, buffers, etc.)
	get user's parameters
	give error if they aren't legal
	try to get a file structure
	clear read and write rawskt pointers
	if can't get a rawfiletab entry, return with ERAWTAB
	say file is netfile, open, given mode
	try to get inodes for read or write socket as specified by user
	    and put entries in rawtab
		on error, release inodes (rawrel) and file structure
			and clear file descriptor entry
	return pointer to file structure

parameters:
	aip     -       inode of file being opened
	mode    -       pointer to open parameters in user space
returns:
	zero    -       not /dev/net/rawmsg (let netopen do its stuff)
	pointer to file structure - for communication over specified msg
	-1      -       error
globals:
	ncpopnstate     1: ncpkernel open; -1: rawkernel open; 0: neither
	LOMSG   -       lowest message parameter allowed

calls:
	bcopyin
	falloc           (sys)
	rawinode        (rawmain.c)
	spl_imp          (sys)
	spl0             (sys)
	getrawtab       (rawtable.c)
	putrawtab       (rawtable.c)
	rawrel          (rawmain.c)
called by:
	netopen (nopcls.c)

history:
	initial coding jsq bbn Fri Sep 22 1978
	changed to convert host to long form jsq bbn 12-6-78
		also for variable LOMSG
	use FRAW flag in file structure jsq BBN 3-14-79
	release file here rather than in rawrel on failure jsq BBN 3-14-79
	unused open options removed jsq BBN 3-21-79

*/

rawopen (aip, amode)
struct inode *aip;
#ifndef BUFMOD
int *amode;     /* bcopyin is used just in case this is odd */
#endif BUFMOD
#ifdef BUFMOD
char *amode;
#endif BUFMOD
{
	register *sktp;
	register struct netfile *fp;
	register i;
#ifdef BUFMOD
	char *cp;
#endif BUFMOD
	int mode;
	long host;
	int low, high;
	struct rawparams ropen;

	sktp = aip;
	if (!sktp || (sktp -> i_addr[0] != RAWDEV)) { /* netopen uses 0 aip */
#ifdef NCP
		if (ncpopnstate < 0) {  /* keep netopen from making a fool */
			u.u_error = ENCPNO;     /* of itself when rawkernel */
			if (sktp) iput (sktp);  /* is open */
			return(-1);
		}
#endif NCP
		return(0);
	}

	iput (sktp);   /* all we wanted to know was was it /dev/net/rawmsg */
	if (ncpopnstate == 0) {/* one of ncpkernel or rawkernel must be open*/
		u.u_error = ENCPNO;
		return(-1);
	}
#ifdef NCP
	if (ncpopnstate > 0) LOMSG = NCPLOMSG;
	else
#endif NCP
	LOMSG = RAWLOMSG;

#ifndef BUFMOD
	bcopyin (amode, &ropen, sizeof (ropen));
#endif BUFMOD
#ifdef BUFMOD
	for (cp = &ropen, i = 0; i < sizeof ropen; i++ )
		*cp++ = fubyte(amode++);
#endif BUFMOD
	mode = ropen.z_mode;
	host = ropen.z_host;
	if (!(host.h_net) && (host)) {  /* if not long and not anyhost... */
		host = stolhost(host.loword.lobyte);    /* convert to long */
	}
	host.h_logh = 0;        /* ignore logical host field */
	mode =% 3;              /* get rid of extra mode flags */
	low = ropen.z_lomsg;
	high = ropen.z_himsg;
	if (high == 0) high = low;
	mode++;                         /* convert to form for file struct */
	if ((mode < 1) || (mode > 3) || (host < 0) ||
	    (low < LOMSG) || (low > high) || (high > HIMSG)) {
		u.u_error = ERAWINV;
		return(-1);
	}

	if ((fp = falloc()) == 0) return(-1);
	spl_imp();
	for (i = 0; i < 3; i++) fp -> f_netnode [i] = 0;
	fp -> f_count++;        /* fp doesn't go in ncp's file_tab */
	fp -> f_flag = (FNET|FRAW|FOPEN|mode);
	spl0();

	for (i = 0; i < 2; i++) {
		if (mode & (i + 1)) {   /* if opening in this mode */
			sktp = rawinode (ROPENFLAGS);
			if (sktp == 0) {
				u.u_error = ERAWINO;
				goto nostruct;
			}
			fp -> f_netnode [i] = sktp;
			spl_imp();
			if (getrawtab(i, host, low, high, 0)) {
				u.u_error = ERAWMDUP;
				goto nostruct;
			}
			if (putrawtab(i, host, low, high, sktp) == 0) {
				u.u_error = ERAWTAB;
				goto nostruct;
			}
			spl0();
		}
	}

	return(fp);     /* success */
nostruct:               /* failure */
	spl0();
	rawrel(fp);
	fp -> f_flag = fp -> f_count = 0;
	i = u.u_ar0[R0];
	u.u_ofile[i] = 0;
	return(-1);
}
/*

name:   rawclose

function:
	implement the close system call on /dev/net/rawmsg

algorithm:
	if file to be closed was not in use by raw message facility
		do nothing
	if this is not last close
		do nothing
	release any messages
	release inodes (rawrel) and file structure
	return

parameters:
	afp     -       file pointer of msg to be closed

returns:
	zero    -       not raw file
	1       -       raw file, now closed

globals:
	none

calls:
	freemsg
	rawrel          (rawmain.c)

called by:
	netclose        (nopcls.c)

history:
	initial coding jsq bbn 9-22-78
	use fp -> flag & FRAW check, release fp directly jsq BBN 3-14-79

*/
rawclose(afp)
struct netfile *afp;
{
	register struct netfile *fp;

	fp = afp;

	if (fp == 0) return(0);
	if (!(fp -> f_flag & FRAW)) return(0);

	if (fp -> f_count > 2) {
		--fp -> f_count;
		return(1);
	}
	rawrel (fp);
	fp -> f_flag = fp -> f_count = 0;
	return(1);
}
/*

name:   rawread

function:
	to do the read system call on /dev/net/rawmsg

algorithm:
	check obvious errors
	if nothing available to return, just say so (never block if can
		be avoided)

	if this is the start of a new message
		make sure it really is, and find out how long it is
		save length in v_bytes for future reference

	find how much to do on this call (lesser of amt asked for and
		what's in the current message)
	give that much to user, noting how much actually sent
	update total bytes available, left in message, and sent this time

parameters:
	aip     -       pointer to a rawskt (open for reading)

returns:
	zero    -       not a rawmessage socket
	one     -       is, and read done

globals:
	none

calls:
	rawrw   (rawmain.c)
	min
	bytesout

called by:
	netread         (nrdwr.c)

history:
	initial coding by jsq bbn 9-22-78
	rawrw call jsq BBN 3-14-79

*/
rawread (aip)
struct rawskt *aip;
{
	register struct rawskt *sktp;
	register struct netbuf *nb;
	register amt;

	sktp = aip;

	if ((amt = rawrw(sktp)) >= 0) return(amt);
	if (sktp -> v_qtotal == 0) return(1); /* nothing to send */

	if (sktp -> v_bytes == 0) {     /* starting new message */
		nb = sktp -> v_msgq -> b_qlink;    /* get first buf */
#ifndef BUFMOD
		amt = (&nb -> b_data[0]) -> integ; /* message length */
#endif BUFMOD
#ifdef BUFMOD
		amt = fbword(nb->b_loc, 0);
#endif BUFMOD
		if ((amt < 2) ||
		    (amt > min(MAXMSG + RAWPAD, sktp -> v_qtotal))) {
			u.u_error = ERAWMLEN;
			sktp -> v_qtotal = 0;
			freemsg (sktp -> v_msgq);
			sktp -> v_msgq = 0;
			return(1);
		}
		sktp -> v_bytes = amt;
	}

	amt = min(u.u_count, sktp -> v_bytes);  /* to do on this syscall */
	amt =- bytesout(&sktp -> v_msgq, u.u_base, amt, 0); /* do it */
	sktp -> v_bytes =- amt;         /* left in this message */
	sktp -> v_qtotal =- amt;        /* update total to be read */
	u.u_count =- amt;               /* asked for but not read this time */
	return(1);
}
/*

name:   rawwrite

function:
	do write system call on /dev/net/rawmsg

algorithm:
	check obvious errors
	if starting a new message
		get a buffer (marked raw)
			return error if can't
		get length of message from user (as passed,
			includes length word and leader length)
		put a Unix style buffer header in the first buffer
			marked as used by network, and with contents
				starting just after self

	find out how much to get from the user this time and do it

	if we just got the last of the message from the user,
		do a little more tinkering with the message
		and give it to the imp

	tell user how much we took from him

parameters:
	aip     -       pointer to a rawskt (open for writing)

returns:
	zero    -       not a rawmessage socket
	one     -       is, and read done

globals:
	none

calls:
	rawrw   (rawmain.c)
	rawbuf          (rawtable.c)
	bcopyin
	min
	vectomsg
	impstrat

called by:
	netwrite        (nrdwr.c)

history:
	initial coding jsq bbn 9-22-78
	rawrw call jsq BBN 3-14-79
	fixed use of v_qtotal so it holds total bytes in que jsq BBN 3-14-79

*/
rawwrite(aip)
struct rawskt *aip;
{
	register struct rawskt *sktp;
	register char *firstbuf;
	register char *nb;
	int amt;
#ifdef BUFMOD
	int i;
#endif BUFMOD

	sktp = aip;

	if ((amt = rawrw(sktp)) >= 0) return(amt);
	if (sktp -> v_bytes == 0) {     /* if starting message */
#ifndef BUFMOD
		bcopyin (u.u_base, &amt, 2);
#endif BUFMOD
#ifdef BUFMOD
		firstbuf = &amt;        /* using firstbuf as temporary */
		nb = u.u_base;          /* using nb as temporary */
		for (i = 2; i > 0; i--) *firstbuf++ = fubyte(nb++);
#endif BUFMOD
		/* message length (including length word and leader) */
		if ((amt < 2) || (amt > MAXMSG)) {
			u.u_error = ERAWMLEN;
			return(1);
		}
		firstbuf = rawbuf();   /* get a buffer */
		if (firstbuf == 0) {
			return(1);
		}
		amt =- 2;
		u.u_count =- 2;
		u.u_base =+ 2;
		sktp -> v_msgq = firstbuf;      /* save as current one */
#ifndef BUFMOD
		firstbuf -> b_len = BUFSIZE;    /* putting a sysbuf header */
		nb = firstbuf + b_overhead;     /* in it starting here */
		nb -> b_flags = B_SPEC;         /* say owned by network */
		nb -> b_addr = nb + BUFSIZE;    /* starts after header */
		nb -> b_wcount = min((net_b_size - firstbuf -> b_len), amt);
					    /* get bytes in first net buf */
#endif BUFMOD
		sktp -> v_qtotal = 0;           /* nothing is now buffered */
		sktp -> v_bytes = amt;          /* but this much will be */
	}

	amt = min(sktp -> v_bytes, u.u_count);  /* on this syscall */
	amt =- vectomsg (u.u_base, amt, &sktp -> v_msgq, 0); /* do it */
	sktp -> v_qtotal =+ amt;                /* this much in que */

	if ((sktp -> v_bytes =- amt) == 0) {    /* all of message sent */
#ifndef BUFMOD
		firstbuf = sktp -> v_msgq -> b_qlink; /* first buffer */
		nb = firstbuf + b_overhead;     /* and sysbuf header */
		nb -> av_forw = firstbuf -> b_qlink;    /* point to buf#2 */
		nb -> b_dev = sktp -> v_msgq;   /* put where imp can release*/
		nb -> b_blkno = (firstbuf == sktp -> v_msgq) ? 0 : 1;
						/* if one buf, set endmsg */
#endif BUFMOD
		sktp -> v_qtotal = 0;           /* nothing is now buffered */
#ifndef BUFMOD
		impstrat (nb);
#endif BUFMOD
#ifdef BUFMOD
		impstrat(sktp->v_msgq);
#endif BUFMOD
	}

	u.u_count =- amt;       /* return amount done this time */
	return(1);
}
/*

name:   rawrel

function:
	release any buffers & inodes associated with the
	read or write f_netnode fields of a file structure

algorithm:
	if the given file structure has any such inodes
		remove any attached buffers
		remove from the raw connection table
		release inodes to the system

parameters:
	afp     -       pointer to a file structure

returns:        nothing

globals:
	none

calls:
	spl_imp         (sys)
	freemsg         (kerbuf.c)
	getrawtab       (rawtable.c)
	rmrawtab        (rawtable.c)

called by:
	rawopen         (rawmain.c)
	rawclose        (rawmain.c)
	rawclean        (rawmisc.c)

history:
	initial coding jsq bbn 9-22-78
	buffer freeing put here from freerbuf, which was removed,
	 and file freeing bumped up to rawopen and rawclose jsq BBN 3-14-79

*/
rawrel(afp)
struct netfile *afp;
{
	register struct netfile *fp;
	register *ip;   /* can't be struct inode * */
	register *j;
	int i;
	int sps;

	fp = afp;
	for (i = 0; i < 2; i++) {
		if (ip = fp -> f_netnode[i]) {
			sps = PS -> integ;
			spl_imp();

			ip -> v_flags =& ~n_open;       /* no longer open */

			/* free any buffers */
			if (ip -> v_qtotal || ip -> v_bytes) {
				freemsg (ip -> v_msgq);
			}
			ip -> v_bytes = ip -> v_qtotal = ip -> v_msgq = 0;

			/* remove from connection table */
			if (((j = getrawtab(i, ZERO0L, 0, 0, ip)) != 0) &&
			    (j != -1)) {
				rmrawtab(j);
			}

			/* free inode */
#ifndef MSG
			/* point ip at top of inode instead of i_addr array */
			ip =- 7;       /* (this kludge stolen from daedes) */
#endif MSG
			/* clean up inode */
			for( j = &ip->i_dev; j <= &ip->i_addr[8]; j++ )
				*j = 0;
			/* reset file socket pointer */
			fp->f_netnode[i] = 0;
			/* release inode */
			ip->i_flag = ip->i_count = ip->i_lastr = 0;
			PS -> integ = sps;
		}
	}
	return; /* file is released in rawopen or rawclose, but not rawclean*/
}
/*

name:   rawinode

function:
	find and initialize an inode for use as a raw msg

algorithm:
	look for the inode table for an inode
	if find one
		set up first part as in ncp connection inodes
		set up rawskt for raw msg
		return pointer to the rawskt
	else
		return zero

parameters:
	flags   -       to go in flags field of rawskt

returns:
	pointer to initialized rawskt
	or zero if none found

calls:
	spl7()          (sys)

called by:
	rawopen         (rawmain.c)

history:
	initial coding jsq bbn 9-22-78

*/
rawinode(flags)
{
	register struct rawskt *sp;
	register struct inode *ip;
	register int sps;

	for (ip = &inode[0]; ip < &inode[NINODE]; ip++) {
		if (ip -> i_count == 0) {
			sps = PS -> integ;
			spl7();
			ip -> i_flag = ILOCK;
			ip -> i_count = 2;              /* 2? */
			ip -> i_mode = IALLOC|IFCHR;
			ip -> i_nlink = 1;
			ip -> i_dev = ip -> i_number = -2;
			ip -> i_lastr = -1;
			ip -> i_size1 = ip -> i_size0 = 0;
#ifdef MSG
			ip -> i_gid = 0;
#endif MSG
			PS -> integ = sps;
#ifndef MSG
			sp = &ip -> i_addr[0];
#endif MSG
#ifdef MSG
			sp = ip;
#endif MSG
			sp -> v_conent = 0;
			sp -> v_flags = flags;
			sp -> v_bytes = 0;
			sp -> v_msgq = 0;
			sp -> v_qtotal = 0;
			sp -> v_proc = u.u_procp;
			return(sp);     /* return rawskt pointer */
		}
	}
	return(0);
}
/*

name:   rawrw
function: do initialization and obvious error checks for rawread & rawwrite
algorithm:
	if null sktp, give EBADF
	if not rmi sktp, let netread or netwrite handle it (return 0)
	if not open, give EBADF
	put pointer to current proc element in skt
	if user asked for no transfer, return done
	return continue

parameters:
	sktp    pointer to a RMI skt
returns:
	1       -      all done: return from rawread and netread (resp. write)
	0       -       let netread or netwrite handle it
	-1      -       let rawread or rawwrite attempt a transfer
globals: none
calls:  nothing
called by:      rawread, rawwrite
history: jsq BBN 3-14-79
*/
rawrw(aip)
struct rawskt *aip;
{
	register struct rawskt *sktp;
	sktp = aip;

	/* is this a valid socket pointer */
	if( sktp == 0 )
	{
		u.u_error = EBADF;
		return(1);
	}
	if (!(sktp -> v_flags & n_raw)) return (0);
	if (!(sktp -> v_flags & n_open)) {
		u.u_error = EBADF;
		return(1);
	}
	sktp -> v_proc = u.u_procp;

	if (u.u_count == 0) return(1);
	return(-1);
}
/*

name:   rawlget

function:
	to get a buffer for the leader of a raw message
		and maybe start read of data

algorithm:
	If we can get a buffer
		put it on the imp input que
		find how long the leader was
		copy it into the buffer, preceding it by the message length
			so far
		save the message length
		if there is any more of the message to read
			start the imp up again
	else
		if there is any more of the message not read, flush it

parameters:
	none

returns:
	1       -       got it
	0       -       none to get

globals:
	impi_msg        -       head of the imp message que
	impi_wcnt       -       what the imp was told to read last time
	imp_stat.inpwcnt -      what was left
	implilob        -       start of the leader which was read
	impi_mlen       -       to save the length of the message
	imp_stat.inpendmsg -    did we read the end of the message

calls:
	rawbuf          (rawtable.c)
	impread
	flushimp

called by:
	imp_input       (impio.c)
	rawih           (rawmain.c)

history:
	initial coding by jsq bbn 9-22-78
	converted for long or short leaders jsq bbn 12-6-78

*/
rawlget(){
	register struct netbuf *bp;
	register char *i, *j;
	int amt;
#ifdef BUFMOD
	int k;
#endif BUFMOD

	if (bp = rawbuf()) {
		impi_msg = bp;
		amt = 2 + ((impi_wcnt + imp_stat.inpwcnt) * 2);
		if ((amt < 2) || (amt > net_b_size)) {
			printf("rawlget: bad amount: %d\n", amt);
			freebuf(bp);
			impi_msg = 0;
			flushimp();
			return(0);
		}
#ifndef BUFMOD
		(&bp -> b_data[0]) -> integ = amt;
		for (i  = &bp -> b_data[2], j = implilob;
		    i < &bp -> b_data[amt];) *i++ = *j++;
		bp -> b_len = impi_mlen = amt;
		if (imp_stat.inpendmsg == 0) {
			impread (i, net_b_size - amt);
		}
#endif BUFMOD
#ifdef BUFMOD
		sbword(bp->b_loc,0,amt);
		for (k = 2, j = implilob; k < amt; k++)
			sbbyte(bp->b_loc, k, *j++);
		bp->b_len = impi_mlen = amt;
		if (imp_stat.inpendmsg == 0) {
			impread(bp->b_loc, amt, net_b_size - amt);
		}
#endif BUFMOD
		return(1);
	} else {
		if (imp_stat.inpendmsg == 0) {
			flushimp();
		}
		return(0);
	}
}
/*

name:   rawhh
function:
	to read all of a raw message except the leader
algorithm:
	update the message length with what was just read
	if we have reached the end of the message
		if we are flushing
			free the imp message que of its buffers
			and are through flushing
		else
			put the message length in the first data word of the
				first buffer of the message
			if anybody still wants the message
				give it to them
			else  throw it away (the user must have closed
					the connection)
		whether flushing or not, say we're done with this message
			and start another leader read
	else if we got the amount read which was last requested
		if flushing, continue doing so
		else, tell imp to give us some more
	else (not endmsg but the imp did not finish reading)
		must have gotten a spurious interrupt

parameters:
	none
returns:
	nothing
globals:
	impi_mlen       -       message length
	impi_wcnt       -       what the imp was told to read last time
	imp_stat.inpwcnt -      what was left
	imp_stat.inpendmsg -    did we read the end of the message
	impi_msg        -       the imp input message que
	impi_flush      -       are we flushing
	impi_con        -       connection for this message
calls:
	freemsg
	catmsg
	implread        -       defined in imp.h
	ihbget
	rawstrat        -       (rawmain.c)

called by:
	imp_input       (impio.c)
history:
	initial coding by jsq bbn 9-22-78, modelled after hh, and
	with some code taken directly from there.
	converted for long or short leaders jsq bbn 12-6-78
	changed input demultiplexing to use rawent, not rawskt jsq BBN 3-21-79

*/
rawhh(){
	register char *src, *dest;
	int cnt;
	struct netbuf *bp;

	cnt = (impi_wcnt + imp_stat.inpwcnt) * 2; /* find how much was read*/
	impi_mlen =+ cnt;                       /* add to total for message */
	if (impi_msg) impi_msg -> b_len =+ cnt; /* and total for buffer */
	if (imp_stat.inpendmsg) {
		if (impi_flush) {
			freemsg(impi_msg);
			impi_flush = 0;
		} else {
			bp = impi_msg -> b_qlink;
#ifndef BUFMOD
			(&bp -> b_data[0]) -> integ = impi_mlen;
#endif BUFMOD
#ifdef BUFMOD
			sbword(bp->b_loc,0,impi_mlen);
#endif BUFMOD
			if (impi_con) rawstrat(impi_con);
			else freemsg (impi_msg);
		}
		impi_msg = 0;
		implread();
	} else if (imp_stat.inpwcnt == 0) {
		if (impi_flush) {
			flushimp();
		} else {
			ihbget();
		}
	} else {
		printf("\nIMP:Phantom Intrp(Csri=%o)\n", imp_stat.inpstat);
	}
}
/*

name:   rawhostmsg

function:
	to find out if a host and msg for which a leader has come from
	the imp are in the read raw connection table

algorithm:
	convert msgid from leader into internal form
		(host was done by impinput)
	use inrawtab to see if it is in the table
	if so, return it

parameters:
	none

returns:
	pointer to a rawent on success
	zero otherwise

globals:
	impi_host
	imp.link
	imp.subtype

calls:
	inrawtab        (rawtable.c)

called by:
	imp_input       (impio.c)
	rawih           (rawmain.c)

history:
	initial coding by jsq bbn 9-22-78
	converted for long or short leaders jsq bbn 12-6-78
	return rawent, not rawskt jsq BBN 3-31-79

*/
rawhostmsg() {
	register *p;
	int messid;
	messid.hibyte = imp.link;          /* pick up the whole thing */
	messid.lobyte = imp.subtype;       /* it's backwards, so fix it */
	messid =>> 4;                      /* make it right adjusted */
	messid =& 07777;                   /* mask off any odd bits */

	if (p = inrawtab(0, impi_host, messid)) {
		return(p);
	} else {
		return(0);
	}
}
/*

name:   rawih

function:
	to put raw message imp to host leaders and host to host leaders
	with data <= 4 bytes on users' ques.

algorithm:
	get the type of the leader just read from the imp
	if it has a host and msgid
		see if anybody claims it
	if no hostmsgid
		if ELSEMSG is open say send to him
	if anybody wants the leader
		if can get a buffer and put the leader in it
			save user's rawent number
			give the user the buffer
		say message has been completely read
		if there was a hostmsgid (ordinary ih has nothing to do)
			start another leader read
	return, letting ih run if there was no hostmsgid.

parameters:
	none

returns:
	1       -       leader completely processed: return from ih
	0       -       let ih do something with the leader

globals:
	imp.type
	elseentry       (rawnet.h)
	impi_con
	impi_mlen
	impi_msg
	impi_adr
	impi_wcnt

calls:
	rawhostmsg      (rawmain.c)
	rawlget         (rawmain.c)
	catmsg
	implread        (imp.h)
	rawstrat        (rawmain.c)

called by:
	ih              (impio.c)


history:
	initial coding by jsq bbn 9-22-78
	converted for long or short leaders jsq bbn 12-6-78
	use impi_con instead of impi_sockt and call rawstrat jsq BBN 3-21-79

*/
rawih() {
	register i;
	register int *j;
	int ret;

	i = imp.type & 0137;
	j = 0;

	if  ((i == ih_stdmsg) || (i == ih_sideband) || (i == ih_rfnm) ||
	    ((i >= ih_hstd) && (i <= ih_ictrans)) ||
	    ((i >= ih_rta) && (i <= ih_ready))) {   /* has message id? */
		if (j = rawhostmsg()) {      /* in table? */
			ret = 1;/* we always process these completely */
		} else {
			ret = 0;/* may be in ncp connection table */
		}
	} else {        /* no message id */
		ret = 0;
		if (elseentry) {
			j = elseentry;
		}
	}
	if (j) {        /* if it goes to anybody we know */
		if (rawlget()) {/* and we can buffer it */
			rawstrat(impi_con = j);
		}
	}
#ifdef NCP
	/* if have completely processed leader or rawkernel open or no ncp */
	if (ret || (ncpopnstate < 0))
#endif NCP
	{
		/* tell ih to do nothing and start another leader read */
		ret = 1;
		impi_msg = 0;
		implread();      /* start another leader read */
	}
	return(ret);
}
/*

name: rawstrat
function: to put message received from the IMP on user's que according
	to connection table pointer passed.
algorithm:
parameters:     anent   -       pointer to a rawent
returns:        nothing
globals:        impi_mlen       length of following
		impi_msgq       IMP's input message que
calls:  awake   -       (awaitr.c)
	catmsg  -       (kerbuf.c)
called by: rawhh        (rawmain.c)
	rawih   -       (rawmain.c)
history:
	initial coding jsq BBN 3-21-79
	make sure that anent is still a valid rawent, i.e. that its
	y_rawskt field is not zero.    S.Y. Chiu : BBN 22Apr79

*/
rawstrat(anent)
struct rawent *anent;
{
	register *j;
#ifndef MSG
	register int *sitp;  /* used exclusively by await/capac code */
#endif MSG

/* It is possible for anent to be an invalid entry in the sense that its
 * y_rawskt field is 0. This is especially true if rawstrat is called
 * with impi_con from rawhh. In this case, impi_con is set after an
 * impread that is previous to the one that causes rawhh to be called.
 * Between these two impreads, the rawtable entry to which impi_con
 * points could have been made a free entry by having its y_rawskt field
 * zeroed.      S.Y. Chiu : BBN 22Apr79
 */

	if (j = anent -> y_rawskt) {
		j -> v_msgq = catmsg(j -> v_msgq, impi_msg);
		j -> v_qtotal =+ impi_mlen;
#ifndef MSG
		sitp = j;    /* awake user if awtenb */
		if (sitp = *(--sitp)) awake (sitp, 0);
#endif MSG
#ifdef MSG
		if (j -> itab_p) awake(j -> itab_p, 0);
#endif MSG
	}
	else
		freemsg(impi_msg);
}

#endif  RMI
