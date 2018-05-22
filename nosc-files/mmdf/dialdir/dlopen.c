# include  "rm.h"
# include  "rmerr.h"
# include  "rmenv.h"




/*
 *     Local convenience definitions
 */

# define  FATAL        1          /*  error has no recovery or retry  */
# define  NONFATAL     2          /*  error with success possible with retry  */

# define  PORTRAW   0341          /*  mode of port when interpreting script  */
# define  PORTCONN  0301          /*  port mode during protocol  */




/*  globals for 'dlopen'  */

  struct
    {
    int  o_sline;                 /*  number of current line in script file  */
    int  o_nqueue;                /*  number of characters in 'o_inque'  */
    char  o_slave[LSLAVNAM];      /*  slave startup string  */
    char  o_canon[30];            /*  translated canonical phone number  */
    char  o_inque[PUSHBACK];      /*  queue for pushed back input characters  */
    char  o_portnm[64];           /*  name of dial out port  */
    char  o_linebuf[MAXLINE];     /*  line buffer for script file  */
    char  *o_inqpt;               /*  pointer to 'o_inque'  */
    struct iobuf  o_sbuff;        /*  io buffer for script file  */
    }  d_open;

  extern struct rmenv  rmenv;     /*  protocol environment structure  */
  extern char dialdev[]; /* dialing device name */





/*
 *      routine which is called to establish a connection with the other
 *      side.
 */

dlopen(scriptfile, port, logfile, tranfile, debugf, ttylog)
  char  *scriptfile, *port, *logfile, *tranfile;
  int  debugf, ttylog;
    {
    extern int  errno;
    register int  result;
    char  msgbuff[MAXMESG];

/*  set up defaults  */

    rmenv.d_linelgh = MAXMESG;
    rmenv.d_window = WINDOW;

/*  start logging and save the port name  */

    result = d_strtlog(logfile, tranfile, debugf, ttylog);
    if (result < 0)
      return(result);

    cpstr(port, d_open.o_portnm);

/*  try to open script file  */

    if (fopen(scriptfile, &d_open.o_sbuff) < 0)
      {
      d_log("***** couldn't open script file - errno %d", errno);
      return(EUSER);
      }

/*  now interpret it  */

    result = d_script();
    if (result < 0)
      return(result);

/*  write the slave startup string on the port and look for a RUN reply  */

    alarm(MATCHTIM);
    result = write(rmenv.d_portfd, d_open.o_slave, length(d_open.o_slave));
    alarm (0);
    if (result < 0)
      {
      d_log("***** error writing slave startup string on port - errno %d",
            errno);
      return(ESYSTEM);
      }
    result = d_initrecv();
    if (result < 0)
      return(result);

    result = d_initsend();
    if (result < 0)
      return(result);

    return(OK);
    }




/*
 *     this routine is called to close the port and cause the line to
 *     be hung up
 */

dlclose()
    {

    if (close(rmenv.d_portfd) < 0)
      {
      d_log("***** error closing port - errno %d", errno);
      return(ESYSTEM);
      }

    return(OK);
    }




/*
 *     this routine reads the script file and interprets it
 */

d_script()
    {
    register int  nfields, result;
    char  *fields[MAXFIELDS], sendbuff[MAXLINE];

/*  read in lines from the script file and break them up into fields.  */
/*  a 0 return from the 'parselin' function indicates that a blank     */
/*  line was encountered, which we'll ignore                           */

    while (1)
      {
      nfields = parselin(fields, d_open.o_linebuf, &d_open.o_sbuff, MAXLINE, MAXFIELDS);
      d_open.o_sline++;

      switch (nfields)
        {
        case  0:

            continue;

        case  -1:

            d_serror("unexpected eof");
            return(EUSER);

        case  -2:

            d_serror("line too long");
            return(EUSER);

        case  -3:

            d_serror("too many fields on line");
            return(EUSER);

        case  -4:

            d_serror("quoted string not closed");
            return(EUSER);
        }

/*  now use the code character in field 0 to determine function  */

      switch (*fields[0])
        {

/*  get phone number and dial it  */

        case  'p':

            if (nfields != 2)
              {
              d_serror("2 fields expected for 'p'");
              return(EUSER);
              }

            result = d_illchar(fields[1], d_open.o_canon);
            if (result < 0)
              return(result);

            result = d_dodial(d_open.o_canon);
            if (result < 0)
              return(result);

            continue;

/*  get startup string for slave process  */

        case  'u':

            if (nfields != 2)
              {
              d_serror("2 fields expected for 'u'");
              return(EUSER);
              }

            if (d_canon(fields[1], d_open.o_slave) < 0)
              {
              d_serror("canonical string error");
              return(EUSER);
              }

            continue;

/*  send string to remote system  */

        case  's':
        case  't':

            if (nfields != 2)
              {
              d_serror("2 fields expected for 's' or 't'");
              return(EUSER);
              }

            if (d_canon(fields[1], sendbuff) < 0)
              {
              d_serror("canonical string error");
              return(EUSER);
              }

            d_dbglog("script: sending '%s'", fields[1]);

            result = d_strout(sendbuff);
            if (result < 0)
              return(result);

            continue;

/*  watch for string from remote system  */

        case  'r':

            result = d_strwatch(fields, nfields);
            if (result < 0)
              return(result);

            continue;

/*  pause in script interpretation to do the file transfers  */

        case  'g':

            if ((result = d_chgport(PORTCONN)) < 0)
              return(result);

            d_log("script pause - starting protocol");
            return(OK);

/*  unrecognized script command character  */

        default:

            d_serror("unrecognized script command character");
            return(EUSER);
        }
      }
    }




/*
 *     routine which manages the dialing.  it forks off the dialer and then
 *     trys to open the port.  if the open completes without error, we have
 *     a successful connection.  if the call was interrupted by a signal,
 *     die quietly.
 */

d_dodial(number)
  char  *number;
    {
    extern int  errno;
    register int  result;
    int pid;
    int  status;

/*  spawn the dialer  */

    result = d_dialer(number, &pid);
    if (result < 0)
      return(result);

/*  try to open the dialer.  if there's and error and it's and interrupt,  */
/*  then the dialer may have had a problem.  check for the right signal    */
    alarm(MATCHTIM);
    rmenv.d_portfd = open(d_open.o_portnm, 2);
    alarm (0);
    if (rmenv.d_portfd < 0)
      if (errno != UEINTR)
        {
	kill (pid, 9);
        d_log("error opening port '%s' errno %d", d_open.o_portnm, errno);
        return(ESYSTEM);
        }

      else
        {
	alarm(MATCHTIM);
	while ((result = waita(&status)) != pid && result != -1);
	alarm (0);
        if (status.hibyte == SIGIOT)
          {
          d_log("dial failed");
          return(ESYSTEM);
          }
        else
          {
          d_log("signal %d interrupted port open call", status.hibyte);
          return(ESYSTEM);
          }
        }

    else
      if ((result = d_setport(PORTRAW)) < 0)
        {
	alarm(MATCHTIM);
	while ((result = waita(&status)) != pid && result != -1);
	alarm (0);
        return(result);
        }

/*  collect the dead child  */

	alarm(MATCHTIM);
	while ((result = waita(&status)) != pid && result != -1);
	alarm (0);
    d_log("connection established");
    }




/*
 *     this routine spawns a child process to open the acu and do the
 *     actual dialing of the number.  the argument string must have the
 *     exact format to be passed to the dialer including tandem dialing
 *     indiactors
 */

d_dialer(number, pid)
  char  *number;
  int *pid;     /* so caller can do wait        */
    {
    extern int  errno;
    extern  d_iotcatch();
    register int  result, try;
    int  parentpid;

    parentpid = getpid();
    signal(SIGIOT, d_iotcatch);

/*  fork so dial process can proceed in parallel  */

/*    d_dbglog("dialer: calling number '%s'", number);*/
    d_dbglog("dialer: calling number");

    *pid = tryfork();

    if (*pid < 0)
      {
      d_log("couldn't fork for dialer");
      return(ESYSTEM);
      }

/*  parent just returns and child does actual dialing  */

    if (*pid)
      return(0);

    for (try = 0; try < NDIALS; try++)
      {
      d_log("Dialing...");
      result = d_realdial(number);

      if (result == FATAL)
        break;

      if (try < (NDIALS - 1))
        sleep(DIALTIME);
      }

/*  we can't get through, so we should signal our parents and die  */

    kill(parentpid, SIGIOT);
    exit (-1);
    }




/*
 *     this routine does the real dialing and decoding of errors
 */
dlopsig ()
{
    signal (14, &dlopsig);
}
d_realdial(number)
  register char  *number;
    {
    register int  acu, flag, errsave;

/*  open the acu, and if we're successful write the number on it  */
    signal (14, &dlopsig);
    flag = 0;
    alarm(MATCHTIM);
/*    acu = open("/dev/acu13",2); */
	if(dialdev[0]==0){
		exit(0);
	}
    acu = open(dialdev,2);
    if (acu != -1)
      {
      flag = 1;
    alarm(MATCHTIM);
      if (write(acu,number,length(number)) != -1)
        exit();
      }
    alarm (0);

/*  handle all errors.  close the acu first so we can return with no  */
/*  extraneous open files to cause problems with retries              */

    errsave = errno;
    close(acu);

    switch (errsave)
      {
      case  UEBUSY:

          if (flag)
            d_log("That number is busy");
          else
            d_log("dialer in use");
          return(NONFATAL);

      case  UEDNPWR:

          d_log("dialer power off");
          return(FATAL);

      case  UEDNABAN:

          d_log("call abandoned");
          return(NONFATAL);

      case  UEDNDIG:

          d_log("internal error: bad digit to dialer");
          return(FATAL);

      default:

          d_log("system error #%d", errsave);
          return(FATAL);
      }
    }




/*
 *     routine to catch iot signals so they'll interrupt open calls to the
 *     port
 */

d_iotcatch()
    {

    signal(SIGIOT, d_iotcatch);
    }




/*
 *     this routine interprets the match string line and attempts to
 *     find the sequence in the input stream
 */

d_strwatch(fields, nfields)
  char  *fields[];
  int  nfields;
    {
    register int  timer, val;
    char  tbuff[80];

    if ((nfields < 2) || (nfields > 3))
      {
      d_serror("line format error");
      return(EUSER);
      }

    if (d_canon(fields[1], tbuff) < 0)
      {
      d_serror("string format error");
      return(EUSER);
      }

    if (length(tbuff) > PUSHBACK)
      {
      d_serror("match string too long");
      return(EUSER);
      }

/*  convert the time out value if one is given  */

    if (nfields == 3)
      {
      timer = atoi(fields[2]);

      if ((timer < 0) || (timer > 300))
        {
        d_serror("bad timeout value");
        return(ETIMEOUT);
        }
      }

    else
      timer = MATCHTIM;

/*  look for the string.  watch for interrupted calls from the alarm  */

    d_dbglog("strwatch: trying to match '%s'", tbuff);

    alarm(timer);
    while (1)
      {
      val = d_match(tbuff);

      if (val == -1)
        if (d_rawgetc() < 0)
          break;
        else
          continue;

      if (val == -2)
        break;

      alarm(0);
      return(OK);
      }

/*  alarm timeouts come here  */

    d_log("no match for '%s' after %d seconds", fields[2], timer);
    return(EUSER);
    }




/*
 *     routine to translate the external form of a startup string to an
 *     internal 'canonical' form
 */

d_canon(in,out)
  register char  *in,*out;
    {
    extern char  *d_escvalue();
    register int  value;
    char  c;

/*  check for obvious bad format  */

    if (*in++ != '"')
      return(-1);

/*  loop through all the characters.  lots of special cases  */

    while (1)
      {
      c = *in++;

      switch (c)
        {
        default:

            *out++ = c;
            continue;

        case  '\0':

            return(-1);

        case  '"':

            *out = '\0';
            return(0);

        case  '\\':

/*  handle the escapes  */

            c = *in++;

            switch (c)
              {
              case  '"':

                  *out++ = '"';
                  continue;

              case  '\0':

                  return(-1);

              case  '\\':

                  *out++ = '\\';
                  continue;

              case  'r':

                  *out++ = 015;
                  continue;

              case  'n':

                  *out++ = 012;
                  continue;

              case  't':

                  *out++ = 011;
                  continue;

              case  'x':

                  *out++ = 0377;
                  continue;

/*  allow the specification of characters by up to 3 octal digits after a  */
/*  backslash                                                              */

              default:

                  in = d_escvalue(--in,out++);

              }
        }
      }
    }




/*
 *     this routine does the translation of numerical escape sequences into
 *     a character with the given octal value
 */

char  *d_escvalue(string,out)
  register char  *string,*out;
    {
    register int  value;
    int  count;
    char  c;

    value = 0;
    c = *string++;

    if ((c < '0') || (c > '7'))
      {
      *out = c;
      return(string);
      }

    value = c & 07;

    for (count = 0; count <= 1; count++)
      {
      c = *string++;

      if ((c >= '0') && (c <= '9'))
        value = (value << 3) | (c & 07);
      else
        {
        *out = value;
        return(--string);
        }
      }

    *out = value;
    return(string);
    }




/*
 *     routine which checks for legal characters in a phone number, discards
 *     extraneous ones, and performs some mapping.  it complains about
 *     illegal ones and number longer than 20 digits or codes.
 */

d_illchar(string,outbuff)
  register char  *string,*outbuff;
    {
    register int  count;

    count = 0;

    while (*string)
      {

      if (count > 20)
        {
        d_serror("phone number too long");
        return(EUSER);
        }

      if ((*string >= '0') && (*string <= '9'))
        {
        *outbuff++ = *string++;
        count++;
        continue;
        }

      switch (*string)
        {
        case  '(':
        case  ')':
        case  '-':

            string++;
            continue;

        case  ':':

            *outbuff++ = '=';
            count++;
            string++;
            continue;

        default:
		*outbuff++ = *string++;
		count++;
		continue;

/*            d_serror("unknown character in phone number");
            return(EUSER); */
        }
      }

    return(0);
    }




/*
 *     this routine checks for a match between the string and the input
 *     stream.  what it dies is read characters from the input and compare
 *     them with the string.  if a match is not found, all the characters
 *     that have been read are pushed back.  if a match is read, the
 *     characters are eaten.  1 is returned on a match, -1 on failure.
 */

d_match(string)
  char  *string;
    {
    register int  c, val;

    if (*string == '\0')
      return(1);

    c = d_rawgetc();

    if (c < 0)
      return(c);

    if (c == *string)
      {
      val = d_match(++string);

      if (val < 0)
        {
        und_rawgetc(c);
        return(val);
        }
      else
        return(1);
      }

    else
      {
      und_rawgetc(c);
      return(-1);
      }
    }




/*
 *     this routine reads a character from the comm line and filters out the
 *     wierd ones.  this routine is called only during script interpretation.
 */

d_rawgetc()
    {
    extern int  errno;
    register int  result;
    char  c;

/*  if there's anything queued, use it first  */

    if (d_open.o_nqueue)
      {
      d_open.o_nqueue--;
      return(*d_open.o_inqpt--);
      }

/*  no such luck.  we have to do a read.  don't return anything wierd  */

    while (1)
      {
      result = read(rmenv.d_portfd, &c, 1);
      if (result == 0)
        return(ESYSTEM);

      if (result < 0)
        if (errno == UEINTR)
          return(ETIMEOUT);
        else
          return(ESYSTEM);


      c =& 0177;

      if (((c < 040) && (c != '\t') && (c != '\n')) || (c == 0177))
        continue;

/*  write it on the transcript file  */

      d_tscribe(&c, 1);
      return(c);
      }
    }




/*
 *      this returns a character to the input queue so he'll get read again
 */

und_rawgetc(c)
  char  c;
    {

/*  if none are currently queued, set things up  */

    if (d_open.o_nqueue == 0)
      {
      d_open.o_inqpt = d_open.o_inque;
      *d_open.o_inqpt = c;
      d_open.o_nqueue = 1;
      }

    else
      {
      *++d_open.o_inqpt = c;
      d_open.o_nqueue++;
      }
    }




/*
 *     routine which dumps a string on the port and also interprets the
 *     sleep characters
 */

d_strout(string)
  char  *string;
    {
    int result;

    while (*string)
      {
      if (*string == 0377)
        sleep(1);

      else
	{
	alarm(MATCHTIM);
	result = write(rmenv.d_portfd, string, 1);
	  alarm (0);
	if (result <= 0)
          {
          d_log("raw mode port write error");
          return(ESYSTEM);
          }
	}
      string++;
      }
    }




/*
 *     routine to log errors found while interpreting script file
 */

d_serror(message)
  char  *message;
    {

    d_log("***** Script %d: %s", d_open.o_sline, message);
    }
