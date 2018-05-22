# include  "rm.h"
# include  "rmerr.h"
# include  "rmenv.h"




  extern struct rmenv  rmenv;       /*  protocol environment structure  */




dlsend(code, text, nbytes)
  char  code, *text;
  int  nbytes;
    {
    register int  tcount, nleft, nused;
    char  msgbuff[MAXMESG], *in, *out;
    int  result;

    d_dbglog("dlsend:  code '%c'  text '%s'  nbytes %d", code, text, nbytes);

/*  negative counts are errors  */

    if (nbytes < 0)
      return(EUSER);

/*  if there's no text, then our job is easy  */

    if (text == 0)
      {
      result = d_sndmsg(code, 0);
      return(result);
      }

/*  otherwise build a single packet.  there may be extra input characters  */

    in = text;
    out = msgbuff;
    nused = 0;
    nleft = rmenv.d_linelgh - HEADLNGH;

    while (nleft && (nused < nbytes))
      {
      tcount = d_tograve(in++, out);

      if (tcount > nleft)
        break;

      nleft =- tcount;
      out =+ tcount;
      nused++;
      }

    *out = '\0';
    result = d_sndmsg(code, msgbuff);
    return(result);
    }
