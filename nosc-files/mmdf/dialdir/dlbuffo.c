# include  "rm.h"
# include  "rmerr.h"
# include  "rmenv.h"




/*  structure used by 'dlbuffo' to assemble messages  */

  struct
    {
    char  b_msgbuff[MAXMESG];     /*  buffer for message being assembled  */
    char  *b_loadpt;              /*  pointer to next available slot in  */
                                  /*  b_msgbuff                          */
    int  b_numleft;               /*  number of open slots in b_msgbuff  */
    }  d_buffo;

  extern struct rmenv  rmenv;    /*  protocol environment structure  */




/*
 *     this routine transmits a buffer of data by breaking it up into
 *     packets.  any partial packets are buffered, and must be cleared
 *     by 'dlflush' in order to be transmitted.  an 'nbytes' value of 0
 *     causes a flush opoeration, followed by an EOT.  The number of
 *     of input bytes processed without error is retuned.  a user error
 *     is possible if the 'nbytes' value is negative.
 */

dlbuffo(buffer, nbytes)
  char  *buffer;
  int  nbytes;
    {
    register int  tcount, nused;
    register char  *in;
    int  result;

    d_dbglog("dlbuffo:  nbytes %d  buffer '%s'", nbytes, buffer);

/*  a negative count is an error.  a count of 0 does a flush followed by  */
/*  an EOT                                                                */

    if (nbytes < 0)
      return(EUSER);

    if (nbytes == 0)
      {
      result = dlflush();
      if (result < 0)
        return(result);

      result = d_sndmsg(EOT, 0);
      return(result);
      }

/*  now assemble lines and ship them off.  first make sure we start up ok  */

    if (d_buffo.b_numleft == 0)
      {
      d_buffo.b_numleft = rmenv.d_linelgh - HEADLNGH;
      d_buffo.b_loadpt = d_buffo.b_msgbuff;
      }

    nused = 0;
    in = buffer;

    while (1)
      {

/*  build up an encoded line  */

      while (d_buffo.b_numleft && (nused < nbytes))
        {
        tcount = d_tograve(in++, d_buffo.b_loadpt);

        if (tcount > d_buffo.b_numleft)
          {
          in--;
          d_buffo.b_numleft = 0;
          break;
          }

        nused++;
        d_buffo.b_numleft =- tcount;
        d_buffo.b_loadpt =+ tcount;
        }

/*  if we've got a full line, transmit it  */

      if (d_buffo.b_numleft == 0)
        {
        *d_buffo.b_loadpt = '\0';
        d_buffo.b_loadpt = d_buffo.b_msgbuff;
        d_buffo.b_numleft = rmenv.d_linelgh - HEADLNGH;

        result = d_sndmsg(DATA, d_buffo.b_msgbuff);
        if (result < 0)
          return(result);
        }

      else
        return(nused);
      }
    }




/*
 *     this routine is called to force any partial packets to be transmitted.
 *     it returns with the result from a call to d_sndmsg.
 */

dlflush()
    {
    register int  result;

    if (d_buffo.b_numleft < (rmenv.d_linelgh - HEADLNGH))
      {
      *d_buffo.b_loadpt = '\0';
      d_buffo.b_loadpt = d_buffo.b_msgbuff;
      d_buffo.b_numleft = rmenv.d_linelgh - HEADLNGH;

      result = d_sndmsg(DATA, d_buffo.b_msgbuff);
      return(result);
      }

    return(OK);
    }
