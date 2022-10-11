#
#include "../h/param.h"
#include "../h/systm.h"
#include "../h/user.h"
#include "../h/proc.h"
#include "../h/reg.h"
#include "../h/file.h"
#include "../h/inode.h"

#ifdef LCBA

/*
 * Process group definition for get_lcba exclusion;
 * also flag for ifdef of code for above.
 */
#define EXCGRP  u.u_procp -> p_pgrp

set_lcba(size)
char *size;
{               /* mallhi instead of malloc (see malloc.c) to force lcba */
    register struct proc *p;    /* to top of core, making set_lcba's easier */
    register char *a;
    register char *s;

    if (lcba_size) mfree (coremap, lcba_size, lcba_address);/*free old space*/

    s = size;           /* get new size */
    if (s == 0) {
        lcba_size = 0;
        return(1);
    }
    a = mallhi (coremap, s);        /* attempt to get space */
    if ( a == 0) {  /* not enough easy core */
            for (p = &proc[0]; p < &proc[NPROC]; p++) {
                if (((p -> p_flag & (SSYS | SLOCK | SLOAD)) == SLOAD) &&
                  ((p -> p_stat == SWAIT) || (p -> p_stat == SSTOP))) {
                    if (p == u.u_procp) continue;   /* don't swap self */
                    spl0();
                    p -> p_flag =& ~SLOAD;
                    xswap (p, 1, 0);
		    a = mallhi (coremap, s);
                    if (a != 0) break;/*if we've got the room, stop swapping*/
                }
            }
            if (a == 0) {
		if ((a = mallhi (coremap, lcba_size)) == 0) {
                        lcba_size = 0;
			u.u_error = ENOSPC;
                } else {
                        lcba_address = a;
                        u.u_error = ENOMEM;
                }
		return(0);
            }
    }
    lcba_size = s;
    lcba_address = a;
    for (s =+ a; a < s; a++) clearseg (a);  /* clear new lcba */
    return(1);
}

/*
 * The following are subroutines because of
 * the double use of the code of each.
 */

get_lcba()
{
#ifdef  EXCGRP
static  int lcba_exc;   /* process group currently owning lcba */
#endif
        spl6();
#ifdef EXCGRP
        if (lcba_exc != EXCGRP) {
                if (lcba_count) {
                        u.u_error = EPERM;
                        spl0();
                        return(0);
                } else {
                        lcba_exc = EXCGRP;
                }
        }
#endif
        if (u.u_lcbflg == 0) {
                u.u_lcbflg = 1;
                lcba_count++;
        }
        spl0();
        return(1);
}

rel_lcba()
{
        spl6();
	if (u.u_lcbflg) {
		lcba_count--;
		u.u_lcbflg = 0;
	}
        spl0();
        return;
}

/*
 * There's too much junk in-line already, so put this here.
 */
lim_page() {
	register *arg, r, i;
	if (u.u_minlcb || u.u_mintlcb) {
		u.u_error = EBUSY;
		return;
	}
	arg = u.u_arg[0];
	r = fuword (arg);
	i = fuword (++arg);
	if ((r < 0) || (r > i) || (i > 7)) {
		u.u_error = EINVAL;
		return;
	}
	if ((r == 0) && (i == 7)) {     /* turn off cache limits */
		u.u_clmin = u.u_clmax = 0;
	} else {                        /* else turn them on */
	    if (u.u_sep) {
		u.u_clmin = r + 8;
		u.u_clmax = i + 9;
	    } else {
		u.u_clmin = r;  /* make both data pages and increment */
		u.u_clmax = i + 1;      /* this one for convenient use */
	    }
	}
	u.u_ar0[R0] = 0;
	return;
}
#endif  /* on LCBA */
