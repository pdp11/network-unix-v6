#define	HELLO		vdh1snd(V_SPBIT | V_HTOI)
#define	I_HEARD_YOU	vdh1snd(V_SPBIT | V_HELLO | V_HTOI)
#define	OACK		vdh1snd(vdhoack)	/* ack the imp		*/

/*
#define	NOCLOCK	1	/* defined if VDH is only clock in system 	*/


#define	VDHSIZE 64		/* Size of VDH buffer in WORDS		*/
#define VDHDSIZE	(128-2)	/* Maximum packet data bytes		*/
#define	NCHNS 2			/* Number of VDH channels		*/
#define	VDH_R ((125*HZ)/100)	/* VDH constant 'r'			*/
#define	VDH_T 4			/* VDH constant 't'			*/
#define	VDH_K 4			/* VDH constant 'k'			*/


int vdhibuf[2][VDHSIZE];	/* Input buffers (must be in first 32K) */
				/* for vdh models except VDHE		*/

int vdhobuf[NCHNS][VDHSIZE];	/* Output buffers			*/
char vdhouse[NCHNS];		/* Output channel-in-use flags		*/
int vdhichn;			/* Current input channel		*/
int vdhosnd;			/* Current output channel		*/
int vdhofill;			/* Next channel to be filled		*/

char helloinarow;	/* Number of hellos acked in a row		*/
char hellounack;	/* Number of unacknownledged HELLOs.  If	*/
			/* many happen, line will be declared down	*/

int hellonext;		/* Number of tics until next hello is sent	*/



char vdhlock;		/* Debugging: zero if at VDH interrupt		*/
int vdhoack;		/* Contains acknowledgement bits for IMP	*/
			/* to be sent with every normal packet		*/

int prvictl;		/* Copied from packet control word of input.	*/
			/* Primarily used to determine if previous 	*/
			/* output has been acked, but also used in	*/
			/* input interrupt to look at other packet	*/
			/* control word bits				*/

int vdherr;		/* Used to remember last VDH error		*/
int impnops;		/* Number of I-H NOPs to send before sending	*/
			/* any data					*/

int	needinit	;	/* temporary  ?  */
int	vstatebits;

/*	Defines on above	*/

#define	TRYING  	0000001	/* Will send and ack hellos		*/
#define	UP      	0000002	/* Will send and ack data		*/
#define	SAWHELLO	0000004	/* We saw a hello since reset		*/
#define	SENDHELLO	0000010	/* Send a hello at first opportunity	*/
#define	CLOCKINT 	0000020	/* For systems only having vdh clock	*/
#define	VDHILAST	0000040	/* Last buf awaiting processing by ncp	*/
#define	VDHINEED	0000100	/* Additional buffers req'd for input	*/
#define	OUTBUSY 	0000200	/* output side idle			*/
#define	ACK2SND 	0000400	/* Ack to send, even if no data to send	*/
#define	VDHIAB  	0001000	/* Which physical input channel in use	*/
#define	OUT2GO		0002000	/* Input considering restarting output	*/
#define	B0004000	0004000	/* Not Used				*/
#define	B0010000	0010000	/* Not Used				*/
#define	B0020000	0020000	/* Not Used				*/
#define	B0040000	0040000	/* Not Used				*/
#define	B0100000	0100000	/* Not Used				*/

struct vdhstatistics vs;	/* Statistics -- read vdhstat.h		*/


struct vdhregs
{
	int csri;		/* Command and Status Register In	*/
	int dbri;		/* Data Buffer In			*/
	int *cwai;		/* Current Word Address In		*/
	int wci;		/* Word Count In			*/
	int csro;		/* Command and Status Register Out	*/
	int dbro;		/* Data Buffer Out			*/
	int *cwao;		/* Current Word Address Out		*/
	int wco;		/* Word Count Out			*/
};

#ifndef	VDHE

#	define	VDH 		0167600	/* Address of VDH-11C			*/

	/* input side bits */

#	define	V_ANYERR	0100000	/* Some kind of VDH error		*/
#	define	V_CHKERR	0040000	/* Checksum error (input)		*/
#	define	V_OVERRUN	0020000	/* Overrun error (input & output)	*/
#	define	V_WCERR		0010000	/* Word count error (input & output)	*/
#	define	V_FORMERR	0004000	/* Packet format error (input)		*/
#	define	V_READY		0000200	/* VDH ready (input & output)		*/
#	define	V_INTENB	0000100	/* Interrupt enable (input & output)	*/
#	define	V_ABRUN		0000010	/* Buffer a/b running (input)		*/
#	define	V_ABSEL		0000004	/* Select buffer a/b [a=0] (input)	*/
#	define	V_EOT		0000002	/* Packet copied into core (input)	*/
#	define	V_GO		0000001	/* Start channel (input & output)	*/

	/* output side bits */

#	define	V_TIMSEL	0000040	/* Timer select (output)		*/
#	define	V_TIMRUN	0000020	/* Timer run, i.e. enable (output)	*/
#	define	V_EOTINT	0000010	/* EOT caused interrupt (output)	*/
#	define	V_TIMINT	0000004	/* Timer caused interrupt (output)	*/

#endif	not VDHE

#ifdef VDHE
#	define	VDH 0167620		/* Address of VDH-11E			*/

	/* input side bits */

#	define	V_ANYERR	0100000	/* Some kind of VDH error		*/
#	define	V_CHKERR	0040000	/* Checksum error (input)		*/
#	define	V_OVERRUN	0020000	/* Overrun error (input & output)	*/
#	define	V_WCERR		0010000	/* Word count error (input & output)	*/
#	define	V_FORMERR	0004000	/* Packet format error (input)		*/
#	define	V_READY		0000200	/* VDH ready (input & output)		*/
#	define	V_INTENB	0000100	/* Interrupt enable (input & output)	*/
#	define	V_ABRUN		0000010	/* Buffer a/b running (input)		*/
#	define	V_ABSEL		0000004	/* Select buffer a/b [a=0] (input)	*/
#	define	V_EOT		0000002	/* Packet copied into core (input)	*/
#	define	V_GO		0000001	/* Start channel (input & output)	*/

	/* output side bits */

#	define	V_TIMSEL	0000040	/* Timer select (output)		*/
#	define	V_TIMRUN	0000020	/* Timer run, i.e. enable (output)	*/
#	define	V_EOTINT	0000010	/* EOT caused interrupt (output)	*/
#	define	V_TIMINT	0000004	/* Timer caused interrupt (output)	*/

#endif	VDHE

	/* Defines on packet control word */

#define	V_HELLO		0100000	/* HELLO/I-HEARD-YOU bit (HELLO=0)	*/
#define	V_LAST		0100000	/* Last packet in message		*/
#define	V_EVNODD	0040000	/* Packet even/odd bit			*/
#define	V_WCBITS	0037400	/* Word count bits			*/
#define	V_WCBIT0	0000400	/* Word count bit 0			*/
#define	V_WCSHIFT	      8	/* amount to shift to get word count	*/
#define	V_HTOI		0000200	/* Host/IMP bit (to IMP = 1)		*/
#define	V_SPBIT		0000100	/* Special packet bit (i.e. HELLO)	*/
#define	V_C1ACK		0000010	/* Channel 1 acknowledge bit		*/
#define	V_C0ACK		0000004	/* Channel 0 acknowledge bit		*/
#define	V_ACKBITS	0000074	/* Acknowledge bits mask		*/
#define	V_CHBITS	0000003	/* Channel number mask			*/
#define	V_CHNUMBIT0	0000001	/* Low order bit of channel number	*/
