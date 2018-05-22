#
#include "../../h/param.h"
#include "../../h/proc.h"
#include "../../h/user.h"

/*
	P R I Q E N T E R	--	enter a process in a process queue
					according to decending priority
*/

priqenter( queue,entry )
struct procq *queue;
struct proc  *entry;
{

	register struct procq *p;	/* address of process queue */
	register char *q;		/* addess of a queue entry */
	register char pri;		/* pri of process to be entered */

	p = queue;

	/* get the queue */
	pee( &p->sem_count );

	if( p->qhead == 0 )		/* if q empty just put at beginning */
		qenter( &p->qhead,entry );
	else				/* search for entry with pri greater than pri */
	{
		q = &p->qhead;		/* point to qhead */
		pri = entry->p_pri;	/* get priority into fast storage */
		do
		{
			q = q->qlink;	/* get next entry */
			if( (q->qlink)->p_pri < pri )
				break;	/* found on exit loop */
		}
		while( q != p->qhead );	/* while not last */

		/* link into queue */
		entry->qlink = q->qlink;	/* take aboves next */
		q->qlink = entry;		/* make aboves next me */
		if( q == p->qhead )		/* did we add to the end */
			p->qhead = entry;	/* then point qhead at me */
	}
	vee( &p->sem_count );				/* release the process list */
}

/*
 *	This will reschedule the processor, arrangements
 *	for restarting the process must be made elsewhere
 */
give_up_processor( priority )
{
	register struct proc *p;
	register int sps;

	p = u.u_procp;
	sps = PS->integ;	/* save processor pri */
	spl6();
	p->p_pri  = priority;
	p->p_stat = SSLEEP;
	spl0();
	swtch();
	PS->integ = sps;	/* put the guy back */
}
