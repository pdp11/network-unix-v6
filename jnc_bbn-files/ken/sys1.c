#
/*
 */

#include "../h/param.h"
#include "../h/systm.h"
#include "../h/user.h"
#include "../h/proc.h"
#include "../h/buf.h"
#include "../h/reg.h"
#include "../h/inode.h"
#include "../h/file.h"    /* rand:bobg for eofp call */

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
	int ts, ds, sep, ss;
	int overlay, entloc;
	int a;		/* temporary address for malloc */
	struct proc *pptr;
	register c, *ip;
	register char *cp;
	extern uchar;

	/*
	 * pick up file names
	 * and check various modes
	 * for execute permission
	 */

	ip = namei(&uchar, 0);
	if(ip == NULL)
		return;
	while(execnt >= NEXEC)
		sleep(&execnt, EXPRI);
	execnt++;
	bp = getblk(NODEV);
	if(access(ip, IEXEC))
		goto bad;
	if((ip->i_mode & IFMT) != 0 ||
	   (ip->i_mode & (IEXEC|(IEXEC>>3)|(IEXEC>>6))) == 0) {
		u.u_error = EACCES;
		goto bad;
	}

	/*
	 * pack up arguments into
	 * allocated disk buffer
	 */

	cp = bp->b_addr;
	na = 0;
	nc = 0;
	if (u.u_arg[1]) {		/* BBN:cdh from Bell diff list */
		while(ap = fuword(u.u_arg[1])) {
			na++;
			if(ap == -1)
				goto bad;
			u.u_arg[1] =+ 2;
			for(;;) {
				c = fubyte(ap++);
				if(c == -1)
					goto bad;
				*cp++ = c;
				nc++;
				if(nc > 510) {
					u.u_error = E2BIG;
					goto bad;
				}
				if(c == 0)
					break;
			}
		}
	}
	if((nc&1) != 0) {
		*cp++ = 0;
		nc++;
	}

	/*
	 * read in first 8 bytes
	 * of file for segment
	 * sizes:
	 * w0 = 407/410/411 (410 implies RO text) (411 implies sep ID)
	 * (405 implies overlay).
	 * w1 = text size
	 * w2 = data size
	 * w3 = bss size
	 */

	/*
	 * Read in entry location for later use.
	 * BBN: cdh
	 */
	u.u_base = &u.u_arg[0];
	u.u_count = 2;
	u.u_offset[1] = 10;
	u.u_offset[0] = 0;
	u.u_segflg = 1;
	readi(ip);
	u.u_segflg = 0;
	if (u.u_error)
		goto bad;
	entloc = u.u_arg[0];

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
	overlay = 0;
	if(u.u_arg[0] == 0407) {
		u.u_arg[2] =+ u.u_arg[1];
		u.u_arg[1] = 0;
	} else
	if(u.u_arg[0] == 0411)
		sep++; else
	if(u.u_arg[0] == 0405)
		overlay++; else
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
	ss = SSIZE;
	if (overlay) {		/* BBN:cdh implementation of 405 overlays */
		if (u.u_sep==0 && ((ts+127)>>7) != ((u.u_tsize+127)>>7)) {
			u.u_error = ENOMEM;
			goto bad;
		}
		pptr = u.u_procp;
		c = pptr->p_size;	/* save swappable size in clicks */
		ds = u.u_dsize;
		ss = u.u_ssize;
		sep = u.u_sep;
		/*
		 * The strategy here is the following:
		 *   1) Allocate swap space.
		 *   2) Swap data space out.
		 *   3) Get new text segment.
		 *   4) Reallocate data space.
		 *   5) Read in data space from swap.
		 *   6) Free swap space.
		 */
		a = malloc(swapmap, (c-USIZE+7)/8);	/* allocate swap area */
		if (a==NULL) {
			printf("No room for overlay swap; killed proc\n");
			psignal(pptr, SIGKIL);
			return;
		}
		/*
		 * swap data space out.
		 */
		pptr->p_flag =| SLOCK;
		if (swap(a,pptr->p_addr+USIZE,c-USIZE,0)) {
			pptr->p_flag =& ~SLOCK;
			printf("Swap write error on overlay, killed proc\n");
			mfree(swapmap,(c-USIZE+7)/8,a);
			psignal(pptr,SIGKIL);
			return;
		}
		pptr->p_flag =& ~SLOCK;
		/*
		 * get new text segment.
		 */
		expand(USIZE);
		xfree();
		xalloc(ip);
		expand(c);
		/*
		 * read data space back from swap.
		 */
		pptr->p_flag =| SLOCK;
		if (swap(a,pptr->p_addr+USIZE,c-USIZE,1)) {
			pptr->p_flag =& ~SLOCK;
			printf("Swap read error on overlay, killed proc\n");
			mfree(swapmap,(c-USIZE+7)/8,a);
			psignal(pptr,SIGKIL);
			return;
		}
		pptr->p_flag =& ~SLOCK;
		/*
		 * free swap space allocated.
		 */
		mfree(swapmap,(c-USIZE+7)/8,a);
		u.u_ar0[R7] = entloc & ~01;	/* make sure even */
	}
	else {
		if(estabur(ts, ds, SSIZE, sep))
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
		c = USIZE+ds+SSIZE;
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
	}

	/*
	 * initialize stack segment
	 */

	u.u_tsize = ts;
	u.u_dsize = ds;
	u.u_ssize = ss;
	u.u_sep = sep;
	estabur(u.u_tsize, u.u_dsize, u.u_ssize, u.u_sep);

	if (overlay) goto bad;  /* don't want to do any of this stuff */

	cp = bp->b_addr;
	ap = -nc - na*2 - 4;
	u.u_ar0[R6] = ap;
	suword(ap, na);
	c = -nc;
	while(na--) {
		suword(ap=+2, c);
		do
			subyte(c++, *cp);
		while(*cp++);
	}
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
	u.u_ar0[R7] = entloc & ~01;	/* make sure even BBN:cdh */
	for(ip = &u.u_fsav[0]; ip < &u.u_fsav[25];)
		*ip++ = 0;
	ip = c;

bad:
	iput(ip);
#ifdef BUFMOD
	brelse(bp,&bfreelist);
#endif BUFMOD
#ifndef BUFMOD
	brelse(bp);
#endif BUFMOD
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
 *
 * Modified 1/78 (BBN:jfh) to call awtdis to clear process' entries
 *
 */
exit()
{
	register int *q, a;
	register struct proc *p;

#ifdef  LCBA
	rel_lcba();
#endif
	p = u.u_procp;
	p->p_flag =& ~STRC;
	p->p_clktim = 0;
	for(q = &u.u_signal[0]; q < &u.u_signal[NSIG];)
		*q++ = 1;
	awtdis(-1);             /* clear any enabled file descriptors */
	for(q = &u.u_ofile[0]; q < &u.u_ofile[NOFILE]; q++)
		if(a = *q) {
			*q = NULL;
			closef(a);
		}
	plock(u.u_cdir);        /* BBN:cdh improved locking */
	iput(u.u_cdir);
	xfree();
	a = malloc(swapmap, 1);
	if(a == NULL)
		panic("out of swap");
	p = getblk(swapdev, a);
	bcopy(&u, p->b_addr, 256);
	bwrite(p);
	q = u.u_procp;
	mfree(coremap, q->p_size, q->p_addr);
	q->p_addr = a;
	q->p_stat = SZOMB;

loop:
	for(p = &proc[0]; p <= MAXPROC; p++)
	if(q->p_ppid == p->p_pid) {
		wakeup(&proc[1]);
		wakeup(p);
		for(p = &proc[0]; p <= MAXPROC; p++)
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
{       wait1(0);
}

/*
 * Waitr system call.
 * Same as wait but returns accounting information.
 */

waitr()
{       wait1(1);
}

wait1(flag)
int flag;
{
	register f, *bp;
	register struct proc *p;
	struct proc *mproc;
	int *pp;

	f = 0;

loop:
	for(p = &proc[0]; p <= MAXPROC; p++)
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
			/* enter process back into the free list */
			cqenter( &Free_proc,p );
			for( mproc = &proc[NPROC-1]; mproc >= &proc[0]; mproc-- )
				if( mproc->p_stat )
				{
					MAXPROC = mproc;
					break;
				}
			p = bp->b_addr;
			u.u_cstime[0] =+ p->u_cstime[0];
			dpadd(u.u_cstime, p->u_cstime[1]);
			u.u_cstime[0] =+ p->u_stime[0];
			dpadd(u.u_cstime, p->u_stime[1]);
			u.u_cutime[0] =+ p->u_cutime[0];
			dpadd(u.u_cutime, p->u_cutime[1]);
			u.u_cutime[0] =+ p->u_utime[0];
			dpadd(u.u_cutime, p->u_utime[1]);
			u.u_tio[0] =+ p->u_tio[0];
			dpadd(u.u_tio, p->u_tio[1]);
			u.u_mem1[0] =+ p->u_mem1[0];
			dpadd(u.u_mem1, p->u_mem1[1]);
			u.u_mem2[0] =+ p->u_mem2[0];
			dpadd(u.u_mem2, p->u_mem2[1]);
			u.u_io[0] =+ p->u_io[0];
			dpadd(u.u_io, p->u_io[1]);
			u.u_ar0[R1] = p->u_arg[0];
			if (flag)       /* if waitr, return statistics */
			{       suword(u.u_arg[0],p->u_ruid);
				u.u_arg[0] =+ 2;
				for(pp = &p->u_utime[0]; pp <= &p->u_io[1];)
				{       suword(u.u_arg[0], *pp++);
					u.u_arg[0] =+ 2;
				}
			}
#ifdef BUFMOD
			brelse(bp,&bfreelist);
#endif BUFMOD
#ifndef BUFMOD
			brelse(bp);
#endif BUFMOD
			return;
		}
		if(p->p_stat == SSTOP) {
			if((p->p_flag&SWTED) == 0) {
				p->p_flag =| SWTED;
				u.u_ar0[R0] = p->p_pid;
				u.u_ar0[R1] = (p->p_sig<<8) | 0177;
				return;
			}

			/*
			 * This change is to avoid finding
			 * ptrace'd procs that are already
			 * stopped when doing the wait.
			 * BBN:cdh 1 May 79
			 */

			f--;	/* Seen stopped proc's don't count */
		}
	}
	if(f) {
		sleep(u.u_procp, PWAIT);
		goto loop;
	}
	u.u_error = ECHILD;
}

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
{
	register struct proc *p1, *p2;
	register int *pp;

	p1 = u.u_procp;
	/* is queue empty */
	if( Free_proc.qhead )
	{
		p2 = (Free_proc.qhead)->qlink;	/* get first in queue */
		goto found;
	}
	u.u_error = EAGAIN;
	goto out;

found:
	if(newproc()) {
		u.u_ar0[R0] = p1->p_pid;
		for(pp = &u.u_utime[0]; pp <= &u.u_io[1];) *pp++ = 0;
		u.u_tim1[0] = time[0]; /* set beginning time of process */
		u.u_tim1[1] = time[1];
		if (sflag) u.u_procp->p_flag =| SNOSIG;
			  /* rand:bobg--set NOSIG flag in child if an sfork */
		return;
	}
	u.u_ar0[R0] = p2->p_pid;

out:
	u.u_ar0[R7] =+ 2;
}
/*
 * ztime system call.
 *   kludge stuck in for accounting -
 *   zeroes accounting information fields and reset process beginning time;
 *     used by login program
 */
ztime()
{
	register int *pp;

	if(suser()) {
		for(pp = &u.u_utime[0]; pp <= &u.u_io[1];) *pp++ = 0;
		u.u_tim1[0] = time[0];
		u.u_tim1[1] = time[1];
	}
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


