/*
 * TcpErr(errno) returns a pointer to the message string for the specified
 * TCP error.
 */

/* The error table */

char *TcpList[]
{
    /* No error */          "No TCP error.",
    /* EEARPRT */           "Open of TCP ear port failed.",
    /* ERSPPRT */           "Can't create tcp command port.",
    /* ECMDPRT */           "Can't open user command port.",
    /* ETCPNRDY */          "TCP not responding.",
    /* ETCPNOPN */          "TCP cannot open tcp port.",
    /* ESNDPRT */           "Error occurred on send data port.",
    /* ERCVPRT */           "Error occurred on receive data port.",
    /* EUSRC */             "Too many connections open in this process.",
    /* ENOSPC */            "Not enough space in send port.",
    /* ENODATA */           "No data in receive port.",
    /* EBADRESP */          "Error on TCP command port.",
    /* ENOCHNG */           "No change notices.",
    /* ESYSC */             "Too many connections system wide.",
    /* ENOBUFS */           "Tcp or library is out of buffer space.",
    /* EILLS */             "Illegal local port specification.",
    /* EUNAUTH */           "Unauthorized parameter in open.",
    /* EUNSPEC */           "Unspecified parameter in CAB.",
    /* ENOCONN */           "No such connection.",
    /* EBADPARAM */         "Bad parameter in TCP library call.",
    /* EMVSPT */            "Bad SPT in accepting TCB.",
    /* EMVSTATE */          "Connection to move is not established.",
    /* ECMDCLS  */          "TCP closed command port.",
};

/* The routine to read it */

char *
TcpErr(eno)
    int eno;
{
    extern char *locv();

    if (eno < 0)
        return("Unknown error");
    if (eno > (sizeof(TcpList)/sizeof(TcpList[0]))-1)
        return(locv(0, eno));    /* Return the number itself */
    return(TcpList[eno]);
}
