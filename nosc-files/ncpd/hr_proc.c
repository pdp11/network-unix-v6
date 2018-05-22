#

/*	hr_proc.c	*/

/*	globals declared in this file:
		hr_proc
		hh_ilgth
		erp_buf
		rrp_buf

	functions declared in this file:
		hr_dcode
		hr_nop
		hr_cpsk
		hr_rfc
		hr_cls
		hr_eco
		hr_err
		hr_rst
		hr_rrp
		hr_all
		hr_data
		hl_find
*/

#include	"files.h"
#include	"hstlnk.h"
#include	"probuf.h"
#include	"kread.h"
#include	"kwrite.h"
#include	"hhi.h"
#include	"socket.h"
#include	"measure.h"
#include	"globvar.h"

/* SCCS PROGRAM IDENTIFICATION STRING */
char id_hr_proc[] "~|^`hr_proc.c\tV3.9E0\tJan78\n";

  extern hr_dcode();
  extern hr_nop();
  extern hr_cpsk();
  extern hr_rfc();
  extern hr_cls();
  extern hr_eco();
  extern hr_err();
  extern hr_rst();
  extern hr_rrp();
  extern hr_all();
  extern hr_data();
  extern struct socket *hl_find();
int	(*hr_proc[])()		/* the host-host read decoding vector */
{
	&hr_nop,	/* 0	nop */
	&hr_rfc,	/* 1	rts */
	&hr_rfc,	/* 2	str */
	&hr_cls,	/* 3	cls */
	&hr_all,	/* 4	all */
	&hr_nop,	/* 5	gvb */
	&hr_nop,	/* 6	ret */
	&hr_nop,	/* 7	inr */
	&hr_nop,	/* 8	ins */
	&hr_eco,	/* 9	eco */
	&hr_nop,	/* 10	erp */
	&hr_err,	/* 11	err */
	&hr_rst,	/* 12	rst */
	&hr_rrp,	/* 13	rrp */
};
char	hh_ilgth[14]	/* indexing by opcode of host-host command gives
			   length of command in bytes */
{
	1,	/* 0	nop */
	10,	/* 1	rts */
	10,	/* 2	str */
	9,	/* 3	cls */
	8,	/* 4	all */
	4,	/* 5	gvb */
	8,	/* 6	ret */
	2,	/* 7	inr */
	2,	/* 8	ins */
	2,	/* 9	eco */
	2,	/* 10	erp */
	12,	/* 11	err */
	1,	/* 12	rst */
	1,	/* 13	rrp */
};

/*name:
	hr_dcode

function:
	parses a host-host protocol message into host-host commands.

algorithm:
	set kr_p to beginning of data in message.
	loop while kr_bytes > 0:
		if opcode ( *kr_p ) is illegal:
			send error via hw_err
			log the error
			return
		decrease kr_bytes by length of command just processed.
		if there are not enough bytes remaining in the message to
		supply the parameters for the command:
			send error via hw_err
			log the error
			return
		increment appropriate statistics counter.
		call appropriate procedure via hr_proc.
		advance kr_p by the length of the command just processed.

parameters:
	none.

returns:
	nothing.

globals:
	kr_p=
	kr_buf
	kr_bytes=
	hhi_rrp
	hhe_ilop
	hhe_sps
	hh_ilgth
	measure.m_hr_ops[]=
	hr_proc[]

calls:
	hw_err
	statist
	hr_nop	thru hr_proc
	hr_rfc	thru hr_proc
	hr_cls	thru hr_proc
	hr_eco	thru hr_proc
	hr_err	thru hr_proc
	hr_rst	thru hr_proc
	hr_rrp	thru hr_proc

called by:
	ir_reglr

history:
	initial coding 12/21/74 by G. R. Grossman

*/
char *hr_txt[] { "NOP", "RTS", "STR", "CLS", "ALL", "GVB", "RET",
		 "INR", "INS", "ECO", "ERP", "ERR", "RST", "RRP" };

hr_dcode()
{
	register int	op;	/* will hold opcode of current command 
				   being checked/parsed */

	if(k_rdebug) log_bin("R: Protocol from", &host, 1);

	kr_p = &kr_buf.krr_data;	/* point global byte pointer at
					   command string */
	while ( kr_bytes > 0 )	/* loop while there are command bytes to
				   process */
	{
		if ( (op = *kr_p&0377) > hhi_rrp )	/* illegal opcode? */
		{
			hw_err(hhe_ilop,kr_p,kr_bytes<10?kr_bytes:10);
				/* log and send the error */
			return;
		}
		if ( (kr_bytes =- hh_ilgth[op]) < 0 )	/* insufficient
							   bytes for
							   command params? */
		{
			hw_err(hhe_sps,kr_p,kr_bytes+hh_ilgth[op]);
				/* log and send the error */
			return;
		}
		statist(&measure.m_hr_ops[op]);	/* update statistics */
		if(k_rdebug) log_bin(hr_txt[op], kr_p, hh_ilgth[op]);
		(*hr_proc[op])();	/* call appropriate procedure */
		kr_p =+ hh_ilgth[op];	/* update byte pointer */
	}
}

/*name:
	hr_nop

function:
	processes the host-host "nop" command.

algorithm:
	does nothing.
	calls hs_alive

parameters:
	none.

returns:
	nothing.

globals:
	none.

calls:
	hs_alive

called by:
	hr_dcode thru hr_proc

history:
	intial coding 12/22/74 by G. R. Grossman.
	calls hs_alive 6/7/76  by S. F. Holmgren

*/

hr_nop()
{
	hs_alive();
}

/*name:
	hr_cpsk

function:
	copies socket numbers from rfc's or cls's in the command stream
	into the appropriate places in skt_req, checking for validity
	of local socket.

algorithm:
	destination pointer is set to local socket field of skt_req.
	source pointer is set to local socket field of command.
	if either of the 1st 2 bytes of the local socket in the command
	are non-zero:
		return 1.
	copy the last 2 bytes of the local socket into skt_req.
	copy the foreign socket into the foregn socket field of skt_req.
	return 0.

parameters:
	none.

returns:
	0	if local socket was valid.
	1	otherwise.

globals:
	skt_req=
	kr_p

calls:
	nothing.

called by:
	hr_rfc
	hr_cls

history:
	initial coding 12/22/74 by G. R. Grossman.

*/

int	hr_cpsk()
{
	register char	*sbp,		/* source byte pointer */
			*dbp;		/* destination byte pointer */

	dbp = &skt_req.s_lskt[0];	/* will copy into local skt */
	sbp = &kr_p[5];			/* from local socket in rfc/cls */
	if ( *sbp++ | *sbp++ )		/* any of 1st 2 bytes !=0? */
		return(1);		/* just return failure code */
	*dbp++ = *sbp++;		/* copy 3rd byte of local skt */
	*dbp++ = *sbp++;		/* same for 4th byte */
	sbp = &kr_p[1];			/* will now copy foreign socket */
	while ( dbp < &skt_req.s_fskt[4] )	/* will copy 4 bytes */
		*dbp++ = *sbp++;
	return(0);			/* return success code */
}

/*name:
	hr_rfc

function:
	processes the "rts" and "str" host-host protocol commands.

algorithm:
	if the socket polarities are not complementary
	or the socket polarities are not correct for the command
	or the command is an rts and the link number is illegal:
		send bad parameter host--host error
		return
	if the command is an str and the byte size is not zero mod 8:
	refuse:	increment statistics counter for refused rfc's
		construct cls in hw_buf
		send cls via qup_pro
		return
	copy sockets to skt_req via hr_cpsk; if this results in an error:
		goto refuse
	set host from global host variable.
	if the command is an str:
		set byte size from command
		clear link filed to avoid spurious matches
	otherwise:
		set link number from command
	set skt_req.s_state to rfc
	clear skt_req.s_filep so if the request gets queued, the socket
	will not be seen as attached to a file. (1/10/75 grg)
	process the rfc via get_skt

parameters:
	none

returns:
	nothing.

globals:
	kr_p
	hhi_rts
	hhe_bpar
	hhi_str
	&measure.m_refrfc
	hw_buf=
	skt_req=
	si_rfc
	host

calls:
	hw_err
	statist
	qup_pro
	hr_cpsk
	get_skt
	hs_alive

called by:
	hr_dcode thru hr_proc

history:
	initial coding 12/26/74 by G. R. Grossman
	code added to zero file pointer in skt_req 1/10/75 by G. R. Grossman.
	code added to set host field in skt_req 7/9/75 by G. R. Grossman.
	code added to zero link field on str 9/18/75 by G. R. Grossman.
	calls hs_alive			     6/7/76  by S. F. Holmgren

*/

hr_rfc()
{
	register char	*p,		/* will point into command stream */
			*sbp,		/* source pointer for copying socket
					   numbers in refusal */
			*dbp;		/* destination pointer for same */

	struct				/* formats the command in the input
					   stream */
	{
		char	op;		/* op code */
		char	fskt0;		/* 1st byte foreign skt */
		char	fskt1;		/* 2nd   "     "     "  */
		char	fskt2;		/* 3nd   "     "     " */
		char	fskt3;		/* 4th	 "     "     " */
		char	lskt0;		/* 1st byte local skt */
		char	lskt1;		/* 2nd   "     "     "  */
		char	lskt2;		/* 3nd   "     "     "  */
		char	lskt3;		/* 4th   "     "     "  */
		char	lnk_bs;		/* link for rts, byte size for str */
	};

	extern char	hw_buf[];	/* so we can use protocol assembly
					   buffer */

	p = kr_p;			/* get copy of input stream pointer */
	if ( ((p->fskt3^p->lskt3)&1) == 0	/* bad polarity btw skts? */
	     || ((p->fskt3^p->op)&1) == 0	/* or bad for command? */
	     || ((p->op == hhi_rts) && (p->lnk_bs <2 || p->lnk_bs >71)) )
						/* or rts with bad link? */
	{
		hw_err(hhe_bpar,p,10);	/* send "bad parameter" error */
		return;
	}
	if ( (p->op == hhi_str) && ((p->lnk_bs&07) != 0))	/* str with byte size
							   not multiple of 8? */
	{
refuse:		statist(&measure.m_refrfc);	/* keep statistics */
		/* now we build cls in hw_buf */
		dbp = &hw_buf[0];	/* point at beginning of assembly
					   buffer */
		*dbp++ = hhi_cls;	/* op code */
		sbp = &p->lskt0;	/* point at local socket in rfc */
		while (dbp < &hw_buf[5])	/* copy local socket */
			*dbp++ = *sbp++;
		sbp = &p->fskt0;	/* now copy foreign socket */
		while (dbp < &hw_buf[9])
			*dbp++ = *sbp++;
		qup_pro(&hw_buf[0],9);	/* queue the cls for sending */
		return;
	}
	if (hr_cpsk())			/* copy sockets into skt_req; bad
					   local socket? */
		goto refuse;		/* go refuse if so */
	hs_alive();				/* say the host is up */
	skt_req.s_hstlnk.hl_host = host;	/* set host field */
	if ( p->op == hhi_str )		/* str? */
	{
		skt_req.s_bysz = p->lnk_bs;	/* get byte size from command */
		skt_req.s_hstlnk.hl_link = 0;	/* no link assigned yet */
	}
	else				/* must be rts */
		skt_req.s_hstlnk.hl_link = p->lnk_bs|0200;
			/* so get link number and set "send link" bit */
	skt_req.s_state = si_rfc;	/* set state for search */
	skt_req.s_filep = 0;		/* so if it gets queued it will not
					   be seen as attached to a file */
	get_skt();			/* go handle the rfc */
}

/*name:
	hr_cls

function:
	processes the host-host "cls" command.

algorithm:
	if socket polarity is wrong:
		send a "bad parameter" error message.
		return
	copy sockets into skt_req via cpsk; if local socket was erroneous:
		increment unmatched cls statistics counter
		return
	set host field form global variable.
	set socket state to si_cls
	process cls via get_skt

parameters:
	none

returns:
	nothing.

globals:
	kr_p
	hhe_bpar
	&measure.m_unmcls=
	skt_req=
	si_cls

calls:
	hw_err
	hr_cpsk
	statist
	get_skt

called by:
	hr_dcode via hr_proc

history:
	initial coding 12/27/74 by G. R. Grossman
	setting host field in skt_req added 9/23/75 by G. R. Grossman.

*/

hr_cls()
{
	register char	*p;	/* will point into input command stream */

	struct				/* formats the command in the input
					   stream */
	{
		char	op;		/* op code */
		char	fskt0;		/* 1st byte foreign skt */
		char	fskt1;		/* 2nd   "     "     "  */
		char	fskt2;		/* 3nd   "     "     " */
		char	fskt3;		/* 4th	 "     "     " */
		char	lskt0;		/* 1st byte local skt */
		char	lskt1;		/* 2nd   "     "     "  */
		char	lskt2;		/* 3nd   "     "     "  */
		char	lskt3;		/* 4th   "     "     "  */
	};

	p = kr_p;		/* get copy of command stream pointer */
	if ( ((p->fskt3^p->lskt3)&1) == 0 )	/* bad socket polarity? */
	{
		hw_err(hhe_bpar,p,9);	/* send bad parameter error */
		return;
	}
	if ( hr_cpsk() )	/* copy sockets to skt_req; bad local skt? */
	{
		statist(&measure.m_unmcls);	/* keep statistics on
						   unmatched cls's */
		return;
	}
	skt_req.s_hstlnk.hl_host = host;	/* set host field */
	skt_req.s_state = si_cls;	/* set socket state for matching */
	get_skt();			/* go process cls */
}

/*name:
	hr_eco

function:
	processes the host-host "eco" command.

algorithm:
	copy the second byte of the eco into the erp buffer
	queue the erp for sending via qup_pro
	say host is alive

parameters:
	none.

returns:
	nothing.

globals:
	hhi_erp
	kr_p

calls:
	qup_pro
	hs_alive

called by:
	hr_dcoe via hr_proc

history:
	initial coding 12/27/74 by G. R. Grossman

*/

char	erp_buf[2]	{hhi_erp,0};	/* erp assembly buf */

hr_eco()
{
	erp_buf[1] = kr_p[1];	/* copy second byte of eco */
	qup_pro(&erp_buf[0],2);	/* queue erp for sending */
	hs_alive();		/* say host is alive */
}

/*name:
	hr_err

function:
	processes the host-host "err" command.

algorithm:
	simply log it.

parameters:
	none.

returns:
	nothing.

globals:
	kr_p

calls:
	log_bin

called by:
	hr_dcode via hr_proc

history:
	initial coding 12/27/74 by G. R. Grossman

*/

hr_err()
{
	log_bin("hh_err:bytes read",kr_p,12);		/* log it */
}

/*name:
	hr_rst

function:
	processes the host-host "rst" command

algorithm:
	cleans up everything associated with the host via h_dead.
	say host is alive
	queue up an rrp to be sent 

parameters:
	none.

returns:
	nothing.

globals:
	host

calls:
	h_dead
	hs_alive
	qup_pro

called by:
	hr_dcode via hr_proc

history:
	initial coding 12/27/74 by G. R. Grossman
	update of host status file added 5/28/75 by G. R. Grossman
	calls hs_alive			 6/7/76  by S. F. Holmgren

*/

char	rrp_buf[1]	{hhi_rrp};	/* rrp buffer */

hr_rst()
{
	h_dead();			/* clean up host's table entries */
	hs_alive();			/* say host is alive */
	qup_pro(&rrp_buf[0],1);		/* queue rrp for sending */
}

/*name:
	hr_rrp

function:
	processes the host-host "rrp" command.

algorithm:
	say host is alive

parameters:
	none.

returns:
	nothing.

globals:
	host

calls:
	hs_alive

called by:
	hr_dcode via hr_proc

history:
	initial coding 12/27/74 by G. R. Grossman
	update of host status file added 5/28/75 by G. R. Grossman
	modified to call hs_alive	 6/07/76 by S. F. Holmgren

*/

hr_rrp()
{
	hs_alive();			/* say host is alive */
}

/*name:
	hr_all

function:
	handles the host_host ALL command.

algorithm:
	if we find a socket matching the link # or'ed with 0200 (to 
	distinguish send links) for the host via hl_find:
		if the socket is in the open state:
			arrange the parameters as ints and pass them to fi_all.
		otherwise:
			if the socket is not in the
				cls and close wait
				cls wait
				close to cls wait
			states:
				send a "socket not connected" error to the
				host via hw_err.
	otherwise there is no matching socket:
		send a "non-existent socket" error to the host via hw_err.

parameters:
	none.

returns:
	nothing.

globals:
	host
	kr_p
	ss_open
	ss_cacw
	ss_clsw
	ss_c2cw
	hhe_snc
	hhe_nxs

calls:
	fi_all
	hw_err

called by:
	hr_dcode via hr_proc.

history:
	initial coding 1/11/75 by G. R. Grossman

*/

hr_all()
{
	register char	*p;	/* copy of kr_p for better code */
	register struct socket	*s_p;	/* will point to socket struct we find,
					   if any */

	p = kr_p;
	if ( (s_p = hl_find( host|((p[1]|0200)<<8) )) != 0 )
		/* find matching host & send link? */
	{
		if ( s_p->s_state == ss_open )		/* connection open? */
			fi_all(s_p,
				(p[2]<<8)|(p[3] & 0377),
				(p[4]<<8)|(p[5] & 0377),
				(p[6]<<8)|(p[7] & 0377) );
					/* pass params as ints to fi_all */
		else			/* connection not open */
			if ( (s_p->s_state != ss_cacw)
			    &&(s_p->s_state != ss_clsw)
			    &&(s_p->s_state != ss_c2cw) )	/* bad state?*/
				hw_err(hhe_snc,p,8);	/* send err to host */
	}
	else		/* no match */
		hw_err(hhe_nxs,p,8);	/* send err to host */
}
/*name:
	hr_data

function:
	handles data received on connections. currently these should be
	only the socket numbers transmitted during user icp.

algorithm:
	if we find a matching socket via hl_find:
		if it is not in the open, cls wait, or close and cls wait
		states:
			send err command via hw_err.
			log the contents of the socket struct.
		otherwise:
			if the socket is in the open state:
				if it is an icp socket:
					if there are ay least 4 bytes
					in the message:
						pass to file level via
						fi_sktnm.
					otherwise:
						kill the socket to abort
						the icp.
				otherwise:
					log the occurrance.
			(NOTE that if the socket was not in the open
			state or not an icp socket the message is dropped on 
			the floor.)
	otherwise:	(there was no such socket)
		log a "no link" condition

parameters:
	none.

returns:
	nothing.

globals:
	host
	krr_link
	hhe_snc
	krr_type
	kr_bytes
	skt_size
	ss_open
	ss_cacw
	ss_clsw
	ksx_icp
	krr_data

calls:
	hl_find
	hw_err
	log_bin
	fi_sktnm
	kill_skt

called by:
	ir_reglr

history:
	initial coding 1/6/75 by G. R. Grossman

*/

hr_data()
{
	register struct socket	*s_p;	/* will point to socket struct
					   for which data was intended,
					   if any */

	if ( (s_p = hl_find(host|(kr_buf.krr_link<<8))) != 0 )
		/* find a matching host & link? */

		if ( !( (s_p->s_state == ss_open)	/* not in open state? */
		     ||(s_p->s_state == ss_cacw)/* or close and cls wait? */
		     ||(s_p->s_state == ss_clsw)) )	/* or cls wait? */
		{
			hw_err(hhe_snc,&kr_buf.krr_type,kr_bytes == 0?9:10);
				/* send and log error and data */
			log_bin(".... socket is",s_p,skt_size);
				/* log socket struct */
		}
		else
		{
			if(k_rdebug)
				log_bin("R: regular message", &kr_buf,
					kr_bytes+krr_ovhd);
			if ( s_p->s_state == ss_open )	/* in open state? */
				if ( s_p->s_sinx == ksx_icp )	/* icp skt? */
					if (kr_bytes >= 4)	/* enough? */
						fi_sktnm(s_p,
						         &kr_buf.krr_data);
							/* pass skt to file
							   level */
					else	/* not enogh bytes */
						kill_skt(s_p);	/* abort */
				else	/* not an icp socket */
				{
					log_bin("hr_data: rcv skt",s_p,
					        skt_size);	/* log skt */
					log_bin(".... msg was",&kr_buf.krr_type,
						krr_ovhd+kr_bytes);
						/* log the data */
				}
		}
	else	/* was no such socket */
		log_bin("no link", &kr_buf, kr_bytes+krr_ovhd);
}

/*name:
	hl_find

function:
	attempts to find a socket struct whose host and link match
	the host and link specified.

algorithm:
	loop thru all socket structs:
		if socket state is not null and host and link match:
			return address of socket struct.
	if fall out of loop, no match, return 0.

parameters:
	hslk	int which has host number in lower byte and link number
		in upper byte.

returns:
	pointer to socket struct if a match is found.
	0 if no match.

globals:
	sockets
	ss_null

calls:
	nothing.

called by:
	hr_data
	ir_rfnm
ir_re_x

history:
	initial coding 1/6/75 by G. R. Grossman

*/

struct socket	*hl_find(hslk)
int	hslk;
{
	register struct socket	*p;	/* will point to socket structs
					   during search */

	for ( p = &sockets[0] ; p < &sockets[nsockets] ; p++ )	/* skt loop */
		if ( (p->s_state != ss_null)	/* socket in use? */
		     &&(p->s_hstlnk.word == hslk) )	/* and host&link
							   match? */
			return(p);	/* return pointer to matched socket */
	return(0);		/* if we fell out, no match, return 0 */
}

