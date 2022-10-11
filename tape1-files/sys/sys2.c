/*	sys2.c	4.7	81/04/13	*/

#include "../h/param.h"
#include "../h/systm.h"
#include "../h/dir.h"
#include "../h/user.h"
#include "../h/reg.h"
#include "../h/file.h"
#include "../h/inode.h"
#include "../h/pte.h"
#include "../h/vm.h"
#include "../h/buf.h"
#include "../h/mtpr.h"
#include "../h/proc.h"
#include "../h/inline.h"

/*
 * READ AND WRITE ARE NEAR DUPLICATES OF EACH OTHER TO SAVE
 * AS MUCH TIME AS POSSIBLE.
 */

/*
 * read system call
 */
read()
{
	register struct file *fp;
	register struct inode *ip;
	register struct a {
		int	fdes;
		char	*cbuf;
		unsigned count;
	} *uap;

	uap = (struct a *)u.u_ap;
	if ((int)uap->count < 0) {
		u.u_error = EINVAL;
		return;
	}
	GETF(fp, uap->fdes);
	if((fp->f_flag&FREAD) == 0) {
		u.u_error = EBADF;
		return;
	}
	u.u_base = (caddr_t)uap->cbuf;
	u.u_count = uap->count;
	u.u_segflg = 0;
	if ((u.u_procp->p_flag&SNUSIG) && setjmp(u.u_qsav)) {
		if (u.u_count == uap->count)
			u.u_eosys = RESTARTSYS;
	} else if((fp->f_flag&FPIPE) != 0)
		readp(fp);
#ifdef BBNNET
	else if (fp->f_flag & FNET)
		netread(fp->f_un.f_ucb);
#endif BBNNET
	else {
		ip = fp->f_inode;
		if (fp->f_flag&FMP)
			u.u_offset = 0;
		else
			u.u_offset = fp->f_un.f_offset;
		if((ip->i_mode&(IFCHR&IFBLK)) == 0)
			plock(ip);
		readi(ip);
		if((ip->i_mode&(IFCHR&IFBLK)) == 0)
			prele(ip);
		if ((fp->f_flag&FMP) == 0)
			fp->f_un.f_offset += uap->count-u.u_count;
	}
	u.u_r.r_val1 = uap->count-u.u_count;
}

/*
 * write system call
 */
write()
{
	register struct file *fp;
	register struct inode *ip;
	register struct a {
		int	fdes;
		char	*cbuf;
		unsigned count;
	} *uap;

	uap = (struct a *)u.u_ap;
	if ((int)uap->count < 0) {
		u.u_error = EINVAL;
		return;
	}
	GETF(fp, uap->fdes);
	if((fp->f_flag&FWRITE) == 0) {
		u.u_error = EBADF;
		return;
	}
	u.u_base = (caddr_t)uap->cbuf;
	u.u_count = uap->count;
	u.u_segflg = 0;
	if ((u.u_procp->p_flag&SNUSIG) && setjmp(u.u_qsav)) {
		if (u.u_count == uap->count)
			u.u_eosys = RESTARTSYS;
	} else if((fp->f_flag&FPIPE) != 0)
		writep(fp);
#ifdef BBNNET
	else if (fp->f_flag & FNET)
		netwrite(fp->f_un.f_ucb);
#endif BBNNET
	else {
		ip = fp->f_inode;
		if (fp->f_flag&FMP)
			u.u_offset = 0;
		else
			u.u_offset = fp->f_un.f_offset;
		if((ip->i_mode&(IFCHR&IFBLK)) == 0)
			plock(ip);
		writei(ip);
		if((ip->i_mode&(IFCHR&IFBLK)) == 0)
			prele(ip);
		if ((fp->f_flag&FMP) == 0)
			fp->f_un.f_offset += uap->count-u.u_count;
	}
	u.u_r.r_val1 = uap->count-u.u_count;
}

/*
 * common code for virtual read/write calls
 * check permissions, set base, count, and offset,
 * and switch out to readi, writei, or pipe code.
 */
vrdwr(mode)
register mode;
{
	register struct file *fp;
	register struct inode *ip;
	register struct pte *pte;
	int npf, n;
	register struct a {
		int	fdes;
		char	*cbuf;
		unsigned count;
	} *uap;

	uap = (struct a *)u.u_ap;
	if ((int)uap->count < 0) {
		u.u_error = EINVAL;
		return;
	}
	fp = getf(uap->fdes);
	if(fp == NULL)
		return;
	if((fp->f_flag&mode) == 0) {
		u.u_error = EBADF;
		return;
	}
	u.u_base = (caddr_t)uap->cbuf;
	u.u_count = uap->count;
	u.u_segflg = 0;
	if ((u.u_procp->p_flag&SNUSIG) && setjmp(u.u_qsav)) {
		if (u.u_count == uap->count)
			u.u_eosys = RESTARTSYS;
#ifdef BBNNET
	} else if (fp->f_flag & FNET) {
		if (mode == FREAD)
        		netread(fp->f_un.f_ucb);
		else
			netwrite(fp->f_un.f_ucb);
#endif BBNNET
	} else if((fp->f_flag&FPIPE) != 0) {
		if(mode == FREAD)
			readp(fp);
		else
			writep(fp);
	} else {
		ip = fp->f_inode;
		if (fp->f_flag&FMP)
			u.u_offset = 0;
		else
			u.u_offset = fp->f_un.f_offset;
		if((ip->i_mode&IFMT)==IFREG) {
			if (((int)u.u_base & CLOFSET) || (fp->f_un.f_offset & CLOFSET)) {
				u.u_error = EINVAL;
				return;
			}
			if (!useracc(u.u_base, u.u_count, B_WRITE)) {
				u.u_error = EFAULT;
				return;
			}
		}
		if((ip->i_mode&(IFCHR&IFBLK)) == 0)
			plock(ip);
		if((ip->i_mode&IFMT)==IFREG) {
			pte = vtopte(u.u_procp, btop(u.u_base));
			npf = clbase(btop((caddr_t)u.u_count));
			if (mode == FWRITE) {
				for (n = npf; n > 0; n -= CLSIZE) {
					if (pte->pg_fod==0 &&
					    (anycl(pte,pg_vreadm) || anycl(pte, pg_m))) {
						u.u_count = NBPG * CLSIZE;
						pte->pg_vreadm = 0;
						if (anycl(pte, pg_m)) {
							pte->pg_swapm = 1;
							pte->pg_m = 0;
						}
						distcl(pte);
						tbiscl(btop(u.u_base));
						writei(ip);
					} else {
						u.u_base += NBPG * CLSIZE;
						u.u_offset += NBPG * CLSIZE;
					}
					pte += CLSIZE;
				}
				u.u_count = uap->count - npf * NBPG;
			} else if (mode == FREAD && u.u_offset < ip->i_size) {
				npf = clbase(min((unsigned)npf, btop(ip->i_size - u.u_offset)));
				u.u_procp->p_rssize -= vmemfree(pte, npf);
				if (u.u_vrpages[uap->fdes] == 0)
					ip->i_vfdcnt++;
				vinifod((struct fpte *)pte, uap->fdes, ip,
				    u.u_offset >> BSHIFT, npf);
				if (u.u_vrpages[uap->fdes] == 0)
					ip->i_vfdcnt--;
				n = npf * NBPG;
				u.u_base += n;
				u.u_offset += n;
				u.u_count -= n;
			}
		}
		if(mode == FREAD)
			readi(ip);
		else
			writei(ip);
		if((ip->i_mode&(IFCHR&IFBLK)) == 0)
			prele(ip);
		if ((fp->f_flag&FMP) == 0)
			fp->f_un.f_offset += uap->count-u.u_count;
	}
	u.u_r.r_val1 = uap->count-u.u_count;
}

/*
 * open system call
 */
open()
{
	register struct inode *ip;
	register struct a {
		char	*fname;
		int	rwmode;
	} *uap;

	uap = (struct a *)u.u_ap;
	ip = namei(uchar, 0);
	if(ip == NULL)
		return;
	open1(ip, ++uap->rwmode, 0);
}

/*
 * creat system call
 */
creat()
{
	register struct inode *ip;
	register struct a {
		char	*fname;
		int	fmode;
	} *uap;

	uap = (struct a *)u.u_ap;
	ip = namei(uchar, 1);
	if(ip == NULL) {
		if(u.u_error)
			return;
		ip = maknode(uap->fmode&07777&(~ISVTX));
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
register struct inode *ip;
register mode;
{
	register struct file *fp;
	int i;

	if(trf != 2) {
		if(mode&FREAD)
			(void) access(ip, IREAD);
		if(mode&FWRITE) {
			(void) access(ip, IWRITE);
			if((ip->i_mode&IFMT) == IFDIR)
				u.u_error = EISDIR;
		}
	}
	if(trf==1&&ip->i_vfdcnt)
		u.u_error = ETXTBSY;
	if(u.u_error)
		goto out;
#ifdef BBNNET
	if (((ip->i_mode & IFMT) == IFCHR) && 
             ((major((dev_t)ip->i_un.i_addr[0])) & 0200)) {
		if (trf != 0)
			u.u_error = EACCES;
		else   
                        netopen(ip, (char *)(mode-1));
		return;
	}    
#endif BBNNET
	if(trf == 1)
		itrunc(ip);
	prele(ip);
	if ((fp = falloc()) == NULL)
		goto out;
	fp->f_flag = mode&(FREAD|FWRITE);
	fp->f_inode = ip;
	i = u.u_r.r_val1;
	openi(ip, mode&(FREAD|FWRITE));
	if(u.u_error == 0)
		return;
	u.u_ofile[i] = NULL;
	fp->f_count--;

out:
	iput(ip);
}

/*
 * close system call
 */
close()
{
	register struct file *fp;
	register struct a {
		int	fdes;
	} *uap;

	uap = (struct a *)u.u_ap;
	fp = getf(uap->fdes);
	if(fp == NULL)
		return;
	if (u.u_vrpages[uap->fdes]) {
		u.u_error = ETXTBSY;
		return;
	}
	u.u_ofile[uap->fdes] = NULL;
	closef(fp);
}

/*
 * seek system call
 */
seek()
{
	register struct file *fp;
	register struct a {
		int	fdes;
		off_t	off;
		int	sbase;
	} *uap;

	uap = (struct a *)u.u_ap;
	fp = getf(uap->fdes);
	if(fp == NULL)
		return;
	if(fp->f_flag&(FPIPE|FMP|FNET)) {
		u.u_error = ESPIPE;
		return;
	}
	if(uap->sbase == 1)
		uap->off += fp->f_un.f_offset;
	else if(uap->sbase == 2)
		uap->off += fp->f_inode->i_size;
	fp->f_un.f_offset = uap->off;
	u.u_r.r_off = uap->off;
}


/*
 * link system call
 */
link()
{
	register struct inode *ip, *xp;
	register struct a {
		char	*target;
		char	*linkname;
	} *uap;

	uap = (struct a *)u.u_ap;
	ip = namei(uchar, 0);
	if(ip == NULL)
		return;
	if((ip->i_mode&IFMT)==IFDIR && !suser())
		goto out1;
	ip->i_nlink++;
	ip->i_flag |= ICHG;
	iupdat(ip, &time, &time, 1);
	prele(ip);
	u.u_dirp = (caddr_t)uap->linkname;
	xp = namei(uchar, 1);
	if(xp != NULL) {
		u.u_error = EEXIST;
		iput(xp);
		goto out;
	}
	if (u.u_error)
		goto out;
	if(u.u_pdir->i_dev != ip->i_dev) {
		iput(u.u_pdir);
		u.u_error = EXDEV;
		goto out;
	}
	wdir(ip);
out:
	if (u.u_error) {
		ip->i_nlink--;
		ip->i_flag |= ICHG;
	}
out1:
	iput(ip);
}

/*
 * mknod system call
 */
mknod()
{
	register struct inode *ip;
	register struct a {
		char	*fname;
		int	fmode;
		int	dev;
	} *uap;

	uap = (struct a *)u.u_ap;
	if(suser()) {
		ip = namei(uchar, 1);
		if(ip != NULL) {
			u.u_error = EEXIST;
			goto out;
		}
	}
	if(u.u_error)
		return;
	ip = maknode(uap->fmode);
	if (ip == NULL)
		return;
	if (uap->dev) {
		/*
		 * Want to be able to use this to make badblock
		 * inodes, so don't truncate the dev number.
		 */
		ip->i_un.i_rdev = uap->dev;
		ip->i_flag |= IACC|IUPD|ICHG;
	}

out:
	iput(ip);
}

/*
 * access system call
 */
saccess()
{
	register svuid, svgid;
	register struct inode *ip;
	register struct a {
		char	*fname;
		int	fmode;
	} *uap;

	uap = (struct a *)u.u_ap;
	svuid = u.u_uid;
	svgid = u.u_gid;
	u.u_uid = u.u_ruid;
	u.u_gid = u.u_rgid;
	ip = namei(uchar, 0);
	if (ip != NULL) {
		if (uap->fmode&(IREAD>>6))
			(void) access(ip, IREAD);
		if (uap->fmode&(IWRITE>>6))
			(void) access(ip, IWRITE);
		if (uap->fmode&(IEXEC>>6))
			(void) access(ip, IEXEC);
		iput(ip);
	}
	u.u_uid = svuid;
	u.u_gid = svgid;
}
