#include "signal.h"
#include "errno.h"

/* flavors for setbf */
#define NetHandle       0
#define FileDescr       1

#define FTPOUT 1		/* fd of pipe in ftptty */
#define OUTSOCK 4		/* fd of net connection in ftpmain */
#define TTYMON  3               /* fd of pipe from ftptty in main  */
#define LINSIZ  250             /* Must be >= 2*argsiz +10 */

extern int errno;

/* Basic user-defined FTP parameters */

