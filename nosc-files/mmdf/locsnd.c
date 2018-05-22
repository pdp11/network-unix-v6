#
/*
 *   MULTI-CHANNEL MEMO DISTRIBUTION FACILITY (MMDF)
 *
 *   Copyright (C) 1979  University of Delaware
 *
 *   This program and its listings may be copied freely by United States
 *   federal, state, and local government agencies and by not-for-profit
 *   institutions, after sending written notification to:
 *
 *      Professor David J. Farber
 *      Department of Electrical Engineering
 *      University of Delaware
 *      Newark, Delaware  19711
 *
 *          Telephone:  (302) 738-2405
 *
 *  Notification should include the name of the acquiring organization,
 *  name and contact information for the person responsible for maintaining
 *  the operating system, and license information if MMDF will be run on a
 *  Western Electric Unix(TM) operating system.
 *
 *  Others may obtain copies by arrangement.
 *
 *  The system was originally implemented by David H. Crocker, and the
 *  effort was supported in part by the University of Delaware and in part
 *  by contracts from the United States Department of the Army Readiness
 *  and Materiel Command and the General Systems Division of International
 *  Business Machines.  The system was built upon software initially
 *  developed by The Rand Corporation, under the sponsorship of the
 *  Information Processing Techniques Office of the Defense Advanced
 *  Research Projects Agency, and was developed with their cooperation.
 *
 *  The above statements must be retained with all copies of this program
 *  and may not be removed without the consent of the University of
 *  Delaware.
 **/
#include "mailer.h"
#include "ffio.h"
extern int errno,
sentprotect;
extern char
stndmail[],
dlvnote[],
ttympre[],
ttymsuf[],
delim1[],
delim2[],
userloc[],
*sender;

extern struct adr adr;
#define NUMFFBUFS 5
char ffbufs[NUMFFBUFS][FF_BUF];
char
tempbuf[512],
ttyname[] "/dev/ttyx";

int
infd,
whofd - 1,
uid,
gid;


int domsg;
int filesiz;
int savenv;
char errline[100];
char userdir[100];
char *txtname;

main (argc, argv)
int argc;
char *argv[];

{
register char *p;
register int c;
int result;
printx("locsnd started\n");

for (c = 0; c < NUMFFBUFS; c++)
ff_use (ffbufs[c]);
initcom (argc, argv);

for EVER
{
switch (getname())
{
default:
case NOTOK:
exit (NOTOK);
case DONE:
putresp (0, 0);
if ((c = newfile ()) != OK)
exit (c);
continue;
case OK:
break;
}
for (p = adr.adr_name; *p && *p != '\n'; p++)
*p = uptolow (*p);

domsg = (adr.adr_hand == 'w');
printx ("%c%s: ", lowtoup (adr.adr_name[0]), &(adr.adr_name[1]));

seek (infd, 0, 0);
if ((result = getusr (adr.adr_name)) == OK)
result = procadr ();
putresp (result, (result == OK) ? 0 : errline);
}
}


procadr ()
{
int result;

if ((result = privpgm ()) == TRYMAIL)
switch (adr.adr_delv)
{
case DELMAIL:
result = deliver (adr.adr_name);
break;
case DELTTY:
printx ("(terminal) ");
result = alert (adr.adr_name, infd);
break;
case DELTorM:
printx ("(terminal) ");
if ((result = alert (adr.adr_name, infd)) < OK)
{
printx ("\n\ttrying (mail) ");
seek (infd, 0, 0);
result = deliver (adr.adr_name);
}
break;
case DELBOTH:
printx ("(terminal) ");
result = alert (adr.adr_name, infd);
printx ("\n\t(mail) ");
seek (infd, 0, 0);
result = deliver (adr.adr_name);
}
return (result);
}

privpgm ()
{
register char *p,
*q;
register int result;
struct inode statbuf;
int i;
int temp;
int status;
char *userprog;
int f;

userprog = nmultcat (userdir, "/bin/", userloc, 0);
if (stat (userprog, &statbuf) < OK)
{
free (userprog);
return (TRYMAIL);
}
log ("User program:");
printx ("(being delivered by receiver's program)... ");
switch (f = fork ())
{
case 0:
if (infd != 0)
{
close (0);
dup (infd);
close (infd);
}
for (i = 2; i < HIGHFD; i++)
close (i);
if (adr.adr_hand != 'w')
close (1);
setgid (gid);
setuid (uid);
execl (userprog, userloc, &(adr.adr_hand), txtname, sender, 0);
error ("can't exec program");
exit (TRYAGN);
case NOTOK:
error ("can't fork");
free (userprog);
return (TRYAGN);
}
free (userprog);
if (envsave (&savenv))
{
alarm (filesiz * 10 + 30);
filesiz = infdsize ();
while ((temp = waita (&status)) != f && temp != NOTOK);
alarm (0);
}
else
{
result = TIMEOUT;
error ("user program timed out");
kill (f, 9);
}
result = (status >> 8) & 0377;
if (temp < OK || result < MINSTAT || result > MAXSTAT)
return (HOSTERR);


return (result);
}


deliver (usrname)
char usrname[];
{
register char *p,
*q;
register char lastchar;
int notify;
struct inode mbxstat;
char *mboxname;
int mboxfd;
int count;

mboxname = nmultcat (userdir, "/", stndmail, 0);

while ((mboxfd = open (mboxname, 5)) < 0)
{
if (errno != 2)
{
error ("can't write on mailbox, ");
free (mboxname);
return (TRYAGN);
}
if ((mboxfd = creat (mboxname, sentprotect)) < 0)
{
error ("can't create mailbox, ");
free (mboxname);
return (TRYAGN);
}
chown (mboxname, uid | (gid << 8));
close (mboxfd);
}
free (mboxname);

if (dlvnote[0] == 0)
notify = FALSE;
else
{
fstat (mboxfd, &mbxstat);
notify = ((mbxstat.fsize0 == 0) && (mbxstat.fsize1 == 0));
}

printx ("...");
seek (infd, 0, 0);
seek (mboxfd, 0, 2);
if (write (mboxfd, delim1, strlen (delim1)) < 0)
{
error ("error writing to mailbox");
return (TRYAGN);
}
while ((count = read (infd, tempbuf, sizeof tempbuf)) > 0)
{
/*printx (".");*/
flush ();
if (write (mboxfd, tempbuf, count) < 0)
{
error ("error writing to mailbox");
return (TRYAGN);
}
lastchar = tempbuf[count - 1];
}
printx (" ");
if (lastchar != '\n')
write (mboxfd, "\n", 1);
if (write (mboxfd, delim2, strlen (delim2)) < 0)
{
error ("error writing to mailbox");
return (TRYAGN);
}
close (mboxfd);

if (notify)
alert (usrname, NOTOK);

return (OK);
}


getusr (usrname)
char usrname[];
{
static int pwdfd;
static char pwline[200];
int i;
char numstr[10];
register char c,
*q;

if (pwdfd == 0 && (pwdfd = ff_open ("/etc/passwd", 0)) == NOTOK)
{
error ("can't open password file");
return (TRYAGN);
}
else
ff_seek (pwdfd, 0, 0);

for EVER
{

if ((i = ff_read (pwdfd, pwline, sizeof pwline, ":\377")) <= 0)
{
error ("unknown user");
return (NODEL);
}
pwline[i - 1] = '\0';
if (strequ (usrname, pwline))
{
ff_read (pwdfd, 0, 32000, ":\377");

ff_read (pwdfd, &numstr, sizeof numstr, ":\377");
uid = atoi (&numstr);
ff_read (pwdfd, &numstr, sizeof numstr, ":\377");
gid = atoi (&numstr);
ff_read (pwdfd, 0, 32000, ":\377");

if ((i = ff_read (pwdfd, userdir, sizeof userdir, ":\n\377"))
<= 0)
{
error ("error with password file");
return (TRYAGN);
}
userdir[i - 1] = '\0';
return (OK);
}
else
if (ff_read (pwdfd, 0, 32000, "\n\377") < OK)
{
error ("error in password file");
return (TRYAGN);
}
}
}


alert (usrname, msgfd)
char usrname[];
int msgfd;
{
struct inode statbuf;
struct
{
char logname[8];
char logtty;
long int logtime;
int dummy;
} wholine;
register int i;
int logged;
int good;

if (whofd < 0 &&
(whofd = open ("/etc/utmp", 0)) < 0)
{
error ("can't open utmp file");

return (TRYAGN);
}
else
seek (whofd, 0, 0);

logged = good = FALSE;
while (read (whofd, &wholine, sizeof wholine) == sizeof wholine)
{
if (wholine.logname[0] == '\0')
continue;
for (i = 0; i < 8; i++)
if (wholine.logname[i] != usrname[i])
break;
if (usrname[i] == '\0' &&
(i == 8 || wholine.logname[i] == ' '))
{
ttyname[8] = wholine.logtty;
logged = TRUE;
if (stat (ttyname, &statbuf) >= 0 &&
(statbuf.iflags & 02))

{
if (alert1 (msgfd, ttyname) == OK)
good = TRUE;
}
}
}
if (!logged)
{
msg ("not logged in; ");
return (NODEL);
}
if (!good)
{
msg ("can't write to terminal; ");
return (NODEL);
}
msg ("notified; ");
return (OK);
}


alert1 (msgfd, ttynm)
int msgfd;
char ttynm[];
{
int ttyfd;
int count;

ttyfd = NOTOK;
if (envsave (&savenv))
{
if ((ttyfd = open (ttynm, 1)) < 0)
return (TRYAGN);
alarm (filesiz * 10 + 30);
if (msgfd >= 0)
{
printx ("...");
if (ttympre)
write (ttyfd, ttympre, strlen (ttympre));
seek (msgfd, 0, 0);
while ((count = read (infd, tempbuf, 512)) > 0)
{
printx (".");
if (write (ttyfd, tempbuf, count) < 0)
{
close (ttyfd);
return (NOTOK);
}
}
if (ttymsuf)
write (ttyfd, ttymsuf, strlen (ttymsuf));
printx (" ");
}
else
if (dlvnote[0] != '\0')
write (ttyfd, dlvnote, strlen (dlvnote));

close (ttyfd);
alarm (0);
return (OK);
}
close (ttyfd);
return (TIMEOUT);
}


msg (string)
char string[];
{
printx("%s, ",string);
log ("\t%s", string);
strcpy (string, errline);
}

error (string)
char string[];
{
log ("\t[locsnd : ERROR ]");
msg (string);
}

