#
/*
 * SYSTEM CODE FOR AWAIT, AWTENB, AWTDIS AND CAPACITY
 * This version implements two system calls, to access
 * both the 'old' and 'new' await calls
 *
 * Extended to cover network special files 7/28/78 S.Y. Chiu
 * Modified by BBN(dan) 9/22/78: Moved error codes to user.h and changed values
 * Modified by BBN(dan) 2/28/79: Changed capac to return -1, -1 on ordinary and
 *      block special files. Provided new routine nullcap to be used on char
 *      special files which want to return -1, -1.
 * Modified by BBN(dan) 4/9/79: Combined awtenb and awtdis code to get
 *      some I space back, combined char device driver enable and disable
 *      functions, added new routine "ablei" for them to call, and moved
 *      the await and capac entries into cdevsw.
 * Modified by BBN(dan) 4/12/79: Removed old await system call code.
 */

/* Define AWDEBUG to get messages on system console */

/*  #define AWDEBUG 1   */      /* undefined by BBN: mek (5/3/79) */

/*
 *      Operating parameters
 */

#define AWTPER 2        /* max # procs enabled for a given inode */
#define PIPSIZ 1024     /* from "ken/pipe.c" */
#define PAWAIT 1        /* max priority when awaking */
#define SAWAKE 0200     /* a flag bit in p_flag */

#include "../h/param.h"
#include "../h/netparam.h"
#include "../h/conf.h"
#include "../h/proc.h"
#include "../h/user.h"
#include "../h/inode.h"
#include "../h/file.h"
#include "../h/reg.h"
#include "../h/net.h"

/*
 * Data Structures
 * Note that these are not referenced externally, but are
 * declared extern for debugging
 */

/*
 *      Structure which says which processes are enabled for
 *      activity on an inode.  The parameter AWTPER controls how many
 *      processes may shmultaneously have any given inode 'enabled'.
 *      The itab array consists of AWTPER*NINODE*2 bytes.  The
 *      slots for each inode are grouped, and they occur
 *      at itab[inodeindex*AWTPER*2], i.e. the area associated with
 *      inode[n] is itab[n*AWTPER*2] through itab[((n+1)*AWTPER*2)-1].
 *
 *      The first byte of each pair contains the process id of the
 *      associated process, or zero, if no process is using the slot.
 *
 *      The succeeding byte contains the file descriptor used to enable
 *      the inode for that process, and indicates that activity has
 *      occurred for that inode by making the file descriptor negative.
 *      Because file descriptor 0 is legal, these are stored after
 *      incrementing by 1.
 */

extern char itab[NINODE*AWTPER*2];      /* maps inodes to pids and fds */

/*
 *      awttim is the primary data structure for timeouts.  It
 *      holds a number for each awaiting process.  This number is
 *      the time (according to "now") at which the process should
 *      be awakened.
 *
 *      Times in "now" are values in the range 0 - 255. A value of
 *      0 indicates that the process is not awaiting.
 */

extern  char    awttim[NPROC];
extern  int     nxttim;         /* next time when some process needs poke */
extern  int     now;            /* current time, 0 means not running */

int deb_proc { -1};

/*      The AWAIT, CAPACITY, AWTENB, and AWTDIS system calls are
 *      multiplexed according to the following subclasses:
 *
 *      AWAIT   : 0
 *      AWTENB  : 1
 *      AWTDIS  : 2
 *      CAPAC   : 3
 */

await()
{
        register int class, arg1;
        int     arg2,arg3;
#ifdef AWDEBUG
        int p;
        p=u.u_procp->p_pid;
#endif

        class = u.u_ar0[R0];
        arg1  = u.u_arg[0];
        if (class==3 || class==0)
        {
            arg2  = u.u_arg[1];     /* random unless class == 3 or 0 */
            arg3  = u.u_arg[2];     /* ditto */
        }
        switch(class)
        {
        case 0:         /* AWAIT */

#ifdef AWDEBUG
            if (p == deb_proc)
                printf("\nAWAIT: p=%d time=%d buf=%d len=%d",
                        p, arg1, arg2, arg3);
#endif
            awt(arg1,arg2,arg3);
            break;

        case 1:         /* AWTENB */

#ifdef AWDEBUG
            if (p == deb_proc)
                printf("\nAWTENB: p=%d fd=%d", p, arg1);
#endif
            awtenb(arg1);
            break;

        case 2:         /* AWTDIS */

#ifdef AWDEBUG
            if (p == deb_proc)
                printf("\nAWTDIS, p:%d, fd:%d",p,arg1);
#endif
            awtdis(arg1);
            break;

        case 3:         /* CAPAC */
            capac(arg1,arg2,arg3);
            break;

        default:
            u.u_error = EBADCLASS;
        }
}

/*
 * awt (AWAIT) routine - await a change or a timeout (in seconds)
 *
 * The result area is supplied by the caller.  It is simply a
 * byte array.  When waking up a process, awt fills in the byte
 * array with the file descriptors of all enabled files which
 * had activity since the last time the process exitted from awt.
 * The end of the list is signalled by a -1.  If this signal occurs
 * as the last byte of the supplied array, it implies that more
 * activity may have occurred which couldn't be reported, in which
 * case the caller should perform capacity checks to determine the
 * true state of all activity.
 *
 * Modified BBN(dan) 3/1/79: to fix itab_ptr++ bug
 */

awt(tim,bbp,bbl)
        int tim;        /* seconds to await */
        int bbl;        /* length of result area, in bytes */
        char *bbp;      /* user address of result area */

        {

        register int i;
        register char *ip;      /* itab scanner */
        register int p;         /* process id */
        struct proc *pp;
        extern int awtto();

        pp = u.u_procp;
        p = pp->p_pid;

/* return if already awakened */

        if ((tim <= 0) || (pp->p_flag & SAWAKE))
            goto awtex;

/* set up timeout */

        if (tim > 255)
            tim = 255; /* max allowed time to wait */
        spl1();            /* disables callouts so 'now' doesn't change */
        i = (now + tim);                /* compute new time */
        if (now == 0)
            i++;                    /* will start clock shortly */
        if ((i =& 0377) == 0)
            i++;                    /* zero is illegal */
        awttim[p] = i;                  /* store time to wakeup */

/* set 'next interesting time' for awtto */

        if (now == 0)
            {                       /* if no other awaiting, start up */
            now++;
            nxttim = i;             /* next time something to do */
            timeout(awtto, 0, 60);  /* call awtto every second */
            }
        else
            nxttim = cirmin(now, nxttim, i); /* sooner if needed */

#ifdef AWDEBUG
        if (p == deb_proc)
            printf("\nNOW: %o, p: %d slp til: %o", now, pp->p_pid, i);
#endif

        sleep(&(proc[p].p_flag), PAWAIT); /* sleep on flag address */
        spl0();                           /* allow callouts */

#ifdef AWDEBUG
        if (p == deb_proc)
            printf("\np: %d waking up",pp->p_pid);
#endif

/* Exit from await.  Collect info about activity and pass to the user */

awtex:
        pp->p_flag =& ~SAWAKE ; /* indicates awakened */
        awttim[p] = 0;          /* and not awaiting */

/* Scan itab, fill in user-supplied result area with fds that had activity */

        for (ip = &itab; ip < &itab[NINODE * AWTPER * 2];)
            {
            if (*ip++ == p) /* This process? */
                {
                if (*ip < 0) /* Activity? */
                    {
                    *ip = -*ip;     /* turn off */

/* if room, report to user */
/* save one byte for the endflag */

#ifdef AWDEBUG
                    if (p == deb_proc)
                        printf("\nAWTex: p=%d fd=%d", p, (*ip) - 1);
#endif

                    if (--bbl > 0)
                        subyte(bbp++, (*ip) - 1);
                    else
                        bbl++;
                    }
                }
            ip++;           /* skip over fd byte */
            }
        if (--bbl >= 0)
            subyte(bbp, -1);        /* indicate end of list */
        }
/**/
/* -------------------------- A W T E N B --------------------------- */
/*
 * Enable the specified file descriptor.
 */
awtenb(fd)
    int fd;
{
    enbdis('e', fd);
}

/* -------------------------- A W T D I S --------------------------- */
/*
 * Disable the specified file descriptor.
 */
awtdis(fd)
    int fd;
{
    if (fd == -1)   /* Disable all */
        awtclr();
    else
        enbdis('d', fd);
}

/* -------------------------- A W T C L R --------------------------- */
/*
 * Disable all of the file descriptors for a process.
 * Called from exit code.
 */
awtclr()
{
    register int fd, p;

    p = u.u_procp -> p_pid;
    for (fd = 0; fd < NOFILE; fd++)
        enbdis('d', fd);
    awttim[p] = 0;
    u.u_error = 0;                     /* ignore errors from non-existent fd */
}

/* -------------------------- E N B D I S --------------------------- */
/*
 * Common code for awtenb/awtdis/awtclr. "type" is either 'e' (enable) or
 * 'd' (disable).
 */
enbdis(type, fd)
    char type;
    int  fd;
{
    register struct inode *ip;
    struct file *f;
    extern char *ablei();
#ifndef MSG
    register int   *soc;
#endif MSG
#ifdef NETWORK
#ifdef MSG
    struct rdskt   *sktp;
#endif MSG
#endif NETWORK

    if ((f = getf(fd)) == NULL)
        return;                        /* error code set by getf */
    ip = f->f_inode;                   /* pick up inode pointer */

#ifdef NETWORK
 /*
  * the "size1" field of an inode that represents a socket
  * is used to hold the pointer to itab that "awake" uses to
  * wake up processes waiting for activity on this socket.
  */

    if (f->f_flag & FNET)
    {
#ifndef MSG
        ip = soc = f->f_netnode[f_rdnode];
        if (ip)
            *(--soc) = ablei(type, 0, ip, fd);
        ip = soc = f->f_netnode[f_wrtnode];
        if (ip)
            *(--soc) = ablei(type, 0, ip, fd);
#endif MSG
#ifdef MSG
        sktp = f->f_netnode[f_rdnode];
        if (sktp)
            sktp->itab_p = ablei(type, 0, sktp, fd);
        sktp = f->f_netnode[f_wrtnode];
        if (sktp)
            sktp->itab_p = ablei(type, 0, sktp, fd);
#endif MSG
    }
    else

#endif NETWORK

    if (f->f_flag & FPIPE)
        ablei(type, 0, ip, fd);
    else if ((ip->i_mode & IFMT) == IFCHR)
        (*cdevsw[ip->i_addr[0].d_major].d_awt) (type, ip, fd);
    else
        u.u_error = EBADDEV;
}

/* -------------------------- A B L E I ----------------------------- */
/*
 * ablei -- given type and arguments to enablei or disablei, switch off
 * to either enablei or disablei. Return the value obtained from them.
 * This routine enables the knowledge about legitimate values for 'type'
 * to be kept out of the device drivers.
 */
char *
ablei(type, ppn, ip, fd)
    char type;
    int ppn;
    struct inode *ip;
    int fd;
{
    extern char *enablei();
    extern char *disablei();

    if (type == 'e')
        return(enablei(ppn, ip, fd));
    else if (type == 'd')
        return(disablei(ppn, ip, fd));
    else
        panic("enbdis");    /* Temporary consistency check */
}

/* -------------------------- E N A B L E I ------------------------- */
/*
 * enablei -- given process id, inode pointer, and fds, enables await
 * for that inode and process.  Process id 0 is used to select
 * the current process.
 *
 * Note that a process may separately enable two or more file descriptors
 * which are associated with the same inode.
 *
 * Returns pointer to top of area in itab for specified inode.
 * The u.u_error code is also set on errors.
 */
char *
enablei(ppn,ip,fd)
    int ppn;
    struct inode *ip;
    int fd;
{
    extern struct inode inode[];
    extern struct user u;
    register char *itp;
    register char p;                        /* process id */
    char *ifree, *itps;

    p = ppn? ppn : u.u_procp->p_pid;
    fd++;                                   /* fds stored incremented */
    ifree = 0;                              /* ptr to free slot */
    itps = &itab[(ip-inode)*AWTPER*2];      /* beginning for this inode */
    for (itp=itps;itp < &itps[AWTPER*2];)
    {
        if ((*itp==p) && ((itp[1] == fd) || (itp[1] == -fd)))
            return(itps);           /* already enabled */
        if (*itp==0) ifree=itp;         /* remember free slot */
        itp =+ 2;                       /* slots are 2 bytes */
    }
    if (ifree == 0)
        u.u_error = EAWTMAX;        /* no free slots */
    else
    {
        itp = ifree;
        *itp++ = p;             /* put pid into table */
        *itp = fd;              /* and incremented file descriptor */
#ifdef AWDEBUG
        if (p == deb_proc)
            printf("\nENB, p:%d, i:%o, f: %d",p,itps,fd-1);
#endif
    }
    return(itps);           /* return pointer to head of table */
}

/* -------------------------- D I S A B L E I ----------------------- */
/*
 * disablei -- given process id, inode pointer, and fds, disables await
 * for that inode and process.  Process id 0 is used to select
 * the current process.
 * returns pointer to itab, unless all processes have been removed
 * in which case 0 is returned.
 */
char *
disablei(ppn,ip,fd)
    int ppn, fd;
    struct inode *ip;
{       extern struct inode inode[];
        extern struct user u;
        register char *itp;
        register char p;        /* process id */
        char *itps;
        char useflag;           /* flag to detect last use of inode */

        useflag = 0;
        fd++;                   /* because they are stored incremented */
        p = ppn?ppn:u.u_procp->p_pid;
        itps = &itab[(ip-inode)*AWTPER*2];      /* beginning for this inode */
        for (itp=itps;itp < &itps[AWTPER*2];) {
                if ((*itp==p) && ((itp[1] == fd) || (itp[1] == -fd))) {
                        *itp++ = 0;     /* remove pid */
                        *itp = 0;       /* clear fds as well */
                }
                else if (*itp++ != 0) useflag++;
                itp++;                  /* skip fds byte */
        }
#ifdef AWDEBUG
        if ( p == deb_proc )
            printf("\nDIS, p:%d, i:%o, f:%d",p,itps,fd-1);
#endif
        return(useflag?itps:0);
}
/*
 *      CAPACITY SYSTEM CALL
 *
 *      capac(fd,*buffer,length)
 *
 *      If fd >= 0, capacity is placed in the supplied buffer.
 *
 *      If fd == -1, the buffer is assumed to contain a table
 *      whose format is a file descriptor followed by two
 *      words into which the capacity values will be placed.
 *      If an error occurs for any file descriptor, the error
 *      code will be negated and returned in place of the file
 *      descriptor. However, the call itself will return
 *      a successful indication.
 */

capac(fd,buf,len)
	int fd,len,buf;
{
	register struct inode *ip;
	int v[3];               /* result area */
	struct  file *fp;
	register int *v1, *v2;
	extern struct user u;

	/* if table supplied, recursively call capac for individual
	 * entries.  If error is indicated, change file descriptor
	 * element of the table to contain the negative of the
	 * error code
	 */
	if (fd < 0) {
		while (len >= 6) {
			if ((fd=fuword(buf)) >= 0) capac(fd,buf+2,4);
			else u.u_error = EINVAL;
			if (u.u_error != 0) suword(buf, -u.u_error);
			buf =+ 6;       /* next triple */
			len =- 6;       /* watch for end */
			u.u_error = 0;  /* ignore errors */
		}
	if (len != 0) u.u_error = EINVAL;       /* bad table length */
	return;
	}
	/* not a table, handle a single file descriptor */
	if(len != 4) {                          /* room for answers? */
		u.u_error = EINVAL;
		return;
	}
	if((fp = getf(fd)) == NULL) return;     /* error code set by getf */
	ip = fp->f_inode;                       /* pointer to inode */
	v2 = v1 = v; v2++;                      /* ptrs to result area */

#ifdef RMI
/*
 *      The capacities are:
 *      Read:   the number of bytes left to be read in the input que,
 *              including those in any message being read and
 *              those in all other messages (if any) waiting.
 *      Write:  if a message is being written by the user
 *                      the min of what's left and the available buffer space
 *              else the min of MAXMSG and the available buffer space
 *      If there is no read or no write socket, that one gets -1.
 *
 *      FRAW check added jsq BBN 3-14-79
 */
	if ((fp -> f_flag & FRAW) && rawcap(fp, v1, v2));
	else
#endif RMI

#ifdef NCP      /* changed to NCP from NETWORK by jsq bbn 10/19/78 */
	/*
	 * if fd is a network special file descriptor, there may be
	 * two inodes to chase, one representing the read socket
	 * and the other the write socket.
	 * The capac of a read socket is the number of bytes
	 * in its read queue.
	 * -1 is returned as the read capacity if there is no
	 * read socket associated with fd.
	 * The capac of a write socket is the number of bytes allocated
	 * by the foreign host to the write socket (max. of 1000
	 * because that's the max. net msg size).
	 * As with its read counterpart, -1 is returned as the
	 * write capacity if there is no write socket associated with
	 * fd.
	 */

	if (fp->f_flag & FNET)
		{
		*v2 = *v1 = -1;
		ip=fp->f_netnode[f_rdnode];
		if (ip) *v1=ip->r_qtotal;
		ip=fp->f_netnode[f_wrtnode];
		if (ip) *v2=anyalloc(ip);
		}
	else
#endif NCP

	/* handle pipes here */
	if(fp->f_flag & FPIPE) {
		*v1 = ip->i_size1 - fp->f_offset[1];
		*v2 = PIPSIZ - ip->i_size1;
		/* following line was looking at fp -> f_flag ... */
		if ( ip->i_flag & IEOFP )  *v1 = -1;    /* jsq BBN 3-31-79 */
		if(ip->i_count < 2) {
			*v1 = -1;
			*v2 = -1;
		}
	}
	/* handle char special devices */
	else if ((ip->i_mode&IFMT) == IFCHR)    /* dispatch via table */
                (*cdevsw[ip->i_addr[0].d_major].d_cap)(ip,v1);
	/* other devices are unsupported */
        else {
                *v1 = -1;
                *v2 = -1;
        }
	/* if no error, return results in user area */
	if (u.u_error == 0) {
		suword(buf,*v1);
		suword(buf+2,*v2);
	}
};

/* 
 * capacity, etc. for unsupported devices
 */

nocap(iip,vv)
struct inode *iip;
{
	extern struct user u;
	u.u_error = EBADDEV ;
};

/*
 * capacity for only slightly supported devices
 */

nullcap(iip, vv)
int *vv;
{
        *vv++ = -1;
        *vv = -1;
}
/*
 *      AWTTO  - await timeout.  This routine implements the
 *               timeout aspect of the AWAIT system call.  It
 *               is called every second by timeout, checks and
 *               wakes any processes which should have timed out
 *             - note that even at 'nxttim', no process need be
 *               awakened, if it was poked or killed some other way
 *
 *      Note that awtto runs at priority level 5 (as a callout)
 */

awtto()
{       register int a,p;
	register struct proc *pp;
	if(now++ > 255) now=1;          /*skip over zero*/
	if(now == nxttim)               /* time to do something ? */
	{       nxttim = 1000;          /* we are searching for the new one */
		for(p = 0; p < NPROC; p++)
		{       a = awttim[p] & 0377;
			if(a == now) {  /* time to wake this process */
				pp = &proc[p];
#ifdef AWDEBUG
				if ( pp->p_pid == deb_proc )
				printf("\nTimeout, p:%d\n",pp->p_pid);
#endif
				pp->p_flag =| SAWAKE ;
				wakeup(&(pp->p_flag)) ;
			}
			else if (a != 0) nxttim = cirmin(now,nxttim,a);
		}
		if(nxttim == 1000)      /* no processes waiting */
		{       now = 0;        /* turn off background */
			nxttim=0;
			return;
		}
	}
	timeout(awtto,0,60);            /* reschedule interrupt */
}

/*
 *      AWAKE - wakes up a process that is awaiting.
 */


/* awakei -- internal call, to awaken everything associated with an inode
 */
awakei(ip,pf)
struct inode *ip;       /* pointer to inode of interest */
char pf;                /* 'process flag' -- i.e. who is doing the awake */
{
	extern struct inode inode[];
	awake(&itab[(ip-inode)*AWTPER*2],pf);
}

/* awake -- called with itab pointer and process flag
 */

awake(itps,pf)
	char *itps;     /* pointer to itab area */
	int pf;         /* process flag -1 -- current user, 0 -- system */
{
	register char p ;       /* process id */
	register char *itp;
	extern struct proc proc[];
	register struct proc *pp;

	p = pf?u.u_procp->p_pid:0;              /* process number of waker */
	for (itp=itps;itp<&itps[AWTPER*2];) {
		if ((*itp != 0) && (*itp != p)) {       /* don't awake yourself */
			pp = &proc[*itp++];             /* proc struct of wakee */
			pp->p_flag =| SAWAKE ;
			wakeup(&pp->p_flag);
			if (*itp >= 0) *itp = -*itp;    /* indicate activity */
#ifdef AWDEBUG
			if ( pp == deb_proc )
			printf("\nSignal activity for %d on %d\n",p,*itp);
#endif


			itp++;
		}
		else itp =+ 2;                          /* skip to next */
	}
}
/*
 *       UTILITY ROUTINES */

/*      cirmin - circular minimum computes the circular minimum
 *               between two eight bit values
 *               Returns the value which is 'closest' to the relative
 *               value (arg 1)
 *               Note: the values are not actually restricted to 8 bits
 *
 *      Ah!!!! The value returned has to be mod 256, otherwise it is
 *      possible for a process to never awaken from its await, the
 *      way cirmin is used in awt and awtto.
 *      The proper changes were made on Feb. 8, 1979. S.Y. Chiu (BBN).
*/

cirmin(r,x,y)
	int     r,x,y;
{
	if(x < r) x =+ 256;
	if(y < r) y =+ 256;
/*      if(x < y) return(x);    */
	if(x < y) return(x%256);
/*      return(y);          */
	return(y%256);
}


