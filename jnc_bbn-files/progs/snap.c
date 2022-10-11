/*
 * snap -- take a snapshot of a running process
 *
 * Generates a core image file, suitable for use by adb, of a specified
 * process image. Reads from /dev/mem or swap device.
 *
 * snap [-k] [-u] pid [-c core] [-s system] [-m memory] [-swap swapdev]
 * snap [-k] [-u] -a u_addr [-c core] [-m memory]
 *
 *   pid: the process-id. Used with system and memory to get address and
 *        residence (memory or swap device) of process image.
 *   u_addr: address in memory of process image.
 *
 * Either u_addr or pid must be specified. If u_addr is specified, any swap
 * device specified is ignored.
 *
 *   core: output core-image file
 *       The default is "core", unless -k specified,
 *       in which case it is "kernel".
 *   system: system namefile to use (default /unix)
 *   memory: file to copy proc entry and in-core core image from (def. /dev/mem)
 *   swapdev: file to copy swapped-out core image from (def. current swap dev)
 *   -k: generate kernel snapshot with U block of specified process. Like
 *       copying /dev/kmem except that the current process (at 0140000) is the
 *       one specified.
 *   -u: Copy out only the U block instead of the whole process image.
 *
 * Algorithm:
 *       Find process image:
 *          If pid given, look up _proc; get offset and residence from "memory"
 *          If u_addr given, it is the offset
 *       If in core
 *          copy from /dev/mem
 *       else
 *          find out swap device
 *          copy from there
 *
 * Note: requires phototypesetter compiler
 * BUGS
 *    -a without -k or -u copies 65k bytes. ought to copy (u_dsize+u_ssize+16)*64.
 *
 * Originally coded by BBN(dan) Jan 23 1978
 * Modified by BBN(dan) April 24 1979: now checks process id to make sure it is
 *    within range, checks to make sure that process is alive, and checks status
 *    of process when copy done to make sure it has not died or moved. If it has
 *    moved, snap will now retry.
 */

#include "../h/param.h"
#include "../h/proc.h"
#include "/usr/sys/h/statbuf.h"

char *ME;

main(argc, argv)
   int argc;
   char *argv[];
   {
   char *sysfile;
   char *corefile;
   char *swapfile;
   char *outfile;
   int i;
   int rgid;
   int pid;
   long offset;
   long size;
   int kflag, uflag;
   int corefd;
   int outfd;
   int procfd;
   struct proc *procp;
   extern char *atoiv();
   extern char *aotol();

   ME = argv[0];

   sysfile = "/unix";
   corefile = "/dev/mem";
   swapfile = 0;           /* Will fill in with current swapdev if need */
   outfile = 0;            /* Uninitialized */

   if (argc == 1)
      {
      fdprintf(2, "Usage:\n");
      fdprintf(2,
"%s [-k] [-u] pid [-c core] [-s sys] [-m mem] [-swap swapdev] [-a u_addr]\n",
      ME);
      exit(-1);
      }

/* Process arguments */

   pid = -1;
   kflag = 0;
   uflag = 0;
   offset = -1;
   for (i = 1; i < argc; i++)
      {
      if (seq(argv[i], "-k"))
         kflag++;
      else if (seq(argv[i], "-u"))
         uflag++;
      else if (seq(argv[i], "-s"))
         sysfile = argv[++i];
      else if (seq(argv[i], "-m"))
         corefile = argv[++i];
      else if (seq(argv[i], "-swap"))
         swapfile = argv[++i];
      else if (seq(argv[i], "-c"))
         outfile = argv[++i];
      else if (seq(argv[i], "-a"))
         {
         if (*aotol(argv[++i], &offset) || offset < 0)
            {
            fdprintf(2, "%s: Bad u block address \"%s\".\n", ME, argv[i]);
            exit(-1);
            }
         }
      else if (argv[i][0] == '-')
         {
         fdprintf(2, "%s: Unknown control argument \"%s\".\n", ME, argv[i]);
         exit(-1);
         }
      else if (pid == -1)  /* Still need a pid */
         {
         if (*atoiv(argv[i], &pid))
            {
            fdprintf(2, "%s: Bad process id \"%s\".\n", ME, argv[i]);
            exit(-1);
            }
         }
      else
         {
         fdprintf(2, "%s: Extraneous argument \"%s\".\n", ME, argv[i]);
         exit(-1);
         }
      if (i == argc)
         {
         fdprintf(2, "%s: %s requires an argument.\n", ME, argv[i-1]);
         exit(1);
         }
      }

   if (outfile == 0)
      if (kflag)
         outfile = "kernel";
      else
         outfile = "core";
   if (pid == -1 && offset == -1)
      {
      fdprintf(2, "%s: Either a process id or -a must be given.\n", ME);
      exit(-1);
      }
   if (pid != -1 && offset != -1)
      {
      fdprintf(2, "%s: Cannot specify both pid and -a.\n", ME);
      exit(-1);
      }

/* Open corefile, set corefd */

   if ((corefd = open(corefile, 0)) == -1)
      {
      fdprintf(2, "%s: %s. \"%s\"\n", ME, errmsg(0), corefile);
      exit(1);
      }

   if (pid != -1)
      {
      int swapfd;
      int swapdev;

      if (pid >= NPROC)
         {
         fdprintf(2, "%s: No such process ID: %d.\n", ME, pid);
         exit(-1);
         }
      getvals(sysfile, &procp, &swapdev);

RETRY:
      seekread(pid, corefd, procp, corefile, &proc[0]);

      if (proc[0].p_stat == 0)
         {
         fdprintf(2, "%s: No such process: %d.\n", ME, pid);
         exit(1);
         }

      if ((proc[0].p_flag & SLOAD) == 0)
         {
         printf("Reading from swap device...\n");
         if (swapfile == 0)
            if (swapdev != 0)
               {
               if (seek(corefd, swapdev, 0) == -1)
                  {
                  fdprintf(2, "%s: %s. Seek on \"%s\" failed.\n", ME,
                               errmsg(0), corefile);
                  exit(1);
                  }
               if (read(corefd, &swapdev, 2) != 2)
                  {
                  fdprintf(2, "%s: Error reading \"%s\".\n", ME, corefile);
                  exit(1);
                  }
               swapfile = findswap(swapdev, "/dev/hps10");
               }
            else
               {
               fdprintf(2, "%s: Symbol not found. \"_swapdev\" in \"%s\"\n",
                           ME, sysfile);
               exit(1);
               }
         if ((swapfd = open(swapfile, 0)) == -1)
            {
            fdprintf(2, "%s: %s. \"%s\"\n", ME, errmsg(0), swapfile);
            exit(1);
            }
         offset = (long)proc[0].p_addr * 01000;
         size = (long)proc[0].p_size * 01000;
         procfd = swapfd;
         }
      else
         {
         offset = (long)proc[0].p_addr * 0100;
         size = (long)proc[0].p_size * 0100;
         procfd = corefd;
         }
      }
   else
      {
      size = 0202000L;     /* For -a, just copy max possible */
      procfd = corefd;
      }

/* Finally create output file... */

   rgid = getgid() & 0377;
   setgid(rgid | (rgid<<8));
   if ((outfd = creat(outfile, 0644)) == -1)
      {
      fdprintf(2, "%s: %s. Cannot create \"%s\".\n", ME, errmsg(0), outfile);
      exit(1);
      }

/* And copy */

   if (kflag)
      {
      copy(corefd, outfd, 0L, 0140000L);
      copy(procfd, outfd, offset, 02000L);
      }
   else
      copy(procfd, outfd, offset, uflag? 02000:size);

/* Check if it moved */

   if (pid != -1)
      {
      struct proc proc2;

      seekread(pid, corefd, procp, corefile, &proc2);
      if (proc2.p_stat == 0)
         {
         printf(
"Process died while it was being copied. File may be inaccurate.\n"
            );
         exit(0);
         }
      if ( ((proc2.p_flag & SLOAD) != (proc[0].p_flag & SLOAD))
            || (proc2.p_addr != proc[0].p_addr)
            || (proc2.p_size != proc[0].p_size) )
         {
         printf("Process moved while it was being copied. Retrying...\n");
         goto RETRY;
         }
      }

   exit(0);
   }

/* -------------------------- S E E K R E A D ----------------------- */
/*
 * seekread() seeks to the process table entry and reads it into proc.
 */
seekread(pid, corefd, procp, corefile, proc)
int  pid;           /* Process ID */
int  corefd;        /* File descriptor of memory file */
struct proc *procp; /* Position of _proc in memory file */
char *corefile;     /* Name of memory file (for diagnostics) */
struct proc proc[]; /* Process table slot (output) */
{
   if (seek(corefd, procp + pid, 0) == -1)   /* Seek to _proc */
      {
      fdprintf(2, "%s: %s. Cannot seek to process slot in \"%s\".\n",
                   ME, errmsg(0), corefile);
      exit(1);
      }

   if (read(corefd, &proc[0], sizeof(proc[0])) != sizeof(proc[0]))
      {
      fdprintf(2, "%s: Error reading \"%s\".\n", ME, corefile);
      exit(1);
      }
}

/* -------------------------- G E T V A L S ------------------------- */
/*
 * getvals(sysfile, &procp, &swapdev) examines the namelist of sysfile
 * in order to look up "_proc" and "swapdev". Their values are assigned to
 * "procp" and "swapdev" respectively. If "_proc" is not found, it is a fatal
 * error; "swapdev" may not be needed, however.
 */
struct
   {
   char    name[8];
   int     type;
   int     value;
   } nl[ ]
   {
   "_proc\0\0", 0, 0,
   "_swapdev", 0, 0,
   0, 0, 0
   };

getvals(sysfile, procpp, swapdevp)
   char *sysfile;
   int *procpp;
   int *swapdevp;
   {

/* Try to open sysfile to see if it is accessible */

   if (open(sysfile, 0) == -1)
      {
      fdprintf(2, "%s: %s. \"%s\"\n", ME, errmsg(0), sysfile);
      exit(1);
      }

/* It is. Set procp and swapdev. */

   nlist(sysfile, nl);

   if (nl[0].type)
      *procpp = nl[0].value;
   else
      {
      fdprintf(2, "%s: Symbol not found. \"%.8s\" in \"%s\"\n",
                  ME, nl[0].name, sysfile);
      exit(1);
      }

   if (nl[1].type)
      *swapdevp = nl[1].value;
   else
      *swapdevp = 0;
   }

/* -------------------------- C O P Y --------------------------------------- */
/*
 * copy(infd, outfd, offset, length) seeks to byte offset in infd and copies
 * length bytes to outfd.
 */
copy(infd, outfd, offset, len)
   int infd, outfd;
   long offset;
   long len;
   {
   char buffer[512];
   int nreq;
   int nact;

   if (lseek(infd, offset, 0) == -1)
      {
      fdprintf(2, "%s: %s. Seek error.\n", ME, errmsg(0));
      exit(1);
      }

   while(len)
      {
      nreq = sizeof(buffer);
      if (len < nreq)
         nreq = len;
      nact = read(infd, buffer, nreq);
      if (nact == 0)
         {
         fdprintf(2, "%s: Premature EOF.\n", ME);
         exit(1);
         }
      if (nact == -1)
         {
         fdprintf(2, "%s: %s. Error while reading.\n", ME, errmsg(0));
         exit(1);
         }
      nreq = nact;
      nact = write(outfd, buffer, nreq);
      if (nact < 0)
         {
         fdprintf(2, "%s: %s. Error writing output file.\n", ME, errmsg(0));
         exit(1);
         }
      if (nact == 0)
         {
         fdprintf(2, "%s: EOF while writing output file.\n", ME, errmsg(0));
         exit(1);
         }
      if (nact < nreq)
         {
         fdprintf(2, "%s: Write error on output file.\n", ME);
         exit(1);
         }
      len =- nact;
      }
   }

/* -------------------------- F I N D S W A P ------------------------------ */
/*
 * findswap(swapdev, guess) searches through /dev for the special file with
 * major and minor device numbers in swapdev. It first stats "guess" to see if
 * it matches. This saves time. It returns a pointer to the name of the swap
 * device.
 */
char *
findswap(swapdev, guess)
   int swapdev;
   char *guess;
   {
   register struct
      {
      int dir_ino;
      char dir_n[14];
      } *p;
   register i;
   int f;
   char fname[20];
   char dbuf[512];
   struct statbuf sbuf;

   if (stat(guess, &sbuf) == 0 && (sbuf.s_flags&SFMT) == SFBLK
   && sbuf.s_addr[0] == swapdev)
      return(guess);

   f = open("/dev", 0);
   if (f < 0)
      {
      fdprintf(2, "%s: %s. Cannot open \"/dev\".\n", ME, errmsg(0));
      exit(1);
      }

   while ((i = read(f, dbuf, 512)) > 0)
      {
      while(i < 512)
         dbuf[i++] = 0;
      for (p = dbuf; p < dbuf + 512; p++)
         {
         if (p->dir_ino == 0) /* Unused entry */
            continue;
         scopy(p->dir_n, scopy("/dev/", fname, -1), -1);
         if (stat(fname, &sbuf) < 0)      /* Cannot stat */
            continue;
         if ((sbuf.s_flags&SFMT) != SFBLK)   /* Block special file? */
            continue;
         if (sbuf.s_addr[0] == swapdev)      /* Of the right value? */
            return(fname);
         }
      }
   close(f);
   fdprintf(2, "%s: Cannot find swap device.\n", ME);
   exit(1);
   }

/* -------------------------- S E Q --------------------------------- */
/*
 * int = seq(str1, str2)
 * Compares str1 and str2; returns 1 if equal, 0 if not
 */
int
seq(s1, s2)
   char *s1, *s2;
   {
   register char *p1, *p2;

   p1 = s1;
   p2 = s2;
   while (*p1 == *p2++)
      if (*p1++ == 0)
         return(1);
   return(0);
   }

/* -------------------------- A O T O L ------------------------------ */
/*
 * aotol -- convert octal string to long number
 */
char *
aotol(str, lng)
   char *str;
   long *lng;
   {
   long temp;
   register char c;
   register char *s;

   s = str;
   temp = 0;
   for(;;)
      {
      c = *s++ - '0';
      if (c >= 0 && c <= 7)
         temp = (temp<<3) + c;
      else
         {
         *lng = temp;
         return(s-1);
         }
      }
   }

/* -------------------------- S C O P Y ----------------------------- */
/*
 * str = scopy(str1, str2, &str2[lastbyte])
 * Copies str1 into str2, truncating if necessary. Returns a pointer to the
 * null. The simplest way to use this function is as follows:
 *    scopy(src, dest, -1);
 * which copies without regard for the length of dest.
 */
char *
scopy(s1, s2, s3)
   char *s1;
   char *s2;
   char *s3;
   {
   register char *p1;
   register char *p2;
   register char *p3;

   p1 = s1;
   p2 = s2;
   p3 = s3;

   while(p2 < p3 && *p1)
      *p2++ = *p1++;

   /* Either p2 = p3 or p1 points at null byte. Either way, terminate. */

   *p2 = 0;
   return(p2);
   }
