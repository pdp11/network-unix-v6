/	C library ftime -- high-resolution real-time clock for timing things

	.globl	_ftime
_ftime:
	sys	60.	/ invoke kernel function -- returns value in r0
	rts	pc	/ so that's all
