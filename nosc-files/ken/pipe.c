#
#include "param.h"
#include "systm.h"
#include "user.h"
#include "inode.h"
#include "file.h"
#include "reg.h"
#ifdef RAND
#include "net_net.h"
#endif RAND

/*
 * Max allowable buffering per pipe.
 * This is also the max size of the
 * file created to implement the pipe.
 * If this size is bigger than 4096,
 * pipes will be implemented in LARG
 * files, which is probably not good.
 */
#define	PIPSIZ	4096

/*
 * The sys-pipe entry.
 * Allocate an inode on the root device.
 * Allocate 2 file structures.
 * Put it all together with flags.
 */
pipe()
{
	register *ip, *rf, *wf;
	int r;

	ip = ialloc(rootdev);
	if(ip == NULL)
		return;
	rf = falloc();
	if(rf == NULL) {
		iput(ip);
		return;
	}
	r = u.u_ar0[R0];
	wf = falloc();
	if(wf == NULL) {
		rf->f_count = 0;
		u.u_ofile[r] = NULL;
		iput(ip);
		return;
	}
	u.u_ar0[R1] = u.u_ar0[R0];
	u.u_ar0[R0] = r;
	wf->f_flag = FWRITE|FPIPE;
	wf->f_inode = ip;
	rf->f_flag = FREAD|FPIPE;
	rf->f_inode = ip;
	ip->i_count = 2;
	ip->i_flag = IACC|IUPD;
	ip->i_mode = IALLOC;
}

#ifdef RAND

empty() 	/* rand addition: see whether pipe or tty is empty */
{	register *fp, *ip;
	int v[3];
	fp = getf(u.u_ar0[R0]); 	/* get file ptr; copied from rdwr */
	if (fp != NULL && fp->f_flag&FREAD)
	{	ip = fp->f_inode;		/* from readp */
		if (fp->f_flag&FPIPE)
		{	/* test from readp */
		 u.u_ar0[R0] = (fp->f_offset[1] == ip->i_size1 /*char count*/
	      /* rand:bobg */  && !(ip->i_flag & IEOFP) /* no EOF waiting */
			       && ip->i_count >=2   /* check other end */
			       && (ip->i_mode&IWRITE)==0); /* more to write?*/
			return;
		}
		else
		if (fp->f_flag&FNET) {
			if (ip = fp->f_netnode [f_rdnode])
				u.u_ar0[R0] = ip->r_qtotal == 0 /* nothing in queue */
					&& !(ip->r_flags & n_eof); /* connection not closed */
			else
				u.u_error = EBADF;
			return;
		}
		else
		{	/* relies on rand change to return 'non-empty flag'
			 * in u.u_arg[4] */
			sgtty(v);
			if (u.u_error == 0)
			{       u.u_ar0[R0] = u.u_arg[4];
				return;
			}
		}
	}
	u.u_error = EBADF;
}

#endif RAND
/*
 * Read call directed to a pipe.
 */
readp(fp)
int *fp;
{
	register *rp, *ip;

	rp = fp;
	ip = rp->f_inode;

loop:
	/*
	 * Very conservative locking.
	 */

	plock(ip);

	/*
	 * If the head (read) has caught up with
	 * the tail (write), reset both to 0.
	 */

	if(rp->f_offset[1] == ip->i_size1) {
#ifdef RAND
		if (ip->i_flag & IEOFP)         /* rand:bobg--EOF on pipe */
		{
			ip->i_flag =& ~IEOFP;   /* `read' the EOF */
			wakeup(ip+3);           /* allow further writes */
			prele(ip);              /* didn't even need it! */
			return;                 /* EOF simulated */
		}
#endif RAND
		if(rp->f_offset[1] != 0) {
			rp->f_offset[1] = 0;
			ip->i_size1 = 0;
			if(ip->i_mode&IWRITE) {
				ip->i_mode =& ~IWRITE;
				wakeup(ip+1);
			}
		}

		/*
		 * If there are not both reader and
		 * writer active, return without
		 * satisfying read.
		 */

		prele(ip);
		if(ip->i_count < 2)
			return;
		ip->i_mode =| IREAD;
		sleep(ip+2, PPIPE);
		goto loop;
	}

	/*
	 * Read and return
	 */

	u.u_offset[0] = 0;
	u.u_offset[1] = rp->f_offset[1];
	readi(ip);
	rp->f_offset[1] = u.u_offset[1];
	prele(ip);
}

/*
 * Write call directed to a pipe.
 */
writep(fp)
{
	register *rp, *ip, c;

	rp = fp;
	ip = rp->f_inode;
	c = u.u_count;

loop:

	/*
	 * If all done, return.
	 */

	plock(ip);
	if(c == 0) {
		prele(ip);
		u.u_count = 0;
		return;
	}

	/*
	 * If there are not both read and
	 * write sides of the pipe active,
	 * return error and signal too.
	 */

	if(ip->i_count < 2) {
		prele(ip);
		u.u_error = EPIPE;
		psignal(u.u_procp, SIGPIPE);
		return;
	}

#ifdef RAND
	/*
	 * rand:bobg--if there is a pending eof, sleep till it is read
	 */

	while (ip->i_flag & IEOFP) /* any other eofs pending? */
	{
		prele(ip);
		sleep(ip+3,PPIPE);  /* sleep till a reader reads an eof */
		plock(ip);
	}

#endif RAND
	/*
	 * If the pipe is full,
	 * wait for reads to deplete
	 * and truncate it.
	 */

	if(ip->i_size1 == PIPSIZ) {
		ip->i_mode =| IWRITE;
		prele(ip);
		sleep(ip+1, PPIPE);
		goto loop;
	}

	/*
	 * Write what is possible and
	 * loop back.
	 */

	u.u_offset[0] = 0;
	u.u_offset[1] = ip->i_size1;
	u.u_count = min(c, PIPSIZ-u.u_offset[1]);
	c =- u.u_count;
	writei(ip);
	prele(ip);
	if(ip->i_mode&IREAD) {
		ip->i_mode =& ~IREAD;
		wakeup(ip+2);
	}
	goto loop;
}

#ifdef RAND
/*
 * eofp routine (rand:bobg) to write a single eof on a pipe
 * passed a ptr to an inode by eofp/sys1 code
 */
eofp(pin)
{
	register int *ip;

	plock(ip = pin);
	if(ip->i_count < 2)     /* make sure someone will read EOF */
	{
		prele(ip);      /* other end must have died */
		u.u_error = EPIPE;
		psignal(u.u_procp, SIGPIPE); /* let writer know */
		return;
	}
	while (ip->i_flag & IEOFP) /* any other eofs pending? */
	{                       /* sleep till last EOF handled */
		prele(ip);
		sleep(ip+3,PPIPE);  /* sleep till a reader reads an eof */
		plock(ip);
	}
	ip->i_flag =| IEOFP;    /* announce EOF to next read */
	prele(ip);
	if (ip->i_mode & IREAD) /* wakeup any waiting reads */
	{
		ip->i_mode =& ~IREAD;
		wakeup(ip+2);
	}
}

#endif RAND
/*
 * Lock a pipe.
 * If its already locked,
 * set the WANT bit and sleep.
 */
plock(ip)
int *ip;
{
	register *rp;

	rp = ip;
	while(rp->i_flag&ILOCK) {
		rp->i_flag =| IWANT;
		sleep(rp, PPIPE);
	}
	rp->i_flag =| ILOCK;
}

/*
 * Unlock a pipe.
 * If WANT bit is on,
 * wakeup.
 * This routine is also used
 * to unlock inodes in general.
 */
prele(ip)
int *ip;
{
	register *rp;

	rp = ip;
	rp->i_flag =& ~ILOCK;
	if(rp->i_flag&IWANT) {
		rp->i_flag =& ~IWANT;
		wakeup(rp);
	}
}
