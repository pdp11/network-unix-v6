# include  "rm.h"
# include  "rmerr.h"
# include  "rmenv.h"



# define  SEQCHAR       4      /*  index in message of sequence character  */
# define  CODECHAR      5      /*  index in message of code character  */
# define  TEXTCHAR      6      /*  index in message of start of text  */


  struct rmenv  rmenv;         /*  protocol environment structure  */

  extern char  *cpstr();       /*  routine which does string copying  */





/*
 *     this routine is called to read a message from the standard input.
 *     the strategy here is to ignore all messages that have an error of
 *     one kind or another, and acknowledge all others.  the routine loads
 *     the command and text part of the message into the buffer given as the
 *     argument, and returns the length.  a return value <= 0 indicates an
 *     interrupted sys call, presumably by alarm.
 */

d_getmsg(message, msgtype)
  int  msgtype;
    {
    register char  *cp;
    register int  sum, nbytes;
    int  count, error, chksum, seqno, intrflag, timeout;
    char  msgbuff[MAXMESG + 1];

    timeout = msgtype == ACKONLY ? TIMEOUT : MATCHTIM; /* short ack timeout */
    while (1)
      {
      nbytes = d_getline(msgbuff, rmenv.d_linelgh + 1, timeout);

      if (nbytes <= 0)
        return(nbytes);

/*  check for line that's too long  */

      if (nbytes > (rmenv.d_linelgh + 1))
        continue;

      msgbuff[nbytes - 1] = '\0';

      d_dbglog("getmesg: mesg '%s'", msgbuff);

/*  check for an obviously short line  */

      if (nbytes <= HEADLNGH)
        {
        d_log("***** getmesg: message too short");
        continue;
        }

/*  check for proper checksum characters  */

      cp = msgbuff;
      error = 0;

      for (count = 0; count <= 3; count++)
        if (d_ishex(*cp++) == 0)
          {
          error++;
          break;
          }

      if (error)
        {
        d_log("***** getmesg: invalid checksum character");
        continue;
        }

      chksum = d_atoh(msgbuff, 4);

/*  accumulate and verify checksum  */

      sum = 0;
      cp = &msgbuff[SEQCHAR];

      while (*cp)
        sum =+ *cp++;

      if (sum != chksum)
        {
        d_log("***** getmesg: checksum error");
        continue;
        }

/*  check the sequence number.  an ACK should have the current transmit  */
/*  sequence number to be valid.  all others have the receive sequence   */
/*  if we've received a repeat of the last message, our ACK may have     */
/*  been stomped, so ACK it again but ignore the message                 */

      seqno = d_atoh(&msgbuff[SEQCHAR], 1);
      intrflag = (seqno & 010) >> 3;
      seqno =& 07;

      if (msgbuff[CODECHAR] == ACK)
        {
        if (seqno != rmenv.d_transeq)
          {
          d_log("***** getmesg: ACK sequence error");
          continue;
          }
        }

      else
        if (seqno != rmenv.d_rcvseq)
          {
          if (seqno == rmenv.d_lastseq)
            d_sndmsg(ACK, 0);
          else
            d_log("***** getmesg: sequence error");

          continue;
          }

        else
          {
          rmenv.d_lastseq = rmenv.d_rcvseq;
          rmenv.d_rcvseq = (rmenv.d_rcvseq + 1) % 8;
          }

/*  ignore the message types that we're supposed to  */

      if (msgtype == ACKONLY)
        if (msgbuff[CODECHAR] != ACK)
          continue;
        else
          break;

      else
        if (msgbuff[CODECHAR] == ACK)
          continue;
        else
          {
          d_sndmsg(ACK, 0);
          break;
          }
      }

/*  check for EXIT messages and handle them here  */

    if (msgbuff[CODECHAR] == EXIT)
      {
      d_log("***** EXIT received");
      return(EEXIT);
      }

/*  copy the message into the user's buffer.  if there was no interrupt  */
/*  return the length, otherwise an error code                           */

    if (intrflag)
      {
      cpstr(&msgbuff[TEXTCHAR], rmenv.d_errmesg);
      return(EINTRUPT);
      }

    else
      {
      cpstr(&msgbuff[CODECHAR], message);
      return(nbytes - HEADLNGH);
      }
    }




/*
 *     routine which sends a message to the other end.  the text is sent
 *     for all messages execpt ACK.
 */

d_sndmsg(code, text)
  char  code;
  char  *text;
    {
    register char  *cp;
    register int  sum, count;
    char  msgbuff[MAXMESG + 1];
    int  seqno, byte, result;

    d_dbglog("sendmesg: code '%c', text '%s'", code, text);
    cp = &msgbuff[SEQCHAR];

/*  set up the code and sequence number.  if we're sending an ACK and there  */
/*  is a pending transmit interrupt,  redirect the text pointer so the       */
/*  reason will be sent                                                      */

    if (code == ACK)
      if (rmenv.d_tranint)
        {
        seqno = rmenv.d_rcvseq | 010;
        text = rmenv.d_intmesg;
        rmenv.d_tranint = 0;
        }
      else
        seqno = rmenv.d_rcvseq;

    else
      {
      seqno = rmenv.d_transeq;
      rmenv.d_transeq = (rmenv.d_transeq + 1) % 8;
      }

    d_htoa(seqno, cp++, 1);
    *cp++ = code;
    sum = msgbuff[SEQCHAR] + msgbuff[CODECHAR];
    count = HEADLNGH + 1;

/*  send text if there is any  */

    if (text)
      while (*text)
        {
        *cp++ = *text;
        sum =+ *text++;
        count++;
        }

    *cp++ = '\n';
    *cp = '\0';

/*  tack on the checksum  */

    d_htoa(sum, msgbuff, 4);

/*  send it  */

    result = d_sndack(msgbuff, count, code);
    return(result);
    }




/*
 *     this routine sends a message to the other side and waits for
 *     acknowledgement.  this is where the retransmission is done.
 */

d_sndack(message, length, code)
  char  *message, code;
  int  length;
    {
    extern int  d_alarmsig(), errno;
    register int  nbytes, retry;
    char  respbuff[MAXMESG + 1];

/*  do this for the specified number of tries.  ignore all but ack messages  */

    signal(SIGALARM, d_alarmsig);

    message[length - 1] = '\0';
    d_dbglog("sendack: mesg '%s'", message);
    message[length - 1] = '\n';

    for (retry = 0; retry < NRETRIES; retry++)
      {
      if (write(rmenv.d_portfd, message, length) != length)
        {
        d_log("***** sendack: error writing on port errno %d", errno);
        return(ESYSTEM);
        }

      d_tscribe(message, length);

      if (code == ACK)
        return(OK);


/*  examine the replies.  watch for interrupts from the other end, timeout  */
/*  interrupts, and end of file on port                                     */


      while (1)
        {
        nbytes = d_getmsg(respbuff, ACKONLY);

        if (nbytes == EINTRUPT)
          {
          d_log("sendack: interrupt received '%s'", rmenv.d_errmesg);
          return(EINTRUPT);
          }

        if (nbytes == ESYSINTR)
          break;

        if (nbytes == 0)
          {
          d_log("***** sendack: end of file from port");
          return(ESYSTEM);
          }

        if (nbytes < 0)
          {
          d_log("***** sendack: unexpected error return from 'getline' %d",
               nbytes);
          return(nbytes);
          }

        if (respbuff[0] == ACK)
          {
          return(OK);
          }
        }

      d_log("sendack: retransmitted");
      }

    d_log("sendack: transmission error after %d retries", NRETRIES);
    return(ETRANS);
    }




/*
 *     this routine reads a line up to 'nbytes' long from the standard
 *     input and loads it into the designated buffer.  it returns the
 *     number of bytes trasnferred which is less than or equal to 'nbytes'.
 *     if the returned value is greater than 'nbytes', then the input line
 *     is longer than requested and some unspecified amount has been loaded
 *     into 'buffer'.
 */

d_getline(buffer, nbytes, timeout)
  char  *buffer;
  int  nbytes, timeout;
    {
    extern int  errno;
    register char  *in, *out;
    register int  count;
    int  result;
    char  lbuff[256];

    while (1)
      {
      alarm (timeout);
      result = read(rmenv.d_portfd, lbuff, 256);
      alarm (0);
      if (result == 0)
        return(0);

      if (result < 0)
        if (errno == UEINTR)
          return(ESYSINTR);
        else
          {
          d_log("getline: error on port read - errno %d", errno);
          return(ESYSTEM);
          }

      d_tscribe(lbuff, result);

/*  walk through the raw input line and filter out the junk  */

      in = lbuff;
      out = buffer;
      count = 0;

      while (result--)
        {
        if (*in == '\n')
          {
          *out = '\n';

          if (++count == 1)
            break;
          else
            return(count);
          }

        if (*in < 040)
          {
          in++;
          continue;
          }

        *out++ = *in++;

        if (++count == nbytes)
          return(0);
        }
      }
    }




/*
 *     this routine does the conversion from clear text to the grave
 *     encoding.  'in' is a pointer to a character, and 'out' is a pointer
 *     to the place where the encoded string should go.  the number of
 *     bytes output is returned.
 */

d_tograve(in, out)
  char  *in, *out;
    {
    char  c;

    c = *in & 0177;

/*  control characters.  codes 01 to 032 map into '`A' to '`Z'.  033 to 037  */
/*  map into '`a' to '`e'                                                    */

    if ((c >= 01) && (c <= 037))
      {
      *out++ = '`';
      if (c <= 032)
        *out = c + 0100;
      else
        *out = c + ('a' - 033);
      return(2);
      }

/*  special cases  */

    switch (c)
      {
      case  '\0':

          *out++ = '`';
          *out = ' ';
          return(2);

      case  '@':

          *out++ = '`';
          *out = '0';
          return(2);

      case  '^':

          *out++ = '`';
          *out = '1';
          return(2);

      case  '`':

          *out++ = '`';
          *out = '`';
          return(2);

      case  0177:

          *out++ = '`';
          *out = '2';
          return(2);

      default:

          *out = c;
          return(1);
      }
    }




/*
 *     this routine decodes a grave format string
 */

d_fromgrave(ain, aout)
  char  *ain, *aout;
    {
    register char  *in, *out;
    register int  count;

    in = ain;
    out = aout;
    count = 0;

    while (*in)
      {
      if (*in != '`')
        {
        *out++ = *in++;
        count++;
        continue;
        }

/*  we've got a grave.  look at the next one;  control characters first,  */
/*  then the special cases                                                */

      in++;
      if ((*in >= 'A') && (*in <= 'Z'))
        {
        *out++ = *in++ - 0100;
        count++;
        continue;
        }

      if ((*in >= 'a') && (*in <= 'e'))
        {
        *out++ = *in++ - ('a' - 033);
        count++;
        continue;
        }

/*  the wierd ones  */

      switch (*in)
        {
        case  '0':

            *out++ = '@';
            break;

        case  '1':

            *out++ = '^';
            break;

        case  '2':

            *out++ = 0177;
            break;

        case  '`':

            *out++ = '`';
            break;

        case  ' ':

            *out++ = '\0';
            break;

        case  '\0':

            *out++ = '`';
            *out = '\0';
            return(count + 1);

        default:

            *out++ = '`';
            *out++ = *in++;
            count =+ 2;
            continue;
        }

      count++;
      in++;
      }

    *out = '\0';
    return(count);
    }




/*
 *     this routine looks at the messages from the other side, watching for
 *     the desired type or ERROR.  it returns 1 for successful match, 0 for
 *     an ERROR encountered, and negative for internal failures
 */

d_response(desired, msgbuff)
  char  desired, *msgbuff;
    {
    extern int  d_alarmsig();
    register int  count;

    d_dbglog("response:  looking for '%c' message", desired);

/*  we ignore everything execpt errors and what we're looking for  */

    signal(SIGALARM, d_alarmsig);
    alarm(TIMEOUT);

    while (1)
      {
      count = d_getmsg(msgbuff, NOTACK);

      d_dbglog("response:  count '%d'  message '%s'", count, msgbuff);

      if (count < 0)
        {
        alarm(0);

        if (count == ESYSINTR)
          return(ETIMEOUT);
        else
          return(count);
        }

      if (msgbuff[0] == desired)
        {
        alarm(0);
        return(count);
        }

      if (msgbuff[0] == ERROR)
        {
        alarm(0);
        cpstr(&msgbuff[1], rmenv.d_errmesg);
        return(0);
        }
      }
    }




/*
 *     this routine is called by both the master and the slave to initialize
 *     the protocol by sending RUN messages with the rmenv.d_window value and maximum
 *     line length
 */

d_initsend()
    {
    register int  result;
    char  msgbuff[24];

/*  load rmenv.d_window value and maximum line length  */

    msgbuff[0] = rmenv.d_window + 060;

    if (rmenv.d_linelgh > 99)
      itoa(rmenv.d_linelgh, &msgbuff[1]);
    else
      {
      msgbuff[1] = '0';
      itoa(rmenv.d_linelgh, &msgbuff[2]);
      }

    d_dbglog("d_initsend:  startup mesg '%s'", msgbuff);

    result = d_sndmsg(RUN, msgbuff);
    return(result);
    }




/*
 *     this routine is used by the master and slave to receive the RUN
 *     message and adjust the rmenv.d_window and line length variables
 */

d_initrecv()
    {
    register int  length, twindow, tlinelngh;
    char  msgbuff[MAXMESG];

/*  look for matching RUN message from the master  */

    while (1)
      {
      length = d_getmsg(msgbuff, NOTACK);

      d_dbglog("d_initrecv:  message received '%s'", msgbuff);

      if (length < 0)
        {
        alarm(0);

        if (length == ESYSINTR)
          return(ETIMEOUT);
        else
          return(length);
        }

      if (msgbuff[0] == RUN)
        {
        alarm(0);
        break;
        }

      if (msgbuff[0] == ERROR)
        {
        alarm(0);
        d_log("***** ERROR response to RUN '%s'", &msgbuff[1]);
        return(EPROTO);
        }
      }

/*  we've got a RUN, but have to check it out  */

    if (length != LRUNMESG)
      {
      d_log("***** RUN length error");
      d_sndmsg(ERROR, "RUN message length error");
      return(EPROTO);
      }

/*  get and check the rmenv.d_window value  */

    twindow = msgbuff[1] - 060;

    if ((twindow < 1) || (twindow > 7))
      {
      d_log("***** illegal window value received %d", twindow);
      d_sndmsg(ERROR, "illegal window value");
      return(EPROTO);
      }

    if (twindow < rmenv.d_window)
      {
      d_log("setting window value to %d", twindow);
      rmenv.d_window = twindow;
      }

/*  do the same for the maximum line length  */

    tlinelngh = atoi(&msgbuff[2]);

    if ((tlinelngh < 32) || (tlinelngh > 128))
      {
      d_log("***** dlstart: bad line length %d", tlinelngh);
      d_sndmsg(ERROR, "illegal line length");
      return(EPROTO);
      }

    if (tlinelngh < rmenv.d_linelgh)
      {
      rmenv.d_linelgh = tlinelngh;
      d_log("setting maximum line length to %d", tlinelngh);
      }

    d_log("protocol running");
    return(OK);
    }




/*
 *     convert 'nplaces' of the hex number stored in 'buffer' and
 *     return the value
 */

d_atoh(buffer, nplaces)
  char  *buffer;
  int  nplaces;
    {
    register int  value, digit;
    register char  *cp;
    int  count;

    cp = buffer;
    value = 0;

    for (count = 1; count <= nplaces; count++)
      {
      if ((*cp >= '0') && (*cp <= '9'))
        digit = *cp++ - '0';
      else
        digit = *cp++ - 'A' + 10;

      value = (value << 4) | (digit & 017);
      }

    return(value);
    }




/*
 *     convert 'value' to an 'nplaces' hex number and store it in 'buffer'
 */

d_htoa(value, buffer, nplaces)
  int  value, nplaces;
  char  *buffer;
    {
    register int  count, digit;
    register char  *cp;

    cp = &buffer[nplaces - 1];

    for (count = 1; count <= nplaces; count++)
      {
      digit = value & 017;
      value =>> 4;

      if (digit < 10)
        *cp-- = digit + '0';
      else
        *cp-- = digit - 10 + 'A';
      }
    }




/*
 *     returns 1 if the character 'c' is a valid hex digit, 0 otherwise
 */

d_ishex(c)
  char  c;
    {

    if (((c >= '0') && (c <= '9')) || ((c >= 'A') && (c <= 'F')))
      return(1);

    return(0);
    }




/*
 *     this routine is called when an alarm signal is caught
 */

d_alarmsig()
    {

    signal(SIGALARM, d_alarmsig);
    }




