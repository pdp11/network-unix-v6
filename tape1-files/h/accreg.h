
/* for ACC LH-11 interface */

#define ACCDEV 23

struct impregs		/* restructured for ACC-LH-11 JSK */
{
	short istat;	/* input status and control */
	short ibuf;	/* DPC input buffer */
	unsigned short spi;	/* input address */
	short iwcnt;	/* 2's compl. of input word count */

	short ostat;	/* output status and control */
	short obuf;	/* DPC output buffer */
	unsigned short spo;	/* output address */
	short owcnt;	/* 2's compl. of output word count */
};


/* defines for accessing bits in the imp interface */
/* altered for ACC-LH-11 05-Jan-76 by JSK */

#define ACC_IBITS \
"\10\20CER\17NEX\16EOM\14HRY\13INRY\12MER\11IBF\10RDY\7IE\6XM1\5XM0\4WEN\3MRY\2RST\1GO"

#define icomperr	0100000	/* $ input composite error bit */
#define itimout 	0040000	/* $ input non existant memory bit */
#define iendmsg 	0020000	/*   input end of message recieved */
#define i0010000	0010000	/*   input not used */
#define ihstrdy 	0004000	/* $ input host ready */
#define imntrdy 	0002000	/* $ input IMP not ready */
#define imrdyerr	0001000	/* $ input Receive Master Ready error */
#define ibfrfull	0000400 /*   indic. a word is still in bfr reg*/
#define idevrdy		0000200	/* $ input Device Ready */
#define iienab  	0000100	/* $ input Interrupt enable */
#define i0000040	0000040	/*   input extend memory bit 17 */
#define i0000020	0000020	/*   input extend memory bit 16 */
#define iwren   	0000010	/* $ input Store enable */
#define ihmasrdy	0000004	/* $ input Host Relay Control */
#define ireset  	0000002	/*   reset input i/f */
#define igo     	0000001	/* $ input Go */ 

#define ACC_OBITS \
"\10\20CER\17NEX\16WC0\12MER\11OBE\10RDY\7IE\6XM1\5XM0\4BUS\3ENL\2RST\1GO"

#define ocomperr	0100000	/* $ output composite error bit */
#define otimout 	0040000	/* $ output Not Existant Memory */
#define o0020000	0020000	/*   output Word Count == 0 */
#define o0010000	0010000	/*   output Not used */
#define o0004000	0004000	/*   output Not used */
#define o0002000	0002000	/*   output Not used */
#define omrdyerr	0001000	/* $ output Transmit Master Ready Error */
#define o0000400	0000400	/*   output Not Used */
#define odevrdy		0000200	/* $ output Device Ready */
#define oienab  	0000100	/* $ output Interrupt Enable */
#define o0000040	0000040	/*   output Extend Memory 17 */
#define o0000020	0000020	/*   output Extend Memory 16 */
#define obusback	0000010	/* $ output Bus Back */
#define oenlbit 	0000004	/*   output Enable Last Bit */
#define oreset  	0000002	/*   reset output i/f */
#define ogo     	0000001	/* $ output Go */ 

/* --------- '$' (comment field) indicates bit is validity checked at beginning
**		of interrupt processing.  Required state varies from 1 bit to
**		another.
*/

