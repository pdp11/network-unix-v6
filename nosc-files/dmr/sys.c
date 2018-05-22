#
/*
 */

/*
 *	indirect driver for controlling tty.
 */
#include "param.h"
#include "conf.h"
#include "user.h"
#include "tty.h"
#include "proc.h"

syopen(dev, flag)
{

	if(u.u_ttyp == NULL) {
		u.u_error = ENXIO;
		return;
	}
	(*cdevsw[u.u_ttyd.d_major].d_open)(u.u_ttyd, flag);
}

syread(dev)
{

	(*cdevsw[u.u_ttyd.d_major].d_read)(u.u_ttyd);
}

sywrite(dev)
{

	(*cdevsw[u.u_ttyd.d_major].d_write)(u.u_ttyd);
}

sysgtty(dev, flag)
{

	(*cdevsw[u.u_ttyd.d_major].d_sgtty)(u.u_ttyd, flag);
}
