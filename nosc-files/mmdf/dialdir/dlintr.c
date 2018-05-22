# include  "rm.h"
# include  "rmerr.h"
# include  "rmenv.h"




  extern struct rmenv  rmenv;  /*  protocol environment structure  */




dlintr(reason)
  char  *reason;
    {

    d_dbglog("dlintr:  reason '%s'", reason);

    cpstr(reason, rmenv.d_intmesg);
    rmenv.d_tranint = 1;
    }
