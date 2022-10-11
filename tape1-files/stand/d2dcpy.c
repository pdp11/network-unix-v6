# define RP6CYL  815  /*  no. RP06 cylinders/pack */
# define RP6TRK  19  /*  no. tracks/cyl  */
# define RP6SEC  22  /*  no. sectors/track  */
# define RP6ST  (rptrk*rpsec)  /*  no. sectors/cyl  */
# define MAXSEC  (rpcyl*rptrk*rpsec)  /*  sectors/pack  */
# define M0  0x20010000  /* phys addr MBA 0  */
# define M_st  2  /*  offset for MBA status reg */
# define M0_cr  (M0+4)  /*  MBA 0 control reg addr */
# define M0_map  (M0+0x800)  /* start MBA 0 map reg's */
# define M0_var  (M0+0xc)  /* MBA 0 virt addr reg */
# define M0_bc  (M0+0x10)  /*  MBA 0 byte count reg */
# define MBAinit 01  /*  MBA init bit */
# define M_DTC  0x2000  /* Data Transfer Complete */
/*		*/
# define RP  (M0+0x400)  /*  base for RP06 reg's, drive 0  */
/*		*/
# define RP_cr  0  /* RP06 control reg offset, longword */
# define RP_sr  1  /*  RP06 status reg offset */
# define RP_er1  2  /*  RP06 error reg 1 */
# define RP_stk  5  /*  RP06 sector/track reg offset */
# define RP_typ  06  /*  disk type reg */
# define RP_off  011  /*  RP offset reg */
# define RP_cyl  10  /*  RP06 cylinder reg offset */
# define RP_Epos 016  /*  RP ECC position reg */
# define RP_Epat 017  /*  RP ECC pattern reg */
/*		*/
# define RP_GO  1  /*  go bit */
# define RP_RD	070	/* RP06 read function code */
# define RP_WR  060  /*  RP06 write function code */
# define RP_DC  010  /*  drive clear function code */
# define RP_FMT  0x1000  /*  format bit in RP offset reg */
# define RP_RIP  020  /*  Read-in Preset function code */
# define RP_MOL  0x1000  /*  medium online bit in status */
# define RP_DRY  0200  /*  drive ready, status reg */
# define RP_ERR 040000  /* composite error, status reg */
# define RP_DCK  0x8000  /*  Data Check Error in ERR1 reg */
# define RP_ECH  0x40  /*  ECC hard error */
/*		*/
# define RXCS	32  /*  receiver control/staus */
# define RXDB  33  /*  receiver data */
# define TXCS  34  /*  transmitter control/status */
# define TXDB  35  /*  transmitter data */
# define RXCS_DONE  0x80  /*  receiver done */
# define TXCS_RDY  0x80  /*  transmitter ready */
/*		*/
# define BLKSIZ 512
# define NB 128  /*  max. byte transfer = 64K bytes */
# define BUFSIZ (NB*BLKSIZ)
# define MAXUNI  1
# define MAXERR 10  /*  max. no. i/o errors allowed */
# define NL  012
# define CR 015
# define CDEL  0x23
# define LDEL  0x40
/*		*/
# define RM3CYL  823  /* RM03 */
# define RM3TRK  5
# define RM3SEC  32
# define RP6typ  022
# define RM3typ  024
 
struct { int reg ; } ;
int dinoff , douoff  , dblks , count  , ecount , daterr ;
char *bufptr ;
unsigned int page ;
int *RPin , *RPout ;
int rpsec,rptrk,rpcyl;
char input[132] ;
 
main() {
/*
*  Stand-alone program to copy rp06/RM03 disk to rp06/RM03 disk, same or
*  different drives. User specifies input and output disk, no.
*  of 512-byte blocks, and disk offsets.
*/
register int i , error ;

putlin("d2dcpy : Disk-to-Disk Copy") ;
putnl() ;
 
d1u :
 
putstr("input disk unit : ") ;
getcon(input) ;
i = a2l(input) ;
if (i < 0) goto fini ;
if (i > MAXUNI) goto d1u ;
RPin = RP + (i * 32 * 4) ;
 
dof1 :
 
putstr("offset : ") ;
getcon(input) ;
dinoff = a2l(input) ;
 
d2u :
 
putstr("output disk unit : ") ;
getcon(input) ;
i = a2l(input) ;
if ((i > MAXUNI) || (i < 0)) goto d2u ;
RPout = RP + (i * 32 * 4) ;
 
dof2 :
 
putstr("offset : ") ;
getcon(input) ;
douoff = a2l(input) ;
 
gknt :
 
putstr("no. 512-byte blocks : ") ;
getcon(input) ;
count = a2l(input) ;
 
error = 0 ;
 
if (init()) {
	putlin("init error") ;
	goto d1u ;
	}
 
if ((dinoff < 0) || (dinoff > MAXSEC-1)) goto dof1 ;
if ((douoff < 0) || (douoff > MAXSEC-1)) goto dof2 ;
if (count < 0) goto gknt ;
if (count == 0) count = MAXSEC ;
 
ecount = daterr = 0 ;
while ((error == 0) && (count>0)) {
	if (dread()) {
		dioerr :
			putlin("disc i/o error") ;
			error++ ;
			continue ;
			}
	if (dwrite()) goto dioerr ;
	count -= dblks ;/*  dec no. blocks read */
	}
 
fini :
 
putstr("# Data Check Errors : ") ;
l2x(daterr,input) ;
putlin(input) ;
putstr("# Other errors : ") ;
l2x(ecount,input) ;
putlin(input) ;
 
return(0) ;
}
 
/*		*/
 
putstr(csp)
register char *csp ;
{
if (putcon(csp)) return(-1) ;
return(0) ;
}
 
/*		*/
 
putlin(sptr)
register char *sptr ;
{
if (putcon(sptr)) return(-1) ;
if (putnl()) return(-1) ;
return(0) ;
}
 
/*		*/
 
putnl()
{
if (putcon("\r\n")) return(-1) ;
return(0) ;
}
 
/*		*/
 
putcon(csp)
register char *csp ;
{
/*
*  Function to output null-terminated string pointed to 
*  by 'csp' to the VAX LSI terminal.
*/
register c ;
 
c = 0 ;
while (c = (*csp++)) putc(c) ;
return(0) ;
}
 
/*		*/
 
putc(c)
{
/*  wait for LSI printer to be ready */
while ((mfpr(TXCS) & TXCS_RDY) == 0) ;
/*  output character */
mtpr(TXDB,c&0177) ;
}
 
/*		*/
 
getcon(cs)
register char *cs ;
{
/*
*  Function to return char's from VAX LSI keyboard to
*  char array 'cs' - input stops when CR or LF received -
*  null char appended to end of input
*/
register int c , c2 ;
int getc() ;
char *ocs;
 
	ocs=cs;
inloop :
	c = getc() ; /* get 1 char from terminal */
	putc(c) ;  /*  echo char */
	if (c==CDEL) { --cs; goto inloop; }
	if (c==LDEL){ putc(NL);putc(0);putc(CR);cs=ocs;goto inloop;}
	if ((c == NL) || (c == CR)) {
		putc(CR) ;
		putc(0) ;
		putc(NL) ;
		(*cs++) = '\0' ;
		return(0) ;
		}
	else {
		(*cs++) = c ;
		goto inloop ;
		}
}
 
/*		*/
 
getc()
{
/*
*  Return char from VAX LSI terminal char buffer
*/
int mfpr() ;
 
/*  Wait for receiver done (user entered char)
*/
while ((mfpr(RXCS) & RXCS_DONE) == 0) ;
return (mfpr(RXDB) & 0177) ;  /* return char from receiver buffer */
}
 
/*		*/
 
mtpr(regno,value)
{
	asm("	mtpr	8(ap),4(ap)") ;
}
 
/*		*/
 
mfpr(regno)
{
	asm("	mfpr	4(ap),r0") ;
}
 
/*		*/
 
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
 
/*		*/
 
init() {
/*
*  Initialization.
*  Initialize MBA 0 for disk i/o.
*  Set up MBA 0 map registers to map a max.
*    transfer of 'BUFSIZ' bytes.
*/
register int *mp0 , i ;
extern char *end ;
 
M0_cr->reg = MBAinit ;
dblks = BUFSIZ/BLKSIZ ; /* no. of disk blocks / input buffer */
 
if ((*(RPin+RP_sr) & RP_MOL) == 0) {
	putlin("input unit not online") ;
	return(-1) ;
	}
 
*(RPin+RP_cr) = RP_RIP | RP_GO ; /* preset */
*(RPin+RP_off) = RP_FMT ; /* set format bit */
 
if (RPin != RPout) {
	if ((*(RPout + RP_sr) & RP_MOL) == 0) {
		putlin("output unit not online") ;
		return(-1) ;
		}
	*(RPout+RP_cr) = RP_RIP | RP_GO ;
	*(RPout+RP_off) = RP_FMT ;
	}
 
i = *(RPout+RP_typ)&0777 ;  /* get disk type */
if (i==RP6typ) { /* RP06 */
	rpsec=RP6SEC; rpcyl=RP6CYL; rptrk=RP6TRK;
	}
else {
	if (i==RM3typ) {
		rpsec=RM3SEC; rpcyl=RM3CYL; rptrk=RM3TRK;
		}
	else return(-1);
	}
 
bufptr = (int)(&end + 511) & 017777777000 ;
page = (int)((int)bufptr>>9) & 017777777 ;
mp0 = M0_map ; /* phys addr of MBA 0 map reg base */
 
i = NB ;
 
while (i--)
	(*mp0++)= 0x80000000 | page++ ; /* map entry */

return(0);
}
 
/*		*/
 
dread()
{
/*
*  Function to read 'BUFSIZ' bytes (512 multiple) from disc
*  to buffer .
*/
register int i , j ;
 
*(RPin+RP_cyl) = dinoff/RP6ST ; /* cylinder no. */
i = dinoff%RP6ST ;
j = (i/rpsec)<<8 ; /* track */
*(RPin+RP_stk) = j | (i%rpsec) ; /* sector : track */
dinoff += dblks ;  /*  point to next disc sector */
M0_bc->reg = (count<dblks?-(count*512):(-BUFSIZ)) ; /* byte count */
M0_var->reg = 0 ; /* virt addr reg = map no. + byte off */
rbit :
*(RPin+RP_cr) = RP_DC | RP_GO ; /* drive clear */
dwait(RPin) ;
*(RPin+RP_cr) = RP_RD | RP_GO ; /* read */
 
dwait(RPin) ; /* wait for i/o to finish */
if (i = mbaerr(M0)) {
	/* don't abort on MBA errors - 'mbaerr()' has cleared
	MBA status reg */
    }
 
if (i = derror(RPin)) { /*  error  */
	putlin("- - - - - -") ;
	putstr("read error") ;
	stmes(i) ;
	dadmes(RPin) ;
	if (i & RP_DCK) { /* Data Check Error */
		daterr++ ;
		putlin("Data Check") ;
		if (i & RP_ECH) { /*  ECC Hard Error */
			putlin("ECC non-recov") ;
			ecount++ ;
			}
		else { /*  ECC Recoverable */
			ECCrcv(RPin) ;
			}
		if (M0_bc->reg) { /* more i-o to finish ? */
			j = (*(RPin+RP_stk));
			i = (j>>8) & 0x1f;
			j = j & 0x1f;
			if (j>=rpsec) /*sector */
				j = 0;
			if (i>=rptrk) /* track */
				i = 0;
			*(RPin+RP_stk) = (i<<8) | j;
			goto rbit ; /* staus reg cleared by 'Drive Clear' */
			}
		else {
		*(RPin+RP_cr) = RP_DC | RP_GO;
			mbaerr(M0);
			}
		}
	else { /* non-Data Check error */
		ecount++ ;
		}
	if (ecount > MAXERR) return(-1) ;
	}
return(0) ; /* normal return */
}
 
/*		*/
 
dwrite()
{
/*
*  Function to write 'BUFSIZ' bytes (512 multiple) to disc
*  from buffer .
*/
register int i , j ;
 
*(RPout+RP_cr) = RP_DC | RP_GO ;
*(RPout+RP_cyl) = douoff/RP6ST;
i = douoff%RP6ST ;
j = (i/rpsec)<<8 ; /* track */
*(RPout+RP_stk) = j | (i%rpsec) ; /* sector : track */
M0_bc->reg = (count<dblks?-(count*512):(-BUFSIZ)) ; /* byte count */
M0_var->reg = 0 ; /* virt addr reg = map no. + byte off */
*(RPout+RP_cr) = RP_WR | RP_GO ; /* write */
 
douoff += dblks ;  /*  point to next disc sector */
dwait(RPout) ; /* wait for i/o to finish */
if (i = mbaerr(M0)) {
  return(-1) ;
  }
if (i = derror(RPout)) {
	putstr("write error") ;
	stmes(i) ;
	dadmes(RPout) ;
	if (++ecount > MAXERR) return(-1) ;
  }
return(0) ; /* normal return */
}
 
/*		*/
 
dwait(dptr)
register int *dptr ;
{
/*
* Function to wait MBA 0 RP06 disc unit to be ready.
*/
while ((*(dptr+RP_sr)&RP_DRY) == 0) ;
}
 
/*		*/
 
derror(dptr)
register int *dptr ;
{
/*
*  Function to check for MBA 0 RP06 error.
*/
if (*(dptr+RP_sr) & RP_ERR) return(*(dptr+RP_er1) & 0177777) ;
return(0) ;
}
 
/*		*/
 
halt()
{
asm("	halt") ;
}
 
/*		*/
 
stmes(code)
int code ;
{
putstr(" : err reg : ") ;
RPE_print(code) ;
}
 
/*		*/
 
dadmes(dptr)
register int *dptr ;
{
register char *mesp ;
register int i ;
 
mesp = " cyl     trk     sec    " ;
i = l2x(*(dptr+RP_cyl) & 01777,&mesp[5]) ;
blnkit(&mesp[5+i],4-i) ;
 
i = l2x((*(dptr+RP_stk)>>8) & 037 , &mesp[13]) ;
blnkit(&mesp[13+i],3-i) ;
 
i = l2x(*(dptr+RP_stk)&037,&mesp[21]) ;
blnkit(&mesp[21+i],3-i) ;
 
putlin(mesp) ;
}
 
/*		*/
 
blnkit(mp,cc)
register char *mp ;
register int cc ;
{
 
while (cc--)
	(*mp++) = ' ' ;
}
 
/*		*/
 
mbaerr(mba)
register int *mba ;
{
register int i ;
 
if ((i = (*(mba+M_st))) != M_DTC) {
	putlin("- - - - - -") ;
	putstr("MBA error : status reg = ") ;
	MBAS_print(i) ;
	*(mba+M_st) = (-1) ;
	return(i) ;
  }
return(0) ;
}
 
/*		*/
 
l2x(val,rptr)
register int val ;
register char *rptr ;
{
register int i , j ;
register char *tp ;
int knt ;
char tmp[20] , sign ;
 
knt = sign = 0 ;
if (val < 0) {
	sign++ ;
	val = (-val) ;
	}
 
tp = tmp ;
loop :
	knt++ ;
	i = val/16  ;  /*  quotient & base 16 */
	j = val%16 ;
	(*tp++) = j + (j<10?0x30:0x57) ;
	val = i ;
	if (val == 0) {
		/*  done  dividing  */
		if (sign) { knt++ ; (*tp++) = '-' ; }
		for (i = knt ; i ; i--)
			(*rptr++) = tmp[i-1] ;
		(*rptr++) = '\0' ;
		return(knt) ;
		}
	else goto loop ;
}
 
/*		*/
 
ECCrcv(dptr)
register int *dptr ;
{
/*
*  Do ECC error recovery on disk whose register set is pointed
*  to by 'dptr'.
*  'mbaerr()' has cleared MBA status reg.
*  With ECC enabled, disk read has stopped after sector with bad
*  data. After correction of data, return to 'dread()' which will
*  continue read of track if more sectors to do.
*  Return 0.
*/
register unsigned int pos , pat ;
register unsigned short *wordp ;
unsigned int ll ;
struct { short wlo , whi ; } ;
char tmp[50] ;
 
pat = (*(dptr+RP_Epat)) & 0xffff ; /* ECC pattern reg */
pos = (*(dptr+RP_Epos)) & 0xffff ; /* ECC position reg */
putstr("pat : ") ;
ul2x(pat,tmp) ;
putlin(tmp) ;
putstr("pos : ") ;
ul2x(pos,tmp) ;
putlin(tmp) ;
wordp = bufptr ;   /* ptr to buffer */
 
/*
*  'BUFSIZ' bytes are read on each read into buffer pointed to
*  by 'bufptr'. MBA byte count reg has neg. no. of bytes remaining
*  in read if this read error was not in the last sector to be
*  read.
*/
/* calculate buffer location of faulty data */
wordp = (char *)wordp + (BUFSIZ + ((M0_bc->reg)>>16) - BLKSIZ) ; /* sector in buffer */
wordp = wordp +  ((pos-1)>>4) ; /* word within sector */
 
/* burst pattern may be across word boundary */
ll = (*wordp) + ((*(wordp+1))<<16) ;
putstr("bad data  : ") ;
ul2x(ll,tmp) ;
putlin(tmp) ;
pat = pat<<((pos%16)-1) ;
ll = ll^pat ; /* correction */
putstr("good data : ") ;
ul2x(ll,tmp) ;
putlin(tmp) ;
 
/* put good data back in buffer */
*wordp = ll.wlo ;
*(wordp+1) = ll.whi ;
 
return(0) ;
}
 
/*		*/
 
ul2x(val,rptr)
register unsigned int val ;
register char *rptr ;
{
register unsigned int i , j ;
register char *tp ;
int knt ;
char tmp[20] ;
unsigned int udiv() , urem() ;
 
knt =  0 ;
 
tp = tmp ;
loop :
	knt++ ;
	/* use unsigned integer divide & remainder routines */
	i = udiv(val,16)  ;  /*  quotient & base 16 */
	j = urem(val,16) ;
	(*tp++) = j + (j<10?0x30:0x57) ;
	val = i ;
	if (val == 0) {
		/*  done  dividing  */
		for (i = knt ; i ; i--)
			(*rptr++) = tmp[i-1] ;
		(*rptr++) = '\0' ;
		return(knt) ;
		}
	else goto loop ;
}
