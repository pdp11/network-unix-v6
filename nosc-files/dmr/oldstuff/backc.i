
/* get defines of various types */
/*
 * tunable variables
 */




















/*
 * Conditional compile switches
 */


/* We don't have floating point....
#define FLOATPT		/* floating point unit present on system */




/*#define VDHSYS	$ /*do this if you want a Very Distant Host kernel*/

/* no parallel swapping for the time being ....
#define NSWBUF	2	/* number of buffer headers for swapping */





/* Turn off for testing without. --KLH
#define UCBUFMOD	/* include the U.Calgary buffer modifications */
/* no high-resolution timing needed....
#define FASTIME		/* maintain line-frequency timer */
/*#define RKDUMP    	/* use RK0 swap area for dump device */
/*#define NODUMP    	/* don't include code to dump core */
/* no accounting for the time being....
#define ACCTSYS		/* Include U.Illinois accounting */
/* actually the next define should be in user.h, but since we don't have
 * recursive includes, we can't expect the ACCSYS definition before the
 * if test ...
 */




/*
 * priorities
 * probably should not be
 * altered too much
 */









/*
 * signals
 * dont change
 */






















/*
 * fundamental constants
 * cannot be changed
 */







/* don't include this stuff in assembly modules */
/*
 * structure to access an
 * integer in bytes
 */






/*
 * structure to access an integer
 */





/*
 * Certain processor registers
 */




/*
 * Waiting for version 7
 */




	.globl	_backc		/ backs up one character in the queue
				/ it's a mildly expensive operation -- use
				/ with care

PS	= 177776
.globl	_cfreelist




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

	bis	$340,PS		/ spl5()
	bic	$100,PS









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





	mov	(sp)+,PS
	mov	(sp)+,r3	/ restore regs
	mov	(sp)+,r2
	rts	pc		/ return (c)
