/ C library -- qtime
/ mek 11/1/78

/ tvec = qtime(tvec)
/
/ tvec[0], tvec[1], tvec[2] contain the time


.globl	_qtime

qtime=67.
_qtime:
	mov	r5,-(sp)
	mov	sp,r5
	sys	qtime
	mov	r3,-(sp)
	mov	4(r5),r3
	mov	r0,(r3)+
	mov	r1,(r3)+
	mov	r2,(r3)+
	mov	(sp)+,r3
	mov	(sp)+,r5
	rts	pc
