#
#include "param.h"
#include "inode.h"
#include "user.h"
#include "acct.h"
#include "proc.h"

#ifdef ACCTSYS

int acc_lock;		/* lock for exclusive use */
char	accsvuid;

char	accbuf	[512];
char	*accbufp accbuf;
char	acc_vec [ACCVLEN]; /* for system users to build in */

accinput (vecaddr)	/* internal form */
char	*vecaddr;
{
	register int n;
	register char *vptr, *bptr;

	acc_lock++;
	while (acc_lock > 1)
		sleep( &acc_lock, -1 );
	bptr = accbufp;
	vptr = vecaddr;
	for (n=ACCVLEN; n--; *bptr++ = *vptr++);
	accbufp = bptr;
	accwrite ();
	if (--acc_lock) signal ( &acc_lock );
}

accput ()	/* called via sys accput;addr;passwd */
{
	register int n, t;
	register char *cp;

	acc_lock++;
	while (acc_lock > 1)
		sleep( &acc_lock, -1 );
	u.u_error = 0;
	if (u.u_arg[1] != ACCPSWD) {
		u.u_error = EACCT;
		if (--acc_lock) signal ( &acc_lock );
		return;
	}
	cp = accbufp;
	u.u_base = u.u_arg[0];
	u.u_count = n = ACCVLEN;
	u.u_segflg = 0;	/* data from user space */
	do {
		if (( t = cpass () ) < 0) {
			printf ("ACC cpass error \n");
		}
		*cp++ = t;
	} while --n;
	accbufp = cp;
	accwrite ();
	if (--acc_lock) signal ( &acc_lock );
}


accwrite ()
{
	register int *ip;
	extern schar ();

	if (accbufp < &accbuf[sizeof accbuf - ACCVLEN])
		return;
	/*
	 * set su priv, open file, check access, and write, then reset
	 * su priv
	 */
	accsvuid = u.u_uid;		/* save uid, and set su priv */
	u.u_uid = 0;
	u.u_error = 0;			/* open file */
	u.u_dirp = "/usr/lpd/logs/logfile";
	ip = namei (&schar, 0);		/* file must already be on disk */
	if (ip == NULL)
		goto out;	/* else fix pointer and return */
	if (( !access (ip, IWRITE))	/* su allowed to write ? */
	&& (ip->i_mode & IFMT) == 0) {	/* is it a data file on disk */
		u.u_offset[0] = ip->i_size0;	/* seek eof */
		u.u_offset[1] =
			ip->i_size1 & 0177000; /* force to a block boundary */
		u.u_base = accbuf;	/* set up for write */
		u.u_count = 512;
		u.u_segflg = 1;		/* we are writing from sys space */
		writei (ip);		/* do the io */
	}
	iput (ip);			/* close the file */
out:
	u.u_uid = accsvuid;		/* relinquish su priv */
	u.u_error = 0;
	accbufp = accbuf;
}

#endif ACCTSYS
