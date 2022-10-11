
#include "errno.h"
/*	Forking is complicated by the fact that a process
 *	that exits becomes a "ZOMBIE".	It does not leave
 *      UNIX until it is waited for by its parent.  There-
 *      fore, the logger creates two processes,
 *	a child and a grandchild, so that the grandchild,
 *	which actually does the work, will be adopted by
 *	the init process when the child exits.
 *
 *      Make sure it waits on the right thing jsq BBN 5Aug79
 *      Put back pipe transfer of granchild id for systems with
 *      full size pids.  jsq BBN 18Feb80.
 */
int par_uid;

spawn ()
{
  int   child,
        grandchild,
        grandparent;
  int   pstat;
  extern int  errno;
  int   pipefds[2];

  while (pipe (pipefds) == -1)
    if (errno != ENFILE)
      return (-1);
    else
      sleep (10);
  par_uid = getpid();
  while ((child = fork ()) == -1)
    sleep (10);
  if (child)
  {                             /* parent waits for pid of grandchild */
    close (pipefds[1]);
    while (wait (&pstat) != child)
      ;
    if (read (pipefds[0], &grandchild, sizeof (grandchild))
	!= sizeof (grandchild))
      grandchild = -1;
    close (pipefds[0]);
    return (grandchild);
  }
  else
  {
    close (pipefds[0]);
    grandparent = par_uid;      /* save (grand)parent uid for use later */
    while ((grandchild = fork ()) == -1)
      sleep (10);
    if (grandchild)
    {
      write (pipefds[1], &grandchild, sizeof (grandchild));
      exit (grandchild);	/* After creating grandchild, exit. */
    }
    par_uid = grandparent;
    close (pipefds[1]);
    return (0);
  }
}

