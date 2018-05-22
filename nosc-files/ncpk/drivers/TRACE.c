entries = 200.

/	TRACE -- keep a trace table of places executed

.globl	_TRACE, _TR_OFF, TR_TBL, TR_MAX, TR_SW

PS	=	177776		/ Processor Status word

_TRACE:
	tst	TR_SW		/ Is tracing enabled?
	beq	2f		/ Jump if not
	mov	PS,r1		/ Save current processor priority
	bis	340,PS		/ Lock out interrupts
	mov	TR_MAX,r0	/ Get index into circular queue
	beq	0f		/ Skips the first one, but that's OK
	mov	2(sp),-(r0)	/ Put calling parameter in next entry
	cmp	r0,$TR_TBL	/ End of circular queue?
	bne	1f		/ Jump if not
0:	mov	$TR_TBL+entries+entries,r0	/ If so, reset pointer
1:	mov	r0,TR_MAX	/ Save new queue pointer
	mov	r1,PS		/ Restore calling processor priority
2:	rts	pc		/ Return

_TR_OFF:
	mov	2(sp),-(sp)	/ Parameter to TRACE
	jsr	pc,_TRACE	/ Trace it
	clr	TR_SW		/ Stop tracing
	tst	(sp)+		/ Skip parameter
	rts	pc		/ Return

.bss
TR_TBL:	. = .+entries+entries	/ Space for table entries
TR_MAX:	. = .+2		/ End of table; contains queue pointer
.data
TR_SW:	1		/ Start with tracing on
