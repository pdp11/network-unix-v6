#
# include "tcpstru.h"

/*
Written by:

Michael A. Wingfield
Bolt Beranek & Newman
Cambridge, MA 02138

*/

/* data declarations */

struct PortLdr PortHdr;

char CmdUsrName[]   CMDUSRNM;    /* user cmd port name */
char CmdTcpName[]   CMDTCPNM;    /* tcp cmd port name */
char SendName[]     SENDNM;      /* send data port name */
char RecvName[]     RECVNM;      /* recv data port name */

char CmdTcp[CMDTCPSIZE];      /* tcp command out buffer */
char CmdUsr[CMDUSRSIZE];      /* user command out buffer */

struct WorkBlk WorkAvl[WORKSIZE], *WrkAvlPtr, *WorkAvail, *WorkPtr;
struct WorkBlk WorkEnds[2],  *WorkHead, *WorkTail;

struct FdsBlk FdsBffer[FDSSIZE]; /* index relating TCB to port */

struct TcpTcb TCBList[NUMBTCBS],        /* the TCBs */
*TcbAvl, *RetranHd, *TcbHd;

int PortMap[PORTCNT];    /* bit map for selecting local port */

char TcpOut[TCPOTSZ];   /* tcp output buffer */
char TcpIn[TCPINSZ];    /* tcp input buffer */
char FlkBuff[TCPOTSZ];  /* buffer for flaky gateway/2 */

/* the next two tables used to simulate Stop/Resume */
long StopSkt[NSTOP];    /* foreign skts from which to discard segs */
int  StopTcc[NSTOP];    /* list of tcc from which to discard segs */

int NetFdsR;            /* fds for net in traffic */
int NetFdsW;            /* fds for net out traffic */
int EarFds;             /* fds for ear port */

int PartWr;             /* 1 => part of segment needs to be written to net */
char *OutPtr;           /* ptr to next byte to send */
int OutCnt;             /* count of remaining bytes */

int *TimePtr;           /* ptr to relative time */
int RTime;              /* relative time for flaky netout */
int FlkFlag;            /* 1 => dup or out of order seg in progress */

int SegNum;             /* segment identification number */
int ShortIn;            /* 1 => PsipIn: segment in TcpIn buffer */

int ShortCkt;           /* 1 => local traffic not sent to net */
int ANLngth;            /* Arpanet leader size (2 or 6 only ) */
int Debug;              /* 1 => turn on debug output */
int NoRetran;           /* 1 => disable retransmissions */
int SegData;            /* max amount of data in segment */
int RWindow;            /* maximum receive window size */
int WindNum;            /* numerator of window size calc */
int WindDenom;          /* denominator of window size calc */
int LocHost;            /* local host number */
int LocImp;             /* local imp number */
int Auth;               /* 1 => do authorization */
int LocNet;             /* the local net number */
int InLink;             /* the internet link number */
int DoChkSum;           /* 1 => checksum the segments */
int NetTable[NUMNET];   /* map between dest net and gateway */
int RetFast;            /* time between fast retransmissions */
int RetSlow;            /* time between slow retransmissions */
int RetCount;           /* number of retransmissions before slow mode */

/* parameters for the flaky gateway/2 */

char NumLost;           /* % of segments discarded */
char NumBad;            /* % of segments smashed */
char NumDup;            /* % of segments duplicated */
char TimeDup;           /* time until duped segment is sent */
char NumReOrd;          /* % of segments reordered */
char TimeReOrd;         /* time until saved segment is sent */

/* The main program */

main(argc, argv)

/* arguments (if present):

1.  Debug parameter      : values 1 to 4 - see below
2.  ShortCkt parameter   : 1 => allow local traffic to be shorted
3.  Auth parameter       : 1 => authorize the user
4.  DoChkSum parameter   : 1 => checksum the segments
5.  NoRetran parameter   : 1 => no retransmissions today
6.  SegData parameter    : maximum data bytes in segment
7.  RWindow parameter    : maximum receive window size
8.  WindNum parameter    : numerator - window size
9.  WindDenom parameter  : denominator - window size
10. RetFast parameter    : retransmission time - fast
11. RetSlow parameter    : retransmission time - slow
12. RetCount parameter   : max retransmissions before slow mode

Debug:

1 => turn on all debugging including printfs
2 => only net out log
3 => only net in log
4 => net out and net in log

*/

int argc;
char *argv[];

{
   extern sg1, sg2, sg3, sg8, sg13;

   int WorkType;
   struct TcpTcb *TempPtr, *XPtr, *NxtPtr, *SavePtr;
   char AwaitBuffer[AWTLENGTH];
   char *AwaitPtr;
   struct FdsBlk *FdsBffPtr;
   int Error;
   int Err;
   int TimeOut;
   int i;
   int tvec[2];
   char *ctime();
   int TcbTm;
   int Stop;
   int TimeSw;
   int Time1;
   int Time2;
   int **ITimePtr;
   int OldTime;
   struct PMask1 *LPtr;
   int n;
   int SaveFds;
   int Host;

   struct
   {  int ZMode;      /* 0 => read, 1 => write, 2 => read/write */
      long ZHost;     /* 0 => any host */
      int ZLoMsg;     /* low end of msg id range */
      int ZHiMsg;     /* high end: 0 => set to ZLoMsg */
    } RawParams;


   Debug = 0;      /* default: no trace */
   ShortCkt = 1;   /* default: allow local echoing */
   Auth = 0;       /* default: don't authorize */
   DoChkSum = 0;   /* default: don't compute checksums */
   NoRetran = 0;   /* default: retransmissions allowed */
   SegData = 600;  /* default: data in TCP segment */
   RWindow = 1000; /* default: maximum receive window size */
   WindNum = 1;    /* default: numerator of window calculation */
   WindDenom = 1;  /* default: denominator of window calculation */
   RetFast = 2;    /* default: seconds */
   RetSlow = 5;    /* default: seconds */
   RetCount = 6;   /* default: number of retransmissions */

   /* process the arguments */

   for (i = 1; i < argc; i++)
   {  switch(i)
      {
         case 1:
         {  Debug = atoi(argv[i]);
            break;
          }
         case 2:
         {  ShortCkt = atoi(argv[i]);
            break;
          }
         case 3:
         {  Auth = atoi(argv[i]);
            break;
          }
         case 4:
         {  DoChkSum = atoi(argv[i]);
            break;
          }
         case 5:
         {  NoRetran = atoi(argv[i]);
            break;
          }
         case 6:
         {  SegData = atoi(argv[i]);
            break;
          }
         case 7:
         {  RWindow = atoi(argv[i]);
            break;
          }
         case 8:
         {  WindNum = atoi(argv[i]);
            break;
          }
         case 9:
         {  WindDenom = atoi(argv[i]);
            break;
          }

         case 10:
         {  RetFast = atoi(argv[i]);
            break;
          }
         case 11:
         {  RetSlow = atoi(argv[i]);
            break;
          }
         case 12:
         {  RetCount = atoi(argv[i]);
            break;
          }
         default:
         {  printf("too many args\n");
          }
       }
    }

   time(tvec);    /* print the time tcp started */
   printf("%s", ctime(tvec));

   printf("Startup Parameters\n");
   printf("Debug: %d\n", Debug);
   printf("ShortCkt: %d\n", ShortCkt);
   printf("Auth: %d\n", Auth);
   printf("DoChkSum: %d\n", DoChkSum);
   printf("NoRetran: %d\n", NoRetran);
   printf("SegData: %d\n", SegData);
   printf("RWindow: %d\n", RWindow);
   printf("WindNum: %d\n", WindNum);
   printf("WindDenom: %d\n", WindDenom);
   printf("RetFast: %d\n", RetFast);
   printf("RetSlow: %d\n", RetSlow);
   printf("RetCount: %d\n", RetCount);

   /* get the "tcptable" contents */
   /* the tcptable essentially tells tcp where in the
   internet it is executing and how it can talk to
   hosts on other networks. See ReadTbl for table format. */

   if (ReadTbl() == -1)
   {  printf("bad tcptable entry\n");
      exit();
    }

   if ((ANLngth != 2)&&(ANLngth != 6))
   {  printf("bad leader length parameter - (2 or 6 only)\n");
      exit();    /* die */
    }

   /* set up the signal routines */
   if ((signal(1,&sg1) == -1)||
   (signal(2,&sg2) == -1)||
   (signal(3,&sg3) == -1)||
   (signal(8,&sg8) == -1)||
   (signal(13,&sg13) == -1))

   {  printf("bad signal call\n");
      exit();
    }

   /* close unused file descriptors */
   close(0);    /* standard input */
   close(2);    /* error output */

   /* initialize the work avail list */
   for (WrkAvlPtr = WorkAvl;WrkAvlPtr < &WorkAvl[WORKSIZE-1];)
   {  WorkPtr = WrkAvlPtr;
      WorkPtr -> NextWork = ++WrkAvlPtr;
    }
   WrkAvlPtr -> NextWork = 0;     /* null pointer */

   WorkAvail = WorkAvl;

   /* inz the two ends of the work queue */
   WorkHead = &WorkEnds[0];
   WorkTail = &WorkEnds[1];
   WorkHead -> NextWork = WorkTail;
   WorkHead -> LastWork = 0;
   WorkTail -> LastWork = WorkHead;
   WorkTail -> NextWork = 0;

   SegNum = 0;
   PartWr = 0;
   RetranHd = 0;
   FlkFlag = 0;
   ShortIn = 0;

   /* clear the FdsBffer which relate Ports to TCBs */
   for (i = 0; i < FDSSIZE; i++)
   {  FdsBffer[i].BuffFlags = 0;
      FdsBffer[i].BuffFds = 0;
      FdsBffer[i].BuffTcbPtr = 0;
    }

   /* set up TCB avail list */
   for (i = 0; i < (NUMBTCBS-1); i++)
   {  TCBList[i].NxtActTcb = &TCBList[i+1];
    }
   TCBList[i].NxtActTcb = 0;
   TcbAvl = TCBList;
   TcbHd = 0;

   /* clear the bit map for selecting local ports */
   for (i = 0; i < PORTCNT; i++)
   {  PortMap[i] = 0;
    }

   /* clear the tables used for Stop/Resume */
   for (i = 0;i < NSTOP; i++)
   {  StopSkt[i] = 0;
      StopTcc[i] = 0;
    }

   /* create the "ear" port */
   unlink (EAR);
   EarFds = port (EAR,0170222,0);
   if (EarFds == -1)
   {  /* couldn't open ear port */
      printf("Tcp: couldn't open ear port\n");
    }
   else if (EarFds > (FDSSIZE-1))
   {  /* too big */
      printf("Tcp: fds1 too big\n");
    }
   else
   {  /* set up corresponding entry to FdsBffer */
      FdsBffPtr = &FdsBffer[EarFds];
      FdsBffPtr -> BuffFlags = EARIN;
      FdsBffPtr -> BuffFds = EarFds;
      FdsBffPtr -> BuffTcbPtr = 0;
      awtenb(EarFds);
    }

   /* open connection to raw message facility */

   RawParams.ZMode = 0;    /* read */
   RawParams.ZHost = 0;    /* any host */
   RawParams.ZLoMsg = InLink << 4;    /* set link field */
   RawParams.ZHiMsg = 0;    /* set to ZLoMsg */

   NetFdsR = open("/dev/net/rmi", &RawParams);

   if (NetFdsR == -1)
   {  /* can't open read connection to raw msg */
      printf("can't connect to ncp - read\n");
      exit();    /* halt */
    }
   else if (NetFdsR > (FDSSIZE-1))
   {  /* too big */
      printf("Tcp: fds2 too big\n");
      exit();    /* halt */
    }
   else
   {  /* set up corresponding entry in FdsBffer */
      FdsBffPtr = &FdsBffer[NetFdsR];
      FdsBffPtr -> BuffFlags = NETINHP;
      FdsBffPtr -> BuffFds = NetFdsR;
      FdsBffPtr -> BuffTcbPtr = 0;
      awtenb(NetFdsR);      /* enable the file descriptor */
    }

   RawParams.ZMode = 1;    /* write */

   NetFdsW = open("/dev/net/rmi", &RawParams);

   if (NetFdsW == -1)
   {  /* can't open write connection to raw msg */
      printf("can't connect to ncp - write\n");
      exit();    /* halt */
    }
   else if (NetFdsW > (FDSSIZE-1))
   {  /* too big */
      printf("Tcp: fds20 too big\n");
      exit();    /* halt */
    }
   else
   {  /* set up corresponding entry in FdsBffer */
      FdsBffPtr = &FdsBffer[NetFdsW];
      FdsBffPtr -> BuffFlags = NETOUTHP;
      FdsBffPtr -> BuffFds = NetFdsW;
      FdsBffPtr -> BuffTcbPtr = 0;
      awtenb(NetFdsW);      /* enable the file descriptor */
    }

   if (Debug > 0)    /* don't waste fds unless debugging */

   {  Error = LogInit("net.log");
      if (Error == -1)
      {  printf("error in LogInit\n");
       }
    }

   TimeSw = 1;
   Time2 = 0;
   ITimePtr = &TimePtr;
   TimePtr = &Time2;
   OldTime = 0;

   itime(ITimePtr);

   while(1)     /* infinite loop */
   {  /* process the work-to-do list */
      WorkPtr = WorkHead -> NextWork;
      while ((WorkPtr -> NextWork) != 0)
      {  /* process the quantum of work */
         WorkType = (WorkPtr -> PortFlags)&037;
         switch (WorkType)
         {  case EARIN:     /* a message to the "ear" */
            {  Error = EarIn(WorkPtr);
               break;
             }
            case USRCMD:
            {  Error = CmdIn(WorkPtr);
               break;
             }
            case NETOUTHP:
            {  Error = PsipOut(WorkPtr);
               break;
             }
            case NETINHP:
            {  Error = PsipIn(WorkPtr);
               break;
             }
            case USERINHP:
            {  Error = DataIn(WorkPtr);
               break;
             }
            default:
            {  /* something bad */
               printf("Sched: bad opcode: %d\n",WorkType);
               Error = 0;
             }
          }

         switch(Error)
         {  /* determine what to do with the work block */
            case 0:    /* remove the work entry */
            {  TempPtr = WorkPtr -> NextWork;
               (WorkPtr -> NextWork) -> LastWork = WorkPtr -> LastWork;
               (WorkPtr -> LastWork) -> NextWork = WorkPtr -> NextWork;
               WorkPtr -> NextWork = WorkAvail;
               WorkAvail = WorkPtr;
               break;
             }

            case 1:    /* do not remove the work block */
            {  TempPtr = WorkPtr -> NextWork;
               break;
             }

            case 2:    /* move work block to end of queue */
            {
               TempPtr = WorkPtr -> LastWork;
               MakeWrk(0, WorkPtr -> PortFlags,
               WorkPtr -> TCBPtr, WorkPtr -> PortFds);
               TempPtr = TempPtr -> NextWork;
               break;
             }
            case 3:    /* connection is to be aborted */
            {  /* move WorkPtr to next block not associated
               with this TCB */

               XPtr = WorkPtr -> TCBPtr;

               while((WorkPtr -> NextWork != 0)&&(WorkPtr -> TCBPtr == XPtr))
               {  WorkPtr = WorkPtr -> NextWork;
                }

               /* remove work blocks, etc assoc with XPtr */
               Abort(XPtr, 0);

               TempPtr = WorkPtr;
               break;
             }
            case 4:     /* abort all connections associated with user */
            {
               SaveFds = WorkPtr -> PortFds;

               /* delete work block entry */

               TempPtr = WorkPtr -> NextWork;
               (WorkPtr -> NextWork) -> LastWork = WorkPtr -> LastWork;
               (WorkPtr -> LastWork) -> NextWork = WorkPtr -> NextWork;
               WorkPtr -> NextWork = WorkAvail;
               WorkAvail = WorkPtr;

               /* find associated TCBs */

               XPtr = TcbHd;

               while(XPtr != 0)
               {  SavePtr = XPtr -> NxtActTcb;

                  if (XPtr -> UserCmdPort == SaveFds)
                  {  /* found an associated TCB */

                     switch(XPtr -> State)
                     {  case CLOSED:
                        case OPEN:
                        case SYNSENT:

                        {  /* clean up and close out the connection */
                           /* move TempPtr to next block not associated with this TCB */

                           while((TempPtr -> NextWork != 0)&&(TempPtr -> TCBPtr == XPtr))
                           {  TempPtr = TempPtr -> NextWork;
                            }

                           Abort(XPtr, 1);
                           break;
                         }
                        default:
                        {  /* every other connection state */
                           /* set the RST flag and let PsipOut finish the abortion */

                           XPtr -> TCPFlags =| RST + ACK;
                           XPtr -> Flags =| 020;    /* abort connection */

                           MakeWrk(0, NETOUTHP, XPtr, NetFdsW);

                           break;
                         }
                      }

                   }

                  XPtr = SavePtr;
                }
               break;
             }

            default:
            {  /* this looks bad */
               TempPtr = WorkPtr -> NextWork;
               printf("Sched: bad work return: %d\n",Error);
             }
          }
         WorkPtr = TempPtr;

       }

      if (FlkFlag == 0)
      {  RTime = 1000;   /* seconds */
       }

      if (RetranHd != 0)
      {  if ((RetranHd -> RetranTm - *TimePtr) < (RTime - *TimePtr))
         {  TimeOut = RetranHd -> RetranTm - *TimePtr;
          }
         else
         {  TimeOut = RTime - *TimePtr;
          }
       }
      else
      {  TimeOut = RTime - *TimePtr;
       }

      if (TimeOut < 0)
      {  TimeOut = 0;
       }

      if (Debug == 1)
      {  printf("Sched: TimeOut = %d\n",TimeOut);
       }

      /* wait for some port activity */
      await(TimeOut, AwaitBuffer, AWTLENGTH);

      if (FlkFlag == 1)
      {  RTime =- *TimePtr;

         if (RTime <= 0)
         {  /* schedule sending of dup or out of order segment */

            MakeWrk(0, NETOUTHP, &FlkFlag, NetFdsW);
          }
       }
      n = 0;
      AwaitPtr = AwaitBuffer;

      /* search the await buffer, putting items on work list */
      while (*AwaitPtr != -1)
      {  FdsBffPtr = &FdsBffer[*AwaitPtr];

         /* put file descriptor blocks in work queue */
         if (WorkAvail == 0)    /* test free list */
         {  /* no more blocks */
            printf("Sched: no more work blocks\n");
          }
         else
         {  /* put work block in work queue */
            switch((FdsBffPtr -> BuffFlags)&037)
            {  case USEROUTHP:
               {  /* no entry in work queue */
                  break;
                }
               case TCPCMD:
               {  /* no entry in work queue */
                  break;
                }
               case NETOUTHP:
               {  /* no entry in work queue */
                  break;
                }
               case 0:
               {  /* the FdsBffer entry was deleted! */
                  break;
                }
               case USERINHP:
               {
                  /* fall through */
                }
               default:
               {  /* put entry in work queue */

                  MakeWrk(0, FdsBffPtr -> BuffFlags,
                  FdsBffPtr -> BuffTcbPtr, FdsBffPtr -> BuffFds);

                }
             }
          }
         AwaitPtr++;
         n++;
       }

      if (n == 0)
      {  /* printf("empty wakeup\n"); */
       }

      /* switch time counts */
      if (TimeSw == 0)
      {  Time2 = 0;
         TimePtr = &Time2;
         OldTime = Time1;
         TimeSw = 1;
       }
      else
      {  Time1 = 0;
         TimePtr = &Time1;
         OldTime = Time2;
         TimeSw = 0;
       }

      if (RetranHd != 0)

      {  /* schedule retransmissions based on OldTime */

         Stop = 0;
         XPtr = RetranHd;
         TcbTm = -OldTime;
         n = 0;

         do
         {  XPtr -> RetranTm =+ TcbTm;
            if (XPtr -> RetranTm <= 0)
            {  /* schedule the TCB for retransmission */
               /* put ptrs back to last acked byte */
               XPtr -> SendSeq = XPtr -> LeftSeq;
               XPtr -> DataNxt = XPtr -> DataHd;

               if (Debug == 1)
               {  printf("Sched: retran\n");
                }
               if (Debug == 4)
               {  Log(5, "si", "retran", XPtr, -1);
                }

               /* indicate that a retransmission is being
               scheduled */
               XPtr -> Flags =| XRETRANOK;

               XPtr -> NumRetran =+ 1;
               XPtr -> RetranSeg =+ 1;

               if (XPtr -> NumRetran == RetCount)
               {  /* foreign process not responding */
                  /* send notice to user */

                  LPtr = CmdTcp;
                  LPtr -> YOpCode = OPROCNR;
                  LPtr -> YHandle = XPtr -> ConnHandle;
                  write(XPtr -> TCPCmdPort, CmdTcp, 4);

                  XPtr -> Flags =| 4;    /* set retransmission mode flag */
                }

               NxtPtr = XPtr -> NxtRetran;
               /* remove TCB from retransmission queue */
               UnQTcb(XPtr);

               if (RetranHd == 0)
               {  /* UnQTcb removed the only entry */
                  Stop = 1;
                }

               /* make work for PsipOut */
               MakeWrk(0, NETOUTHP, XPtr, NetFdsW);
               TcbTm = XPtr -> RetranTm;
               XPtr -> RetranTm = 0;
               XPtr = NxtPtr;
             }
            else
            {  Stop = 1;
             }
            n++;
          }
         while((Stop == 0)&&(XPtr != RetranHd)&&(n < NUMBTCBS));
       }
    }
 }

/*  */
/* subroutine which acts on messages to the "ear" */

EarIn(WrkPtr)

struct WorkBlk *WrkPtr;
{
   extern char NumLost;
   extern char NumBad;
   extern char NumDup;
   extern char TimeDup;
   extern char NumReOrd;
   extern char TimeReOrd;

   int UserPID;
   int i;
   int FDesc;
   int Cap[2];
   int UsrFds;
   struct PMask1 *AckPtr;
   int xx;
   struct FdsBlk *FdsBffPtr;
   struct PMask1 *InitPtr;


   if (Debug == 1)
   {  printf("EarIn: entered\n");
    }
   capac(WrkPtr -> PortFds,Cap,4);
   if (Cap[0] == 0)
   {  /* no real work here */
      return(0);
    }

   do
   {  /* flush zero length messages */
      /* these occur when processes close the ear port */
      read(WrkPtr -> PortFds, &PortHdr, HDRLNGTH);   /* get message header */
      Cap[0] =- HDRLNGTH;
    }
   while (((PortHdr.DCount&~0100000) == 0)&&(Cap[0] >= HDRLNGTH));

   if (Cap[0] == 0)
   {  /* no real messages here */
      return(0);
    }

   /* get rest of message */
   read(WrkPtr -> PortFds, CmdUsr, PortHdr.DCount&~0100000);

   InitPtr = CmdUsr;
   switch(InitPtr -> YOpCode)
   {  case OINIT:
      {
         /* create names of command in, command out ports */
         UserPID = PortHdr.UsrPID;
         for (i = PRTPREFIX+2; i > PRTPREFIX-1; i--)
         {  CmdUsrName[i] = '0' + (UserPID&07);
            CmdTcpName[i] = '0' + (UserPID&07);
            UserPID =>> 3;
          }
         unlink(CmdUsrName);
         UsrFds = port(CmdUsrName,0170222,0);       /* create user command port */

         if (UsrFds == -1)
         {  /* couldn't create user command port */
            printf("EarIn: couldn't create user command port\n");
            return(0);
          }
         FDesc = open(CmdTcpName,1);       /* open the tcp command port */
         if (FDesc == -1)
         {  /* ignore the clown */
            printf("EarIn: couldn't open tcp command port\n");
            close(UsrFds);
            return(0);
          }

         unlink(CmdTcpName);

         if (UsrFds > (FDSSIZE-1))
         {  /* too big */
            printf("EarIn: fds3 too big\n");
          }
         else
         {  /* enter file descriptors in FdsBffer */
            FdsBffPtr = &FdsBffer[UsrFds];
            FdsBffPtr -> BuffTcbPtr = FDesc;  /* saved for Open response */
            FdsBffPtr -> BuffFlags = USRCMD;
            FdsBffPtr -> BuffFds = UsrFds;

            awtenb(UsrFds);
          }
         if (FDesc > (FDSSIZE-1))
         {  /* too big */
            printf("EarIn: fds4 too big\n");
          }
         else
         {  FdsBffPtr = &FdsBffer[FDesc];
            FdsBffPtr -> BuffTcbPtr = 0;
            FdsBffPtr -> BuffFlags = TCPCMD;
            FdsBffPtr -> BuffFds = FDesc;
            awtenb(FDesc);
          }

         /* send back the acknowledge message */
         AckPtr = CmdTcp;
         AckPtr -> YOpCode = OACK;  /* acknowledge */
         AckPtr -> YHandle = 0;
         xx = write(FDesc,CmdTcp,4);

         break;
       }

      case OSTAT:
      {  /* special command to print status */
         Status();
         break;
       }

      case OFLAKY:
      {  /* new parameters for flaky gateway/2 */

         NumLost   = CmdUsr[2];
         NumBad    = CmdUsr[3];
         NumDup    = CmdUsr[4];
         TimeDup   = CmdUsr[5];
         NumReOrd  = CmdUsr[6];
         TimeReOrd = CmdUsr[7];

         printf("NL: %d NB: %d ND: %d TD: %d NR: %d TR: %d\n",
         CmdUsr[2], CmdUsr[3], CmdUsr[4], CmdUsr[5],
         CmdUsr[6], CmdUsr[7]);

         break;
       }
      default:
      {  printf("EarIn: some bad opcode %d\n", InitPtr -> YOpCode);
         break;
       }

    }
   Cap[0] =- PortHdr.DCount&~0100000;
   if (Cap[0] > 0)
   {  /* more ear commands */
      return(2);
    }
   return(0);
 }


/*  */
/* Module which handles commands sent from user to TCP */

CmdIn(WrkPtr)

struct WorkBlk *WrkPtr;

{
   register struct TcpTcb *XPtr, *Handle, *TPtr;

   int i;
   int x;
   int j;
   int k;
   int T;
   int Found;
   int Stop;
   int SndFds;
   int RcvFds;
   int Error;
   int Cap[2];
   int UserPID;
   int R1;
   int UserFds;
   int ParamVal[20];

   struct WorkBlk *WPtr;
   struct PMask1 *ChngPtr;
   struct PMask1 *AckPtr;
   struct PMask1 *MovePtr;
   struct PMask1 *ErrPtr;
   struct MOpen *OpenPtr;
   struct MUrgent *UrgPtr;
   struct MClose *ClosePtr;
   struct PMask1 *AbortPtr;
   struct FdsBlk *FdsBffPtr;
   struct MUpdate *UPtr;
   struct MStatus *SPtr;

   if (Debug == 1)
   {  printf("CmdIn: entered\n");
    }
   /* get port capacity */

   capac(WrkPtr -> PortFds, Cap, 4);

   while(1)      /* exit by return */

   {  if (Cap[0] == 0)
      {  /* no more commands */
         return(0);
       }
      if (Cap[0] == HDRLNGTH)
      {  /* user closed his command port */

         UserFds = WrkPtr -> TCBPtr;

         awtdis(WrkPtr -> PortFds);
         awtdis(UserFds);

         close(WrkPtr -> PortFds);
         close(UserFds);

         FdsBffer[WrkPtr -> PortFds].BuffFlags = 0;
         FdsBffer[UserFds].BuffFlags = 0;

         return(4);   /* abort all connections for this process */
       }

      if (Cap[0] < HDRLNGTH)
      {  /* can't happen!? */
         printf("CmdIn: huh?\n");
         return(4);    /* abort all connections */
       }

      read(WrkPtr -> PortFds, &PortHdr, HDRLNGTH);  /* get msg header */

      /* read in the text */
      read(WrkPtr -> PortFds, CmdUsr, (PortHdr.DCount&~0100000));

      Cap[0] =- HDRLNGTH + PortHdr.DCount&~0100000;

      OpenPtr = CmdUsr;
      switch (OpenPtr -> OpCode)

      {  case OOPEN:
         {  /* find an unused TCB */
            if (TcbAvl == 0)
            {  /* preempt TCB if authorization bit on */
               if (Auth == 1)
               {  /* try to find a non-CAT-I TCB to steal */

                  T = -1;
                  XPtr = TcbHd;

                  /* find the lowest precedence TCB */
                  while(XPtr != 0)
                  {  if ((XPtr -> SndPrec > T)&&(XPtr -> SndPrec < 12))
                     {  /* CAT-I traffic has precedences of 12-15 */

                        T = XPtr -> SndPrec;
                        TPtr = XPtr;
                      }
                   }

                  if (T == -1)
                  {  /* no TCB available */
                     ErrPtr = CmdTcp;
                     ErrPtr -> YOpCode = OERR;
                     ErrPtr -> YMisc = ESYSC;
                     write(WrkPtr -> TCBPtr, CmdTcp, 6);
                     break;
                   }

                  /* abort connection of this TCB */

                  CmdOut(TPtr, ORESET, 0, 0);

                  CmdOut(TPtr, OCHNG, CLOSED, 0);

                  Abort(TPtr, 1);

                }
               else
               {  /* no TCB available */
                  ErrPtr = CmdTcp;
                  ErrPtr -> YOpCode = OERR;
                  ErrPtr -> YMisc = ESYSC;
                  write(WrkPtr -> TCBPtr, CmdTcp, 6);
                  break;
                }
             }

            /* got a TCB */
            Handle = TcbAvl;
            TcbAvl = TcbAvl -> NxtActTcb;

            /* link it on the active list */
            Handle -> NxtActTcb = TcbHd;
            TcbHd = Handle;

            /* set up TCB for possible error handling */
            Handle -> ConnHandle = OpenPtr -> OHandle;
            Handle -> TCPCmdPort = WrkPtr -> TCBPtr;

            if (Auth == 1)
            {  /* verify that user can do an Open */
               Error = Authorize(PortHdr.RealUserID, OpenPtr, Handle);

               if (Error != 0)
               {  /* user is not authorized in some way */

                  CmdOut(Handle, OERR, Error, 0);

                  /* return the TCB */
                  TcbHd = Handle -> NxtActTcb;
                  Handle -> NxtActTcb = TcbAvl;
                  TcbAvl = Handle;

                  break;
                }
             }

            /* check validity of local TCP port number */

            if (OpenPtr -> SrcPrt == 0)
            {  /* create a unique local port number */
               i = 0;    /* row index */
               Stop = 0;

               while ((i < PORTCNT)&&(Stop == 0))
               {  k = 1;
                  j = 0;    /* column index */
                  while ((j < 16)&&(Stop == 0))
                  {  if ((PortMap[i]&k) == 0)
                     {  Stop = 1;
                      }
                     else
                     {  k =<< 1;
                        j++;
                      }
                   }
                  if (Stop == 0)
                  {  i++;
                   }
                }

               if (Stop == 0)
               {  /* no unused entries */

                  CmdOut(Handle, OERR, ESYSC, 0);

                  /* return the TCB */
                  TcbHd = Handle -> NxtActTcb;
                  Handle -> NxtActTcb = TcbAvl;
                  TcbAvl = Handle;

                  break;
                }

               PortMap[i] =| k;   /* set the used bit */

               /* make up the port number */
               OpenPtr -> SrcPrt = BASEPORT + (i << 4) + j;

             }
            else if (OpenPtr -> SrcPrt < 0400)
            {  /* check authorization to use first 256 ports */
               if (Auth == 1)
               {  k = GetFields(PortHdr.RealUserID, &ParamVal);
                  if ((k == -1)||(ParamVal[0] != 1))
                  {  /* unauthorized use of these ports */

                     CmdOut(Handle, OERR, EUNAUTH, 0);

                     /* return the TCB */
                     TcbHd = Handle -> NxtActTcb;
                     Handle -> NxtActTcb = TcbAvl;
                     TcbAvl = Handle;

                     break;
                   }
                }
             }

            else if ((OpenPtr -> SrcPrt >= BASEPORT)&&
            (OpenPtr -> SrcPrt < BASEPORT + 0400))
            {  /* ports reserved for tcp use */

               CmdOut(Handle, OERR, EBADPARAM, 0);

               /* return the TCB */
               TcbHd = Handle -> NxtActTcb;
               Handle -> NxtActTcb = TcbAvl;
               TcbAvl = Handle;

               break;
             }

            /* check for Open of connection mode */
            if (OpenPtr -> OpenMode == 0)
            {  /* check for fully specified foreign socket */
               if ((OpenPtr -> Net == 0)||((OpenPtr -> HostH == 0)&&
               (OpenPtr -> HostL == 0))||(OpenPtr -> DstPrt == 0))
               {  /* parameter unspecified: return error */

                  CmdOut(Handle, OERR, EUNSPEC, 0);

                  /* return the TCB */
                  TcbHd = Handle -> NxtActTcb;
                  Handle -> NxtActTcb = TcbAvl;
                  TcbAvl = Handle;

                  break;
                }
             }

            /* derive the data send and receive ports */
            UserPID = PortHdr.UsrPID;
            for (i = PRTPREFIX+2; i > PRTPREFIX-1; i--)
            {  SendName[i] = '0' + (UserPID&07);
               RecvName[i] = '0' + (UserPID&07);
               UserPID =>> 3;
             }

            j = OpenPtr -> OHandle;
            for (i = PRTPREFIX+5; i > PRTPREFIX+2; i--)
            {  SendName[i] = '0' + (j&07);
               RecvName[i] = '0' + (j&07);
               j =>> 3;
             }

            /* create the send data port */
            unlink(SendName);
            SndFds = port(SendName,0170222,0);
            if (SndFds == -1)
            {  /* couldn't create send data port */
               /* send error message */

               CmdOut(Handle, OERR, ESNDPRT, 0);

               /* return the TCB */
               TcbHd = Handle -> NxtActTcb;
               Handle -> NxtActTcb = TcbAvl;
               TcbAvl = Handle;

               break;
             }

            /* open the receive data port */
            RcvFds = open(RecvName,1);
            if (RcvFds == -1)
            {  /* couldn't open recv data port */

               CmdOut(Handle, OERR, ERCVPRT, 0);

               /* return the TCB */
               TcbHd = Handle -> NxtActTcb;
               Handle -> NxtActTcb = TcbAvl;
               TcbAvl = Handle;

               close(SndFds);
               break;
             }

            unlink(RecvName);

            /* fill in the relevant TCB parameters */
            Handle -> State = OPEN;
            Handle -> Flags = 0;
            Handle -> TMode = OpenPtr -> OpenMode;
            Handle -> ProcID = PortHdr.UsrPID;
            Handle -> UserCmdPort = WrkPtr -> PortFds;
            Handle -> SendPort = SndFds;
            Handle -> RecvPort = RcvFds;
            Handle -> DataHd = 0;
            Handle -> DataNxt = 0;
            Handle -> DataTl = 0;
            Handle -> VerHdr = (INLNGTH/4)|(INVERNO << 4);
            Handle -> TCPFlags = (TCPLNGTH/4) << 12;
            Handle -> DataState = 0;

            /* select an initial sequence number */
            /* say, zero! */
            z = &(Handle -> SndISN);
            z -> bb[0] = 0;
            z -> bb[1] = 0;

            Handle -> LeftSeq = Handle -> SndISN;
            Handle -> SendSeq = Handle -> SndISN;
            Handle -> MaxSend = Handle -> SndISN;
            Handle -> SndFSN = 0;

            Handle -> OpnNet = OpenPtr -> Net;
            Handle -> OpnHstH = OpenPtr -> HostH;
            Handle -> OpnHstL = OpenPtr -> HostL;
            Handle -> OpnPort = OpenPtr -> DstPrt;
            Handle -> DstNet = OpenPtr -> Net;
            Handle -> DstHstH = OpenPtr -> HostH;
            Handle -> DstHstL = OpenPtr -> HostL;
            Handle -> DestPort = OpenPtr -> DstPrt;
            Handle -> SrcePort = OpenPtr -> SrcPrt;
            Handle -> SrcNet = LocNet;
            Handle -> SrcHstH = LocHost;
            Handle -> SrcHstL = LocImp;

            Handle -> Window = (PORTSIZE*WindNum)/WindDenom;
            if (Handle -> Window > RWindow)
            {  Handle -> Window = RWindow;
             }

            Handle -> SndWindow = 1;    /* allow one byte */

            Handle -> NumRetran = 0;
            Handle -> NxtRetran = 0;

            Handle -> TimeToLive = 10;      /* 10 seconds...? */
            Handle -> Protocol = TCPVERNO;

            Handle -> Options[0] = OPTSPT;
            Handle -> Options[1] = 4;

            Handle -> SegRecv = 0;
            Handle -> DupRecv = 0;
            Handle -> BusyRecv = 0;
            Handle -> RetranSeg = 0;

            /* put user send and receive data ports in FdsBffer */
            if (SndFds > (FDSSIZE-1))
            {  /* too big */
               printf("CmdIn: fds5 too big\n");
             }
            else
            {  FdsBffPtr = &FdsBffer[SndFds];
               FdsBffPtr -> BuffFlags = USERINHP;
               FdsBffPtr -> BuffFds = SndFds;
               FdsBffPtr -> BuffTcbPtr = Handle;
               x = awtenb(SndFds);
             }
            if (RcvFds > (FDSSIZE-1))
            {  /* too big */
               printf("CmdIn: fds6 too big\n");
             }
            else
            {  FdsBffPtr = &FdsBffer[RcvFds];
               FdsBffPtr -> BuffFlags = USEROUTHP;
               FdsBffPtr -> BuffFds = RcvFds;
               FdsBffPtr -> BuffTcbPtr = Handle;
               awtenb(RcvFds);
             }
            if ((OpenPtr -> OpenMode) == 0)
            {  /* user wants to Open a connection */

               /* check that socket pair is unique */

Found = 0;
               XPtr = TcbHd;

               while(XPtr != 0)
               {  if (XPtr != Handle)
                  {  if ((XPtr -> SrcePort == OpenPtr -> SrcPrt)&&
                     (XPtr -> DestPort == OpenPtr -> DstPrt)&&
                     (XPtr -> DstNet == OpenPtr -> Net)&&
                     (XPtr -> DstHstH == OpenPtr -> HostH)&&
                     (XPtr -> DstHstL == OpenPtr -> HostL))
                     {  /* socket pair duplication */

                        CmdOut(Handle, OERR, EBADPARAM, 0);

                        /* return the TCB */
                        TcbHd = Handle -> NxtActTcb;
                        Handle -> NxtActTcb = TcbAvl;
                        TcbAvl = Handle;

close(SndFds);
close(RcvFds);

/* remove entries from FdsBffer */
FdsBffer[SndFds].BuffFlags = 0;
FdsBffer[RcvFds].BuffFlags = 0;

Found = 1;
			break;  /* break out of while */
                      }
                   }
                  XPtr = XPtr -> NxtActTcb;
                }
if (Found == 1)
{ break;    /* break out of case */
}

               /* create some work for PsipOut */
               MakeWrk(0, NETOUTHP, Handle, NetFdsW);

             }

            /* send back an acknowledge */
            AckPtr = CmdTcp;
            AckPtr -> YOpCode = OACK;
            /* return the actual ptr to TCB this time only */
            /* all other acks return the index into the usertcb */
            AckPtr -> YHandle = Handle;
            write(Handle -> TCPCmdPort, CmdTcp, 4);

            break;
          }

         case OCLOSE:
         {  /* begin connection closing procedure */

            ClosePtr = CmdUsr;
            XPtr = ClosePtr -> CHandle;

            Error = ChkPtr(XPtr, WrkPtr -> PortFds);

            if (Error == -1)
            {  break;    /* user is doing something dumb */
             }

            switch (XPtr -> State)
            {  case OPEN:
               case SYNSENT:
               {  /* move connection state to CLOSED */

                  XPtr -> State = CLOSED;

                  /* send user a change notice */
                  CmdOut(XPtr, OCHNG, CLOSED, 0);

                  break;
                }

               case SYNRECD:
               case ESTAB:
               case CLOSEWAIT:
               {  XPtr -> Flags =| 010;    /* send FIN after data */
                  XPtr -> SndFSN = XPtr -> SndISN + ClosePtr -> CFinNum + 1;
                  MakeWrk(0, NETOUTHP, XPtr, NetFdsW);
                  break;
                }

               default:
               {  break;    /* ignore request */
                }
             }

            /* return an ack message to user */
            CmdOut(XPtr, OACK, 0, 0);

            break;
          }

         case OABORT:
         {  /* abort the connection */
            AbortPtr = CmdUsr;
            XPtr = AbortPtr -> YHandle;

            Error = ChkPtr(XPtr, WrkPtr -> PortFds);

            if (Error == -1)
            {  break;    /* user is doing something dumb */
             }

            /* verify that abort is allowed here */

            switch(XPtr -> State)
            {  case CLOSED:
               case OPEN:
               case SYNSENT:

               {  /* clean up and close out the connection */
                  Abort(XPtr, 0);
                  break;
                }
               default:
               {  /* every other connection state */
                  /* set the RST flag and let PsipOut finish the abortion */

                  XPtr -> TCPFlags =| RST + ACK;
                  XPtr -> Flags =| 020;    /* abort connection */

                  MakeWrk(0, NETOUTHP, XPtr, NetFdsW);

                  break;
                }
             }
            break;
          }
         case OURGENT:
         {  /* send urgent condition */
            UrgPtr = CmdUsr;
            XPtr = UrgPtr -> UHandle;

            Error = ChkPtr(XPtr, WrkPtr -> PortFds);

            if (Error == -1)
            {  break;    /* user is doing something dumb */
             }

            XPtr -> SndUrgNo = (UrgPtr -> UByteNo) + (XPtr -> SndISN) + 1;

            /* check if everything has been ACKed */
            R1 = ULCompare(&(XPtr -> SndUrgNo), &(XPtr -> LeftSeq));

            if (R1 != 1)
            /* SndUrgNo <= LeftSeq */
            {  break;   /* ignore it - already ACKed */
             }

            /* set urgent work bit */
            XPtr -> Flags =| 01;

            if ((XPtr -> State == ESTAB)||(XPtr -> State == CLOSEWAIT))
            {  /* make work for PsipOut */
               MakeWrk(0, NETOUTHP, XPtr, NetFdsW);
             }

            break;
          }
         case OMOVE:
         {  MovePtr = CmdUsr;

            TPtr = MovePtr -> YHandle;

            Error = ChkPtr(TPtr, WrkPtr -> PortFds);

            if (Error == -1)
            {  break;    /* user is doing something dumb */
             }

            if ((TPtr -> State != ESTAB)&&
            (TPtr -> State != CLOSEWAIT)&&
            (TPtr -> State != FINWAIT))
            {  CmdOut(TPtr, OERR, EMVSTATE, 0);
               break;
             }

            /* find a TCB in ACCEPT mode */
            Found = 0;
            XPtr = TcbHd;

            while((XPtr != 0)&&(Found == 0))
            {  /* TMode = 2 => ACCEPT */
               if ((XPtr -> TMode == 2)&&(XPtr -> State == OPEN))
               {  /* check process ID */
                  if ((MovePtr -> YMisc == 0 /* unspecified */ )||
                  (MovePtr -> YMisc == XPtr -> ProcID))
                  {  /* check for SPT data */

                     if (Auth == 1)
                     {  /* validate S/P parameters */
                        if ((TPtr -> ScMaxOut <= XPtr -> ScMaxOut)&&
                        (TPtr -> ScMinOut >= XPtr -> ScMinOut)&&
                        (TPtr -> ScMaxIn <= XPtr -> ScMaxIn)&&
                        (TPtr -> ScMinIn >= XPtr -> ScMinIn)&&
                        (TPtr -> SndPrec <= XPtr -> ATPrec)&&
                        (TPtr -> MxRcvPrec <= XPtr -> MxRcvPrec)&&
                        (TPtr -> MnRcvPrec >= XPtr -> MnRcvPrec))

                        {  /* so far so good */
                           /* check for matching TCC */

                           for (i = 0; i < XPtr -> TCCCnt; i++)
                           {  if (TPtr -> TccList[0] == XPtr -> TccList[i])
                              {  Found = 1;    /* at last! */
                               }
                            }
                         }
                      }
                     else
                     {  Found = 1;
                      }
                   }
                }

               if (Found == 0)
               {  XPtr = XPtr -> NxtActTcb;
                }
             }

            if (Found == 0)
            {  CmdOut(TPtr, OERR, EBADPARAM, 0);
               break;
             }

            /* exchange the four port fds */

            T = TPtr -> TCPCmdPort;
            TPtr -> TCPCmdPort = XPtr -> TCPCmdPort;
            XPtr -> TCPCmdPort = T;

            T = TPtr -> UserCmdPort;
            TPtr -> UserCmdPort = XPtr -> UserCmdPort;
            XPtr -> UserCmdPort = T;

            T = TPtr -> SendPort;
            TPtr -> SendPort = XPtr -> SendPort;
            XPtr -> SendPort = T;

            T = TPtr -> RecvPort;
            TPtr -> RecvPort = XPtr -> RecvPort;
            XPtr -> RecvPort = T;

            /* reset state of TCB */

            XPtr -> State = CLOSED;

            /* exchange connection handles */

            T = TPtr -> ConnHandle;
            TPtr -> ConnHandle = XPtr -> ConnHandle;
            XPtr -> ConnHandle = T;

            /* reset FSM of DataIn routine */

            TPtr -> DataState = 0;

            /* set process ID */

            TPtr -> ProcID = XPtr -> ProcID;

            /* fix up FdsBffer */
            for (i = 0; i < FDSSIZE; i++)
            {  if (FdsBffer[i].BuffFlags != 0)
               {  if (FdsBffer[i].BuffTcbPtr == XPtr)
                  {  FdsBffer[i].BuffTcbPtr = TPtr;
                   }
                  else if (FdsBffer[i].BuffTcbPtr == TPtr)
                  {  FdsBffer[i].BuffTcbPtr = XPtr;
                   }
                }
             }
            /* fix up work list */

            WPtr = WorkHead -> NextWork;

            while(WPtr -> NextWork != 0)
            {  if (WPtr -> TCBPtr == XPtr)
               {  WPtr -> TCBPtr = TPtr;
                }
               else if (WPtr -> TCBPtr == TPtr)
               {  WPtr -> TCBPtr = XPtr;
                }
               WPtr = WPtr -> NextWork;
             }

            /* ack the move command */

            CmdOut(XPtr, OACK, 0, 0);

            /* update the library UTCB parameters of mover */

            UPtr = CmdTcp;
            UPtr -> ZOpCode = OUPDATE;
            UPtr -> ZHandle = XPtr -> ConnHandle;
            UPtr -> ZConnIndex = XPtr;

            i = XPtr -> DataTl - XPtr -> DataHd;
            if (i < 0)
            {  i =+ RBUFFSIZE;
             }

            UPtr -> ZSndCnt = TPtr -> LeftSeq + i - TPtr -> SndISN - 1;
            UPtr -> ZRcvCnt = TPtr -> AckNo - TPtr -> RcvISN - 1;
            write(XPtr -> TCPCmdPort, CmdTcp, 14);

            /* update the library UTCB parameters of movee */

            UPtr -> ZHandle = TPtr -> ConnHandle;
            UPtr -> ZConnIndex = TPtr;
            write(TPtr -> TCPCmdPort, CmdTcp, 14);

            /* send CLOSED to the mover */

            CmdOut(XPtr, OCHNG, CLOSED, 0);

            /* send state to movee */

            CmdOut(TPtr, OCHNG, TPtr -> State, 0);

            /* make work in case movee has sent data */

            MakeWrk(0, USERINHP, TPtr, TPtr -> SendPort);

            break;
          }

         case OSTAT:
         {  /* send back status information to user */

            SPtr = CmdUsr;
            XPtr = SPtr -> YHandle;

            Error = ChkPtr(XPtr, WrkPtr -> PortFds);

            if (Error == -1)
            {  break;    /* user is doing something dumb */
             }

            SPtr = CmdTcp;

            SPtr -> SOpCode    = OSTAT;
            SPtr -> SState     = XPtr -> State;
            SPtr -> SNet       = XPtr -> DstNet;
            SPtr -> SHost      = XPtr -> DstHstH;
            SPtr -> SImp       = XPtr -> DstHstL;
            SPtr -> SLocPrt    = XPtr -> SrcePort;
            SPtr -> SForPrt    = XPtr -> DestPort;
            SPtr -> SScMxOt    = XPtr -> ScMaxOut;
            SPtr -> SScMnOt    = XPtr -> ScMinOut;
            SPtr -> SScMxIn    = XPtr -> ScMaxIn;
            SPtr -> SScMnIn    = XPtr -> ScMinIn;
            SPtr -> SSndPrec   = XPtr -> SndPrec;
            SPtr -> SRcvPrec   = XPtr -> MnRcvPrec;
            SPtr -> SNTcc      = XPtr -> TCCCnt;

            for (i = 0;i < XPtr -> TCCCnt; i++)
            {  SPtr -> STcc[i] = XPtr -> TccList[i];
             }

            SPtr -> SSndWind   = XPtr -> SndWindow;
            SPtr -> SRcvWind   = XPtr -> Window;

            SPtr -> SAckData   = XPtr -> DataNxt - XPtr -> DataHd;
            if (SPtr -> SAckData < 0)
            {  SPtr -> SAckData =+ RBUFFSIZE;
             }

            SPtr -> SSndBuff   = XPtr -> DataTl - XPtr -> DataHd;
            if (SPtr -> SSndBuff < 0)
            {  SPtr -> SSndBuff =+ RBUFFSIZE;
             }

            SPtr -> SSegRecv   = XPtr -> SegRecv;
            SPtr -> SDupSeg    = XPtr -> DupRecv;
            SPtr -> SBusyRecv  = XPtr -> BusyRecv;
            SPtr -> SRetran    = XPtr -> RetranSeg;

            write (XPtr -> TCPCmdPort, CmdTcp, 48);

            break;
          }

         case OSTOP:
         {  /* stop traffic from host/tcc */
            ErrPtr = CmdTcp;

            if (Auth == 0)
            {  /* this command not allowed */
               ErrPtr -> YOpCode = OERR;
               ErrPtr -> YMisc = EUNAUTH;
               write(WrkPtr -> TCBPtr, CmdTcp, 6);
               break;
             }

            /* verify that user is priviledged */
            k = GetFields(PortHdr.RealUserID, &ParamVal);

            if ((k == -1)||(ParamVal[0] != 1))
            {  /* unauthorized action */
               ErrPtr -> YOpCode = OERR;
               ErrPtr -> YMisc = EUNAUTH;
               write(WrkPtr -> TCBPtr, CmdTcp, 6);
               break;
             }

            ClosePtr = CmdUsr;
            if (ClosePtr -> CHandle == 1)
            {  /* foreign socket number */
               /* check if already in table */

               k = -1;

               for (i = 0; i < NSTOP; i++)
               {  if (StopSkt[i] != 0)
                  {  if (ClosePtr -> CFinNum == StopSkt[i])
                     {  /* already in table */
                        AckPtr = CmdTcp;
                        AckPtr -> YOpCode = OACK;
                        write(WrkPtr -> TCBPtr, CmdTcp, 2);
                        break;
                      }
                   }
                  else
                  {  k = i;   /* save index */
                   }
                }

               if (k == -1)
               {  /* no space */
                  ErrPtr = CmdTcp;
                  ErrPtr -> YOpCode = OERR;
                  ErrPtr -> YMisc = EBADPARAM;
                  write(WrkPtr -> TCBPtr, CmdTcp, 6);

                  break;
                }
               else
               {  StopSkt[k] = ClosePtr -> CFinNum;

                  AckPtr = CmdTcp;
                  AckPtr -> YOpCode = OACK;
                  write(WrkPtr -> TCBPtr, CmdTcp, 2);

                  break;
                }
             }
            else
            {  /* TCC specified */
               /* check if already in table */

               k = -1;
               j = ClosePtr -> CFinNum;

               for (i = 0; i < NSTOP; i++)
               {  if (StopTcc[i] < 0)
                  {  if (j == (StopTcc[i]&0377))
                     {  /* already in table */
                        AckPtr = CmdTcp;
                        AckPtr -> YOpCode = OACK;
                        write(WrkPtr -> TCBPtr, CmdTcp, 2);
                        break;
                      }
                   }
                  else
                  {  k = i;   /* save index */
                   }
                }

               if (k == -1)
               {  /* no space */
                  ErrPtr = CmdTcp;
                  ErrPtr -> YOpCode = OERR;
                  ErrPtr -> YMisc = EBADPARAM;
                  write(WrkPtr -> TCBPtr, CmdTcp, 6);

                  break;
                }
               else
               {  StopTcc[k] = j + 0100000;

                  AckPtr = CmdTcp;
                  AckPtr -> YOpCode = OACK;
                  write(WrkPtr -> TCBPtr, CmdTcp, 2);

                  break;
                }
             }
          }

         case ORESUME:
         {  /* resume traffic from host/tcc */
            if (Auth == 0)
            {  /* this command not allowed */
               ErrPtr = CmdTcp;
               ErrPtr -> YOpCode = OERR;
               ErrPtr -> YMisc = EUNAUTH;
               write(WrkPtr -> TCBPtr, CmdTcp, 6);
               break;
             }

            k = GetFields(PortHdr.RealUserID, &ParamVal);

            if ((k == -1)||(ParamVal[0] != 1))
            {  /* unauthorized action */
               ErrPtr -> YOpCode = OERR;
               ErrPtr -> YMisc = EUNAUTH;
               write(WrkPtr -> TCBPtr, CmdTcp, 6);
               break;
             }

            ClosePtr = CmdUsr;
            if (ClosePtr -> CHandle == 1)
            {  /* foreign socket number */
               /* find in table */

               for (i = 0; i < NSTOP; i++)
               {  if (StopSkt[i] != 0)
                  {  if (ClosePtr ->CFinNum == StopSkt[i])
                     {  StopSkt[i] = 0;

                        AckPtr = CmdTcp;
                        AckPtr -> YOpCode = OACK;
                        write(WrkPtr -> TCBPtr, CmdTcp, 2);

                        break;
                      }
                   }
                }

               /* must not have been there */

               ErrPtr = CmdTcp;
               ErrPtr -> YOpCode = OERR;
               ErrPtr -> YMisc = EBADPARAM;
               write(WrkPtr -> TCBPtr, CmdTcp, 6);

               break;
             }
            else
            {  /* TCC specified */
               /* find in table */
               j = ClosePtr -> CFinNum;
               for (i = 0; i < NSTOP; i++)
               {
                  if (StopTcc[i] < 0)
                  {
                     if (j == (StopTcc[i]&0377))
                     {  StopTcc[i] = 0;

                        AckPtr = CmdTcp;
                        AckPtr -> YOpCode = OACK;
                        write(WrkPtr -> TCBPtr, CmdTcp, 2);

                        break;
                      }
                   }
                }

               /* must not have been there */

               ErrPtr = CmdTcp;
               ErrPtr -> YOpCode = OERR;
               ErrPtr -> YMisc = EBADPARAM;
               write(WrkPtr -> TCBPtr, CmdTcp, 6);

               break;
             }
          }

         default:
         {  printf("CmdIn: bad opcode\n");
            break;
          }
       }
    }
 }

/*  */
/* Subroutine which handles data from the user */

DataIn(WrkPtr)

struct WorkBlk *WrkPtr;

{  struct MSend *SendPtr;
   struct PMask1 *SecPtr;
   int DatCnt;
   int Cap[2];
   struct TcpTcb *XPtr;
   char *InAddr;
   int SpcAvl;
   int TailSpc;
   struct WorkBlk *APtr;
   int Error;
   int d;
   int RemData;
   int Secur;
   int i;
   int NewData;


   /* DataState format

   +--------+----+----+
   ! secur  !eol !sta !   (bits: 8, 4, 4)
   +--------+----+----+

   */

   /* FlagBuff format

   +-----+---+
   !secur!eol!    (bits: 5, 3)
   +-----+---+

   */


   XPtr = WrkPtr -> TCBPtr;

   if (XPtr -> State == CLOSED)
   {  return(0);
    }

   capac(WrkPtr -> PortFds, Cap, 4);

   if (Debug == 1)
   {  printf("DataIn: entered\n");
      /*
      printf("DataIn: %d %d %d %d %d\n", Cap[0], XPtr -> DataState,
      XPtr -> DataHd, XPtr -> DataTl, XPtr -> DataNxt);
      */
    }

   if (Cap[0] == 0)
   {  return(0);    /* no data to read */
    }

   /*
   if (Debug == 1)
   {  XPtr = WorkPtr -> TCBPtr;
      printf("%d %d %d %d\n", XPtr -> DataState,
      XPtr -> DataHd, XPtr -> DataTl, XPtr -> DataNxt);
      for (i = 0; i < RBUFFSIZE; i++)
      {  printf("%o\n", XPtr -> FlagBuff[i]);
       }
    }
   */

   /* calculate space available in input buffer */
   SpcAvl = (XPtr -> DataHd) - (XPtr -> DataTl) -1;

   if (SpcAvl < 0)
   {  /* wrap around */
      SpcAvl =+ RBUFFSIZE;
    }

   if (SpcAvl == 0)
   {  /* input buffer full */
      return(1);   /* leave work block on queue */
    }

   NewData = 0;

   /* check if FIN flag set */
   if (((XPtr -> Flags)&010) > 0)
   {  /* check if all data has been received from user */

      RemData = XPtr -> DataTl - XPtr -> DataHd;
      if (RemData < 0)
      {  RemData =+ RBUFFSIZE;
       }

      if ((XPtr -> LeftSeq + RemData) == (XPtr -> SndFSN))
      {  /* incoming data is beyond the close point! */
         return(0);    /* flush it */
       }
    }

   /* read in all data possible */

   while((SpcAvl > 0)&&(Cap[0] > 0))

   {  /* determine state of input stream */
      switch ((XPtr -> DataState)&017)

      {  case 0:    /* read control message */
         {  /* read in message header */
            d = read(WrkPtr -> PortFds, &PortHdr, HDRLNGTH);
            DatCnt = PortHdr.DCount&~0100000;
            if (DatCnt == 0)
            {  /* user closed his data port */
               /* printf("DataIn: user closed data port to tcp\n"); */
               return(0);
             }
            /* read in the rest of the message */
            d = read(WrkPtr -> PortFds, CmdUsr, DatCnt);

            /* extract the byte count of data message */
            SendPtr = CmdUsr;
            Cap[0] =- (DatCnt + HDRLNGTH);
            /* set state = 1; save EOL, security info */
            XPtr -> DataState = ((SendPtr -> SnSecur) << 8) +
            ((SendPtr -> SnFlags) << 4) + 1;

            /* check if security level is within range */
            XPtr -> Flags =& ~0200;
            if (Auth == 1)
            {
               if ((SendPtr -> SnSecur > XPtr -> ScMaxOut)||
               (SendPtr -> SnSecur < XPtr -> ScMinOut))
               {  /* set flag to make data be discarded */
                  XPtr -> Flags =| 0200;   /* discard data */
                }
             }
            break;
          }

         case 1:    /* read in the data message */
         {  /* read in the header */
            d = read(WrkPtr -> PortFds, &PortHdr, HDRLNGTH);

            /* set state = 2, resid data */
            /* need to increment to preserve security/flag info */
            XPtr -> DataState =+ 1;
            XPtr -> DataLeft = PortHdr.DCount&~0100000;
            Cap[0] =- HDRLNGTH;

            break;
          }

         case 2:    /* read some residual data in port */
         {
            DatCnt = XPtr -> DataLeft;

            /* check if need to discard */
            if ((Auth == 1)&&(((XPtr -> Flags)&0200) > 0))
            {  /* security level wrong - discard data */

               /* save the count of discarded data */
               SecPtr = CmdTcp;
               SecPtr -> YMisc = XPtr -> DataLeft;

               Cap[0] =- DatCnt;

               /* flush the data */
               while ((XPtr -> DataLeft) > TCPINSZ)
               {  read(WrkPtr -> PortFds, TcpIn, TCPINSZ);
                  XPtr -> DataLeft =- TCPINSZ;
                }
               read(WrkPtr -> PortFds, TcpIn, XPtr -> DataLeft);

               /* inform user of illegal security level */

               SecPtr -> YOpCode = OSECERR;
               SecPtr -> YHandle = XPtr -> ConnHandle;
               write(XPtr -> TCPCmdPort, CmdTcp, 6);

               /* return datain state to zero */
               XPtr -> DataState = 0;
             }

            else
            {  /* read in the residual data */
               /* read in only enough to fill buffer */

               InAddr = &(XPtr -> RetranBuff[XPtr -> DataTl]);

               /* calculate space remaining in tail of circular buffer */

               if (XPtr -> DataTl >= XPtr -> DataHd)
               {  TailSpc = RBUFFSIZE - (XPtr -> DataTl);
                  if (XPtr -> DataHd == 0)
                  {  TailSpc =- 1;    /* special case */
                   }
                }
               else
               {  TailSpc = (XPtr -> DataHd) - (XPtr -> DataTl) - 1;
                }

               if (DatCnt <= TailSpc)
               {  d = read(WrkPtr -> PortFds, InAddr, DatCnt);

                  Cap[0] =- DatCnt;
                  SpcAvl =- DatCnt;
                  NewData = 1;

                  /* set EOL, security levels here */

                  if (Auth == 1)
                  {  Secur = ((XPtr -> DataState) >> 5)&0170;
                     for (i = XPtr -> DataTl; i < (XPtr -> DataTl) + DatCnt; i++)
                     {  XPtr -> FlagBuff[i] = Secur;
                      }

                     /* check for EOL */
                     if ((((XPtr -> DataState) >> 4)&01) == 1)
                     {  /* set EOL Flag */
                        XPtr -> FlagBuff[i-1] =| 1;
                      }
                   }

                  (XPtr -> DataTl) =+ DatCnt;

                  if ((XPtr -> DataTl) == RBUFFSIZE)
                  {  XPtr -> DataTl = 0;
                   }

                  /* return data-in state to zero */
                  XPtr -> DataState = 0;

                }

               else
               {  d = read(WrkPtr -> PortFds, InAddr, TailSpc);

                  Cap[0] =- TailSpc;
                  SpcAvl =- TailSpc;
                  NewData = 1;

                  /* set security level */
                  if (Auth == 1)
                  {  Secur = ((XPtr -> DataState) >> 5)&0170;
                     for (i = XPtr -> DataTl; i < (XPtr -> DataTl) + TailSpc; i++)
                     {  XPtr -> FlagBuff[i] = Secur;
                      }
                   }

                  XPtr -> DataLeft =- TailSpc;

                  XPtr -> DataTl =+ TailSpc;
                  if ((XPtr -> DataTl) == RBUFFSIZE)
                  {  XPtr -> DataTl = 0;
                   }
                }
             }
            break;
          }
       }
    }

   if (NewData == 1)

   {  if ((XPtr -> State == ESTAB)|(XPtr -> State == CLOSEWAIT))
      {  MakeWrk(0, NETOUTHP, XPtr, NetFdsW);
       }
    }

   if (Cap[0] == 0)
   {  return(0);    /* remove work block */
    }
   return(1);    /* leave work on queue */
 }


/*  */
/* Signal trapping subroutines */

sg1()
{  printf("signal 1\n");
   signal(1,&sg1);
 }

sg2()
{  printf("signal 2\n");
   signal(2,&sg2);
 }

sg3()
{  printf("signal 3\n");
   signal(3,&sg3);
 }

sg8()
{  printf("signal 8\n");
   LogEnd();
   Status();
   exit();     /* halt */
 }

sg13()
{  printf("signal 13\n");
   signal(13,&sg13);
 }



/*  */
/* Subroutine for parsing "tcptable" */

ReadTbl()
{
   extern int LocNet;
   extern int LocHost;
   extern int LocImp;
   extern int InLink;
   extern int ANLngth;
   extern int NetTable[NUMNET];

   int GtHost;
   int GtImp;
   int State;
   int i, k;
   char Chr;
   char Numb[12];
   int Param[NUMTCPTBL];

   struct buf
   {  int fildes;
      int nunused;
      char *xfree;
      char buff[512];
    } ibuf;


   /* first line of table has the following example format :

   Net Host Imp Link ANLngth GtHost GtImp : 10 0 63 155 2 2 5

   where:

   Net     : the local net tcp lives on
   Host    : the local host tcp lives on
   Imp     : the local imp tcp host is attached to
   Link    : the link number for internet traffic
   ANLngth : the Arpanet leader length in words
   GtHost  : the smart gateway host to forward to unknown nets
   GtImp   : the imp that the smart gateway host is attached to

   */

   /* The next N lines contain gateway routing information.

   Example:

   arpanet -> rccnet           :1: 10 3 2 5

   where:

   the parameter in colon brackets is the line enable bit,
   with a 1 => use this line and 0 => ignore this line.

   format of following parameters:

   1. from net
   2. to net
   3. gateway host
   4. gateway imp

   */


   i = fopen("tcptable", &ibuf);
   if (i == -1)
   {  printf("make me a tcptable!\n");
      exit();    /* die */
    }

   State = 6;  /* read the first line */

   while((Chr = getc(&ibuf)) != -1)
   {
      switch(State)
      {  case 0:    /* looking for colon */
         {  if (Chr == ':')
            {  State = 1;
             }
            break;
          }

         case 1:    /* get enable bit */
         {  if (Chr == '0')
            {  /* this line is disabled */
               State = 2;    /* flush line */
             }
            else
            {  State = 3;    /* look for colon */
             }
            break;
          }

         case 2:    /* flush line */
         {  if (Chr == '\n')
            {  State = 0;
             }
            break;
          }

         case 3:   /* look for terminating colon */
         {  if (Chr == ':')
            {  State = 4;    /* get parameters */
               i = 0;
               k = 0;
             }
            else
            {  return(-1);
             }
            break;
          }

         case 4:    /* get parameters */
         {  if ((Chr != ' ')&&(Chr != '\t'))
            {  State = 5;    /* stop at next blank/tab */
             }
            Numb[i] = Chr;
            i++;

            if (i == 12)
            {  return(-1);
             }

            break;
          }

         case 5:    /* wait for blank/tab/nl */
         {  if ((Chr == ' ')||(Chr == '\t')||(Chr == '\n'))
            {  Numb[i] = '\0';
               Param[k] = atoi(Numb);
               if (k == 0)
               {  if (Param[0] != LocNet)
                  {  State = 2;    /* flush line */
                     break;
                   }
                }
               k++;

               if ((k == 4)||(Chr == '\n'))
               {  /* enter in gateway table */

                  if (Param[1] >= NUMNET)
                  {  return(-1);
                   }

                  NetTable[Param[1]] = (Param[2] << 8) + Param[3];

                  if (Chr == '\n')
                  {  State = 0;
                     break;
                   }
                  State = 2;    /* flush line */
                  break;
                }

               i = 0;
               State = 4;   /* get next parameter */
             }
            else
            {  Numb[i] = Chr;
               i++;

               if (i == 12)
               {  return(-1);
                }
             }
            break;
          }

         case 6:    /* looking for first line colon */
         {  if (Chr == ':')
            {  State = 7;

               for (i = 0; i < NUMTCPTBL; i++)
               {  Param[i] = 0;
                }

               i = 0;
               k = 0;
             }
            break;
          }

         case 7:    /* getting parameters */
         {  if ((Chr != ' ')&&(Chr != '\t'))
            {  State = 8;    /* stop at next blank/tab */
             }

            Numb[i] = Chr;
            i++;

            if (i == 12)
            {  return(-1);
             }
            break;
          }

         case 8:    /* wait for blank/tab/nl */
         {  if ((Chr == ' ')||(Chr == '\t')||(Chr == '\n'))
            {  Numb[i] = '\0';

               i = 0;
               Param[k] = atoi(Numb);
               k++;
               if ((k == NUMTCPTBL)||(Chr == '\n'))
               {  /* set up tcp network values */

                  LocNet  = Param[0];
                  LocHost = Param[1];
                  LocImp  = Param[2];
                  InLink  = Param[3];
                  ANLngth = Param[4];
                  GtHost  = Param[5];
                  GtImp   = Param[6];

                  if (Chr == '\n')
                  {  State = 0;    /* get gateway connectivity values */
                     if (k != NUMTCPTBL)
                     {  return(-1);
                      }
                     break;
                   }
                  State = 2;    /* flush line */
                }
               else
               {  State = 7;    /* get next parameter */
                }
             }
            else
            {  Numb[i] = Chr;
               i++;
               if (i == 12)
               {  return(-1);
                }
             }
            break;
          }
       }
    }

   if (NetTable[LocNet] == 0)
   {  /* set flag sending net output directly to host */
      NetTable[LocNet] = -1;
    }

   /* for unknown nets - send to more intelligent gateway */

   for (i = 0; i < NUMNET; i++)
   {  if (NetTable[i] == 0)
      {  NetTable[i] = (GtHost << 8) + GtImp;
       }
    }

   close(ibuf.fildes);

   return(0);

 }



/*  */
/* Subroutine for putting a work block on a work queue */

MakeWrk(Queue, WFlags, WTcbPtr, WFds)

int Queue;             /* specifies the particular work queue */
int WFlags;
struct TcpTcb *WTcbPtr;
int WFds;

{  register struct WorkBlk *TempPtr;
   int Stop;

   /* check if duplicate entry */

   TempPtr = WorkHead -> NextWork;
   Stop = 0;

   while ((TempPtr -> NextWork != 0)&&(Stop == 0))
   {
      if ((TempPtr -> PortFlags == WFlags)&&
      (TempPtr -> TCBPtr == WTcbPtr)&&(TempPtr -> PortFds == WFds))
      {  /* found duplicate entry */
         /* delete old block */
         (TempPtr -> NextWork) -> LastWork = TempPtr -> LastWork;
         (TempPtr -> LastWork) -> NextWork = TempPtr -> NextWork;
         TempPtr -> NextWork = WorkAvail;
         WorkAvail = TempPtr;
         Stop = 1;
       }
      TempPtr = TempPtr -> NextWork;
    }

   if (WorkAvail == 0)
   {  /* no more blocks */
      printf("MakeWrk: no more work blocks\n");
      return(ENOBUFS);
    }

   TempPtr = WorkAvail;
   WorkAvail = WorkAvail -> NextWork;

   TempPtr -> LastWork = WorkTail -> LastWork;
   TempPtr -> NextWork = WorkTail;
   (WorkTail -> LastWork) -> NextWork = TempPtr;
   WorkTail -> LastWork = TempPtr;

   /* fillin the block */
   TempPtr -> PortFlags = WFlags;
   TempPtr -> TCBPtr = WTcbPtr;
   TempPtr -> PortFds = WFds;

   return(0);
 }

/*  */
/* Subroutine for unsigned long compare */

ULCompare(A, B)

long *A, *B;
{  /* returns: 0 = equal */
   /*             1 = A > B */
   /*             2 = A < B */

   if (*A == *B)
   {  return(0);
    }
   if ((*A - *B) < 0)
   {  return(2);
    }
   return(1);
 }


/*  */
/* Subroutine for putting a TCB on the retransmission queue */

QTcb(XPtr)

struct TcpTcb *XPtr;

{  int First;
   int Time;
   struct TcpTcb *SPtr;
   int Stop;
   int AbsTime;


   /* check debug parameter */
   if (NoRetran == 1)
   {  /* no retransmissions allowed today */
      return(0);
    }

   /* check if TCB is already on retransmission queue */

   if (XPtr -> NxtRetran != 0)
   {  /* already linked in */
      return(0);
    }

   /* determine next retransmission time */

   if (XPtr -> NumRetran >= RetCount)
   {  /* go into slow mode */
      Time = *TimePtr + RetSlow;
    }
   else
   {  /* still in fast mode */
      Time = *TimePtr + RetFast;
    }

   if (RetranHd == 0)
   {  /* empty queue */
      RetranHd = XPtr;
      XPtr -> NxtRetran = XPtr;
      XPtr -> LstRetran = XPtr;
      XPtr -> RetranTm = Time;
      return(0);
    }

   /* find insertion point */
   First = 1;
   SPtr = RetranHd;
   AbsTime = 0;
   Stop = 0;

   do
   {  AbsTime =+ SPtr -> RetranTm;
      if (AbsTime < Time)
      {  First = 0;
         SPtr = SPtr -> NxtRetran;
       }
      else
      {  Stop = 1;
       }
    }
   while((Stop == 0)&&(SPtr != RetranHd));

   XPtr -> NxtRetran = SPtr;
   XPtr -> LstRetran = SPtr -> LstRetran;
   (SPtr -> LstRetran) -> NxtRetran = XPtr;
   SPtr -> LstRetran = XPtr;

   if (First == 1)
   {  /* TCB is at head of queue */
      XPtr -> RetranTm = Time;
      SPtr -> RetranTm =- XPtr -> RetranTm;
      RetranHd = XPtr;
      return(0);
    }

   if (SPtr == RetranHd)
   {  /* TCB is at end of queue */
      XPtr -> RetranTm = Time - AbsTime;
      return(0);
    }

   /* TCB is in middle of queue */
   XPtr -> RetranTm = Time - AbsTime + (SPtr -> RetranTm);
   SPtr -> RetranTm =- XPtr -> RetranTm;
   return(0);
 }



/*  */
/* Subroutine for removing TCB from retransmission queue */

UnQTcb(XPtr)

struct TcpTcb *XPtr;

{  /* check if not queued */
   if (XPtr -> NxtRetran == 0)
   {  return(0);   /* not queued */
    }

   /* check for single entry */
   if (XPtr -> NxtRetran == XPtr)
   {  /* just one entry in queue */
      RetranHd = 0;
      XPtr -> NxtRetran = 0;
      return(0);
    }

   (XPtr -> NxtRetran) -> LstRetran = XPtr -> LstRetran;
   (XPtr -> LstRetran) -> NxtRetran = XPtr -> NxtRetran;

   if (XPtr -> NxtRetran != RetranHd)
   {  /* update time increment */
      (XPtr -> NxtRetran) -> RetranTm =+ XPtr -> RetranTm;
    }

   if (XPtr == RetranHd)
   {  /* deleted head of queue */
      RetranHd = XPtr -> NxtRetran;
    }

   XPtr -> NxtRetran = 0;

 }




/*  */
/* Subroutine for cleaning up an aborted connection */

Abort(XPtr, Flag)

struct TcpTcb *XPtr;
int Flag;

{  struct TcpTcb *TempPtr;
   struct PMask1 *AckPtr;
   struct WorkBlk *WrkPtr;
   int Stop;
   int i,j,k;

   /* disable wakeups on data ports */
   awtdis(XPtr -> SendPort);
   awtdis(XPtr -> RecvPort);

   /* close the ports */

   close(XPtr -> SendPort);
   close(XPtr -> RecvPort);

   /* clear FdsBffer entries */

   FdsBffer[XPtr -> SendPort].BuffFlags = 0;
   FdsBffer[XPtr -> RecvPort].BuffFlags = 0;

   /* remove bit in PortMap if applicable */
   if ((XPtr -> SrcePort >= BASEPORT)&&
   (XPtr -> SrcePort < BASEPORT + 0400))
   {  /* clear the Port Map bit */

      k = 1;    /* bit index */
      j = (XPtr -> SrcePort)&017;

      for (i = 0; i < j; i++)
      {  k =<< 1;
       }

      PortMap[((XPtr -> SrcePort) >> 4)&017] =& ~k;  
    }

   /* remove TCB from retransmission Q if applicable */

   UnQTcb(XPtr);

   /* remove work queue entries associated with TCB */

   WrkPtr = WorkHead -> NextWork;

   while(WrkPtr -> NextWork != 0)
   {  TempPtr = WrkPtr -> NextWork;
      if (WrkPtr -> TCBPtr == XPtr)
      {  /* found an entry to remove */
         (WrkPtr -> NextWork) -> LastWork = WrkPtr -> LastWork;
         (WrkPtr -> LastWork) -> NextWork = WrkPtr -> NextWork;
         WrkPtr -> NextWork = WorkAvail;
         WorkAvail = WrkPtr;
       }
      WrkPtr = TempPtr;
    }

   if (Flag == 0)

   {  /* send ACK to user */

      AckPtr = CmdTcp;
      AckPtr -> YOpCode = OACK;
      AckPtr -> YHandle = XPtr -> ConnHandle;
      write(XPtr -> TCPCmdPort, CmdTcp, 4);
    }

   /* delete TCB */
   if (XPtr == TcbHd)
   {  TcbHd = XPtr -> NxtActTcb;
      XPtr -> NxtActTcb = TcbAvl;
      TcbAvl = XPtr;
    }
   else
   {  Stop = 0;
      TempPtr = TcbHd;

      while((TempPtr -> NxtActTcb != 0)&&(Stop == 0))
      {
         if (TempPtr -> NxtActTcb == XPtr)
         {  Stop = 1;
          }
         else
         {  TempPtr = TempPtr -> NxtActTcb;
          }
       }

      if (Stop == 1)
      {  /* found it, as expected */
         TempPtr -> NxtActTcb = XPtr -> NxtActTcb;
         XPtr -> NxtActTcb = TcbAvl;
         TcbAvl = XPtr;
       }
    }
 }


/* Subroutine which verifies that ptr to TCB is valid */

ChkPtr(XPtr, Fds)

struct TcpTcb *XPtr;
int Fds;

{  struct TcpTcb *TPtr;

   TPtr = TcbHd;

   while (TPtr != 0)
   {  if (XPtr == TPtr)
      {  /* check correct file desc */
         if (Fds == TPtr -> UserCmdPort)
         {  return(0);    /* everything okay */
          }
         else
         {  printf("ChkPtr: bad fds\n");
            return(-1);
          }
       }
      else
      {  TPtr = TPtr -> NxtActTcb;
       }
    }

   printf("ChkPtr: bad ptr\n");
   return(-1);
 }



Status()

{
   extern int Auth;
   extern long StopSkt[NSTOP];
   extern int StopTcc[NSTOP];

   struct TcpTcb *XPtr;
   int Cap0[2];
   int Cap1[2];
   int Cap2[2];
   int Cap3[2];
   int i;
   struct WorkBlk *WPtr;

   struct
   {  char XHost;
      char XNet;
      int XImp;
    } *SktPtr;

   /* print the capacity of the ear port */

   capac(EarFds, Cap2, 4);

   printf("Ear: %d\n", Cap2[0]);

   /* print the work queue */

   WPtr = WorkHead -> NextWork;

   while(WPtr -> NextWork != 0)
   {
      printf("WrkQ: %d %d %o\n", WPtr -> PortFlags,
      WPtr -> PortFds, WPtr -> TCBPtr);

      WPtr = WPtr -> NextWork;
    }


   /* print active entries in FdsBffer */

   for (i = 0; i < FDSSIZE; i++)
   {
      if (FdsBffer[i].BuffFlags != 0)
      {  printf("Fds: %d %d %o\n", FdsBffer[i].BuffFlags,
         FdsBffer[i].BuffFds, FdsBffer[i].BuffTcbPtr);
       }
    }


   /* print the active TCB list */

   XPtr = TcbHd;

   while (XPtr != 0)
   {
      capac(XPtr -> TCPCmdPort, Cap0, 4);
      capac(XPtr -> UserCmdPort, Cap1, 4);
      capac(XPtr -> SendPort, Cap2, 4);
      capac(XPtr -> RecvPort, Cap3, 4);

      printf("TCB: %o %d %d %d %d S: %d N: %o H: %o I: %o, PID: %d\n",
      XPtr, Cap0[1], Cap1[0], Cap2[0], Cap3[1], XPtr -> State,
      XPtr -> DstNet, XPtr -> DstHstH, XPtr -> DstHstL, XPtr -> ProcID);

      XPtr = XPtr -> NxtActTcb;

    }

   /* print the retransmission queue */

   if (RetranHd != 0)

   {  XPtr = RetranHd;

      do
      {  printf("RQ: %o %d\n", XPtr, XPtr -> RetranTm);
         XPtr = XPtr -> NxtRetran;
       }
      while(XPtr != RetranHd);
    }

   if (Auth == 1)
   {  /* print the Stop/Resume entries */
      printf("Stop Table\n");

      for (i = 0; i < NSTOP; i++)
      {  if (StopSkt[i] != 0)
         {  SktPtr = &StopSkt[i];
            printf("%d %d %d\n", SktPtr -> XNet,
            SktPtr -> XHost, SktPtr -> XImp);
          }
       }

      for (i = 0; i < NSTOP; i++)
      {  if (StopTcc[i] < 0)
         {  printf("%d ", StopTcc[i]&0377);
          }
       }
      printf("\n");
    }
 }



/* Subroutine for sending commands to user process */

CmdOut(XPtr, OpCode, Param1, Param2)

struct TcpTcb *XPtr;
int OpCode;
int Param1;
int Param2;

{  struct PMask1 *CPtr;
   struct PMask1 *EPtr;
   int DatCnt;

   CPtr = CmdTcp;
   CPtr -> YOpCode = OpCode;
   CPtr -> YHandle = XPtr -> ConnHandle;

   switch(OpCode)
   {
      case OERR:
      case OCHNG:
      case ONETMSG:
      {  CPtr -> YMisc = Param1;
         DatCnt = 6;
         break;
       }

      case ORESET:
      case OACK:
      {  DatCnt = 4;
         break;
       }

      default:
      {  printf("CmdOut: bad opcode\n");
       }
    }

   write(XPtr -> TCPCmdPort, CmdTcp, DatCnt);
 }














