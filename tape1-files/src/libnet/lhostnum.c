#define	LIBN
#include "netlib.h"
#include "sys/netparam.h"

/*
 * return network address as long host number (for NCP kernel)
 */
long
lhostnum(addr)
netaddr	addr;
{
	union lhost host;

	host.lhost1.h_net = net_part(addr);
	host.lhost1.h_hoi = hoi_part(addr);
	host.lhost1.h_imp0 = imp_part(addr) & _NABM;
	host.lhost1.h_imp1 = (imp_part(addr) >> _NABW) & _NABM;
	return host.lhst;
}
