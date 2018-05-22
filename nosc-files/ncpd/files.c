#

/*	files.c		*/

/*	globals declared in this file:
		n_f_left

	functions declared in this file:
		f_make
		fi_sktnm
		fi_sopn
		fi_sgone
		fi_all
		f_rlse
		f_by_id
		fi_ssn
		fi_rfnm
*/

#include	"hstlnk.h"
#include	"files.h"
#include	"globvar.h"
#include	"kread.h"
#include	"kwrite.h"
#include	"socket.h"
#include	"leader.h"

/* SCCS PROGRAM IDENTIFICATION STRING */
char id_files[] "~|^`files.c\tV3.9E0\tJan78\n";

int	n_f_left	nfiles;	/* counter for allocating files */

/*name:
	f_make

function:
	attempts to allocate a file struct and reserve the speified number of
	socket structs. if unsuccessful, it 
	returns 1; otherwise it sets the global fp to the
	address of the file struct allocated, initializes the file
	struct, and returns 0.

algorithm:
	if the number of files left is 0 or the number of sockets left
	is less than the number requested:
		return 1.
	loop thru all file structs:
		if the file state is null:
			set global file pointer fp.
			set id as given by the parameter.
			set socket number base to zero.
			set number of sockets reserved as specified.
			set all f_skt_x to null.
			set number of attached sockets to zero.
			decrement file allocation counter by 1.
			decrement socket allocation counter by the number
			reserved for this file.
			return 0.
	if we fall out of the loop then the file allocation counter is 
	inconsistent with the state of the tables and we die with
	a suitable error message.

parameters:
	id		kernel id to be assigned to the file.
	n_r_skts	number of socket structs to be reserved for this
			file.

returns:
	1	if the attempt to allocate file and/ or sockets was
		unsuccessful.
	0	if it was successful.
	dies	if it discovers inconsistency in the tables.

globals:
	fp=
	files=
	fs_null
	fsx_null
	n_f_left=
	n_s_left=

calls:
	die

called by:
	kr_oicp
	kr_odrct

history:
	initial coding 12/30/74 by G. R. Grossman
	clearing f_sbase added 5/28/75 by G. R. Grossman.

*/

int	f_make(id,n_r_skts)
int	id,n_r_skts;
{
	register struct file	*lfp;	/* will point to file structs
					   during search */

	if ( n_f_left <= 0 || n_s_left < n_r_skts )	/* can we allocate
							   file and socket
							   structs? */
		return(1);	/* no, return 1 */
	for ( lfp = &files[0] ; lfp < &files[nfiles] ; lfp++ )
			/* loop thru all file structs */
	{
		if ( lfp->f_state == fs_null )	/*this one not in use? */
		{
			fp = lfp;		/* set global file pointer */
			lfp->f_id = id;		/* set file's kernel id */
			lfp->f_sbase = 0;	/* we don't have a base socket
						   number yet */
			lfp->f_nsrsrv = n_r_skts;	/* set number of
							   reserved sockets */
			lfp->f_skt_x[0] =	/* null socket indices */
				lfp->f_skt_x[1] =
					lfp->f_skt_x[2] = fsx_null;
			lfp->f_nsatt = 0;	/* number of attached sockets */
			n_f_left =- 1;		/* update file struct
						   allocation counter */
			n_s_left =- n_r_skts;	/* update socket struct
						   allocation counter */
			return(0);		/* signifies success */
		}
	}
	die("f_make - file allocation inconsistent");
		/* if we got here the file allocation counter did not
		   agree with the number of free files, so die */
}

/*name:
	fi_sktnm

function:
	handles the socket number transmitted by the foreign logger in
	user icp.

algorithm:
	set global file pointer to point to the associated file.
	if the file is in the socket number wait state:
		if the socket number is of the wrong (odd) polarity:
			log the occurrance.
			kill the socket (this will eventually cause the
			socket to be closed and fi_sgone to be called
			while the file is still in the socket number
			wait state, causing the open to be terminated
			abnormally).
			return.
		set file state to data open wait.
		compute proper socket offset for local base socket (add 2
		to icp local socket.
		create the data sockets via sm_dux.
		kill the icp socket.
	(NOTE that if the file was not in socket number wait state, we
	 take no action; this happens only if a close is in progress
	 when the socket number arrives.)

parameters:
	icpsktp		pointer to icp socket for connection on which
			socket number has arrived.
	fkstn_p		pointer to socket number.

returns:
	nothing.

globals:
	fp=
	fs_uisnw
	fs_dopnw
	si_init

calls:
	log_bin
	kill_skt
	sm_dux

called by:
	hr_data

history:
	initial coding 12/30/74 by G. R. Grossman

*/

fi_sktnm(icpsktp,fsktn_p)
struct socket	*icpsktp;
char	*fsktn_p;
{
	register struct file	*f_p;	/* will point to file struct
					   associated with socket */
	char	lskt[2];		/* will hold base for local data
					   sockets */
	fp = f_p = icpsktp->s_filep;	/* set global file pointer and local 
					   one to socket's file pointer */
	if ( f_p->f_state == fs_uisnw )	/* file state = socket number wait? */
	{
		if ( fsktn_p[3] & 1 )	/* wrong socket polarity? */
		{
			log_bin("fi_sktnm - bad uicp fskt",fsktn_p,4);
				/* log socket number */
			log_bin("....from host",&host,1);	/* and host */
			kill_skt(icpsktp);	/* kill connection */
			return;
		}
		f_p->f_state = fs_dopnw;	/* set state to data open
						   wait */
		skt_off(&icpsktp->s_lskt[2],&lskt[2],2,2);
			/* compute proper local socket: offset 2 from
			   number of icp socket */
		sm_dux(&lskt[0],fsktn_p,si_init);	/* make 2 data 
							   sockets */
		kill_skt(icpsktp);		/* kill icp socket */
	}
}

/*name:
	fi_sopn

function:
	called every time a connection is opened; handles the opening
	of a connection at the file level: initial allocations and mods
	are taken care of here.

algorithm:
	set global file pointer fp.
	if the just-opened socket is an icp socket:
		if file state is user icp open wait:
			write mod to kernel ( status = open, daemon data
					      messages =1, bits = 32 )
			send all to host (messages = 1, bits = 32 )
			set file state to socket number wait.
			return.
		if file state is server icp open wait:
			write mod to kernel (status = open, daemon data).
			save current time for possible time out
			set file state to ALL wait.
			return.
		otherwise file state is invalid:
			die with appropriate messages.
	otherwise the just-opened socket is a data socket:
		if file state is data open wait:
			if socket is a receive socket:
				write mod to kernel(status = open,
						    messages and bits nominal )
				send all to host (messages and bits nominal )
			else it is a send socket:
				write mod to kernel(status = open,
						    messages and bits zero )
		otherwise file state is invalid:
			die with appropriate messages.
		if all reserved sockets are attached,
		and other data socket is non-existent or open:
			set file state to open.
			reset timer info
			write ready to kernel.

parameters:
	skt_p	pointer to socket struct for just-openend socket.

returns:
	nothing.

globals:
	fp=
	ksx_icp
	fs_uiopw
	kwi_mod
	ksb_open
	ksb_ddat
	fs_uisnw
	fs_dopnw
	nom_mall
	nom_ball
	fsx_null
	ss_open
	fs_open
	fs_siopw
	fs_sialw

calls:
	die
	log_bin
	kw_sumod
	hw_all
	kw_rdy
	time (sys)

called by:
	lsint_q
	rfc_rfcw

history:
	initial coding 12/31/74 by G. R. Grossman
	fix to check for all reserved sockets attached before
	checking for existence or openness of other data socket
	made 1/10/75 by G. R. Grossman.
	server icp state handling added 7/3/75 by G. R. Grossman.
	time out code for server connection added 6/14/76 by S. F. Holmgren

*/

fi_sopn(skt_p)
struct socket	*skt_p;
{
	register int	oth_skt;	/* holds index of socket for other
					   direction */
	register struct socket	*s_p;	/* will point to socket struct */
	register struct file	*f_p;	/* will point to file struct */

	s_p = skt_p;			/* get pointer to socket */
	fp = f_p = s_p->s_filep;	/* get pointer to associated file
					   and set global file pointer */
	if ( s_p->s_sinx == ksx_icp )	/* icp socket? */
	{
		switch ( f_p->f_state )	/* decode file state */
		{
			case	fs_uiopw:	/* user icp open wait */
			{
				kw_sumod(kwi_mod,s_p,(ksb_open|ksb_ddat),
					 1,0,32);/* write mod:open, daemon data
						   1 message, 32 bits */
				hw_all(s_p,1,32);	/* send all:
							   1 msg, 32 bits */
				f_p->f_state = fs_uisnw;/* file state =
							   socket number wait */
				return;
			}
			case	fs_siopw:	/* server icp open wait */
			{
				kw_sumod ( kwi_mod, s_p, ( ksb_open | ksb_ddat ),
					   0, 0, 0 );	/* write mod: open,
							   daemon data */
				/* keep track of time this open takes */
				time( &f_p->f_timeo );
				f_p->f_timeo =+ FTIMEOUT;
				ftimeo++;                       /* inc people waiting */
				f_p->f_state = fs_sialw;	/* file state =
								   ALL wait */
				return;
			}
invalid:		default:		/* bad file state */
				die("fi_sopn - bad file state");
		}
	}
	else				/* data socket */
	{
		if ( f_p->f_state != fs_dopnw )	/* bad file state? */
			goto invalid;		/* go take care of it */
		if ( (s_p->s_lskt[1] & 1) == 0 )	/* receive socket? */
		{
			kw_sumod(kwi_mod,s_p,ksb_open,nom_mall,0,nom_ball);
				/* write mod:state = open, nominal alloc */
			hw_all(s_p,nom_mall,nom_ball);	/* send nominal alloc */
		}
		else		/* send sokcket */
		{
			kw_sumod(kwi_mod,s_p,ksb_open,0,0,0);
				/* write mod:state = open, zero alloc */
		}
		if ( (fp->f_nsatt == fp->f_nsrsrv)	/* all needed sockets
							   attached? */
		&& ( ((oth_skt = f_p->f_skt_x[s_p->s_sinx^1]) == fsx_null)
			/* no other socket? */
		    ||(sockets[oth_skt].s_state == ss_open) ) )
			/* or other socket open? */
		{
			f_p->f_state = fs_open;	/* set file state to open */
			f_p->f_timeo = 0;
			kw_rdy(f_p->f_id,0);	/* write errorless ready */
		}
	}
}

/*name:
	fi_sgone

function:
	handles the closing of a connection at the file level.

algorithm:
	the global file pointer, fp, is set to point to the file
	associated with the socket.
	the socket is detached from the file by setting the 
	corresponding socket index byte to null and decrementing the
	attached and reserved counters.
	if the socket was an icp socket:
		if the file is in the user icp open or socket number wait
		or server icp open or ALL wait or rfnm wait
		states:
			write error ready to kernel (due to premature
			closing of contact connection).
			release the file via f_rlse.
			return
		if the file is in the gone wait state:
	gonechk:	if the number of attached sockets is now zero:
				release the file via f_relse.
			return.
	otherwise the socket was a data socket:
		if the file was in the data open wait state:
			send error ready to kernel.
			if there is another data socket:
				kill other data socket.
			set file state to gone wait.
			go to gonechk.
		if the file was in the open state:
			if there is another data socket:
				kill other data socket.
			set file state to gone wait.
			go to gonechk.
		if the file was in the gone wait state:
			go to gonechk.

parameters:
	skt_p	points to socket whose connection has just been closed.

returns:
	nothing.

globals:
	fp=
	fsx_null
	ksx_icp
	fs_uiopw
	fs_uisnw
	fs_gonew
	fs_siopw
	fs_sialw
	fs_sirfw
	EDAEIO
	fs_dopnw
	sockets

calls:
	kw_rdy
	f_rlse
	kill_skt

called by:
	free_skt

history:
	initial coding 12/31/74 by G. R. Grossman
	setting file state to gone wait in data socket cases added
	5/29/75 by G. R. Grossman.
	server icp state handling added 7/3/75 by G. R. Grossman.
	modified user & server open waits to just release the file
	6/27/76 by S. F. Holmgren

*/

fi_sgone(skt_p)
struct socket	*skt_p;
{
	register struct socket	*s_p;		/* will point to socket */
	register struct file	*f_p;		/* will point to its file */
	register int	x;	/* in one case will hold index of other data
				   socket; at first, holds type index of socket
				   we're passed */

	s_p = skt_p;			/* get pointer to socket */
	fp = f_p = s_p->s_filep;	/* set global file pointer from 
					   socket's file pointer */
	x = s_p->s_sinx;		/* get socket type index form socket */
	f_p->f_skt_x[x] = fsx_null;	/* set corresponding skt_x to null */
	f_p->f_nsatt =- 1;		/* decrement attached socket count */
	f_p->f_nsrsrv =- 1;		/* decrement reserved socket count */
	if ( x == ksx_icp )		/* icp socket? */
	{
		switch ( f_p->f_state )	/* decode file state */
		{
		case	fs_uisnw:	/* user icp socket number wait */
		case	fs_sialw:	/* server icp ALL wait */
		case	fs_sirfw:	/* server icp rfnm wait */
		case	fs_uiopw:	/* user icp open wait */
		case	fs_siopw:	/* server icp open wait */

				kw_rdy(f_p->f_id,EDAEIO);/* write error ready
							   to kernel */
				f_rlse();		/* release file */
				return;

gonechk:	case	fs_gonew:	/* gone wait */
			{
				if ( f_p->f_nsatt == 0 )/* no sockets 
							   attached? */
					f_rlse();	/* release file */
				return;
			}
		}
	}
	else	/* data socket */
	{
		switch ( f_p->f_state )		/* decode file state */
		{
		case	fs_dopnw:	/* data open wait */
			kw_rdy(f_p->f_id,EDAEIO);	/* write error ready */
		case	fs_open:	/* file was open */
			if ( (x = f_p->f_skt_x[x^1]) != fsx_null )
#ifndef HALFCLOSE
				/* socket for other direction exists? */
				kill_skt(&sockets[x]);	/* kill it */
#endif
#ifdef HALFCLOSE
				/* null statement */;
			else
#endif
			f_p->f_state = fs_gonew;	/* set file state to
							   gone wait */
		case	fs_gonew:	/* gone wait */
			goto gonechk;	/* go check for release */
		}
	}
}

/*name:
	fi_all

function:
	handles the reception of an ALL at the file level.

algorithm:
	if the socket is an icp socket:
		if the socket is a receive socket:
			go die, see below.
		if the file associated with the socket is not in the ALL
		wait state:
			return, ignoring the ALL.
		if the allocation is insufficient ( msgs < 1, bits < 32 ):
			initiate close, aborting the icp.
			return.
		send socket number via fi_ssn.
		set file state to rfnm wait.
	otherwise:
		if the socket is a data send socket:
			write mod to kernel passing on the ALL parameters.
		otherwise:
			die because it is inconsistent for us to get an ALL
			on receive socket, as table structure, notably the
			oring in of 0200 to send links, should preclude this.

parameters:
	skt_p	pointer to socket struct for which the ALL was received.
	msgs	message filed from ALL as an int.
	bits_hi	hi 16 bits of bits field from ALL as an int.
	bits_lo	lo 16 bits as above.

returns:
	nothing.

globals:
	ksx_icp
	ksx_xmit
	kwi_mod
	ksb_open
	s_*
	f_*
	fp=
	fs_*

calls:
	kw_sumod
	die
	kill_skt
	fi_ssn

called by:
	hr_all

history:
	initial coding 1/11/75 by G. R. Grossman
	modified to handle server icp 7/8/75 by G. R. Grossman.

*/

fi_all(skt_p,msgs,bits_hi,bits_lo)
struct socket	*skt_p;
char	*msgs,*bits_hi,*bits_lo;
{
	register struct socket	*s_p;	/* will point to socket struct */

	s_p = skt_p;
	fp = s_p->s_filep;
	if ( s_p->s_sinx == ksx_icp )		/* icp socket? */
	{
		if ( ( s_p->s_lskt[1] & 1 ) == 0 )
			goto broken;	/* ALL on receive socket, go dump */
		if ( fp->f_state != fs_sialw )		/* not ALL wait? */
			return;		/* simply ignore */
		if (    ( msgs == 0 )	/* no msgs allocated? */
		     ||	( ( bits_hi == 0 ) && ( bits_lo < 32 ) ) )
				/* or not enough bits? */
		{
			kill_skt ( s_p );	/* initiate close which will
						   abort icp */
			return;
		}
		fi_ssn ( s_p );		/* alles ist in ordnung, send socket
					   number */
		fp->f_state = fs_sirfw	;	/* set file to rfnm
						   wait state */
		s_p->s_state = ss_orfnmw;
	}
	else					/* data socket */
		if ( s_p->s_sinx == ksx_xmit )	/* send socket? */
			kw_sumod(kwi_mod,s_p,ksb_open,msgs,bits_hi,bits_lo);
				/* pass ALL parameters to kernel */
		else				/* rcv socket */
broken:			die("fi_all - ALL on bad socket");	/* let's see */
}

/*name:
	f_rlse

function:
	releases a socket struct from use.

algorithm:
	increment the free socket count by the number of sockets still
	reserved for the file.
	set the file state to null.
	make sure timer info is stepped on
	increment the free file count by 1.
	if the socket number base is non-zero:
		loop thru all file structs:
			if the struct is in use and it has the same socket
			number base:
				return.
		if we get here no other file is using the same base:
			release the socket number group via rls_sktn.

parameters:
	none.

returns:
	nothing.

globals:
	fp
	fs_null
	n_f_left=
	n_s_left=

calls:
	rls_sktn
	kw_frlse

called by:
	fi_sgone
	kr_osicp

history:
	initial coding 12/31/74 by G. R. Grossman
	releasing socket number group added 5/28/75 by G. R. Grossman.
	step on time out information added 6/15/76 by S. F. Holmgren
	call kw_frlse added 7/8/76 by S. F. Holmgren

*/

f_rlse()
{
	register struct file	*f_p;	/* for searching socket structs
					   for matching socket number bases */
	register int	sbase;		/* will hold socket number base for
					   speeding up search */

	n_s_left =+ fp->f_nsrsrv;	/* add still-reserved socket count
					   to free socket count */
	fp->f_state = fs_null;		/* set file state to null (THIS
					   frees the file struct */
	fp->f_timeo = 0;                /* step on timer info */
	n_f_left++;			/* increment free file count */
	kw_frlse();			/* tell kernel to release file */
	if ( ( sbase = fp->f_sbase ) != 0 )	/* own a socket group? */
	{
		for ( f_p = &files[0] ; f_p < &files[nfiles] ; f_p++ )
					/* loop thru all file structs */
		{
			if (  ( f_p->f_state != fs_null )	/* struct in
								   use? */
			    &&( f_p->f_sbase == sbase ) )	/* and match? */
				return;		/* can't release socket group,
						   so quit */
		}
		rls_sktn( sbase );	/* if we got here no-one else is
					   using the socket group, so release
					 */
	}
}


/*name:
	f_by_id

function:
	attempts to find an in-use file struct whose id field is identical
	to the specified id.

algorithm:
	loop thru all file structs:
		if file state is not null ( therefore in use ):
			if file id matches specified id:
				return file struct address.
	if fall out of loop, no match:
		return -1

parameters:
	fid	file id for which a match is sought.

returns:
	address of file whose id matches the parameter fid, if any.
	-1 otherwise.

globals:
	files
	fs_null

calls:
	nothing.

called by:
	kr_odrct
	kr_close
	kr_open

history:
	initial coding 6/9/75 by G. R. Grossman.

*/

struct file	*f_by_id ( fid )
int	fid;
{
	register struct file	*f_p;	/* will point to file structs during
					   search */
	register int	id;		/* will hold id during search for
					   faster code */

	id = fid;			/* copy sought id */
	for ( f_p = &files[0] ; f_p < &files[nfiles] ; f_p++ )
			/* loop thru all file structs */
		if (    ( f_p->f_state != fs_null )	/* struct in use? */
		     && ( f_p->f_id == id ) )		/* and id matches? */
			return ( f_p );		/* return address of struct */
	return ( -1 );		/* return -1 on failure */
}

/*name:
	fi_ssn

function:
	sends socket number for server icp.

algorithm:
	set up leader in kw_buf via mak_ldr.
	set link field in leader from that in socket struct ( masking out 
	hi-order bit ).
	set byte size in leader to 32.
	set byte count in leader to 1.
	set first two bytes of data to zero.
	set next two bytes to the local socket number base of the file
	assooiated with the socket ( this assumes fp valid ).
	send the leader and data via kw_write.

parameters:
	skt_p	pointer to the icp socket on which the socket number
		base is to be sent as data.

returns:
	nothing.

globals:
	kw_buf=
	fp
	s_*
	f_*

calls:
	mak_ldr
	kw_write

called by:
	fi_all

history:
	initial coding 7/8/75 by G. R. Grossman.

*/

fi_ssn ( skt_p )
struct socket	*skt_p;
{
	mak_ldr();		/* set up default leader fields */
	kw_buf.kw_data->l_link = skt_p->s_hstlnk.hl_link & 0177;
		/* set link, masking out the 0200 which identifies a send
		   link */
	kw_buf.kw_data->l_bysz = 32;		/* and byte size */
	kw_buf.kw_data->l_bycnt = ( 1 << 8 );	/* and byte count */
	kw_buf.kw_data->l_data[0] =		/* zero 1st 2 bytes of data */
	kw_buf.kw_data->l_data[1] = 0;
	kw_buf.kw_data->l_data[2] = fp->f_sbase.lo_byte;	/* copy socket */
	kw_buf.kw_data->l_data[3] = fp->f_sbase.hi_byte;	   /* from file */
	kw_write ( &kw_buf.kw_data->l_data[4] - &kw_buf.lo_byte );
			/* send off leader and data */
}
/*name:
	fi_rfnm

function:
	handles rfnm's at the file level ( for server icp ).

algorithm:
	if the file is in the rfnm wait state:
		set file state to data open wait.
		compute foreign socket from that of the icp socket + 2, via
		skt_off.
		initiate duplex data connection via sm_dux.
		initiate close of icp socket via kill_skt.

parameters:
	skt_p	pointer to server icp contact socket ( hopefully ) for
		which rfnm arrived.

returns:
	nothing.

globals:
	fp=
	s_*
	f_*
	fs_*
	si_init

calls:
	skt_off
	sm_dux
	kill_skt

called by:
	ir_rfnm

history:
	initial coding 7/8/75 by G. R. Grossman.

*/

fi_rfnm ( skt_p )
struct socket	*skt_p;
{
	char	fskt[4];	/* for holding foreign data socket */

	fp = skt_p->s_filep;	/* set global file pointer */
	if ( fp->f_state == fs_sirfw )		/* file in server icp rfnm wait
						   state? */
	{
		fp->f_state = fs_dopnw;		/* set file state to data open
						   wait */
		skt_off ( &skt_p->s_fskt[4], &fskt[4], 2, 4 );
				/* compute offset of 2 from foreign socket */
		sm_dux ( &fp->f_sbase, fskt, si_init );
				/* initiate duplex data connection  */
		kill_skt ( skt_p );	/* start closing contact socket */
	}
}
