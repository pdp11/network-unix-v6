#define NETCONF
#include "../h/param.h"
#include "../h/accreg.h"
#include "../bbnnet/mbuf.h"
#include "../bbnnet/net.h"
#include "../bbnnet/ifcb.h"

extern int imp_output(), imp_rcv(), imp_raw(), acc_init(), acc_output();

/*
 * Someday this will be generated from the configuration file.
 */
struct ifcb ifcb[] = {

	/* interface 1 */
	{ FALSE, FALSE, FALSE, FALSE, FALSE, FALSE,
	  /*  device		      mtu   net */
	  0, makedev(ACCDEV,0), 0, 0, 1018, 0xa, 0, 0, 
	  NULL, NULL, NULL, 0, 0, NULL, NULL, NULL, NULL, 
	  imp_output, imp_rcv, imp_raw, acc_init, acc_output },

	/* interface 2 */
	{ FALSE, FALSE, FALSE, FALSE, FALSE, FALSE,
	  /*  device		      mtu   net */
	  0, makedev(ACCDEV,1), 0, 0, 1018, 0x3, 0, 0,
	  NULL, NULL, NULL, 0, 0, NULL, NULL, NULL, NULL,
	  imp_output, imp_rcv, imp_raw, acc_init, acc_output }
};
int nifcb = (sizeof ifcb/sizeof(struct ifcb));

struct if_localx locnet[] = {
	/* interface 1 */
	{ ipaddr(10,1,0,82), &ifcb[0] },
	/* interface 2 */
	{ ipaddr(3,1,0,8), &ifcb[1] }
};
int nlocnet = (sizeof locnet/sizeof(struct if_local));
