#
/* Kludge Teletype: /dev/tty
 * It accesses the controlling teletype.
 *
 *	It doesn't work yet because I haven't looked into how to
 *	handle close--I may or may not want to close the associated
 *	tely
 */ 

#include "/usr/sys/h/param.h"
#include "/usr/sys/h/user.h"
#include "/usr/sys/h/conf.h"
#include "/usr/sys/h/proc.h"
#include "/usr/sys/h/tty.h"

ktopen(dev,flag)
{
	register struct proc *p;

	if(u.u_procp->p_pgrp==0)
	{
		u.u_error = ENXIO;
		return;
	}

	dev = u.u_ttyd;
	(*cdevsw[dev.d_major].d_open)(dev,flag);
}

ktclose(dev)
{
	dev = u.u_ttyd;
	(*cdevsw[dev.d_major].d_close)(dev);
}

ktread(dev)
{
	dev = u.u_ttyd;
	(*cdevsw[dev.d_major].d_close)(dev);
}

ktwrite(dev)
{
	dev = u.u_ttyd;
	(*cdevsw[dev.d_major].d_write)(dev);
}
