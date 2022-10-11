#
# include "tcpstru.h"
# include "tcp.h"


/* misc constants */
# define MAXDATA        256     /* maximum port message data area */
# define NUMBCONNS      10      /* maximum number of connections/proc */

/* data declarations */

struct PortLdr PortHdr;

char CmdUsrName[]   CMDUSRNM;   /* user cmd port name */
char CmdTcpName[]   CMDTCPNM;   /* tcp cmd port name */
char SendName[]     SENDNM;     /* send data port name */
char RecvName[]     RECVNM;     /* recv data port name */

char CmdTcp[CMDTCPSIZE];      /* tcp command out buffer */
char CmdUsr[CMDUSRSIZE];      /* user command out buffer */

struct                     /* structure used to decompose long int */
{  int bb[2];
 } *p;


int CmdUsrFds;   /* fdesc for user cmd port */
int CmdTcpFds;  /* fdesc for tcp cmd port */

extern int errno;    /* system error number */
int tm[3];        /* used for qtime */

struct UserTcb ConnBlock[NUMBCONNS];


/* Library subroutines */

TcpInit(TimeOut)
int TimeOut;
{  int i;
   int ProcID;
   int EarFds;
   int Error;
   int More;
   struct PMask1 *InitPtr;
   struct PMask1 *AckPtr;
   struct PMask1 *ErrPtr;

   /* reset the ConnBlocks */
   for (i = 0;i < NUMBCONNS;i++)
   {  ConnBlock[i].ConnIndex = 0;
    }


   EarFds = open(EAR,1);
   if (EarFds == -1)
   {  /* can't find ear port */
      return(EEARPRT);
    }

   /* derive tcp <=> lib port names */
   ProcID = getpid();   /* get my process id */
   for (i = PRTPREFIX+2;i > PRTPREFIX-1; i--)
   {  CmdTcpName[i] = '0' + (ProcID&07);
      CmdUsrName[i] = '0' + (ProcID&07);
      SendName[i] = '0' + (ProcID&07);
      RecvName[i] = '0' + (ProcID&07);
      ProcID =>> 3;
    }
   unlink(CmdTcpName);
   CmdTcpFds = port(CmdTcpName,0170222,0);
   if (CmdTcpFds == -1)
   {  /* can't create tcp command port */
      close(EarFds);    /* for the retry */
      return(ERSPPRT);
    }

   /* send a listen down the ear */
   InitPtr = CmdUsr;
   InitPtr -> YOpCode = OINIT;
   write(EarFds,CmdUsr,2);
   /* wait for response from TCP */
   Error = ReadCmd(TimeOut);
   if (Error != 0)
   {  /* Something wrong */
      close(EarFds);    /* for the retry */
      close(CmdTcpFds);    /* for the retry */
      return(Error);
    }
   close(EarFds);       /* close the ear port */
   AckPtr = CmdTcp;
   switch(AckPtr -> YOpCode)
   {  case OERR:
      {  /* error from tcp */
         return(ErrPtr -> YMisc);
       }
      case OACK:

      {  CmdUsrFds = open(CmdUsrName,1);   /* open the user command port */
         if (CmdUsrFds == -1)
         {  /* no user command port */
            close(EarFds);    /* for the retry */
            close(CmdTcpFds);    /* for the retry */
            return(ECMDPRT);
          }

         unlink(CmdUsrName);
         unlink(CmdTcpName);

         return(0);
       }
    }
 }


TcpQuit()
{  int i;
   int Error;


   /* disable wakeups on command ports */
   awtdis(CmdUsrFds);
   awtdis(CmdTcpFds);

   /* search down UserTcb's for active connections */
   for (i = 0; i < NUMBCONNS; i++)
   {  if (ConnBlock[i].ConnIndex != 0)
      {  /* found an active connection block */

         ConnBlock[i].ConnIndex = 0;

       }
    }
   /* close out command ports */
   close(CmdUsrFds);
   close(CmdTcpFds);
 }



TcpOpen(CABPtr, ConnPtr, Mode, TimeOut)
struct CAB *CABPtr;
int *ConnPtr;
int Mode;
int TimeOut;

{  int Fdesc;
   int More;
   int i;
   int j;
   int Error;
   int Found;
   int Index;
   struct UserTcb *x;
   struct PMask1 *AckPtr;
   struct PMask1 *ErrPtr;
   struct MOpen *OpenPtr;


   if (CABPtr -> TccCnt > NUMBTCC)
   {  /* too many */
      return(EBADPARAM);
    }

   if (Mode > 2)
   {  /* illegal mode */
      return(EBADPARAM);
    }

   /* find an unused ConnBlock */
   Found = 0;
   Index = 0;
   while ((Index < NUMBCONNS)&&(Found == 0))
   {  if (ConnBlock[Index].ConnIndex == 0)
      {  Found = 1;
       }
      else
      {  Index++;
       }
    }
   if (Found == 0)
   {  /* too many connections for this process */
      return(EUSRC);
    }

   x = *ConnPtr = &ConnBlock[Index];   /* give user ptr to ConnBlock */

   /* initialize the UserTcb */
   x -> CNFlags = 0;
   x -> CNState = 0;
   x -> CNPrec = 0;
   x -> CNMsg = 0;
   x -> ResidData = 0;
   x -> SendCapac = 0;
   x -> RecvCapac = 0;
   x -> SndCount = 0;
   x -> RcvCount = 0;
   x -> UrgCount = 0;

   ConnBlock[Index].CmdFds = CmdTcpFds;   /* enter tcp command port fds */

   OpenPtr = CmdUsr;    /* inz the open mask ptr */

   OpenPtr -> OpCode = OOPEN;
   OpenPtr -> OpenMode = Mode;
   OpenPtr -> OHandle = Index;
   OpenPtr -> Net = CABPtr -> Network;
   OpenPtr -> HostH = CABPtr -> HostHigh;
   OpenPtr -> HostL = CABPtr -> HostLow;
   OpenPtr -> SrcPrt = CABPtr -> SrcPort;
   OpenPtr -> DstPrt = CABPtr -> DstPort;
   OpenPtr -> SendPrec = CABPtr -> PrecSend;
   OpenPtr -> RecvPrec = CABPtr -> PrecRecv;
   OpenPtr -> MaxSecur = CABPtr -> SecurMax;
   OpenPtr -> AbsSecur = CABPtr -> SecurAbs;
   OpenPtr -> NumbTcc = CABPtr -> TccCnt;

   /* copy the list of TCCs */
   for (i = 0; i < CABPtr -> TccCnt; i++)
   {
      OpenPtr -> OTcc[i] = CABPtr -> TCC[i];
    }


   /* derive the data send and receive port names */
   j = Index;
   for (i = PRTPREFIX+5;i > PRTPREFIX+2;i--)
   {  SendName[i] = '0' + (j&07);
      RecvName[i] = '0' + (j&07);
      j =>> 3;
    }


   /* create the receive data port */
   unlink(RecvName);
   Fdesc = port(RecvName,0170222,0);
   if (Fdesc == -1)
   {  /* couldn't create receive port */
      printf("Open1: errno = %d\n", errno);

      /* return ConnBlock to free list */
      ConnBlock[Index].ConnIndex = 0;

      return(ERCVPRT);
    }

   ConnBlock[Index].RecvFds = Fdesc;   /* enter fds for receive */

   /* send OPEN to tcp */
   write(CmdUsrFds,&CmdUsr,(19 + (OpenPtr -> NumbTcc)));

   /* wait for response from TCP */
   Error = ReadCmd(TimeOut);
   if (Error != 0)
   {  /* Something happened */

      /* return ConnBlock to free list */
      ConnBlock[Index].ConnIndex = 0;
      close(Fdesc);
      return(Error);
    }

   ErrPtr = CmdTcp;
   AckPtr = CmdTcp;
   switch (AckPtr -> YOpCode)
   {  case OERR:
      {  /* something wrong with open */

         /* return ConnBlock to free list */
         ConnBlock[Index].ConnIndex = 0;
         close(Fdesc);
         return(ErrPtr -> YMisc);
       }
      case OACK:
      {  /* open the send port */
         Fdesc = open(SendName,1);
         if (Fdesc == -1)
         {  /* couldn't open the send port */
            printf("Open2: errno = %d\n", errno);

            /* return ConnBlock to free list */
            ConnBlock[Index].ConnIndex = 0;
            close(Fdesc);
            return(ESNDPRT);
          }

         unlink(SendName);
         unlink(RecvName);

         ConnBlock[Index].SendFds = Fdesc;
         ConnBlock[Index].ConnIndex = AckPtr -> YHandle;
         return(0);
       }
    }
 }

TcpClose(ConnPtr, TimeOut)

struct UserTcb *ConnPtr;
int TimeOut;

{  int Error;
   struct MClose *ClosePtr;

   if (ConnPtr -> ConnIndex == 0)
   {  /* connection doesn't exist */
      return(ENOCONN);
    }

   ClosePtr = CmdUsr;
   ClosePtr -> COpCode = OCLOSE;
   ClosePtr -> CHandle = ConnPtr -> ConnIndex;
   ClosePtr -> CFinNum = ConnPtr -> SndCount;
   write(CmdUsrFds, CmdUsr, 8);
   Error = ReadCmd(TimeOut);
   return(Error);
 }



TcpSend(ConnPtr,BuffPtr,Count,Security,Eol,Urgent)

struct UserTcb *ConnPtr;
char *BuffPtr;
int Count;
int Security;
int Eol;
int Urgent;

{  int ByteCnt[2];
   struct SendBlock *SendPtr;
   long Temp;
   struct MUrgent *UrgPtr;
   int d;

   if (ConnPtr -> ConnIndex == 0)
   {  /* connection doesn't exist */
      return(ENOCONN);
    }

   /* prepare a long with Count */
   p = &Temp;
   p -> bb[0] = 0;
   p -> bb[1] = Count;

   if (Urgent != 0)
   {  /* urgent information in data */
      UrgPtr = CmdUsr;
      UrgPtr -> UOpCode = OURGENT;
      UrgPtr -> UHandle = ConnPtr -> ConnIndex;
      UrgPtr -> UByteNo = ConnPtr -> SndCount + Temp;
      write(CmdUsrFds, CmdUsr, 8);
    }

   capac(ConnPtr -> SendFds, ByteCnt, 4);
   if (ByteCnt[1] < Count + 2*HDRLNGTH + 4)
   {  /* not enough space in port */
      return(ENOSPC);
    }

   ConnPtr -> SndCount =+ Temp;

   /* create and send the header message */

   SendPtr = CmdUsr;
   SendPtr -> SnFlags = Eol + (Urgent << 1);
   SendPtr -> SnSecur = Security;
   SendPtr -> SnByteCnt = Count;

   d = write(ConnPtr -> SendFds,CmdUsr,4);

   /* write the actual data */
   d = write(ConnPtr -> SendFds,BuffPtr,Count);

   return(0);
 }



TcpReceive(ConnPtr, RecvPtr, MaxCount, RealCount, UrgentFlag)

struct ConnBlock *ConnPtr;
int *RecvPtr;
int MaxCount;
int *RealCount;
int *UrgentFlag;

{  int ByteCnt[2];
   int ReadCnt;
   long Temp;

   if (ConnPtr -> ConnIndex == 0)
   {  /* connection doesn't exist */
      return(ENOCONN);
    }

   if (ConnPtr -> ResidData > 0)
   {  /* data left in port from previous msg */
      if (ConnPtr -> ResidData > MaxCount)
      {  read (ConnPtr -> RecvFds, RecvPtr, MaxCount);
         *RealCount = MaxCount;
         ConnPtr -> ResidData =- MaxCount;
       }
      else
      {  read(ConnPtr -> RecvFds, RecvPtr, ConnPtr -> ResidData);
         *RealCount = ConnPtr -> ResidData;
         ConnPtr -> ResidData = 0;
       }
    }
   else
   {  capac(ConnPtr -> RecvFds, ByteCnt, 4);

      if (ByteCnt[0] == 0)
      {  /* no data present */
         *RealCount = 0;
         return(ENODATA);
       }
      read(ConnPtr -> RecvFds, &PortHdr, HDRLNGTH);    /* get header */
      ReadCnt = PortHdr.DCount&077777;      /* mask out EOM bit */

      if (ReadCnt == 0)
      {  /* receive port closed */
         return(ERCVPRT);
       }

      if (ReadCnt > MaxCount)
      {  /* read only amount allowed */
         read(ConnPtr -> RecvFds, RecvPtr, MaxCount);
         *RealCount = MaxCount;
         ConnPtr -> ResidData = ReadCnt - MaxCount;
       }
      else
      {  read(ConnPtr -> RecvFds, RecvPtr, ReadCnt);
         *RealCount = ReadCnt;
       }
    }

   /* compare urgent count with current recv byte count */
   /* these tests will fail after about 2 billion bytes are
   sent over a connection */

   /* this is known to be beyond the MTBF of the system */

   *UrgentFlag = 0;
   if ((ConnPtr -> RcvCount) < (ConnPtr -> UrgCount))
   {  *UrgentFlag = 1;
    }

   /* update received byte count */
   /* this wierdness required as you can't add longs and ints */

   p = &Temp;
   p -> bb[0] = 0;
   p -> bb[1] = *RealCount;

   ConnPtr -> RcvCount =+ Temp;

   return(0);
 }


TcpStatus(ConnPtr, StatPtr, TimeOut)

struct ConnBlock *ConnPtr;
struct ConnStat *StatPtr;
int TimeOut;

{  struct PMask1 *SPtr;
   int ByteCnt[2];
   int Count;

   if (ConnPtr -> ConnIndex == 0)
   {  /* connection doesn't exist */
      return(ENOCONN);
    }

   /* send request to tcp */
   SPtr = CmdUsr;
   SPtr -> YOpCode = OSTAT;
   SPtr -> YHandle = ConnPtr -> ConnIndex;
   write(CmdUsrFds, CmdUsr, 4);

   while(1)     /* exits by return */

   {  capac(CmdTcpFds, ByteCnt, 4);
      if (ByteCnt[0] == 0)
      {  sleep(1);     /* one second */
         if (TimeOut == 0)
         {  return(ETCPNRDY);
          }
         TimeOut--;
       }
      else
      {  read(CmdTcpFds, &PortHdr, HDRLNGTH);

         Count = PortHdr.DCount&~0100000;

         read(CmdTcpFds, CmdTcp, 2);

         /* check opcode here */

         read (CmdTcpFds, StatPtr, Count-2);
         return(0);
       }
    }
 }

TcpAbort(ConnPtr, TimeOut)

struct ConnBlock *ConnPtr;
int TimeOut;

{  int Error;
   struct PMask1 *AbortPtr;

   if (ConnPtr -> ConnIndex == 0)
   {  /* connection doesn't exist */
      return(ENOCONN);
    }

   AbortPtr = CmdUsr;
   AbortPtr -> YOpCode = OABORT;
   AbortPtr -> YHandle = ConnPtr -> ConnIndex;
   write(CmdUsrFds, CmdUsr, 4);

   Error = ReadCmd(TimeOut);
   if (Error == 0)
   {  /* clean up library side of connection */
      awtdis(ConnPtr -> RecvFds);
      awtdis(ConnPtr -> SendFds);

      close(ConnPtr -> RecvFds);
      close(ConnPtr -> SendFds);

      ConnPtr -> ConnIndex = 0;    /* delete UserTcb */
    }
   return(Error);
 }



TcpCmd(CmdPtr)

struct CmdBlk *CmdPtr;

{  int ByteCnt[2];
   int i;
   int Count;
   struct MUrgent *UrgPtr;
   struct PMask1 *ChngPtr;
   struct PMask1 *PPtr;
   struct PMask1 *LPtr;
   struct PMask1 *ResetPtr;
   struct PMask1 *RejectPtr;
   struct MUpdate *UPtr;

   CmdPtr -> CFlags = 0;

   /* first check the UserTcb's for outstanding events */

   for (i = 0; i < NUMBCONNS; i++)
   {  if ((ConnBlock[i].CNFlags != 0)&&(ConnBlock[i].ConnIndex != 0))
      {  /* found an entry */
         CmdPtr -> XPtr = &ConnBlock[i];
         CmdPtr -> NewState = ConnBlock[i].CNState;
         CmdPtr -> NewPrec = ConnBlock[i].CNPrec;
         CmdPtr -> NetMsg = ConnBlock[i].CNMsg;
         CmdPtr -> CFlags =| ConnBlock[i].CNFlags;
         ConnBlock[i].CNFlags = 0;    /* reset flags */
         return(0);                 /* normal 'found entry' return */
       }
    }

   /* if we made it to here, there were no UserTcb's with info */
   /* now check the Tcp Cmd Port */

   capac(CmdTcpFds, ByteCnt, 4);
   if (ByteCnt[0] == 0)
   {  return(ENOCHNG);
    }

   /* something here */

   read(CmdTcpFds, &PortHdr, HDRLNGTH);
   Count = PortHdr.DCount&~0100000;      /* remove EOM bit */

   if (Count == 0)
   {  /* port closed by tcp */
      return(ECMDCLS);
    }

   read(CmdTcpFds, CmdTcp,Count);

   ChngPtr = CmdTcp;
   switch(ChngPtr -> YOpCode)

   {  case OCHNG:
      {  CmdPtr -> NewState = ChngPtr -> YMisc;
         CmdPtr -> CFlags =| 01;   /* set flag */
         CmdPtr -> XPtr = &ConnBlock[ChngPtr -> YHandle];
         ConnBlock[ChngPtr -> YHandle].CNState = ChngPtr -> YMisc;
         return(0);
       }

      case OURGENT:
      {  UrgPtr = CmdTcp;

         if ((UrgPtr -> UByteNo) > (ConnBlock[UrgPtr -> UHandle].RcvCount))
         {  /* go into urgent mode */
            CmdPtr -> CFlags =| 02;      /* set flag */
            ConnBlock[UrgPtr -> UHandle].UrgCount = UrgPtr -> UByteNo;
            CmdPtr -> XPtr = &ConnBlock[UrgPtr -> UHandle];
            return(0);
          }
         return(ENOCHNG);
       }

      case OSECERR:
      {  /* bad security level */
         LPtr = CmdTcp;
         CmdPtr -> XPtr = &ConnBlock[LPtr -> YHandle];
         CmdPtr -> CFlags =| 04;   /* set flag */

         /* decrement the number of bytes sent */
         ConnBlock[LPtr -> YHandle].SndCount =- LPtr -> YMisc;

         return(0);
       }

      case OPROCNR:
      {  /* foreign process not responding */
         LPtr = CmdTcp;
         CmdPtr -> XPtr = &ConnBlock[LPtr -> YHandle];
         CmdPtr -> CFlags =| 010;   /* set flag */
         return(0);
       }

      case OPROCR:
      {  /* foreign process responding again */
         PPtr = CmdTcp;
         CmdPtr -> XPtr = &ConnBlock[PPtr -> YHandle];
         CmdPtr -> CFlags =| 020;   /* set flag */
         return(0);
       }

      case ONETMSG:
      {  /* net message arrived */
         LPtr = CmdTcp;
         CmdPtr -> XPtr = &ConnBlock[LPtr -> YHandle];
         CmdPtr -> NetMsg = LPtr -> YMisc;
         CmdPtr -> CFlags =| 040;    /* set flag */
         return(0);
       }

      case ORESET:
      {  /* connection reset */
         ResetPtr = CmdTcp;
         CmdPtr -> XPtr = &ConnBlock[ResetPtr -> YHandle];
         CmdPtr -> CFlags =| 0100;    /* set flag */

         /* delete the connection in case of preemption */
         ConnBlock[ResetPtr -> YHandle].ConnIndex = 0;

         return(0);
       }

      case OREJECT:
      {  /* foreign TCP rejecting */
         RejectPtr = CmdTcp;
         CmdPtr -> XPtr = &ConnBlock[RejectPtr -> YHandle];
         CmdPtr -> CFlags =| 0200;    /* set flag */
         return(0);
       }

      case OPRECCHNG:
      {  /* send precedence level raised */
         LPtr = CmdTcp;
         CmdPtr -> XPtr = &ConnBlock[LPtr -> YHandle];
         CmdPtr -> NewPrec = LPtr -> YMisc;
         CmdPtr -> CFlags =| 0400;     /* set flag */
         return(0);
       }

      case OUPDATE:
      {  /* response to move - update utcb parameters */
         UPtr = CmdTcp;
         ConnBlock[UPtr -> ZHandle].ConnIndex = UPtr -> ZConnIndex;
         ConnBlock[UPtr -> ZHandle].SndCount =+ UPtr -> ZSndCnt;
         ConnBlock[UPtr -> ZHandle].RcvCount = UPtr -> ZRcvCnt;

         return(ENOCHNG);
       }

      default:
      {  /* found something strange */
         return(ENOCHNG);
       }
    }
 }




TcpMove(ConnPtr, NewProcId, TimeOut)

struct UserTcp *ConnPtr;
int NewProcId;
int TimeOut;

{  int Error;
   struct PMask1 *MovePtr;

   if (ConnPtr -> ConnIndex == 0)
   {  /* connection doesn't exist */
      return(ENOCONN);
    }

   MovePtr = CmdUsr;
   MovePtr -> YOpCode = OMOVE;
   MovePtr -> YHandle = ConnPtr -> ConnIndex;
   MovePtr -> YMisc = NewProcId;

   write(CmdUsrFds, CmdUsr, 6);

   Error = ReadCmd(TimeOut);
   return(Error);
 }



TcpStop(Type, StopIdPtr, TimeOut)

int Type;
struct
{  long FSocket;
 } *StopIdPtr;
int TimeOut;

{  int Error;
   struct MClose *StopPtr;

   StopPtr = CmdUsr;
   StopPtr -> COpCode = OSTOP;
   StopPtr -> CHandle = Type;
   StopPtr -> CFinNum = StopIdPtr -> FSocket;

   write(CmdUsrFds, CmdUsr, 8);

   Error = ReadCmd(TimeOut);
   return(Error);
 }



TcpResume(Type, RsmIdPtr, TimeOut)

int Type;
struct
{  long FSocket;
 } *RsmIdPtr;
int TimeOut;

{  int Error;
   struct MClose *ResPtr;

   ResPtr = CmdUsr;
   ResPtr -> COpCode = ORESUME;
   ResPtr -> CHandle = Type;
   ResPtr -> CFinNum = RsmIdPtr -> FSocket;

   write(CmdUsrFds, CmdUsr, 8);

   Error = ReadCmd(TimeOut);
   return(Error);
 }







/* Miscellaneous subroutines */


ReadCmd(TimeOut)

/* wait for a command from TCP and then read it */

int TimeOut;

{  int ByteCnt[2];
   int Count;
   struct MUrgent *UrgPtr;
   struct PMask1 *ChngPtr;
   struct PMask1 *AckPtr;
   struct PMask1 *ErrPtr;
   struct PMask1 *PPtr;
   struct PMask1 *LPtr;
   struct PMask1 *ResetPtr;
   struct PMask1 *RejectPtr;

   while(1)             /* all exits by return */
   {  capac(CmdTcpFds, ByteCnt, 4);
      if (ByteCnt[0] == 0)
      {  sleep(1);             /* sleep for one second */
         if (TimeOut == 0)
         {  return(ETCPNRDY);
          }
         TimeOut--;
       }

      else
      {
         read(CmdTcpFds, &PortHdr, HDRLNGTH);
         Count = PortHdr.DCount&~0100000;     /* remove EOM bit */
         read(CmdTcpFds, CmdTcp, Count);

         ErrPtr = CmdTcp;

         switch (ErrPtr -> YOpCode)

         {  case OACK:
            {
               return(0);
             }

            case OERR:
            {  return(ErrPtr -> YMisc);
             }

            case OCHNG:
            {  /* found an injected change notice here */
               ChngPtr = CmdTcp;
               /* update new state */
               ConnBlock[ChngPtr -> YHandle].CNState = ChngPtr -> YMisc;
               /* set change state bit */
               ConnBlock[ChngPtr -> YHandle].CNFlags =| 01;
               break;
             }

            case OURGENT:
            {  /* found an injected urgent notice here */
               UrgPtr = CmdTcp;

               if ((UrgPtr -> UByteNo) > (ConnBlock[UrgPtr -> UHandle].RcvCount))
               {  /* go into urgent mode */
                  ConnBlock[UrgPtr -> UHandle].CNFlags =| 02;
                  ConnBlock[UrgPtr -> UHandle].UrgCount = UrgPtr -> UByteNo;
                }
               break;
             }

            case OSECERR:
            {  /* bad security level */
               LPtr = CmdTcp;
               ConnBlock[LPtr -> YHandle].CNFlags =| 04;  /* set flag */

               /* decrement the number of bytes sent */
               ConnBlock[LPtr -> YHandle].SndCount =- LPtr -> YMisc;

               break;
             }

            case OPROCNR:
            {  /* foreign process not responding */
               LPtr = CmdTcp;
               ConnBlock[LPtr -> YHandle].CNFlags =| 010;  /* set flag */
               break;
             }

            case OPROCR:
            {  /* foreign process responding again */
               PPtr = CmdTcp;
               ConnBlock[PPtr -> YHandle].CNFlags =| 020;  /* set flag */
               break;
             }

            case ONETMSG:
            {  /* network message arrived */
               LPtr = CmdTcp;
               ConnBlock[LPtr -> YHandle].CNMsg = LPtr -> YMisc;
               ConnBlock[LPtr -> YHandle].CNFlags =| 040;  /* set flag */
               break;
             }

            case ORESET:
            {  /* connection reset */
               ResetPtr = CmdTcp;
               /* ConnBlock[ResetPtr -> YHandle].CNFlags =| 0100; */

               /* delete the connection in case of preemption */
               ConnBlock[ResetPtr -> YHandle].ConnIndex = 0;
               break;
             }

            case OREJECT:
            {  /* foreign TCP rejecting */
               RejectPtr = CmdTcp;
               ConnBlock[RejectPtr -> YHandle].CNFlags =| 0200;    /* set flag */
               break;
             }

            case OPRECCHNG:
            {  /* send precedence level raised */
               LPtr = CmdTcp;
               /* update new state */
               ConnBlock[LPtr -> YHandle].CNPrec = LPtr -> YMisc;
               ConnBlock[LPtr -> YHandle].CNFlags =| 0400;   /* set flag */
               break;
             }

            default:
            {  return(EBADRESP);
             }
          }
       }
    }
 }
