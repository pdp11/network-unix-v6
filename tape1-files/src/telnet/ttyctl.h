/*
 * Include file for use with ttyctl.c routines.
 * You should include globdefs.h first.
 */
#ifdef NEWTTY
#include <modtty.h>
#endif
#include <sgtty.h>

struct tm
{
    struct sgttyb tm_sgtty;
    struct tchars tm_tchars;
#ifdef NEWTTY
    struct modes  tm_modes;
#endif
    int tm_local;
};

typedef struct tm TTYMODE;

extern TTYMODE *AllocMode();
extern TTYMODE *ChgMode();
extern struct sgttyb *GetMode();
#ifdef NEWTTY
extern struct modes *GetXMode();
#endif NEWTTY
extern TTYMODE *OrigMode();
extern TTYMODE *CurMode();
