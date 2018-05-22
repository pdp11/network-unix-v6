/*
 *     Configuration parameters
 */

# define  MAXMESG     128     /*  maximum message length  */
# define  WINDOW        1     /*  maximum window value  */
# define  HEADLNGH      6     /*  number of bytes in message header  */
# define  NRETRIES     5     /*  maximum number of retries on message  */
                              /*  transmission                          */
# define  NDIALS        2     /*  number of times to retry dialing  */
# define  DIALTIME     20     /*  number of seconds between dial tries  */
# define  NFILENAM     64     /*  maximum length of current file name  */
# define  NREADBUF    512     /*  number of characters from input file to  */
                              /*  buffer                                   */
# define  TIMEOUT      20     /*  number of seconds to wait for a reply  */
                              /*  to a protocol message                  */
# define  MATCHTIM    120     /*  number of seconds to wait for a script  */
                              /*  receive sequence                        */
# define  PUSHBACK     64     /*  number of characters of lookahead on  */
                              /*  watching for strings.                 */
# define  MAXFIELDS     5     /*  maximum number of fields on script file  */
                              /*  line                                     */
# define  MAXLINE      81     /*  maximum number of characters on a line in  */
                              /*  script file (including newline)            */
# define  LSLAVNAM     80     /*  maximum length of slave startup string  */
# define  LRUNMESG      5     /*  length of RUN message  */




/*
 *     Signals
 */

# define  SIGHUP        1     /*  hangup signal  */
# define  SIGINTR       2     /*  interrupt signal  */
# define  SIGQUIT       3     /*  quit signal  */
# define  SIGIOT        4     /*  IOT instruction trap  */
# define  SIGALARM     14     /*  alarm clock  */




/*
 *      System and internal error codes
 */

# define  UEINTR        4     /*  interrupted system call  */
# define  UEBUSY       16     /*  something's busy  */
# define  UEDNPWR      80     /*  acu has no power  */
# define  UEDNABAN     81     /*  acu abandon call and retry  */
# define  UEDNDIG      82     /*  illegal digit in dialer  */

# define  ESYSINTR   -100     /*  interruted system call  */




/*
 *     Message types to be accepted by 'getmesg'
 */

# define  ACKONLY       1     /*  we are only looking for ACK messages  */
# define  NOTACK        2     /*  we don't want an ACK  */




/*
 *     Miscellaneous
 */

# define  SOH          01     /*  used as erase and kill character  */




/*
 *     format of data returned by 'gtty' and expected by 'stty'
 */

  struct  sgtty
    {
    char  s_ispeed;           /*  input speed index  */
    char  s_ospeed;           /*  output speed index  */
    char  s_erase;            /*  erase line character  */
    char  s_kill;             /*  kill character  */
    int  s_mode;              /*  word of mode bits  */
    };




/*
 *     format of io buffer for 'putc' and 'getc'
 */

  struct  iobuf
    {
    int  b_fildes;            /*  file descriptor  */
    int  b_nleft;             /*  chars left or unused  */
    char  *b_nextp;           /*  pointer to next character  */
    char  b_buff[512];        /*  the buffer  */
    };




/*
 *     Structure to get at the bytes of an integer
 */

  struct
    {
    char  lobyte;
    char  hibyte;
    };
