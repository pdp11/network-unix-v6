#
#define	PHYS	0
#define	KDAT	1
#define	MEOF	2
#define	KINS	3
#define	KISA0	0172340

/*
 *	Memory special file
 *	minor device 0 is physical memory
 *	minor device 1 is kernel data space
 *	minor device 2 is EOF/RATHOLE
*	minor device 3 is kernel instruction space  (PBJ)
 */

#include "param.h"
#include "user.h"
#include "conf.h"
#include "seg.h"

#ifndef CPU70
#ifndef CPU45
#define CPU40	/* model 40 kernel is default */
#endif not CPU45
#endif not CPU70

mmread(dev)
{
	register c, bn, on;
	int a, d;

	if(dev.d_minor == MEOF)
		return;
	do {
		bn = lshift(u.u_offset, -6);
		on = u.u_offset[1] & 077;
		spl7();
		a = UISA->r[0];
		d = UISD->r[0];
		UISD->r[0] = 077406;
		switch (dev.d_minor) {	/* PBJ */

		default:
			u.u_error = ENXIO;
			return;

		case PHYS:
			UISA->r[0] = bn;
			break;

		case KINS:
#ifndef CPU40		/* model 40 doesn't have split I/D */
			UISA->r[0] = KISA0->r[(bn>>7)&07] + (bn & 0177);
			break;
#endif not CPU40

		case KDAT:
			UISA->r[0] = (ka6-6)->r[(bn>>7)&07] + (bn & 0177);
		}
		c = fuibyte(on);
		UISA->r[0] = a;
		UISD->r[0] = d;
		spl0();
	} while(u.u_error==0 && passc(c)>=0);
}

mmwrite(dev)
{
	register c, bn, on;
	int a, d;

	if(dev.d_minor == 2) {
		c = u.u_count;
		u.u_count = 0;
		u.u_base =+ c;
		dpadd(u.u_offset, c);
		return;
	}
	for(;;) {
		bn = lshift(u.u_offset, -6);
		on = u.u_offset[1] & 077;
		if ((c=cpass())<0 || u.u_error!=0)
			break;
		a = UISA->r[0];
		d = UISD->r[0];
		spl7();
		UISD->r[0] = 077406;
		switch (dev.d_minor) {	/* PBJ */

		default:
			u.u_error = ENXIO;
			return;

		case PHYS:
			UISA->r[0] = bn;
			break;

		case KINS:
#ifndef CPU40
			UISA->r[0] = KISA0->r[(bn>>7)&07] + (bn & 0177);
			break;
#endif not CPU40

		case KDAT:
			UISA->r[0] = (ka6-6)->r[(bn>>7)&07] + (bn & 0177);
		}
		suibyte(on, c);
		UISA->r[0] = a;
		UISD->r[0] = d;
		spl0();
	}
}
