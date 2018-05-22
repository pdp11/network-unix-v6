  struct  rmenv
    {
    int  d_rcvseq,               /*  sequence number of the next message to  */
                                 /*  be received                             */
         d_transeq,              /*  sequence number of the next message to  */
                                 /*  be transmitted                          */
         d_lastseq,              /*  sequence number of the previous message  */
         d_tranint,              /*  set to indicate that an interrupt  */
                                 /*  should be sent to remote system    */
         d_linelgh,              /*  maximum number of characters in message  */
         d_window,               /*  maximum number of outstanding  */
                                 /*  unacknowledged messages        */
         d_logfd,                /*  file descriptor of log file  */
         d_portfd,               /*  file descriptor of comm line  */
         d_tranfd,               /*  file descriptor of transcript file  */
         d_ttylog,               /*  set to enable standard output logging  */
         d_tranon,               /*  set to enable transcribing  */
         d_debug;                /*  set to enable debugging logging  */

    char  d_errmesg[MAXMESG],    /*  buffer for incoming error messages  */
          d_intmesg[MAXMESG];    /*  buffer for outgoing interrupt messages  */
    };
