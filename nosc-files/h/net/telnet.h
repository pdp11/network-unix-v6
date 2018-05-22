/*
	This file contains the necessary defines for the telnet protocol
	note that the numbers are specified in decimal.
*/

#define TEL_IAC		255		/* Interpret as command: */
#define TEL_DONT	254		/* You are not to use option */
#define TEL_DO		253		/* Please, you use option */
#define TEL_WONT	252		/* I won't use option */
#define TEL_WILL	251		/* I will use option */
#define TEL_SB		250		/* Interpret as subnegotiation */
#define TEL_GA		249		/* You may reverse the line */
#define TEL_EL		248		/* Erase the current line */
#define TEL_EC		247		/* Erase the current character */
#define TEL_AYT		246		/* Are you there */
#define TEL_AO		245		/* abort output--but let prog finish */
#define TEL_IP		244		/* interrupt process--permanently */
#define TEL_BREAK	243		/* Break */
#define TEL_DM		242		/* Data mark--for connect. cleaning */
#define TEL_NOP		241		/* Nop */
#define TEL_SE		240		/* End sub negotiation */

/* Telnet Options */

#define TO_TR_BIN	0		/* Transmit binary */
#define TO_ECHO		1		/* Echo */
#define TO_RCP		2		/* Reconnect */
#define TO_SGA		3		/* Suppress go ahead */
#define TO_NAMS		4		/* Approximate message size */
#define	TO_STATUS	5		/* Status */
#define TO_TMARK	6		/* Timing mark */
#define TO_RCTE		7		/* RCTE */
#define TO_NAOL		8		/* Output line width */
#define TO_NAOP		9		/* Output size */
#define TO_NAOCRD	10		/* Output <cr> disposition */
#define TO_NAHTS	11		/* Horizontal tab stops */
#define TO_NAHTD	12		/* Horizontal tap disposition */
#define TO_NAOFFD	13		/* Output Formfeed Disposition */
#define TO_NAOVTS	14		/* Nego. about Vertical Tabstops */
#define TO_NAOVTD	15		/* Nego. about Output V. T. Disp. */
#define TO_NAOLFD	16		/* Nego. Output L.f. Disp. */
#define TO_EASCII	17		/* Extended ascii option */
#define TO_EXOPL	255		/* Extend Option list */
/* more to come */

/* telnet defines for the old telnet protocol */

#define	oTEL_DM		128		/* data mark - for cleaning the conn */
#define oTEL_BREAK	129		/* break */
#define oTEL_NOP	130		/* nop */
#define OTEL_NOECHO	131		/* user -> server, quit echoing */
#define oTEL_ECho	132		/* user -> server, start echoing */
#define oTEL_HIDE	133		/* server -> user, hide your input */
