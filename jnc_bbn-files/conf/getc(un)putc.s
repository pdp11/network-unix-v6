/ =========================== C L I S T ============================== /
/
/ These routines manipulate character queues of the form declared in
/ tty.h. putc(char, queue) puts a char on the tail of the queue;
/ getc(queue) returns a char from the head of the queue. unputc(queue)
/ returns a char from the tail of the queue, that is, the last char
/ putc'ed.
/
/ This implementation differs from the standard one in that
/ in addition to handling char values from 0 to 255, it can also
/ handle values from -2 to -256. They are represented internally
/ with a 255 byte as prefix. A plain 255 byte is represented as
/ 255 255. (Note that -1 comes out as 255.) These additional values ease the
/ task of providing a true 8-bit path in the driver.
/ putc, unputc, getc are NOT significantly less efficient in the normal case.

.globl	_cfreelist

/ --------------------------- P U T C ----------------------------- /
/
/ putc(char, queue_ptr) puts the char (actually an int) on the specified
/ queue. It returns 0 if it was successful, a nonzero value if it was not.
/
.globl _putc
_putc:
	mov	2(sp),r0    / r0 := char
	mov	4(sp),r1    / r1 := q_ptr
/ save environment
	mov	PS,-(sp)
	mov	r2,-(sp)
	mov	r3,-(sp)

	spl	5	    / protect
	cmp	r0,$377     / is it >= 0377 (unsigned)?
	bhis	esc_put     / yes.
	jsr	pc, putc    / no, do it normally
	br	rest	    / done
esc_put:
	mov	$377,r0     / yes. Load escape char
	jsr	pc, putc    / enter it
	mov	10(sp),r0   / get char again
	jsr	pc, putc    / enter it
	beq	rest	    / if it worked, done
/ Here \377 entered, but char did not. Remove \377 and report failure.
	jsr	pc, unputc
/ The \377 will serve as a nonzero return value.

/ restore environment
rest:
	mov	(sp)+,r3
	mov	(sp)+,r2
	mov	(sp)+,PS
	rts	pc

putc:	 / char in r0, q_ptr in r1; clobbers r2, r3
	mov	4(r1),r2	/ r2 := c_cl
	bne	1f

/ first time, get clist block

	mov	_cfreelist,r2	    / r2 := &clist
	beq	9f		    / zero? moby loss
	mov	(r2),_cfreelist     / unchain clist
	clr	(r2)+		    / clear its ptr, r2++
	mov	r2,2(r1)	    / c_cf := r2 (incremented)
	br	2f

/ not first time. Check if c_cl points just after last char position in block

1:
	bit	$7,r2		    / need new block?
	bne	2f		    / if nonzero, no.
	mov	_cfreelist,r3	    / else r3 := &clist
	beq	9f		    / zero? moby loss
	mov	(r3),_cfreelist     / unchain clist block
	mov	r3,-10(r2)	    / make current clist chain ptr -> new block
	mov	r3,r2
	clr	(r2)+		    / and zero new block's chain ptr
2:

/ here r2 finally points at a nice place to put a char

	movb	r0,(r2)+	    / put in char
	mov	r2,4(r1)	    / update c_cl
	inc	(r1)		    / and c_cc
	clr	r0		    / return zero
	rts	pc

/ moby loss, no free storage

9:
	mov	pc,r0		    / return nonzero value
	rts	pc

/ --------------------------- G E T C ----------------------------
/
/ getc -- get a char from a clist
/
/ use: c = getc(queue_ptr)
/ returns -1 if empty

.globl _getc

_getc:
	mov	PS,-(sp)
	mov	r2,-(sp)
	spl	5
	jsr	pc,getc 	/ get char
	cmp	r0,$377 	/ escape char?
	bne	done		/ no, all done
	jsr	pc,getc 	/ yes, get following char
	cmp	r0,$377 	/ also escape?
	beq	done		/ if so, all done
	bis	$!377,r0	/ if not, must be negative value
done:
	mov	(sp)+,r2
	mov	(sp)+,PS
	rts	pc		/ and leave

getc:	 / q_ptr at 10(sp); clobbers r2
	mov	10(sp),r1	/ r1 := q_ptr
	mov	2(r1),r2	/ r2 := c_cf
	beq	9f		/ empty
	movb	(r2)+,r0	/ r0 := char
	bic	$!377,r0	/ undo sign-extension
	mov	r2,2(r1)	/ update c_cf
	dec	(r1)+		/ update c_cc
	bne	1f
/ here was last block
	clr	(r1)+		/ zero c_cf
	clr	(r1)+		/ zero c_cl
	br	2f		/ go put block on cfreelist

1:
	bit	$7,r2		/ end of block?
	bne	3f		/ nope
	mov	-10(r2),(r1)	/ yes, update c_cf to point to next block
	add	$2,(r1) 	/ actually to first char of that block
/ here free block
2:
	dec	r2		/ get back into current block
	bic	$7,r2		/ make pointer to base of block
	mov	_cfreelist,(r2) / chain it onto free list
	mov	r2,_cfreelist
/ here leave, all OK
3:
	rts	pc

/ no char to give!

9:
	clr	4(r1)		/ zero c_cl???
	mov	$-1,r0		/ return -1
	rts	pc

/ --------------------------- U N P U T C --------------------------- /
/
/ char = unputc(queue_ptr) is like getc, but gets char from tail of
/ queue instead of head, thus undoing last putc.
/ Modified by BBN:Dan Franklin for escape byte processing.

.globl _unputc

_unputc:
	mov PS, -(sp)	    / up priority
	spl 5
	mov r2, -(sp)	    / save r2
	mov 6(sp), r1	    / r1 := q_ptr
	jsr pc, unputc	    / get last char
	mov r0, r2
	blt finish	    / if neg, must be empty
/ here we got a char. Peek at previous to see if it's escape.
	mov 6(sp), r1	    / r1 := q_ptr
	mov 4(r1), r1	    / r1 := c_cl
	beq finish	    / if zero, there is no previous char
	cmpb -(r1),$377     / is previous char escape byte?
	bne finish	    / nope, go away
/ here previous was escape byte. Remove it and set high byte of char.
	mov 6(sp), r1	    / r1 := q_ptr
	jsr pc, unputc	    / else get it for real
	mov r2, r0
	cmp $377, r0
	beq finish	/ \377 \377 maps into \377, not -1
	bis $!377, r0
finish:
	mov (sp)+, r2
	mov (sp)+, PS
	rts pc

/ unputc: qp in R1, char left in R0; clobbers r1
unputc: 			/ Mike Patrick, June 76
	mov	r2,-(sp)	/ save regs
	mov	r3,-(sp)

	mov	4(r1),r2	/ c_cl into r2
	beq	5f		/ if 0, empty queue
	dec	r2		/ Last char put in
	movb	(r2),r0 	/ char into r0
	bic	$!377,r0	/ undo DEC braindamage
	dec	(r1)+		/ decrement count, advance r1
	beq	4f		/ if zilch, empty queue now
	bit	$5,r2		/ check if emptied block
	bne	3f		/ if not 010b, return. (can't be 000b)
	bic	$7,r2		/ point to c_next
	mov	_cfreelist,(r2) / put empty block on freelist
	mov	r2,_cfreelist
	mov	(r1),r3 	/ c_cf into r3
	bic	$7,r3		/ ptr to next block down the line
1:	cmp	(r3),r2 	/ block preceding c_cl's ?
	beq	2f
	mov	(r3),r3 	/ nope, move down the line
	br	1b
2:				/ yep
	clr	(r3)		/ end of list
	mov	r3,r2
	add	$10,r2		/ r2 now points past last char put in
3:
	mov	r2,2(r1)	/ New c_cl
	br	9f		/ return

4:	clr	(r1)+		/ c_cf zeroed
	clr	(r1)+		/ c_cl	 "
	bic	$7,r2
	mov	_cfreelist,(r2) / put block on freelist
	mov	r2,_cfreelist
	br	9f		/ return

5:	mov	$-1,r0		/ error condition

9:
	mov	(sp)+,r3	/ restore state
	mov	(sp)+,r2
	rts	pc		/ bye!
