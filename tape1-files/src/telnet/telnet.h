/*
 * Definitions for the TELNET Protocol.
 * You should #define PLATFORM in your program if you want
 * the PLATFORM version (which leaves out some things).
 */

#define TELNET true
#define SERVER_SOCKET 23    /* Normal server socket */

#define NETGROW 2   /* Max # network bytes 1 user byte may expand to. */

#define IAC   255             /* Interpret as command: */
#define DONT  254             /* You are not to use option */
#define DO    253             /* Please, you use option */
#define WONT  252             /* I won't use option */
#define WILL  251             /* I will use option */
#define SB    250             /* Interpret as subnegotiation */
#define GA    249             /* You may reverse the line */
#define EL    248             /* Erase the current line */
#define EC    247             /* Erase the current character */
#define AYT   246             /* Are you there */
#define AO    245             /* abort output--but let prog finish */
#define IP    244             /* interrupt process--permanently */
#define BREAK 243             /* Break */
#define DM    242             /* Data mark--for connect. cleaning */
#define NOP   241             /* Nop */
#define SE    240             /* End sub negotiation */

#define SYNCH 242	    /* For telfunc calls */

/* Telnet Options */

#define OPT_BINARY       0               /* Transmit binary */
#define OPT_ECHO         1               /* Echo */
#define OPT_RECONNECT	 2		 /* Reconnect */
#ifndef PLATFORM
#define OPT_SUPPRESS_GA  3               /* Suppress go ahead */
#endif
#define OPT_RECSIZE  4			 /* Approximate message size */
#define OPT_STATUS       5               /* Status */
#define OPT_TMARK        6               /* Timing mark */
#define OPT_RCTE         7               /* RCTE */
#define OPT_LINEWIDTH    8               /* Output line width */
#define OPT_PAGESIZE     9               /* Output size */
#ifndef PLATFORM
#define OPT_CR_DISP      10              /* Output <cr> disposition */
#define OPT_HT_SET       11              /* Horizontal tab stops */
#define OPT_HT_DISP      12              /* Horizontal tap disposition */
#define OPT_FF_DISP      13              /* Output Formfeed Disposition */
#define OPT_VT_SET       14              /* Nego. about Vertical Tabstops */
#define OPT_VT_DISP      15              /* Nego. about Output V. T. Disp. */
#define OPT_LF_DISP      16              /* Nego. Output L.f. Disp. */
#define OPT_XASCII       17              /* Extended ascii option */
#endif
#define OPT_VTERM	 32		 /* Special VTSS option */
#   define SB_VTSWITCH	 1
#   define SB_MODES	 2
struct vterm_sb
{
    char sb_num;    /* SB_VTSWITCH or SB_MODES */
    union
    {
	char sb_vtno;
	int sb_flags;
    } sb_data;
};

#define OPT_EXOPL	 255		 /* Extend Option list */

/* For formatting options */

#define DATA_RCVR    0
#define DATA_SNDR    1

/* For reconnection option */

#define RC_START     0
#define RC_WAIT      1

#define SHELL_0 "net-sh"
