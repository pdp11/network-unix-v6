#
#include "param.h"
#include "inode.h"
#include "file.h"
#include "net_ncp.h"
#include "net_net.h"
#include "net_open.h"	/* for open parameters */
#include "user.h"
#include "conf.h"
#include "reg.h"
#include "net_contab.h"
#include "systm.h"

#ifdef SCCSID
/* SCCS PROGRAM IDENTIFICATION STRING */
char id_nopcls[] "~|^`nopcls.c\tV3.9E2\t09Mar78\n";
#endif

int ncpopnstate;       /*current state of ncp's io channel. 0=closed,>0=open*/

/*name:
	netopen

function:
	serves as an interface between the user in his open calls
	and the ncpdaemon, which actually does the opening of the
	connections.

algorithm:
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
	spl_imp

called by:
	open1 (sys2.c)
	tsrvopen	(newsrvtel.c)

history:
	initial coding 1/7/75 by S. F. Holmgren
	modified for parameterized open 6/11/75 Holmgren
	modified to net_frlse 7/8/76 by S. F. Holmgren (deleted below 20Apr78)
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
	20Apr78	Rearranged code so that trapped signals don't hurt
		us if we never come back from the sleep J.S.Kravitz
	20Apr78	Removed the aip == 0 code, since it's not used JSK

*/

netopen (aip, mode)
struct inode *aip;
{

	struct openparams nopen;

	register *fp;		/* file ptr get aip for speed */
	register *ip;		/* inode ptr also for speed */
	register i;
	int hst;				/* for inode host num */

	/*
	 * get host number and release host spec file
	 */
	hst = (aip->i_addr[0]).d_minor;
	iput( aip );

	/* see whether ncp is up */
	if( ncpopnstate == 0 )
	{
		u.u_error = ENCPNO;	/* no ncp to use! (was ENXIO) */
		return;
	}

	/* does user have available files */
	if( fp=falloc() )
	{
		/* set default params in structure */
		for( ip= &nopen; ip <= &nopen.o_relid; *ip++ = 0 );
		fp->f_inode = 0;	/* make sure file struct inited */

		/* this an open with params or stnd telnet? */
		if( mode < 1 || mode > 3)
		{
			/*
			 * user has specified parameters, so copy them in
			 */
#ifndef UCBUFMOD
			bcopyin (mode, &nopen, sizeof nopen);
#endif UCBUFMOD
#ifdef UCBUFMOD
			for(ip = &nopen, i = 0; i < sizeof nopen; i=+2)
				*ip++ = fuword(mode + i);
#endif UCBUFMOD
			if( (i=nopen.o_relid) >= 0 && i < NOFILE )/* he give relid */
				nopen.o_relid = getf( i );	/* yes then trans */
			mode = (FREAD|FWRITE);			/* set mode */
		}
		if( nopen.o_frnhost == 0 )	/* if no frh host */
			nopen.o_frnhost = hst;	/* use hst from inode */

		nopen.o_op = ncp_open;			/* set open opcode */
		nopen.o_id = fp;			/* give daemon id */

		spl_imp();
		/*
		 * because we may never come back from the sleep
		 * later, we fudge the ofile descripter JSK
		 */
		u.u_ofile[u.u_ar0[R0]] = 0;

		if( ( ip = infiletab( 0 ) ) )		/* get file tab entry */
		{
			fp->f_flag = (FNET|mode);	/* init access and type */
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
					u.u_error = fp->f_netnode[f_rdnode];
					return;
				}
			} while ( ! (fp->f_flag & FOPEN) );

			/*
			 * this was moved here to compensate for the fact
			 * that the user may not ever return from the
			 * sleep, if the user gets a trapped signal
			 */
			fp->f_count++;		/* inc count for ncp use */

			/* if there is a read socket */
			if( (ip=fp->f_netnode[f_rdnode]))
			{
				ip->r_flags =| n_usriu;	/* mark user using */
				/* set nominal allocation */
#ifndef SFHBYTE
				ip->r_hiwat = nopen.o_nomall?nopen.o_nomall:256;
#endif
#ifdef SFHBYTE
				ip->r_hiwat = nopen.o_nomall?nopen.o_nomall:
					((256*8)/ip->r_bsize);
#endif
			}
			/* if there is a write socket */
			if( (ip=fp->f_netnode[f_wrtnode]))
			{
				ip->w_flags =| n_usriu;	/* mark user using */
				ip->w_fid = fp;	/* load local file id */
			}
			/*
			 * this reloads the open file descriptor in
			 * the U, now that the file is really open
			 * JSK
			 */
			u.u_ofile[u.u_ar0[R0]] = fp;
			return;
		}
		else
		{
			/*
			 * decrement file descriptor use count (to zero)
			 * since we didn't need it after all
			 */
			--fp->f_count;
			u.u_error = EMNETF;	/* to many open net files */
						/* was EMFILE */
		}
	}
}

/*name:
	netclose

function:
	serves to initiate a close to the ncpdaemon in response to a user
	close on a network file

algorithm:
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
	incontab
	rmcontab

called by:
	closef	(fio.c)
	tsrvcls	(newsrvtel.c)

history:
	initial coding 1/7/75 by S. F. Holmgren
	modified to call net_frlse 7/8/76 by S. F. Holmgren
	modified by greep to remove connection table entry

*/

netclose( afp )
struct netfile *afp;
{

	register i;
	register struct netfile *fp;	/* afp for speed */
	register *sktp;			/* pointer to inode for speed */
	
	
	fp = afp;

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
				if( sktp->r_hostlink )
				    rmcontab( incontab( sktp->r_hostlink,sktp ) );
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

	to_ncp( &nclose,4,0 );		/* send off to ncp */

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
	/* point ip at top of inode instead of i_addr array */
	ip =- 7;
	
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
	incontab

called by:
	stat (sys3)

history:
	initial coding 4/16/75 by S. F. Holmgren
	12Jun77 J.S.Kravitz Inclusion of openparams used in
		declaration. Note that the connection table should
		eventually use long integers

*/
netstat( filep,usraddr )
{

	register *fp;
	register *conp;
	register ip;

	struct	openparams nopen;	/* place to build returned info */

	fp = filep;
	if( (ip = fp->f_netnode[ f_rdnode ]) != 0 )
		if( (conp=incontab( ip->r_hostlink,ip )) != 0 )
		{
			nopen.o_frnhost = conp->c_hostlink>>8;
			nopen.o_lskt    = conp->c_localskt;
			nopen.o_fskt[0] = conp->c_fskt[0];
			nopen.o_fskt[1] = conp->c_fskt[1];
			ip = sizeof nopen>>1;
			fp = usraddr;
			conp = &nopen;
			do
				suword( fp++,*conp++ );
			while( --ip );
			return;
		}
	u.u_error = EBADF;
}

