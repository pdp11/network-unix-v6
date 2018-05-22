/ Since clock must examine the parameters of our caller, we recover that
/ environment and transfer directly to it.  The net effect is that it looks
/ like clock was called directly from the interrupt.

.globl _vdh_clock, _clock

_vdh_clock:
	mov	r5,r2
	mov	-(r2),r4
	mov	-(r2),r3
	mov	-(r2),r2
	mov	r5,sp
	mov	(sp)+,r5
	bic	$40,177776
	jmp	_clock
