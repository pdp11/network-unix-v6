# include  "rm.h"
# include  "rmerr.h"
# include  "rmenv.h"

# define  PORTMODE   0300     /*  mode of standard input/output for protocol  */
                              /*  any parity, no echo, no <cr> to <nl>        */
#define PORTSPEED 9		/* port speed : 1200 bAud */




  struct sgtty  savestty;      /*  used to save the original mode of the  */
                               /*  port by 'setmode'                      */

  struct iobuf  transbuf;      /*  buffer for transcript file  */

  extern struct rmenv  rmenv;  /*  protocol environment structure  */

  extern char  *cpstr();       /*  function to copy string  */

  extern int  errno;           /*  contains error code for system calls  */



/*
 *     this routine starts up logging in the designated file.  the results of
 *     the current session are appended to the file if it already exists.
 *     if 'tranfile' is non-zero, a transcript file is created.
 */

d_strtlog(logfile, tranfile, debugf, ttylog)
  char  *logfile, *tranfile;
  int  debugf, ttylog;
    {

    rmenv.d_ttylog = ttylog;

/*  try to open the log file.  if we can't, create it.  if it exists, seek  */
/*  to its end                                                              */

    rmenv.d_logfd = open(logfile, 1);

    if (rmenv.d_logfd < 0)
      {
      rmenv.d_logfd = creat(logfile, 0600);

      if (rmenv.d_logfd < 0)
        return(EUSER);
      }

    else
       seek(rmenv.d_logfd, 0, 2);

/*  set debugging flag  */

    if (debugf)
      rmenv.d_debug = 1;

/*  if indicated, create the transcript file and set the flag  */

    if (tranfile)
      {
      if (fcreat(tranfile, &transbuf) < 0)
        {
        d_log("***** couldn't create transcript file");
        return(EUSER);
        }

      rmenv.d_tranon = 1;
      }

    d_dbglog("startlog:  logging started.  tranfile = %o", tranfile);
    return(OK);
    }




/*
 *     this routine does the logging via printf's to the channel which has
 *     previously be directed.  time and date are added to each line.
 *     there are lots of arguments so we can log longs and doubles.
 */

d_log(format, a, b, c, d, e, f, g, h)
  char  *format, *a, *b, *c, *d, *e, *f, *g, *h;
    {
    extern int  *localtime(), fout;
    register int  *tpt, userfd;
    long  ltime;

/*  we want to redirect the output stream of printf for our own use.  save  */
/*  the state that the user thinks it's in                                  */

    flush();
    userfd = fout;
    fout = rmenv.d_logfd;

/*  timestamp things when they're written into the log file  */

    time(&ltime);
    tpt = localtime(&ltime);

    printf("%2d/%2d/%2d %2d:%2d:%2d - ", tpt[4] + 1, tpt[3], tpt[5],
         tpt[2], tpt[1], tpt[0]);

    printf(format, a, b, c, d, e, f, g, h);
    putchar('\n');
    flush();

/*  if selected, write on the standard output also  */

    if (rmenv.d_ttylog)
      {
      fout = 1;
      printf(format, a, b, c, d, e, f, g, h);
      putchar('\n');
      flush();
      }

/*  restore the user  */

    fout = userfd;
    }




/*
 *     routine called to log rmenv.d_debugging information
 */

d_dbglog(format, a, b, c, d, e, f, g, h,)
  char  *format, *a, *b, *c, *d, *e, *f, *g, *h;
    {

    if (rmenv.d_debug)
      d_log(format, a, b, c, d, e, f, g, h);
    }




/*
 *     this routine writes up to 'length' characters from 'string' on the
 *     transcript file
 */

d_tscribe(string, length)
  char  *string;
  int  length;
    {

    if (rmenv.d_tranon)
      while (length--)
        putc(*string++, &transbuf);

    fflush(&transbuf);
    }




/*
 *     this routine is called to save the current characteristics of the
 *     port and then change its erase and kill characters and its mode
 */

d_setport(mode)
  int  mode;
    {
    struct sgtty  sttybuf;

    if (gtty(rmenv.d_portfd, &savestty) < 0)
      {
      d_log("***** can't get port characteristics - errno %d", errno);
      return(ESYSTEM);
      }

    sttybuf.s_mode = mode;
    sttybuf.s_erase = SOH;
    sttybuf.s_kill = SOH;
/*.   sttybuf.s_ispeed = savestty.s_ispeed;*/
    sttybuf.s_ispeed = PORTSPEED;
/*    sttybuf.s_ospeed = savestty.s_ospeed;*/
    sttybuf.s_ospeed = PORTSPEED;

    if (stty(rmenv.d_portfd, &sttybuf) < 0)
      {
      d_log("setport: can't do 'stty' - errno %d", errno);
      return(ESYSTEM);
      }

    d_dbglog("setport:  port mode set to %o", mode);
    return(OK);
    }




/*
 *     this routine is called to change the parameters of the port without
 *     disturbing the original values saved by 'setport'
 */

d_chgport(mode)
  int  mode;
    {
    struct sgtty  sttybuf;

    if (gtty(rmenv.d_portfd, &sttybuf) < 0)
      {
      d_log("chgport: can't do 'gtty' - errno %d", errno);
      return(ESYSTEM);
      }

    sttybuf.s_mode = mode;

    if (stty(rmenv.d_portfd, &sttybuf) < 0)
      {
      d_log("chgport: can't do 'stty' - errno %d", errno);
      return(ESYSTEM);
      }

    d_dbglog("chngport:  port mode changed to %o", mode);
    return(OK);
    }




/*
 *     this routine is called to reset the parameters of the port
 *     to their original values as saved by 'setport'
 */

d_rstport()
    {

    if (stty(rmenv.d_portfd, &savestty) < 0)
      {
      d_log("resetport: can't do 'stty' - errno %d", errno);
      return(ESYSTEM);
      }

    d_dbglog("resetport:  port mode reset to %o", savestty.s_mode);
    }
