/ low core

br4 = 200
br5 = 240
br6 = 300
br7 = 340

. = 0^.
	br	1f

.globl	sexdisp		/ this is the location to display if you want to get
sexdisp:	017	/ the idle pattern


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
	jmp     dump            /44


. = 60^.
	klin; br4
	klou; br4

. = 100^.
	kwlp; br6
	kwlp; br6

. = 114^.
	trap; br7+7.		/ 11/70 parity

. = 124^.			/ DEC IMP-11A interface (output DR-11B)
	netou; br5

. = 160^.		/ RL11 controller (note not at standard 330 location!)
	rlio; br5

/. = 224^.
/	htio; br5

. = 240^.
	trap; br7+7.		/ programmed interrupt
	trap; br7+8.		/ floating point
	trap; br7+9.		/ segmentation violation

/. = 254^.
/	hpio; br5

. = 274^.			/ DEC IMP-11A interface (input DR-11B)
	netin; br5

/ floating vectors

/. = 300^.	/ DH11 for Green 40
/	dhin; br5
/	dhou; br5

/	DH11-AD's for 70
. = 300^.	/dm11 (modem control)
	dmint; br4+0.
/	dmint; br4+1.	/ Have two Dm11's

. = 310^.       /dh11
	dhin; br5+0.
	dhou; br5+0.
/	dhin; br5+1.	/ have 2 DH-11's
/	dhou; br5+1.


.globl	sexcount	/ this is a counter used to get the idle pattern
sexcount:	4.


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


/.globl	_htintr
/htio:	jsr	r0,call; _htintr

/.globl	_hpintr
/hpio:	jsr	r0,call; _hpintr

.globl _rlintr
rlio:	jsr	r0,call; _rlintr	/ RL01 driver

.globl	_dmint
dmint:	jsr	r0,call; _dmint

.globl	_dhrint
dhin:	jsr	r0,call; _dhrint
.globl	_dhxint
dhou:	jsr	r0,call; _dhxint

.globl  _i11a_iint,_i11a_oint             / ADDED FOR NCP
netin:  jsr     r0,call; _i11a_iint      / DEC IMP-11A interface
netou:  jsr     r0,call; _i11a_oint      / ADDED FOR NCP

