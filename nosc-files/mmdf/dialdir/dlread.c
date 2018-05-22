# include  "rm.h"
# include  "rmerr.h"




dlread(buffer, code)
  char  *buffer, *code;
    {
    register int  nbytes, count;
    char  msgbuff[MAXMESG];

    nbytes = d_getmsg(msgbuff, NOTACK);
    if (nbytes <= 0)
      return(nbytes);

    count = d_fromgrave(&msgbuff[1], buffer);
    *code = msgbuff[0];
    return(count);
    }
