#
/*
 * The table system call.  Seperated out of sys1.c because of all the
 * odd .h files (in particular netparam.h) included.  Avoids having to
 * do too much compilation.  jsq bbn 2-29-79
 *
 * This is a slight modification of Rand's table system call.
 */

#include "../h/param.h"
#include "../h/netparam.h"
#include "../h/systm.h"
#include "../h/user.h"
#include "../h/proc.h"
#include "../h/buf.h"
#include "../h/reg.h"
#include "../h/inode.h"
#include "../h/file.h"    /* rand:bobg for eofp call */
#include "../h/text.h"  /* for "table" call mob:04198 */
#include "../h/contab.h"/* for "table" call mob:04198 */
#include "../h/rawnet.h"

/*
 * data table for "table" system call.
 */
int null_tab;
struct s_tab {
	int *tb_addr;
	int tb_size;
} tables[] {
	&proc,          sizeof proc,            /* 0 */
	&text,          sizeof text,            /* 1 */
	&u,             sizeof u,               /* 2 */
	&file,          sizeof file,            /* 3 */
	&inode,         sizeof inode,           /* 4 */
	&mount,         sizeof mount,           /* 5 */
#ifdef NCP
	&r_contab,      sizeof r_contab,        /* 6 */
	&w_contab,      sizeof w_contab,        /* 7 */
	&file_tab,      sizeof file_tab,        /* 8 */
#endif NCP
#ifndef NCP
	&null_tab,      sizeof null_tab,        /* 6 */
	&null_tab,      sizeof null_tab,        /* 7 */
	&null_tab,      sizeof null_tab,        /* 8 */
#endif NCP
#ifdef RMI
	&r_rawtab,      sizeof r_rawtab,        /* 9 */
	&w_rawtab,      sizeof w_rawtab,        /* 10 */
#endif RMI
#ifndef RMI
	&null_tab,      sizeof null_tab,        /* 9 */
	&null_tab,      sizeof null_tab,        /* 10 */
#endif RMI
	&netparam,      sizeof netparam,        /* 11 */
	&null_tab,      sizeof null_tab,        /* in case you want to */
						/* compare others */
};

/*
 * table system call.
 * Destined to replace "gprocs".
 * Returns size of system tables and
 * (optionally) copies the table out to
 * a user array.  Useful for measurement.
 * Returns address of table in r1.  jsq bbn 2-9-79.
 */
table()
{
	register int n, p;
	register struct s_tab *tp;
	if ((n = u.u_ar0[R0]) < 0 ||
	    n >= (sizeof tables)/(sizeof tables[0])) {
		u.u_error = EINVAL;
		return;
	}

	tp = &tables[n];
	if (p = u.u_arg[0])
		if(copyout(tp->tb_addr, p, tp->tb_size) == -1) {
			u.u_error = EIO;
			return;
		}
	u.u_ar0[R0] = tp->tb_size;
	u.u_ar0[R1] = tp -> tb_addr;
}
