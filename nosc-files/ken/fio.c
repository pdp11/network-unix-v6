#
#include "param.h"
#include "user.h"
#include "filsys.h"
#include "file.h"
#include "conf.h"
#include "inode.h"
#include "reg.h"
#ifdef UCBUFMOD
#include "systm.h"	/* to get mount struct */
#endif UCBUFMOD

/*
 * Convert a user supplied
 * file descriptor into a pointer
 * to a file structure.
 * Only task is to check range
 * of the descriptor.
 */
getf(f)
{
	register *fp, rf;

	rf = f;
	if(rf<0 || rf>=NOFILE)
		goto bad;
	fp = u.u_ofile[rf];
	if(fp != NULL)
		return(fp);
bad:
	u.u_error = EBADF;
	return(NULL);
}

/*
 * Internal form of close.
 * Decrement reference count on
 * file structure and call closei
 * on last closef.
 * Also make sure the pipe protocol
 * does not constipate.
 */
closef(fp)
int *fp;
{
	register *rfp, *ip;

	rfp = fp;
#ifdef	NETSYS
	if( rfp->f_flag & FNET )
	{
		netclose( rfp );
		return;
	}
#endif	NETSYS
	if(rfp->f_flag&FPIPE) {
		ip = rfp->f_inode;
		ip->i_mode =& ~(IREAD|IWRITE);
		wakeup(ip+1);
		wakeup(ip+2);
#ifdef RAND
		wakeup(ip+3);		/* rand:bobg--in case of eof's */
#endif RAND
	}
	if(rfp->f_count <= 1)
		closei(rfp->f_inode, rfp->f_flag&FWRITE);
	rfp->f_count--;
}

/*
 * Decrement reference count on an
 * inode due to the removal of a
 * referencing file structure.
 * On the last closei, switchout
 * to the close entry point of special
 * device handler.
 * Note that the handler gets called
 * on every open and only on the last
 * close.
 */
closei(ip, rw)
int *ip;
{
	register *rip;
	register dev, maj;

	rip = ip;
	dev = rip->i_addr[0];
	maj = rip->i_addr[0].d_major;
	if(rip->i_count <= 1)
	switch(rip->i_mode&IFMT) {

	case IFCHR:
		(*cdevsw[maj].d_close)(dev, rw);
		break;

	case IFBLK:
		(*bdevsw[maj].d_close)(dev, rw);
	}
	iput(rip);
}

/*
 * openi called to allow handler
 * of special files to initialize and
 * validate before actual IO.
 * Called on all sorts of opens
 * and also on mount.
 */
openi(ip, rw)
int *ip;
{
	register *rip;
	register dev, maj;

	rip = ip;
	dev = rip->i_addr[0];
	maj = rip->i_addr[0].d_major;
	switch(rip->i_mode&IFMT) {

	case IFCHR:
		if(maj >= nchrdev)
			goto bad;
		(*cdevsw[maj].d_open)(dev, rw);
		break;

	case IFBLK:
		if(maj >= nblkdev)
			goto bad;
		(*bdevsw[maj].d_open)(dev, rw);
	}
	return;

bad:
	u.u_error = ENXIO;
}

/*
 * Check mode permission on inode pointer.
 * Mode is READ, WRITE or EXEC.
 * In the case of WRITE, the
 * read-only status of the file
 * system is checked.
 * Also in WRITE, prototype text
 * segments cannot be written.
 * The mode is shifted to select
 * the owner/group/other fields.
 * The super user is granted all
 * permissions except for EXEC where
 * at least one of the EXEC bits must
 * be on.  Permissions are inherited
 * to the left.
 */
access(aip, mode)
int *aip;
{
	register *ip, m;
	register mm;

	ip = aip;
	m = mode;
	if(m == IWRITE) {
#ifndef UCBUFMOD
		if(getfs(ip->i_dev)->s_ronly != 0) {
#endif not UCBUFMOD
#ifdef UCBUFMOD
		if(getfs(ip->i_dev)->m_ronly != 0) {
#endif UCBUFMOD
			u.u_error = EROFS;
			return(1);
		}
		if(ip->i_flag & ITEXT) {
			u.u_error = ETXTBSY;
			return(1);
		}
	}
	if(u.u_uid == 0) {
		if(m != IEXEC) return(0);
	} else
	if(u.u_uid != ip->i_uid) {
		m =>> 3;
		if(u.u_gid != ip->i_gid)
			m =>> 3;
	}
	mm = ip->i_mode; mm =| ((mm<<3)|mm)<<3;
	if(mm&m)
		return(0);

	u.u_error = EACCES;
	return(1);
}

/*
 * Look up a pathname and test if
 * the resultant inode is owned by the
 * current user.
 * If not, try for super-user.
 * If permission is granted,
 * return inode pointer.
 */
owner()
{
	register struct inode *ip;
	extern uchar();

	if ((ip = namei(uchar, 0)) == NULL)
		return(NULL);
	if(u.u_uid == ip->i_uid)
		return(ip);
	if (suser())
		return(ip);
	iput(ip);
	return(NULL);
}

/*
 * Test if the current user is the
 * super user.
 */
suser()
{

	if(u.u_uid == 0)
		return(1);
	u.u_error = EPERM;
	return(0);
}

/*
 * Allocate a user file descriptor.
 */
ufalloc()
{
	register i;

	for (i=0; i<NOFILE; i++)
		if (u.u_ofile[i] == NULL) {
			u.u_ar0[R0] = i;
			return(i);
		}
	u.u_error = EMFILE;
	return(-1);
}

/*
 * Allocate a user file descriptor
 * and a file structure.
 * Initialize the descriptor
 * to point at the file structure.
 *
 * no file -- if there are no available
 * 	file structures.
 */
falloc()
{
	register struct file *fp;
	register i;

	if ((i = ufalloc()) < 0)
		return(NULL);
	for (fp = &file[0]; fp < &file[NFILE]; fp++)
		if (fp->f_count==0) {
			u.u_ofile[i] = fp;
			fp->f_count++;
			fp->f_offset[0] = 0;
			fp->f_offset[1] = 0;
			return(fp);
		}
	printf("no file\n");
	u.u_error = ENFILE;
	return(NULL);
}
