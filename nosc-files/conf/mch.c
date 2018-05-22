#define ASSEMBLY	/* so C code will not be defined */
#include "param.h"	/* get defines of various types */

#ifndef CPU70
#ifndef CPU45
#define CPU40		/* default is an 11/40 kernel */
#endif not CPU45
#endif not CPU70

#ifdef CPU70
#define CPU45		/* They're the same to us */
#endif CPU70

/ machine language assist module

/ non-UNIX instructions
mfpi	= 6500^tst
mtpi	= 6600^tst
wait	= 1
rtt	= 6
reset	= 5

#ifndef PROFILER
HIPRI	= 340
HIGH	= 7
#endif not PROFILER
#ifdef PROFILER
HIPRI	= 300
HIGH	= 6
#endif PROFILER

#ifdef CPU40
mfpd	= mfpi
mtpd	= mtpi
#define SPL0	bic $340,PS
#define SPL1	bis $040,PS;  bic $300,PS
#define SPL4	bis $200,PS;  bic $140,PS
#define SPL5	bis $240,PS;  bic $100,PS
#define SPL6	bis $300,PS;  bic $040,PS
#define SPL7	bis $340,PS
#define SPLHIGH	bis $HIPRI,PS;  .if 340-HIPRI;  bic $340-HIPRI,PS;  .endif
#endif CPU40

#ifdef CPU45
mfpd	= 106500^tst
mtpd	= 106600^tst
spl	= 230
#define SPL0	spl 0
#define SPL1	spl 1
#define SPL4	spl 4
#define SPL5	spl 5
#define SPL6	spl 6
#define SPL7	spl 7
#define SPLHIGH	spl HIGH
.data
#endif CPU45

#ifdef FLOATPT
ldfps	= 170100^tst
stfps	= 170200^tst
#endif FLOATPT

/ save registers in low core and
/ write all core onto dump device.
/ entry is thru 44 abs

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
	mov	KDSA6,(r0)+

#ifdef RKDUMP		/* dump is to RK device */
#define NODUMP		/* don't include default dump routine */
/ RK dump:  Write all core onto an rk disk.  The area dumped to must not be
/ in a logical file system.  On our machine we made a file system 4000 blocks
/ long on rk0 (/etc/mkfs /dev/rk0 4000).  Our dump area starts at block 4000.

RKC	= 177404

	mov	$RKC,r0
	clr	4(r0)			/ starting core addr 
	mov	$4000.,r4		/ starting address of the dump area
1:
	mov	$-8192.,2(r0)		/ word count -- 32 sectors at a time
	mov	r4,r3
	clr	r2			/ calculate disk addr
	div	$12.,r2			/ 12 sectors per cylinder
	ash	$4,r2			/ << 4
	bis	r3,r2			/ or in sector address
	mov	r2,6(r0)		/ to rk disk addr register
	bis	$3,(r0)			/ get the rk going
	add	$32.,r4			/ next RK track
2:
	tstb	(r0)			/ is the i/0 done
	bge	2b			/ if not
	tst	(r0)			/ any error (hopefully nonexistent mem
	bge	1b			/ if not more to do
	reset
#endif RKDUMP

#ifdef TUDUMP		/* dump is to TU10 tape drive */
#define NODUMP		/* turn off default dump routine */
/ dump all of core (ie to first mt error)
/ onto TU16 mag tape. (800 bpi) stolen from RJB 10/14/75 by JGD 04/16/76
/ stolen again by JGN 03/14/79 to incorporate in mch.c

HTCS1   = 172440        /TU16 Control and Status Register
HTCS2   = 172450        /TU16 Control and Status Register 2

	mov	$HTCS1,r0
	mov     $HTCS2,r1
	mov     $40,(r1)+       /clear drives, select drive 0, r1-> HTDS
	mov     $1300,32(r0)    / 800bpi        MTTC = 772472
1:
	bit     $10002,(r1)     /online, BOT?
	beq     1b              /not yet
	movb    $11,(r0)        /issue drive clear command
1:
	tstb   (r1)             /drive ready?
	bpl     1b              /not yet
	clr     4(r0)           /HBTA starts at absolute 0
1:
	mov     $-512.,6(r0)    /frame count = 512 bytes
	mov     $-256.,2(r0)    /word count = 256.
	movb    $61,(r0)        /do write
2:
	tstb    (r1)            /done yet?
	bpl     2b              /no
	tst     (r0)            /error or done?
	bpl     1b              /no

/ end of file, rewind, and loop

	reset
	mov     $-1,6(r0)       /frame count = 1 for tape mark (do we need?)
	mov     $27,(r0)        /write tape mark
1:
	tstb    (r1)            /drive ready?
	bpl     1b              /not yet
	mov     $3,(r0)         /unload drive
#endif TUDUMP

#ifndef NODUMP
/ Mag tape dump
/ dump all of core (ie to first mt error)
/ onto mag tape. (9 track or 7 track 'binary')

MTC	= 172522

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

/ end of file

	mov	$60007,-(r0)
#endif not NODUMP

/ loop

	br	.

#ifdef CPU40

/ 11/40 startup.
/ entry is through 0 abs.

.globl	start, _end, _edata, _main
start:
	bit	$1,SSR0
	bne	.			/ loop if restart
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

/ start segmentation

	inc	SSR0

/ clear bss

	mov	$_edata,r0
1:
	clr	(r0)+
	cmp	r0,$_end
	blo	1b
#endif CPU40

#ifdef CPU45
.globl	start, _end, _edata, _etext, _main

/ 11/45 and 11/70 startup.
/ entry is thru 0 abs.
/ since core is shuffled,
/ this code can be executed but once

start:
	inc	$-1
	bne	.
	reset
	clr	PS

/ set KI0 to physical 0

	mov	$77406,r3
	mov	$KISA0,r0
	mov	$KISD0,r1
	clr	(r0)+
	mov	r3,(r1)+

/ set KI1-6 to eventual text resting place

	mov	$_end+63.,r2
	ash	$-6,r2
	bic	$!1777,r2
1:
	mov	r2,(r0)+
	mov	r3,(r1)+
	add	$200,r2
	cmp	r0,$KISA7
	blos	1b

/ set KI7 to IO seg for escape

	mov	$IO,-(r0)

/ set KD0-7 to physical

	mov	$KDSA0,r0
	mov	$KDSD0,r1
	clr	r2
1:
	mov	r2,(r0)+
	mov	r3,(r1)+
	add	$200,r2
	cmp	r0,$KDSA7
	blos	1b

/ initialization
/ get a temp (1-word) stack
/ turn on segmentation
/ copy text to I space
/ clear bss in D space

	mov	$stk+2,sp
	mov	$65,SSR3		/ 22-bit, map, K+U sep
	bit	$20,SSR3
	beq	1f
	mov	$70.,_cputype
1:
	inc	SSR0
	mov	$_etext,r0
	mov	$_edata,r1
	add	$_etext-8192.,r1
1:
	mov	-(r1),-(sp)
	mtpi	-(r0)
	cmp	r1,$_edata
	bhi	1b
1:
	clr	(r1)+
	cmp	r1,$_end
	blo	1b

/ use KI escape to set KD7 to IO seg
/ set KD6 to first available core

	mov	$IO,-(sp)
	mtpi	*$KDSA7
	mov	$_etext-8192.+63.,r2
	ash	$-6,r2
	bic	$!1777,r2
	add	KISA1,r2
	mov	r2,KDSA6

/ set up supervisor D registers

	mov	$6,SISD0
	mov	$6,SISD1
#endif CPU45

/ set up real sp
/ clear user block

	mov	$_u+[usize*64.],sp
	mov	$_u,r0
1:
	clr	(r0)+
	cmp	r0,sp
	blo	1b

#ifdef PROFILER
	jsr	pc,_isprof
#endif PROFILER

/ set up previous mode and call main
/ on return, enter user mode at 0R

	mov	$30000,PS
	jsr	pc,_main
	mov	$170000,-(sp)
	clr	-(sp)
	rtt

.globl	trap, call
.globl	_trap

/ all traps and interrupts are
/ vectored thru this routine.

trap:
	mov	PS,-4(sp)
	tst	nofault
	bne	1f
	mov	SSR0,ssr
#ifdef CPU45
	mov	SSR1,ssr+2
#endif CPU45
	mov	SSR2,ssr+4
	mov	$1,SSR0
	jsr	r0,call1; _trap
	/ no return
1:
	mov	$1,SSR0
	mov	nofault,(sp)
	rtt
.text

.globl	_badvec,_badint,bcall

/ processing for an interrupt from an unexpected
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
	SPL0
	br	1f

call:
	mov	PS,-(sp)
1:
	mov	r1,-(sp)
	mfpd	sp
	mov	4(sp),-(sp)
	bic	$!37,(sp)
	bit	$30000,PS
	beq	1f
#ifdef FLOATPT
	mov	$20,_u+4		/ FP maint mode
#endif FLOATPT
	jsr	pc,*(r0)+
2:
	SPLHIGH
	tstb	_runrun
	beq	2f
	SPL0
#ifdef FLOATPT
	jsr	pc,_savfp
#endif FLOATPT
	jsr	pc,_swtch
	br	2b
2:
#ifdef FLOATPT
	mov	$_u+4,r1
	bit	$20,(r1)
	bne	2f
	mov	(r1)+,r0
	ldfps	r0
	movf	(r1)+,fr0
	movf	(r1)+,fr1
	movf	fr1,fr4
	movf	(r1)+,fr1
	movf	fr1,fr5
	movf	(r1)+,fr1
	movf	(r1)+,fr2
	movf	(r1)+,fr3
	ldfps	r0
2:
#endif FLOATPT
	tst	(sp)+
	mtpd	sp
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

.globl	_savfp
_savfp:
#ifdef FLOATPT
	mov	$_u+4,r1
	bit	$20,(r1)
	beq	1f
	stfps	(r1)+
	movf	fr0,(r1)+
	movf	fr4,fr0
	movf	fr0,(r1)+
	movf	fr5,fr0
	movf	fr0,(r1)+
	movf	fr1,(r1)+
	movf	fr2,(r1)+
	movf	fr3,(r1)+
1:
#endif FLOATPT
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
	mfpd	(r1)
	inc	(sp)
	mtpd	(r1)
	br	3f
2:
	clr	6(r2)
3:
	mov	(sp)+,nofault
1:
	mov	(sp)+,r2
	rts	pc

.globl	_display
_display:
#ifdef CPU45
	dec	dispdly
	bge	2f
	clr	dispdly
	mov	PS,-(sp)
	mov	$HIPRI,PS
	mov	CSW,r1
	bit	$1,r1
	beq	1f
	bis	$30000,PS
	dec	r1
1:
	jsr	pc,gword
	mov	r0,CSW
	mov	(sp)+,PS
	cmp	r0,$-1
	bne	2f
	mov	$120.,dispdly		/ 2 sec delay after CSW fault
.bss
dispdly:.=.+2
.text
2:
#endif CPU45
	rts	pc

/ Character list get/put

.globl	_getc, _putc
.globl	_cfreelist

_getc:
	mov	2(sp),r1
	mov	PS,-(sp)
	mov	r2,-(sp)
#ifndef UCBUFMOD
	SPL5
#endif not UCBUFMOD
#ifdef UCBUFMOD
	mov	r4,-(sp)
	mov	KDSA6-40,-(sp)
	mov	KDSA6,r4
	SPLHIGH
	mov	_clistp,KDSA6	/ map in clist
	mov	$77406,KDSA6-40
#endif UCBUFMOD
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
#ifdef UCBUFMOD
	mov	r4,KDSA6	/ recover _u map
	mov	(sp)+,KDSA6-40
	mov	(sp)+,r4
#endif UCBUFMOD
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
#ifndef UCBUFMOD
	SPL5
#endif not UCBUFMOD
#ifdef UCBUFMOD
	mov	r4,-(sp)
	mov	KDSA6-40,-(sp)
	mov	KDSA6,r4
	SPLHIGH
	mov	_clistp,KDSA6	/ map in clist
	mov	$77406,KDSA6-40
#endif UCBUFMOD
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
#ifdef UCBUFMOD
	mov	r4,KDSA6	/ recover _u map
	mov	(sp)+,KDSA6-40
	mov	(sp)+,r4
#endif UCBUFMOD
	mov	(sp)+,r3
	mov	(sp)+,r2
	mov	(sp)+,PS
	rts	pc
9:
	mov	pc,r0
	br	3b

#ifndef CANBSIZ
.globl _unputc,_zapc		/ synonyms for backc used by other system code
_unputc:		/ Used by MIT tty driver
_zapc:			/ Used by UCLA tty driver
.globl	_backc
_backc:		/ backc(arg)

	/ first check to see if the queue is empty -- if so, return a
	/ minus one to indicate that fact

	mov	2(sp),r1	/ q = arg
	tst	(r1)		/ if(q->count == 0)
	bne	1f
	mov	$-1,r0		/ return (-1)
	rts	pc

	/ so there is at least one character in the queue.  since we will
	/ muck around with the list, we arrange for indivisibility and get
	/ the last character (the tail pointer points one past the last
	/ character in the queue).

1:
	mov	r2,-(sp)	/ save regs
	mov	r3,-(sp)
	mov	PS,-(sp)	/ save old priority
#ifndef UCBUFMOD
	SPL5
#endif not UCBUFMOD
#ifdef UCBUFMOD
	mov	r4,-(sp)
	mov	KDSA6-40,-(sp)
	mov	KDSA6,r4
	SPLHIGH
	mov	_clistp,KDSA6	/ map in clist area
	mov	$77406,KDSA6-40
#endif UCBUFMOD
	mov	4(r1),r2	/ c = *(t = --q->tail)&0377
	movb	-(r2),r0
	mov	r2,4(r1)
	bic	$!377,r0

	/ decrement the queue counter and check to see if we removed the last
	/ character.  if we did, zap the queue pointers and arrange
	/ for the return of the queue element to the free list.

	dec	(r1)+		/ if(--q->count)
	bne	5f
	clr	(r1)+		/ q->head = 0
	clr	(r1)+		/ q->tail = 0
	bic	$7,r2		/ t =& not 7
	br	6f

	/ check to see if we just removed the first character in this queue
	/ element.  if we did, we must find its predecessor (which must exist,
	/ since there is at least one other character in the queue).  if not,
	/ we can just peaceably go away.


5:
	mov	r2,r3		/ if((t&7) == 2)
	bic	$!7,r3
	cmp	$2,r3
	bne	4f

	/ expensive part -- must search queue from front to find element
	/ linked to this one -- it will become new tail

	/ fix the pointers so they point at the beginning  of the q element.


	bic	$7,r2		/ t =& not 7
	mov	(r1),r3		/ p = (q->head & not 7)
	bic	$7,r3

	/ search down the queue until the predecessor is found

2:
	cmp	(r3),r2		/ if(p->link != t)
	beq	3f
	mov	(r3),r3		/ p = p->link
	bne	2b		/ loop if not end-of-list
	br	.		/ error, should not find it

	/ fixup the end of the queue by making the last element point to
	/ null and the tail pointer point past the last character in that
	/ element.

3:
	clr	(r3)		/ p->link = 0
	add	$10,r3		/ q->tail = ++p
	mov	r3,2(r1)

	/ return the released queue element to the free list

6:
	mov	_cfreelist,(r2)	/ t->link = cfreelist
	mov	r2,_cfreelist	/ cfreelist = t

	/ that's all -- return the character we removed to the caller.

4:
#ifdef UCBUFMOD
	mov	r4,KDSA6	/ map _u back in
	mov	(sp)+,KDSA6-40
	mov	(sp)+,r4
#endif UCBUFMOD
	mov	(sp)+,PS
	mov	(sp)+,r3	/ restore regs
	mov	(sp)+,r2
	rts	pc		/ return (c)
#endif not CANBSIZ

#ifdef UCBUFMOD
.globl	_m_cinit, _clistp	/ NOSC mod to externalize clist
_m_cinit:
	mov	2(sp),r1	/ pick up count
	mov	r2,-(sp)	/ save work reg and other
	mov	PS,-(sp)	/	processor status
	mov	r4,-(sp)
	mov	KDSA6-40,-(sp)
	mov	KDSA6,r4

	mov	$_u,r2		/ initalize
	clr	r0
	SPLHIGH			/ lock out interrupts
	mov	_clistp,KDSA6	/ map in clist
	mov	$77406,KDSA6-40
1:
	mov	r0,(r2)	/ link in buffer
	mov	r2,r0		/ new head of list
	add	$10,r2		/ move to next
	sob	r1,1b		/ do all entries

	mov	r4,KDSA6	/ recover processor info
	mov	(sp)+,KDSA6-40
	mov	(sp)+,r4
	mov	(sp)+,PS
	mov	(sp)+,r2
	rts	pc		/ return list head in r0
#endif UCBUFMOD

.globl	_backup
.globl	_regloc
_backup:
#ifdef CPU40
	mov	2(sp),ssr+2
	mov	r2,-(sp)
	jsr	pc,backup
	mov	r2,ssr+2
	mov	(sp)+,r2
	movb	jflg,r0
	bne	2f
#endif CPU40
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

#ifdef CPU40
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
#endif CPU40


.globl	_fubyte, _subyte
.globl	_fuword, _suword
.globl	_fuibyte, _suibyte
.globl	_fuiword, _suiword
_fuibyte:
#ifdef CPU45
	mov	2(sp),r1
	bic	$1,r1
	jsr	pc,giword
	br	2f
#endif CPU45

_fubyte:
	mov	2(sp),r1
	bic	$1,r1
	jsr	pc,gword
2:
	cmp	r1,2(sp)
	beq	1f
	swab	r0
1:
	bic	$!377,r0
	rts	pc

_suibyte:
#ifdef CPU45
	mov	2(sp),r1
	bic	$1,r1
	jsr	pc,giword
	mov	r0,-(sp)
	cmp	r1,4(sp)
	beq	1f
	movb	6(sp),1(sp)
	br	2f
1:
	movb	6(sp),(sp)
2:
	mov	(sp)+,r0
	jsr	pc,piword
	clr	r0
	rts	pc
#endif CPU45

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
#ifdef CPU45
	mov	2(sp),r1
	jsr	pc,giword
	rts	pc
#endif CPU45

_fuword:
	mov	2(sp),r1
	jsr	pc,gword
	rts	pc

#ifdef CPU45
giword:
	mov	PS,-(sp)
	SPLHIGH
	mov	nofault,-(sp)
	mov	$err,nofault
	mfpi	(r1)
	mov	(sp)+,r0
	br	1f
#endif CPU45

gword:
	mov	PS,-(sp)
	SPLHIGH
	mov	nofault,-(sp)
	mov	$err,nofault
	mfpd	(r1)
	mov	(sp)+,r0
	br	1f

_suiword:
#ifdef CPU45
	mov	2(sp),r1
	mov	4(sp),r0
	jsr	pc,piword
	rts	pc
#endif CPU45

_suword:
	mov	2(sp),r1
	mov	4(sp),r0
	jsr	pc,pword
	rts	pc

#ifdef CPU45
piword:
	mov	PS,-(sp)
	SPLHIGH
	mov	nofault,-(sp)
	mov	$err,nofault
	mov	r0,-(sp)
	mtpi	(r1)
	br	1f
#endif CPU45

pword:
	mov	PS,-(sp)
	SPLHIGH
	mov	nofault,-(sp)
	mov	$err,nofault
	mov	r0,-(sp)
	mtpd	(r1)
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

#ifndef UCBUFMOD
.globl	_copyin, _copyout
.globl	_copyiin, _copyiout
_copyiin:
#ifdef CPU45
	jsr	pc,copsu
1:
	mfpi	(r0)+
	mov	(sp)+,(r1)+
	sob	r2,1b
	br	2f
#endif CPU45

_copyin:
	jsr	pc,copsu
1:
	mfpd	(r0)+
	mov	(sp)+,(r1)+
	sob	r2,1b
	br	2f

_copyiout:
#ifdef CPU45
	jsr	pc,copsu
1:
	mov	(r0)+,-(sp)
	mtpi	(r1)+
	sob	r2,1b
	br	2f
#endif CPU45

_copyout:
	jsr	pc,copsu
1:
	mov	(r0)+,-(sp)
	mtpd	(r1)+
	sob	r2,1b
2:
	mov	(sp)+,nofault
	mov	(sp)+,r2
	clr	r0
	rts	pc

copsu:
	mov	(sp)+,r0
	mov	r2,-(sp)
	mov	nofault,-(sp)
	mov	r0,-(sp)
	mov	10(sp),r0
	mov	12(sp),r1
	mov	14(sp),r2
	asr	r2
	mov	$1f,nofault
	rts	pc

1:
	mov	(sp)+,nofault
	mov	(sp)+,r2
	mov	$-1,r0
	rts	pc
#endif not UCBUFMOD

#ifdef UCBUFMOD

/ w = fbword(addr, offset)
/ fetch buffer word.

.globl	_fbword
_fbword:
	mov	PS,-(sp)
	mov	KDSA6,r1
	mov	6(sp),r0	/offset
	SPLHIGH
	mov	4(sp),KDSA6	/base address
	mov	_u(r0),r0
	mov	r1,KDSA6
	mov	(sp)+,PS
	rts	pc


/ b = fbbyte(addr, offset)
/ fetch buffer byte.

.globl	_fbbyte
_fbbyte:
	mov	PS,-(sp)
	mov	KDSA6,r1
	mov	6(sp),r0	/offset
	SPLHIGH
	mov	4(sp),KDSA6	/base address
	movb	_u(r0),r0
	mov	r1,KDSA6
	mov	(sp)+,PS
	bic	$!377,r0
	rts	pc


/ sbword(addr, offset, value)
/ store buffer word

.globl	_sbword
_sbword:
	mov	r2,-(sp)
	mov	PS,-(sp)
	mov	KDSA6,r0
	mov	10.(sp),r1	/value
	mov	8.(sp),r2	/offset
	SPLHIGH
	mov	6(sp),KDSA6	/base address
	mov	r1,_u(r2)
	mov	r0,KDSA6
	mov	(sp)+,PS
	mov	(sp)+,r2
	rts	pc


/ sbbyte(addr, offset, value)
/ store buffer byte

.globl	_sbbyte
_sbbyte:
	mov	r2,-(sp)
	mov	PS,-(sp)
	mov	KDSA6,r0
	mov	10.(sp),r1	/value
	mov	8.(sp),r2	/offset
	SPLHIGH
	mov	6(sp),KDSA6	/base address
	movb	r1,_u(r2)
	mov	r0,KDSA6
	mov	(sp)+,PS
	mov	(sp)+,r2
	rts	pc

/ fkbyte(addr)
/ NOSC addition -- fetch kernel byte
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
	mfpd	(r1)+
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
	mtpd	(r1)+
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
	mov	PS,-(sp)
	mov	$stack,r0
	SPLHIGH
	mov	sp,-(r0)
	mov	KDSA6,-(r0)
	mov	2(sp),-(r0)
	mov	6(sp),-(r0)	/buffer address (/64)
	mov	r2,2(sp)
	mov	12.(sp),r2	/wc
	beq	1f		/nothing to transfer
	mov	10.(sp),r1	/kernel or user address
	mov	8.(sp),r0	/offset
	add	$_u,r0
	mov	$stack-8,sp
	mov	(sp)+,KDSA6
	jsr	pc,*(sp)+	/copy
	mov	(sp)+,KDSA6
	mov	(sp)+,sp
1:
	mov	(sp)+,PS
	mov	(sp)+,r2
	rts	pc

.bss		/ Worst case is an interrupt during utb or btu, at which
		/ point the stack will contain:
		/	1. Saved SP
		/	2. Saved KDSA6
		/	3. Return to atb code
		/	4. Saved nofault
		/	5. Interrupt PC
		/	6. Interrupt PS
		/	7. (unused)
		/	8. New PS saved by trap routine
	.=.+16.
stack:
.text
#endif UCBUFMOD

.globl	_idle
_idle:
	mov	PS,-(sp)
	SPL0
	wait
	mov	(sp)+,PS
	rts	pc

.globl	_savu, _retu, _aretu
_savu:
	SPLHIGH
	mov	(sp)+,r1
	mov	(sp),r0
	mov	sp,(r0)+
	mov	r5,(r0)+
	SPL0
	jmp	(r1)

_aretu:
	SPL7
	mov	(sp)+,r1
	mov	(sp),r0
	br	1f

_retu:
	SPL7
	mov	(sp)+,r1
	mov	(sp),KDSA6
	mov	$_u,r0
1:
	mov	(r0)+,sp
	mov	(r0)+,r5
	SPL0
	jmp	(r1)

.globl	_spl0, _spl1, _spl4, _spl5, _spl6, _spl7
_spl0:
	SPL0
	rts	pc

_spl1:
	SPL1
	rts	pc

_spl4:
	SPL4
	rts	pc

_spl5:
	SPL5
	rts	pc

_spl6:
			/ Clock priority must be same as VDH
_spl7:
	SPLHIGH
	rts	pc

.globl	_copyseg
_copyseg:
#ifdef CPU40
	mov	PS,-(sp)
	mov	UISA0,-(sp)
	mov	UISA1,-(sp)
	mov	$30000+HIPRI,PS
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
#endif CPU40

#ifdef CPU45
	mov	PS,-(sp)
	mov	4(sp),SISA0
	mov	6(sp),SISA1
	mov	$10000+HIPRI,PS
	mov	r2,-(sp)
	clr	r0
	mov	$8192.,r1
	mov	$32.,r2
1:
	mfpd	(r0)+
	mtpd	(r1)+
	sob	r2,1b
	mov	(sp)+,r2
	mov	(sp)+,PS
	rts	pc
#endif CPU45

.globl	_clearseg
_clearseg:
#ifdef CPU40
	mov	PS,-(sp)
	mov	UISA0,-(sp)
	mov	$30000+HIPRI,PS
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
#endif CPU40

#ifdef CPU45
	mov	PS,-(sp)
	mov	4(sp),SISA0
	mov	$10000+HIPRI,PS
	clr	r0
	mov	$32.,r1
1:
	clr	-(sp)
	mtpd	(r0)+
	sob	r1,1b
	mov	(sp)+,PS
	rts	pc
#endif CPU45

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

.globl	cret
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
KISA1	= 172342
KISA6	= 172354
KISA7	= 172356
KISD0	= 172300

#ifdef CPU40
KDSA0	= KISA0
KDSA1	= KISA1
KDSA6	= KISA6
KDSA7	= KISA7
UISA0	= 177640
UISA1	= 177642
UISD0	= 177600
UISD1	= 177602
IO	= 7600
#endif CPU40

#ifdef CPU45
CSW	= 177570
SSR1	= 177574
SSR3	= 172516
KDSA0	= 172360
KDSA6	= 172374
KDSA7	= 172376
KDSD0	= 172320
SISA0	= 172240
SISA1	= 172242
SISD0	= 172200
SISD1	= 172202
IO	= 177600
#endif CPU45

.data
.globl	_ka6
.globl	_cputype

_ka6:	KDSA6
#ifdef CPU40
_cputype:40.
#endif CPU40
#ifdef CPU45
_cputype:45.
stk:	0
#endif CPU45

.bss
.globl	nofault, ssr
nofault:.=.+2
ssr:	.=.+6

#ifdef PROFILER
.text
/ system profiler

CCSB	= 172542
CCSR	= 172540

.globl	_sprof, _xprobuf, _probuf, _probsiz, _mode
_probsiz = 7500.+2048.

_isprof:
	mov	$_sprof,104	/ interrupt
	mov	$340,106	/ pri
	mov	$100.,CCSB	/ count set = 100
	mov	$113,CCSR	/ count down, 10kHz, repeat
	rts	pc

_sprof:
	mov	r0,-(sp)
	mov	PS,r0
	ash	$-11.,r0
	bic	$!14,r0
	add	$1,_mode+2(r0)
	adc	_mode(r0)
	cmp	r0,$14		/ user
	beq	done
	mov	2(sp),r0	/ pc
	asr	r0
	asr	r0
	bic	$140001,r0
	cmp	r0,$_probsiz
	blo	1f
	inc	_outside
	br	done
1:
	inc	_probuf(r0)
	bne	done
	mov	r1,-(sp)
	mov	$_xprobuf,r1
2:
	cmp	(r1)+,r0
	bne	3f
	inc	(r1)
	br	4f
3:
	tst	(r1)+
	bne	2b
	sub	$4,r1
	mov	r0,(r1)+
	mov	$1,(r1)+
4:
	mov	(sp)+,r1
done:
	mov	(sp)+,r0
	mov	$113,CCSR
	rtt

.bss
_xprobuf:	.=.+512.
_probuf:.=.+_probsiz
_mode:	.=.+16.
_outside: .=.+2
#endif PROFILER
