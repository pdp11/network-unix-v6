#

/*	kr_dcode.c	*/

/*	globals declared in this file:
		kr_proc

	functions declared in this file:
		kr_dcode
		kr_open
		kr_odrct
		kro_rfskt
		kr_oicp
		kr_ouicp
		kr_osicp
		kr_rcv
		kr_close
		kr_timo
		kr_reset
*/

#include	"files.h"
#include	"hstlnk.h"
#include	"socket.h"
#include	"kread.h"
#include	"kwrite.h"
#include	"globvar.h"
#include	"impi.h"
#include	"measure.h"

/* SCCS PROGRAM IDENTIFICATION STRING */
char id_kr_dcode[] "~|^`kr_dcode.c\tV3.9E1\t11Mar78\n";

  extern kr_dcode();
  extern kr_open();
  extern kr_odrct();
  extern kro_rfskt();
  extern kr_oicp();
  extern kr_ouicp();
  extern kr_osicp();
  extern kr_rcv();
  extern kr_close();
  extern kr_timo();
  extern kr_reset();
  extern kr_log();


struct {
	int (*kr_proc)();	/* the kernel read decoding transfer vector */
	char *kr_txt;		/* text describing the meaning of operation */
} kr_proc[] {
	&kr_open,	"R: user open",
	&kr_rcv,	0,	/* R: network protocol */
	&kr_close,	"R: user close",
	&kr_timo,	"R: timeout",
	&kr_reset,	"R: reset request",
	&kr_log,	0,	/* R: log from kernel */
};

/*name:
	kr_dcode

function:
	decodes an instruction from the kernel ( the result of a read on
	the ncp kernel file ).

algorithm:
	check opcode; if bad:
		log error
		return
	otherwise, call appropriate procedure.

parameters:
	none.

returns:
	nothing.

globals:
	files
	sockets
	kr_proc
	kri_reset
	kr_buf

calls:
	log_bin
	kr_open		thru kr_proc
	kr_rcv			"
	kr_close		"
	kr_timo			"
	kr_reset		"

called by:
	main

history:
	initial coding 12/12/74 by G. R. Grossman.
	kr_reset added 01/7/75 by S. F. Holmgren
	kr_log added 8/2/77 by J. G. Noel
	debug logging moved here from main 7/11/78 by Greg Noel

*/

kr_dcode()
{
	register int	op;	/* will hold read opcode */
	register char *txt;	/* text describing opcode read for debugging */

			/*The test below, revised from original version,
			  will make it easier to add new op codes later*/
	if( (op = kr_buf.kr_op&0377) >= ((sizeof kr_proc)/sizeof kr_proc[0]) )	
	{
		log_bin("kr_dcode: bad read opcode",&kr_buf.lo_byte,kr_bytes);	
			/* log error */
		return;
	}
	if (k_rdebug && (txt = kr_proc[op].kr_txt))	/* if read debugging enabled */
		log_bin(txt,&kr_buf,kr_bytes);	/* log what we read */
	(*kr_proc[op].kr_proc)();	/* call correct procedure */
}

/*name:
	kr_open

function:
	initial decoding of open command.

algorithm:
	if there already exists a file with the same kernel id
	( i.e., the kernel and daemon have got out of sync ):
		log the occurrance via log_bin
		set the kernel id of the already existent file
		to a nonsense value.
	tests open type byte; if direct bit is set:
		calls kr_odrct
	otherwise
		calls kr_oicp

parameters:
	none.

returns:
	nothing.

globals:
	fid_null
	kr_buf.kro_id
	kr_buf.kro_type
	otb_drct
	f_id

calls:
	kr_odrct
	kr_oicp
	f_by_id
	log_bin

called by:
	kr_dcode

history:
	initial coding12/12/74 by G. R. Grossman
	check for duplicate kernel file id's added 9/17/75 by G. R. Grossman

*/

kr_open()
{
	register char	*f_p;	/* holds result of checking
					   for already existent file
					   w/ same kernel id */

	if ( ( f_p = f_by_id ( kr_buf.kro_id ) ) != (-1) ) /* already a file w/
							   same id? */
	{
		log_bin ( "kr_open: duplicate kernel id",
			  &kr_buf.kro_id, 2);	/* log the occurrance */
		f_p->f_id = fid_null;		/* wipe out id of old file */
	}
	if ( kr_buf.kro_type & otb_drct )	/* direct open? */
		kr_odrct();
	else
		kr_oicp();
}
/*The routines kr_odrct and kr_oicp both return error codes, which are 
  not being checked here. Since there are a number of possible reasons
  why an error might be returned, someone may later want to add such 
  checking. For most purposes it is not needed. (after all, it has not 
  been missed up to this time) K.Kelley March 78 */

/*name:
	kr_odrct

function:
	handles non-icp opens.

algorithm:
	initialize state variables.
	if relative open:
		if the relative file dos not exist or is not open:
			bad parameter error.
		if the local socket is > 7:
			bad parameter error.
		compute the local socket from the socket base of the relative
		file plus the local socket given in the open.
	if absolute open:
		if local socket is zero:
			if type is not init:
				bad parameter error.
			if cannot assign a socket number:
				no-resource error.
			generate correct gender by or-ing in the inverted
			low-order bit of the foreign socket.
		if local socket is non-zero:
			if it is in the ncp-daemon's allocating name space:
				bad paramter error.
			swap bytes of local socket.
	if general listen:
		if foreign socket is zero, set wild flag.
	if not general listen:
		if host is zero:
			bad parameter error.
	if relative and wild flag not set:
		if foreign socket <= 7:
			use foreign socket as relative offset and compute
			absolute foreign socket from relative file via
			kro_rfskt.
		if foreign socket >7:
			treat as absolute and just swap bytes.
	if absolute open:
		swap bytes of foreign socket.
	if simplex and wild flag not set:
		if low-order bits of local and foreign sockets are equal:
			bad parameter error.
	if cannot, via f_make, allocate file or socket structs:
		no-resource error.
	set file state to data open wait.
	set file's socket base to local socket w/ low-order three bits cleared.
	if host is not wild:
		check if host is up via chk_host.
	if duplex:
		clear low-order bits of socket numbers.
		set up sockets via sm_dux.
	if simplex:
		set up socket via sm_splx.

parameters:
	none.

returns:
	nothing.

globals:
	kr_buf=
	host=
	otb_*
	si_*
	files=
	f_*
	fs_*
	kro_*
	skt_base
	E*		open errors in daemon

calls:
	chk_host
	f_by_id
	kw_rdy
	skt_off
	asn_sktn
	swab
	kro_rfskt
	f_make
	sm_dux
	sm_splx

called by:
	kr_open

history:
	initial coding 6/10/75 by G. R. Grossman
	check for host up via chk_host added 9/17/75 by G. R. Grossman.
_ifdef SFHBYTE
	added bytesize 01/27/78 by S. F. Holmgren
_endif

*/

kr_odrct()
{
	register char 		*rel_fp;	/* points to relative file
						   struct on relative open */
	register int		type,		/* holds open type */
				op;		/* holds socket operation to
						   be eventually performed */
	char			fs_wild;	/* true iff foreign socket is
						   "wild" */

	/* initialize variables */

	type = kr_buf.kro_type;			/* open type */
	op = (type & otb_init)	?	si_init		/* set socket op */
				:	(type & otb_spcf)	?	si_slsn
								:	si_glsn;
	fs_wild = 0;				/* initially set to not wild */
	host.lo_byte = kr_buf.kro_host;		/* set global host variable */

	/* check relative file if any and check local socket */

	if ( type & otb_rltv )		/* relative open? */
	{
		if (    ( ( rel_fp = f_by_id ( kr_buf.kro_relid ) ) == -1 )
				/* no such file id? */
		     || ( rel_fp->f_state != fs_open ) ) /* or file not open */
		{
bad_par:	/* come here for bad parameter errors */
			kw_rdy ( kr_buf.kro_id, EDINV );
				/* write error ready */
			return;		/* and quit */
		}
		if ( kr_buf.kro_lskt > 7 )	/* illegal socket offset? */
			goto bad_par;		/* go process error if so */
		skt_off ( &rel_fp->f_sbase.bytes[2], & kr_buf.kro_lskt.bytes[2],
			  kr_buf.kro_lskt,
			  2 );	/* compute absolute local socket from base
				   socket of relative file and offset given
				   in the open, put result in open struct */
	}	/* end relative */
	else	/* absolute open */
	{
		if ( kr_buf.kro_lskt == 0 )	/* local socket zero? */
		{
			if ( op != si_init )	/* not init operation? */
				goto bad_par;	/* go process error */
			if ( ( kr_buf.kro_lskt = asn_sktn() ) == -1 )
				/* failed to assign socket number? */
			{
no_resource:	/* come here to process error due to failure to allocate a
		   resource */
				kw_rdy ( kr_buf.kro_id, EDNORES );
					/* write error ready */
				return;	/* and quit */
			}
			kr_buf.kro_lskt.hi_byte =|	/* generate correct */
				( ( kr_buf.kro_fskt[1] + 1 ) & 1 ); /* gender */
		}		/* end local socket zero */
		else		/* non-zero local socket */
		{
			if ( kr_buf.kro_lskt >= skt_base )	/* lskt too
								   large? */
				goto bad_par;	/* go process error */
			kr_buf.kro_lskt = swab ( kr_buf.kro_lskt );
				/* swap bytes of local socket */
		}
	}		/* end of absolute local socket handling */

	/* if general listen, check for wild foreign socket; otherwise,
	   check validity of host number */

	if ( op == si_glsn )	/* general listen? */
	{
		if ( ( kr_buf.kro_fskt[0] | kr_buf.kro_fskt[1] ) == 0 )
				/* foreign socket zero? */
			fs_wild = 1;	/* set wild flag if so */
	}
	else	/* not general listen */
		if ( host == 0 )	/* illegal host? */
			goto bad_par;	/* process error if so */

	/* handle foreign socket */

	if ( ( type & otb_rltv ) && ( fs_wild == 0 ) )	/* relative, not wild?
							*/
	{
		if (   ( kr_buf.kro_fskt[0] == 0 )
		    && ( kr_buf.kro_fskt[1] <= 7 ) )	/* relative fskt? */
			kro_rfskt ( rel_fp );		/* go compute abs */
		else		/* not <=7, absolute */
		{
swap_fskt:	/* come here to swap bytes of foreign socket */
			kr_buf.kro_fskt[0].word = 	/* swap bytes... */
				swab ( kr_buf.kro_fskt[0].word ); /* ...of... */
			kr_buf.kro_fskt[1].word = 	/* ...foreign... */
				swab ( kr_buf.kro_fskt[1].word ); /* ...socket */
		}
	}
	else	/* absolute open, just go swap bytes */
		goto swap_fskt;

	/* check socket gender if simplex and not wild */

	if ( ( ( type & otb_dux ) == 0 ) && ( fs_wild == 0 ) )	/* simplex and
								   not wild? */
		if ( ( kr_buf.kro_lskt.hi_byte & 1 ) ==
		     ( kr_buf.kro_fskt[1].hi_byte & 1 ) )	/* lo bits =? */
			goto bad_par;		/* process error */

	/* get file struct and fill in fields */

	if ( f_make ( kr_buf.kro_id, ( type & otb_dux ) ? 2 : 1 ) )
			/* got file and socket structs allocated? */
		goto	no_resource;		/* error if not */
	fp->f_state = fs_dopnw;		/* state is data open wait */
	fp->f_sbase = kr_buf.kro_lskt & ~( 7 * 256 );
					/* set socket base */
#ifdef SFHBYTE
	fp->f_bysz = kr_buf.kro_bysz;	/* save bytesize */
#endif

	/* check for host up and send reset if necessary */

	if ( host != 0 )	/* host not wild? */
		chk_host();
	/* call appropriate socket creation routine for simplex/duplex */

	if ( type & otb_dux )		/* duplex? */
	{
		kr_buf.kro_lskt.hi_byte =& ~1;	/* neuterize sockets */
		kr_buf.kro_fskt[1].hi_byte =& ~1;
		sm_dux ( &kr_buf.kro_lskt, kr_buf.kro_fskt, op );
						/* set up 2 sockets */
	}
	else	/* simplex */
		sm_splx ( &kr_buf.kro_lskt, kr_buf.kro_fskt,
			  kr_buf.kro_lskt.hi_byte & 1, op );
				/* set up a socket whose type is the low
				   order bit of the local socket */
}

/*name:
	kro_rfskt

function:
	computes relative foreign socket given the state of kr_buf and
	a pointer to a file relative to which the socket number is to
	be computed.

algorithm:
	if the relative file has neither receive nor send data sockets:
		log the occurrance.
		die (this is a table inconsistency).
	copy foreign socket out of socket struct.
	set its low-order bit to zero.
	compute absolute socket relative to this with contents of
	foreign socket in kr_buf as offset, using skt_off.

parameters:
	rfp	pointer to file which should have associated with it a
		socket struct relative to whose foreign socket field the
		foreign socket is to be computed.

returns:
	nothing.

globals:
	kr_buf=
	f_*
	files
	sockets
	s_*
	fsx_null

calls:
	log_asc
	die
	skt_off

called by:
	kr_odrct

history:
	initial coding 6/10/75 by G. R. Grossman
	modified to compute offset from foreign base
	rather than read base 4/17/76 by S. F. Holmgren

*/

kro_rfskt(rfp)
struct socket	*rfp;
{
	register struct file	*rel_fp;	/* copy of rfp for better
						   addressing */
	register struct socket	*rel_sp;	/* will point to socket we use
						   to find base fskt */
	register int		x;		/* will hold socket index
						   during computation of
						   rel_sp */
	char	t_fskt[4];			/* scratch space for computing
						   fskt */

	rel_fp = rfp;		/* copy pointer to relative file */
	if (    ( ( x = rel_fp->f_skt_x[0] ) == fsx_null )
						/* no receive socket? */
	     && ( ( x = rel_fp->f_skt_x[1] ) == fsx_null ) )
						/* or send socket? */
	{	/* give it up, tell why via die	J.S.Goldberg 01Feb78	*/
		die( "kro_rfskt: open file w/ no data sockets" );
	}
	rel_sp = & sockets[x];		/* get address of socket struct */
	t_fskt[0].word = rel_sp->s_fskt[0].word;	/* cpy foreign skt */
	t_fskt[2].word = rel_sp->s_fskt[2].word;	/* cpy foreign skt */
	t_fskt[3] =& ~7;		/* neuterize socket */
	skt_off ( &t_fskt[4], &kr_buf.kro_fskt[2], kr_buf.kro_fskt[1], 4);
			/* compute new foreign socket from base + offset and
			   put in open struct */
}


/*name:
	kr_oicp

function:
	basic preparation for icp open, selection of correct icp open
	procedure to call.

algorithm:
	attemp to set up file with three reserved sockets; if fail:
		write back a ready with error code.
		return.
	call appropriate user/server icp open procedure.

parameters:
	none.

returns:
	nothing.

globals:
	host=
	kr_buf.kro_id
	kr_buf.kro_type
	otb_serv
	kr_buf.kro_host
	EDNORES

calls:
	kw_rdy
	kr_ouicp
	kr_osicp
	f_make

called by:
	kr_open

history:
	initial coding 12/12/74 by G. R. Grossman.
	host check moved to kr_ouicp 7/8/75 by G. R. Grossman.

*/

kr_oicp()
{
	if ( f_make(kr_buf.kro_id,3) )	/* could we reserve file and 3
					   sockets? */
	{
		kw_rdy(kr_buf.kro_id,EDNORES);	/* couldn't, error */
		return;
	}
	if ( kr_buf.kro_type & otb_serv )	/* server icp? */
		kr_osicp();
	else	/* user icp */
		kr_ouicp();
}

/*name:
	kr_ouicp

function:
	initiates user icp open.

algorithm:
	check host number; if zero:
		deallocate file.
		write error ready.
		return.
	attempts to allocate a link number; if fails:
		deallocate file
		write error ready
		return
	check if host is up via chk_host.
	set socket number base in file and
	fill in fields in skt_req for init'ing icp contact socket.
	NOTE:	a zero foreign socket results in a default contact socket
		of 1; a non-zero odd foreign socket is used a s the contact
		socket; a non-zero even foreign socket is a bad parameter.
	set file state to fs_uiopw ( user icp open wait ).
	call get_skt to initiate action on socket init.

parameters:
	none.

returns:

globals:
	host
	si_init
	fp=
	ksx_icp
	fs_uiopw
	skt_req=
	kr_buf.kro_id
	EDNORES
	EDINV

calls:
	asn_lnkn
	asn_sktn
	get_skt
	chk_host
	f_rlse
	kw_rdy

called by:
	kr_oicp

history:
	initial coding 12/12/74 by G. R. Grossman.
	host up check inserted 1/17/75 by G. R. Grossman
	setting socket number base in file added 5/28/75 by G. R. Grossman
	non-default contact socket added 6/12/75 by G. R. Grossman
	host number check moved here from kr_oicp 7/8/75 by G. R. Grossman.
_ifdef SFHBYTE
	added saving of connection bytesize in file 01/27/78 S. F. Holmgren
_endif

*/

kr_ouicp()
{
	register char	link;	/* holds link number assigned to icp socket */

	if ( (host.lo_byte = kr_buf.kro_host) == 0 )	/* illegal host? */
		goto error;	/* go process error */
	if ( (link = asn_lnkn()) < 0 )	/* failed to assign link number? */
	{
		f_rlse();	/* deallocate file */
		kw_rdy(kr_buf.kro_id,EDNORES);	/* error ready */
		return;
	}
	chk_host();				/* check if host is up and take
						   appropriate action */
	fp->f_sbase =				/* set file's socket number
						   base when we ... */
	skt_req.s_lskt->word = asn_sktn();	/* assign local socket */
	if ( ( kr_buf.kro_fskt[0] | kr_buf.kro_fskt[1] ) == 0 )	/* fskt = 0? */
	{
		/* set to default foreign socket = 1 */
		skt_req.s_fskt->word = 0;		/* top 2 byte of frgn skt */
		skt_req.s_fskt[2].word = 1<<8;		/* last 2 bytes of f skt */
	}
	else	/* foreign socket not zero */
		if (( kr_buf.kro_fskt[1] & 1 ) != 0 )	/* foreign socket odd? */
		{
			skt_req.s_fskt[0].word =	/* copy w/ swapped */
				swab ( kr_buf.kro_fskt[0] );	/* bytes */
			skt_req.s_fskt[2].word =
				swab ( kr_buf.kro_fskt[1] );
		}
		else	/* contact socket even */
		{
error:			f_rlse();	/* release file struct */
			kw_rdy ( kr_buf.kro_id, EDINV );	/* error ready */
			return;
		}
	skt_req.s_hstlnk.hl_host = host;	/* host field */
	skt_req.s_hstlnk.hl_link = link;	/* link field */
	skt_req.s_state = si_init;		/* socket state */
	skt_req.s_bysz	= 32;			/* byte size */
	skt_req.s_filep = fp;			/* file pointer */
	skt_req.s_sinx = ksx_icp;		/* socket index is icp */
	fp->f_state = fs_uiopw;			/* file state is user icp open
						   wait */
#ifdef SFHBYTE
	fp->f_bysz = kr_buf.kro_bysz;		/* save connection bytesize */
#endif
	if ( kr_buf.kro_timo )                  /* was a timeout specified? */
	{       time ( &fp->f_timeo );          /* if so add it to current */
		fp->f_timeo =+ kr_buf.kro_timo; /*  time to make timeout */
		ftimeo++;                       /* indicate a file waiting */
	}
	get_skt();      /* this initiates the action on the socket */
}

/*name:
	kr_osicp

function:
	initiates server icp open.

algorithm:
	attempts to assign a group of 8 socket numbers; if fails:
		deallocate file via f_rlse.
		write error ready via kw_rdy.
		return.
	if local (contact) socket is zero:
		set contact socket to one (swapped).
	otherwise:
		if contact socket is in the daemon-allocable name space
		or contact socket is even:
			deallocate file via f_rlse.
			write error ready via kw_rdy.
			return.
		otherwise:
			swap bytes of contact socket.
	set foreign socket wild (zero).
	set host wild (zero).
	set socket state (i.e., socket machine op) to general listen.
	set byte size to 32.
	set file pointer.
	set socket type to icp.
	set file state to server icp open wait.
	initiate general listen via get_skt.

parameters:
	none.

returns:
	nothing.

globals:
	kr_buf
	skt_req=
	skt_base
	E*		open errors in the daemon
	ss_glsn
	fp
	ksx_icp
	fs_siopw

calls:
	asn_sktn
	f_rlse
	kw_rdy
	get_skt

called by:
	kr_oicp

history:
	original version did nothing but return a "bad parameter" error.
	recoded to actually initiate server icp 7/3/75 by G. R. Grossman.
_ifdef SFHBYTE
	added saving of connection bytesize 01/27/78 S. F. Holmgren
_endif

*/

kr_osicp()
{
	if ( ( fp->f_sbase = asn_sktn() ) == -1 )	/* fail to assign
							   socket group? */
	{
		f_rlse();		/* deallocate file */
		kw_rdy ( kr_buf.kro_id, EDNORES );	/* error ready */
		return;
	}
	if ( kr_buf.kro_lskt == 0 )	/* local socket zero? */
		skt_req.s_lskt[0].word = (1 * 256 );	/* set to 1 */
	else				/* local socket not zero */
		if (    ( kr_buf.kro_lskt >= skt_base )	/* out of bounds? */
		     || ( ( kr_buf.kro_lskt & 1 ) == 0 ) )	/* or even? */
		{
			f_rlse();	/* deallocate file */
			kw_rdy ( kr_buf.kro_id, EDINV );	/* error rdy */
			return;
		}
		else		/* no error */
			skt_req.s_lskt[0].word = swab ( kr_buf.kro_lskt );
				/* just copy w/ swapped bytes */
	skt_req.s_fskt[0].word =	/* zero foreign socket */
	skt_req.s_fskt[2].word = 0;
	skt_req.s_hstlnk.word = ( 0377 << 8 );	/* and host, set link to null code */
	skt_req.s_state = ss_glsn;	/* set request op to general listen */
	skt_req.s_bysz = 32;		/* byte size to 32 */
	skt_req.s_filep = fp;		/* point socket's file pointer at
					   file struct */
	skt_req.s_sinx = ksx_icp;	/* socket type to icp */
#ifdef SFHBYTE
	fp->f_bysz = kr_buf.kro_bysz;	/* save connection bytesize */
#endif
	fp->f_state = fs_siopw;		/* file state to server icp open wait */
	get_skt();			/* go initiate listen on contact socket
					*/
}


/*name:
	kr_rcv

function:
	decodes the kernel "rcv" read instruction.

algorithm:
	if the minimum legal number of bytes for a rcv instruction have
	not been read:
		log the error
		return
	set global host number from imp leader.
	increment appropriate statistics counter
	call appropriate procedure via ir_proc

parameters:
	none.

returns:
	nothing.

globals:
	host=
	kr_buf.krr_host
	kr_bytes=
	krr_ovhd
	kr_buf
	kr_buf.krr_type
	iri_reset
	measure.m_ir_ops
	ir_proc

calls:
	log_bin
	ir_iltyp
	statist
	ir_reglr	via ir_proc
	ir_erldr	     "
	ir_igd		     "
	ir_nop		     "
	ir_rfnm		     "
	ir_hdeds	     "
	ir_re_x		     "

called by:
	kr_dcode

history:
	initial coding 12/13/74 by G. R. Grossman.
	modified to and kr_buf.krr_host with 0377 06/07/76 by S. F. Holmgren

*/
char *ir_txt[] { 0, 0, 0, 0, "R: I-H NOP", "R: RFNM", "R: Host dead status",
	"R: Host/Imp dead", "R: Error in data", "R: Incomplete xmit",
	0, 0, 0, 0, 0};

kr_rcv()
{
	extern int (*ir_proc[])();	/* the decoding array */
	register int	type;		/* will hold relevant bits of imp
					   leader type field */
	register char	*txt;		/* text string to log if debugging */

	if ( (kr_bytes =- krr_ovhd) < 0)	
		/* not enough bytes on read? */ 
	{
		log_bin("kr_rcv: rcv too short",&kr_buf.lo_byte,
		        kr_bytes+krr_ovhd);	/* log error */
			/* log the error */
		return;
	}
	host.lo_byte = kr_buf.krr_host&0377;	/* set global host from leader
					   (NOTE that in some cases of
					    message type this is
					    superfluous) */
	type = kr_buf.krr_type & 017;	/* isolate message type */
	/*
	 * there is no imp zero... especially there is no host zero.
	 * it has been known to happen that for some reason, occasionally
	 * 'resets from host zero' arrive... closing all connections when
	 * kw_reset (0) gets called. 
	 */
	if (host.lo_byte == 0 && !(type == 10 || type == 1 || type == 2))
	{
		log_bin ("kr_rcv: Data from host zero", &kr_buf.lo_byte,
			kr_bytes+krr_ovhd);
		return;
	}
	statist( &measure.m_ir_ops[type] );	/* count message type */
	if(k_rdebug && (txt = ir_txt[type]))
		log_bin(txt, &kr_buf, kr_bytes+krr_ovhd);
	(*ir_proc[type])();	/* call appropriate decoding procedure */
}

/*name:
	kr_close

function:
	handles the close instruction from the kernel.

algorithm:
	if no match for the file id can be found via f_by_id
	or there is no attached socket of the type specified in the close:
		log the fact that unmatched close was received.
		return.
	otherwise:
		perform close operation on the specified socket via skt_oper.

parameters:
	none.

returns:
	nothing.

globals:
	fs_null
	kr_buf
	ksx_null
	skt_oper
	si_close
	host=

calls:
	(*skt_oper[si_close][])()
	log_bin
	f_by_id

called by:
	kr_dcode thru kr_proc

history:
	initial coding 1/8/75 by G. R. Grossman
	fixed bug wherein host was not being set before calling close
	procedure 1/11/75 by G. R. Grossman.
	revised to use f_by_id 6/10/75 by G. R. Grossman.

*/

kr_close()
{
	register char		*f_p;	/* will point to file struct */
	register struct socket	*s_p;	/* will point to relevant socket
					   struct if one exists */
	register int	i;		/* will hold index of socket if one
					   exists */

	if (    ( ( f_p = f_by_id ( kr_buf.kro_id ) ) == -1 )	/* no match? */
	     || ( ( i = f_p->f_skt_x[kr_buf.krc_sinx] ) == fsx_null ) )
				/* or referenced socket doesn't exist? */
	{
		log_bin ( "kr_close: unmatched close", &kr_buf.lo_byte, 4 );
			/* log error */
		return;		/* and go away */
	}
	s_p = &sockets[i];	/* pointer to socket */
	host.lo_byte = s_p->s_hstlnk.hl_host;
		/* set global host from socket */
	(*skt_oper[si_close][s_p->s_state])(s_p);
		/* call appropriate close procedure */
}

/*name:
	kr_timo

function:
	handles the timeout instruction from the kernel.

algorithm:
	if there are any files waiting to be timed out
		for each file
			if it is waiting to be timed out
				if the timeout has elapsed
					do a 'kill' on each socket
				else increment number of files waiting
	if there are any sockets waiting to be timed out
		for each socket
			if it is waiting to be timed out
				if the timeout has elapsed
					do a timeout on each socket
				else increment number of sockets waiting

parameters:
	none.

returns:
	nothing.

globals:
	ftimeo
	stimeo
	host

calls:
	skt_oper
	kw_rdy
	log_bin

called by:
	kr_dcode

history:
	initial coding 6/10/75 by G. R. Grossman
	written by greep

*/

kr_timo()
{
	register struct file *f_p;
	register struct socket *s_p;
	register int i;
	long int tim;                   /* for storing current time */
	int oftimeo, ostimeo;           /* old values for ftimeo and stimeo */

	timeo = 0;                      /* indicate no request outstanding */
	oftimeo = ftimeo;               /* remember whether any files and */
	ostimeo = stimeo;               /*  sockets are now waiting */
	ftimeo = stimeo = 0;            /* reset flags for files and sockets
					   to be timed out */

	time( &tim );			/* get current time */

	if ( oftimeo )                  /* see if any files waiting */
	for( f_p = &files[0]; f_p < &files[ nfiles ]; f_p++ )
	/* see if this file is interested in timing out */
	if( f_p->f_state != fs_null && f_p->f_timeo )
	{
		if( tim >= f_p->f_timeo )
		{
			if(k_rdebug) log_bin("kr_timo: Timeout on file",&f_p->f_id,2);
			/* loop thru possible attached sockets
			   and time out all */
			for( i = 0; i < 3; i++ )
			if( f_p->f_skt_x[i] != fsx_null )
			{
				s_p = &sockets[ f_p->f_skt_x[i] ];
				host = s_p->s_hstlnk.hl_host&0377;
				(*skt_oper[si_kill][s_p->s_state])(s_p);
			}
			f_p->f_timeo = 0;
			kw_rdy( f_p->f_id,EDAEIO );
		}
		else
			ftimeo++;
	}

	if ( ostimeo )                  /* see if any sockets waiting */
	for ( s_p = &sockets[0]; s_p < &sockets[nsockets]; s_p++ )
	/* see if this socket is interested in timing out */
	if ( s_p->s_state != ss_null && s_p->s_timeo )
	{
		if ( tim >= s_p->s_timeo )
		{
			if(k_rdebug) log_bin("kr_timo: Timeout on sock",s_p,17);
			(*skt_oper[si_timo][s_p->s_state])(s_p);
			s_p->s_timeo = 0;
		}
		else
			stimeo++;       /* increment number of sockets */
	}
};

/*name:
	kr_reset

function:
	sends a host_to_host reset to a specific host
	and cleans up any connections to that host

algorithm:
	clean up local data structs and send a clean to the kernel
	send a reset to the host specified

parameters:
	indirectly kr_buf.krst_host host to be reset

returns:
	nothing

globals:
	host =
	kr_buf

calls:
	h_dead
	chk_host

called by:
	kr_dcode

history:
	initial coding 01/7/75 by S. F. Holmgren

*/
kr_reset()
{
	host = (kr_buf.krst_host & 0377);
	h_dead();		/* clean things up */
	chk_host();		/* send a reset to the host */
}
/*name:
	kr_log

function:
	Log a message recieved from the resident portion.

algortihm:
	Pass the string to log_asc to be logged.

parameters:
	indirectly kr_buf.krl_msg string to be logged.

returns:
	nothing.

globals:
	kr_buf

calls:
	log_asc

called by:
	kr_dcode

history:
	initial coding 8/2/77 by J. G. Noel

*/
/* following temporary until moved into kread.h */
struct kr_log
{
char kr_op;		/* op field */
char krl_msg[];		/* null-terminated message */
};
kr_log()
{
	log_asc(kr_buf.krl_msg);
}

