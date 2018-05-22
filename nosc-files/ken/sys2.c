#
#include "param.h"
#include "systm.h"
#include "user.h"
#include "proc.h"
#include "reg.h"
#include "file.h"
#include "inode.h"
#ifdef NETSYS
#include "conf.h"
#endif NETSYS

/*
 * read system call
 */
read()
{
	rdwr(FREAD);
}

/*
 * write system call
 */
write()
{
	rdwr(FWRITE);
}

/*
 * common code for read and write calls:
 * check permissions, set base, count, and offset,
 * and switch out to readi, writei, or pipe code.
 */
rdwr(mode)
{
	register *fp, m;

	m = mode;
	fp = getf(u.u_ar0[R0]);
	if(fp == NULL)
		return;
	if((fp->f_flag&m) == 0) {
		u.u_error = EBADF;
		return;
	}
	u.u_base = u.u_arg[0];
	u.u_count = u.u_arg[1];
	u.u_segflg = 0;
#ifdef NETSYS
	if( fp->f_flag & FNET )
	{
		if( m == FREAD )
			netread( fp->f_netnode[ f_rdnode ] );
		else
			netwrite( fp->f_netnode[ f_wrtnode ]);
	}
	else
#endif NETSYS
	if(fp->f_flag&FPIPE) {
		if(m==FREAD)
			readp(fp); else
			writep(fp);
	} else {
		u.u_offset[1] = fp->f_offset[1];
		u.u_offset[0] = fp->f_offset[0];
		if(m==FREAD)
			readi(fp->f_inode); else
			writei(fp->f_inode);
		dpadd(fp->f_offset, u.u_arg[1]-u.u_count);
	}
	u.u_ar0[R0] = u.u_arg[1]-u.u_count;
}

/*
 * open system call
 */
open()
{
	register *ip;
	extern uchar;

	ip = namei(&uchar, 0);
	if(ip == NULL)
		return;
	u.u_arg[1]++;
	open1(ip, u.u_arg[1], 0);
}

/*
 * creat system call
 *
 * bug fix reported May-June 1977
 * UNIX NEWS Vol 2, No. 5 p5-3
 * George Goble - Purdue University
 */
creat()
{
	register *ip;
	extern uchar;

	ip = namei(&uchar, 1);
	if(ip == NULL) {
		if(u.u_error)
			return;
		if( (u.u_cdir->i_nlink == 0)		/* bug fix */
		  && (fubyte(u.u_arg[0]) != '/') ) {	/* bug fix */
			u.u_error = ENOENT;             /* bug fix */
	err:            iput(u.u_pdir); /* namei left parent dir locked */
							/* bug fix */
			return;         /* bug fix */
		}
		ip = maknode(u.u_arg[1]&07777&(~ISVTX));
		if (ip==NULL)
			goto err;       /* bug fix */
		open1(ip, FWRITE, 2);
	} else
		open1(ip, FWRITE, 1);
}

/*
 * common code for open and creat.
 * Check permissions, allocate an open file structure,
 * and call the device open routine if any.
 */
open1(ip, mode, trf)
int *ip;
{
	register struct file *fp;
	register *rip, m;
	int i;

	rip = ip;
	m = mode;
#ifdef NETSYS
	if( m < 1 || m > 3 )
	{
		m = (FREAD|FWRITE);
		mode--;
	}
#endif NETSYS
	if(trf != 2) {
		if(m&FREAD)
			access(rip, IREAD);
		if(m&FWRITE) {
			access(rip, IWRITE);
			if((rip->i_mode&IFMT) == IFDIR)
				u.u_error = EISDIR;
		}
	}
	if(u.u_error)
		goto out;
#ifdef NETSYS
	if ( ((rip->i_mode & IFMT) == IFCHR)
		&& (rip->i_addr[0].d_major <= 0177777) )
	{
		/* normal net open */
		netopen( rip,mode );
		return;
	}
#endif NETSYS
	if(trf)
		itrunc(rip);
	prele(rip);
	if ((fp = falloc()) == NULL)
		goto out;
	fp->f_flag = m&(FREAD|FWRITE);
	fp->f_inode = rip;
	i = u.u_ar0[R0];
	openi(rip, m&FWRITE);
	if(u.u_error == 0)
		return;
	u.u_ofile[i] = NULL;
	fp->f_count--;

out:
	iput(rip);
}

/*
 * close system call
 */
close()
{
	register *fp;

	fp = getf(u.u_ar0[R0]);
	if(fp == NULL)
		return;
	u.u_ofile[u.u_ar0[R0]] = NULL;
	closef(fp);
}

/*
 * seek system call
 */
seek()
{
	int n[2];
	register *fp, t;

	fp = getf(u.u_ar0[R0]);
	if(fp == NULL)
		return;
#ifdef NETSYS
	if( fp->f_flag&FNET) 
	{
		u.u_error = ESNET;
		return;
	}
#endif NETSYS
	if(fp->f_flag&FPIPE) {
		u.u_error = ESPIPE;
		return;
	}
	t = u.u_arg[1];
	if(t > 2) {
		n[1] = u.u_arg[0]<<9;
		n[0] = u.u_arg[0]>>7;
		if(t == 3)
			n[0] =& 0777;
	} else {
		n[1] = u.u_arg[0];
		n[0] = 0;
		if(t!=0 && n[1]<0)
			n[0] = -1;
	}
	switch(t) {

	case 1:
	case 4:
		n[0] =+ fp->f_offset[0];
		dpadd(n, fp->f_offset[1]);
		break;

	default:
		n[0] =+ fp->f_inode->i_size0&0377;
		dpadd(n, fp->f_inode->i_size1);

	case 0:
	case 3:
		;
	}
	fp->f_offset[1] = n[1];
	fp->f_offset[0] = n[0];
}

/*
 * link system call
 */
link()
{
	register *ip, *xp;
	extern uchar;

	ip = namei(&uchar, 0);
	if(ip == NULL)
		return;
	if(ip->i_nlink >= 127) {
		u.u_error = EMLINK;
		goto out;
	}
	if((ip->i_mode&IFMT)==IFDIR && !suser())
		goto out;
	/*
	 * unlock to avoid possibly hanging the namei
	 */
	ip->i_flag =& ~ILOCK;
	u.u_dirp = u.u_arg[1];
	xp = namei(&uchar, 1);
	if(xp != NULL) {
		u.u_error = EEXIST;
		iput(xp);
	}
	if(u.u_error)
		goto out;
	if(u.u_pdir->i_dev != ip->i_dev) {
		iput(u.u_pdir);
		u.u_error = EXDEV;
		goto out;
	}
	wdir(ip);
	ip->i_nlink++;
	ip->i_flag =| IUPD;

out:
	iput(ip);
}

/*
 * mknod system call
 */
mknod()
{
	register *ip;
	extern uchar;

	if(suser()) {
		ip = namei(&uchar, 1);
		if(ip != NULL) {
			u.u_error = EEXIST;
			goto out;
		}
	}
	if(u.u_error)
		return;
	ip = maknode(u.u_arg[1]);
	if (ip==NULL)
		return;
	ip->i_addr[0] = u.u_arg[2];

out:
	iput(ip);
}

/*
 * sleep system call
 * not to be confused with the sleep internal routine.
 */
sslep()
{
	/*
	 * modified 31Oct77 by J.S.Kravitz and R.J.Balocca to
	 * make sleep better (no unneeded swaps)
	 *
	 * note that sleep now changes 'alarm' value, it is not
	 * a good idea to enter sleep with an alarm set... not
	 * a common mode of programming anyway
	 */
	alarm ();	/* set up alarm clock with parameters from user */
	spl6();
	while (u.u_procp->p_clktim)
		sleep (&u.u_procp->p_clktim, PSLEP);
	spl0();
}

/*
 * access system call
 */
saccess()
{
	extern uchar;
	register svuid, svgid;
	register *ip;

	svuid = u.u_uid;
	svgid = u.u_gid;
	u.u_uid = u.u_ruid;
	u.u_gid = u.u_rgid;
	ip = namei(&uchar, 0);
	if (ip != NULL) {
		if (u.u_arg[1]&(IREAD>>6))
			access(ip, IREAD);
		if (u.u_arg[1]&(IWRITE>>6))
			access(ip, IWRITE);
		if (u.u_arg[1]&(IEXEC>>6))
			access(ip, IEXEC);
		iput(ip);
	}
	u.u_uid = svuid;
	u.u_gid = svgid;
}

#ifdef NETSYS
/*
 *	tell the ncpdaemon to reset all he knows about a host 
 */
net_reset()
{

	struct
	{
		char  net_op;
		char  net_rsthost;
	}nreset;


	register char *ip;
	extern uchar;

	if (!suser())
	{
		return;			/* suser sets u.u_error to EPERM */
	}
	nreset.net_op = 4;	/* assign reset opcode for daemon */
	nreset.net_rsthost = u.u_ar0[R0];	/* assign host number*/
	switch ( to_ncp (&nreset, 2, 0) )	/* send instruction to daemon */
	{
	case 0:
		u.u_ar0[R0] = 0;
		return;

	case 1:
		u.u_error = ENCPBUF;
		return;

	default:	/* -1 and anything else we dont know about */
		u.u_error = ENCPNO;
		return;
	}
}
#endif NETSYS
