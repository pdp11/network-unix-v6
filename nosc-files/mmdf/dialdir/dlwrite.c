# include  "rm.h"
# include  "rmerr.h"




dlwrite(text, nbytes)
  char  *text;
  int  nbytes;
    {
    register int  result;

/*  check for negative counts.  0 counts send an EOT  */

    if (nbytes < 0)
      return(EUSER);

    if (nbytes == 0)
      {
      result = dlsend(EOT, 0, 0);
      return(result);
      }

/*  otherwise, send DATA type  */

    result = dlsend(DATA, text, nbytes);
    return(result);
    }
