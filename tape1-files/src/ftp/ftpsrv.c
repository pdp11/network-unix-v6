#include "ftp.h"                /* Server FTP daemon */


main ()
{
	struct net_stuff stuff;
	register struct net_stuff *nethandle;
	char *us, *them;

	/* detach from parent */
	if (fork())
		exit(0);
	nethandle = &stuff;
	NetInit(nethandle);
	signal (SIGURG, 1);           /* ignore INS interrupts */
	dup2 (1, 2);                   /* make log file standard error */
	while (1)
	{
		net_listen(nethandle);
		if (spawn () == 0)
		{
			us = thisname();
			get_stuff(nethandle);
			them = hostname(GetHostNum(nethandle));
			ftpsrv_plumb(nethandle);
			execl (NET, "srvrftp", them, us, 0);
			execl (ETC, "srvrftp", them, us, 0);
			cmderr(-1,"can execl neither %s nor %s\n", NET, ETC);
			exit (1);
		}
		net_close (nethandle);
	}
}
