/*
 *     Message code characters
 */

# define  ACK         '0'     /*  acknowledge data  */
# define  DATA        '1'     /*  data transfer  */
# define  EOT         '2'     /*  end of data  */
# define  RUN         '3'     /*  start up protocol with given line length  */
# define  EXIT        '4'     /*  one end of link is exitting  */
# define  ERROR       '5'     /*  an error occurred on the last command  */
# define  DONE        'E'     /*  current function completed  */
# define  FINISHED    'G'     /*  all transfers complete - exit  */
# define  RECVFILE    'H'     /*  copy data from line to named file  */
# define  SENDFILE    'J'     /*  send named file  */
# define  FUNCACC     'L'     /*  function accepted  */




# define  OK                 0     /*  routine did its thing successfully  */
# define  ESYSTEM           -1     /*  system error  */
# define  EUSER             -2     /*  user error  */
# define  EINTRUPT          -3     /*  interrupt received  */
# define  ETIMEOUT          -4     /*  time limit exceeded  */
# define  EPROTO            -5     /*  protocol error  */
# define  ENONFATAL         -6     /*  non-fatal error.  */
# define  ETRANS            -7     /*  transmission error  */
# define  EEXIT             -8     /*  EXIT message received  */
