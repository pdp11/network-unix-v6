/ C Library -- send host-to-host reset
/ f = net_reset("<hostname>")
/ f = -1 means error host file does not exist

net_reset = 51.
.globl	_net_reset

_net_reset:
	mov	r5,-(sp)
	mov	sp,r5
	mov	4(r5),0f
	sys	0; 2f
	bec	1f
	mov	$0177777,r0
1:
	mov	(sp)+,r5
	rts	pc
.data
2:
	sys	net_reset; 0:..
