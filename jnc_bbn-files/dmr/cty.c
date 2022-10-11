#
/*
 * /dev/tty driver
 *
 * Only an open routine is needed.
 * The routine repoints the file structure just allocated
 * -- the one for this device -- at the inode for the controlling tty.
 * It then calls openi() on that inode.
 * This method is used to enable await and capacity stuff to work.
 */

#include "../h/param.h"
#include "../h/user.h"
#include "../h/inode.h"
#include "../h/conf.h"
#include "../h/file.h"

ctyopen(dev, rw)
   int dev;
   int rw;
   {
   register struct inode *ip;
   register struct file *fp;
   struct inode *myp;
   int inodep;       /* For comparison with file.f_inode */
   struct inode *realp;

/* Find the in-core inode for this device. */

   for (ip = &inode[0]; ip < &inode[NINODE]; ip++)
      {
      if (ip->i_count != 0
      && (ip->i_mode & IFMT) == IFCHR
      && (ip->i_addr[0] == dev))
         goto got_inode;
      }
   u.u_error = EISDIR;     /* "Can't happen" */
   return;
got_inode:

   myp = ip;

/* Given the inode, we can find the file structure. */

   inodep = myp;
   for (fp = &file[0]; fp < &file[NFILE]; fp++)
      {
      if (fp->f_count != 0
      && fp->f_inode == inodep)
         goto got_file;
      }
   u.u_error = ENOTDIR;    /* Can't happen */
   return;
got_file:

/* Now find the inode for the controlling terminal. */

   if (u.u_ttyp)
      for (ip = &inode[0]; ip < &inode[NINODE]; ip++)
         {
         if (ip->i_count != 0
         && (ip->i_mode & IFMT) == IFCHR
         && (ip->i_addr[0] == u.u_ttyd))
            goto got_ctrl;
         }
   u.u_error = ENOTTY;  /* No controlling terminal */
   return;
got_ctrl:

   realp = iget(ip->i_dev, ip->i_number);  /* Get the real device */
   fp->f_inode = realp;    /* Repoint our file structure */
   prele(realp);           /* Release it just like open does */
   plock(myp);             /* Lock our inode so that we can... */
   iput(myp);              /* Legally relinquish access to it */
   openi(realp, rw);       /* Open actual controlling terminal */
   }
