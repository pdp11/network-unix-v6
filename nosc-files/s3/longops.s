/ C library-- versions of long *, /, % for floating point hardware
/  also =* =% =/

.globl	lmul, ldiv, lrem, almul, aldiv, alrem
.globl	fptrap

/
/ called: 2(sp):LHS 6(sp):RHS
lmul:
	setl
	movif	2(sp),r0
	movif	6(sp),r1
	mulf	r1,r0
	movfi	r0,-(sp)
	mov	(sp)+,r0
	mov	(sp)+,r1
	seti
	rts	pc

ldiv:
	setl
	movif	2(sp),r0
	movif	6(sp),r1
	divf	r1,r0
	movfi	r0,-(sp)
	mov	(sp)+,r0
	mov	(sp)+,r1
	seti
	rts	pc

lrem:
	setl
	movif	2(sp),r0
	movf	r0,r2
	movif	6(sp),r1
	movf	r1,r3
	divf	r1,r0
	modf	$40200,r0
	mulf	r3,r1
	subf	r1,r2
	movfi	r2,-(sp)
	mov	(sp)+,r0
	mov	(sp)+,r1
	seti
	rts	pc

/
/ called: 2(sp):ptr to LHS  4(sp):RHS
almul:
	setl
	mov	2(sp),r0
	movif	(r0),fr0
	movif	4(sp),fr1
	mulf	fr1,fr0
	movfi	fr0,(r0)
	mov	2(r0),r1
	mov	(r0),r0
	seti
	rts	pc

aldiv:
	setl
	mov	2(sp),r0
	movif	(r0),fr0
	movif	4(sp),fr1
	divf	fr1,fr0
	movfi	fr0,(r0)
	mov	2(r0),r1
	mov	(r0),r0
	seti
	rts	pc

alrem:
	setl
	mov	2(sp),r0
	movif	(r0),fr0
	movif	4(sp),fr1
	movf	fr0,fr2
	movf	fr1,fr3
	divf	fr1,fr0
	modf	$40200,fr0
	mulf	fr3,fr1
	subf	fr1,fr2
	movfi	fr2,(r0)
	mov	2(r0),r1
	mov	(r0),r0
	seti
	rts	pc
