


/ machine language assist
/ for 11/40

/ non-UNIX instructions
mfpi	= 6500^tst
mtpi	= 6600^tst
wait	= 1
rtt	= 6
reset	= 5

.globl	trap, call
.globl	_trap
trap:
	mov	PS,-4(sp)
	tst	nofault
	bne	1f
	mov	SSR0,ssr
	mov	SSR2,ssr+4
	mov	$1,SSR0
	jsr	r0,call1; _trap
	/ no return
1:
	mov	$1,SSR0
	mov	nofault,(sp)
	rtt

.globl	_badvec,_badint,bcall	/ processing for an interrupt from an unexpected
				/ location.  translate into a standard call with
				/ _badvec set to the offending location.
bcall:
	mov	PS,-(sp)	/ low four bits of PS give low four
	bic	$177760,(sp)	/ bits of interrupt vector number
	asl	(sp)		/ multiply by four to get memory address
	asl	(sp)
	bis	(r0),(sp)	/ octal centade follows calling location
	mov	(sp)+,_badvec	/ make accessible to C code
	mov	(sp)+,r0	/ recover interrupt environment
	jsr	r0,call; _badint  / call C code at _badint to handle it
	/ no return
.bss
_badvec:  .=.+2			/ bad interrupt location
.text

.globl	_runrun, _swtch
call1:
	tst	-(sp)
	bic	$340,PS
	br	1f

call:
	mov	PS,-(sp)
1:
	mov	r1,-(sp)
	mfpi	sp
	mov	4(sp),-(sp)
	bic	$!37,(sp)
	bit	$30000,PS
	beq	1f
	jsr	pc,*(r0)+
2:
	bis	$340,PS
	tstb	_runrun
	beq	2f
	bic	$340,PS
	jsr	pc,_swtch
	br	2b
2:
	tst	(sp)+
	mtpi	sp
	br	2f
1:
	bis	$30000,PS
	jsr	pc,*(r0)+
	cmp	(sp)+,(sp)+
2:
	mov	(sp)+,r1
	tst	(sp)+
	mov	(sp)+,r0
	rtt

.globl	_savfp, _display
_savfp:
_display:
	rts	pc

.globl	_incupc
_incupc:
	mov	r2,-(sp)
	mov	6(sp),r2	/ base of prof with base,leng,off,scale
	mov	4(sp),r0	/ pc
	sub	4(r2),r0	/ offset
	clc
	ror	r0
	mul	6(r2),r0	/ scale
	ashc	$-14.,r0
	inc	r1
	bic	$1,r1
	cmp	r1,2(r2)	/ length
	bhis	1f
	add	(r2),r1		/ base
	mov	nofault,-(sp)
	mov	$2f,nofault
	mfpi	(r1)
	inc	(sp)
	mtpi	(r1)
	br	3f
2:
	clr	6(r2)
3:
	mov	(sp)+,nofault
1:
	mov	(sp)+,r2
	rts	pc

/ Character list get/put

.globl	_getc, _putc
.globl	_cfreelist

_getc:
	mov	2(sp),r1
	mov	PS,-(sp)
	mov	r2,-(sp)





	mov	r4,-(sp)
	mov	KISA6-40,-(sp)
	mov	KISA6,r4
	bis	$340,PS		/ spl 7
	mov	_clistp,KISA6	/ map in clist
	mov	$77406,KISA6-40

	mov	2(r1),r2	/ first ptr
	beq	9f		/ empty
	movb	(r2)+,r0	/ character
	bic	$!377,r0
	mov	r2,2(r1)
	dec	(r1)+		/ count
	bne	1f
	clr	(r1)+
	clr	(r1)+		/ last block
	br	2f
1:
	bit	$7,r2
	bne	3f
	mov	-10(r2),(r1)	/ next block
	add	$2,(r1)
2:
	dec	r2
	bic	$7,r2
	mov	_cfreelist,(r2)
	mov	r2,_cfreelist
3:

	mov	r4,KISA6	/ recover _u map
	mov	(sp)+,KISA6-40
	mov	(sp)+,r4

	mov	(sp)+,r2
	mov	(sp)+,PS
	rts	pc
9:
	clr	4(r1)
	mov	$-1,r0
	br	3b

_putc:
	mov	2(sp),r0
	mov	4(sp),r1
	mov	PS,-(sp)
	mov	r2,-(sp)
	mov	r3,-(sp)





	mov	r4,-(sp)
	mov	KISA6-40,-(sp)
	mov	KISA6,r4
	bis	$340,PS		/ spl 7
	mov	_clistp,KISA6	/ map in clist
	mov	$77406,KISA6-40

	mov	4(r1),r2	/ last ptr
	bne	1f
	mov	_cfreelist,r2
	beq	9f
	mov	(r2),_cfreelist
	clr	(r2)+
	mov	r2,2(r1)	/ first ptr
	br	2f
1:
	bit	$7,r2
	bne	2f
	mov	_cfreelist,r3
	beq	9f
	mov	(r3),_cfreelist
	mov	r3,-10(r2)
	mov	r3,r2
	clr	(r2)+
2:
	movb	r0,(r2)+
	mov	r2,4(r1)
	inc	(r1)		/ count
	clr	r0
3:

	mov	r4,KISA6	/ recover _u map
	mov	(sp)+,KISA6-40
	mov	(sp)+,r4

	mov	(sp)+,r3
	mov	(sp)+,r2
	mov	(sp)+,PS
	rts	pc
9:
	mov	pc,r0
	br	3b


.globl	_m_cinit, _clistp	/  mod to externalize clist
_m_cinit:
	mov	2(sp),r1	/ pick up count
	mov	r2,-(sp)	/ save work reg and other
	mov	PS,-(sp)	/	processor status
	mov	r4,-(sp)
	mov	KISA6-40,-(sp)
	mov	KISA6,r4

	mov	$_u,r2		/ initalize
	clr	r0
	bis	$340,PS		/ lock out interrupts
	mov	_clistp,KISA6	/ map in clist
	mov	$77406,KISA6-40
1:
	mov	r0,(r2)	/ link in buffer
	mov	r2,r0		/ new head of list
	add	$10,r2		/ move to next
	sob	r1,1b		/ do all entries

	mov	r4,KISA6	/ recover processor info
	mov	(sp)+,KISA6-40
	mov	(sp)+,r4
	mov	(sp)+,PS
	mov	(sp)+,r2
	rts	pc		/ return list head in r0


.globl	_backup
.globl	_regloc
_backup:
	mov	2(sp),ssr+2
	mov	r2,-(sp)
	jsr	pc,backup
	mov	r2,ssr+2
	mov	(sp)+,r2
	movb	jflg,r0
	bne	2f
	mov	2(sp),r0
	movb	ssr+2,r1
	jsr	pc,1f
	movb	ssr+3,r1
	jsr	pc,1f
	movb	_regloc+7,r1
	asl	r1
	add	r0,r1
	mov	ssr+4,(r1)
	clr	r0
2:
	rts	pc
1:
	mov	r1,-(sp)
	asr	(sp)
	asr	(sp)
	asr	(sp)
	bic	$!7,r1
	movb	_regloc(r1),r1
	asl	r1
	add	r0,r1
	sub	(sp)+,(r1)
	rts	pc

/ hard part
/ simulate the ssr2 register missing on 11/40

backup:
	clr	r2		/ backup register ssr1
	mov	$1,bflg		/ clrs jflg
	mov	ssr+4,r0
	jsr	pc,fetch
	mov	r0,r1
	ash	$-11.,r0
	bic	$!36,r0
	jmp	*0f(r0)
0:		t00; t01; t02; t03; t04; t05; t06; t07
		t10; t11; t12; t13; t14; t15; t16; t17

t00:
	clrb	bflg

t10:
	mov	r1,r0
	swab	r0
	bic	$!16,r0
	jmp	*0f(r0)
0:		u0; u1; u2; u3; u4; u5; u6; u7

u6:	/ single op, m[tf]pi, sxt, illegal
	bit	$400,r1
	beq	u5		/ all but m[tf], sxt
	bit	$200,r1
	beq	1f		/ mfpi
	bit	$100,r1
	bne	u5		/ sxt

/ simulate mtpi with double (sp)+,dd
	bic	$4000,r1	/ turn instr into (sp)+
	br	t01

/ simulate mfpi with double ss,-(sp)
1:
	ash	$6,r1
	bis	$46,r1		/ -(sp)
	br	t01

u4:	/ jsr
	mov	r1,r0
	jsr	pc,setreg	/ assume no fault
	bis	$173000,r2	/ -2 from sp
	rts	pc

t07:	/ EIS
	clrb	bflg

u0:	/ jmp, swab
u5:	/ single op
	mov	r1,r0
	br	setreg

t01:	/ mov
t02:	/ cmp
t03:	/ bit
t04:	/ bic
t05:	/ bis
t06:	/ add
t16:	/ sub
	clrb	bflg

t11:	/ movb
t12:	/ cmpb
t13:	/ bitb
t14:	/ bicb
t15:	/ bisb
	mov	r1,r0
	ash	$-6,r0
	jsr	pc,setreg
	swab	r2
	mov	r1,r0
	jsr	pc,setreg

/ if delta(dest) is zero,
/ no need to fetch source

	bit	$370,r2
	beq	1f

/ if mode(source) is R,
/ no fault is possible

	bit	$7000,r1
	beq	1f

/ if reg(source) is reg(dest),
/ too bad.

	mov	r2,-(sp)
	bic	$174370,(sp)
	cmpb	1(sp),(sp)+
	beq	t17

/ start source cycle
/ pick up value of reg

	mov	r1,r0
	ash	$-6,r0
	bic	$!7,r0
	movb	_regloc(r0),r0
	asl	r0
	add	ssr+2,r0
	mov	(r0),r0

/ if reg has been incremented,
/ must decrement it before fetch

	bit	$174000,r2
	ble	2f
	dec	r0
	bit	$10000,r2
	beq	2f
	dec	r0
2:

/ if mode is 6,7 fetch and add X(R) to R

	bit	$4000,r1
	beq	2f
	bit	$2000,r1
	beq	2f
	mov	r0,-(sp)
	mov	ssr+4,r0
	add	$2,r0
	jsr	pc,fetch
	add	(sp)+,r0
2:

/ fetch operand
/ if mode is 3,5,7 fetch *

	jsr	pc,fetch
	bit	$1000,r1
	beq	1f
	bit	$6000,r1
	bne	fetch
1:
	rts	pc

t17:	/ illegal
u1:	/ br
u2:	/ br
u3:	/ br
u7:	/ illegal
	incb	jflg
	rts	pc

setreg:
	mov	r0,-(sp)
	bic	$!7,r0
	bis	r0,r2
	mov	(sp)+,r0
	ash	$-3,r0
	bic	$!7,r0
	movb	0f(r0),r0
	tstb	bflg
	beq	1f
	bit	$2,r2
	beq	2f
	bit	$4,r2
	beq	2f
1:
	cmp	r0,$20
	beq	2f
	cmp	r0,$-20
	beq	2f
	asl	r0
2:
	bisb	r0,r2
	rts	pc

0:	.byte	0,0,10,20,-10,-20,0,0

fetch:
	bic	$1,r0
	mov	nofault,-(sp)
	mov	$1f,nofault
	mfpi	(r0)
	mov	(sp)+,r0
	mov	(sp)+,nofault
	rts	pc

1:
 	mov	(sp)+,nofault
	clrb	r2			/ clear out dest on fault
	mov	$-1,r0
	rts	pc

.bss
bflg:	.=.+1
jflg:	.=.+1
.text

.globl	_fubyte, _subyte
.globl	_fuibyte, _suibyte
.globl	_fuword, _suword
.globl	_fuiword, _suiword
_fuibyte:
_fubyte:
	mov	2(sp),r1
	bic	$1,r1
	jsr	pc,gword
	cmp	r1,2(sp)
	beq	1f
	swab	r0
1:
	bic	$!377,r0
	rts	pc

_suibyte:
_subyte:
	mov	2(sp),r1
	bic	$1,r1
	jsr	pc,gword
	mov	r0,-(sp)
	cmp	r1,4(sp)
	beq	1f
	movb	6(sp),1(sp)
	br	2f
1:
	movb	6(sp),(sp)
2:
	mov	(sp)+,r0
	jsr	pc,pword
	clr	r0
	rts	pc

_fuiword:
_fuword:
	mov	2(sp),r1
fuword:
	jsr	pc,gword
	rts	pc

gword:
	mov	PS,-(sp)
	bis	$340,PS
	mov	nofault,-(sp)
	mov	$err,nofault
	mfpi	(r1)
	mov	(sp)+,r0
	br	1f

_suiword:
_suword:
	mov	2(sp),r1
	mov	4(sp),r0
suword:
	jsr	pc,pword
	rts	pc

pword:
	mov	PS,-(sp)
	bis	$340,PS
	mov	nofault,-(sp)
	mov	$err,nofault
	mov	r0,-(sp)
	mtpi	(r1)
1:
	mov	(sp)+,nofault
	mov	(sp)+,PS
	rts	pc

err:
	mov	(sp)+,nofault
	mov	(sp)+,PS
	tst	(sp)+
	mov	$-1,r0
	rts	pc












































/ w = fbword(addr, offset)
/ fetch buffer word.

.globl	_fbword
_fbword:
	mov	PS,-(sp)
	mov	KISA6,r1
	mov	6(sp),r0	/offset
	bis	$340,PS
	mov	4(sp),KISA6	/base address
	mov	_u(r0),r0
	mov	r1,KISA6
	mov	(sp)+,PS
	rts	pc


/ b = fbbyte(addr, offset)
/ fetch buffer byte.

.globl	_fbbyte
_fbbyte:
	mov	PS,-(sp)
	mov	KISA6,r1
	mov	6(sp),r0	/offset
	bis	$340,PS
	mov	4(sp),KISA6	/base address
	movb	_u(r0),r0
	mov	r1,KISA6
	mov	(sp)+,PS
	bic	$!377,r0
	rts	pc


/ sbword(addr, offset, value)
/ store buffer word

.globl	_sbword
_sbword:
	mov	r2,-(sp)
	mov	PS,-(sp)
	mov	KISA6,r0
	mov	10.(sp),r1	/value
	mov	8.(sp),r2	/offset
	bis	$340,PS
	mov	6(sp),KISA6	/base address
	mov	r1,_u(r2)
	mov	r0,KISA6
	mov	(sp)+,PS
	mov	(sp)+,r2
	rts	pc


/ sbbyte(addr, offset, value)
/ store buffer byte

.globl	_sbbyte
_sbbyte:
	mov	r2,-(sp)
	mov	PS,-(sp)
	mov	KISA6,r0
	mov	10.(sp),r1	/value
	mov	8.(sp),r2	/offset
	bis	$340,PS
	mov	6(sp),KISA6	/base address
	movb	r1,_u(r2)
	mov	r0,KISA6
	mov	(sp)+,PS
	mov	(sp)+,r2
	rts	pc

/ fkbyte(addr)
/  addition -- fetch kernel byte
/ interface is identical to fubyte

.globl	_fkbyte
_fkbyte:
	movb	*2(sp),r0
	bic	$!377,r0
	rts	pc


/ utb(buffer address, offset, user address, word count)
/ Block move from user to buffer.

.globl	_utb
_utb:
	jsr	pc,atb
	mov	nofault,-(sp)
	mov	$3f,nofault

1:
	mfpi	(r1)+
	mov	(sp)+,(r0)+
	sob	r2,1b
	br	2f


/ btu(buffer address, offset, user address, word count)
/ Block move from buffer to user.

.globl	_btu
_btu:
	jsr	pc,atb
	mov	nofault,-(sp)
	mov	$3f,nofault

1:
	mov	(r0)+,-(sp)
	mtpi	(r1)+
	sob	r2,1b

2:	mov	(sp)+,nofault
	clr	r0
	rts	pc

3:
	mov	(sp)+,nofault
	mov	$-1,r0
	rts	pc


/ ktb(buffer address, offset, kernel address, word count)
/ Block move from kernel to buffer.

.globl	_ktb
_ktb:
	jsr	pc,atb

1:
	mov	(r1)+,(r0)+
	sob	r2,1b
	rts	pc


/ btk(buffer address, offset, kernel address, word count)
/ Block move from buffer to kernel.

.globl	_btk
_btk:
	jsr	pc,atb

1:
	mov	(r0)+,(r1)+
	sob	r2,1b
	rts	pc


/ Initialization and completion for routines btu, utb, btk, ktb.

atb:
	mov	$stack,r0
	mov	sp,-(r0)
	mov	KISA6,-(r0)
	mov	(sp),-(r0)
	mov	4(sp),-(r0)	/buffer address (/64)
	mov	r2,(sp)
	mov	6(sp),r0	/offset
	add	$_u,r0
	mov	8.(sp),r1	/kernel or user address
	mov	10.(sp),r2	/wc
	mov	$stack-8,sp
	mov	(sp)+,KISA6
	jsr	pc,*(sp)+	/copy
	mov	(sp)+,KISA6
	mov	(sp)+,sp
	mov	(sp)+,r2
	rts	pc


/ clrbuf(buffer address)
/ clear the 256 word core area specified


.globl	_clrbuf
_clrbuf:
	mov	2(sp),r0
	mov	sp,stack-2	/buffer address (/64)
	mov	$stack-2,sp
	mov	KISA6,-(sp)
	mov	r0,KISA6
	mov	$_u,r0
	mov	$256.,r1

1:
	clr	(r0)+
	sob	r1,1b

	mov	(sp)+,KISA6
	mov	(sp)+,sp
	rts	pc



.globl	_idle
_idle:
	mov	PS,-(sp)
	bic	$340,PS
	wait
	mov	(sp)+,PS
	rts	pc

.globl	_savu, _retu, _aretu
_savu:
	bis	$340,PS
	mov	(sp)+,r1
	mov	(sp),r0
	mov	sp,(r0)+
	mov	r5,(r0)+
	bic	$340,PS
	jmp	(r1)

_aretu:
	bis	$340,PS
	mov	(sp)+,r1
	mov	(sp),r0
	br	1f

_retu:
	bis	$340,PS
	mov	(sp)+,r1

	mov	(sp),_cu

	mov	(sp),KISA6
	mov	$_u,r0
1:
	mov	(r0)+,sp
	mov	(r0)+,r5
	bic	$340,PS
	jmp	(r1)

.globl	_spl0, _spl1, _spl4, _spl5, _spl6, _spl7
_spl0:
	bic	$340,PS
	rts	pc

_spl1:
	bis	$40,PS
	bic	$300,PS
	rts	pc

_spl4:
_spl5:
	bis	$340,PS
	bic	$100,PS
	rts	pc

_spl6:
	/  clock-level interrupts must run at prio 7 to avoid race with VDH

_spl7:
	bis	$340,PS
	rts	pc

.globl	_copyseg
_copyseg:
	mov	PS,-(sp)
	mov	UISA0,-(sp)
	mov	UISA1,-(sp)
	mov	$30340,PS
	mov	10(sp),UISA0
	mov	12(sp),UISA1
	mov	UISD0,-(sp)
	mov	UISD1,-(sp)
	mov	$6,UISD0
	mov	$6,UISD1
	mov	r2,-(sp)
	clr	r0
	mov	$8192.,r1
	mov	$32.,r2
1:
	mfpi	(r0)+
	mtpi	(r1)+
	sob	r2,1b
	mov	(sp)+,r2
	mov	(sp)+,UISD1
	mov	(sp)+,UISD0
	mov	(sp)+,UISA1
	mov	(sp)+,UISA0
	mov	(sp)+,PS
	rts	pc

.globl	_clearseg
_clearseg:
	mov	PS,-(sp)
	mov	UISA0,-(sp)
	mov	$30340,PS
	mov	6(sp),UISA0
	mov	UISD0,-(sp)
	mov	$6,UISD0
	clr	r0
	mov	$32.,r1
1:
	clr	-(sp)
	mtpi	(r0)+
	sob	r1,1b
	mov	(sp)+,UISD0
	mov	(sp)+,UISA0
	mov	(sp)+,PS
	rts	pc

.globl	dump
dump:
	bit	$1,SSR0
	bne	dump

/ save regs r0,r1,r2,r3,r4,r5,r6,KIA6
/ starting at abs location 4

	mov	r0,4
	mov	$6,r0
	mov	r1,(r0)+
	mov	r2,(r0)+
	mov	r3,(r0)+
	mov	r4,(r0)+
	mov	r5,(r0)+
	mov	sp,(r0)+
	mov	KISA6,(r0)+

/ dump all of core (ie to first mt error)
/ onto mag tape. (9 track or 7 track 'binary')

	mov	$MTC,r0
	mov	$60004,(r0)+
	clr	2(r0)
1:
	mov	$-512.,(r0)
	inc	-(r0)
2:
	tstb	(r0)
	bge	2b
	tst	(r0)+
	bge	1b
	reset

/ end of file and loop

	mov	$60007,-(r0)
	br	.

.globl	start, _end, _edata, _main
start:
	bit	$1,SSR0
	bne	start			/ loop if restart
	reset

/ initialize systems segments

	mov	$KISA0,r0
	mov	$KISD0,r1
	mov	$200,r4
	clr	r2
	mov	$6,r3
1:
	mov	r2,(r0)+
	mov	$77406,(r1)+		/ 4k rw
	add	r4,r2
	sob	r3,1b

/ initialize user segment

	mov	$_end+63.,r2
	ash	$-6,r2
	bic	$!1777,r2
	mov	r2,(r0)+		/ ksr6 = sysu
	mov	$usize-1\<8|6,(r1)+

/ initialize io segment
/ set up counts on supervisor segments

	mov	$IO,(r0)+
	mov	$77406,(r1)+		/ rw 4k

/ get a sp and start segmentation

	mov	$_u+[usize*64.],sp
	inc	SSR0

/ clear bss

	mov	$_edata,r0
1:
	clr	(r0)+
	cmp	r0,$_end
	blo	1b

/ clear user block

	mov	$_u,r0
1:
	clr	(r0)+
	cmp	r0,$_u+[usize*64.]
	blo	1b


/ initialize saved u area address to current.

	mov	KISA6,_cu


/ set up previous mode and call main
/ on return, enter user mode at 0R

	mov	$30000,PS
	jsr	pc,_main
	mov	$170000,-(sp)
	clr	-(sp)
	rtt

.globl	_swab
_swab:
	mov	2(sp),r0
	swab	r0
	rts	pc

.globl	_dpadd
_dpadd:
	mov	2(sp),r0
	add	4(sp),2(r0)
	adc	(r0)
	rts	pc

.globl	_dpsub
_dpsub:
	mov	2(sp),r0
	sub	4(sp),2(r0)
	sbc	(r0)
	rts	pc

.globl	_dpcmp
_dpcmp:
	mov	2(sp),r0
	mov	4(sp),r1
	sub	6(sp),r0
	sub	8(sp),r1
	sbc	r0
	bge	1f
	cmp	r0,$-1
	bne	2f
	cmp	r1,$-512.
	bhi	3f
2:
	mov	$-512.,r0
	rts	pc
1:
	bne	2f
	cmp	r1,$512.
	blo	3f
2:
	mov	$512.,r1
3:
	mov	r1,r0
	rts	pc

.globl	_ldiv
_ldiv:
	clr	r0
	mov	2(sp),r1
	div	4(sp),r0
	rts	pc

.globl	_lrem
_lrem:
	clr	r0
	mov	2(sp),r1
	div	4(sp),r0
	mov	r1,r0
	rts	pc

.globl	_lshift
_lshift:
	mov	2(sp),r1
	mov	(r1)+,r0
	mov	(r1),r1
	ashc	4(sp),r0
	mov	r1,r0
	rts	pc

.globl	csv
csv:
	mov	r5,r0
	mov	sp,r5
	mov	r4,-(sp)
	mov	r3,-(sp)
	mov	r2,-(sp)
	jsr	pc,(r0)

.globl cret
cret:
	mov	r5,r2
	mov	-(r2),r4
	mov	-(r2),r3
	mov	-(r2),r2
	mov	r5,sp
	mov	(sp)+,r5
	rts	pc

.globl	_u
_u	= 140000
usize	= 16.

PS	= 177776
SSR0	= 177572
SSR2	= 177576
KISA0	= 172340
KISA6	= 172354
KISD0	= 172300
MTC	= 172522
UISA0	= 177640
UISA1	= 177642
UISD0	= 177600
UISD1	= 177602
IO	= 7600

.data
.globl	_ka6, _cputype
_ka6:	KISA6
_cputype:40.

.globl	_cu


.bss
.globl	nofault, ssr, badtrap
nofault:.=.+2
ssr:	.=.+6
badtrap:.=.+2

.=.+200.
stack:

