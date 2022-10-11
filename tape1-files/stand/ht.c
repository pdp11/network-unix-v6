/*	ht.c	4.5	81/03/22	*/

/*
 * TM03/TU?? tape driver
 */

#include "../h/htreg.h"
#include "../h/param.h"
#include "../h/inode.h"
#include "../h/pte.h"
#include "../h/mbareg.h"
#include "saio.h"
#include "savax.h"

short	httypes[] =
	{ MBDT_TM03, MBDT_TE16, MBDT_TU45, MBDT_TU77, 0 };

#define	MASKREG(reg)	((reg)&0xffff)

htopen(io)
	register struct iob *io;
{
	register int skip;
	register struct htdevice *htaddr = (struct htdevice *)mbadrv(io->i_unit);
	int i;

	for (i = 0; httypes[i]; i++)
		if (httypes[i] == (htaddr->htdt&MBDT_TYPE))
			goto found;
	_stop("not a tape\n");
found:
	mbainit(UNITTOMBA(io->i_unit));
	htaddr->htcs1 = HT_DCLR|HT_GO;
	htstrategy(io, HT_REW);
	skip = io->i_boff;
	while (skip--) {
		io->i_cc = -1;
		while (htstrategy(io, HT_SFORW))
			;
		DELAY(65536);
		htstrategy(io, HT_SENSE);
	}
}

htclose(io)
	register struct iob *io;
{

	htstrategy(io, HT_REW);
}

htstrategy(io, func)
	register struct iob *io;
	int func;
{
	register int den, errcnt, ds;
	short fc;
	register struct htdevice *htaddr =
	    (struct htdevice *)mbadrv(io->i_unit);

	errcnt = 0;
retry:
	den = HTTC_1600BPI|HTTC_PDP11;
	htquiet(htaddr);
	htaddr->htcs1 = HT_DCLR|HT_GO;
	htaddr->httc = den;
	htaddr->htfc = -io->i_cc;
	if (func == HT_SREV) {
		htaddr->htfc = -1;
		htaddr->htcs1 = HT_SREV|HT_GO;
		return (0);
	}
	if (func == READ || func == WRITE)
		mbastart(io, func);
	else
		htaddr->htcs1 = func|HT_GO;
	htquiet(htaddr);
	ds = htaddr->htds;
	if (ds & HTDS_TM) {
		htaddr->htcs1 = HT_DCLR|HT_GO;
		return (0);
	}
	if (ds & HTDS_ERR) {
		printf("ht error: ds=%b, er=%b\n",
		    MASKREG(htaddr->htds), HTDS_BITS,
		    MASKREG(htaddr->hter), HTER_BITS);
		htaddr->htcs1 = HT_DCLR|HT_GO;
		if (errcnt == 10) {
			printf("ht: unrecovered error\n");
			return (-1);
		}
		errcnt++;
		htstrategy(io, HT_SREV);
		goto retry;
	}
	if (errcnt)
		printf("ht: recovered by retry\n");
	fc = htaddr->htfc;
	return (io->i_cc+fc);
}

htquiet(htaddr)
	register struct htdevice *htaddr;
{
	register int s;

	do
		s = htaddr->htds;
	while ((s & HTDS_DRY) == 0);
}
