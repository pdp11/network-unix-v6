#/*
Module Name:
 
    N     N  EEEEEEE  TTTTTTT   GGGGG   RRRRRR   FFFFFFF
    NN    N  E           T     G     G  R     R  F
    N N   N  E           T     G     G  R     R  F
    N  N  N  EEEEEE      T     G        RRRRR    FFFFFF
    N   N N  E           T     G   GGG  R    R   F
    N    NN  E           T     G     G  R     R  F
    N     N  EEEEEEE     T      GGGGG   R     R  F      
	   
 
	Network Graphics Server for UNIX

Installation:
	if $1x = finalx goto final
	cc -c svrtel.c
	if ! -r svrtel.o exit
	cc svrtel.o /usr/net/hnumcvt.c -lj
	rm -f svrtel.o hnumcvt.o
	exit
: final
	cc -O -c svrtel.c
	if ! -r svrtel.o exit
	cc -O -n -s svrtel.o /usr/net/hnumcvt.c -lj
	if ! -r a.out exit
	if ! -r ncpp/tel-s/svrtel.c goto same
	if { cmp -s svrtel.c ncpp/tel-s/svrtel.c } goto same
	su cp svrtel.c ncpp/tel-s/svrtel.c
	rm -f svrtel.c
: same
	if -r /usr/net/etc/svrtel su rm -f /usr/net/etc/svrtel
	su cp a.out /usr/net/etc/svrtel
	e - ncpp/tel-s/svrtel.c
1i
#define OLDTLNT
. 
w /tmp/svrtel.c
q
	cc -O -s -n /tmp/svrtel.c hnumcvt.o -lj
	if -r /usr/net/etc/srvrtelnet su rm -f /usr/net/etc/srvrtelnet
	su cp a.out /usr/net/etc/srvrtelnet
	rm -f a.out svrtel.o hnumcvt.o /tmp/svrtel.c
	chdir /usr/net/etc
	su chmod 744 svrtel srvrtelnet
	su chown root svrtel srvrtelnet
	su chgrp system svrtel srvrtelnet

Synopsis:

Function:

Restrictions:

Diagnostics:

Files:

See Also:

Bugs:

Globals contained:

Routines contained:

Modules referenced:

Modules referencing:

Compile time parameters and effects:

Module History:
*/
/* Parameters */
#define SIGINR  15      /* Interrupt-by-sender (INS) */
#define NETBUFSIZE 60	/* Number of characters in network buffer */
char ptys[] "ABCEDFGH";	/* Suffix characters of psuedo-teletypes */
 
/* TELNET Opcodes */
#define SE 0360		/* Subnegotiation_end */
#define NOP 0361	/* Nop */
#define DATAM 0362	/* Data_mark */
#define BREAK 0363	/* Break */
#define IP 0364		/* Interrupt_Process */
#define AO 0365		/* Abort_Output */
#define AYT 0366	/* Are_You_There */
#define EC 0367		/* Erase_Character */
#define EL 0370		/* Erase_Line */
#define GA 0371		/* Go_Ahead */
#define SB 0372		/* Subnegotiation_Begin */
#define WILL 0373	/* Will */
#define WONT 0374	/* Wont */
#define DO 0375		/* Do */
#define DONT 0376	/* Dont */
#define IAC 0377	/* Interpet_As_Command */

#ifdef OLDTLNT
/* Old TELNET Opcodes */
#define ODATAM 128      /* Data_mark */
#define OBREAK 129      /* Break */
#define ONOP   130      /* Nop */
#define OIECHO 131      /* I echo */
#define OUECHO 132      /* You echo */
#define OHYI   133      /* Hide your input */
#endif

/*  TELNET Options
TELOPT($BINO,0,Transmit_binary);
TELOPT($ECHO,1,Echo);
TELOPT($RCP,2,Prepare_to_reconnect);
TELOPT($SGA,3,Suppress_go_ahead);
TELOPT($NAMS,4,Negotiate_approximate_message_size);
TELOPT($STATUS,5,Status_of_TELNET_options);
TELOPT($TIMRK,6,Timing_Mark);
TELOPT($RCTE,7,Remote_Controled_Transmission_and_Echoing);
TELOPT($NAOL,10,Negotiate_About_Output_Line_width);
TELOPT($NAOP,11,Negotiate_About_Output_Page_Size);
TELOPT($EXTASC,17,Extended_ASCII);
TELOPT($EXOPL,377,Extended_Options_List);
*/
 
/*
Global variables			*/
 
int pid;		/* Process ID of subsidiary fork */
int netchan;		/* Channel descriptor for network */
			/* channel used for TELNET data */
int ptychan;		/* Channel number of PTY */
char echoflag;		/* Indicates 'will echo' has been sent */
int junk;		/* Somewhere to throw away things */
int pty;                /* Pty name */
char *arg1p;            /* Pointer to arg[1] */
int inter;              /* Indication that an interrupt came in */
int ins();              /* INS interrupt routine */
 
struct openparams	/* struct for parameterized network opens */
{
  char o_op;		/* opcode for kernel & daemon - unused here */
  char o_type;		/* Type for connection.  See defines below */
  int o_id;		/* ID of file for kernel & daemon, unused here */
  int o_lskt;		/* Local socket number either abs or rel */
  int o_fskt[2];	/* Foreign socket either abs or rel */
  char o_frnhost;	/* Foreign host number */
  char o_bsize;		/* bytesize for the connection */
  int o_nomall;		/* nominal allocation for the connection */
  int o_timeo;		/* Number of seconds before time out */
  int o_relid;		/* fid of file to base a data connection on */
} openparams;
 
#define o_direct        01      /* icp |~  direct */
#define o_server        02      /* user | server */
#define o_init          04      /* listen | init */
#define o_specific      010     /* general  | specific */
#define o_duplex        020     /* simplex | duplex */
#define o_relative      040     /* absolute | relative */
 
struct ttymode
{ char ispeed, ospeed;
  char erase, kill;
  int mode;
};

/*
main   - Handles new connections		*/
 
int foochan;		/* Due to crock in close */
main(argcnt,argvec)
int argcnt;
char *argvec[];
{
	log("Awaiting connection\n");
	signal(1,1);			/* Ignore quits */
	signal(2,1);			/* Ignore hangups */
	signal(SIGINR,1);             /* Ignore ins interrupts */
	if(argcnt > 1) arg1p = argvec[1];
	for (;;) {
		openparams.o_type = o_server;	/* Server ICP */
		openparams.o_fskt[0] = 0;	/* Clear any crud */
		openparams.o_fskt[1] = 0;
#ifndef OLDTLNT
		openparams.o_lskt = 23;	 /* Use socket 23 */
#endif
#ifdef OLDTLNT
		openparams.o_lskt = 1;	 /* Use socket 1 */
#endif
		netchan = open("/dev/net/anyhost",&openparams);
		if (netchan < 0) {
			log("Open failure.\n");
			continue;
		}
		foochan = dup(netchan);	/* Bagbiting close!!! */
		if ((pid = fork())<0) {
			log("No processes left!\n"); }
		else if (pid==0) {
			netgrf();
			exit(0);
		}
		else {		/* More logger */
			wait(&junk);	/* Wait for server to become orphan */
			close(foochan);	/* Flush extra channel */
			close(netchan);	/* And real channel, then wait for */
		}			/* another user */
	}
}
 
/*
netgrf - Sets up processes for NGP server	*/
 
netgrf()
{
  /* Fork another level deep so parent can die and leave the server as */
  /* an orphan to be inherited by /etc/init.  Otherwise, zombies are */
  /* left lying around because the logger (i.e. main) cannot afford to */
  /* do a wait. */
	switch(pid=fork()) {
	case -1:
		log("No processes left!\n");
		write(netchan,"Sorry, not enough resources available.\r\n",41);
		exit(1);	/* Failure return */
	default:
		exit(0);	/* Continues logger */
	case 0:		/* child continues on.... */
		break;
	}
	if (openpty()<0) {
		log("No ptys\n");
		write(netchan,"Sorry, no ptys\r\n",16);
		exit(1);	/* Failure return */
	}
	setss();
	if ((pid = fork()) < 0) {
		log("No processes left!\n");
		write(netchan,"Sorry, no processes\r\n",21);
		exit(1);	/* Failure return */
	}
	close(foochan);	/* Two process have it open now (bagbitting close!) */
	if (pid==0)
		telxmt();	/* Child handles transmit side */
	else
		telrcv();	/* Parent handles recieve side */
}
 
/*
telrcv - TELNET recieve side			*/
 
telrcv()
{
	int c;

/*	signal(SIGINR,&ins);  ***/
	sleep(2);		/* Give getty a chance to do stty */
	for(;;) {
		c = telgetc();		/* Fetch a character */
		if (c==IAC)		/* IAC? */
			telcmd();	/* Yes, handle special command */
#ifdef OLDTLNT
		else if (c >=128 )
			otelcmd(c);
#endif
		else {
			write(ptychan,&c,1);	/* Otherwise, just enter it */
			if (c=='\r') {		/* Special case? */
				c = telgetc();	/* Yes, look for LF */
				if (c && c!='\n') /* If different, enter */
					write(ptychan,&c,1);	/* it too! */
			}
		}
	}
}
 
#ifdef OLDTLNT
/*
otelcmd - Handle old TELNET commands */

otelcmd(c)
int c;
{
	struct ttymode status;		/* Buffer for stty and gtty */

	switch(c)  {
	case OBREAK:
		gtty(ptychan,&status);	/* return NUL in raw mode, else DEL */
		write(ptychan,(status.mode&040) ? "\0" : "\177",1);
		break;
	case OIECHO:
	case OUECHO:
		gtty(ptychan,&status);
		status.ispeed = status.ospeed = ( (c == OIECHO) ? 4 : 11 );
		stty(ptychan,&status);
		break;
	 }
}
#endif

/*
telcmd - Handle new TELNET commands */
 
telcmd()
{
	char negbuf[3];
	int command;
	struct ttymode status;		/* Buffer for stty and gtty */

	command = telgetc();		/* Get a character */
	switch(command)  {
	case BREAK:
	case IP:
		gtty(ptychan,&status);	/* output NUL in raw mode, else DEL */
		write(ptychan,(status.mode&040) ? "\0" : "\177",1);
		break;
	case AYT:
		write(netchan,"\007",1);
		break;
	case EC:
	case EL:
		gtty(ptychan,&status);
		write(ptychan,(command == EC) ? &status.erase : &status.kill, 1);
		break;
	case SB:	/* Begin subnegotiation? */
		/* Look for end of subnegotiation */
		do {
			while(command != IAC)
				command = telgetc();
		} while telgetc() != SE;
		break;
	case WILL:
	case WONT:
	case DO:
	case DONT:
		/* Negotiate.	Always give negative acknowledgement */
		negbuf[0] = IAC;		/* Interpete As Command */
		negbuf[1] = DONT+WONT-((command+1)&0376);   /* Negative ack. */
		negbuf[2] = telgetc();	/* Set code for option */
		if (negbuf[2]==1 &&		/* Echo option? */
		   (negbuf[1]&0377) == WONT) {
			gtty(ptychan,&status);
			if (command == DO) {	/* User wants us to echo */
				status.ispeed = status.ospeed = 11; /* 2400 baud echos */
				negbuf[1] = WILL;
			} else {	/* User doesn't want us to echo */
				status.ispeed = status.ospeed = 4;	/* 134.5 baud doesn't echo */
				negbuf[1] = WONT;
			}
			stty(ptychan,&status);	/* Set desired echo mode */
			if (echoflag == 0) {	/* Was this a response to our WILL ECHO? */
				echoflag++;	/* If so don't do anything */
				break;
			}
		}
		write(netchan,negbuf,3);	/* Send off confirmation/denial to net */
		break;
	case IAC:		/* Not really a command */
		write(ptychan,&command,1);	/* but a character */
		break;
	}
}
/*
telxmt - TELNET transmit side			*/
 
char ttybuf[NETBUFSIZE+1];	/* This size is best for NetUNIX */
 
telxmt()
{
	register int bytcnt;
	register char *s1,*s2;
	int retries;
	struct ttymode status;

	/* Send 'WILL ECHO' and a reminder */
#ifdef OLDTLNT
	write(netchan,"\203\r\n",3);
#endif
#ifndef OLDTLNT
	write(netchan,"\377\373\001\r\n",5);
#endif
	log("connection on tty%c", pty);
	if(arg1p) printf(" from %s", &arg1p[10]);
	printf(".\n");  flush();

	/* Copy characters from output buffer to network */
	retries = 10;
	for(;;) switch(bytcnt = read(ptychan,ttybuf,sizeof ttybuf)) {
	case -1:	/* error returned; time to quit! */
		exit(0);
	case 0:		/* EOF -- other side closed */
		gtty(ptychan, &status);
		if(status.mode&1 || --retries == 0)
			exit(0);
		sleep(1);	/* give a chance to re-open */
		continue;
	default:
		retries = 5;
		for (s1 = s2 = ttybuf, ttybuf[bytcnt] = 0; *s1; s1++)
			if (*s1 == IAC) {
				write(netchan,s2,s1-s2+1);
				s2 = s1;
			}
		write(netchan,s2,s1-s2);
	}
}
/*
openpty- Find a usable PTY			*/
 
char *ptyname "/dev/ptyx";	/* Where to look for pseudo-teletypes */
 
int openpty()	/* Find an available pseudo-teletype */
{
	register char *p;

	for (p = ptys; ptyname[8] = *p++;) {
		ptychan = open(ptyname,2);	/* Try this PTY */
		if (ptychan>=0) {
			pty = ptyname[8]&0177;
			return(ptychan);
		}
	}
	return -1;
}

/*setss - Set arguments to appear in ss */

char argstr[] "ttyX host ";
 
setss()
{       register char *p, *q;
	struct openparams statparams;

	if ((q=arg1p) == 0) return;
	argstr[3] = pty;
	p = argstr;
	while (*p) { if (*q) *q++ = *p++; else return; }
	if (fstat(netchan,&statparams) >= 0) {
		p = hnumcnv(statparams.o_frnhost&0377);
		while (*p) { if (*q) *q++ = *p++; else return; }
	}
	*q = '\0';
}
/*
telgetc- Get a character from TELNET stream 	*/
 
telgetc()
{
	char c;
	register int i;
	int ttytype[3];

	i = read(netchan,&c,1);
	if (inter) {		/* check for interrupts */
		inter = 0;	/* DM will be ignored */
		gtty(ptychan,ttytype);	/* return NUL in raw mode, else DEL */
		return( (ttytype[2]&040) ? 0 : 0177);
	}
	if (i != 1) {
/*DEBUG*/if(pid==0)log("PID not set!  This can't happen!\n");else
		kill(pid,9);		/* Kill other side */
		exit(1);
	} else {
		return(c&0377);
	}
}
 

ins()
{       inter++;                /* note that an interrupt has occurred */
	signal(SIGINR,&ins);
}

log(txt, a, b, c, d)
char *txt;
{
	long t;			/* gets the result from time */

	time(&t);		/* get system time */
#ifndef OLDTLNT
	printf ("%-16.16sNew Telnet server: ",
#endif
#ifdef OLDTLNT
	printf ("%-16.16sOld Telnet server: ",
#endif
		ctime(&t)+4);	/* Mmm dd hh:mm:ss */
	printf(txt, a, b, c, d);
	flush();
}
