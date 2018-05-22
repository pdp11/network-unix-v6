#define ASSEMBLY	/* so C code will not be defined */
#include "../../h/param.h"	/* get defines of various types */

	.globl	_backc		/ backs up one character in the queue
				/ it's a mildly expensive operation -- use
				/ with care

PS	= 177776
.globl	_cfreelist
#ifdef UCBUFMOD
KISA6	= 172354
.globl	_clistp, _cu
#endif UCBUFMOD
_backc:		/ backc(arg)

	/ first check to see if the queue is empty -- if so, return a
	/ minus one to indicate that fact

	mov	2(sp),r1	/ q = arg
	tst	(r1)		/ if(q->count == 0)
	bne	1f
	mov	$-1,r0		/ return (-1)
	rts	pc

	/ so there is at least one character in the queue.  since we will
	/ muck around with the list, we arrange for indivisibility and get
	/ the last character (the tail pointer points one past the last
	/ character in the queue).

1:
	mov	r2,-(sp)	/ save regs
	mov	r3,-(sp)
	mov	PS,-(sp)	/ save old priority
#ifndef UCBUFMOD
	bis	$340,PS		/ spl5()
	bic	$100,PS
#endif not UCBUFMOD
#ifdef UCBUFMOD
	mov	r4,-(sp)
	mov	KISA6-40,-(sp)
	mov	KISA6,r4
	bis	$340,PS		/ spl7()
	mov	_clistp,KISA6	/ map in clist area
	mov	$77406,KISA6-40
#endif UCBUFMOD
	mov	4(r1),r2	/ c = *(t = --q->tail)&0377
	movb	-(r2),r0
	mov	r2,4(r1)
	bic	$!377,r0

	/ decrement the queue counter and check to see if we removed the last
	/ character.  if we did, zap the queue pointers and arrange
	/ for the return of the queue element to the free list.

	dec	(r1)+		/ if(--q->count)
	bne	5f
	clr	(r1)+		/ q->head = 0
	clr	(r1)+		/ q->tail = 0
	bic	$7,r2		/ t =& not 7
	br	6f

	/ check to see if we just removed the first character in this queue
	/ element.  if we did, we must find its predecessor (which must exist,
	/ since there is at least one other character in the queue).  if not,
	/ we can just peaceably go away.


5:
	mov	r2,r3		/ if((t&7) == 2)
	bic	$!7,r3
	cmp	$2,r3
	bne	4f

	/ expensive part -- must search queue from front to find element
	/ linked to this one -- it will become new tail

	/ fix the pointers so they point at the beginning  of the q element.


	bic	$7,r2		/ t =& not 7
	mov	(r1),r3		/ p = (q->head & not 7)
	bic	$7,r3

	/ search down the queue until the predecessor is found

2:
	cmp	(r3),r2		/ if(p->link != t)
	beq	3f
	mov	(r3),r3		/ p = p->link
	bne	2b		/ loop if not end-of-list
	br	.		/ error, should not find it

	/ fixup the end of the queue by making the last element point to
	/ null and the tail pointer point past the last character in that
	/ element.

3:
	clr	(r3)		/ p->link = 0
	add	$10,r3		/ q->tail = ++p
	mov	r3,2(r1)

	/ return the released queue element to the free list

6:
	mov	_cfreelist,(r2)	/ t->link = cfreelist
	mov	r2,_cfreelist	/ cfreelist = t

	/ that's all -- return the character we removed to the caller.

4:
#ifdef UCBUFMOD
	mov	r4,KISA6	/ map _u back in
	mov	(sp)+,KISA6-40
	mov	(sp)+,r4
#endif UCBUFMOD
	mov	(sp)+,PS
	mov	(sp)+,r3	/ restore regs
	mov	(sp)+,r2
	rts	pc		/ return (c)
