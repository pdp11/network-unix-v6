# include  "rm.h"
# include  "rmerr.h"
# include  "rmenv.h"



# define  PORTMODE   0300     /*  mode of standard input/output for protocol  */
                              /*  any parity, no echo, no <cr> to <nl>        */




  extern struct rmenv  rmenv;     /*  protocol environment structure  */




/*
 *     this routine is called by the slave to start up his half of the
 *     link protocol
 */

dlstart(logfile, tranfile, debugf)
  char  *logfile, *tranfile;
  int  debugf;
    {
    register int  result, length, temp;
    char  lbuff[8], msgbuff[MAXMESG];

/*  set up defaults  */

    rmenv.d_linelgh = MAXMESG;
    rmenv.d_window = WINDOW;

/*  start logging and clear echo on our standard input  */

    result = d_strtlog(logfile, tranfile, debugf, 0);
    if (result < 0)
      return(result);

    result = d_setport(PORTMODE);
    if (result < 0)
      return(result);

/*  the slave sends the first RUN message  */

    result = d_initsend();
    if (result < 0)
      return(result);

/*  and waits for a matching one from the master  */

    result = d_initrecv();
    if (result < 0)
      return(result);

    return(OK);
    }




/*
 *     routine which is called by the slave to sop the operation of the
 *     protocol.
 */

dlstop()
    {
    register int  result;

    result = d_rstport();
    if (result < 0)
      return(result);

    d_log("slave stopping");
    return(OK);
    }
