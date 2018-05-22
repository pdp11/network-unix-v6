.text
.globl	_swabuf
_swabuf:
	mov	2(sp),r1
	mov	4(sp),r0
1:
	swab	(r1)+
	sob	r0,1b
	rts	pc

.globl	_spl_imp, _spl7
_spl_imp:
	jmp	_spl7
