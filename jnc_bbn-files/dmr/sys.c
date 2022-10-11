#
/*
 */

/*
 *	indirect driver for controlling tty.
 */
#include "../h/param.h"
#include "../h/conf.h"
#include "../h/user.h"
#include "../h/tty.h"
#include "../h/proc.h"

syopen(xdev, flag)
{
	int dev;

	if(dev = syttyp())
	(*cdevsw[dev.d_major].d_open)(dev, flag);
}

syread(xdev)
{
	int dev;

	if(dev = syttyp())
	(*cdevsw[dev.d_major].d_read)(dev);
}

sywrite(xdev)
{
	int dev;

	if(dev = syttyp())
	(*cdevsw[dev.d_major].d_write)(dev);
}

sysgtty(xdev, flag)
{
	int dev;

	if(dev = syttyp())
	(*cdevsw[dev.d_major].d_sgtty)(dev, flag);
}

syttyp()
{
	register dev;

	if(u.u_ttyp == NULL) {
		u.u_error = ENXIO;
		dev = NULL;
	}
	else
		dev = u.u_ttyd;
	return(dev);
}
