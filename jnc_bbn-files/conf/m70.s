/ machine language assist
/ for 11/70 CPUs
/ same as m45.s except for mag tape dump code

.fpp = 1	/ fpp=0 iff no floating point hardware, otherwise = 1
.rkboot = 0	/ set to one if you have an rk disk suitable for booting.
.hpboot = 1	/ set to one if you have an hp disk suitable for booting.
		/ jsq BBN 2-20-79

/ non-UNIX instructions
mfpi	= 6500^tst
mtpi	= 6600^tst
mfpd	= 106500^tst
mtpd	= 106600^tst
spl	= 230
ldfps	= 170100^tst
stfps	= 170200^tst
wait	= 1
rtt	= 6
reset	= 5

HIPRI	= 300
HIGH	= 6

/ Mag tape dump
/ save registers in low core and
/ write all core onto mag tape.
/ entry is thru 44 abs

.data
.globl	dump
dump:
	spl	7		/no interrupts please
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

/ dump all of core (ie to first mt error) onto TU-16 tape
/ TU16-ized by mob on 2/15/78.
/ The lines marked "/*" will cause a TJU-16 UNIBUS tape drive to
/ dump all of physical memory.
/ If they are commented out then a TWU-16 will work.

/*	bis	$40,*$172516	/ enable unibus mapping
/*	clr	*$170200	/ clear unibus base registers
/*	clr	*$170202

	mov	$HTC+2,r0	/ HTWC
	mov	$001300, 30(r0) / drive 0, 800 bpi NRZI => HTTC
	clr	2(r0)		/ clear address register initially => HTBA
	movb	$11, -2(r0)	/ drive clear => HTCS1
6:
	tstb	10(r0)		/ wait for ready =>HTDS
	bpl	6b
/*	br	2f
1:
/*	bit	$20000,2(r0)	/ check if address reached 8k boundary
/*	beq	3f
/*	add	$20000,*$170200 / reset unibus map
/*	adc	*$170202
2:
/*	clr	2(r0)		/ set buffer address to zero
3:
	mov	$-256.,(r0)	/ set block size to 256 words => HTWC
	mov	$-512.,4(r0)	/ set frame counter=>HTFC
	tst	-(r0)		/ HTCS1 => r0
	movb	$61,(r0)	/ start the write  => HTCS1
4:
	tstb	12(r0)		/ wait until io is done => HTDS
	bge	4b
	bit	$40000,(r0)+	/ go get the next block if no error (HTWC=>r0)
	beq	1b
	tst	-(r0)		/ HTCS1 => r0
	movb	$27,(r0)	/ write an eof tape => HTCS1
5:
	tstb	12(r0)		/ wait until io is done => HTDS
	bge	5b
	movb	$7,(r0) 	/ rewind the tape => HTCS1
7:
	tstb	12(r0)		/ wait until io is done=> HTDS
	bge	7b
	reset
	br	.		/ just idle in place

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

/ set KI1-7 to eventual text resting place

	mov	$_end+63.,r2
	ash	$-6,r2
	bic	$!1777,r2
1:
	mov	r2,(r0)+
	mov	r3,(r1)+
	add	$200,r2
	cmp	r0,$KISA7
	blos	1b

/ set KD0-6 to physical

	mov	$KDSA0,r0
	mov	$KDSD0,r1
	clr	r2
1:
	mov	r2,(r0)+
	mov	r3,(r1)+
	add	$200,r2
	cmp	r0,$KDSA7
	blos	1b

/ top data page is peripherals page
	mov	$IO,-(r0)

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

/ turn on memory mapping
1:
	inc	SSR0

/ use data page six to point into the current source text page

	mov	$_edata, r1	/ get data size (current text address)
	mov     $_etext-20000, r3 / get real text size
	clr     r2              / this will be half a long
	add     r1, r3          / add text and data size
	adc     r2              / get high part
	mov     r3, r1          / we'll use this for remainder below
	ashc    $-6., r2        / convert to core clicks and drop high word
	mov     r3, KDSA6       / point page six at TOP of source text

	bic	$!77, r1	/ get offset in current source page
	add	$140000, r1	/ convert to data virtual address
	mov     $_etext, r0     / top virt. dest. text address + 2

/ copy until text dest. virtual address equals bottom of page 1
1:
	cmp     r1, $140000     / are we finished with this page?
	bhi     0f              / no
	sub     $200, r3        / yes
	mov     r3, KDSA6       / point page six one page down
	mov     $160000, r1     / say we have a whole page in source
0:
	mov     -(r1), -(sp)    / virtual source address
	mtpi    -(r0)           / virtual destination text address
	cmp     r0, $20000      / are we through?
	bhi     1b

/ text has been moved, data was always in the right place.
/ bss is where text was and must be cleared.  Note page six points
/ nowhere in particular now, but data+bss cannot use page six anyway,
/ as that is where the user structure goes.  Initial page six is set
/ up shortly below.

	mov	$_edata, r1	/ point r1 to beginning of bss
1:
	clr	(r1)+
	cmp	r1,$_end
	blo	1b

/ set KD6 to first available core

	mov	$_etext-8192.+63.,r2
	ash	$-6,r2
	bic	$!1777,r2
	add	KISA1,r2
	mov	r2,KDSA6

/ set up supervisor D registers

	mov	$6,SISD0
	mov	$6,SISD1

/ set up real sp
/ clear user block

	mov	$_u+[usize*64.],sp
	mov	$_u,r0
1:
	clr	(r0)+
	cmp	r0,sp
	blo	1b
/	jsr	pc,_isprof

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
	mov	PS,saveps
	tst	nofault
	bne	1f
	mov	SSR0,ssr
	mov	SSR1,ssr+2
	mov	SSR2,ssr+4
	mov	$1,SSR0
	jsr	r0,call1; _trap
	/ no return
1:
	mov	$1,SSR0
	mov	nofault,(sp)
	rtt
.text

.globl	_runrun, _swtch
call1:
	mov	saveps,-(sp)
	spl	0
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
.if .fpp
	mov	$20,_u+4		/ FP maint mode
.endif
	jsr	pc,*(r0)+
2:
	spl	HIGH
	tstb	_runrun
	beq	2f
	spl	0
	jsr	pc,_savfp
	jsr	pc,_swtch
	br	2b
2:
.if .fpp
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
.endif
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
.if .fpp
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
.endif
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
	jsr	pc,fuword
	mov	r0,CSW
	mov	(sp)+,PS
	cmp	r0,$-1
	bne	2f
	mov	$120.,dispdly		/ 2 sec delay after CSW fault
2:
	rts	pc

/ =========================== C L I S T ============================== /
/
/ These routines manipulate character queues of the form declared in
/ tty.h. putc(char, queue) puts a char on the tail of the queue;
/ getc(queue) returns a char from the head of the queue. unputc(queue)
/ returns a char from the tail of the queue, that is, the last char
/ putc'ed.
/
/ This implementation differs from the standard one in that
/ in addition to handling char values from 0 to 255, it can also
/ handle values from -2 to -256. They are represented internally
/ with a 255 byte as prefix. A plain 255 byte is represented as
/ 255 255. (Note that -1 comes out as 255.) These additional values ease the
/ task of providing a true 8-bit path in the driver.
/ putc, unputc, getc are NOT significantly less efficient in the normal case.

.globl	_cfreelist

/ --------------------------- P U T C ----------------------------- /
/
/ putc(char, queue_ptr) puts the char (actually an int) on the specified
/ queue. It returns 0 if it was successful, a nonzero value if it was not.
/
.globl _putc
_putc:
	mov	2(sp),r0    / r0 := char
	mov	4(sp),r1    / r1 := q_ptr
/ save environment
	mov	PS,-(sp)
	mov	r2,-(sp)
	mov	r3,-(sp)

	spl	5	    / protect
	cmp	r0,$377     / is it >= 0377 (unsigned)?
	bhis	esc_put     / yes.
	jsr	pc, putc    / no, do it normally
	br	rest	    / done
esc_put:
	mov	$377,r0     / yes. Load escape char
	jsr	pc, putc    / enter it
	mov	10(sp),r0   / get char again
	jsr	pc, putc    / enter it
	beq	rest	    / if it worked, done
/ Here \377 entered, but char did not. Remove \377 and report failure.
	jsr	pc, unputc
/ The \377 will serve as a nonzero return value.

/ restore environment
rest:
	mov	(sp)+,r3
	mov	(sp)+,r2
	mov	(sp)+,PS
	rts	pc

putc:	 / char in r0, q_ptr in r1; clobbers r2, r3
	mov	4(r1),r2	/ r2 := c_cl
	bne	1f

/ first time, get clist block

	mov	_cfreelist,r2	    / r2 := &clist
	beq	9f		    / zero? moby loss
	mov	(r2),_cfreelist     / unchain clist
	clr	(r2)+		    / clear its ptr, r2++
	mov	r2,2(r1)	    / c_cf := r2 (incremented)
	br	2f

/ not first time. Check if c_cl points just after last char position in block

1:
	bit	$7,r2		    / need new block?
	bne	2f		    / if nonzero, no.
	mov	_cfreelist,r3	    / else r3 := &clist
	beq	9f		    / zero? moby loss
	mov	(r3),_cfreelist     / unchain clist block
	mov	r3,-10(r2)	    / make current clist chain ptr -> new block
	mov	r3,r2
	clr	(r2)+		    / and zero new block's chain ptr
2:

/ here r2 finally points at a nice place to put a char

	movb	r0,(r2)+	    / put in char
	mov	r2,4(r1)	    / update c_cl
	inc	(r1)		    / and c_cc
	clr	r0		    / return zero
	rts	pc

/ moby loss, no free storage

9:
	mov	pc,r0		    / return nonzero value
	rts	pc

/ --------------------------- G E T C ----------------------------
/
/ getc -- get a char from a clist
/
/ use: c = getc(queue_ptr)
/ returns -1 if empty

.globl _getc

_getc:
	mov	PS,-(sp)
	mov	r2,-(sp)
	spl	5
	jsr	pc,getc 	/ get char
	cmp	r0,$377 	/ escape char?
	bne	done		/ no, all done
	jsr	pc,getc 	/ yes, get following char
	cmp	r0,$377 	/ also escape?
	beq	done		/ if so, all done
	bis	$!377,r0	/ if not, must be negative value
done:
	mov	(sp)+,r2
	mov	(sp)+,PS
	rts	pc		/ and leave

getc:	 / q_ptr at 10(sp); clobbers r2
	mov	10(sp),r1	/ r1 := q_ptr
	mov	2(r1),r2	/ r2 := c_cf
	beq	9f		/ empty
	movb	(r2)+,r0	/ r0 := char
	bic	$!377,r0	/ undo sign-extension
	mov	r2,2(r1)	/ update c_cf
	dec	(r1)+		/ update c_cc
	bne	1f
/ here was last block
	clr	(r1)+		/ zero c_cf
	clr	(r1)+		/ zero c_cl
	br	2f		/ go put block on cfreelist

1:
	bit	$7,r2		/ end of block?
	bne	3f		/ nope
	mov	-10(r2),(r1)	/ yes, update c_cf to point to next block
	add	$2,(r1) 	/ actually to first char of that block
/ here free block
2:
	dec	r2		/ get back into current block
	bic	$7,r2		/ make pointer to base of block
	mov	_cfreelist,(r2) / chain it onto free list
	mov	r2,_cfreelist
/ here leave, all OK
3:
	rts	pc

/ no char to give!

9:
	clr	4(r1)		/ zero c_cl???
	mov	$-1,r0		/ return -1
	rts	pc

/ --------------------------- U N P U T C --------------------------- /
/
/ char = unputc(queue_ptr) is like getc, but gets char from tail of
/ queue instead of head, thus undoing last putc.
/ Modified by BBN:Dan Franklin for escape byte processing.

.globl _unputc

_unputc:
	mov PS, -(sp)	    / up priority
	spl 5
	mov r2, -(sp)	    / save r2
	mov 6(sp), r1	    / r1 := q_ptr
	jsr pc, unputc	    / get last char
	mov r0, r2
	blt finish	    / if neg, must be empty
/ here we got a char. Peek at previous to see if it's escape.
	mov 6(sp), r1	    / r1 := q_ptr
	mov 4(r1), r1	    / r1 := c_cl
	beq finish	    / if zero, there is no previous char
	cmpb -(r1),$377     / is previous char escape byte?
	bne finish	    / nope, go away
/ here previous was escape byte. Remove it and set high byte of char.
	mov 6(sp), r1	    / r1 := q_ptr
	jsr pc, unputc	    / else get it for real
	mov r2, r0
	cmp $377, r0
	beq finish	/ \377 \377 maps into \377, not -1
	bis $!377, r0
finish:
	mov (sp)+, r2
	mov (sp)+, PS
	rts pc

/ unputc: qp in R1, char left in R0; clobbers r1
unputc: 			/ Mike Patrick, June 76
	mov	r2,-(sp)	/ save regs
	mov	r3,-(sp)

	mov	4(r1),r2	/ c_cl into r2
	beq	5f		/ if 0, empty queue
	dec	r2		/ Last char put in
	movb	(r2),r0 	/ char into r0
	bic	$!377,r0	/ undo DEC braindamage
	dec	(r1)+		/ decrement count, advance r1
	beq	4f		/ if zilch, empty queue now
	bit	$5,r2		/ check if emptied block
	bne	3f		/ if not 010b, return. (can't be 000b)
	bic	$7,r2		/ point to c_next
	mov	_cfreelist,(r2) / put empty block on freelist
	mov	r2,_cfreelist
	mov	(r1),r3 	/ c_cf into r3
	bic	$7,r3		/ ptr to next block down the line
1:	cmp	(r3),r2 	/ block preceding c_cl's ?
	beq	2f
	mov	(r3),r3 	/ nope, move down the line
	br	1b
2:				/ yep
	clr	(r3)		/ end of list
	mov	r3,r2
	add	$10,r2		/ r2 now points past last char put in
3:
	mov	r2,2(r1)	/ New c_cl
	br	9f		/ return

4:	clr	(r1)+		/ c_cf zeroed
	clr	(r1)+		/ c_cl	 "
	bic	$7,r2
	mov	_cfreelist,(r2) / put block on freelist
	mov	r2,_cfreelist
	br	9f		/ return

5:	mov	$-1,r0		/ error condition

9:
	mov	(sp)+,r3	/ restore state
	mov	(sp)+,r2
	rts	pc		/ bye!

.globl	_backup
.globl	_regloc
_backup:
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


.globl	_fubyte, _subyte
.globl	_fuword, _suword
.globl	_fuibyte, _suibyte
.globl	_fuiword, _suiword
_fuibyte:
	mov	2(sp),r1
	bic	$1,r1
	jsr	pc,giword
	br	2f

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
	mov	2(sp),r1
fuiword:
	jsr	pc,giword
	rts	pc

_fuword:
	mov	2(sp),r1
fuword:
	jsr	pc,gword
	rts	pc

giword:
	mov	PS,-(sp)
	spl	HIGH
	mov	nofault,-(sp)
	mov	$err,nofault
	mfpi	(r1)
	mov	(sp)+,r0
	br	1f

gword:
	mov	PS,-(sp)
	spl	HIGH
	mov	nofault,-(sp)
	mov	$err,nofault
	mfpd	(r1)
	mov	(sp)+,r0
	br	1f

_suiword:
	mov	2(sp),r1
	mov	4(sp),r0
suiword:
	jsr	pc,piword
	rts	pc

_suword:
	mov	2(sp),r1
	mov	4(sp),r0
suword:
	jsr	pc,pword
	rts	pc

piword:
	mov	PS,-(sp)
	spl	HIGH
	mov	nofault,-(sp)
	mov	$err,nofault
	mov	r0,-(sp)
	mtpi	(r1)
	br	1f

pword:
	mov	PS,-(sp)
	spl	HIGH
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

.globl	_copyin, _copyout
.globl	_copyiin, _copyiout
_copyiin:
	jsr	pc,copsu
1:
	mfpi	(r0)+
	mov	(sp)+,(r1)+
	sob	r2,1b
	br	2f

_copyin:
	jsr	pc,copsu
1:
	mfpd	(r0)+
	mov	(sp)+,(r1)+
	sob	r2,1b
	br	2f

_copyiout:
	jsr	pc,copsu
1:
	mov	(r0)+,-(sp)
	mtpi	(r1)+
	sob	r2,1b
	br	2f

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

.globl	_idle
_idle:
	mov	PS,-(sp)
	spl	0
	wait
	mov	(sp)+,PS
	rts	pc

.globl	_savu, _retu, _aretu
_savu:
	spl	HIGH
	mov	(sp)+,r1
	mov	(sp),r0
	mov	sp,(r0)+
	mov	r5,(r0)+
	spl	0
	jmp	(r1)

_aretu:
	spl	7
	mov	(sp)+,r1
	mov	(sp),r0
	br	1f

_retu:
	spl	7
	mov	(sp)+,r1
	mov	(sp),KDSA6
	mov	$_u,r0
1:
	mov	(r0)+,sp
	mov	(r0)+,r5
	spl	0
	jmp	(r1)

.globl	_spl0, _spl1, _spl4, _spl5, _spl6, _spl7, _spl_imp
_spl0:
	spl	0
	rts	pc

_spl1:
	spl	1
	rts	pc

_spl4:
	spl	4
	rts	pc

_spl_imp:
_spl5:
	spl	5
	rts	pc

_spl6:
	spl	6
	rts	pc

_spl7:
	spl	HIGH
	rts	pc

.globl	_copyseg
_copyseg:
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

.globl	_clearseg
_clearseg:
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

.globl	_itol
_itol:
	mov	2(sp),r0
	mov	4(sp),r1
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

.globl	_swab
_swab:
	mov	2(sp),r0
	swab	r0
	rts	pc


.globl	_u
_u	= 140000
usize	= 16.

CSW	= 177570
PS	= 177776
SSR0	= 177572
SSR1	= 177574
SSR2	= 177576
SSR3	= 172516
KISA0	= 172340
KISA1	= 172342
KISA7	= 172356
KISD0	= 172300
KDSA0	= 172360
KDSA6	= 172374
KDSA7	= 172376
KDSD0	= 172320
HTC	= 172440	/TU16 Control and Status Register
MTC	= 172522	/TM11 Control Register
SISA0	= 172240
SISA1	= 172242
SISD0	= 172200
SISD1	= 172202
IO	= 177600

.data

.globl _boot	/ bootstraps must go in .data for 45 & 70, .text for 40
/ argument passing part of reboot	jsq BBN 2-20-79
_boot:
	jsr	r5, csv
	mov	6(r5), r4	/ get code for bootstrap on disk
	mov	10(r5), r3	/ inumber of file to boot
.if .rkboot
	tst	4(r5)		/ is boot device rk?
	beq	_rkboot 	/ yes.
.endif
.if .hpboot
	cmp	$6,4(r5)	/ is it hp?
	beq	_hpboot 	/ yes.
.endif
	jmp	cret		/ any return is an error

/ machine language part of reboot system call.
/ reads block 0 off of disk into the low 512 bytes of memory,
/ then executes it.  This is the usual bootstrap (the one
/ with the @ prompt to which you type /unix or whatever).
/ if r4 contains a special code, r4 contains an inumber
/ No need to enter a name on tty8.

.if .rkboot
.globl _rkboot
_rkboot:
	reset			/ resets memory mapping, etc.
	mov	$177412,r1	/ clear disk sector address
	clr	(r1)		/ set track and block address
	clr	-(r1)		/ unibus address
	mov	$-256.,-(r1)	/ word count
	mov	$05,-(r1)	/ read and go in controller
1:
	tstb	(r1)		/ wait until done
	bge	1b
	clr	pc		/ execute it
.endif

.if .hpboot
.globl	_hpboot
_hpboot:
hpda	= 176706
hpca	= 176734
	reset	/clears HPCS2, HPBAE, drive and unit select,disables interrupts
	clr	*$hpca
	mov	$hpda,r1
	clr	(r1)		/ set track and block address to zero
	clr	-(r1)		/ read 512 bytes into low memory
	mov	$-256.,-(r1)
	mov	$71,-(r1)	/ read and go
1:
	tstb	(r1)		/ wait until done
	bge	1b
	clr	pc		/ execute it
.endif

.globl	_ka6
.globl	_cputype

_ka6:	KDSA6
_cputype:45.
stk:	0

.bss
.globl	nofault, ssr
nofault:.=.+2
ssr:	.=.+6
dispdly:.=.+2
saveps:	.=.+2

.text
/ system profiler
/
/rtt	= 6
/CCSB	= 172542
/CCSR	= 172540
/PS	= 177776
/
/.globl	_sprof, _xprobuf, _probuf, _probsiz, _mode
/_probsiz = 7500.+2048.
/
/_isprof:
/	mov	$_sprof,104	/ interrupt
/	mov	$340,106	/ pri
/	mov	$100.,CCSB	/ count set = 100
/	mov	$113,CCSR	/ count down, 10kHz, repeat
/	rts	pc
/
/_sprof:
/	mov	r0,-(sp)
/	mov	PS,r0
/	ash	$-11.,r0
/	bic	$!14,r0
/	add	$1,_mode+2(r0)
/	adc	_mode(r0)
/	cmp	r0,$14		/ user
/	beq	done
/	mov	2(sp),r0	/ pc
/	asr	r0
/	asr	r0
/	bic	$140001,r0
/	cmp	r0,$_probsiz
/	blo	1f
/	inc	_outside
/	br	done
/1:
/	inc	_probuf(r0)
/	bne	done
/	mov	r1,-(sp)
/	mov	$_xprobuf,r1
/2:
/	cmp	(r1)+,r0
/	bne	3f
/	inc	(r1)
/	br	4f
/3:
/	tst	(r1)+
/	bne	2b
/	sub	$4,r1
/	mov	r0,(r1)+
/	mov	$1,(r1)+
/4:
/	mov	(sp)+,r1
/done:
/	mov	(sp)+,r0
/	mov	$113,CCSR
/	rtt
/
/.bss
/_xprobuf:	.=.+512.
/_probuf:.=.+_probsiz
/_mode:	.=.+16.
/_outside: .=.+2

