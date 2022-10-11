#define NULL 0

/*
 * Modifications:
 * jsq BBN 21Mar80:  put in ifdef on BELL and stat check of BELL_PASSWD
 * jsq BBN 24Mar80:  made tpass work if the recorded password is passed
 *	an ordinary string, not terminated by a colon.
 * dan BBN 22 July 80: moved #define BELL into /sys/sys/h/sitedefs.h
 * dan BBN 24 Oct 80: deleted BELL stuff for V7
 */

/* tpass(typed,recorded,rout) is a password checker. A pointer
	to the null-terminated string <typed> containing the
	input value of the password, and a pointer to the beginning
	of the password entry from /etc/passwd <recorded>,
	which [may be] terminated by ':', are passed into tpass.

   In the normal case, the routine pointer <rout> is NULL, and
	the password is checked using the BBN algorithm "crypt"
	(if it is 16 characters long, or by the Bell Version 6
	algorithm "bcrypt" (if it is 8 characters long).
	Passwords of the wrong length always fail.

  If the third argument is non-null, it is used to encrypt the
	the typed value, which is then compared with the recorded
	value.

  If the password checks, 1 is returned, otherwise 0 is returned.

*/

int   tpass (typed, found, rout)
char *typed;
char *found;
char *(*rout)();
{
  register char *cryptp;
  register int	passlen;
  static char recorded[24];	/* convert found into ordinary string here */
  extern char *sfind ();
  extern char *scopy ();
  extern int  seq ();
  extern char *crypt ();
  passlen = sfind (found, ":") - found;
  if (passlen >= sizeof (recorded))
    return (0);
  scopy (found, recorded, &recorded[passlen]);
  if (rout != NULL)
    cryptp = (*rout) (typed, recorded);
  else
      cryptp = crypt (typed, recorded);
  if (seq (cryptp, recorded))
    return (1);
  return (0);
}
