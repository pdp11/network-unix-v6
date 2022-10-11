#
#include "../h/param.h"
#include "../h/netparam.h"
#include "../h/inode.h"
#include "../h/file.h"
#include "../h/ncp.h"
#include "../h/net.h"
#include "../h/netopen.h"		/* for open parameters */
#include "../h/user.h"
#include "../h/conf.h"
#include "../h/reg.h"
#include "../h/contab.h"
#include "../h/systm.h"

#ifdef NCP              /* jsq bbn 10/19/78 */

/* SCCS PROGRAM IDENTIFICATION STRING */
/*char id_nopcls[] "~|^`nopcls.c\tV3.9E2\t09Mar78\n";*/


/*name:
	netopen

function:
	serves as an interface between the user in his open calls
	and the ncpdaemon, which actually does the opening of the
	connections.

algorithm:
	if this /dev/net/rawmsg, do nothing
	make sure ncp is up
	allocate a file for the user
	initialize flags bits for the file
	format an open command for the ncpdaemon
	if this is a server request plug in server and lskt num
	else
	if this is a standard open, second param a number between 0 and 4
	then set type to zero.
	else load user parameters from his space(fubyte)
	find a free entry in the network file table
	make that entry known to the network portions of the 
	system.
	send the open command off to the daemon(to_ncp)
	wait for a return
	check for an error ( hibyte zero, lobyte contains error type )
	point fid part of write socket at file
	mark file as user using
	set nominal allocation
	exit
	ERROR
	return file to system resources

parameters:
	aip	-	inode of the file specified as the first param
			to the open
			if zero means that this a request to issue a server
			open to the ncpdaemon
	mode	-	0 - 2 menas standard telnet open read, write, r/w
			any other number denotes a pointer to a structure
			of type nopen, with parameters filled in by the
			user.
			if aip is zero, mode is the local socket number
			to do the listen on.



returns:
	a file to be used in further communication with the network over
	this port
	or zero if there was an error

globals:
	none

calls:
	infiletab
	to_ncp
	bcopyin
	net_frlse
	spl_imp
	rawopen         (rawnet.c)

called by:
	open1 (/usr/sys/sys2.c)
	tsrvopen	(newsrvtel.c)

history:
	initial coding 1/7/75 by S. F. Holmgren
	modified for parameterized open 6/11/75 Holmgren
	modified to net_frlse 7/8/76 by S. F. Holmgren
	modified to check for server open 8/27/76 by S. F. Holmgren
	modified by greep to see whether ncp is running
	modified by greep to free inode if no files available
	comment added at sleep 12Jun77 J.S.Kravitz
	inclusion of netopen.h for open parameters 12Jun77 J.S.Kravitz
	12Jun77 deleted setting rfnm rcvd bit in open J.S.Kravitz
	12Jun77 moved f_count++ to inside (infiletab()) if statement...
		if infiletab returned a null entry, the file descriptor
		would stay tied up forever
	14Jun77	J.S.Kravitz  put sleep in a loop to see if a wf_ready
		had really been done (FOPEN set)
		the ifdef SFHBYTE surrounds the following mods
	added multiple of 8 bytesize 01/27/78 S. F. Holmgren
	31Aug78 added a test clause to make sure that the nopen.o_relid
		field is set only if it is a relative open, S.Y. Chiu
	31Aug78 open error code taken from the global var "open_err"
		instead of the f_rdnode field of the file, S.Y. Chiu
	26Sep78 added call on rawopen jsq
	20Dec78 changed for long hosts jsq bbn
	3-13-79 put FSHORT in file structure flags if old style open jsq BBN

*/

netopen( aip,mode )
struct inode *aip;
{
	struct openparams oopen;
	struct nopenparams nopen;

	register *fp;		/* file ptr get aip for speed */
	register *ip;		/* inode ptr also for speed */
	register i;
	int dev;
	long host;              /* for host number from inode */
	int sdev;               /* is it short open? */

#ifdef RMI
	if (rawopen(aip, mode)) return;
#endif RMI

	/* see whether ncp is up */
	if( ncpopnstate == 0 )
	{       u.u_error = ENCPNO ;	/* no ncp to use! */ /*was ENXIO*/
		goto ng;
	}
	sdev = 0;       /* long open is the default */
	/* does user have available files */
	if( fp=falloc() )
	{
		/* set default params in structure */
		for( ip= &nopen; ip < (&nopen + 1); *ip++ = 0 );
		fp->f_inode = 0;	/* make sure file struct inited */

		if( aip == 0 )		/* this a server open */
		{
			nopen.o_type = 2;	/* server request listen */
			nopen.o_lskt = mode;	/* mode is lcl skt num */
		}
		else
		{
			/* get host number and release host spec file */
			dev = aip -> i_addr[0];
			sdev = !(dev.d_major == LONGDEV);
			if ((!sdev) && (dev.d_minor == -1)) {
				host.loword = (&(aip -> i_addr[1])) -> loword;
				host.hiword = (&(aip -> i_addr[1])) -> hiword;
			} else {
				host = stolhost(dev.d_minor);
			}
			iput( aip );

			/* this an open with params or stnd telnet? */
			if( mode < 1 || mode > 3)
			{				/* open with params */
				/* get params */
				if (!sdev) {    /* new style */
#ifndef BUFMOD
				  bcopyin(mode, &nopen, sizeof(nopen));
#endif BUFMOD
#ifdef BUFMOD
				for (ip = &nopen, i = 0; i < sizeof nopen; i =+ 2)
					*ip++ = fuword(mode+i);
#endif BUFMOD
				} else {        /* old style */
#ifndef BUFMOD
				  bcopyin( mode,&nopen,sizeof(oopen));
#endif BUFMOD
#ifdef BUFMOD
				for (ip = &nopen, i = 0; i < sizeof oopen; i =+ 2)
					*ip++ = fuword(mode+i);
#endif BUFMOD

				  nopen.o_host = stolhost(nopen.o_frnhost);
				}
				if (nopen.o_type & o_relative)

				  /* the following two lines should only
				   * be executed if we are doing a relative
				   * open
				   */

				if( (i=nopen.o_relid) >= 0 && i < NOFILE )/* he give relid */
					nopen.o_relid = getf( i );	/* yes then trans */
				mode = (FREAD|FWRITE);			/* set mode */
			}
			if( nopen.o_host == 0 )      /* if no frh host */
			  nopen.o_host = host;  /* use host from inode */
		}

		if (!good_host(nopen.o_host)) {
			u.u_error = ENCPINV;
			goto out;
		}
		nopen.o_op = ncp_open;			/* set open opcode */
		nopen.o_id = fp;			/* give daemon id */

		spl_imp();
		if( ( ip = infiletab( 0 ) ) )		/* get file tab entry */
		{
			fp->f_count++;			/* inc count for ncp use */
			fp->f_flag = (FNET|mode);	/* init access and type */
			if (sdev) fp -> f_flag =| FSHORT;
			*ip = fp;			/* use file entry */
			spl0();
			to_ncp( &nopen, sizeof nopen, 0 );	/* send open off */
			do {
				sleep (fp, NETPRI);	/* wait for open */
				if( fp->f_flag & FERR )	/* get an error */
				{
					/*
					 * Return a useful error code to the
					 * user from the ncpdaemon
					 */
					u.u_error = open_err;
					goto out;		
				}
			} while ( ! (fp->f_flag & FOPEN) );

			/* if there is a read socket */
			if( (ip=fp->f_netnode[f_rdnode]))
			{
				ip->r_rdproc = u.u_procp;
					/* associate process with skt */
				ip->r_flags =| n_usriu;	/* mark user using */
				/* set nominal allocation */
#ifndef SFHBYTE
				ip->r_hiwat = nopen.o_nomall?nopen.o_nomall:256;
#endif SFHBYTE
#ifdef SFHBYTE
				ip->r_hiwat = nopen.o_nomall?nopen.o_nomall:
					((256*8)/ip->r_bsize);
#endif SFHBYTE
			}
			/* if there is a write socket */
			if( (ip=fp->f_netnode[f_wrtnode]))
			{
				ip->w_wrtproc = u.u_procp;
					/* associate process with skt */
				ip->w_flags =| n_usriu;	/* mark user using */
				ip->w_fid = fp;	/* load local file id */
			}
			if( aip == 0 )		/* srvr open */
			{
				i = u.u_ar0[R0];
				u.u_ofile[i] = 0;
			}
			return( fp );		/* return file ptr */
		}
		else
		{
			spl0();			/* may be an non zero prio */
			u.u_error = EMNETF;	/* to many open net files */ /*was EMFILE*/
		out:
			net_frlse( fp );		/* release file */
			i = u.u_ar0[R0];		/* get return file desc */
			u.u_ofile[i] = 0;		/* return u file entry */
			return( 0 );			/* say no soap */
		}
	}
ng:     if( aip )
		iput( aip );
}

/*name:
	netclose

function:
	serves to initiate a close to the ncpdaemon in response to a user
	close on a network file

algorithm:
	if this is a rawmessage file, do nothing
	if i am second to last guy closing the file 
		tell the last guy to close ( he may be hung on a read )
	if i am the last guy closing the file
		check each possible socket
			if its there
				if ncp still using
					send close
				destroy the socket

		call net_frlse and try to give back the file

parameters:
	afp	-	a pointer to the file which indirectly contains
			pointers to any inodes associated with the connection

returns:
	nothing

globals:
	none

calls:
	wakeup
	daecls
	daedes
	net_frlse
	ipcontab
	rmcontab
	rawclose        (rawnet.c)

called by:
	closef	(/usr/sys/fio.c)
	tsrvcls	(newsrvtel.c)

history:
	initial coding 1/7/75 by S. F. Holmgren
	modified to call net_frlse 7/8/76 by S. F. Holmgren
	modified by greep to remove connection table entry
	26Sep78 added call on rawclose jsq bbn
	modified to call ipcontab for long host jsq bbn 12-20-78
*/

netclose( afp )
struct netfile *afp;
{

	register i;
	register struct netfile *fp;	/* afp for speed */
	register *sktp;			/* pointer to inode for speed */
	
	
	fp = afp;

#ifdef RMI
	if (rawclose(afp)) return;
#endif RMI

	/* is this the second to last guy out ?? */
	if( fp->f_count == 3 )
	{

		/* yes then possibly tell someone waiting on rdskt */
		if( sktp = fp->f_netnode[ f_rdnode ] ) /* read skt? */
		{
			/* reset open */
			sktp->r_flags =& ~n_open;
			/* set eof */
			sktp->r_flags =| n_eof;
			/* let user know that close is taking place */
			wakeup( sktp );
		}
	}

	/* is this the last guy out ?? */
	if( fp->f_count <= 2 )
	{

		for( i=0; i < 3; i++ )
			if( (sktp = fp->f_netnode[i]) >= &inode[0] ) /* each inode*/
			{
				if( sktp->r_flags & n_ncpiu )
					daecls( fp,i );         /* say im closing */
				rmcontab (ipcontab (sktp));
				daedes( fp,i );
			}

	}

	net_frlse( fp );		/* decrement count and release if done  */
}

/*name:
	net_frlse

function:
	decrement the number of users who have a file
	if the count is zero, release the file.

algorithm:
	decrement the count
	if the count is zero 
		if the file is in the file table
			take it out
		clean up the flag field of the file

parameters:
	address of the file to be cleaned up

returns:
	nothing

globals:
	none

calls:
	infiletab

called by:
	wf_frlse	(ncpio.c)
	netopen
	netclose

history:
	initial coding 7/8/76 by S. F. Holmgren


*/
net_frlse( afp )
struct netfile *afp;
{
	register struct netfile *fp;
	register int *p;

	fp = afp;
	if( --fp->f_count == 0 )
	{
		/* file is release by zero count clean up */
		if( p = infiletab( fp ) )
			*p = 0;		/* take it out of filetab */
		fp->f_flag = 0;		/* clean up flags field */
	}
}

/*name:
	daecls

function:
	sends a close command to the ncpdaemon

algorithm:
	build the close command according to structure nclose
	send it to the ncpdaemon

parameters:
	a pointer to a file
	and offset into that structure specifing which inode the close
	references

returns:
	nothing.

globals:
	none

calls:
	to_ncp

called by:
	netclose
	iclean	(ncpio.c)

history:
	initial coding 1/7/75 by S. F. Holmgren
	second to_ncp argument changed to sizeof(nclose) jsq bbn 1-23-79
*/

daecls( afp,offset )
struct netfile *afp;
char offset;
{

	struct
	{
		char c_op;	/* contains close opcode */
		char c_indx;	/* indx into f_netnode of skt to close */
		struct netfile *c_fp;	/* file ptr for daemon */
	} nclose;

	nclose.c_op = ncp_close;	/* load close opcode */
	nclose.c_indx = offset;		/* load offset */
	nclose.c_fp = afp;		/* load file pointer */

	to_ncp( &nclose,sizeof(nclose),0 );          /* send off to ncp */

}

/*name:
	daedes

function:
	to release an inode to the system

algorithm:
	remove the entry from the appropriate connection table
	release in associated queued data
	clean up the inode
	release the inode

parameters:
	a pointer to a file containing a pointer to the inode to be
	released
	and offset into that file structure, denoting which inode
	to be released.

returns:
	nothing

globals:
	none

calls:
	freemsg

called by:
	netclose
	iclean	(ncpio.c)

history:
	initial coding 1/7/75 by S. F. Holmgren
	modified to call freemsg 7/7/76 by S. F. Holmgren
	modified by greep - no longer removes connection
	   table entry (this has already been done)
	modified by JSK - spl_imp around some of release code, so that
		consistancy check code works ok

*/

daedes( afp,offset )
struct netfile *afp;
char offset;
{

	register struct netfile *fp;
	register int *ip;
	register int *sktp;
	int	saveps;

	fp = afp;
	ip = fp->f_netnode[offset];
	if( ip == 0 )	return;

	/* free any queued data if this is read socket */
	if( offset == f_rdnode )
		freemsg( ip->r_msgq );

	saveps = PS->integ;
	spl_imp();
#ifndef MSG
	/* point ip at top of inode instead of i_addr array */
	ip =- 7;
#endif MSG
	/* clean up inode */
	for( sktp = &ip->i_dev; sktp <= &ip->i_addr[8]; sktp++ )
		*sktp = 0;

	/* reset file socket pointer */
	fp->f_netnode[offset] = 0;

	/* release inode */
	ip->i_flag = ip->i_count = ip->i_lastr = 0;

	PS->integ = saveps;


}
/*name:
	netstat

function:
	return host, localsocket, and foreignsocket from read connection

algorithm:
	if read socket exists
		if there is a connection table entry 
			copy in relavent info
			copy info into user

parameters:
	address of a network file
	user address

returns:
	nothing

globals:
	contab

calls:
	ipcontab

called by:
	stat (sys3)

history:
	initial coding 4/16/75 by S. F. Holmgren
	12Jun77 J.S.Kravitz Inclusion of openparams used in
		declaration. Note that the connection table should
		eventually use long integers
	19OCT78 jsq bbn added rawsstat call
	changed for long host number jsq bbn 12-20-78
	26Feb79 jsq BBN added chiu's MSG changes from sys.112.12
	13Mar79 jsq BBN take advantage of expanded file structure
		by using FSHORT flag bit to determine type of structure
		and host number to use
*/
netstat( filep,usraddr )
{

	register *fp;
	register *conp;
	register ip;
	int got;
	long itol ();

	struct  nopenparams nopen;       /* place to build returned info */
	struct  openparams oopen;
#ifdef MSG
	int saveps;
#endif MSG

	fp = filep;
	got = 0;

#ifdef RMI
	if (rawsstat(fp, usraddr)) return;
#endif RMI

	if ((conp = ipcontab(fp -> f_netnode[ f_rdnode ]))
#ifdef MSG
	    || (conp = ipcontab(fp -> f_netnode[ f_wrtnode ]))
#endif MSG
	      ) {
		got++;
		nopen.o_frnhost = ltoshost(conp -> c_host);
		nopen.o_host = conp -> c_host;
		nopen.o_lskt    = conp->c_localskt;
		nopen.o_fskt = /* mod for longs 12Jun77 J.S.K */
			itol (conp->c_fskt[0], conp->c_fskt[1]);
	}
#ifdef MSG
	saveps = PS->integ;
	spl_imp();
	if (ip = fp -> f_netnode[ f_rdnode ]) {
		nopen.o_timeo = ip->INS_cnt;
		ip->INS_cnt = 0;
	}
	if (ip = fp->f_netnode[f_wrtnode]) {
		nopen.o_relid = ip->INR_cnt;
		ip->INR_cnt = 0;
	} else  nopen.o_relid = 0;
	PS->integ = saveps;
#endif MSG
	if (got) {
		if (fp -> f_flag & FSHORT) ip = sizeof oopen;
		else ip = sizeof nopen;
		ip =>> 1;
		fp = usraddr;
		conp = &nopen;
		do
			suword( fp++,*conp++ );
		while( --ip );
		return;
	}
	u.u_error = EBADF;
}

/*

name: net_reset
function: tell the daemon to reset all it knows about a host
algorithm:
	if we don't have a legal host number, give an error
	build a kernel to daemon reset command
	send it
parameters:     a host number passed from users r0 and r1
returns:        possible EBADF in u.u_error
globals:        LHOI, LIMPS
calls:  nothing
called by: system
history:
	modified from original for long hosts jsq bbn 1-25-79
 */
net_reset()
{

	struct
	{
		char  net_op;
		char  net_unused;
		long  net_rsthost;
	}nreset;
	long i;

	i.hiword = u.u_ar0[R0];
	i.loword = u.u_ar0[R1];
	if (!(i && good_host(i))) {
		u.u_error = EBADF;
		return;
	}
	nreset.net_op = ncp_rreset;   /* assign reset opcode for daemon */
	nreset.net_rsthost = i; /* assign host num */
	to_ncp( &nreset,sizeof(nreset),0 );  /* send instruction to daemon */
	return;
}

#endif NCP
