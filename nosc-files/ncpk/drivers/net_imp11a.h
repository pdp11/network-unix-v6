
#define IMPADDR 0172410

int  HMR_interval ;	/* 60ths of sec per strobing of host master ready */


struct impregs
{
	int owcnt;	/* output word count register */
	int spo;	/* output start ptr */
	int ostat;	/* output stat reg */
	int omaint;	/* output maint reg */

	int idummy[4];	/* on IMP11-As registers are not contig */

	int iwcnt;	/* input word count register */
	int spi;	/* start input ptr */
	int istat;	/* input status reg */
	int imaint;	/* input maint reg */
};




/* defines for accessing bits in the imp interface */

#define iendmsg		04000	/* full msg from imp */
#define iienab		0100	/* input and output inter enable */
#define imasrdy		02000	/* imp master ready */
#define ixmem		060	/* extended memory address bits */
#define itimout		040000	/* timeout on output */
#define idone		0200	/* done bit on output */
#define igo		01	/* make it go */
#define iwrtenble	010	/* so we can recieve data (weird) */
#define irdyclr		010	/* clear ready error */
#define irst		02	/* reset the interface */
#define hmasrdy		04	/* host master ready */


/* output csr */
#define	oendmsg		04	/* enable last host bit */

