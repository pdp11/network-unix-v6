# include  "rm.h"
# include  "rmerr.h"




dlresp(okcode, msgbuff, lenptr)
  char  okcode, *msgbuff;
  int  *lenptr;
    {
    register int  length;
    char  code;

    d_dbglog("dlresp:  looking for code '%c'", okcode);

    while (1)
      {
      length = dlread(msgbuff, &code);

      if (length < 0)
        return(length);

      if (code == okcode)
        {
        *lenptr = length;
        return(1);
        }

      if (code == ERROR)
        {
        *lenptr = length;
        return(0);
        }
      }
    }
