#
# include "tcpstru.h"
# include "hdrmasks.h"

# define MAXMSG      1012       /* input/output buffer length */
# define ENDARGS       -1       /* end of Log argument list */

/* ARPANET message types */
# define REGULAR        0
# define NOP            4
# define RFNM           5
# define NOHOST         7


PsipIn(WrkPtr)

struct WorkBlk *WrkPtr;

{
   extern int ShortIn;
   extern int ANLngth;
   extern int Auth;
   extern int Debug;
   extern char TcpIn[TCPINSZ];
   extern char TcpOut[TCPOTSZ];
   extern struct FdsBlk FdsBffer[FDSSIZE];
   extern int WindNum;
   extern int WindDenom;
   extern struct TcpTcb *TcbHd;
   extern int RWindow;
   extern char CmdTcp[CMDTCPSIZE];
   extern int NetFdsW;
   extern int PartWr;
   extern char *OutPtr;
   extern int OutCnt;
   extern long StopSkt[NSTOP];
   extern int StopTcc[NSTOP];

   register struct TcpTcb *XPtr;
   register struct INMask *INPtr;
   register struct TCPMask *TCPPtr;

   struct ANMask *ANPtr;
   struct PortLdr PortHdr;
   int i;
   int INLength;
   int Found;
   struct BSLMask *BSLPtr;
   int Error;
   int PktLength;
   int Cap[2];
   long temp1, temp2;
   long UrgNo;
   int R1, R2;
   char *OctetPtr;
   int BCnt;
   struct WorkBlk *APtr;
   struct PMask1 *ChngPtr;
   struct MUrgent *UrgPtr;
   struct PMask1 *PPtr;
   struct PMask1 *ResetPtr;
   struct PMask1 *RejectPtr;
   int l;
   int *MPtr;
   int DataAmt;
   int AckOk;
   long W;
   int V;
   struct FdsBlk *FdsBffPtr;
   int InPrec;
   int InSec;
   int InTcc;
   int SptOpt;
   int TccOk;
   int Stat;
   int Count;
   char *SPtr;
   char *DPtr;
   int FinOn;
   int SynOn;
   int FinFlag;

   struct
   {  char XHost;
      char XNet;
      int XImp;
    } *SktPtr;


   if (Debug == 1)
   {  printf("PsipIn: entered\n");
    }

   /*
   if (Debug == 4)
   {  Log(6, "s", "PsipIn", ENDARGS);
    }
   */

   /* The ShortIn flag indicates that a segment addressed
   locally is ready in the TcpIn buffer */

   if (ShortIn == 0)
   {  /* read segment from the net */
      Stat = NetIn();

      if (Stat == -1)
      {  return(0);     /* no more incoming segments */
       }

      if (Stat == 0)
      {  /* non-tcp message */
         return(2);        /* move block to end of work queue */
       }
    }

   INPtr = &TcpIn[BSLLNGTH];
   TCPPtr = &TcpIn[(((INPtr -> IVerHdr)&017)*4) + BSLLNGTH];

   if (ShortIn == 1)
   {  /* log it */
      if (Debug == 4)
      {  Log(6, "n", INPtr, ENDARGS);
         Log(6, "t", TCPPtr, ENDARGS);
       }

      ShortIn = 0;
    }

   SynOn = 0;
   FinOn = 0;

   if (((TCPPtr -> TTCPFlags)&SYN) > 0)
   {  SynOn = 1;
    }
   if (((TCPPtr -> TTCPFlags)&FIN) > 0)
   {  FinOn = 1;
    }

   DataAmt = (INPtr -> ISegLength) - (((INPtr -> IVerHdr)&017)*4) -
   ((((TCPPtr ->TTCPFlags)&(0170000)) >> 12)*4);

   PktLength = DataAmt + SynOn + FinOn;

   if (Debug == 1)
   {  /* snap the segment info */
      printf("PsipIn: seg info: L = %d, F = %o, Src = %d\n",
      DataAmt, (TCPPtr -> TTCPFlags)&0777, TCPPtr -> TSrcePort);
    }

   /* check for internet options */

   INLength = (INPtr -> IVerHdr)&017;

   SptOpt = 0;

   i = 0;

   while (i < ((INLength - 5)*4))
   {  switch(INPtr -> IOptions[i])
      {  case OPTSPT:
         {  InSec = (INPtr -> IOptions[i+2])&017;
            InPrec = ((INPtr -> IOptions[i+2]) >> 4)&017;
            InTcc = INPtr -> IOptions[i+3];
            SptOpt = 1;
            i = i + 4;
            break;
          }
         default:
         {  /* unknown option */
            if (INPtr -> IOption[i+1] == 0)
            {  /* will cause hang */
               i = (INLength - 5)*4;  /* get out of loop */
             }
            else
            {  i = i + INPtr -> IOptions[i+1];
             }
          }
       }
    }

   if (Auth == 1)
   {  /* check if incoming segment prohibited by Stop */

      for (i = 0;i < NSTOP; i++)
      {  SktPtr = &StopSkt[i];

         if ((SktPtr -> XHost == INPtr -> ISHstH)&&
         (SktPtr -> XNet == INPtr -> ISrcNet)&&
         (SktPtr -> XImp == INPtr -> ISHstL))
         {  /* ignore the segment */
            printf("PsipIn: Stop %o %o %o\n", INPtr -> ISrcNet,
            INPtr -> ISHstH, INPtr -> ISHstL);

            return(2);    /* move to end of queue */
          }
       }

      if (SptOpt == 0)
      {  return(2);    /* ignore segment */
       }

      /* check TCC */
      for (i = 0; i < NSTOP; i++)
      {  if (StopTcc[i] < 0)
         {  if ((StopTcc[i]&017) == InTcc)
            {  printf("PsipIn: Stop T %d\n", InTcc);
               /* ignore the segment */
               return(2);
             }
          }
       }
    }

   /* find the destination TCB */
   /* first check for non-OPENed connections */
   Found = 0;
   XPtr = TcbHd;

   while((Found == 0)&&(XPtr != 0))
   {  /* match local socket and foreign socket */

      if ((XPtr -> State != OPEN)&&(XPtr -> TMode != 2))
      {  /* check for exact match */
         if ((XPtr -> SrcePort == TCPPtr -> TDestPort)&&
         (XPtr -> DestPort == TCPPtr -> TSrcePort)&&
         (XPtr -> DstNet == INPtr -> ISrcNet)&&
         (XPtr -> DstHstH == INPtr -> ISHstH)&&
         (XPtr -> DstHstL == INPtr -> ISHstL))

         {  /* found the TCB */
            Found = 1;
          }
       }
      if (Found == 0)
      {  XPtr = XPtr -> NxtActTcb;
       }
    }

   if (Found == 0)
   {  /* check TCBs with unspecified local socket parameters */
      XPtr = TcbHd;

      while ((Found == 0)&&(XPtr != 0))
      {  /* match local socket and foreign socket taking into
         account possible unspecified foreign socket parameters */

         if ((XPtr -> State == OPEN)&&(XPtr -> TMode != 2))
         {
            if ((XPtr -> SrcePort == TCPPtr -> TDestPort)&&
            ((XPtr -> DestPort == TCPPtr -> TSrcePort)||(XPtr -> DestPort == 0))&&
            ((XPtr -> DstNet == INPtr -> ISrcNet)||(XPtr -> DstNet == 0))&&
            (((XPtr -> DstHstH == INPtr -> ISHstH)&&
            (XPtr -> DstHstL == INPtr -> ISHstL))||
            ((XPtr -> DstHstH == 0)&&(XPtr -> DstHstL == 0))))

            {  /* found the TCB */
               Found = 1;
             }
          }
         if (Found == 0)
         {  XPtr = XPtr -> NxtActTcb;
          }
       }
    }

   /* check SPT parameters */

   if ((Found == 1)&&(Auth == 1))
   {  /* check for internet SPT option */
      if (SptOpt == 0)
      {  /* no option in segment */
         Found = 0;
       }
      else
      {  /* ignore security if unspecified */
         if (XPtr -> ScMaxIn != -1)
         {  /* validate the security */
            if ((InSec > XPtr -> ScMaxIn)||
            (InSec < XPtr -> ScMinIn))
            {  /* bad security level */
               Found = 0;
             }
          }

         /* validate the receive precedence */
         if ((InPrec > XPtr -> MxRcvPrec)||
         (InPrec < XPtr -> MnRcvPrec))
         {  /* bad precedence level */
            Found = 0;
          }

         /* validate the TCC */
         TccOk = 0;
         i = 0;

         while ((i < XPtr -> TCCCnt)&&(TccOk == 0))
         {  if (XPtr -> TccList[i] == InTcc)
            {  TccOk = 1;
             }
            else
            {  i++;
             }
          }

         if (TccOk == 0)
         {  Found = 0;
          }

       }
    }

   if ((Found == 0)||(XPtr -> State == CLOSED))
   {  /* no matching TCB */
      printf("nm %o %o %o %o\n", TCPPtr -> TSrcePort, INPtr -> ISHstH,
      INPtr -> ISHstL, INPtr -> ISrcNet);

      if (PartWr == 0)      /* forget RST if waiting for netout */
      {  /* if no RST, send ACK, RST */
         if (((TCPPtr -> TTCPFlags)&RST) == 0)
         {  /* immediately send a RST segment */

            /* swap internet fields */
            INPtr -> ISegLength = (((INPtr -> IVerHdr)&017)*4) + TCPLNGTH;
            INPtr -> IINChkSum = 0;
            V = INPtr -> ISrcNet;
            INPtr -> ISrcNet = INPtr -> IDstNet;
            INPtr -> IDstNet = V;
            V = INPtr -> ISHstL;
            INPtr -> ISHstL = INPtr -> IDHstL;
            INPtr -> IDHstL = V;
            V = INPtr -> ISHstH;
            INPtr -> ISHstH = INPtr -> IDHstH;
            INPtr -> IDHstH = V;

            /* swap TCP fields */
            V = TCPPtr -> TSrcePort;
            TCPPtr -> TSrcePort = TCPPtr -> TDestPort;
            TCPPtr -> TDestPort = V;

            W = TCPPtr -> TAckNo;  
            TCPPtr -> TAckNo = TCPPtr -> TSeqNo + PktLength;
            TCPPtr -> TSeqNo = W;

            TCPPtr -> TTCPChkSum = 0;

            TCPPtr -> TTCPFlags = ACK + RST
            + ((TCPPtr -> TTCPFlags)&0170000);

            /* copy segment to TcpOut */

            SPtr = TcpIn;
            DPtr = TcpOut;

            for (i = 0; i < BSLLNGTH + (INPtr -> ISegLength); i++)
            {  *DPtr++ = *SPtr++;
             }

            /* send segment to the net */

            NetOut(0);

          }
       }
      return(2);    /* move work entry */
    }

   /* reset "foreign host not resp" bit */
   XPtr -> Flags =& ~0400;

   XPtr -> SegRecv =+ 1;

   AckOk = 0;

   /* check for ACK */

   if (((TCPPtr -> TTCPFlags)&ACK) > 0)
   {  /* found an ACK bit on */
      /* check for range of Ack */

      R1 = ULCompare(&(XPtr -> LeftSeq), &(TCPPtr -> TAckNo));
      R2 = ULCompare(&(TCPPtr -> TAckNo), &(XPtr -> MaxSend));

      if ((R1 == 2)&&(R2 != 1))
      /* LeftSeq < AckNo <= MaxSend */
      {  /* acceptable */

         AckOk = 1;

         if (R2 == 0)
         /* AckNo = MaxSend */
         {  UnQTcb(XPtr);

            if (XPtr -> State == CLOSING)
            {  /* nothing else to send */
               XPtr -> State = CLOSED;

               /* inform user */

               CmdOut(XPtr, OCHNG, CLOSED, 0);
             }
          }

         XPtr -> DataHd =+ (TCPPtr -> TAckNo) - (XPtr -> LeftSeq);

         /* compensate for SYN, FIN sequence numbers */

         switch(XPtr -> State)
         {  case FINWAIT:
            case CLOSING:
            case CLOSED:
            {  R1 = ULCompare(&(TCPPtr -> TAckNo), &(XPtr -> SndFSN));
               if (R1 == 1)       /* AckNo > SndFSN */
               {  XPtr -> DataHd =- 1;
                }
               break;
             }
            case SYNSENT:
            case SYNRECD:
            {  if (XPtr -> LeftSeq == XPtr -> SndISN)
               {  XPtr -> DataHd =- 1;
                }
               break;
             }
          }

         if (XPtr -> DataHd >= RBUFFSIZE)
         {  XPtr -> DataHd =- RBUFFSIZE;
          }

         XPtr -> LeftSeq = TCPPtr -> TAckNo;

         R1 = ULCompare(&(TCPPtr -> TAckNo), &(XPtr -> SendSeq));

         if (R1 == 1)
         /* AckNo > SendSeq */
         {  XPtr -> SendSeq = TCPPtr -> TAckNo;

            XPtr -> DataNxt = XPtr -> DataHd;
          }

         /* check if send urgent can be turned off */
         R1 = ULCompare(&(XPtr -> LeftSeq), &(XPtr -> SndUrgNo));

         if (R1 != 2)   /* LeftSeq >= SndUrgNo */
         {  /* turn off send urgent flag */
            XPtr -> Flags =& ~01;
          }

         /* schedule work for DataIn in case there is
         more data in the port */

         MakeWrk(0, USERINHP, XPtr, XPtr -> SendPort);

         if (((TCPPtr -> TTCPFlags)&RST) == 0)
         {  /* schedule work for PsipOut in case segmentation
            is being held up because of window or buffer limits */

            MakeWrk(0, NETOUTHP, XPtr, NetFdsW);

            if (XPtr -> State == SYNRECD)
            {  /* go into established state */

               XPtr -> State = ESTAB;

               /* inform user */

               CmdOut(XPtr, OCHNG, ESTAB, 0);

               /* bind receive precedence */
               XPtr -> MxRcvPrec = InPrec;
               XPtr -> MnRcvPrec = InPrec;

             }

            XPtr -> NumRetran = 0;

            if (((XPtr -> Flags)&04) > 0)
            {  /* inform user foreign proc is back */
               PPtr = CmdTcp;
               PPtr -> YOpCode = OPROCR;
               PPtr -> YHandle = XPtr -> ConnHandle;
               write(XPtr -> TCPCmdPort, CmdTcp, 4);

               XPtr -> Flags =& ~04;    /* reset flag */
             }
          }
       }

      else
      {  /* ACK not acceptable */

         /* decide whether to return a RST */

         switch(XPtr -> State)
         {  case OPEN:
            case SYNSENT:
            case SYNRECD:
            case CLOSED:
            {  /* check for RST */

               if (((TCPPtr -> TTCPFlags)&RST) == 0)
               {  /* return a RST */

                  XPtr -> SeqNo = TCPPtr -> TAckNo;
                  XPtr -> AckNo = TCPPtr -> TSeqNo + PktLength;
                  XPtr -> TCPFlags =| ACK + RST;

                  MakeWrk(0, NETOUTHP, XPtr, NetFdsW);
                }
             }
          }
       }
    }


   /* check for RST */
   /* ACK ignored since I could lose a "courtesy RST" this way */

   if (((TCPPtr -> TTCPFlags)&RST) > 0)
   {  /* RST bit is on */

      switch (XPtr -> State)
      {  case OPEN:
         case CLOSED:
         {  break;    /* ignore the RST */
          }
         case SYNSENT:
         {  /* foreign TCP rejecting */
            /* send only one message to user */

            if (((XPtr -> Flags)&0100) == 0)
            {  /* inform user */
               RejectPtr = CmdTcp;
               RejectPtr -> YOpCode = OREJECT;
               RejectPtr -> YHandle = XPtr -> ConnHandle;
               write(XPtr -> TCPCmdPort, CmdTcp, 4);

               XPtr -> Flags =| 0100;    /* set flag */

             }
            XPtr -> LeftSeq = XPtr -> SndISN;
            XPtr -> SendSeq = XPtr -> SndISN;
            XPtr -> MaxSend = XPtr -> SndISN;

            /* put TCB on retransmission queue */
            QTcb(XPtr);
            break;
          }
         case SYNRECD:
         {  XPtr -> State = OPEN;

            /* restore unspecified foreign socket parameters */

            XPtr -> DstNet = XPtr -> OpnNet;
            XPtr -> DstHstH = XPtr -> OpnHstH;
            XPtr -> DstHstL = XPtr -> OpnHstL;
            XPtr -> DestPort = XPtr -> OpnPort;

            XPtr -> LeftSeq = XPtr -> SndISN;
            XPtr -> SendSeq = XPtr -> SndISN;
            XPtr -> MaxSend = XPtr -> SndISN;

            XPtr -> DataHd = 0;
            XPtr -> DataTl = 0;
            XPtr -> DataNxt = 0;

            /* restore precedence, TCC parameters */

            break;
          }
         default:
         {  /* all other connection states */
            /* close the connection and inform user */

            XPtr -> State = CLOSED;

            XPtr -> Flags =| 040;    /* send no more segments */

            UnQTcb(XPtr);

            /* send "connection reset" message to user */

            ResetPtr = CmdTcp;
            ResetPtr -> YOpCode = ORESET;
            ResetPtr -> YHandle = XPtr -> ConnHandle;
            write(XPtr -> TCPCmdPort, CmdTcp, 4);

            /* send state change message */

            CmdOut(XPtr, OCHNG, CLOSED, 0);

            break;
          }
       }
      return(2);    /* move work block */
    }

   /* update the send window size */
   XPtr -> SndWindow = TCPPtr -> TWindow;

   if (XPtr -> SndWindow > 0)
   {  /* reset flag which ensures that ACKs into
      a zero window do not send data unless retransmitting */

      XPtr -> Flags =& ~XRTNDATA;
    }

   switch (XPtr -> State)
   {  case OPEN:
      {
         if (SynOn > 0)
         {  /* segment contains SYN bit */
            /* fill in unspecified foreign socket parameters */
            XPtr -> DestPort = TCPPtr -> TSrcePort;
            XPtr -> DstNet = INPtr -> ISrcNet;
            XPtr -> DstHstH = INPtr -> ISHstH;
            XPtr -> DstHstL = INPtr -> ISHstL;

            if (Auth == 1)
            {  /* check if send precedence level is high enough */
               if (InPrec > XPtr -> SndPrec)
               {  /* check if allowable to boost send precedence */
                  if (InPrec <= XPtr -> ATPrec)
                  {  /* boost to match incoming */
                     XPtr -> SndPrec = InPrec;

                     /* inform user of send precedence change */

                     PPtr = CmdTcp;
                     PPtr -> YOpCode = OPRECCHNG;
                     PPtr -> YHandle = XPtr -> ConnHandle;
                     PPtr -> YMisc = XPtr -> SndPrec;

                     write(XPtr -> TCPCmdPort, CmdTcp, 6);
                   }
                  else
                  {  /* check if can boost to highest possible */
                     if (XPtr -> SndPrec < XPtr -> ATPrec)
                     {  /* boost up to auth table value */
                        XPtr -> SndPrec = XPtr -> ATPrec;

                        /* inform user of send precedence change */

                        PPtr = CmdTcp;
                        PPtr -> YOpCode = OPRECCHNG;
                        PPtr -> YHandle = XPtr -> ConnHandle;
                        PPtr -> YMisc = XPtr -> SndPrec;

                        write(XPtr -> TCPCmdPort, CmdTcp, 6);
                      }
                   }
                }
               /* bind the security for unspecified connection */
               if (XPtr -> ScMaxOut == -1)
               {  XPtr -> ScMaxOut = InSec;
                  XPtr -> ScMinOut = InSec;
                }

               /* bind the TCC */
               XPtr -> TCCCnt = 1;
               XPtr -> TccList[0] = InTcc;
             }

            /* begin 3-way handshake */
            /* assumes no data with SYN */

            XPtr -> TCPFlags =| ACK;  /* set ACK bit */
            XPtr -> AckNo = (TCPPtr -> TSeqNo) +1;
            XPtr -> RcvISN = TCPPtr -> TSeqNo;

            /* create some work for PsipOut */
            MakeWrk(0, NETOUTHP, XPtr, NetFdsW);

            /* go into SYN-RECD state */
            XPtr -> State = SYNRECD;

            /* notify user in case he is a server
            and needs to fork */

            CmdOut(XPtr, OCHNG, SYNRECD, 0);
          }
         return(2);    /* move work entry */
       }
      case SYNRECD:
      case CLOSED:      /* since ACK can cause CLOSED state */
      {
         return(2);    /* move work entry */
       }
      case SYNSENT:
      {
         if (SynOn > 0)
         {  /* found SYN = 1 */
            XPtr -> AckNo = (TCPPtr -> TSeqNo) + 1;
            XPtr -> TCPFlags =| ACK;   /* set ACK = 1 */

            if (Auth == 1)
            {  /* check if send precedence level is high enough */
               if (InPrec > XPtr -> SndPrec)
               {  /* check if allowable to boost send precedence */
                  if (InPrec <= XPtr -> ATPrec)
                  {  /* boost to match incoming */
                     XPtr -> SndPrec = InPrec;

                     /* inform user of send precedence change */

                     PPtr = CmdTcp;
                     PPtr -> YOpCode = OPRECCHNG;
                     PPtr -> YHandle = XPtr -> ConnHandle;
                     PPtr -> YMisc = XPtr -> SndPrec;

                     write(XPtr -> TCPCmdPort, CmdTcp, 6);
                   }
                  else
                  {  /* check if can boost to highest possible */
                     if (XPtr -> SndPrec < XPtr -> ATPrec)
                     {  /* boost up to auth table value */
                        XPtr -> SndPrec = XPtr -> ATPrec;

                        /* inform user of send precedence change */

                        PPtr = CmdTcp;
                        PPtr -> YOpCode = OPRECCHNG;
                        PPtr -> YHandle = XPtr -> ConnHandle;
                        PPtr -> YMisc = XPtr -> SndPrec;

                        write(XPtr -> TCPCmdPort, CmdTcp, 6);
                      }
                   }
                }
             }

            R1 = ULCompare(&(XPtr -> LeftSeq),&(XPtr -> SndISN));

            /* check for ACK */
            if ((AckOk == 1)||(R1 == 1))
            /* R1 = 1 => LeftSeq > SndISN */
            /* implies SYN already acked */
            {  XPtr -> State = ESTAB;

               /* send change notice to user */

               CmdOut(XPtr, OCHNG, ESTAB, 0);

               /* bind receive precedence */
               XPtr -> MxRcvPrec = InPrec;
               XPtr -> MnRcvPrec = InPrec;

             }
            else
            {  XPtr -> State = SYNRECD;
             }

            /* send ACK segment */
            /* make some work for PsipOut */

            MakeWrk(0, NETOUTHP, XPtr, NetFdsW);

          }
         return(2);    /* move work entry */
       }



      case ESTAB:
      case FINWAIT:
      case CLOSEWAIT:
      case CLOSING:

      {
         /* check for urgent flag */
         if ((XPtr -> State == ESTAB)||(XPtr -> State == FINWAIT))
         {  if (((TCPPtr -> TTCPFlags)&URG) > 0)
            {  /* URG = 1 */
               /* calculate actual sequence number */
               /* the following craziness reqd because
               of sign extension */

               z = &UrgNo;
               z -> bb[0] = 0;
               z -> bb[1] = TCPPtr -> TUrgentPtr;

               UrgNo =+ TCPPtr -> TSeqNo;

               /* check to see if user should be notified */
               /* fix this: segments may arrive out of order */

               if ((((XPtr -> Flags)&02) == 0)||(XPtr -> RcvUrgNo != UrgNo))
               {  /* send urgent message to user */
                  XPtr -> RcvUrgNo = UrgNo;
                  XPtr -> Flags =| 02;

                  UrgPtr = CmdTcp;
                  UrgPtr -> UOpCode = OURGENT;
                  UrgPtr -> UHandle = XPtr -> ConnHandle;
                  UrgPtr -> UByteNo = UrgNo - (XPtr -> RcvISN) - 1 /* for SYN */;
                  write(XPtr -> TCPCmdPort, CmdTcp, 8);
                }
             }
          }

         if (PktLength > 0)
         {  /* data arrived (or data plus SYN or FIN) */

            /* get capacity of user receive port */

            capac(XPtr -> RecvPort, Cap, 4);

            /* calculate new receive window */
            XPtr -> Window = ((Cap[1]*WindNum)/WindDenom) - HDRLNGTH;

            if (XPtr -> Window > RWindow)
            {  /* reset to max size allowed */
               XPtr -> Window = RWindow;
             }

            if (XPtr -> Window < 0)
            {  XPtr -> Window = 0;
             }

            /* check if next in sequence */
            /* check PKT-SEQUENCE */
            temp1 = (TCPPtr -> TSeqNo) + (PktLength-1);
            temp2 = (XPtr -> AckNo) + (XPtr -> Window);

            R1 = ULCompare(&(XPtr -> AckNo), &temp1);
            R2 = ULCompare(&temp1, &temp2);

            if ((R1 != 1)&&(R2 == 2))
            /* AckNo <= SeqNo + PktLength-1 < AckNo + Window */

            {  /* acceptable segment */
               R1 = ULCompare(&(TCPPtr -> TSeqNo), &(XPtr -> AckNo));

               if (R1 == 1)    /* SeqNo > AckNo */

               {  /* not in order */
                  /* for now discard segment! */
                  XPtr -> BusyRecv =+ 1;
                  printf("out of order\n");
                }
               else
               {
                  /* make ptr to first data octet */

                  OctetPtr = &TcpIn[(((INPtr -> IVerHdr)&017)*4) + BSLLNGTH +
                  ((((TCPPtr -> TTCPFlags)&0170000) >> 12)*4)];

                  /* determine number of next in sequence data bytes */

		  /* the SynOn compensation is there to fix a bug
		  which caused a loss of the first byte when a
		  retransmission of SYN + Data was received in the
		  ESTAB state.  */

		  if (SynOn == 1)
		  { printf("PsipIn: ESTAB and SYN + %d bytes\n", DataAmt);
		   }

		  BCnt = DataAmt + SynOn
		  - ((XPtr -> AckNo) - (TCPPtr -> TSeqNo));

                  if (Cap[1] < HDRLNGTH + BCnt)
                  {  /* no space in receive port */
                     /* for now, discard segment */
                     printf("no recv space\n");
                   }
                  else
                  {  /* move ptr to next octet in sequence */
                     OctetPtr =+ DataAmt - BCnt;

                     if (BCnt != 0)
                     {  write(XPtr -> RecvPort, OctetPtr, BCnt);

                        /* update window */
                        XPtr -> Window =- BCnt + HDRLNGTH;
                        if (XPtr -> Window < 0)
                        {  XPtr -> Window = 0;
                         }
                      }

                     XPtr -> AckNo = (TCPPtr -> TSeqNo) + PktLength;

                     /* data sent to user, check for FIN */

                     if (FinOn > 0)
                     {  /* FIN bit is on */

                        /* close port to indicate EOF */
                        /* close(XPtr -> RecvPort); */

                        if (XPtr -> State == ESTAB)
                        {  XPtr -> State = CLOSEWAIT;

                           /* notify user of receipt of FIN */

                           CmdOut(XPtr, OCHNG, CLOSEWAIT, 0);
                         }

                        else if (XPtr -> State == FINWAIT)
                        {  /* check if all data and controls
                           have been acked */

                           if (XPtr -> LeftSeq == XPtr -> MaxSend)
                           {  /* everything acknowledged */
                              XPtr -> State = CLOSED;

                              /* send user connection state change */

                              CmdOut(XPtr, OCHNG, CLOSED, 0);
                            }
                           else
                           {  XPtr -> State = CLOSING;
                            }
                         }
                      }
                   }
                }
             }
            /* send back an ACK */
            XPtr -> TCPFlags =| ACK;    /* set the ACK bit */

            /* make some work for PsipOut */

            MakeWrk(0, NETOUTHP, XPtr, NetFdsW);

          }
         return(2);    /* move work entry */
       }

      default:
      {
         printf("PsipIn: bad TCB state: %d\n", XPtr -> State);
         return(0);
       }

    }
 }


/* Subroutine for handling traffic from the net */

NetIn()

{
   extern int ANLngth;
   extern Debug;
   extern int NetFdsR;
   extern char TcpOut[TCPOTSZ];
   extern char TcpIn[TCPINSZ];
   extern int DoChkSum;
   extern struct TcpTcb *TcbHd;
   extern int LocNet;
   extern int LocHost;
   extern int LocImp;
   extern int PartWr;

   register char *SPtr, *DPtr;

   int BCnt;
   struct ANMask1 *ANPtr1;
   struct ANMask2 *ANPtr2;
   struct BSLMask *BSLPtr;
   struct INLMask *INPtr;
   struct TCPMask *TCPPtr;
   struct TcpTcb *XPtr;
   int ILength;
   int DLength;
   int TLength;
   char *BPtr;
   int Chk;
   int Error;
   int PHdr[2];
   int BHost;
   int BImp;
   int MType;
   int i;
   int SType;

   BCnt = read(NetFdsR, &TcpIn[BSLLNGTH - ANLngth*2 - 2], MAXMSG);

   if (BCnt == -1)
   {  /* read error */
      printf("read error\n");
      return(0);
    }

   if (BCnt == 0)
   {  return(-1);
    }
   else
   {  /* check for message type */
      ANPtr1 = &TcpIn[BSLLNGTH - ANLngth*2 - 2];
      ANPtr2 = ANPtr1;

      if (ANLngth == 2)
      {  MType = ANPtr1 -> Type1;
         SType = ANPtr1 -> SbType1;
       }
      else
      {  MType = ANPtr2 -> Type2;
         SType = ANPtr2 -> SbType2;
       }

      switch(MType)
      {  case RFNM:
         {  /* RFNM arrived */
            return(0);
          }

         case REGULAR:
         {  /* a regular message arrived */
            BSLPtr = TcpIn;

            INPtr = &TcpIn[BSLLNGTH];

            /* remember, the bytes are arriving swapped */
            ILength = ((INPtr -> ITypeService)&017)*4;

            if (ILength < 20)
            {  printf("PsipIn: IN ldr too short\n");
               return(0);
             }

            /* swap the internet bytes */
            Swap(INPtr, ILength);

            /* check if this is version 4 internet leader */

            if ((((INPtr -> IVerHdr) >> 4)&017)  != INVERNO)
            {  /* printf("PsipIn: bad IN version\n"); */
               return(0);
             }

            if (DoChkSum == 1)
            {  /* check the internet checksum */

               if (ChkSum(INPtr, ILength/2, 0, 0) != -1)
               {  /* bad internet checksum -
                  drop incoming segment on the floor */

                  printf("bad IN checksum\n");
                  return(0);
                }
             }

            /* check if this host is the destination */

            if ((INPtr -> IDstNet != LocNet)||
            (INPtr -> IDHstH != LocHost)||
	    (INPtr -> IDHstL != LocImp))
            {  /* segment is destined for some other host */

               if (PartWr == 1)
               {  /* discard it */
                  return(0);
                }

               /* copy to TcpOut */

               DPtr = &TcpOut[BSLLNGTH];
               SPtr = &TcpIn[BSLLNGTH];
               BCnt = INPtr -> ISegLength;

               for (i = 0; i < BCnt; i++)
               {  *DPtr++ = *SPtr++;
                }

               NetOut(1);  /* send back out to net */

               return(0);
             }

            /* check for TCP protocol */

            if (INPtr -> IProtocol != TCPVERNO)
            {  printf("Psipin: bad TCP version\n");
               return(0);
             }

            /* get the length of the tcp header */
            /* the 12th byte is the TCP offset byte */

            TCPPtr = &TcpIn[BSLLNGTH + ILength];

            TLength = ((TcpIn[BSLLNGTH + ILength + 12]) >> 4)*4;

            if (TLength < TCPLNGTH)
            {  printf("PsipIn: TCP ldr too short\n");
               return(0);
             }

            DLength = INPtr -> ISegLength - ILength - TLength;

            /* swap the TCP header bytes */

            Swap(TCPPtr, TLength);

            if ((Debug == 1)||(Debug == 3)||(Debug == 4))
            {
               Error = Log(2, "n", INPtr, ENDARGS);
               Error = Log(2, "t", TCPPtr, ENDARGS);
             }

            if (DoChkSum == 1)
            {  /* check the TCP checksum */

               BPtr = &TcpIn[BSLLNGTH + 12];

               Chk = ChkSum(BPtr, 4, 0, 0);
               Chk = ChkSum(TCPPtr, TLength/2, Chk, 0);

               /* don't forget the protocol and length additions */
               BPtr = &PHdr[0];
               PHdr[0] = (INPtr -> ISegLength) - ILength;
               PHdr[1] = INPtr -> IProtocol;

               Chk = ChkSum(BPtr, 2, Chk, 0);

               BPtr = &TcpIn[BSLLNGTH + ILength + TLength];

               /* check for odd byte count */
               if ((DLength&01) > 0)
               {
                  /* need to pad with zero byte */
                  TcpIn[BSLLNGTH + ILength + TLength + DLength] = 0;
                  DLength =+ 1;
                }
               Chk = ChkSum(BPtr, DLength/2, Chk, 1 /* swap */);

               if (Chk != -1)
               {  /* TCP checksum error -
                  drop segment on the floor */
                  printf("bad TCP checksum\n");
                  return(0);
                }
             }
            return(1);
          }

         case NOP:
         {  /* NOP message arrived */
            return(0);
          }

         case NOHOST:
         {  /* host not responding */

            if (ANLngth == 2)
            {  BHost = (ANPtr1 -> Host1) >> 6;
               BImp = (ANPtr1 -> Host1)&077;
             }
            else
            {  BHost = ANPtr2 -> Host2;
               BImp = ANPtr2 -> Imp2;
             }

            /* inform all users talking to this host */
            /* search TCBs for this foreign host */

            XPtr = TcbHd;

            while(XPtr != 0)
            {  if ((XPtr -> DstHstH == BHost)&&
               (XPtr -> DstHstL == BImp)&&
               (XPtr -> DstNet == LocNet))
               {  /* found one */
                  /* inform user only once */
                  if (((XPtr -> Flags)&0400) == 0)
                  {  CmdOut(XPtr, ONETMSG, 1 /* host not resp */, 0);
                     XPtr -> Flags =| 0400;  /* set flag */
                   }
                }
               XPtr = XPtr -> NxtActTcb;
             }

            return(0);
          }

         default:
         {  printf("Type %d message, Subtype %d\n", MType, SType);
          }

       }
    }
 }



/* Subroutine for swapping bytes to get them out in
the right order */

Swap(P, BCnt)

char *P;
int BCnt;

{
   struct aa
   {  char Byte[MAXMSG];
    };

   register struct aa *Ptr;
   register int i;
   register char T;

   Ptr = P;

   for (i = 0; i < BCnt;)
   {
      T = Ptr -> Byte[i];
      Ptr -> Byte[i] = Ptr -> Byte[i+1];
      Ptr -> Byte[i+1] = T;
      i = i + 2;
    }
 }



/* Subroutine which calculates the checksum */

ChkSum(Ptr, WCnt, OldChk, SwapFlag)

int *Ptr;
int WCnt;
int OldChk;
int SwapFlag;

{
   int i;
   int CheckSum;
   int C;
   char Temp;
   int NxtWd;

   struct
   {  char Byte[2];
    } *BPtr;


   BPtr = &NxtWd;

   CheckSum = OldChk;

   for (i = 0; i < WCnt; i++)
   {  NxtWd = *Ptr;

      if (SwapFlag == 1)
      {  /* the bytes need to be swapped first */
         Temp = BPtr -> Byte[0];
         BPtr -> Byte[0] = BPtr -> Byte[1];
         BPtr -> Byte[1] = Temp;
       }

      /* determine the carry bit */
      C = 0;

      if ((CheckSum < 0)&&(NxtWd < 0))
      {  C = 1;
       }
      else if (((CheckSum^(NxtWd)) < 0)&&((CheckSum + NxtWd) >= 0))
      {  C = 1;
       }

      CheckSum =+ (NxtWd) + C;
      Ptr++;
    }
   return(CheckSum);
 }
