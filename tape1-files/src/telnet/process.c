#include "stdio.h"
#include "tnio.h"
#include "globdefs.h"
#include "ttyctl.h"

/* -------------------------- P R O C E S S ------------------------- */
/*
 * Read from input buffer and put in command buffer cline (contained in
 * this routine) or output buffer via telwrite.
 * This routine identifies and executes the user's commands.
 * If the user types the escape char twice, that sends the escape char.
 * If the user types the escape char followed by a newline, the modes are
 * changed to normal. If the user types the escape char
 * followed by anything else, it is just executed without changing tty modes.
 */
process(buf, cnt, connp)
    char * buf;
    int cnt;
    NETCONN * connp;
{
    register char *p;	  /* Points into inbuf */
    static char outbuf[WNETSIZE];     /* Static just to keep stack size down */
    register char *op;	  /* Points into outbuf */
    int c;	      /* Unsigned char */
    int tflags;
    static TTYMODE *oldmode = NULL;
    static int cmdstate = 0;
#	define SENDING 0
#	define GOTESC  1
#	define INCMD   2

    static char cline[256];
    static char *clinep;

    static trunc;
    extern int escape;
    extern int udone;
    extern int termfd;
    extern int inputfd;
    extern int usercr;
    extern int needprompt;

    if (cnt == -1 || cnt == 0)
    {
	udone++;
	return(0);
    }

    op = outbuf;
    for (p = buf; cnt > 0; p++, cnt--)
    {
	int type;
#	    define ORDINARY 10
#	    define CMDBEGIN 20
#	    define CMDEND 30

	if (connp == NULL && cmdstate == SENDING)
	{
	    cmdstate = GOTESC;
	    needprompt = 1;
	}

	c = *p & 0377;

	if ((c&0177) == escape)
	    type = CMDBEGIN;
	else if ((c&0177) == '\n' || (c&0177) == '\r')
	    type = CMDEND;
	else
	    type = ORDINARY;

	switch(type + cmdstate)
	{
	case SENDING + ORDINARY:    /* Send it */
	case SENDING + CMDEND:	    /* Send it */
    xmit:
	    *op++ = c;
	    if (c == '\r' && usercr) {	/* Follow CR with LF */
		*op++ = '\n';
		tflags = GetFlags(CurMode());
		if (tflags&ECHO)	
			echo('\r');
	    }
	    cmdstate = SENDING;
	    break;
	case SENDING + CMDBEGIN:    /* Start of command? */
	    echo(c);
	    cmdstate = GOTESC;
	    break;
	case GOTESC + CMDBEGIN:     /* Doubled command char, send it */
	    if (connp != NULL)
		goto xmit;
	    break;
	case GOTESC + CMDEND:	    /* Line ended with escape, enter cmd mode */
	    echo(c);
	    if (oldmode == NULL)
		oldmode = ChgMode(OrigMode());
	    needprompt = 1;
	    break;
	case GOTESC + ORDINARY:     /* Start of command */
	    clinep = cline;
	    trunc = 0;
	case INCMD + ORDINARY:	    /* Collect command */
	case INCMD + CMDBEGIN:	    /* Collect command */
	    echo(c);
	    if (clinep < &cline[sizeof(cline)])
		*clinep++ = c;
	    else
		trunc = 1;  /* Remember we are losing chars */
	    cmdstate = INCMD;
	    break;
	case INCMD + CMDEND:
	    echo(c);
	    if (oldmode != NULL)
	    {
		ChgMode(oldmode);
		oldmode = NULL;
	    }
	    if (trunc)
		fprintf(stderr, "Command line too long -- ignored.\r\n");
	    else
	    {
/* Flush what we have in case command involves it */
		if (connp != 0 && op - outbuf > 0)
		    telwrite(connp, outbuf, op - outbuf);
		op = outbuf;
/* Ignore initial escape char */
		if (cline[0] == escape)
		    cline[0] = ' ';
/* Execute command */
		cline[clinep-cline] = '\0';
		callcmd(cline);
	    }
	    cmdstate = SENDING;
	    break;
	}
    }
    if (connp != 0 && op - outbuf > 0) 
	telwrite(connp, outbuf, op - outbuf);
    return(0);
}
