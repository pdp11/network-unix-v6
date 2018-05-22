#

/*	kwrite.c	*/

/*	globals declared in this file:
		NONE.

	functions declared in this file:
		kw_write
		kw_sumod
		kw_rdy
		kw_clean
		kw_reset
		kw_frlse
		kw_timo
*/
#include	"hstlnk.h"
#include	"files.h"
#include	"socket.h"
#include	"kwrite.h"
#include	"globvar.h"

/* SCCS PROGRAM IDENTIFICATION STRING */
char id_kwrite[] "~|^`kwrite.c\tV3.9E0\tJan78\n";

/*name:
	kw_write

function:
	writes a specified number of bytes from kw_buf to the kernel,
	logging the transaction when appropriate and checking for 
	errors.

algorithm:
	if the kernel write debug flag is on:
		log the transaction via log_bin.
	write the specified number of bytes from kw_buf on k_file;
	if an error results:
		log the error via log_asc
		log the transaction via log_bin
		return 1.
	otherwise:
		return 0

parameters:
	n_bytes	the number of bytes of kw_buf to write.

returns:
	0	iff no error resulted from the write.
	1	otherwise.

globals:
	k_wdebug
	kw_buf
	k_file

calls:
	log_bin
	log_asc
	write	(sys)

called by:
	kw_sumod
	kw_rdy
	kw_clean
	kw_reset
	rst_all

history:
	initial coding 12/28/74 by G. R. Grossman

*/

char *kw_txt[] {
	"W: send to net",
	"W: setup i-node",
	"W: modify i-node",
	"W: wakeup file",
	"W: cleanup i-node",
	"W: reset host",
	"W: release file",
	"W: set timer",
	"W: ((UNDEFINED))",
	"W: ((UNDEFINED))",
	"W: ((UNDEFINED))",
	"W: ((UNDEFINED))",
};


int	kw_write(n_bytes)
int	n_bytes;
{
	extern int errno;	/* error code returned if write error */

	if ( write(k_file,&kw_buf,n_bytes) < 0 )	/* error on write? */
	{
		log_bin("kw_write: write error", errno, 1);  /* log the error */
		log_bin(kw_txt[kw_buf.kw_op],&kw_buf,n_bytes);
			/* log what we wrote */
		return(1);	/* indicate error */
	}
	if ( k_wdebug )		/* debugging write flag on? */
		log_bin(kw_txt[kw_buf.kw_op],&kw_buf,n_bytes);
			/* log what we wrote */
	return(0);		/* indicate that everything's ok */
}

/*name:
	kw_sumod

function:
	formats and writes the kernel "setup" and "mod" instructions.

algorithm:
	the fileds of kw_buf are set appropriately.
	the instruction is sent to the kernel via kw_write.
	the result of kw_write is returned.

parameters:
	op	op code: kwi_stup or kwi_mod.
	skt_p	pointer to the socket structure which is to have its
		kernel analog set up or modified.
	status	value for the status byte of the instruction.
	msgs	number of messages to be aadded to the allocation field
		in the kernel analog.
	bits_hi	hi word of number of bits to be added to the allocation field in
		the kernel analog.
	bits_lo	lo word of same.
	local socket number
	foreign socket number

returns:
	the result of the call on kw_write.

globals:
	kw_buf=

calls:
	kw_write
	swab

called by:
	fi_sopn
	so_ulsn
	lsint_q

history:
	initial coding 12/28/74 by G. R. Grossman
	modified 4/16/76 by S. F. Holmgren added socket information

*/

int	kw_sumod(op,skt_p,status,msgs,bits_hi,bits_lo)
int	op,status,msgs,bits_hi,bits_lo;
struct socket	*skt_p;
{
	register struct socket	*p;	/* will point to socket struct */

	struct				/* formats the messages and bits at
					   end of instruction */
	{
		int	mb_msgs;	/* message field */
		int	mb_bits[2];	/* bits field */
		int	mb_lskt;	/* local socket */
		int	mb_fskt[2];	/* foreign socket */
	};

	p = skt_p;		/* get pointer to socket struct */
	kw_buf.kw_op = op;	/* set instruction op code */
	kw_buf.kw_sinx = p->s_sinx;	/* set socket index field */
	kw_buf.kw_id = p->s_filep->f_id;	/* id field */
	kw_buf.kw_hstlnk.word = p->s_hstlnk.word;	/* host & link */
	kw_buf.kw_stat = status;		/* status */
	kw_buf.kw_bysz = p->s_bysz;		/* byte size */
	kw_buf.kw_data->mb_msgs = msgs;		/* allocated messages */
	kw_buf.kw_data->mb_bits[0] = bits_hi;	/* 1st word of bits */
	kw_buf.kw_data->mb_bits[1] = bits_lo;	/* 2nd word of bits */
	kw_buf.kw_data->mb_lskt = swab( p->s_lskt[0].word );	/* local skt */
	kw_buf.kw_data->mb_fskt[0] = swab( p->s_fskt[0].word );	/* frn skt */
	kw_buf.kw_data->mb_fskt[1] = swab( p->s_fskt[2].word );	/* frn skt */
	return( kw_write(&kw_buf.kw_data->mb_fskt[2].lo_byte
			 -&kw_buf.lo_byte) );	/* return result of call on
						   kw_write */
}

/*name:
	kw_rdy

function:
	formats and writes the kernel "ready" instruction.

algorithm:
	fills in appropriate fields.
	sends instruction to kernel via kw_write.
	returns the result returned by kw_write.

parameters:
	id	the kernel's id for the current file.
	code	the code to be included as the status byte of the instruction.

returns:
	the result returned by kw_write.

globals:
	kwi_redy
	kw_buf=

calls:
	kw_write

called by:
	f_make
	fi_sgone
	kr_oicp
	kr_ouicp
	kr_osicp
	kr_odrct

history:
	initial coding 12/28/74 by G. R. Grossman

*/

int	kw_rdy(id,code)
int	id,code;
{
	kw_buf.kw_op = kwi_redy;	/* op code */
	kw_buf.kw_id = id;		/* id */
	kw_buf.kw_stat = code;		/* ready code */
	return( kw_write(&kw_buf.kw_data[0]-&kw_buf.lo_byte) );
		/* return result returned by kw_write */
}

/*name:
	kw_clean

function:
	formats and writes the kernel "clean" instruction.

algorithm:
	fill in appropriate fields.
	write to kernel via kw_write.
	return result returned by kw_write.

parameters:
	skt_p	pointer to struct of socket whose kernel analog is to
		be cleaned up and deleted.

returns:
	the result returned by kw_write.

globals:
	kw_buf=
	kwi_clen

calls:
	kw_write

called by:
	cls_open
	clo_lsn
	clo_rfop
	so_ut1
	so_ut3

history:
	initial coding 12/28/74 by G. R. Grossman

*/

int	kw_clean(skt_p)
struct socket	*skt_p;
{
	kw_buf.kw_op = kwi_clen;		/* op code */
	kw_buf.kw_id = skt_p->s_filep->f_id;	/* id */
	kw_buf.kw_sinx = skt_p->s_sinx;		/* socket index */
	kw_buf.kw_hstlnk.word = skt_p->s_hstlnk.word;	/* host & link */
	return( kw_write(&kw_buf.kw_data[0]-&kw_buf.lo_byte) );
		/* return result returned by kw_write */
}

/*name:
	kw_reset

function:
	formats and writes the kernel "reset" instruction.

algorithm:
	fill in appropriate fields.
	write instruction to kernel via kw_write.
	return result returned by kw_write.

parameters:
	hst	number of host whose kernel tables are to be reset.
		0 => all hosts.

returns:
	result returned by kw_write.

globals:
	kw_buf=

calls:
	kw_write

called by:
	h_dead

history:
	initial coding 12/28/74 by G. R. Grossman

*/

int	kw_reset(hst)
int	hst;
{
	kw_buf.kw_op = kwi_rset;		/* op code */
	kw_buf.kw_hstlnk.hl_host = hst;	/* host */
	return( kw_write(&kw_buf.kw_data[0] - &kw_buf.lo_byte) );
		/* return result returned by kw_write */
}

/*name:
	kw_frlse

function:
	send a file release instruction to the kernel

algorithm:
	set up the instruction
	send it to the kernel

parameters:
	none

returns:
	nothing

globals:
	kw_buf=

calls:
	kw_write

called by:
	f_rlse

history:
	initial coding 7/8/76 by S. F. Holmgren

*/
kw_frlse()
{
	kw_buf.kw_op = kwi_frlse;
	kw_buf.kw_id = fp->f_id;
	kw_write( &kw_buf.kw_data[0] - &kw_buf );
}
/*name:
	kw_timo

function:
	send a timeout instruction to the kernel

algorithm:
	set up the instruction
	send it to the kernel

parameters:
	none

returns:
	nothing

globals:
	timeo=

calls:
	kw_write

called by:
	main

history:
	initial coding by greep

*/
kw_timo()
{
	struct
	{       int     hz;
		char    end;
	};

	kw_buf.kw_op = kwi_timo;
	kw_buf.kw_data->hz = TIMEINT;
	kw_write( &kw_buf.kw_data->end - &kw_buf.lo_byte );
	timeo++;                        /* indicate a request is outstanding */
}


