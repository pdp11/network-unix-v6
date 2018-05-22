#
#include "param.h"
#include "systm.h"
#include "user.h"
#include "proc.h"
#include "buf.h"
#include "reg.h"
#include "inode.h"
#ifdef RAND
#include "file.h"    /* rand:bobg for eofp call */
#endif RAND
#ifdef ACCTSYS
#include "acct.h"
#endif ACCTSYS

/*
 * exec system call.
 * Because of the fact that an I/O buffer is used
 * to store the caller's arguments during exec,
 * and more buffers are needed to read in the text file,
 * deadly embraces waiting for free buffers are possible.
 * Therefore the number of processes simultaneously
 * running in exec has to be limited to NEXEC.
 */
#define EXPRI	-1

exec()
{
	int ap, na, nc, *bp;
	int ts, ds, ss, sep;	/* In V6 ss was SSIZE: a const
				 * probably == 20.  This was a bad
				 * guess for initial stack size--
				 * too big for most initial stacks
				 * and too small for the max arg list.
				 * It was replaced by
				 * ss = ((ap+63)>>6)+SINCR
				 * i.e. the arg list size in 64 byte
				 * fragments plus the standard stack incr.
				 * RJB @ CAC 77-7-25
				 */
	register c, *ip;
	register char *cp;
#ifdef UCBUFMOD
	int *bk, t;
#endif UCBUFMOD
	extern uchar;
#ifdef ACCTSYS
	extern acc_vec;
#endif ACCTSYS

	/*
	 * pick up file names
	 * and check various modes
	 * for execute permission
	 */

	ip = namei(&uchar, 0);
	if(ip == NULL)
		return;
	if(access(ip, IEXEC))
		goto bad1;
	if((ip->i_mode & IFMT) != 0) {
		u.u_error = EACCES;
		goto bad1;
	}
	while(execnt >= NEXEC)
		sleep(&execnt, EXPRI);
	execnt++;
	bp = getblk(NODEV);

	/*
	 * pack up arguments into
	 * allocated disk buffer
	 */

#ifndef UCBUFMOD
	cp = bp->b_addr;
#endif not UCBUFMOD
#ifdef UCBUFMOD
	cp = 0; bk = bp->b_paddr;
#endif UCBUFMOD
	na = 0;
	nc = 0;
	while(ap = fuword(u.u_arg[1])) {
		na++;
		if(ap == -1)
			goto bad;
		u.u_arg[1] =+ 2;
		for(;;) {
			c = fubyte(ap++);
			if(c == -1)
				goto bad;
#ifndef UCBUFMOD
			*cp++ = c;
#endif not UCBUFMOD
#ifdef UCBUFMOD
			sbbyte(bk,cp++,c);
#endif UCBUFMOD
			nc++;
			if(nc > 510) {
				u.u_error = E2BIG;
				goto bad;
			}
			if(c == 0)
				break;
		}
	}
	if((nc&1) != 0) {
#ifndef UCBUFMOD
		*cp++ = 0;
#endif not UCBUFMOD
#ifdef UCBUFMOD
		sbbyte(bk,cp++,0);
#endif UCBUFMOD
		nc++;
	}

	/*
	 * read in first 8 bytes
	 * of file for segment
	 * sizes:
	 * w0 = 407/410/411 (410 implies RO text) (411 implies sep ID)
	 * w1 = text size
	 * w2 = data size
	 * w3 = bss size
	 */

	u.u_base = &u.u_arg[0];
	u.u_count = 8;
	u.u_offset[1] = 0;
	u.u_offset[0] = 0;
	u.u_segflg = 1;
	readi(ip);
	u.u_segflg = 0;
	if(u.u_error)
		goto bad;
	sep = 0;
	if(u.u_arg[0] == 0407) {
		u.u_arg[2] =+ u.u_arg[1];
		u.u_arg[1] = 0;
	} else
	if(u.u_arg[0] == 0411)
		sep++; else
	if(u.u_arg[0] != 0410) {
		u.u_error = ENOEXEC;
		goto bad;
	}
	if(u.u_arg[1]!=0 && (ip->i_flag&ITEXT)==0 && ip->i_count!=1) {
		u.u_error = ETXTBSY;
		goto bad;
	}

	/*
	 * find text and data sizes
	 * try them out for possible
	 * exceed of max sizes
	 */

	ts = ((u.u_arg[1]+63)>>6) & 01777;
	ds = ((u.u_arg[2]+u.u_arg[3]+63)>>6) & 01777;
	ap = nc+na*2+4;
	ss = ((ap+63)>>6)+SINCR;
	if(estabur(ts, ds, ss, sep))
		goto bad;

	/*
	 * allocate and clear core
	 * at this point, committed
	 * to the new image
	 */

	u.u_prof[3] = 0;
	xfree();
	expand(USIZE);
	xalloc(ip);
	c = USIZE+ds+ss;
	expand(c);
	while(--c >= USIZE)
		clearseg(u.u_procp->p_addr+c);

	/*
	 * read in data segment
	 */

	estabur(0, ds, 0, 0);
	u.u_base = 0;
	u.u_offset[1] = 020+u.u_arg[1];
	u.u_count = u.u_arg[2];
	readi(ip);

	/*
	 * initialize stack segment
	 */

	u.u_tsize = ts;
	u.u_dsize = ds;
	u.u_ssize = ss;
	u.u_sep = sep;
	estabur(u.u_tsize, u.u_dsize, u.u_ssize, u.u_sep);
#ifndef UCBUFMOD
	cp = bp->b_addr;
	ap = -ap;
	u.u_ar0[R6] = ap;
	suword(ap, na);
	c = -nc;
	while(na--) {
		suword(ap=+2, c);
		do
			subyte(c++, *cp);
		while(*cp++);
	}
#endif not UCBUFMOD
#ifdef UCBUFMOD
	bk = bp->b_paddr;
	cp = 0;
	ap = -ap;
	u.u_ar0[R6] = ap;
	suword(ap, na);
	c = -nc;
	while(na--) {
		suword(ap=+2, c);
		do
			subyte(c++, t = fbbyte(bk,cp++));
		while (t);
	}
#endif UCBUFMOD
	suword(ap+2, -1);

	/*
	 * set SUID/SGID protections, if no tracing
	 */

	if ((u.u_procp->p_flag&STRC)==0) {
		if(ip->i_mode&ISUID)
			if(u.u_uid != 0) {
				u.u_uid = ip->i_uid;
				u.u_procp->p_uid = ip->i_uid;
			}
		if(ip->i_mode&ISGID)
			u.u_gid = ip->i_gid;
	}

	/*
	 * clear sigs, regs and return
	 */

	c = ip;
	for(ip = &u.u_signal[0]; ip < &u.u_signal[NSIG]; ip++)
		if((*ip & 1) == 0)
			*ip = 0;
	for(cp = &regloc[0]; cp < &regloc[6];)
		u.u_ar0[*cp++] = 0;
	u.u_ar0[R7] = 0;
	for(ip = &u.u_fsav[0]; ip < &u.u_fsav[25];)
		*ip++ = 0;
	ip = c;

#ifdef	ACCTSYS
	/*
	 * Put the pieces together and write out an
	 * accounting record.
	 * JSK
	 */
	acc_vec.ae_type = AC_EXEC;
	acc_vec.ae_pid = u.u_procp->p_pid;
	acc_vec.ae_uid = u.u_uid;
	acc_vec.ae_gid = u.u_gid;
	for (c = 0; c < 14; c++)
		acc_vec.ae_name[c] = u.u_dbuf[c];
	accinput (&acc_vec);

	/*
	 * end of accounting
	 */
#endif ACCTSYS

bad:
	brelse(bp);
bad1:
	iput(ip);
	if(execnt >= NEXEC)
		wakeup(&execnt);
	execnt--;
}

/*
 * exit system call:
 * pass back caller's r0
 */
rexit()
{

	u.u_arg[0] = u.u_ar0[R0] << 8;
	exit();
}

/*
 * Release resources.
 * Save u. area for parent to look at.
 * Enter zombie state.
 * Wake up parent and init processes,
 * and dispose of children.
 */
exit()
{
	register int *q, a;
	register struct proc *p;

	p = u.u_procp;
	p->p_flag =& ~STRC;
	p->p_clktim = 0;                                /* ALARM */
	for(q = &u.u_signal[0]; q < &u.u_signal[NSIG];)
		*q++ = 1;
	for(q = &u.u_ofile[0]; q < &u.u_ofile[NOFILE]; q++)
		if(a = *q) {
			*q = NULL;
			closef(a);
		}
	iput(u.u_cdir);
	xfree();
	a = salloc(1);		/* salloc rounds up to one disk block */
#ifndef UCBUFMOD
	p = getblk(swapdev, a);
	bcopy(&u, p->b_addr, 256);
	bwrite(p);		/* what if this doesn't work? */
#endif not UCBUFMOD
#ifdef UCBUFMOD
	/*
	 * NOSC mod to U.C. original code.  Just use swap to move the U
	 * area onto the disk.
	 */
	swap(a, p->p_addr, 512/64, 0);
#endif UCBUFMOD
	q = u.u_procp;
	mfree(coremap, q->p_size, q->p_addr);
	q->p_addr = a;
	q->p_stat = SZOMB;

loop:
	for(p = &proc[0]; p <= &proc[NPROC]; p++)
	if(q->p_ppid == p->p_pid) {
		wakeup(&proc[1]);
		wakeup(p);
		for(p = &proc[0]; p <= &proc[NPROC]; p++)
		if(q->p_pid == p->p_ppid) {
			p->p_ppid  = 1;
			if (p->p_stat == SSTOP)
				setrun(p);
		}
		swtch();
		/* no return */
	}
	q->p_ppid = 1;
	goto loop;
}

/*
 * Wait system call.
 * Search for a terminated (zombie) child,
 * finally lay it to rest, and collect its status.
 * Look also for stopped (traced) children,
 * and pass back status from them.
 */
wait()
{
	register f, *bp;
	register struct proc *p;
#ifdef ACCTSYS
	extern acc_vec;
#endif ACCTSYS

	f = 0;

loop:
	for(p = &proc[0]; p < &proc[NPROC]; p++)
	if(p->p_ppid == u.u_procp->p_pid) {
		f++;
		if(p->p_stat == SZOMB) {
			u.u_ar0[R0] = p->p_pid;
			bp = bread(swapdev, f=p->p_addr);
			mfree(swapmap, 1, f);
			p->p_stat = NULL;
			p->p_ppid = 0;
			p->p_sig = 0;
			p->p_pgrp = 0;
			p->p_flag = 0;
			p->p_pid =+ 256;	/* spin process id */
			p->p_pid =& 077777;	/* don't let it go negative */
#ifndef UCBUFMOD
			p = bp->b_addr;
			u.u_cstime[0] =+ p->u_cstime[0];
			dpadd(u.u_cstime, p->u_cstime[1]);
			u.u_cstime [0] =+ p->u_stime[0];	/* JSK */
			dpadd (u.u_cstime, p->u_stime[1]);	/* JSK */
			u.u_cutime[0] =+ p->u_cutime[0];
			dpadd(u.u_cutime, p->u_cutime[1]);
			u.u_cutime [0] =+ p->u_utime[0];	/* JSK */
			dpadd (u.u_cutime, p->u_utime[1]);	/* JSK */
#endif not UCBUFMOD
#ifdef UCBUFMOD
			p = bp->b_paddr;
			/*
			 * NOSC mod corresponding to the change in exit above
			 */
			u.u_cstime[0] =+ fbword(p, &0->u_cstime[0]);
			dpadd(u.u_cstime, fbword(p, &0->u_cstime[1]));
			u.u_cstime [0] =+ fbword(p, &0->u_stime[0]);
			dpadd (u.u_cstime, fbword(p, &0->u_stime[1]));
			u.u_cutime[0] =+ fbword(p, &0->u_cutime[0]);
			dpadd(u.u_cutime, fbword(p, &0->u_cutime[1]));
			u.u_cutime [0] =+ fbword(p, &0->u_utime[0]);
			dpadd (u.u_cutime, fbword(p, &0->u_utime[1]));
#endif UCBUFMOD
#ifdef	ACCTSYS
			/*
			 * put the pieces of an accounting record
			 * together
			 *
			 */
			acc_vec.az_type		= AC_ZOMB;
			acc_vec.az_uid		= p-> u_ruid;
			acc_vec.az_hstime	= p-> u_stime [0];
			acc_vec.az_stime	= p-> u_stime [1];
			acc_vec.az_utime [0]	= p-> u_utime [0];
			acc_vec.az_utime [1]	= p-> u_utime [1];
			acc_vec.az_proc_id 	= u.u_ar0[R0];
			acc_vec.az_hsyscalls	= p-> u_hsyscalls;
			acc_vec.az_syscalls	= p-> u_syscalls;
			acc_vec.az_date [0]	= time [0];
			acc_vec.az_date [1]	= time [1];
			accinput (&acc_vec);
			/*
			 * end accounting
			 * JSK
			 */
#endif ACCTSYS
#ifndef UCBUFMOD
			u.u_ar0[R1] = p->u_arg[0];
#endif not UCBUFMOD
#ifdef UCBUFMOD
			u.u_ar0[R1] = fbword(p, &0->u_arg[0]);
			bp->b_dev.lobyte = -1;	/* no assoc. on this block */
#endif UCBUFMOD
			brelse(bp);
			return;
		}
		if(p->p_stat == SSTOP) {
			if((p->p_flag&SWTED) == 0) {
				p->p_flag =| SWTED;
				u.u_ar0[R0] = p->p_pid;
				u.u_ar0[R1] = (p->p_sig<<8) | 0177;
				return;
			}
			p->p_flag =& ~(STRC|SWTED);
			setrun(p);
		}
	}
	if(f) {
		sleep(u.u_procp, PWAIT);
		goto loop;
	}
	u.u_error = ECHILD;
}

#ifdef RAND
/*
 * The following was added by rand:bobg. Sfork is identical to fork
 * except that it sets the SNOSIG flag in the child inhibiting signals
 * from the teletype.
 */
sfork()
{
	fork1(1);
}

fork()
{
	fork1(0);
}

/*
 * fork system call. (Changed to fork1 by rand:bobg.)
 */
fork1(sflag)
#endif RAND
#ifndef RAND
fork()
#endif not RAND
{
	register struct proc *p1, *p2;
#ifdef ACCTSYS
	extern acc_vec;
#endif ACCTSYS

	p1 = u.u_procp;
	for(p2 = &proc[0]; p2 < &proc[NPROC]; p2++)
		if(p2->p_stat == NULL)
			goto found;
	u.u_error = EAGAIN;
	goto out;

found:
	if(newproc()) {
		u.u_ar0[R0] = p1->p_pid;
		u.u_cstime[0] = 0;
		u.u_cstime[1] = 0;
		u.u_stime[0] = 0;
		u.u_stime[1] = 0;
		u.u_cutime[0] = 0;
		u.u_cutime[1] = 0;
		u.u_utime[0] = 0;
		u.u_utime[1] = 0;
#ifdef RAND
		if (sflag) u.u_procp->p_flag =| SNOSIG;
			  /* rand:bobg--set NOSIG flag in child if an sfork */
#endif RAND
		return;
	}

#ifdef	ACCTSYS
	/*
	 *
	 * Ok, make an accounting entry. We seem to know the
	 * Child's ID here, as well as the parent's id.
	 * JSK
	 */
	acc_vec.af_type = AC_FORK;
	acc_vec.af_1spare = 0;
	acc_vec.af_rgid = u.u_rgid;
	acc_vec.af_ruid = u.u_ruid;
	acc_vec.af_gid = u.u_gid;
	acc_vec.af_uid = u.u_uid;
	acc_vec.af_par_proc_id = p1 -> p_pid;
	acc_vec.af_child_proc_id = p2 -> p_pid;
	acc_vec.af_date [0] = time [0];
	acc_vec.af_date [1] = time [1];
	acc_vec.af_2spare[0] =
		acc_vec.af_2spare[1] = 0;
	accinput (&acc_vec);
	/*
	 * end of accounting
	 */
#endif ACCTSYS

	u.u_ar0[R0] = p2->p_pid;
out:
	u.u_ar0[R7] =+ 2;
}

/*
 * break system call.
 *  -- bad planning: "break" is a dirty word in C.
 */
sbreak()
{
	register a, n, d;
	int i;

	/*
	 * set n to new data size
	 * set d to new-old
	 * set n to new total size
	 */

	n = (((u.u_arg[0]+63)>>6) & 01777);
	if(!u.u_sep)
		n =- nseg(u.u_tsize) * 128;
	if(n < 0)
		n = 0;
	d = n - u.u_dsize;
	n =+ USIZE+u.u_ssize;
	if(estabur(u.u_tsize, u.u_dsize+d, u.u_ssize, u.u_sep))
		return;
	u.u_dsize =+ d;
	if(d > 0)
		goto bigger;
	a = u.u_procp->p_addr + n - u.u_ssize;
	i = n;
	n = u.u_ssize;
	while(n--) {
		copyseg(a-d, a);
		a++;
	}
	expand(i);
	return;

bigger:
	expand(n);
	a = u.u_procp->p_addr + n;
	n = u.u_ssize;
	while(n--) {
		a--;
		copyseg(a-d, a);
	}
	while(d--)
		clearseg(--a);
}

#ifdef RAND
/*
 * gprocs system call (rand:bobg)
 * copies proc table into buffer supplied by user
 * returns size of proc table
 */

gprocs()
{
	register int p;

	if ((p = u.u_ar0[R0]) & 1)      /* get addr of buffer from R0 */
	{                               /* won't allow odd address */
		u.u_error = EINVAL;
		return;
	}
	if (p) copyout(&proc[0],p,sizeof(proc));
				/* if adr not 0, copyout proc tbl to user */
	u.u_ar0[R0] = NPROC;    /* return NPROC as value in any case */
}

/*
 * eofp system call (rand:bobg)
 * write an end-of-file on a pipe
 */
eofpipe()
{
	register int *p;

	p = getf(u.u_ar0[R0]);  /* get file desc from r0 */
	if (p == NULL) return;  /* couldn't */
	if (!(p->f_flag & FPIPE) || !(p->f_flag & FWRITE))
	      return(u.u_error = EBADF); /* must be the write end of a pipe */
	eofp(p->f_inode);       /* call eof code on the inode */
}
#endif RAND
