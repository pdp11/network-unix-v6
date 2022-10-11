#define	BBN-VAX
#include "stdio.h"
#include "ctype.h"
#include "signal.h"
#ifdef BBN-VAX
#define VFORK vfork
#define VEXIT _exit
#endif BBN-VAX
#ifndef BBN-VAX
#define VFORK fork
#define VEXIT exit
#endif BBN-VAX
#define ERRFD   stderr       /* file descriptor for error output */
#define TNDM    242                 /*     function   */
#define TNEC    247
#define TNEL    248
#define TNWILL  251
#define TNWONT  252
#define TNDO    253
#define TNDONT  254
#define TNIAC   255

#define CEOR	0300	/* end of record for mode = text */
#define CEOF	0301	/* end of file for mode = text */

#define FTPTIMO 45
#define FTPSOCK 21

#ifdef TCP

#	define U4 0
#	define U5 1
#	define SIG_NETINT SIGURG
#	include "netlib.h"
#	include "con.h"
#	define PROTOPEN con
#	define PROTSTAT netstate
#	define	NET	"/etc/net/srvrftp"
#	define ETC     "/etc/net/tcp/srvrftp"
#	define tcpfile	 "/dev/net/tcp"
#	define netfile	 tcpfile
#	define PORT "port"
#	define QUIT "quit"
#	define MSRQ "msrq"
#	define MRCP "mrcp"
#	define MSND "msam"
#	define MSOM "msom"

typedef unsigned short portsock;		/* 16-bit data ports */
#define TOSOCK(a) (a)
#define ATOSOCK(a) atoi (a)
#endif TCP

#ifdef NCP

#	include "ncpopen.h"
#	define SIG_NETINT SIGINR
#	define NET     "/etc/net/ncp/srvrftp"
#	define ETC     "/etc/net/ncp/srvrftp"
#	define ncpfile "/dev/net/ncp"
#	define netfile	ncpfile

#	define ftprintr  "/etc/net/ftptty"
#	define ft2printr "/etc/net/ncp/ftptty"
#	define ftkeybd   "/etc/net/ftpmain"
#	define ft2keybd  "/etc/net/ncp/ftpmain"
#	define PROTOPEN ncpopen

#	define PORT "sock"
#	define QUIT "bye"
#	define MSRQ "xrsq"
#	define MRCP "xrcp"
#	define MSND "xsen"
#	define MSOM "xsem"
typedef unsigned long portsock;		/* 32-bit data sockets*/	1
#define TOSOCK(a) ((long) a)
#define ATOSOCK(a) atol(a)
#define U4 4
#define U5 5
#endif NCP
/*
 */

struct net_stuff {      /* structure contains useful information which i */
			/* would normally obtain through fstat() (in the */
			/* NCP), or tcp_stat() (in TCP).                 */

     int fds;			/* file-descriptor for the network	*/
     struct PROTOPEN np;	/* net structure			*/
#    ifdef TCP
	struct PROTSTAT ns;
#    endif TCP
 };

NetInit();
net_listen();
inherit_net();
ins();
net_open();
net_close();
ftp2_plumber();
netaddr GetHostNum();
net_read();
net_write();
