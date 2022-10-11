/*
 * Definition of connection block
 */

struct NetConn
{
    char CName[16]; /* Name of connection (if any) */
    char *StateP;   /* Points to SB */
    int ExitOnClose;/* Exit when this connection closed */
    char *Init[5];  /* Commands to be executed at beginning of connection */
    int SendSecur;  /* Send security level (TCP only) */
    char *UtcbP;    /* Points to UserTCB (TCP only) */
    int NetFD;      /* File descriptor of network connection (NCP only) */
    char UsePtySw;  /* Set to cause pty to be hooked up on SynRecd */
    char UsingPty;  /* Used to indicate that this connection is on a pty */
    char Type;      /* NORMAL or PREEMPTING */
    char LocState;  /* ACTIVE SUSPING or SUSP */
    char RemState;  /* ACTIVE or SUSP */
    int OutSynCnt;  /* Amount of urgent data in ToNetBuf (TCP only) */
    int InSynCnt;   /* Nonzero when there is urgent network data to be read */
    int RecSize;    /* Record or message size */
    char ToNetBin;  /* Set if binary mode (suppresses Cr handling) */
    char ToNetCr;   /* State variable for to-net stream */
    char ToUsrBin;  /* Set if binary mode (suppresses Cr handling) */
    char ToUsrCr;   /* State variable for to-user/to-pty stream */
    char ToNetEOF;  /* No more data for net */
    char ToUsrEOF;  /* No more data for user */
    struct CircBuf ToNetBuf;    /* Data for this connection */
    struct CircBuf ToUsrBuf;    /* Data from this connection */
    struct CircBuf FmNetBuf;    /* Data from net for ToUsrBuf */
};

#define ACTIVE 0
#define SUSPING 1
#define SUSP 2

#define NORMAL 0
#define PREEMPT 1
