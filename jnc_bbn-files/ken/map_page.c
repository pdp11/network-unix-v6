#
#include "../h/param.h"
#include "../h/systm.h"
#include "../h/user.h"
#include "../h/reg.h"
#include "../h/file.h"
#include "../h/inode.h"
#include "../h/seg.h"

#ifdef LCBA

char *lcba_address 128*16;     /* UNIXMIN: left operand is K bytes */
/*
 * UNIXMIN is the minimum amount of memory in core clicks left
 * for non-kernel and non-lcba Unix; lcba_address is a convenient
 * place for main to look at it in initializing lcba_max.
 */

/*
 * map_page system call:
 * includes lim_page, rel_lcba, get_lcba,
 * rel_all, set_lcba, max_lcba, siz_lcba,
 * rel_page, map_page.
 * jsq BBN Tue Jul 25 19:28:56 EDT 1978
 */

/*
 *  Flags for lcba_fs.
 *
 *  The synchronization is an odd solution to the reader-writer
 *  problem, where set_lcba's are writers and exclude all lcba calls
 *  and other lcba calls are readers and are not exclusive except
 *  with set_lcba's.
 */
#define FS_B    1       /* doing set_lcba */
#define FS_W    2       /* waiting on other set_lcba */
#define FS_X    4       /* waiting on other lcba call */

map_page()
{
        register r, *arg, *t;
        int mode; char *offset, *length;
        int i;

        static char lcba_fs;    /* get_lcba synchronization flags */
        static char lcba_fm;    /* synchronization count for other calls */

        r = u.u_ar0[R0];        /* get page from user r0 */

        if (r == -2) {  /* set_lcba */
                spl6();
                while (lcba_fs & FS_B) { /* wait for other set_lcba calls */
                        lcba_fs =| FS_W;
                        sleep (&lcba_fs, PWAIT);
                }
                lcba_fs = (FS_B | FS_X); /* wait for other lcba calls */
                while (lcba_fm) sleep (&lcba_fm, PSLEP);
                lcba_fs =& (~FS_X);
                spl0(); /* synchronized: all other lcba calls waiting on us */
                if (lcba_count) {       /* error: lcba in use */
                        u.u_error = EBUSY;
                } else {
                        length = u.u_arg[0];
                        if (length > lcba_max) { /* error: no core */
				u.u_error = EFBIG;
                        } else { /* do it */
                                if (set_lcba(length)) {
                                        u.u_ar0[R0] = 0;
                                }
                        }
                }
                spl6();
                lcba_fs = 0;    /* let somebody else have a turn */
                wakeup (&lcba_fs);      /* who cares if it's inefficient */
                spl0();         /* to do a wakeup on every set_lcba, */
                return; /* because how often is anybody going to use it? */
        }
        spl6();
        while (lcba_fs) sleep (&lcba_fs, PSLEP);/* wait for any set_lcba's */
        lcba_fm++;      /* increment count of other lcba calls in progress */
        spl0();
	if (r == -7) {  /* lim_page */
		lim_page();
		goto out;
	}
	if (r == -5) {  /* rel_lcba */
                if (u.u_minlcb || u.u_mintlcb) {
                        u.u_error = EBUSY;
                } else {
                        rel_lcba();
                        u.u_ar0[R0] = 0;
                }
                goto out;
        }
        if (r == -4) {  /* get_lcba */
                if(get_lcba()) u.u_ar0[R0] = 0;
                goto out;
        }
        if (r == -3) {  /* rel_all */
                t = &u.u_lcbmd[0];
                arg = &u.u_lcbma[0];
                while (t < &u.u_lcbmd[16]) *arg++ = *t++ = 0;
		u.u_mintlcb = u.u_minlcb = u.u_maxlcb = 0;
                u.u_ar0[R0] = 0;
                rel_lcba();
                goto out;
        }
	if (r == -6) {  /* max_lcba */
		u.u_ar0[R0] = lcba_max;
		goto out;
	}
        if (r == -1) {  /* siz_lcba */
                u.u_ar0[R0] = lcba_size;
                goto out;
        }
        /* rel_page or map_page */
	if ((r < 0) || (u.u_sep?(r > 15):(r > 7))) {/* page is out of bounds */
                u.u_error = EINVAL;
                goto out;
        }
        mode = r;       /* save page for goto out on successful rel_page */
        if (u.u_sep){if (r>7) r =- 8; else r =+ 8;} /* convert to internal form */
        arg = u.u_arg[0];       /* get arg */
        if (arg == 0) { /* rel_page */
                if (u.u_lcbmd[r]) {
                        u.u_lcbmd[r] = 0;
                        u.u_lcbma[r] = 0;
                        if (!u.u_sep) { /* Unix requires copies in data APFs*/
                                i = r + 8;
                                u.u_lcbmd[i] = 0;
                                u.u_lcbma[i] = 0;
                        }
                        u.u_ar0[R0] = mode;     /* goto out saved page number */
                }
                else {
                        u.u_error = EINVAL;
                        goto out;
                }
        }
        else{   /* map_page */

                /* the implicit get_lcba: */
                if (!get_lcba()) goto out;

                /* get other arguments from user space */
                mode = fuword (arg);
                offset = fuword (++arg);
                length = fuword (++arg);
                if (length == 0) { /* zero length is no good */
                        u.u_error = EINVAL;
                        goto out;
                }
                if (offset+length > lcba_size){ /* lcb runs off end of lcba */
                        u.u_error = ENOMEM;
                        goto out;
                }
                i = nseg(length);     /* segments required for lcb */
		if(((r & 07) + i) > 8){ /* lcb runs off end of I or D space */
			u.u_error = ENOSPC;
                        goto out;
                }
                i =+ r;
		if (u.u_clmax) {        /* check cache limits, if any */
			if ((r < u.u_clmin) || (i > u.u_clmax)) {
				u.u_error = EFBIG;
				goto out;
			}
		}
                for (t = &u.u_uisd[r]; t < &u.u_uisd[i]; t++){
                /* for each unix software descriptor register */
                        if (*t) {       /* if page is in use give error */
                                u.u_error = EBUSY;
                                goto out;
                        }
                }
                /* all possible errors have been checked for */
                u.u_ar0[R0] = nseg(length); /* goto out number of pages mapped */
                mode = mode?RO:RW;      /* convert mode to hardware form */

                /* install whole segments in software registers */
                t = &u.u_lcbmd[r];
                arg = &u.u_lcbma[r];
                while (length >= 128) {
                        *t++ = (127 << 8) | mode;
                        *arg++ = offset;
                        length =- 128;
                        offset =+ 128;
                }
                if (length) {   /* install any partial page remaining */
                        *t = ((length - 1) << 8) | mode;
                        *arg = offset;
                }
                if (!u.u_sep) { /* copy i into d space registers */
                        t = &u.u_lcbmd[r];
                        arg = &u.u_lcbmd[r+8];
                        while (t < &u.u_lcbmd[i]) *arg++ = *t++;
                        t = &u.u_lcbma[r];
                        arg = &u.u_lcbma[r+8];
                        while (t < &u.u_lcbma[i]) *arg++ = *t++;
                }
        }
        /* after body of rel_page or map_page, */
        /* recompute min and max lcb page and reset hardware registers */
        t = u.u_lcbmd;
        u.u_minlcb = 0;
        for (r = 8; r < 16; r++){
                if (t[r]){
                        u.u_minlcb = (r&07)+1;
                        break;
                }
        }
        u.u_maxlcb = 0;
        if (u.u_minlcb){
                for (r = 16; r > 8;){
                        if (t[--r]){
                                u.u_maxlcb = (r&07)+1;
                                break;
                        }
                }
        }
        u.u_mintlcb = 0;
        if (u.u_sep) {
                for (r = 0; r < 8; r++) {
                        if (t[r]) {
                                u.u_mintlcb = (r + 1);
                                break;
                        }
                }
        }
        sureg ();       /* install software in hardware registers */
out:
        spl6();
        lcba_fm--;
        if ((lcba_fm == 0) && (lcba_fs & FS_X)) {   /* if nobody's siezed */
                wakeup (&lcba_fm);      /* and set_lcba waiting, wake it up */
        }
        spl0();
        return;
}

#endif  /* on LCBA */
