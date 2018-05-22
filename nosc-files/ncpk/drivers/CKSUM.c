.globl _CKSUM,_TR_OFF,TR_SW,_etext
_CKSUM:
	tst	TR_SW		/ Is traceing disabled?
	beq	9f		/ If so, don't bother
	mov	$_etext,r1	/ Initalize
	clr	r0
1:
	add	-(r1),r0	/ Accumulate checksum
	tst	r1		/ Loop back if not to beginning of memory
	bne	1b

	cmp	r0,cksum	/ Does it match checksum?
	beq	9f		/ Jump out if match
2:
	tst	cksum		/ First time through?
	bne	3f		/ Jump if not
	mov	r0,cksum	/ Save calculated checksum
	rts	pc
3:
	mov	$4f,-(sp)	/ Set up param to TRACE
	jsr	pc,_TR_OFF	/ Do a TRACE and freeze traceing
	tst	(sp)+		/ Pop off param to TRACE
	clr	cksum		/ Reinitalize when restarted
9:
	rts	pc
.data
cksum:	0
4:	<CKSUM!\0>
