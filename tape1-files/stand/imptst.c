#define BBNNET

#include "../h/param.h"
#include "../bbnnet/net.h"
#include "../h/inode.h"
#include "../bbnnet/imp.h"
#include "../h/upreg.h"
#include "../h/pte.h"
#include "../h/ubareg.h"
#include "../h/accreg.h"
#include "saio.h"
#include "savax.h"


#define min(a,b) (a<b ? a : b)
#define BUFSIZ 512

char input[132];        /* input buffer */
struct imp imps;         /* imp leader buffer */
char inbuf[BUFSIZ];      /* imp input buffer */
int writeflg = 0;       



main()
{
	register error, i, len;
	short type, host, impno, link;
	n_short htons();
	unsigned short ntohs();

	register struct impregs *impaddr =
	    (struct impregs *)ubamem(0, 0767700);

	error = 0;

	/* get user parameters */

	printf("IMP interface diagnostic\nread or write test(r or w)? ");
	gets(input);
	while (input[0] != 'r' && input[0] != 'w') {
		printf("reply r or w: ");
		gets(input);
	}

	if (input[0] == 'w') {
		writeflg++;
		printf("enter destination host number: ");
		gets(input);
		while ((host = (short)a2l(input)) < 0 || host > 255) {
			printf("host number out of range -- re-enter: ");
			gets(input);
		}
		printf("imp number: ");
		gets(input);
		while ((impno = (short)a2l(input)) < 0 || impno > 32767) {
			printf("imp number out of range -- re-enter: ");
			gets(input);
		}
		printf("link number: ");
		gets(input);
		while ((link = (short)a2l(input)) < 0 || link > 255) {
			printf("link number out of range -- re-enter: ");
			gets(input);
		}
	}

	/* initialize imp i/f */

	printf("initialization starting...\n");
	imp_init();

	/* send 3 noops and init imp leader buffer */

	snd_noops(&imps);
	printf("initialization complete\n");

	/* write test */

	if (writeflg) {
		printf("starting write test...\n");
		imps.i_hst = host;
		imps.i_impno = htons((short)impno);
		imps.i_link = link;

		/* write leaders while no errors */

		while (error == 0)
			error = imp_write(&imps, sizeof(struct imp));
        	printf("IMP write error, Csro=%o\n", (short)error);

	/* read test */

	} else {
		printf("starting read test...\n");
		while (error == 0) {

			/* read a leader */

			error = imp_read(&imps, sizeof(struct imp));
			type = imps.i_type;
			impno = ntohs(imps.i_impno);
			host = imps.i_hst;
			link = imps.i_link;
			len = ntohs(imps.i_mlen);
			printf("IMP read: type=%d link=%o host=%o imp=%o\n",
				type, link, host, impno);

			/* read any data */

			while ((error&iendmsg)==0 && (error&~iendmsg)==0 &&
				len > 0) {

				i = min(len, BUFSIZ);
				error = imp_read(inbuf, i);
				len -= i;
			}

			error &= ~iendmsg;
			if (error==0 && (len > 0 || impaddr->iwcnt != 0))
				printf("IMP input length mismatch\n");

		}
		printf("IMP read error, Csri=%o\n", (short)error);
	}

	printf("...imptest exiting\n");
}

snd_noops(ip)           /* send 3 noops */
register struct imp *ip;
{
	register i, error;

	ip->i_nff = 0xf;
	ip->i_dnet = 0;
	ip->i_lflgs = 0;
	ip->i_type = 4;
	ip->i_htype = 0;
	ip->i_hst = 0;
	ip->i_impno = 0;
	ip->i_stype = 0;
	ip->i_mlen = 0;

	for (i=0; i < 3; i++) {
        	ip->i_link = i;
		if ((error = imp_write(ip, sizeof(struct imp))) != 0) {
			printf("IMP init error, Csro=%o\n", (short)error);
			_stop();
		}
	}
}

unsigned short ntohs(ls)
n_short ls;
  { 	union sshuffle
	  { unsigned short x;
	    char a[2];
	  } local, remote;

	register int i;

	remote.x = ls;
	for (i=0; i<2; i++)
     	     local.a[i] = remote.a[1-i];

	return(local.x);
}

n_short htons(ls)
unsigned short ls;
  { return(ntohs((n_short)ls));
  }

imp_write(buf, len)
register struct imp *buf;
register len;
{
	register uba, error;
	struct iob io;

	register struct impregs *impaddr =
	    (struct impregs *)ubamem(0, 0767600);

	/* set up uba mapping */

	io.i_ma = (char *)buf;
	io.i_cc = len;
	uba = ubasetup(&io, 0);

	/* set IMP device regs and start i/o */

	impaddr->spo = (int)uba;
	impaddr->owcnt = -((io.i_cc + 1) >> 1);
	impaddr->ostat = ((short) ((uba & 0x30000) >> 12) + oenlbit + ogo);

	/* wait for completion */

	while ((impaddr->ostat & odevrdy) == 0);

	/* check if error */

	error = impaddr->ostat & (ocomperr|otimout|omrdyerr|obusback|ogo);

	ubafree(uba);

	return(error);
}

imp_read(buf, len)
register struct imp *buf;
register len;
{
	register uba, error;
	struct iob io;

	register struct impregs *impaddr =
	    (struct impregs *)ubamem(0, 0767600);


	/* set up uba mapping */

	io.i_ma = (char *)buf;
	io.i_cc = len;
	uba = ubasetup(&io, 0);

	/* set up IMP device regs and start i/o */

	impaddr->spi = (int)uba;
	impaddr->iwcnt = -(io.i_cc >> 1);
	impaddr->istat = (ihmasrdy | iwren | ((uba & 0x30000)>>12) | igo);

	/* wait for completion */

	while ((impaddr->istat & idevrdy) == 0);

	/* check if error */

	error = ( impaddr->istat & (iendmsg|icomperr|itimout|imntrdy|imrdyerr|igo)) |
		(~impaddr->istat & (ihstrdy|iwren|ihmasrdy));

	ubafree(uba);
	return(error);
}

imp_init()                              /* init the imp interface */
{	
	
	/* reset the imp interface */


	register struct impregs *impaddr =
	    (struct impregs *)ubamem(0, 0767600);

	impaddr->istat = 2;
        impaddr->ostat = 2;    /* WAS 2??? */
	impaddr->ostat = obusback;      /* reset host master ready */
	impaddr->ostat = 0;

	impaddr->istat = ihmasrdy; /* close relay */
	while (!(impaddr->istat&ihstrdy) || (impaddr->istat&(imrdyerr|imntrdy)))
	      {
		impaddr->istat = ihmasrdy;      /* keep turning imrdyerr off*/
		DELAY(200);
	      }
	DELAY(200);
}


a2l(as)
register char *as ;
{
/*
*  Convert null-terminated ascii string to binary
*  and return value.
*  1st char in string :
*	0 -> octal
*	x -> hex
*	else decimal
*/
register value , base , sign , digit ;
 
digit = value = sign = 0 ;
base = 10 ;  /* default base */
 
aloop :
if ((digit = (*as++)) == 0) return(value) ; /* null */
 
if (digit == '-') {
	sign++ ;
	goto aloop ;
	}
 
if (digit == '0') base = 8 ;  /* octal base  */
else { if (digit == 'x') base = 16 ;  /* hex base */
	else value = (digit-060) ; /* 060 = '0' */
	}
 
while (digit = (*as++)) {
	if (digit < '0') return(0) ;
	switch (base) {
		case 8 : {
			if (digit > '7') return(0) ;
			digit -= 060 ;
			break ;
			}
		case 10 : {
			if (digit > '9') return(0) ;
			digit -= 060 ;
			break ;
			}
		case 16 : {
			if (digit <= '9') {
				digit -= 060 ;
				break ;
				}
			if ((digit >= 'A') && (digit <= 'F')) {
				digit = (digit - 0101 + 10) ;
					break ;
				}
			if ((digit >= 'a') && (digit <= 'f')) {
				digit = digit - 0141 + 10 ;
				break ;
				}
			return(0) ;
			break ;
			}
		}
	value = (value * base) + digit ;
	}
return (sign ? -value : value) ;
}
