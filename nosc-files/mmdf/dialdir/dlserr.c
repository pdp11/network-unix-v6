# include  "rm.h"
# include  "rmerr.h"




dlserr(message)
  char  message;
    {
    register int  result;

    result = dlsend(ERROR, message, length(message));
    return(result);
    }
