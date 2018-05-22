#

/*	send_pro.c	*/

/*	globals declared in this file:
		x_retries
		rst_buf

	functions declared in this file:
		mak_ldr
		send_pro
		rst_all
		chk_host
		qup_pro
		hw_s2b
		hw_rfc
		hw_cls
		hw_all
		hw_err
		hw_dwn
		pr_flush
*/

#include	"files.h"
#include	"hstlnk.h"
#include	"kwrite.h"
#include	"globvar.h"
#include	"probuf.h"
#include	"hhi.h"
#include	"leader.h"
#include	"kread.h"
#include	"socket.h"
#include	"io_buf.h"

/* SCCS PROGRAM IDENTIFICATION STRING */
char id_send_pro[] "~|^`send_pro.c\tV4.0E1\t7Jul78\n";

int	x_retries	1;	/* number of retries on protocol xmission */


/*name:
	mak_ldr

function:
	constructs the common protions of the kernel, im, and host leaders
	for a "send" write to the kernel.

algorithm:
	clears all the leader bytes to zero.
	fills in kernel opcode, host, and byte size.

parameters:
	none.

returns:
	nothing.

globals:
	kw_buf=
	host

calls:
	nothing.

called by:
	send_pro
	rst_all
	fi_ssn

history:
	initial coding 12/09/74 by G. R. Grossman.

*/

mak_ldr()
{
	register char	*byte_p;	/* used in clearing all leader bytes */

	for(	byte_p= &kw_buf;	/* clear leader bytes to zero */
		byte_p != kw_buf.kw_data->l_data;
		*byte_p++ = 0
	   );

	kw_buf.kw_op = kwi_send;	/* kernel write opcode */
	kw_buf.kw_data->l_host = host;	/* imp leader host field */
	kw_buf.kw_data->l_bysz = 8;	/* host leader byte size field */
}

/*name:
	send_pro

function:
	if appropriate, accumulates up to 120 bytes of integral host_host
	protocol commands and sends them via the kernel "send" write
	instruction, setting appropriate flags and counters.

algorithm:
	if there is a rfnm outstanding for the current host, return.
	set up leader via mak_ldr.
	copy the minimum of: (120) (number of bytes of available integral
	host-host command bytes) from probuf's in host's h_pb_q into
	kw_buf, incrementing h_pb_sent[host] for each probuf copied.
	set byte count in host leader.
	if host's h_pb_rtry = 0, set to x_retries.
	write kw_buf to the kernel.
	set host's rfnm bit in rfnm_bm.

parameters:
	none.

returns:
	nothing.

globals:
	h_pb_sent=
	h_pb_q
	h_pb_rtry=
	rfnm_bm=
	pro2send=
	host
	kw_buf=
	x_retries

calls:
	mak_ldr
	kw_write
	set_bit
	log_time
	printf

called by:
	main
	chk_host

history:
	initial coding 12/10/74 by G. R. Grossman.
	modified to check for something in proto queue and reset
	pro2send  S. F. Holmgren 1/20/76

*/

send_pro()
{
	register char	*byte_p,	/* for copying into kw_buf */
			*pb_bp;		/* for copying out of probufs */
	register int	n;		/* count for copying out of
					   probufs */
	int		byte_cnt;	/* counts down total bytes copied
					   from 120 */
	struct probuf	*pb_p,		/* for running thru host's probuf
					   queue */
			*stop;		/* stopper for above */


	/* check rfnm bit, return if set */
	if ( bit_on(&rfnm_bm[0],host) )		/* rfnm outstanding for host? */
		return;

	/* set up leader and initialize pointers */
	mak_ldr();
	byte_cnt = 120;		/* maximun bytes of integral commands we
				   may send */
	byte_p = kw_buf.kw_data->l_data;	/* point at data field in
						   buffer */
	if( h_pb_q[host] == 0 )
	{
		log_time();
		printf("Send_proto and no proto to send to host :%d\n",host);
		return;
	}
	pb_p = stop = h_pb_q[host]->pb_link; /* point at 1st probuf in
						hosts's q */

	/* main loop: copy probufs into kw_buf */
	do
	{
		if ( (byte_cnt =- pb_p->pb_count) >= 0) /* can we send this
							   probuf? */
		{
			pb_bp = pb_p->pb_text;	/* point at text in probuf */
			n = pb_p->pb_count;	/* byte count for copy */
			if( n <= 0 )
			{
				log_err("send_pro: probuf with zero count \n");
				return;
			}
			do	/* copy loop */
			{
				*byte_p++ = *pb_bp++;	/* copy a byte */
			}
			while (--n);	/* do it (byte count) times */
			h_pb_sent[host]++;	/* inc probufs sent count */
		}	/* end of probuf ok branch */
		else	/* otherwise quit */
			break;
	}
	while ( (pb_p = pb_p->pb_link) != stop );	/* loop til back
							   at beginning
							   of probuf q */

	/* finish leader */
	kw_buf.kw_data->l_bycnt = swab(byte_p - kw_buf.kw_data->l_data);
		/* host byte count is end of data - beginning of data */

	/* set retry count if appropriate */
	if ( h_pb_rtry[host] == 0 )	/* retries zero means they haven't been
					   set */
		h_pb_rtry[host] = x_retries;	/* so set them */

	/* write buffer to kernel */
	kw_write(byte_p - &kw_buf.lo_byte);

	/* set rfnm bit */
	set_bit(&rfnm_bm[0],host);
	/* reset pro2send */
	pro2send = 0;

}

/*name:
	rst_all

function:
	sends host-host resets to all possible (or only known) hosts 
	on the network.

algorithm:
	if there is protocol to send for the current host, sent it,
		cause we're going to change the host number
	if init_host zero
		if compile option RSTKNOWN is set
			open and scan the host name file, setting the
			known bit for each host we find
		else
			set the known bit for all hosts.

	do:
		find the next host; return if there are no more
		if there is protocol to send, send it so that
			it isn't forgotten when we change host number.
	while chk_host does not reset this new host;
parameters:
	none.

returns:
	nothing.

globals:
	init_host=
	host=

calls:
	chk_host
	fopen	(lib)
	getl	(lib)
	bit_on
	set_bit
	log_asc

called by:
	ir_reset
	ir_rfnm
	ir_re_x

history:
	initial coding 12/10/74 by G. R. Grossman.
	modifications for shutting off debugging and logging 1/9/75
	by G. R. Grossman.
	modified to send prior protocol by Greg Noel on 8/31/77
	modified to scan /usr/net/hnames file for known hosts (under
		compiletime switch) 25Jan78 by J.S.Goldberg at ill-nts
	modified to clean code and prevent the clobbering of the global host
	from causing the daemon to forget about queued up protocol.
		1978/Jul/14 R. Balocca and B. Greiner


notes:
	when the compiletime switch RSTKNOWN is defined, rst_all will
	scan the list of host names (/usr/net/hnames) and set a bit map
	if hosts that are known according to that scan.  otherwise, the
	bitmap is completely filled with ones, to force reset attempts 
	on all hosts.  if the open of the file fails, all hosts are
	assumed to be known (e.g. the map is filled with ones).

*/

	/* if RSTKNOWN is defined, only those hosts that are listed	*/
	/* in /usr/net/hnames will get a reset sent to them.		*/
	/* else, all hosts get one.					*/
#define RSTKNOWN	on

#define NUMHOSTS 256			/* move to probuf.h */
#ifdef RSTKNOWN
rst_all() {
    register
    char   *cp				/* ptr for hnames parsing	 */
           ;
    register
    int     h				/* for host number, and countr	 */
           ;
    struct io_buf   fbuf[1];
    char    line[100]			/* generous line buffer		 */
           ;
    static char h_kno_bm[NUMHOSTS / 8]      /* map of known hosts		 */
               ;			/* N.B.	could be external if other	 */
					/* routines have need of the bitmap	 */


    if (init_host == 0) {		/* assumes initial value of 0	 */
	if (fopen("/usr/net/hnames", fbuf) < 0) {
	    log_asc("rst_all: can't open /usr/net/hnames");
	    for (h = 0; h < NUMHOSTS / 8; h_kno_bm[h++] = 0377);
	}
	else {
	    for (h = 0; h < NUMHOSTS / 8; h_kno_bm[h++] = 0);
	    while ((h = getl(line, fbuf)) > 0) {
		line[h] = '\0';
		cp = line;
					/* skip hostname, till get white space	 */
		while (*cp != ' ' && *cp != '\t' && *cp)
		    cp++;
		if (*cp == '\0') {      /* paranoid of editable files	 */
		    log_asc("rst_all: bad line in hnames, viz:");
		    log_asc(line);
		    continue;
		}
		if ((h = atoi(cp)) & ~0377) {
					/* host num too big		 */
		    log_asc("rst_all: bad host# in hnames, viz:");
		    log_asc(line);
		    continue;
		}
		set_bit(h_kno_bm, h);
	    }
	    close(fbuf -> fid);
	}
    }					/* endof if( init_host == 0 )				 */
					/* look for next host, while the host number is legal		 */
    for (; ++init_host < NUMHOSTS;) {
	if (bit_on(h_kno_bm, init_host)) {
	    if (pro2send)
		send_pro();		/* send any queued protocol */
	    host = (init_host & 0377);  /* set host this host */
	    if (!chk_host())		/* host down? */
		return;
	}
    }
}
#endif
#ifndef RSTKNOWN
rst_all() {

 /* look for next host, while the host number is legal		 */
    for (; ++init_host < NUMHOSTS;) {
	if (pro2send)
	    send_pro();			/* send any queued protocol */
	host = (init_host & 0377);      /* set host this host */
	if (!chk_host())		/* host down? */
	    return;
    }
}
#endif

/*name:
	chk_host

function:
	checks if host is marked as up or if we are awaiting rfnm from host
	and, if neither is the case, sends a reset to the host.

algorithm:
	save result of or'ing results of examining the host's up and rfnm
	bits; if = 0:
              send rst via qup_pro
	return result.

parameters:
	none.

returns:
	0	if it sends a reset to the host.
	!=0	otherwise.

globals:
	host
	h_up_bm
	rfnm_bm

calls:
	bit_on
	qup_pro
	send_pro

called by:
	kr_odrct
	rst_all
	kr_ouicp

history:
	initial coding 1/17/75 by G. R. Grossman

*/

char	rst_buf[]{hhi_rst};	/* buffer containing a host-host rst */

chk_host()
{
	register int	h;		/* will hold host number */
	register int	res;		/* holds result of test for return */

	h = host;
	if ( (res = (bit_on(&h_up_bm[0],h) | bit_on(&rfnm_bm[0],h))) == 0 )
		/* neither up nor awaiting rfnm? */
	{
		qup_pro(&rst_buf[0],1);		/* send rst */
  send_pro();
	}
	return(res);				/* tell caller what we did */
}

/*name:
	qup_pro

function:
	queues up host-host protocol for sending.

algorithm:
	an attempt is made to get a probuf from pb_fr_q via q_dlink;
	if this fails:
		allocate a new probuf via alloc.
		increment n_pb, which counts the existing probufs.
	set count in probuf to the size of the command.
	copy the command into the probuf.
	enter the probuf in the host's h_pb_q via q_enter.
	set pro2send to cause transmission at end of current major cycle.

parameters:
	cmd_p	pointer to char[] containing the command.
	n	size of command in bytes.

returns:
	nothing.

globals:
	pb_fr_q=
	n_pb=
	pb_size
	h_pb_q=
	host

calls:
	q_dlink
	q_enter
	alloc	(sys)

called by:
	hw_rfc
	hw_cls
	hw_all
	hw_err
	hr_rfc
	hr_eco
	hr_rst

history:
	initial coding 12/29/74 by G. R. Grossman

*/

qup_pro(cmd_p,n)
char	*cmd_p;
int	n;
{
	register struct probuf	*pb_p;	/* will point to the probuf we use */
	register char	*sbp,		/* source pointer for copying cmd */
			*dbp;		/* dest      "     "     "     "  */

	if ( (pb_p = q_dlink(&pb_fr_q)) == 0 )	/* fail to get probuf? */
	{
		pb_p = alloc(pb_size);	/* get a new one */
		++n_pb;			/* increment informational counter */
	}
	pb_p->pb_count = n;		/* set byte count in probuf */
	dbp = &pb_p->pb_text[0];	/* destination for copy is text
					   field of probuf */
	for ( sbp = cmd_p;		/* source is command */
	      n > 0;			/* loop for n bytes */
	      n--	)
		*dbp++ = *sbp++;	/* copy a byte */
	q_enter(&h_pb_q[host],pb_p);	/* enter probuf in host's probuf q */
	pro2send = 1;			/* set send flag */
}

/*name:
	hw_s2b

function:
	copies socket numbers from socket struct to hw_buf for hw_rfc
	and hw_cls.

algorithm:
	the source byte pointer is pointed at the local socket field in
	the socket struct.
	the destination byte pointer is pointed at the 2nd byte of hw_buf.
	two zero bytes are put in the destination to serve as the first
	two bytes of the local socket.
	the local and foreign sockets are copied from the socket struct.

parameters:
	skt_p	pointer to the socket struct containing the socket
		numbers to be copied.

returns:
	nothing.

globals:
	hw_buf=

calls:
	nothing.

called by:
	hw_rfc
	hw_cls

history:
	initial coding 12/30/74 by G. R. Grossman

*/

hw_s2b(skt_p)
struct socket	*skt_p;
{
	register char	*sbp,	/* source byte pointer for copying */
			*dbp;	/*  dest   "      "     "     "    */
	register int	n;	/* counter for copying */

	sbp = &skt_p->s_lskt[0];	/* source is local socket field */
	dbp = &hw_buf[1];		/* dest is 2nd byte of command */
	*dbp++ = 0;			/* 1st byte of local socket */
	*dbp++ = 0;			/* "nd  "   "    "     "    */
	for (n = 6 ; n > 0 ; n-- )	/* now copy six bytes */
		*dbp++ = *sbp++;	/* one at a time */
}
/*name:
	hw_rfc

function:
	queues for sending the rfc appropriate to the socket struct
	specified.

algorithm:
	if the socket represents a send connection:
		set opcode to str.
		set last byte of command to byte size of connection.
	otherwise (receive connection):
		set op code to rts.
		set last byte to link number.
	copy socket numbers via hw_s2b.
	queue for sending via qup_pro.

parameters:
	skt_p	points to the socket struct defining the rfc to be sent.

returns:
	nothing.

globals:
	hw_buf=
	hhi_rts
	hhi_str

calls:
	hw_s2b
	qup_pro

called by:
	lsint_q
	rfc_slsn
	so_uinit

history:
	initial coding 12/30/74 by G. R. Grossman

*/

hw_rfc(skt_p)
struct socket	*skt_p;
{
	register struct socket	*s_p;	/* points to socket struct */

	s_p = skt_p;
	if ( s_p->s_lskt[1] & 1 )	/* send connection? */
	{
		hw_buf[0] = hhi_str;	/* op code is str */
		hw_buf[9] = s_p->s_bysz;	/* last byte is byte size */
	}
	else				/* receive connection */
	{
		hw_buf[0] = hhi_rts;	/* op code is rts */
		hw_buf[9] = s_p->s_hstlnk.hl_link;	/* last byte is link */
	}
	hw_s2b(s_p);			/* copy socket numbers */
	qup_pro(&hw_buf[0],10);		/* queue command for sending */
}

/*name:
	hw_cls

function:
	queues for sending the cls appropriate to the socket struct
	specified.

algorithm:
	set op code to cls.
	copy socket numbers via hw_s2b.
	queue for sending via qup_pro.

parameters:
	skt_p	pointer to socket struct defining the cls.

returns:
	nothing.

globals:
	hw_buf=
	hhi_cls

calls:
	hw_s2b
	qup_pro

called by:
	cls_rfcw
	cls_q
	clo_rfop
	clo_c2cw
	su_ut3
	tmo_q

history:
	initial coding 12/30/74 by G. R. Grossman

*/

hw_cls(skt_p)
struct socket	*skt_p;
{
	hw_buf[0] = hhi_cls;		/* set op code */
	hw_s2b(skt_p);			/* copy socket numbers */
	qup_pro(&hw_buf[0],9);		/* queue for sending */
}

/*name:
	hw_all

function:
	queues for sending the all command determined by the socket
	struct specified and the speified number of messages and bits.

algorithm:
	set op code to all.
	set link # from socket struct.
	copy message count, swapping bytes.
	set 1st 2 bytes of bits field to zero.
	copy 2nd word of bits field, swapping bytes.

parameters:
	skt_p	points to the socket struct descriibg the connection
		for which the all is being sent.
	msgs	number of messages to be allocated.
	bits	number of bits to be allocated.

returns:
	nothing.

globals:
	hw_buf=
	hhi_all

calls:
	qup_pro

called by:
	fi_sopn

history:
	initial coding 12/30/74 by G. R. Grossman

*/

hw_all(skt_p,msgs,bits)
struct socket	*skt_p;
int	msgs,bits;
{
	register char	*bp;	/* byte pointer for constructing command */
	register char	*BUG;	/* to avoid compiler bug in optimization */

	BUG =	/* compiler incorrectly uses bp below, so we save copy */
	bp = &hw_buf[0];	/* point at beginning of assembly buffer */
	*bp++ = hhi_all;	/* op code */
	*bp++ = skt_p->s_hstlnk.hl_link;	/* link number */
	*bp++ = msgs.hi_byte;	/* copy messages, swapping bytes */
	*bp++ = msgs.lo_byte;
	*bp++ = 0;		/* 1st 2 bytes of bits are zero */
	*bp++ = 0;
	*bp++ = bits.hi_byte;	/* copy bits, swapping bytes */
	*bp++ = bits.lo_byte;
	qup_pro( BUG,8 );	/* compiler uses bp instead of recalculating
	qup_pro( &hw_buf[0],8 );	/* queue for sending */
}

/*name:
	hw_err

function:
	queues for sending an err command with the specified code and
	containing the command bytes specified; also logs the received
	message containing the erroneous command and the err command
	sent.

algorithm:
	the op code is set to err.
	the error code is set as specified by the parameters.
	the specified command bytes are copied into the err command.
	if there were less than 10 command bytes, pad with zeroes.
	queue for sending via qup_pro.
	log the entire received message containing the ostensible error.
	log the err command just queued for sending.

parameters:
	code	the err code as specified by the host-host protocol.
	byte_p	pointer to the command bytes to be included in the
		err.
	n_bytes	number of command bytes to be included.

returns:
	nothing.

globals:
	hw_buf=
	hhi_err
	kr_buf
	kr_bytes
	kr_p

calls:
	qup_pro
	log_bin

called by:
	hr_rfc
	hr_cls

history:
	initial coding 12/30/74 by G. R. Grossman

*/

hw_err(code,byte_p,n_bytes)
int	code,n_bytes;
char	*byte_p;
{
	register char	*sbp,		/* source pointer for copying */
			*dbp;		/*  dest     "     "     "    */
	register int	n;		/* counter         "     "    */

	dbp = &hw_buf[0];		/* point at assembly buffer */
	*dbp++ = hhi_err;		/* set op code */
	*dbp++ = code;			/* set err code */
	sbp = byte_p;			/* pointer to command bytes to be
					   copied */
	for (	n = n_bytes;		/* set counter */
		n-- > 0 ;		/* done when counter is zero */
		*dbp++ = *sbp++		/* copy a byte each time */
	    );
	while ( n_bytes++ < 10 )	/* pad with zero until have 10 bytes */
		*dbp++ = 0;
	qup_pro(&hw_buf[0],12);		/* queue for sending */
	log_bin("hw_err: hh err detected;message",	/* log bad message */
		&kr_buf.krr_type,	/* start at type field */
		kr_p + kr_bytes - &kr_buf.krr_type );	/* size is current
							   pointer + bytes
							   left - start */
	log_bin(".... err sent was",&hw_buf[0],12);	/* log err sent */
}

/*name:
	hw_dwn

function:
	send an imp to host type 2 ( host going down ) message to
	the imp

algorithm:
	clean up the buffer with mak_ldr
	stick in a type 2 message
	say we wont know when will be up
	this is an emergency restart

parameters:
	none

returns:
	nothing

globals:
	kw_buf=

calls:
	mak_ldr
	kwrite

called by:
	daemon_dwn

history:
	initial coding 6/23/76 by S. F. Holmgren

*/
hw_dwn()
{
	register char *p;

	/* clean up hw_buf */
	mak_ldr();

	/* build a host going down message */
	p = &kw_buf.kw_data[0];
	*p++ = 2;		/* host down op code */
	*p++ = 0;		/* no destination */
	*p++ = 0377;		/* we dont know when coming back */
	*p++ = 0350;		/* this is an emergency restart */

	kw_write( p - &kw_buf.lo_byte );
}

/*name:
	pr_flush

function:
	flushes the protocol buffer queue for the current host.

algorithm:
	while q_dlink yields a non-zero pointer on delinking a probuf
	from the host's h_pb_q, enter the probuf in the free probuf q,
	pb_fr_q.
      indicate that there is no protocol in flight

parameters:
	none.

returns:
	nothing.

globals:
	host
	pro2send=
	h_pb_q[host]=
	pb_fr_q=
      h_pb_sent[host]=

calls:
	q_dlink
	q_enter

called by:
	h_dead

history:
	initial coding 1/2/75 by G. R. Grossman
	modified to reset pro2send by S. F. Holmgren 1/20/76
      modified to step on h_pb_sent by Greg Noel 8/31/77

*/

pr_flush()
{
	register struct probuf	*p;	/* holds pointer to probuf between
					   its being delinked from host's
					   h_pb_q and linked into pb_fr_q */

	while ( p = q_dlink(&h_pb_q[host]) )	/* was q non-empty? */
		q_enter(&pb_fr_q,p);		/* put probuf in free q */
	pro2send = 0;				/* say no protocol to send */
      h_pb_sent[host] = 0;
}

