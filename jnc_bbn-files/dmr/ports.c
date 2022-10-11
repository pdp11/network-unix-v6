#
/*
 * Interprocess communication ports
 */

#include "../h/param.h"
#include "../h/systm.h"
#include "../h/conf.h"
#include "../h/user.h"
#include "../h/proc.h"
#include "../h/inode.h"
#include "../h/file.h"
#include "../h/reg.h"

#define PORTSIZ 1024 /* Must be less than 32768 */

/* Usage of inode words in port inode
 *
** NOTE:  The useage described below in this comment was changed to be
** like the definition of the structure portskt following jsq BBN 3-14-79
 *
 *  Words 0-3 have the format of a file structure for use in readp
 *  i_addr[0]: major/minor device code (See below)
 *  i_addr[1]: pointer to disk inode
 *  i_addr[2]: zero (high order part of offset)
 *  i_addr[3]: read offset (Write offset is i_size1 in disk inode)
 *  i_addr[4]:  port size ( BBN:mek - 11/20/78 )
 *  i_addr[5]:  available
 *  i_addr[6]:  available
 *  i_addr[7]:  available
 *  i_lastr:    available
 *
 */
struct portskt {        /* goes over an inode structure */
	int     po_1unused[7];  /* unused words */
	int     po_dev;         /* major device number in high byte, */
				/* following flags in low byte  */
	struct file po_f;       /* file structure for use in readp */
	int     po_size;        /* port size */
	int     po_2unused[2];   /* unused words */
};

/*  Minor device bits  */
#define PHEADER   07    /* Write headers     */
#define PNAMED    010   /* Named port        */
#define PNOREADER 020   /* Reader has closed */

/* Header format */
#define HDRSIZ 8        /* Max header size (bytes) */
struct header
{       int hpid;       /* Process id of writer */
	int hpgrp;      /* Process group (file table pointer) */
	int huid;       /* User id of writer */
	int hcf;        /* Count (low order 12 bits) + flags below */
};
	/* A zero count indicates that the writer whose */
	/* process id is given has closed the file      */
#define EOM    0100000  /* End of message flag */
/*#define FPORT 08   */ /* File table flag for ports added to file.h  */
			/*    Invokes special close */

extern int portdev;    /* Defined in conf/c.c */


/* Make a port.
 *
 * Arguments: (1) File name pointer (also in u.u_dirp)
 *                  (possibly pointer to null string)
 *            (2) Mode
 * Returns read file descriptor in r0, write file descriptor
 *      in r1 (if (mode&RDONLY) != 0)
 */
#define RDONLY  0100000 /* Mode flag for read-only */
#define HDRPID  040000  /* Write process id in header */
#define HDRPGRP 020000  /* Write process group in header */
#define HDRUID  010000  /* Write user id in header */
#define HEADER  HDRPID|HDRPGRP|HDRUID

port()

{       register *ip, *dip, *rf;
	extern uchar;
	int wf, r, devcode;

	devcode = portdev;
	if (fubyte(u.u_dirp) > 0) /* Named - port or npipe */
	{       if ((ip = namei(&uchar,1)) == NULL)
		{       if (u.u_error ||
			   (ip = maknode((u.u_arg[1]&0777)|IFCHR)) == NULL)
				return;
			devcode =| PNAMED;
		}
		else /* File exists */
		{       u.u_error = EEXIST;
			iput(ip);
			return;
		}
	}
	else /* Unnamed */
	{       if ((ip = ialloc(rootdev)) == NULL)
			return;
		ip->i_mode = IALLOC|IFCHR|0666;
	}
	ip->i_flag = IUPD|IACC; /* Unlocks it too */

	/* Set up disk inode */
	if ((dip = ialloc(rootdev)) == NULL)
err:    {       iput(ip);
		return;
	}
	dip->i_flag = IACC|IUPD;  /* Unlocks it too, like pipe() */
	dip->i_mode =| IPIPE;

	if ((rf = falloc()) == NULL) goto err;
	if ((u.u_arg[1]&RDONLY) == 0)
	{       r = u.u_ar0[R0];
		if ((wf = falloc()) == NULL)
		{       rf->f_count = 0;        /* Undo read falloc */
			u.u_ofile[r] = NULL;
			goto err;
		}
		u.u_ar0[R1] = u.u_ar0[R0];
		u.u_ar0[R0] = r;
		wf->f_flag = FWRITE|FPORT;
		wf->f_inode = ip;
		ip->i_count++;
	}

	rf->f_flag = FREAD|FPORT;
	rf->f_inode = ip;
	if (u.u_arg[1]&HEADER) devcode =|
		(u.u_arg[1]&(HDRPID|HDRPGRP|HDRUID)) >> 12;
	ip -> po_dev = devcode;         /* jsq BBN 3-14-79 changed to */
	ip -> po_f.f_inode = dip;       /* facilitate expanding   */
	ip -> po_size = PORTSIZ;        /* file  structure */

}

/* portopen, portread, and portwrite rely on u.u_ar0[R0] */
/* containing the port file descriptor */

portopen(dev,flag)
int dev, flag;
{       /* Can't open a port for reading.
	 * Open for writing only if there is a reader
	 */
	if (flag&FREAD || dev.d_minor&PNOREADER)
		u.u_error = EIO;
	else
		u.u_ofile[u.u_ar0[R0]]->f_flag =| FPORT;
}


/* Modified 12/29/77 to interface to awake (BBN: jfh)
 * Modified 4/9/79 for new awake interface (BBN: dan)
 */

portwrite(dev)
int dev;
{       register *dip;
	register int m;
	register char *ucount;
	int *ip, *fp, space, size, hold;

	fp = u.u_ofile[u.u_ar0[R0]];    /* Checked by rdwr */
	ip = fp->f_inode;
	dip = ip -> po_f.f_inode;       /* jsq BBN 3-15-79 */
	ucount = u.u_count;
	u.u_offset[0] = 0; /* u.u_offset[1] set below */

	while (ucount)
	{       plock(dip);


		if (ip -> po_dev&PNOREADER)     /* jsq BBN 3-14-79 */
		{       prele(dip);
			u.u_error = EPIPE;
			psignal(u.u_procp,SIGPIPE);
			return;
		}
		hold = ip -> po_size;           /* jsq BBN 3-13-79 */
		if ( dev&PHEADER ) hold =- HDRSIZ;
		if ( ( space = hold -
			(size = u.u_offset[1] = dip->i_size1)) <= 0)
		{       /* No space for header + 1 byte */
			dip->i_mode =| IWRITE;
			prele(dip);
			sleep(dip+1,PPIPE);
			continue;  /* Try again on wakeup */
		}

		m = min (space,ucount);
		if (dev&PHEADER)
			porthdr(dev, fp, dip, m, ucount==m);

		/* Write data */
		u.u_count = m;
		u.u_segflg = 0;
		writei(dip);
		if (u.u_error) /* E.g., Memory fault */
		{       dip->i_size1 = size;    /* Reset to before header */
			prele(dip);
			return;
		}
		prele(dip);
		awakei(ip,-1);          /* awake other end (BBN:jfh) */
		if (dip->i_mode&IREAD)
		{       dip->i_mode =& ~IREAD;
			wakeup(dip+2);
		}
		ucount =- m;
	}
}


porthdr(dev, fp, ino, count, eomflag)
int dev, *fp, *ino, count, eomflag;
/* Write header.  inode = locked disk inode.  Space is available. */
{       register int *h, ubase, hcount;
	int header[HDRSIZ/2];

	ubase = u.u_base;
	u.u_base = h = header;
	hcount = 1;
	if (dev&(HDRPID>>12))
	{       *h++ = u.u_procp->p_pid;  hcount++; }
	if (dev&(HDRPGRP>>12))
	{       *h++ = fp;  hcount++; }
	if (dev&(HDRUID>>12))
	{       *h++ = u.u_uid<<8 | u.u_ruid; hcount++;  }
	*h = count;
	if (eomflag) *h =| EOM;
	u.u_count = hcount<<1;
	u.u_segflg = 1;
	writei(ino);
	u.u_base = ubase;
}

/* Modified 12/29/77 to interface to awake (BBN: jfh)
 */

portread(dev)
int dev;
{       register *ip, *dip, ucount;

	ip = (u.u_ofile[u.u_ar0[R0]]) -> f_inode;
	dip = ip -> po_f.f_inode;       /* jsq BBN 3-14-79 */
	ucount = u.u_count;


	for (;;)
	{
		readp (&ip -> po_f, -1);        /* jsq BBN 3-14-79 */
		plock(dip);                     /* Unnecessary? */
		if (u.u_count != ucount  ||     /* Some chars were read or  */
		   (ip->i_count < 2 &&          /* There are no writers and */
		    (ip -> po_dev&PNAMED) == 0))/* jsq BBN 3-14-79 */
			break;
		dip->i_mode =| IREAD;
		prele(dip);                     /* Unnecessary? */
		sleep(dip+2,PPIPE);
	}
	awakei(ip,-1);		/* awake other end (BBN:jfh) */
	prele(dip);

}

portstty(dev,v)
int *v;
/* for later use */
{
}
/*
 * Await and Capacity routines
 */

/* portcap -- capacity for ports 
 */

portcap(iip,vv)
struct inode *iip;
int *vv;
{
register struct file *fp;
register int *v;
register struct inode *ip,*nip;
	v=vv; ip=iip;
	fp = &ip -> po_f;       /* jsq BBN 3-14-79 */
	nip = fp->f_inode;
	*v++ = nip->i_size1 - fp->f_offset[1];
	*v = ip -> po_size - nip -> i_size1;    /* jsq BBN 3-14-79 */
}

portawt(type, ip, fd)
    char type;
    struct inode *ip;
    int fd;
{
    extern char *ablei();

    ablei(type, 0, ip, fd);
}

/*
 */
portclose(fp)
struct file *fp;
/* Entered via special case code in closef (fio.c) */
{       register *ip, *dip;

	ip = fp->f_inode;
	plock (dip = ip -> po_f.f_inode);       /* jsq BBN 3-14-79 */

	if (fp->f_flag&FREAD)
	{
		ip -> po_dev =| PNOREADER;      /* jsq BBN 3-14-79 */
		if (dip->i_mode&IWRITE)
		{       dip->i_mode =& ~IWRITE;
			wakeup(dip+1); /* Notify writers */
		}
	}
	else /* A writer */
	{
		if (ip -> po_dev & PHEADER)     /* jsq BBN 3-14-79 */
		{
/* jsq BBN 3-14-79 */   while ((ip -> po_dev & PNOREADER) == 0)
			{
/* jsq BBN 3-14-79 */           if ((ip->po_size -
				    (u.u_offset[1] = dip->i_size1)) >= HDRSIZ)
				{       u.u_offset[0] = 0;
					/* Write header with EOM, no data */
/* jsq BBN 3-14-79 */                   porthdr(ip -> po_dev, fp, dip,0,1);
					break;
				}
				else
				{       dip->i_mode =| IWRITE;
					prele(dip);
					sleep(dip+1,PPIPE);
					plock(dip);
				}
			}
		}
		if (dip->i_mode&IREAD)
		{       dip->i_mode =& ~IREAD;
			wakeup(dip+2);
		}
	}

	awakei(ip,-1);	/* wake up other end (BBN:cdh) 07/20/78 */
	dip->i_mode =& ~IPIPE;
	if (ip->i_count > 1)
	{       ip->i_count--;
		prele(dip);
	}
	else
	{       iput(ip);
		iput(dip);
	}
}
