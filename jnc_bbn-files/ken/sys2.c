#
#include "../h/param.h"
#include "../h/systm.h"
#include "../h/user.h"
#include "../h/reg.h"
#include "../h/file.h"
#include "../h/inode.h"
#include "../h/conf.h"

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
	register struct inode *ip;
	extern int awakei();

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
#ifdef NETWORK
	if( fp->f_flag & FNET )
	{
		if( m == FREAD )
			netread( fp, fp->f_netnode[ f_rdnode ] ); /* AGN */
		else
			netwrite( fp->f_netnode[ f_wrtnode ],
				  &u.u_base,
				  &u.u_count,
				  0
				);
	}
	else
#endif
	if(fp->f_flag&FPIPE) {
		/* the following line put in for await/capac
		 * S.Y. Chiu 12Sept78
		 */
		awakei(fp->f_inode,-1);    /* awake other end of pipe */
		/* pass 0 flag to readp so it will call awakei
		(BBN:mek 11/20/78 ) */

		if(m==FREAD)
			readp(fp,0); else
			writep(fp);
	} else {
		u.u_offset[1] = fp->f_offset[1];
		u.u_offset[0] = fp->f_offset[0];
		ip = fp->f_inode;
		if(m==FREAD)
			readi(ip); else
			writei(ip);
		dpadd(fp->f_offset, u.u_arg[1]-u.u_count);
		if (((ip->i_mode&IFMT) == 0) || ((ip->i_mode&IFMT) == IFDIR))
			dpadd(u.u_io, u.u_arg[1]-u.u_count);
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
 */
creat()
{
	register *ip;
	extern uchar;

	ip = namei(&uchar, 1);
	if(ip == NULL) {
		if(u.u_error)
			return;
		ip = maknode(u.u_arg[1]&07777&(~ISVTX));
		if (ip==NULL)
			return;
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
	if( m < 1 || m > 7 )
	{
		m = (FREAD|FWRITE);
		mode--;
	}
	if(trf != 2) {
		if(m&FREAD)
			access(rip, IREAD);
		if(m&FEXCLU)
			access(rip, IEXOPN);
		if(m&(FWRITE|FEXCLU)) {
			access(rip, IWRITE);
			if((rip->i_mode&IFMT) == IFDIR)
				u.u_error = EISDIR;
		}
	}
	if(u.u_error)
		goto out;
#ifdef NETWORK
	if ( ((rip->i_mode & IFMT) == IFCHR) && (rip->i_addr[0].d_major  <= 0177777) )
	{
		/* normal net open */
		netopen( rip,mode );
		return;
	}
#endif
	if(trf)
		itrunc(rip);
	prele(rip);
	if ((fp = falloc()) == NULL)
		goto out;
	fp->f_flag = m&(FREAD|FWRITE|FEXCLU);
	fp->f_inode = rip;
	i = u.u_ar0[R0];
	openi(rip, m&FWRITE);
	if(u.u_error == 0)
	{       if (m&FEXCLU)
			rip->i_flag =| IEXCLU;
		return;
	}
	u.u_ofile[i] = NULL;
	fp->f_count--;

out:
	iput(rip);
}

/*
 * close system call
 *
 * Modified 2/78 (BBN:jfh) to disable (see awaitr.c)
 *
 */
close()
{
	register *fp;

	fp = getf(u.u_ar0[R0]);
	if(fp == NULL)
		return;
	awtdis(u.u_ar0[R0]);	/* disable */
	u.u_error=0;		/* ignore any error (may not have been enb) */
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
#ifdef NETWORK
	if( fp->f_flag&FNET) 
	{
		u.u_error = ESNET;
		return;
	}
#endif
	if(fp->f_flag&(FPIPE|FPORT)) { /* "|FPORT" added at Rand - jsz 3/76 */
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
	if((ip->i_nlink&255) >= 255) {  /* khd: mask off high bits */
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
	ip->i_flag =| IACC;     /* IUPD to IACC jsq BBN 4-12-79 */

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
	char *d[2];

	spl7();
	d[0] = time[0];
	d[1] = time[1];
	dpadd(d, u.u_ar0[R0]);

	while(dpcmp(d[0], d[1], time[0], time[1]) > 0) {
		if(dpcmp(tout[0], tout[1], time[0], time[1]) <= 0 ||
		   dpcmp(tout[0], tout[1], d[0], d[1]) > 0) {
			tout[0] = d[0];
			tout[1] = d[1];
		}
		sleep(tout, PSLEP);
	}
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

/*
 *	reboot system primitive -- local mod
 *      put here to keep number of file modifications down
 */
reboot()
{
	if (!suser()) { u.u_error = EPERM; return; }
	/* arguments: major device, code for disk boot, inumber if (code) */
	boot(u.u_ar0[R0], u.u_ar0[R4], u.u_ar0[R3]);    /* in m70.s */
	u.u_error = ENODEV;     /* boot failed:  nonexistent device */
}
