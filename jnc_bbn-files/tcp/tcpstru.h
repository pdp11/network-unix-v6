#
/* Port name declarations */
# define CMDUSRNM  "/dev/net/cmdusrxxx"    /* user cmd  port name prototype */
# define CMDTCPNM  "/dev/net/cmdtcpxxx"    /* tcp cmd   port name prototype */
# define SENDNM    "/dev/net/datsndxxxxxx" /* send data port name prototype */
# define RECVNM    "/dev/net/datrcvxxxxxx" /* recv data port name prototype */
# define EAR       "/dev/net/earx"          /* ear port name */

# define PRTPREFIX      15      /* number of chars in port name prefix */

/* the following are the names for the absolute version

/dev/net/cmdusrxxx
/dev/net/cmdtcpxxx
/dev/net/datsndxxxxxx
/dev/net/datrcvxxxxxx
/dev/net/earx

PRTPREFIX = 15

*/

/* TCP Library error return codes */
# define EEARPRT        1       /* open of TCP ear port failed */
# define ERSPPRT        2       /* can't create tcp command port */
# define ECMDPRT        3       /* can't open user command port */
# define ETCPNRDY       4       /* TCP not responding */
# define ETCPNOPN       5       /* TCP cannot open tcp port */
# define ESNDPRT        6       /* error on send data port */
# define ERCVPRT        7       /* error on recv data port */
# define EUSRC          8       /* too many connections for this proc */
# define ENOSPC         9       /* not enough space in send port */
# define ENODATA        10      /* no data in recv port */
# define EBADRESP       11      /* error on TCP command port */
# define ENOCHNG        12      /* no change notices */
# define ESYSC          13      /* too many connections system wide */
# define ENOBUFS        14      /* tcp (or lib) is out of buffer space */
# define EILLS          15      /* illegal local port specification */
# define EUNAUTH        16      /* unauthorized parameter */
# define EUNSPEC        17      /* unspecified parameter in CAB */
# define ENOCONN        18      /* connection doesn't exist */
# define EBADPARAM      19      /* bad tcplib call parameter */
# define EMVSPT         20      /* Move: bad SPT in accepting TCB */
# define EMVSTATE       21      /* Move: connection not established */
# define ECMDCLS        22      /* tcp closed cmd port */

/* Command Port Message Opcodes */
# define OINIT          1       /* init opcode */
# define OABORT         2       /* abort opcode */
# define OURGENT        3       /* urgent opcode */
# define ORQSTAT        4       /* request status opcode */
# define OOPEN          5       /* open opcode */
# define OCLOSE         6       /* close opcode */
# define OSTOP          7       /* stop opcode */
# define ORESUME        8       /* resume opcode */
# define OMOVE          9       /* move opcode */
# define OACK           10      /* ack opcode */
# define OERR           11      /* error opcode */
# define OCHNG          12      /* change notice opcode */
# define OPROCNR        13      /* foreign process not responding */
# define OPROCR         14      /* foreign process responding again */
# define ORESET         15      /* connection reset */
# define OSTAT          16      /* TCP internal status opcode */
# define OREJECT        17      /* foreign TCP rejecting */
# define OSECERR        18      /* data security out of range */
# define OPRECCHNG      19      /* send precedence changed upward */
# define ONETMSG        20      /* net message arrived */
# define OUPDATE        21      /* update utcb parameters (move) */
# define OFLAKY         22      /* update flakeway parameters */

/* Connection States */
# define CLOSED         1
# define OPEN           2
# define SYNRECD        3
# define SYNSENT        4
# define ESTAB          5
# define FINWAIT        6
# define CLOSEWAIT      7
# define CLOSING        8

/* Internet option opcodes */
# define OPTSPT         2

/* Misc constants */
# define RBUFFSIZE     257      /* TCB data buffer size */
# define CMDTCPSIZE     64      /* max bytes in a tcp cmd msg */
# define CMDUSRSIZE     64      /* max bytes in a usr cmd msg */
# define HDRLNGTH       8       /* # bytes in port message hdr */
# define BSLLNGTH       16      /* BSL length in bytes */
# define INLNGTH        24      /* INL length in bytes */
# define TCPLNGTH       20      /* TCP header length in bytes */
# define NUMBTCC        4       /* number of TCCs allowed in Open */
# define AWTLENGTH      50      /* length of await buffer */
# define WORKSIZE       100     /* size of work list */
# define FDSSIZE        50      /* size of file descriptor blocks */
# define NUMBTCBS       10      /* number of allowed connections */
# define PORTCNT        16      /* size of port selector bit map */
# define TCPOTSZ        700     /* size of tcp output buffer */
# define TCPINSZ        700     /* size of tcp input buffer */
# define PORTSIZE       1024    /* size of Rand Port */
# define INVERNO        4       /* internet header version number */
# define TCPVERNO       6       /* TCP version number */
# define MINPREC        0       /* default minimum receive precedence */
# define MAXPREC        15      /* default maximum receive precedence */
# define NSTOP          4       /* Stop/Resume table size */
# define BASEPORT       01000   /* base of locally assigned port #s */
# define NUMNET         25      /* max number of foreign nets */
# define NUMTCPTBL      7       /* number of "tcptable" first line entries */

/* Opcodes for work list */
# define NETINHP        1
# define NETOUTHP       2
# define NETINLP        3
# define NETOUTLP       4
# define USERINHP       5
# define USEROUTHP      6
# define USERINLP       7
# define USEROUTLP      8
# define USRCMD         9
# define TCPCMD         10
# define EARIN          11
# define TTYIN          12
# define TTYOUT         13

/* TCP control bit definitions */
# define FIN            01
# define SYN            02
# define RST            04
# define EOL            010
# define ACK            020
# define URG            040

/* TCB Flags field definitions */
# define XURGSEND         01     /* urgent data to be sent */
# define XURGRECV         02     /* incoming urgent condition */
# define XSLOWRETN        04     /* entered slow transmition mode */
# define XSENDFIN        010     /* send FIN after all data */
# define XABTCONN        020     /* abort the connection */
# define XNOMOSEG        040     /* send no more segments */
# define XREJECT        0100     /* foreign TCP rejecting */
# define XDISCDATA      0200     /* discard user data */
# define XHNORESP       0400     /* foreign host not responding */
# define XRTNDATA      01000     /* don't send data unless retran */
# define XRETRANOK     02000     /* retransmit data */

/* various net and gateway information */
# define ARPANET        10      /* arpanet net number */
# define RCCNET          3      /* rcc net number */
# define PTIPGTWY     0205      /* rcc to arpanet gateway (133 decimal) */


/* Port Message Format Masks */


/* general message mask */

struct PMask1
{  int YOpCode;
   int YHandle;
   int YMisc;
   int YMisc1;
 };


/* Update UTCB parameters */

/* tcp -> lib: update parameters */

struct MUpdate
{  int ZOpCode;            /* opcode */
   int ZHandle;              /* connection index */
   int ZConnIndex;           /* new tcp connection index */
   long ZSndCnt;            /* new send data count */
   long ZRcvCnt;             /* new recv data count */
 };


/* Urgent */

/* lib -> tcp: send an urgent */
/* tcp -> lib: go into urgent mode */

struct MUrgent
{  int UOpCode;           /* opcode */
   int UHandle;           /* connection index */
   long UByteNo;          /* byte number at which urgent is to end */
 };


/* Open Connection */

/* lib -> tcp: open (listen for) a connection */

struct  MOpen
{  int OpCode;             /* opcode */
   int OHandle;            /* tcp connection index */
   int OpenMode;           /* 1 = Listen only, 2 = Accept only */
   char Net;               /* foreign network */
   char HostH;             /* foreign host - high byte */
   int HostL;              /* foreign host - low bytes */
   int SrcPrt;             /* local port */
   int DstPrt;             /* destination port */
   char SendPrec;          /* send precedence */
   char RecvPrec;          /* minimum recv precedence */
   char MaxSecur;          /* maximum security level of connection */
   char AbsSecur;          /* absolute security level of connection */
   char NumbTcc;           /* number of TCCs following */
   char OTcc[NUMBTCC];     /* transmission control codes */
 };


/* Close Connection */

/* lib -> tcp: I am finished sending data */

struct MClose
{  int COpCode;            /* opcode */
   int CHandle;            /* tcp connection index */
   long CFinNum;           /* byte number where FIN should occur */
 };


/* Status */

/* tcp -> lib: connection status information */
/* size of array is 48 bytes */

struct MStatus
{  int SOpCode;         /* opcode */
   int SState;          /* state of connection (see below) */
   char SNet;           /* foreign net */
   char SHost;          /* foreign host */
   int SImp;            /* foreign imp */
   int SLocPrt;         /* local port */
   int SForPrt;         /* foreign port */
   char SScMxOt;        /* max security to net */
   char SScMnOt;        /* min security to net */
   char SScMxIn;        /* max security from net */
   char SScMnIn;        /* min security from net */
   char SSndPrec;       /* send precedence */
   char SRcvPrec;       /* recv precedence */
   char SNTcc;          /* size of TCC list */
   char STcc[10];       /* list of TCCs */
   int SSndWind;        /* send window */
   int SRcvWind;        /* receive window */
   int SAckData;        /* # of bytes awaiting ack */
   int SSndBuff;        /* Data in send buffer */
   int SRecBuff;        /* Data in receive buffer */
   int SSegRecv;        /* # of segment received */
   int SDupSeg;         /* # segments received with duplicate data */
   int SBusyRecv;       /* # discarded segments */
   int SRetran;         /* # retransmitted segments */
 };


/* Send Data */

/* lib -> tcp: send user data to tcp on send port */

struct MSend
{  char SnSecur;           /* security level for this data */
   char SnFlags;           /* eol flag, urgent flag */
   int SnByteCnt;          /* amount of data in next msg */
 };


struct TcpTcb
{  char ConnHandle;     /* local connection index */
   char State;          /* state of connection */

   /* see above for flag definitions */
   int Flags;           /* various work-to-do flags */
   char TMode;          /* mode of TcpOpen */
   char ProcID;         /* user ID */
   int TCPCmdPort;      /* file desc - TCP to user command port */
   int UserCmdPort;     /* file desc - user to TCP command port */
   int SendPort;        /* file desc - data from user port */
   int RecvPort;        /* file desc - data to user port */

   /* Open Parameters */
   char ScMaxOut;       /* max security user to net */
   char ScMinOut;       /* min security user to net */
   char ScMaxIn;        /* max security net to user */
   char ScMinIn;        /* min security net to user */
   char ATPrec;         /* maximum user precedence allowed */
   char SndPrec;        /* send precedence */
   char MnRcvPrec;      /* minimum receive precedence */
   char MxRcvPrec;      /* maximum receive precedence */
   char TCCCnt;         /* count of following TCCs */
   char TccList[NUMBTCC];  /* list of TCCs */
   char OpnNet;         /* foreign network */
   char OpnHstH;        /* foreign host - high byte */
   int OpnHstL;         /* foreign host - low word */
   int OpnPort;         /* foreign port */

   /* Binary Segment Leader */
   int PrecSec;         /* precedence, security fields */
   int TccCcc;          /* transmission control code, command control code */
   int SecTcc;          /* redundant security, TCC fields */
   int FnTcp;           /* foreign TCP ID */
   int VerSegNo;        /* version, s-segment number */
   int BSLSave[3];      /* unused BSL fields */

   /* InterNet Header */
   char TypeService;    /* type of service */
   char VerHdr;         /* 4 bit version, 4 bit header length */
   int SegLength;       /* length of segment in octets */
   int SegID;           /* segment identification */
   int FragOffset;      /* 3 bits flags, 13 bits fragment offset */
   char Protocol;       /* next layer protocol identifier */
   char TimeToLive;     /* time to live... */
   int INChkSum;        /* internet header checksum */
   char SrcHstH;        /* source host - high byte */
   char SrcNet;         /* source network */
   int SrcHstL;         /* source host - lower two bytes */
   char DstHstH;        /* destination host - high byte */
   char DstNet;         /* destination network */
   int DstHstL;         /* destination host - lower two bytes */
   char Options[4];     /* option field */

   /* TCP Header */
   int SrcePort;        /* source port */
   int DestPort;        /* destination port */
   long SeqNo;          /* sequence number */
   long AckNo;          /* acknowledge number */
   int TCPFlags;        /* 4 bits data offset, rest are flags */
   int Window;          /* flow control window */
   int TCPChkSum;       /* checksum of TCP & parts of IN header */
   int UrgentPtr;       /* urgent pointer */

   /* miscellaneous state information */
   long LeftSeq;        /* earliest unacknowledged octet seq no. */
   long SendSeq;        /* sequence # next to send */
   long MaxSend;        /* seq no. of last data sent + 1 */
   int DataHd;          /* octet last acknowledged */
   int DataNxt;         /* next octet to send */
   int DataTl;          /* last data item in send buffer */
   int SndWindow;       /* send window size in octets */
   char RetranTm;       /* retransmission time out */
   char NumRetran;      /* no. of retrans before good ACK */
   int RetrSw;          /* 1 => slow mode */
   long SndUrgNo;       /* urgent number for send */
   long RcvUrgNo;       /* urgent number for receive */
   int DataState;       /* state of data port from user */
   int DataLeft;        /* remaining data in input port */
   int LastAckTime;     /* time of last acknowledge - seconds */
   long SndISN;         /* initial send seq number */
   long RcvISN;         /* initial recv seq number */
   long SndFSN;         /* final send seq number */
   int SndSpc1;         /* data in send port */
   int RcvSpc1;         /* space in recv port */

   /* measurement data */
   int SegRecv;         /* # of segments recvd */
   int DupRecv;         /* # of duplicate segments received */
   int BusyRecv;        /* # of discarded segments */
   int RetranSeg;       /* # of retransmitted segments */

   /* Linkage area */
   int *NxtActTcb;      /* ptr to next active TCB */
   int *NxtRetran;      /* ptr to next retransmit TCB */
   int *LstRetran;      /* ptr to last retransmit TCB */

   /* Retransmission Ring Buffer */
   char RetranBuff[RBUFFSIZE];  /* input buffer/retransmission buffer */
   char FlagBuff[RBUFFSIZE];    /* flags associated with data */

 };

struct WorkBlk        /* a block in the work list */
{  char PortFlags;            /* misc work flags */
   char PortFds;              /* file descriptor of port */
   struct TcpTcb *TCBPtr;     /* ptr to TCB (also is fds for tcp) */
   struct WorkBlk *LastWork;  /* last node link address */
   struct WorkBlk *NextWork;  /* next node link address */
 };

struct FdsBlk         /* picture of an FdsBffer entry */
{  char BuffFlags;      /* misc work flags */
   char BuffFds;        /* fds for port */
   int *BuffTcbPtr;     /* ptr to relevent TCB, temp for fds usr cmd */
 };

struct      /* used to break apart longs for printing */
{  int bb[2];
 } *z;


struct PortLdr
{  int UsrPID;
   int GroupID;    /* file table pointer */
   char RealUserID;
   char EffUserID;
   int DCount;
 };
