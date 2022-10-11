
#define IMPADDR 0172410

int  HMR_interval ;	/* 60ths of sec per strobing of host master ready */


struct impregs
{
	int owcnt;	/* output word count register */
	int spo;	/* output start ptr */
	int ostat;	/* output stat reg */
	int odata;	/* output data reg */

	int idummy[4];	/* on IMP11-As registers are not contig */

	int iwcnt;	/* input word count register */
	int spi;	/* start input ptr */
	int istat;	/* input status reg */
	int idata;	/* input data reg */
};




/* defines for accessing bits in the imp interface */

#define ierror		0100000	/* input error */
#define iendmsg		04000	/* full msg from imp */
#define impnotrdy	02000	/* imp not ready */
#define irdyerr		01000	/* imp ready error */
#define iienab		0100	/* input and output inter enable */
#define igo		011	/* make it go */
				/* includes write enable bit */
#define hmasrdy		04	/* host master ready */
#define irst		02	/* reset the input interface */


/* output csr */
#define oerror		0100000	/* output error flag */
#define oienab		0100	/* enable interrupts */
#define ordyclr		010	/* clear irdyerr */
#define	oendmsg		04	/* enable last host bit */
#define orst		02	/* clear transmit logic */
#define ogo		01	/* make it go */

int imp_trying;	/* when non-zero, irdyerr causes input messages to be
		 * ignored; set to 3 by imp_init and counted down to
		 * 0 by imp_iinit */

#ifdef BUFMOD

/* The following two buffers are needed in the calls to mapalloc made
 * in imp11a.c. Mapalloc has been changed so that it now allows multiple
 * processes to use different portions of the unibus map concurrently.
 * The mapfree routine that goes with this improved mapalloc requires
 * that it be called with exactly the same buffer pointer as the one
 * used in the mapalloc call in order to do function correctly.
 *              S.Y. Chiu : BBN 26Apr79
 */

struct buf i11a_obuf;

struct buf i11a_ibuf;

#endif BUFMOD
