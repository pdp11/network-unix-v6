
/ read rl01

rlcs = 174400
rlba = 2
rlda = 4
rlmp = 6
rhcom = 10
scom = 6
rcom = 14

rblk:
	mov	dska,r1
	mov	$6.,r3
	clr	r0
	div	$20.,r0
	ash	r3,r0
	asl	r1
	bis	r1,r0
	mov	r0,-(sp)
	mov	r0,r1
	bic	$!100,r0
	asr	r0
	asr	r0
	mov	r0,-(sp)
	bic	$177,r1
	mov	r1,-(sp)
	mov	$rhcom,r0
	jsr	pc,rlexec
	mov	(r1),r0
	bic	$177,r0
	sub	(sp)+,r0
	bcc	1f
	neg	r0
	bis	$4,(sp)
1:
	inc	r0		/ set mark bit
	bis	(sp)+,r0
	mov	r0,-(r1)
	mov	r3,r0
	jsr	pc,rlexec
	mov	wc,(r1)
	mov	(sp)+,-(r1)
	mov	ba,-(r1)
	mov	$rcom,-(r1)
	br	1f

rlexec:
	mov	$rlcs,r1
	mov	r0,(r1)
1:
	tstb	(r1)
	bpl	1b
	add	r3,r1
	rts	pc
