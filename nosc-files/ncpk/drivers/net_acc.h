/* for ACC LH-11 interface */

#define IMPADDR	0167600		/* JSK */

struct impregs		/* restructured for ACC-LH-11 JSK */
{
	int istat;	/* input status and control */
	int ibuf;	/* DPC input buffer */
	int spi;	/* input address */
	int iwcnt;	/* 2's compl. of input word count */

	int ostat;	/* output status and control */
	int obuf;	/* DPC output buffer */
	int spo;	/* output address */
	int owcnt;	/* 2's compl. of output word count */
};


/* defines for accessing bits in the imp interface */
/* altered for ACC-LH-11 05-Jan-76 by JSK */

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
#define i0000002	0000002	/*   input Not used */
#define igo     	0000001	/* $ input Go */

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
#define o0000002	0000002	/*   output Not Used */
#define ogo     	0000001	/* $ output Go */
/* --------- '$' (comment field) indicates bit is validity checked at beginning
**		of interrupt processing.  Required state varies from 1 bit to
**		another.
*/

