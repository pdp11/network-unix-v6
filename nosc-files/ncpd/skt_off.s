/	skt_off.s
/
/name:
/	skt_off
/
/function:
/	computes new socket number given a socket number, an offset, and
/	a number of bytes.
/
/algorithm:
/	r0 is loaded with the address of the last+1 byte of where the
/	new socket number is to go.
/	r1 is loaded with the address of the last+1 byte of the given
/	socket number.
/	r3 is loaded with the increment.
/	r4 is loaded with the number of bytes.
/	goto enter.
/	loop:
/		swap bytes of r2 to get carry into low byte.
/		move carry into r3 as new increment.
/	enter:	(come here to enter loop on 1st iteration).
/		move a byte by autodecrement fetch thru r1 to r2 (actually
/		or in the byte to a cleared register to avoid sign
/		extension).
/		add r3 (the increment) to r2.
/		move byte from r2 thru autodecremented r0.
/		decrement r4 by 1 and go to loop if non-zero.
/	return.
/
/calling sequence from C:
/	skt_off(src,dest,incr,size);
/	char	*src,dest;
/	int	incr,size;
/
/parameters:
/	src	pointer to last+1 byte of existing socket number.
/	dest	pointer to last+1 byte of where new socket number is to go.
/	incr	amount to be added to existing socket number to produce
/		new socket number.
/	size	size of socket numbers in bytes.
/
/called by:
/	sm_dux
/	kr_odrct
/	fi_rfnm
/
/history:
/	initial coding 1/2/75 by G. R. Grossman.
/
/
.globl	_skt_off,csv,cret
/
_skt_off:
	jsr	r5,csv		/save registers, set up r5
	mov	6(r5),r0	/dest
	mov	4(r5),r1	/src
	mov	10(r5),r3	/incr
	mov	12(r5),r4	/size
	br	1f		/go enter loop
2:				/top of loop
	swab	r2		/get carry
	movb	r2,r3		/isolate it as new incr
1:				/enter loop here on 1st iteration
	clr	r2		/so we can...
	bisb	-(r1),r2	/...or in a byte from source to avoid sign 
				/extension.
	add	r3,r2		/add current increment
	movb	r2,-(r0)	/store in dest
	sob	r4,2b		/decrement count and loop if > 0
	jmp	cret		/return
/
