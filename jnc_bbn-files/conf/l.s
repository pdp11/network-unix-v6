/ low core

br4 = 200
br5 = 240
br6 = 300
br7 = 340

. = 0^.
	br	1f
	4

/ trap vectors
	trap; br7+0.		/ bus error
	trap; br7+1.		/ illegal instruction
	trap; br7+2.		/ bpt-trace trap
	trap; br7+3.		/ iot trap
	trap; br7+4.		/ power fail
	trap; br7+5.		/ emulator trap
	trap; br7+6.		/ system entry

. = 40^.
.globl	start, dump
1:	jmp	start
	jmp	dump


. = 60^.
	klin; br4
	klou; br4

. = 100^.
	kwlp; br6
	kwlp; br6

. = 114^.
	trap; br7+7.		/ 11/70 parity

. = 124^.
	impou; br5+1.           / IMP-11A #1, TX side

. = 170^.
	impou; br5+3.           / IMP-11A #2, TX side

. = 174^.
	impin; br5+2.           / IMP-11A #2, RX side

. = 220^.
	rkio; br5

. = 240^.
	trap; br7+7.		/ programmed interrupt
	trap; br7+8.		/ floating point
	trap; br7+9.		/ segmentation violation

. = 274^.
	impin; br5+0.           / IMP-11A, input side

/ floating vectors
. = 300^.
/       dcin; br5+0.
/       dcou; br5+0.
/       dnou; br5+0.
/       dnou; br5+1.
. = 320^.
	klin; br4+5.
	klou; br4+5.
/       klin; br4+6.
/       klou; br4+6.

. = 340^.
	klin; br4+1
	klou; br4+1
	klin; br4+2
	klou; br4+2
	klin; br4+3
	klou; br4+3
	klin; br4+4
	klou; br4+4

//////////////////////////////////////////////////////
/		interface code to C
//////////////////////////////////////////////////////

.globl	call, trap

.globl	_klrint
klin:	jsr	r0,call; _klrint
.globl	_klxint
klou:	jsr	r0,call; _klxint

.globl	_clock
kwlp:	jsr	r0,call; _clock

.globl	_rkintr
rkio:	jsr	r0,call; _rkintr

/.globl  _dcrint
/dcin:   jsr     r0,call; _dcrint
/.globl  _dcxint
/dcou:   jsr     r0,call; _dcxint

/.globl  _dnint
/dnou:   jsr     r0,call; _dnint

.globl  _impint
impin:  jsr     r0,call; _impint
.globl  _impint
impou:  jsr     r0,call; _impint
