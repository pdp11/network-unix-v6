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

/*
 *  await and capacity:
 *      rawcap, rawawk
 *
 *  status system calls:
 *      rawstat, rawsstat
 *
 *  open and close for /dev/rawkernel:
 *      rwkopen, rwkclose, rawclean
 *
 *  dummies for when the ncp is not compiled:
 *      netopen, netclose, netread, netwrite, netstat
 */

/*

name:   rawcap

function:
	to see if a netfile is an RMI file and give capacities if so

algorithm:
	if the file structure is not raw
		return(0), for not raw file
	find the capacities     (defaults are -1 if no rawskt)
	Read:   the number of bytes left to be read in the input que,
	write:  if a message being written, min of
		    rest of message,
		    remainder of current buffer plus free raw buffer capacity
		else min of
		    maximum message size, free raw buffer capacity
	return one.

parameters:
	afd     -       pointer to a file structure
	v1      -       pointer to place for read capacity
	v2      -       same for write capacity

returns:
	0       -       not rawmessage file
	1       -       capacities put in appropriate places

globals:
	rbuf_count      (rawnet.h)
	rbuf_max        (rawnet.h)

calls:
	min

called by:
	capac           (ken/awaitr.c)

history:
	initial coding by jsq bbn 9-29-78
	fp_is_raw call removed, fp -> f_flag & FRAW used instead
		jsq BBN 3-14-79
	capacs fixes jsq BBN 3-14-79

*/

rawcap (afd, v1, v2)
struct netfile *afd;
int *v1, *v2;
{
	register int i;
	register int *j;
	register struct rawskt *sp;

	if (!(afd -> f_flag & FRAW)) return(0);
	j = &afd -> f_netnode[0];
	*v2 = *v1 = -1;
	if (sp = j[f_rdnode]) {
		*v1 = sp -> v_qtotal;
	}
	if (sp = j[f_wrtnode]) {
		i = (rbuf_max - rbuf_count) * net_b_size;
		if (sp -> v_bytes) {
	    *v2 = min(sp -> v_bytes, i+(net_b_size - sp -> v_msgq -> b_len));
		} else {
			*v2 = min(MAXMSG, i);
		}
	}
	return(1);
}
/*

name:   rawawk

function:
	awake any processes which have enabled await on the write end
	of a rawmessage msg

algorithm:
	look through the write rawtab for any open msgs which are await
		enabled, and awake them

parameters:
	none

returns:
	nothing

globals:
	w_rawtab        (rawnet.h)

calls:
	awake           (ken/awaitr.c)

called by:
	freebuf         (kerbuf.c)

history:
	initial coding by jsq bbn 9-29-78
*/

rawawk()
{
	register struct rawentry *rp;
	register int *s;

	for (rp = &w_rawtab[0]; rp < &w_rawtab[RAWTSIZE]; rp++) {
#ifndef MSG
		if ((s = rp -> y_rawskt) && (s = *(--s))) {
			awake(s, 0);
		}
#endif MSG
#ifdef MSG
		if ((s = rp->y_rawskt) && (s->itab_p)) {
			awake(s->itab_p, 0);
		}
#endif MSG
	}
	return;
}
/*

name:   rawstat
function:
	to copy rawtab into a user's space, and to give its size
algorithm:
	if an odd address was given, return an error
	else if the address was non-zero
		copy rawtab into user's space
	in any case, return its size
parameters: none
returns:    RAWTSIZE (to user via u.u_ar0[R0])
globals:
	r_rawtab        (rawnet.h)
	w_rawtab        (rawnet.h)
calls:
	copyout         (sys)
called by:
	system
history:
	initial coding by jsq bbn 10-4-78: basically stolen from gprocs code
*/

rawstat()
{
	register int p;

	if ((p = u.u_ar0[R0]) & 1)      /* get addr of buffer from R0 */
	{                               /* won't allow odd address */
		u.u_error = EINVAL;
		return;
	}
	if (p) {                /* if adr not 0, copyout rawtab to user */
		copyout(&r_rawtab[0],p,sizeof(r_rawtab));
		p =+ sizeof(r_rawtab);
		copyout(&w_rawtab[0],p,sizeof(w_rawtab));
	}
	u.u_ar0[R0] = RAWTSIZE; /* return RAWTSIZE as value in any case */
}
/*

name:   rawsstat

function:
	to do something with the stat system call on a raw file
algorithm:
	if raw, give an error
parameters:
	address of a network file
	user address
returns:
	1: raw file; 0: not raw file.
globals:
	none
calls:  nothing
called by:
	netstat
history:
	initial coding jsq bbn 10/19/78
	fp_is_raw exchanged for FRAW check jsq BBN 3-14-79
*/
rawsstat(afp, usradd)
{
	if ((afp -> f_flag & FRAW)) {
		u.u_error = EBADF;
		return(1);
	}
	return(0);
}
/*

name:   rwkopen

function:
	To open /dev/rawkernel, grabbing kernel buffers and initializing
	the imp

algorithm:
	If either /dev/rawkernel or /dev/ncpkernel is already open
		give an error and return

parameters:     (both ignored)
	dev             device of /dev/rawkernel
	rw              mode of open

returns:
	nothing         for child spawned by impopen
	never           for parent

globals:
	ncpopnstate     1: ncpkernel open; -1: rawkernel open; 0: neither
	init_b_hyster
	net_b.b_hyster
	net_b.b_alloc

calls:
	getkb           (kerbuf.c)
	impopen         (impio.c)

called by:
	system

history:
	initial coding jsq bbn 10-5-78  (mostly stolen from ncpopen)
*/
rwkopen(dev, rw)
{
	if (ncpopnstate) {
		u.u_error = ENCP2;
		return;
	}
	net_b.b_hyster = init_b_hyster;	/*setup net buffer surplus*/
	while(net_b.b_alloc < init_b_hyster) getkb();
	ncpopnstate--;                  /* mark us open */
	impopen();			/* bring up the imp */
	return;
}
/*

name:   rwkclose

function:
	to close /dev/rawkernel, telling the imp to die,

algorithm:
	clean up
	say rawkernel is closed
	tell imp about it

parameters:     (both ignored)
	dev             device of /dev/rawkernel
	rw              mode of open

returns:
	nothing

globals:
	ncpopnstate     1: ncpkernel open; -1: rawkernel open; 0: neither

calls:
	wakeup          (sys)
	rawclean        (rawmisc.c)

called by:
	system

history:
	initial coding jsq bbn 10-5-78
	"rawdaemon down" removed jsq BBN 3-14-79
*/
rwkclose(dev, rw)
{
	rawclean();
	ncpopnstate = 0;
	wakeup (&imp);
	return;
}
/*

name:   rawclean
function:       to clean up rawskts and mark them closed
algorithm:
	look down system's file table (since rmi doesn't have one)
		if this is a raw file
			use rawrel to clean up

parameters:     none
returns:        nothing
globals:
	file, NFILE     (file.h)

calls:  rawrel          (rawmain.c)
called by:
	rwkclose        (rawmisc.c)
	ncpclose        (ncpio.c)
history:
	initial coding jsq bbn 1-16-79
	fp_is_raw check exchanged for FRAW check, and freerbuf for rawrel
		jsq BBN 3-14-79
*/
rawclean(){
	register struct netfile *fp;

	for (fp = &file[0]; fp < &file[NFILE]; fp++) {
		if (fp -> f_count && (fp -> f_flag & FRAW)) {
			rawrel(fp);
		}
	}
}
/*

name:   netopen, netclose, netread, netwrite, netstat
function:
	to dummy the real ones when the ncp code is not compiled
algorithm:
	call the appropriate raw routines
parameters:
	those of the things being dummied
returns:nothing
globals:none
calls:
	rawopen, rawclose, rawread, rawwrite    (rawnmain.c)
	rawsstat        (rawmisc.c)
called by:
	system
history:
	initial coding jsq bbn 10-5-78
*/
#ifndef NCP
netopen( aip,mode )
struct inode *aip;
{
	if (rawopen(aip, mode)) return;
	if (aip) iput(aip);
	u.u_error = EBADF;
}

netclose( afp )
struct netfile *afp;
{
	if (rawclose(afp)) return;
	u.u_error = EBADF;
}

netread( fp, aip )	/* handles user reads -- called from rdwr of */
int *fp;
struct inode *aip;	   /*net/sys/sys2.c  */
{
	if (rawread(aip)) return;
	u.u_error = EBADF;
}

netwrite( aip )
struct wrtskt *aip;
{
	if (rawwrite(aip)) return;
	u.u_error = EBADF;
}

netstat( filep, usraddr)
{
	if (rawsstat(filep, usraddr)) return;
	u.u_error = EBADF;
}
#endif NCP
#endif RMI
