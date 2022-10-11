/*
 * TTY control package
 *
 * This set of routines establishes one or more "modes", each of which
 * is a tty structure. Routines are provided for allocating a new mode,
 * changing between modes, and setting and clearing the tty flags in a mode.
 * If the mode specified is the current mode, the tty structure is regarded as
 * being kept in the kernel itself; gtty, stty, and modtty are used on it.
 * Otherwise it is one of the allocated user-space structures.
 *
 * This package attempts to handle both old and new tty drivers, by
 * keeping both sets of modes around.
 *
 * A mode of NULL is regarded as a reference to the current mode.
 *
 * Note: the user must declare the external int "termfd" and set it to a file
 * descriptor opened to the terminal to be controlled. The statement
 *     int termfd 2;
 * external to any function, is sufficient.
 *
 * Eric Shienbrood(bbn) 16 Jul 81 - Added struct tchars to the TTYMODE
 *  structure.
 */

#include <stdio.h>
#include "ttyctl.h"

/* -------------------------- A L L O C M O D E --------------------- */
/*
 * AllocMode() returns a pointer to a new tty structure.
 * It is initialized to the current tty modes.
 */
TTYMODE *
AllocMode()
{
    TTYMODE *p;
    extern int termfd;
    extern char *malloc();

    p = (TTYMODE *)malloc(sizeof(*p));
    if (p == NULL)
	return(p);  /* Let the caller decide what to do */
    check("AllocMode", gtty(termfd, &(p->tm_sgtty)));
    check("AllocMode", ioctl(termfd, TIOCGETC, &(p->tm_tchars)));
    check("AllocMode", ioctl(termfd, TIOCLGET, &(p->tm_local)));
    return(p);
}

/* -------------------------- F R E E M O D E ----------------------- */

FreeMode(mode)
    TTYMODE *mode;
{
    extern TTYMODE *curmode;
    extern free();

    if (mode == NULL || mode == curmode)
	fprintf(stderr, "FreeMode: Attempt to free current mode block ignored\r\n");
    else
	free(mode);
}

/* -------------------------- C H G M O D E ------------------------- */
/*
 * ChgMode(mode) changes to a different mode. The current tty structure
 * is saved away, and the new mode structure is applied. The old mode
 * is returned.
 */
TTYMODE *
ChgMode(mode)
    TTYMODE *mode;
{
    TTYMODE *oldmode;
    extern int termfd;
    extern TTYMODE *curmode;

    if (mode == NULL || mode == curmode) /* Change to current mode -- no-op */
	return(curmode);

    check("ChgMode", gtty(termfd, &(curmode->tm_sgtty)));/* Save current mode */
    check("ChgMode", ioctl(termfd, TIOCGETC, &(curmode->tm_tchars)));/* Save current mode */
    check("ChgMode", ioctl(termfd, TIOCLGET, &(curmode->tm_local)));
    oldmode = curmode;
    curmode = mode;
    check("ChgMode", stty(termfd, &(curmode->tm_sgtty)));
    check("ChgMode", ioctl(termfd, TIOCSETC, &(curmode->tm_tchars)));
    check("ChgMode", ioctl(termfd, TIOCLSET, &(curmode->tm_local)));
    return(oldmode);
}

/* -------------------------- G E T M O D E ------------------------- */
/*
 * GetMode(mode) returns the sgttyb portion of the mode block for "mode".
 * It implements the current mode special casing.
 */
struct sgttyb *
GetMode(mode)
    TTYMODE *mode;
{
    extern TTYMODE *curmode;
    extern int termfd;

    if (mode == NULL)
	mode = curmode;
    if (mode == curmode)
	check("GetMode", gtty(termfd, &(curmode->tm_sgtty)));
    return(&(mode->tm_sgtty));
}

/* -------------------------- S E T M O D E ------------------------- */
/*
 * SetMode(mode) sets the sgttyb portion of the mode block for "mode".
 * It implements the current mode special casing.
 */
SetMode(mode, ttyblock)
    TTYMODE *mode;
    struct sgttyb *ttyblock;
{
    register struct sgttyb *p;
    register struct sgttyb *q;
    extern int termfd;
    extern TTYMODE *curmode;

    if (mode == 0)
	mode = curmode;
    p = &(mode->tm_sgtty);
    q = ttyblock;
    p->sg_ospeed = q->sg_ospeed;
    p->sg_ispeed = q->sg_ispeed;
    p->sg_erase = q->sg_erase;
    p->sg_kill = q->sg_kill;
    p->sg_flags = q->sg_flags;
    if (mode == curmode)
	check("SetMode", stty(termfd, &(curmode->tm_sgtty)));
}

/* -------------------------- S E T F L A G S ----------------------- */
/*
 * SetFlags(mode, mask, flags) sets the tty flags of "mode" to "flags".
 * However, only those flag bits accessible through "mask" are set or
 * cleared; the rest of the flags are untouched. The old values of those
 * flags are returned.
 */
SetFlags(mode, mask, flags)
    TTYMODE *mode;
    int mask;
    int flags;
{
    int oldflags;
    int newflags;
    struct sgttyb *sgmodes;
    extern int termfd;
    extern TTYMODE *curmode;
    extern struct sgttyb *GetMode();

    flags &= mask;

    sgmodes = GetMode(mode);

    oldflags = newflags = sgmodes->sg_flags;
    newflags &= ~mask;
    newflags |= flags;
    sgmodes->sg_flags = newflags;

    if (sgmodes == &curmode->tm_sgtty)
	check("SetFlags", stty(termfd, &curmode->tm_sgtty));

    return(oldflags);
}

/* -------------------------- G E T F L A G S ----------------------- */
/*
 * GetFlags(mode) returns the tty flags of "mode".
 */
GetFlags(mode)
    TTYMODE *mode;
{
    return(GetMode(mode)->sg_flags);
}

/* -------------------------- O R I G M O D E ----------------------- */
/*
 * OrigMode() returns a pointer to the original mode block -- that is,
 * the one containing the modes in effect before the first ChgMode was done.
 */
TTYMODE *
OrigMode()
{
    extern TTYMODE origmode;

    return(&origmode);
}

/* -------------------------- C U R M O D E ------------------------- */
/*
 * CurMode() returns a pointer to the current mode block. Useful
 * mainly for freeing the block when a mode is no longer needed.
 */
TTYMODE *
CurMode()
{
    extern TTYMODE *curmode;

    return(curmode);
}

/* -------------------------- C H E C K ----------------------------- */
/*
 * check(name, val) is an internal routine to check system calls. If val is
 * -1, it prints an error message and aborts.
 */
check(name, val)
    char *name;
    int val;
{
    extern int termfd;
    extern char *errmsg();

    if (val != -1)
	return;
    fprintf(stderr, "%s: Error. %s. termfd = %d\r\n", name, errmsg(0), termfd);
    ecmderr(0, "Exiting.");
}

/* -------------------------- G L O B A L S ------------------------- */
/*
 * curmode points to the tty structure to save the current tty modes in
 * when switching to a new mode.
 *
 * origmode is the original set of modes.
 */
TTYMODE origmode;
TTYMODE *curmode = &origmode;
