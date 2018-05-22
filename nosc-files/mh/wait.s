/ C library -- wait     /* modified - does not call nargs */

/ pid = wait(&status);
/
/ pid == -1 if error
/ status indicates fate of process, if given

.globl  _wait, _waita, cerror

_waita:
_wait:
	mov	r5,-(sp)
	mov	sp,r5
	sys	wait
	bec	1f
	tst	(sp)+
	jmp	cerror
1:
	mov	r1,*4(r5)	/ status return
	mov	(sp)+,r5
	rts	pc
