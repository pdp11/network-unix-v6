/*	sys.c	4.3	81/03/08	*/

/*
 * Indirect driver for controlling tty.
 */
#include "../h/param.h"
#include "../h/systm.h"
#include "../h/conf.h"
#include "../h/dir.h"
#include "../h/user.h"
#include "../h/tty.h"
#include "../h/proc.h"

/*ARGSUSED*/
syopen(dev, flag)
{

	if(u.u_ttyp == NULL || (u.u_procp->p_flag&SDETACH)) {
		u.u_error = ENXIO;
		return;
	}
	(*cdevsw[major(u.u_ttyd)].d_open)(u.u_ttyd, flag);
}

/*ARGSUSED*/
syread(dev)
{

	if (u.u_procp->p_flag&SDETACH) {
		u.u_error = ENXIO;
		return;
	}
	(*cdevsw[major(u.u_ttyd)].d_read)(u.u_ttyd);
}

/*ARGSUSED*/
sywrite(dev)
{

	if (u.u_procp->p_flag&SDETACH) {
		u.u_error = ENXIO;
		return;
	}
	(*cdevsw[major(u.u_ttyd)].d_write)(u.u_ttyd);
}

/*ARGSUSED*/
syioctl(dev, cmd, addr, flag)
caddr_t addr;
{

	if (u.u_procp->p_flag&SDETACH) {
		u.u_error = ENXIO;
		return;
	}
	(*cdevsw[major(u.u_ttyd)].d_ioctl)(u.u_ttyd, cmd, addr, flag);
}
