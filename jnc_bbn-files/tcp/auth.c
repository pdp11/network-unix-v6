#
# include "tcpstru.h"

Authorize(Ident, InfoPtr, XPtr)

char Ident;
struct MOpen *InfoPtr;
struct TcpTcb *XPtr;

{

   int i,j,k;
   int ParamVal[20];
   int Found;

   /* check for proper Tcc parameter */
   if ((InfoPtr -> OpenMode == 0)&&(InfoPtr -> NumbTcc > 1))
   {  printf("X9\n");
      return(EBADPARAM);
    }

   /* number of entries returned in k */

   k = GetFields(Ident, &ParamVal);

   if (k == -1)
   {  /* some error present */
      return(EUNAUTH);
    }

   /* must have found it */

   /* check that authtable entries are okay */

   if ((ParamVal[0] > 1)||(ParamVal[0] < 0))
   {  printf("Auth: bad priv %d\n", Ident);
      return(EBADPARAM);
    }

   for (i = 0; i < 6; i++)
   {  if ((ParamVal[i] > 15)||(ParamVal[i] < 0))
      {  printf("Auth: bad S/P value %d\n", Ident);
         return(EBADPARAM);
       }
    }

   if ((ParamVal[2] > ParamVal[1])||(ParamVal[4] > ParamVal[3]))
   {  printf("Auth: sec fields swapped %d\n", Ident);
      return(EBADPARAM);
    }

   for (i = 6; i < k; i++)
   {  if ((ParamVal[i] < 0)||(ParamVal[i] > 255))
      {  printf("Auth: bad tcc %d\n", Ident);
         return(EBADPARAM);
       }
    }

   /* check parameters with authtable entries */

   if (InfoPtr -> AbsSecur < 0)
   {  /* absolute security level specified */
      InfoPtr -> AbsSecur =& 0177;

      if (InfoPtr -> AbsSecur > 15)
      {  /* too big */
         return(EBADPARAM);
       }

      if ((InfoPtr -> AbsSecur > ParamVal[3])||
      (InfoPtr -> AbsSecur < ParamVal[4]))
      {  /* out of range */
         printf("X1\n");
         return(EUNAUTH);
       }
      XPtr -> ScMaxOut = InfoPtr -> AbsSecur;
      XPtr -> ScMinOut = InfoPtr -> AbsSecur;
    }
   else
   {  if (InfoPtr -> MaxSecur < 0)
      {  /* maximum security level specified */
         InfoPtr -> MaxSecur =& 0177;

         if (InfoPtr -> MaxSecur > 15)
         {  /* too big */
            return(EBADPARAM);
          }

         if ((InfoPtr -> MaxSecur > ParamVal[3])||
         (InfoPtr -> MaxSecur < ParamVal[4]))
         {  /* out of range */
            printf("X2\n");
            return(EUNAUTH);
          }
         XPtr -> ScMaxOut = InfoPtr -> MaxSecur;
         XPtr -> ScMinOut = ParamVal[4];
       }
      else
      {  if (InfoPtr -> OpenMode == 1)
         {  /* unspecified listen here */
            XPtr -> ScMaxOut = -1;  /* mark unspecified */
            XPtr -> ScMinOut = -1;
          }
         else
         {  XPtr -> ScMaxOut = ParamVal[3];
            XPtr -> ScMinOut = ParamVal[4];
          }
       }
    }

   /* set the security parameters for net in */
   XPtr -> ScMaxIn  = ParamVal[1];  /* max net to user */
   XPtr -> ScMinIn  = ParamVal[2];  /* min net to user */

   /* do the precedence checking */
   if ((InfoPtr -> SendPrec) < 0)
   {  /* send precedence specified */
      InfoPtr -> SendPrec =& 0177;

      if (InfoPtr -> SendPrec > 15)
      {  /* too big */
         return(EBADPARAM);
       }

      if (InfoPtr -> SendPrec > ParamVal[5])
      {  /* out of range */
         printf("X6\n");
         return(EUNAUTH);
       }
      else
      {  XPtr -> SndPrec = InfoPtr -> SendPrec;
       }
    }
   else
   {  XPtr -> SndPrec = MINPREC;
    }

   if ((InfoPtr -> RecvPrec) < 0)
   {  /* minimum receive precedence specified */
      InfoPtr -> RecvPrec =& 0177;

      if (InfoPtr -> RecvPrec > 15)
      {  /* too big */
         return(EBADPARAM);
       }

      XPtr -> MnRcvPrec = InfoPtr -> RecvPrec;

    }
   else
   {  XPtr -> MnRcvPrec = MINPREC;
    }

   /* set maximum precedence since maximum
   receive precedence is not specified by user */
   XPtr -> MxRcvPrec = MAXPREC;

   /* save auth table max send prec */
   XPtr -> ATPrec = ParamVal[5];

   /* do the TCC checking */

   if (InfoPtr -> NumbTcc == 0)
   {  /* unspecified - get values from auth table */
      printf("no TCC\n");
      if (InfoPtr -> OpenMode == 0)
      {  /* just get the first one */
         InfoPtr -> OTcc[0] = ParamVal[6];
         InfoPtr -> NumbTcc = 1;
       }
      else
      {  i = 0;
         for (j = 6; j < k; j++)
         {  InfoPtr -> OTcc[i] = ParamVal[j];
            i++;
          }
         InfoPtr -> NumbTcc = k - 6;
       }
    }
   else
   {  /* check each TCC against authtable values */

      for (i = 0; i < InfoPtr -> NumbTcc; i++)
      {  j = 6;
         Found = 0;
         while((Found == 0)&&(j < k))
         {  if (InfoPtr -> OTcc[i] == ParamVal[j])
            {  Found = 1;
             }
            else
            {  j++;
             }
          }

         if (Found == 0)
         {  /* bad Tcc */
            printf("X8\n");
            return(EUNAUTH);
          }
       }
    }
   /* copy the TCC list into the TCB */
   for (i = 0; i < InfoPtr -> NumbTcc; i++)
   {  XPtr -> TccList[i] = InfoPtr -> OTcc[i];
    }
   XPtr -> TCCCnt = InfoPtr -> NumbTcc;

   return(0);
 }


GetFields(Ident, PPtr)

int Ident;

struct
{  int ParamVal[20];
 } *PPtr;


{
   struct buf
   {  int fildes;
      int nunused;
      char *xfree;
      char buff[512];
    } ibuf;

   int Numb;
   char UserNmb[6];
   char Chr;
   int State;
   int Halt;
   int Found;
   int Error;
   int i,j,k;
   int AuthFds;

   /* scan Authorization Table for this user */

   Error = fopen("authtable", &ibuf);

   if (Error == -1)
   {  /* can't open it */
      printf("Authorize: can't open auth table\n");
      return(-1);
    }

   AuthFds = ibuf.fildes;

   State = 0;    /* looking for colon */
   while ((Chr = getc(&ibuf)) != -1)
   {
      switch(State)
      {  case 0:    /* looking for colon */
         {  if (Chr == ':')
            {  /* get user number */
               i = 0;
               State = 1;    /* check user number */
             }
            break;
          }
         case 1:    /* check user number */
         {  if (Chr == ':')
            {  /* end of string */
               UserNmb[i] = '\0';    /* terminator */

               /* convert string to number */
               Numb = atoi(UserNmb);

               if (Numb == Ident)
               {  /* found it */
                  State = 2;    /* get rest of values */
                  k = 0;
                  i = 0;
                }
               else
               {  /* go to next line */
                  State = 3;    /* look for end of line */
                }
             }
            else
            {  /* stash it away */
               UserNmb[i] = Chr;
               i++;
             }
            break;
          }

         case 2:    /* get rest of values */
         {  switch(Chr)
            {  case ':':
               case '\n':
               {  /* end of string */
                  UserNmb[i] = '\0';
                  PPtr -> ParamVal[k] = atoi(UserNmb);
                  i = 0;
                  k++;

                  if (k > 6 + NUMBTCC)
                  {  /* too many entries */
                     printf("X5\n");
                     close(AuthFds);
                     return(-1);
                   }

                  if (Chr == '\n')
                  {  /* end of line */

                     if (k < 7)     /* the minimum number of entries */
                     {  close(AuthFds);
                        return(-1);
                      }

                     close(AuthFds);
                     return(k);
                   }
                  break;
                }

               default:
               {  UserNmb[i] = Chr;
                  i++;
                }
             }
            break;
          }

         case 3:    /* look for new line */
         {  if (Chr == '\n')
            {  State = 0;
             }
            break;
          }
       }
    }
   printf("X4\n");
   close(AuthFds);
   return(-1);
 }
