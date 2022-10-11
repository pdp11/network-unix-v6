#
# include "tcpstru.h"
# include "hdrmasks.h"

# define ENDARGS        -1


PsipOut(WrkPtr)

struct WorkBlk *WrkPtr;

{
   extern char FlkBuff[TCPOTSZ];
   extern int FlkFlag;
   extern int Auth;
   extern int Debug;
   extern struct FdsBlk FdsBffer[FDSSIZE];
   extern char TcpOut[TCPOTSZ];
   extern int SegData;
   extern int PartWr;
   extern int NetFdsW;
   extern char *OutPtr;
   extern int OutCnt;
   extern ANLngth;

   register struct TcpTcb *XPtr;
   register char *DPtr;
   register char *SPtr;

   int DataSent;
   char *FPtr;
   int RemData;
   char *EndPtr;
   int i,j;
   int R1;
   int Beyond;
   struct FdsBlk *FdsBffPtr;
   int SecChng;
   int SegSec;
   int Count;

   struct
   {  int ADat;
    } *APtr;


   if (Debug == 1)
   {  printf("PsipOut: entered\n");
    }

   /* check if part of previous segment needs to be written */

   if (PartWr == 1)
   {  /* still have some of buffer left to export */
      Count = write(NetFdsW, OutPtr, OutCnt);
      if (Count == -1)
      {  printf("PsipOut: write error\n");
         return(1);     /* leave on work queue */
       }

      if (Count != OutCnt)
      {  OutPtr =+ Count;
         OutCnt =- Count;
         if (Debug == 4)
         {  Log(4, "s", "Part write", -1);
          }
         return(1);     /* leave on work queue */
       }

      PartWr = 0;
    }

   XPtr = WrkPtr -> TCBPtr;

   if (XPtr == &FlkFlag)
   {  /* send duplicate or out of order segment */
      /* copy segment to TcpOut buffer because
      of partial write problem with raw message interface */

      SPtr = &FlkBuff[BSLLNGTH - ANLngth*2 - 2];
      DPtr = &TcpOut[BSLLNGTH - ANLngth*2 - 2];

      APtr = SPtr;

      OutPtr = DPtr;
      OutCnt = APtr -> ADat;

      for (i = 0; i < OutCnt; i++)
      {  *DPtr++ = *SPtr++;
       }

      FlkFlag = 0;

      Count = write(NetFdsW, OutPtr, OutCnt);

      if (Count == -1)
      {  printf("PsipOut: write error\n");
         PartWr = 1;
         return(1);    /* leave on work queue */
       }

      if (Count != OutCnt)
      {  OutPtr =+ Count;
         OutCnt =- Count;
         PartWr = 1;
         return(1);    /* leave on work queue */
       }
      return(0);
    }

   if (((XPtr -> Flags)&040) > 0)
   {  /* RST received - send no more segments */
      return(0);    /* remove work block */
    }

   /* send out a segment */

   XPtr -> SeqNo = XPtr -> SendSeq;

   i = 0;   /* byte counter */

   Beyond = 0;    /* flag: 1 => sending beyond right edge */

   /* calculate remaining data to be sent */
   RemData = (XPtr -> DataTl) - (XPtr -> DataNxt);

   if (RemData < 0)
   {  RemData =+ RBUFFSIZE;
    }

   /* check if SYN needs to be sent */
   if (XPtr -> SeqNo == XPtr -> SndISN)
   {  /* first sequence number */
      XPtr -> TCPFlags =| SYN;    /* set SYN bit */
      XPtr -> SendSeq =+ 1;

      if (XPtr -> State == OPEN)
      {  XPtr -> State = SYNSENT;
       }
    }

   /* check for urgent state */
   if (((XPtr -> Flags)&01) > 0)
   {  /* urgent condition flag on */
      /* check if urgent number is beyond next to send */

      R1 = ULCompare(&(XPtr -> SndUrgNo), &(XPtr -> SeqNo));

      if (R1 != 2)
      /* SndUrgNo >= SendSeq */
      {  /* put on the urgent bit */
         XPtr -> UrgentPtr = (XPtr -> SndUrgNo) - (XPtr -> SeqNo);
         XPtr -> TCPFlags =| URG;    /* URG <- 1 */
       }
    }

   DataSent = (XPtr -> DataNxt) - (XPtr -> DataHd);

   if (DataSent < 0)
   {  DataSent =+ RBUFFSIZE;
    }

   if (RemData != 0)
   {
      DPtr = &TcpOut[BSLLNGTH + INLNGTH + TCPLNGTH];
      SPtr = &(XPtr -> RetranBuff[XPtr -> DataNxt]);
      FPtr = &(XPtr -> FlagBuff[XPtr -> DataNxt]);
      EndPtr = &(XPtr -> RetranBuff[RBUFFSIZE]);

      /* pick up the security level */
      SegSec = (*FPtr) >> 3;

      /* check if being held up by window limits */
      if ((XPtr -> SndWindow == 0)||
      ((((XPtr -> TCPFlags)&URG) > 0)&&(DataSent >= XPtr -> SndWindow)))

      {  /* check if this is the first segment into a zero window or
         that this segment is a retransmission */

         if ((((XPtr -> Flags)&XRTNDATA) == 0)||
         (((XPtr -> Flags)&(XRTNDATA+XRETRANOK)) == (XRTNDATA+XRETRANOK)))
         {  /* send at least one byte */
            *DPtr = *SPtr;
            i = 1;
            /* prohibit data sent until retransmission time */
            /* this could occur with an ACK being sent */

            XPtr -> Flags =| XRTNDATA;
          }

         /* ensure that no more data is sent until retransmission */
         Beyond = 1;
       }
      else
      {
         SecChng = 0;
         while ( ((i + DataSent) < (XPtr -> SndWindow))&&
         (i < RemData)&&( i < SegData)&&(SecChng == 0) )
         {  *DPtr++ = *SPtr++;
            i++;
            if (SPtr == EndPtr)
            {  SPtr = &(XPtr -> RetranBuff[0]);
               FPtr = &(XPtr -> FlagBuff[0]);
             }
            if (Auth == 1)
            {  if (((*FPtr) >> 3) == SegSec)
               {  FPtr++;
                }
               else
               {  SecChng = 1;
                }
             }
          }
       }
    }

   /* reset the retran flag */
   XPtr -> Flags =& ~XRETRANOK;

   /* check if time to send FIN */
   if ((((XPtr -> Flags)&010) > 0)&&
   (XPtr -> SndFSN == XPtr -> SendSeq))
   {  /* no more data will be sent */
      XPtr -> TCPFlags =| FIN;    /* FIN <- 1 */
    }

   if ((i == 0)&&(((XPtr -> TCPFlags)&0377) == 0))
   {  /* nothing to send */
      if (RemData > 0)
      {  /* still have data though */
         return(1);
       }
      return(0);
    }

   /* set EOL bit always for the tenex guys */

   XPtr -> TCPFlags =| EOL;

   XPtr -> SegLength = i + INLNGTH + TCPLNGTH;
   /* always set ACK bit if connection is not SYNSENT */
   switch(XPtr -> State)
   {  case SYNRECD:
      case ESTAB:
      case CLOSEWAIT:
      case FINWAIT:
      case CLOSING:
      {  /* set ACK bit */
         XPtr -> TCPFlags =| ACK;
       }
    }

   /* set the security level */
   if (i == 0)   /* check for data */
   {  XPtr -> Options[2] = XPtr -> ScMinOut;
    }
   else
   {  XPtr -> Options[2] = SegSec;
    }

   /* set precedence level */
   XPtr -> Options[2] =| (XPtr -> SndPrec) << 4;

   /* set the TCC */
   XPtr -> Options[3] = XPtr -> TccList[0];

   /* copy leaders to output buffer */

   DPtr = TcpOut;
   SPtr = &(XPtr -> PrecSec);

   for (j = 0; j < (BSLLNGTH + INLNGTH + TCPLNGTH); j++)
   {  *DPtr++ = *SPtr++;
    }

   /* write out the segment to the net */

   NetOut(0);

   /* update the state information */
   XPtr -> DataNxt =+ i;

   if ((XPtr -> DataNxt) >= RBUFFSIZE)
   {  XPtr -> DataNxt =- RBUFFSIZE;
    }

   XPtr -> SendSeq =+ i;

   if (Debug == 1)
   {  /* snap the segment info */
      printf("PsipOut: segment info:  L = %d, F = %o, Src = %d\n", i,
      (XPtr -> TCPFlags)&07777, XPtr -> SrcePort);
    }

   if ((((XPtr -> TCPFlags)&RST) > 0)&&(((XPtr -> Flags)&020) > 0))
   {  /* the RST was sent, now clean up the connection */
      return(3);    /* make the scheduler abort the connection */
    }

   /* check for state change */
   if (((XPtr -> TCPFlags)&FIN) > 0)
   {  XPtr -> SendSeq =+ 1;

      switch(XPtr -> State)
      {  case SYNRECD:
         case ESTAB:
         {  XPtr -> State = FINWAIT;
            break;
          }
         case CLOSEWAIT:
         {  XPtr -> State = CLOSING;
            break;
          }
       }
    }

   /* update MaxSend if not retransmitting */
   R1 = ULCompare(&(XPtr -> SendSeq),&(XPtr -> MaxSend));

   if (R1 == 1)
   /* SendSeq > MaxSend */
   {  XPtr -> MaxSend = XPtr -> SendSeq;
    }

   if ((i > 0)||(((XPtr -> TCPFlags)&(SYN+FIN)) > 0))
   {  /* put TCB on retransmission queue */
      QTcb(XPtr);
    }

   /* clear the control bits */
   XPtr -> TCPFlags =& ~07777;

   /* if window not closed,
   move work block to end of queue if
   there is more data to segmentize */
   if ((Beyond != 1)&&((RemData - i) > 0))
   {
      return(2);   /* move to end of queue */
    }
   if ((Beyond == 1)&&(PartWr == 1))
   {  printf("B = 1 and PWr = 1\n");
    }

   if (PartWr == 1)
   {  return(1);      /* leave on queue */
    }

   return(0);   /* delete work entry */
 }



NetOut(EchoSeg)

int EchoSeg;

{
   extern int ShortCkt;
   extern int ShortIn;
   extern int PartWr;
   extern char *OutPtr;
   extern int OutCnt;
   extern int *TimePtr;
   extern int RTime;
   extern int FlkFlag;
   extern char FlkBuff[TCPOTSZ];
   extern int SegNum;
   extern int ANLngth;
   extern int Debug;
   extern int InLink;
   extern int LocHost;
   extern int LocImp;
   extern int LocNet;
   extern int NetFdsW;
   extern int NetFdsR;
   extern char TcpOut[TCPOTSZ];
   extern char TcpIn[TCPINSZ];
   extern char NumLost;
   extern char NumBad;
   extern char NumDup;
   extern char TimeDup;
   extern char NumReOrd;
   extern char TimeReOrd;
   extern int NetTable[NUMNET];
   extern int DoChkSum;

   register char *SPtr, *DPtr;

   struct ANMask1 *ANPtr1;
   struct ANMask2 *ANPtr2;
   struct INLMask *INPtr;
   struct TCPMask *TCPPtr;

   int Error;
   int ILength;
   int DLength;
   int TLength;
   int Count;
   char *BPtr;
   int Chk;
   int Echo;
   int PHdr[2];
   int i;

   INPtr = &TcpOut[BSLLNGTH];
   ILength = ((INPtr -> IVerHdr)&017)*4;

   if (ShortCkt == 1)
   {  /* check if this segment is destined for this host */

      if ((INPtr -> IDstNet == LocNet)&&
      (INPtr -> IDHstH == LocHost)&&
      (((INPtr -> IDHstL)&0377) == LocImp))
      {  /* no real need to send to the net */

         /* set the segment ID */
         INPtr -> ISegID = SegNum;
         SegNum =+ 1;

         /* copy TcpOut buffer to TcpIn */

         SPtr = &TcpOut[BSLLNGTH];
         DPtr = &TcpIn[BSLLNGTH];
         Count = INPtr -> ISegLength;

         for (i = 0; i < Count; i++)
         {  *DPtr++ = *SPtr++;
          }

         ShortIn = 1;    /* set flag for PsipIn */

         /* make work for PsipIn */

         MakeWrk(0, NETINHP, 0, NetFdsR);

         return(0);
       }
    }

   ANPtr1 = &TcpOut[BSLLNGTH - ANLngth*2 - 2];
   ANPtr2 = ANPtr1;

   /* set the length regardless of AN leader size */
   ANPtr1 -> MLngth1 = ANLngth*2 + 2 + INPtr -> ISegLength;

   OutPtr = ANPtr1;
   OutCnt = ANPtr1 -> MLngth1;

   /* determine destination host */

   if (INPtr -> IDstNet >= NUMNET)
   {  i = NetTable[0];  /* send to smart gateway */
    }
   else
   {  i = NetTable[INPtr -> IDstNet];
    }

   if (i == -1)
   {  /* don't use gateway */
      i = ((INPtr -> IDHstL)&0377) + ((INPtr -> IDHstH) << 8);
    }

   if (ANLngth == 2)
   {  ANPtr1 -> Type1 = 0;
      ANPtr1 -> Link1 = InLink;    /* internet link number */
      ANPtr1 -> SbType1 = 0;
      ANPtr1 -> Host1 = (i&077) + ((i&01400) >> 2);
    }
   else
   {  ANPtr2 -> Fmt2 = 15;
      ANPtr2 -> SNet2 = 0;
      ANPtr2 -> LdrFlgs2 = 0;
      ANPtr2 -> Type2 = 0;
      ANPtr2 -> HandType2 = 0;
      ANPtr2 -> LogHost2 = 0;
      ANPtr2 -> Link2 = InLink;
      ANPtr2 -> SbType2 = 0;
      ANPtr2 -> Host2 = i >> 8;
      ANPtr2 -> Imp2 = i&0377;

      /* the next value may eventually have to be real */
      ANPtr2 -> ALngth2 = 0;
    }

   if (EchoSeg == 0)
   {  /* set the segment ID */
      INPtr -> ISegID = SegNum;
      SegNum =+ 1;

      TCPPtr = &TcpOut[BSLLNGTH + ILength];
      TLength = ((TCPPtr -> TTCPFlags) >> 12)*4;

      DLength = INPtr -> ISegLength - ILength - TLength;

      if (DoChkSum == 1)
      {  /* compute internet checksum */
         INPtr -> IINChkSum = ~ChkSum(INPtr, ILength/2, 0, 0);

         /* compute TCP checksum */
         /* point to internet source address */

         BPtr = &TcpOut[BSLLNGTH+12];

         Chk = ChkSum(BPtr, 4,0 ,0);

         BPtr = &PHdr[0];
         PHdr[0] = (INPtr -> ISegLength) - ILength;
         PHdr[1] = INPtr -> IProtocol;

         Chk = ChkSum(BPtr, 2, Chk, 0);

         BPtr = TCPPtr;

         Chk = ChkSum(BPtr, TLength/2, Chk, 0);

         BPtr = &TcpOut[BSLLNGTH + ILength + TLength];

         /* check for odd number of bytes */
         if ((DLength&01) > 0)
         {  /* need to pad with a zero byte */

            TcpOut[BSLLNGTH + ILength + TLength + DLength] = 0;
            DLength =+ 1;
          }

         TCPPtr -> TTCPChkSum = ~ChkSum(BPtr, DLength/2, Chk, 1);
       }
    }

   if ((Debug == 1)||(Debug == 2)||(Debug == 4))
   {
      Error = Log(1, "n", INPtr, ENDARGS);
      if (EchoSeg == 0)
      {  Error = Log(1, "t", TCPPtr, ENDARGS);
       }
    }

   /* swap bytes before sending */

   Swap(INPtr, ILength);

   if (EchoSeg == 0)
   {  Swap(TCPPtr, TLength);
    }

   /* flakiness simulation */

   /* do we need to lose the segment? */
   if (NumLost > 0)
   {  if (rand()%100 < NumLost)
      {  /* discard the segment */
         printf("Lost %d\n", SegNum-1);
         return(ANPtr1 -> MLngth1);  /* don't write to net */
       }
    }

   /* do we need to smash the segment? */
   if (NumBad > 0)
   {  if (rand()%100 < NumBad)
      {  i = BSLLNGTH + rand()%40;   /* 40 => in TCP/IN hdrs */
         TcpOut[i] = ~TcpOut[i];   /* some random place */
         printf("Smashed %d B: %d\n", SegNum-1, i);
       }
    }

   /* do we need to make a delayed duplicate? */
   if ((NumDup > 0)&&(FlkFlag == 0))
   {  if ((rand()%100) < NumDup)
      {  /* copy TcpOut to FlkBuff */

         SPtr = &TcpOut[BSLLNGTH - ANLngth*2 - 2];
         DPtr = &FlkBuff[BSLLNGTH - ANLngth*2 - 2];

         Count = ANPtr1 -> MLngth1;

         for (i = 0; i < Count; i++)
         {  *DPtr++ = *SPtr++;
          }

         RTime = (rand()%TimeDup) + *TimePtr;

         /* set flag to allow only one duplicate */
         FlkFlag = 1;

         printf("Duped %d T: %d\n", SegNum-1, RTime - *TimePtr);
       }
    }

   /* do we need to reorder the segment? */
   if ((NumReOrd > 0)&&(FlkFlag == 0))
   {  if ((rand()%100) < NumReOrd)
      {  /* copy TcpOut to FlkBuff */

         SPtr = &TcpOut[BSLLNGTH - ANLngth*2 - 2];
         DPtr = &FlkBuff[BSLLNGTH - ANLngth*2 - 2];

         Count = ANPtr1 -> MLngth1;

         for (i = 0; i < Count; i++)
         {  *DPtr++ = *SPtr++;
          }

         RTime = (rand()%TimeReOrd) + *TimePtr;

         /* set flag to allow only one duplicate */
         FlkFlag = 1;

         printf("Reordered %d T: %d\n", SegNum-1, RTime - *TimePtr);

         return(ANPtr1 -> MLngth1);  /* don't write to net */
       }
    }

   /* write the segment to the net */

   Count = write(NetFdsW, ANPtr1, ANPtr1 -> MLngth1);

   if (Count == -1)
   {  /* write error */
      printf("NetOut: write error\n");
      return(0);
    }

   if (Count != OutCnt)
   {  PartWr = 1;  /* need to finish sending this segment */
      OutPtr =+ Count;
      OutCnt =- Count;
      if (Debug == 4)
      {  Log(3, "s", "Part write", -1);
       }
    }

   return(0);
 }
