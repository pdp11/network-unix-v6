#define IMPADDR	0164000

int  HMR_interval ;	/* num 60ths of sec to wait before strobing host master ready */


struct impregs
{
	int spi;	/* start input ptr */
	int epi;	/* end input ptr */
	int istat;	/* input status reg */
	int imaint;	/* input maint reg */

	int spo;	/* output start ptr */
	int epo;	/* output end ptr */
	int ostat;	/* output stat reg */
	int omaint;	/* output maint reg */
};

/* defines for accessing bits in the imp interface */

#define	iserr   	0100000 /* imp error */
#define	istimeo 	0040000 /* input memory timeout */
#define	is020000	0020000 /* */
#define	isspeqep	0010000 /* start and end pointers equal */
#define	isbusy  	0004000 /* interface busy */
#define	is002000	0002000 /* */
#define	is001000	0001000 /* */
#define	isendmsg	0000400 /* end of message occured during this read */
#define	isbufful	0000200 /* input buffer is full */
#define	isintenb	0000100 /* input interrupt enable */
#define	is000040	0000040 /* */
#define	isnoswap	0000020 /* disable byte swapping */
#define	is000010	0000010 /* */
#define	is000004	0000004 /* */
#define	is000002	0000002 /* */
#define	is000001	0000001 /* */

#define	im100000	0100000 /* */
#define	im040000	0040000 /* */
#define	im020000	0020000 /* */
#define	im010000	0010000 /* */
#define	im004000	0004000 /* */
#define	imtstinc	0002000 /* test mode inc counters */
#define	imlock  	0001000 /* maint inhib dma inc, both chan */
#define	imloop  	0000400 /* loop back */
#define	imimprdy	0000200 /* imp master ready */
#define	im000100	0000100 /* */
#define	im000040	0000040 /* */
#define	im000020	0000020 /* */
#define	im000010	0000010 /* */
#define	im000004	0000004 /* */
#define	im000002	0000002 /* */
#define	imscosyn	0000001 /* scope sync bit */

#define	os100000	0100000 /* */
#define	ostimout	0040000 /* output memory timeout */
#define	os020000	0020000 /* */
#define	osspeqep	0010000 /* start end end poointers equal */
#define	osbusy  	0004000 /* output channel busy */
#define	os002000	0002000 /* */
#define	os001000	0001000 /* */
#define	os000400	0000400 /* */
#define	osdone  	0000200 /* output channel done */
#define	osintenb	0000100 /* output channel interrupt enable */
#define	os000040	0000040 /* */
#define	osnoswap	0000020 /* output channel byte swap disable */
#define	os000010	0000010 /* */
#define	os000004	0000004 /* */
#define	os000002	0000002 /* */
#define	osdiseom	0000001 /* disable setting end of message bit */

#define	om100000	0100000 /* */
#define	om040000	0040000 /* */
#define	om020000	0020000 /* */
#define	om010000	0010000 /* */
#define	om004000	0004000 /* */
#define	om002000	0002000 /* */
#define	om001000	0001000 /* */
#define	omreset 	0000400 /* reset interface */
#define	om000200	0000200 /* */
#define	om000100	0000100 /* */
#define	om000040	0000040 /* */
#define	om000020	0000020 /* */
#define	om000010	0000010 /* */
#define	om000004	0000004 /* */
#define	om000002	0000002 /* */
#define	omhstrdy	0000001 /* strobe host master ready */

