/* Global defines for TELNET library */

/* -------------------------- P A R A M S --------------------------- */
/*
 * Tunable parameters
 */
#define N_OPTIONS 35	/* Max options */
#define SB_SIZE   32	/* # Bytes to allow for subneg */
#define PROTMAX   3	/* # bytes needed to send out one protocol sequence */
#define RNETSIZE  128	/* Size of buffer used to read from net */
#define WNETSIZE  256	/* Size of buffer used to write to net */

/* -------------------------- C O N S T A N T S --------------------- */
/*
 * Constants needed by library and user program
 */

#ifndef NULL
#define NULL	  0
#endif

/* Values returned by test function */

#define CANT 0
#define CAN  1
#define ALREADY 2

/* Values returnable by user interface functions */

#define TELEOF -2
#define TELERR -1

/* Internal representation of WILL, WONT, DO, and DONT */

#define TN_DONT 0
#define TN_DO	1
#define TN_WILL 2
#define TN_WONT 3

#define NETCONN struct NetConn	/* For declarations */

/* -------------------------- P R I V A T E ------------------------- */
/*
 * Private declarations for the library. In practice, the user program
 * must see NetConn, and hence GBuffer and PBuffer, but the program
 * should not depend on the contents of NetConn.
 */

/*
 * Read buffer -- Bfill with data, use Bgetc to fetch chars
 */
struct GBuffer
{
    char   *GBptr;
    int     GBcnt;
};

#define Bfill(b,p,c)	{(b).GBptr = (p); (b).GBcnt = (c);}
#define Bgetc(b)	 (--(b).GBcnt>=0? (*(b).GBptr++ & 0377) : -1)
#define Bleft(b)	 ((b).GBcnt)

/*
 * Write buffer
 * Use Bsetbuf to init to empty buffer,
 * Bputc to fill, Baddr and Blen to get the filled buffer
 */
struct PBuffer
{
    char   *PBstart;	/* Base of buffer */
    int     PBsize;	/* Size of buffer */
    int     PBcnt;	/* Room left */
    char   *PBptr;	/* Position of next char */
};

#define Bsetbuf(b,p,c)	{(b).PBstart = (b).PBptr = (p); (b).PBsize = (b).PBcnt = (c); }
#define Bputc(x,b)	(--(b).PBcnt>=0? (*(b).PBptr++ = (x)) : -1)
#define Bfree(b)	((b).PBcnt)
#define Baddr(b)	((b).PBstart)
#define Blen(b) 	((b).PBsize - (b).PBcnt)
#define PBcopy(b1,b2)	{b2.PBstart=b1.PBstart;b2.PBsize=b1.PBsize;b2.PBcnt=b1.PBcnt;b2.PBptr=b1.PBptr;}

/*
 * Definition of connection block
 */
struct NetConn
{
    struct NetConn *NextConP;	/* Pointer to next conn block (or NULL) */
    int NetFD;			/* File descriptor of network connection */

    int INSCount;		/* Count of incoming NCP INSs
    				   or TCP SUGURG's. */

#   ifdef TCP
      int UrgCnt;		/* Count of bytes to send urgently. */
      int UrgInput;		/* Count of bytes recieved urgently. */
#   endif TCP

    struct GBuffer FmNetBuf;	/* Incoming data */
     struct PBuffer ToNetBuf;	/* Outgoing data */
    struct PBuffer ToUsrBuf;	/* Data for user */
    int (*ECf)();		/* Erase Char function */
    int (*ELf)();		/* Erase Line function */
    int (*AOf)();		/* Abort Output function */
    int (*IPf)();		/* Interrupt Process */
    int (*BREAKf)();		/* Break */
    int (*AYTf)();		/* Are you there */
    int (*GAf)();		/* Go Ahead function */
    int (*SYNCHf)();		/* Synch-received function */
    int  ProtState;		/* State var for protocol FSM */
    int  OptCtrl;		/* (unsigned char) WILL|WONT|DO|DONT */
    int  OptNum;		/* (unsigned char) option number */
    struct PBuffer ToOptBuf;	/* Data for option subnegotiation */
    char OptState[N_OPTIONS][2];/* State of each option (Boolean) */
    char Orig[N_OPTIONS][2];	/* ON if option originated at this end */
    char SBarray[SB_SIZE];	/* Array for use with subnegotiations */
    char FmNetData[RNETSIZE];	/* Array for use with net reads */
    char ToNetData[WNETSIZE];	/* Array for use with net writes */
};

#define PROTINIT 0		       /* Initial value of ProtState */
