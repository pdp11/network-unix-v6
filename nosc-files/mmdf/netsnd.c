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
#include "open.h"
#define MLFL

struct iobuf tnibuf, tnobuf;
struct netstruct net_arpa;
int domsg;
char errline[ERRLINSIZ + 1];
int savenv;
int infd;
int tnfd;
int tnfd1;
int filesiz;
char *txtname;
char *hostnam;
struct adr adr;

main(argc,argv)
int argc;
char *argv[];

{
register char *p;
register int c;
int result;
char *name;
int host;

signal (SIGINR, 1);
openparam.o_fskt[1] = 3;
openparam.o_timo = 60;
initcom (argc, argv);

for EVER
{
switch (getname ())
{
case NOTOK:
sysabrt (HOSTDEAD);
case DONE:
putresp (0, 0);
if ((c = newfile ()) != OK)
sysabrt (c);
continue;
}
for (p = adr.adr_name; *p && *p != '\n'; p++);
*p = '\0';

domsg = (adr.adr_hand == 'w');

seek (infd, 0, 0);
result = send();
putresp (result, (result == OK) ? 0 : errline);
}
}


opnnet (hostnum)
int hostnum;
{
static int result;
static int curhost;
int code;

if (tnfd)
{ if (hostnum == curhost)
return(OK);
clsnet (1);
}
else
{ if (hostnum == curhost)
return(result);
}
curhost = hostnum;
openparam.o_host = hostnum;
printx ("connecting... ");
log ("[ %s ]", hostnam);
if ((tnfd = open ("/dev/net/anyhost", &openparam)) < 0)
{
log ("Can't connect");
tnfd = 0;
result = HOSTDEAD;
}
else
{ zero (&tnibuf, sizeof tnibuf);
zero (&tnobuf, sizeof tnobuf);
tnibuf.fildes = tnobuf.fildes = tnfd;
tnfd1 = dup (tnfd);

if (envsave (&savenv))
{
alarm (30);
while ((code = getcode ()) != 300 && code >= 0);
alarm (0);
result = (code < 0 ? BADNET : OK);
}
else
result = TIMEOUT;

if (result != OK)
{
close (tnfd);
close (tnfd1);
tnfd = 0;
}
}
return (result);
}


clsnet (flag)
int flag;
{
if (tnfd)
{
if (flag)
sendbye ();
close (tnfd);
close (tnfd1);
tnfd = 0;
}
}


sendbye ()
{
int code;

if (envsave (&savenv))
{
alarm (20);
puts ("BYE\r\n", &tnobuf);
fflush (&tnobuf);
log ("[ BYE ]");
while ((code = getcode ()) != 231)
if (code < 0)
break;
alarm (0);
}
}


send ()
{
static int curhost;
int result;
int domail;
int hostnum;
char tadr[LINESIZE];
char tfirst[LINESIZE];
char *name;
register char *ptr;

for (ptr = adr.adr_name; !lwsp (*ptr); ptr++);
for (*ptr = '\0'; lwsp (*ptr); ptr++);
for (name = ++ptr; !lwsp (*ptr) && *ptr != '\n'; ptr++);
*ptr = '\0';
host2adr (&net_arpa, TRUE, adr.adr_name, tadr);
hostnum = atoi (tadr);
adr2first (&net_arpa, tadr, tfirst);
if (hostnum != curhost)
{ curhost = hostnum;
if (hostnam)
free (hostnam);
hostnam = strdup (tfirst);
}
printx ("%s at %s: ", name, hostnam);




if ((result = opnnet (hostnum)) == OK)
{ domail = 0;
filesiz = infdsize ();
switch(adr.adr_delv)
{ case DELMAIL:
domail++;
break;
case DELTTY:
result = sndmail(name,"XSEN");
break;
case DELTorM:
result = sndmail(name,"XSEM");
if (result == NODEL || result == TRYAGN)
{ result = sndmail(name,"XSEN");
if (result == NODEL || result == TRYAGN)
domail++;
}
break;
case DELBOTH:
result = sndmail(name,"XMAS");
if (result == NODEL || result == TRYAGN)
{ result = sndmail(name,"XSEN");
if (result == NODEL || result == TRYAGN ||
result == OK)
domail++;
}
break;
}

#ifdef MLFL
if (domail && tnfd)
{ result = sndmlfl (name);
if (result == TRYMAIL)
{
printx ("\nMLFL failed, trying MAIL... ");
result = sndmail (name,"MAIL");
}
}
#endif
#ifndef MLFL
if (domail && tnfd)
result = sndmail (name,"MAIL");
#endif
}
return (result);
}







sndmail (name, ftpcmd)
char name[];
char ftpcmd[];
{
register int result;

if (envsave (&savenv))
{
printx ("checking... ");
alarm (60);
if ((result = mailcmd (name,ftpcmd)) == OK)
{
printx ("sending... ");
alarm (filesiz * 20 + 60);
mailsnd ();
result = mailend ();
}
alarm (0);
}
else
result = TIMEOUT;

return (result);
}





mailcmd (name,ftpcmd)
char name[];
char ftpcmd[];
{
register int code;
int tries;

tries = 0;
retry: puts (ftpcmd, &tnobuf);
putc (' ', &tnobuf);
puts (name, &tnobuf);
puts ("\r\n", &tnobuf);
fflush (&tnobuf);
log ("%s %s", ftpcmd, name);

while ((code = getcode ()) != 350)
switch (code)
{
case NOTOK:
return (BADNET);
case 450:
return (NODEL);
case 504:
if (tries++)
return (BADNET);
if ((code = multics ()) != OK)
return (NODEL);
goto retry;
case 951:
break;
default:
switch (code / 100)
{
case 0:
break;
case 4:
return (TRYAGN);
case 5:
return (NODEL);
default:
return (HOSTERR);
}
}
return (OK);
}





mailsnd ()
{
register int c;
int newline;
struct iobuf inbuf;

newline = 1;
zero(&inbuf, sizeof inbuf);
inbuf.fildes = infd;

while ((c = getc (&inbuf)) > 0)
{
if (c == '\n')
{
newline = 1;
putc ('\r', &tnobuf);
}
else
{
if (newline && c == '.')
putc (' ', &tnobuf);
newline = 0;
}
putc (c, &tnobuf);
}
if (!newline)
puts ("\r\n", &tnobuf);
puts (".\r\n", &tnobuf);
fflush (&tnobuf);

}





mailend ()
{
int code;

while ((code = getcode ()) != 256)
{
switch (code)
{
case NOTOK:
return (BADNET);
case 331:
return (TRYAGN);
default:
switch (code / 100)
{
case 0:
break;
case 2:
return (OK);
case 4:
return (TRYAGN);
case 5:
return (NODEL);
default:
return (HOSTERR);
}
}
}
return (OK);
}


#ifdef MLFL



sndmlfl (name)
char name[];
{
register int result;
int mlflpid;
int status;
register int temp;

mlflpid = mlflsnd ();
if (mlflpid == NOTOK)
{
printx ("can't fork - ");
log ("Can't fork for MLFL");
return (TRYMAIL);
}
if (envsave (&savenv))
{
printx ("checking... ");
alarm (60);
if ((result = mlflcmd (name)) == OK)
{
printx ("sending... ");
alarm (filesiz * 20 + 60);
result = mlflend ();
if (result == OK)
{
while ((temp = waita (&status)) != mlflpid)
if (temp == NOTOK)
{
status = BADNET << 8;
break;
}
mlflpid = 0;
result = status >> 8;
}
}
alarm (0);
}
else
result = TIMEOUT;

if (mlflpid)
{
kill (mlflpid, 9);
while ((temp = waita (&status)) != mlflpid && temp != NOTOK);
}

return (result);
}






mlflcmd (name)
char name[];
{
int tries;
register int code;

tries = 0;
retry: puts ("MLFL ", &tnobuf);
puts (name, &tnobuf);
puts ("\r\n", &tnobuf);
fflush (&tnobuf);
log ("MLFL %s", name);

while ((code = getcode ()) != 250)
switch (code)
{
case NOTOK:
return (BADNET);
case 507:
case 450:
return (NODEL);
case 504:
if (tries++)
return (TRYMAIL);
if ((code = multics ()) != OK)
return (TRYMAIL);
goto retry;
case 255:
case 951:
break;
default:
switch (code / 100)
{
case 0:
break;
case 4:
return (TRYMAIL);
case 5:
return (TRYMAIL);
default:
return (HOSTERR);
}
}
return (OK);
}






mlflsnd ()
{
struct iobuf mlflbuf;
struct netopen openp;
int f;
register int c;
struct iobuf inbuf;

f = fork ();
if (f == 0)
{
zero (&openp, sizeof openp);
openp.o_type = RELATIVE | DIRECT;
openp.o_lskt = 5;
openp.o_relid = tnfd;
openp.o_host = openparam.o_host;
zero (&mlflbuf, sizeof mlflbuf);
if ((mlflbuf.fildes = open ("/dev/net/anyhost", &openp)) >= 0)
{
zero(&inbuf,sizeof inbuf);
inbuf.fildes = infd;

while ((c = getc (&inbuf)) > 0)
{
if (c == '\n')
putc ('\r', &mlflbuf);
putc (c, &mlflbuf);
}
fflush (&mlflbuf);
exit (OK);
}
exit (BADNET);
}
return (f);
}





mlflend ()
{
register int code;

while ((code = getcode ()) != 252)
{
switch (code)
{
case NOTOK:
return (BADNET);
case 331:
return (TRYAGN);
default:
switch (code / 100)
{
case 0:
break;
case 2:
return (OK);
case 4:
return (TRYMAIL);

case 5:
return (TRYMAIL);

default:
return (HOSTERR);

}
}
}
return (OK);
}
#endif








multics ()
{
register int code;

printx ("logging in... ");
puts ("USER NETML\r\n", &tnobuf);
fflush (&tnobuf);
log ("[ USER NETML ]");
while ((code = getcode ()) != 330)
{
if (code < 0)
return (BADNET);
if (code == 230)
return (OK);
if (code >= 400)
return (NODEL);
}

puts ("PASS NETML\r\n", &tnobuf);
fflush (&tnobuf);
log ("[ PASS NETML ]");
while ((code = getcode ()) != 230)
{
if (code < 0)
return (BADNET);
if (code >= 400)
return (NODEL);
}
return (OK);
}


getcode ()
{
register int code;
register int temp;

code = getcod1 ();
if (errline[3] == '-')
while ((temp = getcod1 ()) != code && temp >= 0);
return (code);
}


getcod1 ()
{
register int code;
register int i;
register int c;

i = 0;
while ((c = getc (&tnibuf)) != '\n')
{
if (c < 0)
{
log ("[ closed ]");
return (NOTOK);
}
if (i < ERRLINSIZ && c >= ' ' && c < '\177')
errline[i++] = c;
}
errline[i] = '\0';
code = 0;
for (i = 0; i < 3 && errline[i] >= '0' && errline[i] <= '9'; i++)
code = code * 10 + (errline[i] - '0');
log (errline);
return (code);
}

sysabrt (stat)
int stat;
{
clsnet (1);
exit (stat);
}
