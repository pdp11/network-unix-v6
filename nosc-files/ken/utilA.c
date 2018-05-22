#
#include "../../h/param.h"
#include "../../h/proc.h"
#include "../../h/user.h"


struct procq Free_proc
{
	1,0,0		/* init for one person */
};

struct procq Run_proc
{
	1,0,0		/* init for one person */
};

struct procq Swap_ready
{
	1,0,0		/* init for one person */
};

/*	Q E N T E R	--	Enter entry into the singly linked ring
				the qhead points to the last in the queue.

*/

qenter( queue,entry )
char *queue;
char *entry;
{

	register char *q;	/* same as queue */
	register char *ent;	/* same as entry */
	register int  sps;	/* save ps */

	ent = entry;
	q   = queue;

	sps = PS->integ;
	spl6();
	if( q->qlink )			/* anything in queue */
	{
		/* point entry at first thing in queue */
		ent->qlink = (q->qlink)->qlink;
		/* point old last and qhead at new last */
		q->qlink = (q->qlink)->qlink = ent;
	}
	else
		/* just point qhead and entry at entry */
		ent->qlink = q->qlink = ent;
	PS->integ = sps;
}

/*
	Q D E L I N K	--	take the first entry out of a singly linked
				ring, last entry is pointed to by qhead

*/

qdelink( queue )
char *queue;
{
	register char *q;	/* same as queue */
	register char *entry;	/* will point to the entry or zero if nothing */
	register int  sps;	/* saved ps */

	q = queue;

	sps = PS->integ;
	spl6();
	if( q->qlink )		/* something in queue */
	{
		/* we emptying it */
		if( (entry = (q->qlink)->qlink) == q->qlink )
			q->qlink = 0;		/* set qhead to zero */
		else
			/* point last at new first */
			(q->qlink)->qlink = entry->qlink;
	}
	else		/* nothing there so just return zero */
		entry = 0;
	PS->integ = sps;

	return( entry );
}

/*

	P E E		--	pee a semaphore word passed is address of
				a semaphore, count field is decremented, if
				less than zero, process is blocked, until
				someone does a vee on same semaphore 
*/

pee( sem )
char *sem;
{

	register char *s;	/* addr of semaphore */

	s = sem;
	if( (--s->sem_count) < 0 )	/* dec count do we block */
	{
		(u.u_procp)->p_stat = SWAIT;	/* yes say waiting */
		qenter( &s->sem_q,u.u_procp );	/* wait on sem queue */
		swtch();			/* give up processor */
	}
}

/*	V E E		--	Used to unblock pieces of critical code, the
				semaphore count is incremented, if it is
				leq zero, then there is someone waiting.
				delink them from the queue, and start them
				running.

*/
vee( sem )
char *sem;
{

	register char *s;	/* address of the semaphore */

	s = sem;

	if( (++s->sem_count) <= 0 )	/* did we unblock someone */
		setrun( qdelink( &s->sem_q ));	/* release first person in queue */
}

/*
	C Q E N T E R	--	controlled queue enter, does a pee before
				doing a qenter, and a vee when done.

*/

cqenter( queue,entry )
char *queue;
char *entry;
{
	register struct procq *p;	/* address of a procq */

	p = queue;
	pee( &p->sem_count );			/* get the process queue */
	qenter( &p->qhead,entry );	/* stick entry in */
	vee( &p->sem_count );			/* give up the queue */
}

/*
	C Q D E L I N K	--	sequel to cqenter
*/

cqdelink( queue )
char *queue;
{
	register struct procq *p;
	register struct proc  *procp;	/* address of process delinked */

	p = queue;
	pee( &p->sem_count );			/* get the process queue */
	procp = qdelink( &p->qhead );	/* get first from queue */
	vee( &p->sem_count );			/* say its ok again */
	return( procp );		/* return process address */
}
