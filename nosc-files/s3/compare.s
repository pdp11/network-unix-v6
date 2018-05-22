.globl _compare
_compare:
	clr	r0
	sub	4(sp),2(sp)
	sbc	r0
	bmi	1f
	tst	2(sp)
	beq	1f
	inc	r0
1:
	rts	pc
