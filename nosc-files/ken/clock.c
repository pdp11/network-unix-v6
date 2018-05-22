#
#include "param.h"
#include "systm.h"
#include "user.h"
#include "proc.h"

#define	UMODE	0170000
#ifdef FASTIME
extern int fastime;
#endif FASTIME

/*
 * clock is called straight from
 * the real time clock interrupt.
 *
 * Functions:
 *	reprime clock
 *	copy *switches to display
 *	implement callouts
 *	maintain user/system times
 *	do most of the work for alarm clock mechanism
 *	maintain date
 *	profile
 *	lightning bolt wakeup (every 4 sec)
 *	alarm clock signals
 *	jab the scheduler
 */
clock(dev, sp, r1, nps, r0, pc, ps)
{
	register struct callo *p1, *p2;
	register struct proc *pp;
	int a;

	/*
	 * restart clock
	 */

	*lks = 0115;

#ifdef FASTIME
	fastime++;
#endif FASTIME

	/*
	 * display register
	 */

	display();

	/*
	 * callouts
	 * if none, just return
	 * else update first non-zero time
	 */

	if(callout[0].c_func == 0)
		goto out;
	p2 = &callout[0];
	while(p2->c_time<=0 && p2->c_func!=0)
		p2++;
	p2->c_time--;

	/*
	 * if ps is high, just return
	 */

	if((ps&0340) != 0)
		goto out;

	/*
	 * callout
	 */

	spl5();
	if(callout[0].c_time <= 0) {
		p1 = &callout[0];
		while(p1->c_func != 0 && p1->c_time <= 0) {
			(*p1->c_func)(p1->c_arg);
			p1++;
		}
		p2 = &callout[0];
		while(p2->c_func = p1->c_func) {
			p2->c_time = p1->c_time;
			p2->c_arg = p1->c_arg;
			p1++;
			p2++;
		}
	}

	/*
	 * lightning bolt time-out
	 * and time of day
	 */

out:
	if((ps&UMODE) == UMODE) {
#ifdef DPTIME		/* double precision time keeping*/
		if (++u.u_utime[1] == 0)	/* JSK */
			u.u_utime [0]++;	/* JSK */
#endif DPTIME
#ifndef DPTIME
		++u.u_utime;
#endif not DPTIME
		if(u.u_prof[3])
			incupc(pc, u.u_prof);
	} else
#ifdef DPTIME		/*double precision time keeping*/
		if (++u.u_stime[1] == 0)	/* JSK */
			u.u_stime[0]++;	/* JSK */
#endif DPTIME
#ifndef DPTIME
		++u.u_stime;
#endif not DPTIME
	pp = u.u_procp;
	if(++pp->p_cpu == 0)
		pp->p_cpu--;
	if(++lbolt >= HZ) {
		if((ps&0340) != 0)
			return;
		lbolt =- HZ;
		if(++time[1] == 0)
			++time[0];
		spl1();
		if((time[1]&03) == 0) {
			runrun++;
			wakeup(&lbolt);
		}
		for(pp = &proc[0]; pp < &proc[NPROC]; pp++)
		if (pp->p_stat) {
			if(pp->p_time != 127)
				pp->p_time++;
			/*
			 * code modifed 31Oct77 by JSK/RJB to
			 * make sleep faster
			 */
			if(pp->p_clktim)			/* ALARM */
				if(--pp->p_clktim == 0) {	/* ALARM */
					/*
					 * &pp->p_clktim is of mode ptr-to-int,
					 * so before it can be compared to
					 * pp->p_wchan (an int), the C compiler
					 * insists upon widening the latter
					 * before the compare.  In the new V7
					 * compiler we will be able to cast it,
					 * but for now, we will have to make do
					 * with this crock.  (a is also an int.)
					 */
/* JSK/RJB */				if (pp->p_wchan == (a = &pp->p_clktim))
/* JSK/RJB */					wakeup (a);
/* JSK/RJB */				else
					psignal(pp, SIGCLK);	/* ALARM */
				}
			/* 8/10 approx (255-HZ)/255 */
			a = (pp->p_cpu & 0377)*8/10;
			if(a < 0)
				a = 0;
			if(a > 255)
				a = 255;
			pp->p_cpu = a;
			if(pp->p_pri > PUSER)
				setpri(pp);
		}
		if(runin!=0) {
			runin = 0;
			wakeup(&runin);
		}
		if((ps&UMODE) == UMODE) {
			u.u_ar0 = &r0;
			if(issig())
				psig();
			setpri(u.u_procp);
		}
	}
}

/*
 * timeout is called to arrange that
 * fun(arg) is called in tim/HZ seconds.
 * An entry is sorted into the callout
 * structure. The time in each structure
 * entry is the number of HZ's more
 * than the previous entry.
 * In this way, decrementing the
 * first entry has the effect of
 * updating all entries.
 */
timeout(fun, arg, tim)
{
	register struct callo *p1, *p2;
	register t;
	int s;

	t = tim;
	s = PS->integ;
	p1 = &callout[0];
	spl7();
	while(p1->c_func != 0 && p1->c_time <= t) {
		t =- p1->c_time;
		p1++;
	}
	p1->c_time =- t;
	p2 = p1;
	while(p2->c_func != 0)
		p2++;
	while(p2 >= p1) {
		(p2+1)->c_time = p2->c_time;
		(p2+1)->c_func = p2->c_func;
		(p2+1)->c_arg = p2->c_arg;
		p2--;
	}
	p1->c_time = t;
	p1->c_func = fun;
	p1->c_arg = arg;
	PS->integ = s;
}
