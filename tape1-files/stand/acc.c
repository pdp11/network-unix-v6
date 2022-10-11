#define BBNNET

#include "../h/param.h"
#include "../bbnnet/net.h"
#include "../h/inode.h"
#include "../bbnnet/imp.h"
#include "../h/upreg.h"
#include "../h/pte.h"
#include "../h/ubareg.h"
#include "../h/accreg.h"
#include "saio.h"
#include "savax.h"


	register struct impregs *impaddr =
	    (struct impregs *)ubamem(io->i_unit, 0767700);

imp_write(buf, len)
register struct imp *buf;
register len;
{
	register uba, error;
	struct iob io;

	/* set up uba mapping */

	io.i_ma = (char *)buf;
	io.i_cc = len;
	uba = ubasetup(&io, 0);

	/* set IMP device regs and start i/o */

	impaddr->spo = (int)uba;
	impaddr->owcnt = -((io.i_cc + 1) >> 1);
	impaddr->ostat = ((short) ((uba & 0x30000) >> 12) + oenlbit + ogo);

	/* wait for completion */

	while ((impaddr->ostat & odevrdy) == 0);

	/* check if error */

	error = impaddr->ostat & (ocomperr|otimout|omrdyerr|obusback|ogo);

	ubafree(uba);

	return(error);
}

imp_read(buf, len)
register struct imp *buf;
register len;
{
	register uba, error;
	struct iob io;

	/* set up uba mapping */

	io.i_ma = (char *)buf;
	io.i_cc = len;
	uba = ubasetup(&io, 0);

	/* set up IMP device regs and start i/o */

	impaddr->spi = (int)uba;
	impaddr->iwcnt = -(io.i_cc >> 1);
	impaddr->istat = (ihmasrdy | iwren | ((uba & 0x30000)>>12) | igo);

	/* wait for completion */

	while ((impaddr->istat & idevrdy) == 0);

	/* check if error */

	error = ( impaddr->istat & (iendmsg|icomperr|itimout|imntrdy|imrdyerr|igo)) |
		(~impaddr->istat & (ihstrdy|iwren|ihmasrdy));

	ubafree(uba);
	return(error);
}

imp_init()                              /* init the imp interface */
{	
	
	/* reset the imp interface */

	impaddr->istat = 2;
        impaddr->ostat = 2;    /* WAS 2??? */
	impaddr->ostat = obusback;      /* reset host master ready */
	impaddr->ostat = 0;

	impaddr->istat = ihmasrdy; /* close relay */
	while (!(impaddr->istat&ihstrdy) || (impaddr->istat&(imrdyerr|imntrdy)))
	      {
		impaddr->istat = ihmasrdy;      /* keep turning imrdyerr off*/
		DELAY(200);
	      }
	DELAY(200);
}


