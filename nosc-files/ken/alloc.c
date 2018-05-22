#
#include "param.h"
#include "filsys.h"
#include "systm.h"
#include "conf.h"
#include "buf.h"
#include "inode.h"
#include "user.h"

/*
 * iinit is called once (from main)
 * very early in initialization.
 * It reads the root's super block
 * and initializes the current date
 * from the last modified date.
 *
 * panic: iinit -- cannot read the super
 * block. Usually because of an IO error.
 */
iinit()
{
	register *cp, *bp;

	(*bdevsw[rootdev.d_major].d_open)(rootdev, 1);
	bp = bread(rootdev, 1);
	cp = getblk(NODEV);
	if(u.u_error)
		panic("iinit");
#ifndef UCBUFMOD
	bcopy(bp->b_addr, cp->b_addr, 256);
	brelse(bp);
	mount[0].m_bufp = cp;
	mount[0].m_dev = rootdev;
	cp = cp->b_addr;
	cp->s_flock = 0;
	cp->s_ilock = 0;
	cp->s_ronly = 0;
	time[0] = cp->s_time[0];
	time[1] = cp->s_time[1];
#endif not UCBUFMOD
#ifdef UCBUFMOD
	btk(bp->b_paddr, &0->s_isize, &mount[0].m_isize, 3);
		/* fetches isize, fsize, and nfree */
	mount[0].m_ninode = fbword(bp->b_paddr, &0->s_ninode);
	f1_copy(bp, cp, mount);	/* copy free lists */
	btk(bp->b_paddr, 0->s_time, time, 2);
	brelse(bp);
	mount[0].m_bufp = cp;
	mount[0].m_dev = rootdev;
	mount[0].m_flock = 0;
	mount[0].m_ilock = 0;
	mount[0].m_ronly = 0;
#endif UCBUFMOD
}

/*
 * alloc will obtain the next available
 * free disk block from the free list of
 * the specified device.
 * The super block has up to 100 remembered
 * free blocks; the last of these is read to
 * obtain 100 more . . .
 *
 * no space on dev x/y -- when
 * the free list is exhausted.
 */
alloc(dev)
{
	int bno;
	register *bp, *ip, *fp;

	fp = getfs(dev);
#ifndef UCBUFMOD
	while(fp->s_flock)
		sleep(&fp->s_flock, PINOD);
	do {
		if(fp->s_nfree <= 0)
			goto nospace;
		bno = fp->s_free[--fp->s_nfree];
		if(bno == 0)
			goto nospace;
	} while (badblock(fp, bno, dev));
	if(fp->s_nfree <= 0) {
		fp->s_flock++;
		bp = bread(dev, bno);
		ip = bp->b_addr;
		fp->s_nfree = *ip++;
		bcopy(ip, fp->s_free, 100);
		brelse(bp);
		fp->s_flock = 0;
		wakeup(&fp->s_flock);
	}
	fp->s_fmod = 1;
#endif not UCBUFMOD
#ifdef UCBUFMOD
	while(fp->m_flock)
		sleep(&fp->m_flock, PINOD);
	do {
		if(fp->m_nfree <= 0)
			goto nospace;
		bno = fbword(fp->m_bufp->b_paddr, &0->s_free[--fp->m_nfree]);
		if(bno == 0)
			goto nospace;
	} while (badblock(fp, bno, dev));
	if(fp->m_nfree <= 0) {
		fp->m_flock++;
		bp = bread(dev, bno);
		ip = bp->b_paddr;
		fp->m_nfree = fbword(ip, 0);
		f_copy(bp, 2, fp->m_bufp, 0->s_free, fp->m_nfree);
		brelse(bp);
		fp->m_flock = 0;
		wakeup(&fp->m_flock);
	}
	fp->m_fmod = 1;
#endif UCBUFMOD
	bp = getblk(dev, bno);
	clrbuf(bp);
	return(bp);

nospace:
#ifndef UCBUFMOD
	fp->s_nfree = 0;
#endif not UCBUFMOD
#ifdef UCBUFMOD
	fp->m_nfree = 0;
#endif UCBUFMOD
	prdev("no space", dev);
	u.u_error = ENOSPC;
	return(NULL);
}

/*
 * place the specified disk block
 * back on the free list of the
 * specified device.
 */
free(dev, bno)
{
	register *fp, *bp, *ip;

	fp = getfs(dev);
#ifndef UCBUFMOD
	fp->s_fmod = 1;
	while(fp->s_flock)
		sleep(&fp->s_flock, PINOD);
	if (badblock(fp, bno, dev))
		return;
	if(fp->s_nfree <= 0) {
		fp->s_nfree = 1;
		fp->s_free[0] = 0;
	}
	if(fp->s_nfree >= 100) {
		fp->s_flock++;
		bp = getblk(dev, bno);
		ip = bp->b_addr;
		*ip++ = fp->s_nfree;
		bcopy(fp->s_free, ip, 100);
		fp->s_nfree = 0;
		bwrite(bp);
		fp->s_flock = 0;
		wakeup(&fp->s_flock);
	}
	fp->s_free[fp->s_nfree++] = bno;
	fp->s_fmod = 1;
#endif not UCBUFMOD
#ifdef UCBUFMOD
	fp->m_fmod = 1;
	while(fp->m_flock)
		sleep(&fp->m_flock, PINOD);
	if(badblock(fp, bno, dev))
		return;
	if(fp->m_nfree <= 0) {
		fp->m_nfree = 1;
		sbword(fp->m_bufp->b_paddr, &0->s_free[0], 0);
	}
	if(fp->m_nfree >= 100) {
		fp->m_flock++;
		bp = getblk(dev, bno);
		ip = bp->b_paddr;
		sbword(ip, 0, fp->m_nfree);
		f_copy(fp->m_bufp, 0->s_free, bp, 2, fp->m_nfree);
		fp->m_nfree = 0;
		bwrite(bp);
		fp->m_flock = 0;
		wakeup(&fp->m_flock);
	}
	sbword(fp->m_bufp->b_paddr, &0->s_free[fp->m_nfree++], bno);
	fp->m_fmod = 1;
#endif UCBUFMOD
}

/*
 * Check that a block number is in the
 * range between the I list and the size
 * of the device.
 * This is used mainly to check that a
 * garbage file system has not been mounted.
 *
 * bad block on dev x/y -- not in range
 */
badblock(afp, abn, dev)
{
	register struct filsys *fp;
	register char *bn;

	fp = afp;
	bn = abn;
#ifndef UCBUFMOD
	if (bn < fp->s_isize+2 || bn >= fp->s_fsize) {
#endif not UCBUFMOD
#ifdef UCBUFMOD
	if(bn < fp->m_isize+2 || bn >= fp->m_fsize) {
#endif UCBUFMOD
		prdev("bad block", dev);
		return(1);
	}
	return(0);
}

/*
 * Allocate an unused I node
 * on the specified device.
 * Used with file creation.
 * The algorithm keeps up to
 * 100 spare I nodes in the
 * super block. When this runs out,
 * a linear search through the
 * I list is instituted to pick
 * up 100 more.
 */
ialloc(dev)
{
	register *fp, *bp, *ip;
	int i, j, k, ino;

	fp = getfs(dev);
#ifndef UCBUFMOD
	while(fp->s_ilock)
		sleep(&fp->s_ilock, PINOD);
loop:
	if(fp->s_ninode > 0) {
		ino = fp->s_inode[--fp->s_ninode];
#endif not UCBUFMOD
#ifdef UCBUFMOD
	while(fp->m_ilock)
		sleep(&fp->m_ilock, PINOD);
loop:
	if(fp->m_ninode > 0) {
		ino = fbword(fp->m_bufp->b_paddr, &0->s_inode[--fp->m_ninode]);
#endif UCBUFMOD
		ip = iget(dev, ino);
		if (ip==NULL)
			return(NULL);
		if(ip->i_mode == 0) {
			for(bp = &ip->i_mode; bp < &ip->i_addr[8];)
				*bp++ = 0;
#ifndef UCBUFMOD
			fp->s_fmod = 1;
#endif not UCBUFMOD
#ifdef UCBUFMOD
			fp->m_fmod = 1;
#endif UCBUFMOD
			return(ip);
		}
		/*
		 * Inode was allocated after all.
		 * Look some more.
		 */
		iput(ip);
		goto loop;
	}
	ino = 0;
#ifndef UCBUFMOD
	fp->s_ilock++;
	for(i=0; i<fp->s_isize; i++) {
#endif not UCBUFMOD
#ifdef UCBUFMOD
	fp->m_ilock++;
	for(i=0; i<fp->m_isize; i++) {
#endif UCBUFMOD
		bp = bread(dev, i+2);
#ifndef UCBUFMOD
		ip = bp->b_addr;
		for(j=0; j<256; j=+16) {
			ino++;
			if(ip[j] != 0)
#endif not UCBUFMOD
#ifdef UCBUFMOD
		ip = bp->b_paddr;
		for(j=0; j<512; j=+32) {
			ino++;
			if(fbword(ip, j) != 0)
#endif UCBUFMOD
				continue;
			for(k=0; k<NINODE; k++)
			if(dev==inode[k].i_dev && ino==inode[k].i_number)
				goto cont;
#ifndef UCBUFMOD
			fp->s_inode[fp->s_ninode++] = ino;
			if(fp->s_ninode >= 100)
#endif not UCBUFMOD
#ifdef UCBUFMOD
			sbword(fp->m_bufp->b_paddr, &0->s_inode[fp->m_ninode++], ino);
			if(fp->m_ninode >= 100)
#endif UCBUFMOD
				break;
		cont:;
		}
		brelse(bp);
#ifndef UCBUFMOD
		if(fp->s_ninode >= 100)
#endif not UCBUFMOD
#ifdef UCBUFMOD
		if(fp->m_ninode >= 100)
#endif UCBUFMOD
			break;
	}
#ifndef UCBUFMOD
	fp->s_ilock = 0;
	wakeup(&fp->s_ilock);
	if (fp->s_ninode > 0)
#endif not UCBUFMOD
#ifdef UCBUFMOD
	fp->m_ilock = 0;
	wakeup(&fp->m_ilock);
	if(fp->m_ninode > 0)
#endif UCBUFMOD
		goto loop;
	prdev("Out of inodes", dev);
	u.u_error = ENOSPC;
	return(NULL);
}

/*
 * Free the specified I node
 * on the specified device.
 * The algorithm stores up
 * to 100 I nodes in the super
 * block and throws away any more.
 */
ifree(dev, ino)
{
	register *fp;

	fp = getfs(dev);
#ifndef UCBUFMOD
	if(fp->s_ilock)
		return;
	if(fp->s_ninode >= 100)
		return;
	fp->s_inode[fp->s_ninode++] = ino;
	fp->s_fmod = 1;
#endif not UCBUFMOD
#ifdef UCBUFMOD
	if(fp->m_ilock)
		return;
	if(fp->m_ninode >= 100)
		return;
	sbword(fp->m_bufp->b_paddr, &0->s_inode[fp->m_ninode++], ino);
	fp->m_fmod = 1;
#endif UCBUFMOD
}

/*
 * getfs maps a device number into
 * a pointer to the incore super
 * block.
 * The algorithm is a linear
 * search through the mount table.
 * A consistency check of the
 * in core free-block and i-node
 * counts.
 *
 * bad count on dev x/y -- the count
 *	check failed. At this point, all
 *	the counts are zeroed which will
 *	almost certainly lead to "no space"
 *	diagnostic
 * panic: no fs -- the device is not mounted.
 *	this "cannot happen"
 */
getfs(dev)
{
	register struct mount *p;
	register char *n1, *n2;

	for(p = &mount[0]; p < &mount[NMOUNT]; p++)
	if(p->m_bufp != NULL && p->m_dev == dev) {
#ifndef UCBUFMOD
		p = p->m_bufp->b_addr;
		n1 = p->s_nfree;
		n2 = p->s_ninode;
		if(n1 > 100 || n2 > 100) {
			prdev("bad count", dev);
			p->s_nfree = 0;
			p->s_ninode = 0;
#endif not UCBUFMOD
#ifdef UCBUFMOD
		n1 = p->m_nfree;
		n2 = p->m_ninode;
		if(n1 > 100 || n2 > 100) {
			prdev("bad count", dev);
			p->m_nfree = 0;
			p->m_ninode = 0;
#endif UCBUFMOD
		}
		return(p);
	}
	panic("no fs");
}

/*
 * update is the internal name of
 * 'sync'. It goes through the disk
 * queues to initiate sandbagged IO;
 * goes through the I nodes to write
 * modified nodes; and it goes through
 * the mount table to initiate modified
 * super blocks.
 */
update()
{
	register struct inode *ip;
	register struct mount *mp;
	register *bp;

	if(updlock)
		return;
	updlock++;
	for(mp = &mount[0]; mp < &mount[NMOUNT]; mp++)
		if(mp->m_bufp != NULL) {
#ifndef UCBUFMOD
			ip = mp->m_bufp->b_addr;
			if(ip->s_fmod==0 || ip->s_ilock!=0 ||
			   ip->s_flock!=0 || ip->s_ronly!=0)
				continue;
			bp = getblk(mp->m_dev, 1);
			ip->s_fmod = 0;
			ip->s_time[0] = time[0];
			ip->s_time[1] = time[1];
			bcopy(ip, bp->b_addr, 256);
#endif not UCBUFMOD
#ifdef UCBUFMOD
			if(mp->m_fmod==0 || mp->m_ilock!=0 ||
			   mp->m_flock!=0 || mp->m_ronly!=0)
				continue;
			bp = getblk(mp->m_dev, 1);
			ip = mp->m_bufp;
			mp->m_fmod = 0;
			ktb(bp->b_paddr, 0->s_time, time, 2);
			ktb(bp->b_paddr, &0->s_isize, &mp->m_isize, 3);
				/* stores isize, fsize, and nfree */
			sbword(bp->b_paddr, &0->s_ninode, mp->m_ninode);
			f1_copy(ip, bp, mp);	/* copy free lists */
#endif UCBUFMOD
			bwrite(bp);
		}
	for(ip = &inode[0]; ip < &inode[NINODE]; ip++)
		if((ip->i_flag&ILOCK) == 0) {
			ip->i_flag =| ILOCK;
			iupdat(ip, time);
			prele(ip);
		}
	updlock = 0;
	bflush(NODEV);
}

#ifdef UCBUFMOD
/*
 * f1_copy copies the free lists from one buffer
 * to another using f_copy.
 */
f1_copy(from, to, mnt)
struct buf *from, *to;
struct mount *mnt;
{
	register struct mount *rmnt;

	rmnt = mnt;
	f_copy(from, 0->s_free, to, 0->s_free, rmnt->m_nfree);
	f_copy(from, 0->s_inode, to, 0->s_inode, rmnt->m_ninode);
}
/*
 * f_copy copies a free list from one
 * buffer to another buffer through
 * a 100 word temporary area in the
 * kernel
 */
int	supfree[100];		/* temporary for free lists */

f_copy(f_buf, f_off, t_buf, t_off, count)
{
	btk(f_buf->b_paddr, f_off, supfree, count);
	ktb(t_buf->b_paddr, t_off, supfree, count);
}
#endif UCBUFMOD
