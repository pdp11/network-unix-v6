/*
* TCP structures and definitions
*/

struct   CAB
{
   char Network;           /* Destination network */
   char HostHigh;          /* High byte of destination host */
   int HostLow;            /* Low two bytes of destination host */
   int SrcPort;            /* Local port number */
   int DstPort;            /* Destination port number */
   /* next 4 bytes: high bit on => specified */
   char PrecSend;          /* Send precedence level for data */
   char PrecRecv;          /* Minimum receive precedence level for data */
   char SecurMax;          /* Maximum security level of connection */
   char SecurAbs;          /* Absolute security level of connection */
   char TccCnt;            /* Count of following TCCs */
   char TCC[10];           /* Transmission control codes */
 };

struct UserTcb
{
   int ConnIndex;          /* TCP name for connection */
   int CNFlags;            /* Connection flags (see below) */
   char CNState;           /* New state of connection (see below) */
   char CNPrec;            /* New send precedence level */
   int CNMsg;              /* Net message - see below */
   int SendFds;            /* Send port file desc */
   int RecvFds;            /* Receive port file desc */
   int CmdFds;             /* From-TCP command port file desc */
   int ResidData;          /* Data remaining in msg in receive port */
   int SendCapac;          /* Guaranteed capacity of send port */
   int RecvCapac;          /* Guaranteed capacity of receive port */
   long SndCount;          /* Abs number of bytes sent */
   long RcvCount;          /* Abs number of bytes received */
   long UrgCount;          /* Byte count where urgent ends */
   int UInfo1;             /* User information word */
   int UInfo2;             /* User information word */
 };

struct CmdBlk
{  struct UserTcb *XPtr;

   /* Flags:
   bit 0 - connection state change
   bit 1 - urgent state
   bit 2 - send security level not in range
   bit 3 - foreign process not responding
   bit 4 - foreign process responding again
   bit 5 - net message
   bit 6 - connection reset
   bit 7 - foreign TCP rejecting
   bit 8 - send precedence level raised
   */

   int CFlags;
   int NewState;
   int NetMsg;
   int NewPrec;
 };

struct ConnStat
{
   int CState;          /* state of connection (see below) */
   char CNet;           /* foreign net */
   char CHost;          /* foreign host */
   int CImp;            /* foreign imp */
   int CLocPrt;         /* local port */
   int CForPrt;         /* foreign port */
   char CScMxOt;        /* max security to net */
   char CScMnOt;        /* min security to net */
   char CScMxIn;        /* max security from net */
   char CScMnIn;        /* min security from net */
   char CSndPrec;       /* send precedence */
   char CRcvPrec;       /* recv precedence */
   char CNTcc;          /* size of TCC list */
   char CTcc[10];       /* list of TCCs */
   int CSndWind;        /* send window */
   int CRcvWind;        /* receive window */
   int CAckData;        /* # of bytes awaiting ack */
   int CSndBuff;        /* Data in send buffer */
   int CRecBuff;        /* Data in receive buffer */
   int CSegRecv;        /* # of segment received */
   int CDupSeg;         /* # segments received with duplicate data */
   int CBusyRecv;       /* # discarded segments */
   int CRetran;         /* # retransmitted segments */
 };

/* Flags */

# define FSTATECHANGE 01
# define FURGENT      02
# define FSECTOOHIGH  04
# define FDEAD        010
# define FALIVE       020
# define FNETMSG      040
# define FRESET       0100
# define FREJECT      0200
# define FPRECCHNG    0400

/* Net message flags */

# define NNOHOST        01      /* foreign host not responding */


/* TCP Library error return codes */

# define EEARPRT        1       /* Open of TCP ear port failed */
# define ERSPPRT        2       /* Can't create tcp command port */
# define ECMDPRT        3       /* Can't open user command port */
# define ETCPNRDY       4       /* TCP not responding */
# define ETCPNOPN       5       /* TCP cannot open tcp port */
# define ESNDPRT        6       /* Error on send data port */
# define ERCVPRT        7       /* Error on recv data port */
# define EUSRC          8       /* Too many connections for this proc */
# define ENOSPC         9       /* Not enough space in send port */
# define ENODATA        10      /* No data in recv port */
# define EBADRESP       11      /* Error on TCP command port */
# define ENOCHNG        12      /* No change notices */
# define ESYSC          13      /* Too many connections system wide */
# define ENOBUFS        14      /* Tcp (or lib) is out of buffer space */
# define EILLS          15      /* Illegal local port specification */
# define EUNAUTH        16      /* Unauthorized parameter */
# define EUNSPEC        17      /* Unspecified parameter in CAB */
# define ENOCONN        18      /* Connection doesn't exist */
# define EBADPARAM      19      /* bad tcplib call parameter */
# define EMVSPT         20      /* Move: bad SPT in accepting TCB */
# define EMVSTATE       21      /* Move: connection not established */
# define ECMDCLS        22      /* tcp closed cmd port */

/* Connection States */

# define CLOSED         1
# define OPEN           2
# define SYNRECD        3
# define SYNSENT        4
# define ESTAB          5
# define FINWAIT        6
# define CLOSEWAIT      7
# define CLOSING        8
