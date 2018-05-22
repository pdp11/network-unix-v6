.so /lib/tmac.d
.nh
.de tx
.ad
.fi
.ls 2
..
.de dg
.na
.nf
.ls 1
..
.he 1 ''Illinois NCP Documentation'- % -'
.tx
.bs "The NCP Environment"
.pg
The Illinois NCP makes an ARPANET host TELNET server look like any other
device. To talk to the MULTICS server TELNET,
the user just opens \fB/dev/multics\fR, and uses the
regular system \fBread\fR and \fBwrite\fR calls to communicate.
In this sense
the NCP kernel and daemon are simply one large device driver.
.pg
The interface that the NCP provides to network files is designed to
be used by programs implementing higher level protocols. If a
user were to open a network host file, he would find TELNET option
requests intermixed with the foreign host's responses. Thus,
net files cannot be connected to filters or used in other standard
ways.
.pg
The basic purpose of the NCP kernel is to relay data between the user
and the IMP, and to update state information for the connection.
The NCP kernel also handles low level (\fBRFNM\fR) acknowledgements. The
daemon worries about most of the protocol issues. In particular it
takes user open and close requests,
which are forwarded to it via the
kernel, and issues instructions to the kernel to perform the appropriate
actions. The daemon is not involved in a regular read or write from
the network.
.sp 2
.pg
The communication paths between the user, kernel, daemon, and 
IMP are shown below.
.sp 5
.kp
.dg
			 ---------
			|   NCP   |
			| Daemon  |
			 ---------
			     |
			     |
 --------		 ---------		   ---------
|  User  |______________|   NCP   |_______________|   IMP   |
|        |		|  Kernel |		  |         |
 --------		 ---------		   ---------
.ke
.sp 3
.tx
.bs "Interface Between the User and Kernel"
.pg
The kernel contains a pseudo device driver which supports the
standard \fBread\fR, \fBwrite\fR, \fBopen\fR, \fBclose\fR, and
\fBstat\fR interface. It is
a "pseudo" device driver because the routines do not appear in
\fBbdevswitch\fR or \fBcdevswitch\fR; instead the system kernel is modified
to notice that a network file is being used (\fBf_flag\fR = \fBFNET\fR) and
directly call one of \fBnetread\fR, \fBnetwrite\fR, \fBnetopen\fR,
\fBnetclose\fR, or
\fBnetstat\fR (the first two are in \fBnrdwr.c\fR, the last three are in
\fBnopcls.c\fR).
(See the description
of the kernel's \fBnetfile\fR data structure for the reason why it
is done that way).
.pg
A regular open (e.g. \fBopen("/dev/multics",READ)\fR) will
initiate a TELNET connection to the foreign host; i.e. it
will perform an ICP to socket one. When the local and foreign
socket numbers (and link number) have been negotiated, the
\fBopen\fR will return. Fancier kinds of open - for example opens
with timeouts - are done with a hack on the mode number. Before
explaining the this, let me first explain server opens.
.pg
A server open has the form \fBopen(0,local_socket_number)\fR. This will
cause the server to sleep until someone connects to him. He can
find out who he's talking to by using the \fBstat\fR system call.
The \fBstat\fR call usually returns the contents
of an inode, but for network files it returns the \fBnopen\fR structure
with the \fBo_lskt, o_frnskt\fR, and \fBo_frnhost\fR fields filled in.
.pg
In a regular open, if the mode is > 7 or < 1 then the system assumes
that the mode word
is a pointer to a network open structure (\fBnopen\fR), which is given
below. If the \fBnopen\fR structure does not specify 
the foreign host (i.e., \fBo_frnhost\fR = 0 ), then the
host named in the \fBopen\fR will be used (e.g.,
\fBopen("/dev/multics",&nopen)\fR
will use MULTICS as the foreign host).
.sp 2
.kp
.dg
\fBstruct
{
	char	o_op;		/* open opcode to daemon ( = 0 ) */
	char	o_type;		/* type of connection. see below */
	int	o_id;		/* pointer to i_addr part of inode */
	int	o_lskt;		/* local socket number */
	int	o_fskt[2];	/* foreign socket number */
	char	o_frnhost;	/* foreign host number */
	char	o_bsize;	/* byte size for the connection */
	int	o_nomall;	/* number of bytes and messages to
				   keep allocated to foreign host */
	int	o_timeo;	/* num of sec to wait before
				   open times out */
	int	o_relid;	/* file number to base data
				   connection on. The kernel will
				   convert this to a pointer
				   to a file structure. */
} nopen;\fR
.ke
.sp 2
.kp
.dg
.ul
  open type bits		    off     |   on
.sp 2
\fB#define otb_drct	01	/* icp      | direct */
#define otb_serv	02	/* user     | server */
#define otb_init	04	/* listen   | init */
#define otb_spcf	010	/* general  | specific
				    listen     listen  */
#define otb_dux		020	/* simplex  | duplex */
#define otb_rltv	040	/* absolute | relative
					socket numbers */\fR
.ke
.tx
.bl 2
.pg
Notice that there are only 16 bits for the local socket number.
The NCP assumes that the high order 16 bits are zero. Also, all
socket conflicts must be resolved at a high level. 
If the \fBnopen\fR structure is not used, the NCP
daemon will allocate socket numbers (in blocks of eight)
between zero and 2048.
.pg
The \fBo_id\fR does not contain what was mentioned above. Rather it
contains a pointer to a file header (\fBfile.h\fR). This pointer
is included with most communications between the daemon and 
the kernel to make sure that they are talking about the same thing.
The reason the \fBo_id\fR field is billed as being something else
is that the \fBnopen\fR structure is used in other places where \fBo_id\fR
is indeed an i_node pointer.
.es
.bs "Interface Between the NCP Daemon and the Kernel"
.pg
The daemon talks to the kernel via \fB/dev/ncpkernel\fR. This is a regular
device that appears in \fBcdevswitch\fR, and is implemented by \fBncpopen\fR,
\fBncpclose\fR, \fBncpread\fR, and \fBncpwrite\fR.
These routines appear in the file
\fBncpio.c\fR. Only one person can open this device, and that person should
be the "small_daemon" process. See the section on the NCP's
birth for more information on the "small_daemon".
.pg
When the daemon has nothing else to do it sits in a loop reading from 
the kernel. There are two kinds of messages that the kernel sends the
daemon. First, there are relayed requests from the user such as open and
close messages, and second there are the contents of data buffers
from the IMP, which
include error messages directly from the IMP. The interface for these
communications is described in \fBkread.h\fR.
.pg
On the other side, the daemon writes on \fB/dev/ncpkernel\fR in order to send
commands to the kernel. The commands do things like send data over a
particular connection, and set up the kernel's socket data structures.
For example, when the user does an open, the request is sent to the
daemon, who in turn tells the kernel to set up certain socket structures
and send an \fBRFC\fR to the foreign host.
.es
.es
.bs "The General Structure of the NCP Kernel"
.pg
In many ways the NCP Kernel is just three device drivers. The files
\fBimpi1.c\fB, \fBimpi2.c\fR, and \fBimpo.c\fR contain the IMP driver. The 
ncpkernel driver is in \fBncpio.c\fR, and the network device driver
(for files like \fB/dev/multics\fR) is in \fBnopcls.c\fR and \fBnrdwr.c\fR.
All the buffer management routines are in \fBkerbuf.c\fR, including
a routines to fill in and link together buffers, plus a routine
\fBsndnetbytes\fR which takes an array of bytes (called a vector by
the program), and links them onto the IMP's output queue after
adding an IMP leader, and copying the array into "netbuf"s. Lastly,
to manipulate a bit map there are a few routines which are
in \fButilities.c\fR. The file \fBcontab.c\fR holds the procedures to
read and update the tables which record the state of each connection.
.bs The IMP Driver
.pg
The IMP driver is cleanly divided into two parts. The input side
is mainly handled by the routines in \fBimpi1.c\fR and \fBimpi2.c\fR while the
output side is handled in \fBimpo.c\fR.
.pg
.bs "The IMP Input Driver"
.pg
The code in \fBimpi1.c\fR behaves as seperate interrupt driven kernel
process. The section on NCP start-up explains how they do this hack.
Whenever the IMP interrupts (i.e. when its core buffer becomes full,
or a messages is completed)
\fBimp_iint\fR gets called. In turn, this wakes up
\fBimp_open\fR (in \fBimpo.c\fR), which checks to see if the IMP
driver should die,
and then calls \fBimp_input\fR. If an IMP to host message leader has
been received, then \fBimp_input\fR calls \fBih\fR. If the leader is for
a host to host message, then \fBimpiptr\fR is set to the socket associated
with leader's link number, and \fBimpiptr\fR is set to zero if the link
number is zero (the control link). Once a host to host leader has been
received, the IMP's device registers are changed so that the
remainder of the message will go into a "netbuf".
.pg
Basically, a "netbuf" is a block
of 60 data bytes, plus a link to the next "netbuf". These are
arranged in a circular list called a "message",
and are referred to by a pointer to the
last buffer in the circle.
When the IMP interrupts, and the \fBIENDMSG\fR flag is not set, then the
"netbuf" which the IMP just filled must be added to the incoming
message, and another "netbuf" has to be put under the IMP's spigot.
This is done by \fBihbget\fR (\fBimpi2.c\fR page 6), which calls
\fBappendb\fR (\fBkerbuf.c\fR) to get another "netbuf" and link it onto the
message. Then, \fBihbget\fR calls \fBimpread\fR (\fBimpo.c\fR)
to change the IMP's
device registers to point to the new "netbuf".
.pg
Eventually, \fBIENDMSG\fR will be set, and \fBhh\fR will have a complete
message on its hands. If \fBimpiptr\fR is zero (i.e. the message
contains control information), then it calls \fBhh1\fR (\fBimpi2.c\fR)
for further decoding. Otherwise, the message is sent to the
person who is waiting for it. If the daemon is waiting for it
(as is the case with
an ICP socket) then \fBto_ncp\fR (\fBncpio.c\fR) is called. If not,
then \fBhh\fR links the message onto the end of the socket's
read queue and
does a \fBwakeup(impiptr)\fR to cause the appropriate
\fBnetread\fR (\fBnrdwr.c\fR) to continue.
.pg
The routine \fBhh1\fR (\fBimpi2.c\fR) checks for \fBALL\fR and \fBINS\fR
commands. When either
is found, it locates the associated socket structure by
looking up the host and link it the connection table, and
then performs the command on the socket by
calling \fBallocate\fR (\fBimpi2.c\fR) or by waking the socket's
read process (\fBsktp->r_rdproc\fR). Usually this will be the
process that called \fBnetread\fR via the \fBread\fR system call.
.pg
If the \fBIENDMSG\fR flag is set after the leader is read, then it
is an IMP to host message, and \fBih\fR (\fBimpi1.c\fR) is called to
handle it. All \fBih\fR does is forward \fBRFNM\fR's or "incomplete
transmission"s, and ignore \fBNOP\fR's. Everything else gets sent to the
daemon via \fBto_ncp\fR (\fBncpio.c\fR) which causes a daemon read to receive
the leader. The forwarding is handled by \fBsiguser\fR (\fBimpi1.c\fR) which
will do one of three things; ignore the message if the connection
is closed, wake up a user process if he is handling things directly,
or else send the leader to the daemon.
.pg
After all this processing completes, \fBimpldrd\fR (\fBimpo.c\fR) is called
to start another IMP leader read. This is done 
by calling \fBimpread\fR (\fBimpo.c\fR) to
change the IMP's device registers to \fBimpistart\fR, \fBimpicount\fR. Normally
these should describe the address and length of the IMP leader structure
called \fBimp\fR.
.es
.bs "The IMP Output Driver"
.pg
The highest level interface for sending data over the network is
defined by the procedure \fBsndnetbytes\fR (\fBkerbuf.c\fR). This routine is
used for both user and daemon network transmissions. Just 
below that there is a routine (\fBvectomsg\fR in \fBkerbuf.c\fR)
to translate the character arrays
that \fBsndnetbytes\fR uses into "net messages" just like the ones
that the input driver uses. Still further down there is an interface
that looks just like any other block I/O device (i.e. \fBimpstrat\fR and
\fBimp_oint\fR routines (in \fBimpo.c\fR))
that accept regular I/O buffers (\fBbuf.h\fR). Closer
inspection reveals that the interrupt routine has a kludge that
recognizes a "net message", and makes it look like a regular
I/O buffer.
.pg
The output driver runs asynchronously. Every time it finishes outputting
a buffer, the IMP interrupts, and \fBimp_oint\fR gets called to set up another
buffer, and to get the IMP going again (via \fBimp_output\fR). This loop
will keep going until there is no more data to output. Of course,
at the same time \fBsndnetbytes\fR can be filling the output queue via
\fBimpstrat\fR. If the IMP is not running, then \fBimpstrat\fR will give
it a kick when more data needs to be sent.
.pg
The exact forms of the buffers and queues are given in the section
on kernel data structures.
.es
.bs IMP Initialization
.pg
The IMP is initialized by \fBimp_init\fR (\fBimpo.c\fR), which is called by
\fBimpopen\fR just before it begins its looping. It does two major
things; initializes global variables and resets the IMP interface.
The variable it initializes are: \fBimpncp\fR, \fBimpistart\fR,
\fBipmicount\fR, \fBimpostart\fR, \fBimpowc\fR.
It sets up the IMP interface by pulsing the
the \fBCLEAR\fR bit, setting the \fBHOSTREADY\fR bit and then sending the IMP
three \fBNOP\fR's.
.es
.es
.bs "The /dev/ncpkernel Driver"
The \fB/dev/ncpkernel\fR driver is in \fBncpio.c\fR. The open and close
routines are well described in the sections on the birth and death
of the NCP, so this section will concentrate on the read and
write routines.
.bs  Read
.pg
\fBncpread\fR is called when the daemon wants instructions from
the kernel. Correspondingly, \fBto_ncp\fR is called when the kernel
wants to send instructions to the daemon. Basically, \fBto_ncp\fR
fills a FILO queue waking \fBncpread\fR up every time it adds something,
and \fBncpread\fR trys to empty the queue each time it wakes up or
it sleeps if the queue is empty. Actually there are two queues;
messages sent to the daemon while the priority bits of the 
PS word are non-zero will be put in the queue that is emptied
first. This would happen when an \fBINS\fR is received.
.pg
The routine \fBto_ncp\fR takes a vector of bytes, turns them
into a "net message"
using \fBvectomsg\fR (\fBkerbuf.c\fR), and links the message on to the front
of the appropriate read queue. Then it wakes up \fBncpread\fR via
\fB&ncprq\fR, and returns. Once awakened, \fBncpread\fR checks for time outs
(see below), then checks for messages in either queue. If there is
data, it pulls "netbuf"s off of the queue one at a time until
it has a complete message. This is then converted to a vector and
copied into the daemon's buffer using the routine \fBbytesout\fR 
(\fBkerbuf.c\fR).
.es
.bs Write
.pg
The daemon writes on \fB/dev/ncpkernel\fR when it wants to issue commands
to the kernel, so the write side of this device looks more like
a parser than a driver. After reading in the command using \fBcpass\fR
(\fBken/subr.c\fR),
it is checked for valid length and identification. If all goes well,
then the instruction is decoded using \fBncpoptab[]\fR as a jump table.
All these commands are listed on page 10 of \fBncpio.c\fR which
also includes the code to execute the commands.
.pg 
The only command worth noting is \fBSEND\fR. All this command does is
to pass the daemon's buffer address, and count field to \fBsndnetbytes\fR.
.es
.bs "The Time-out Mechanism"
.pg
One of the commands that the daemon can send the kernel is a request
to get a time-out message after a certain amount of time. This is
implemented using the \fBtimeout\fR system call (which Delphi UNIX doesn't
have), plus a slight addition to the \fBncpread\fR routine. The command
causes \fBwf_stimo\fR (\fBncpio.c\fR) to be called via \fBncpoptab[]\fR,
which then
tells the system to call \fBdeatim\fR (\fBncpio.c\fR) after a given wait.
All \fBdeatim\fR
does is increment the global variable \fBncptimo\fR. The addition to
\fBncpread\fR
checks \fBncptimo\fR whenever it wakes up, and it sends a timeout message
to the daemon if \fBncptimo\fR is non-zero.
.pg
Time outs have to be handled in this indirect way in order to avoid
an interrupt race with the IMP, since \fBdeatim\fR is called with a
priority of 6.
.es
.es
.bs "The Net Device Drive"
.pg
This device is really just a forwarding station. The system calls
\fBopen\fR and \fBclose\fR
are forwarded to the daemon, and \fBread\fR and \fBwrite\fR calls are used to
pass bytes to and from the IMP driver. The only call that
does most of its own work is \fBstat\fR, but that's not much since it simply
has to copy data from the kernel's \fBnopen\fR structure for this file.
.pg
There is one unusual trait of the \fBnetclose\fR routine. It gets called
every time the device is closed, not just the last time. The reason
for this is that the last process with the connection open might be
hung in a \fBread\fR which will never complete, so a connection gets
broken (closed) whenever the file is closed and there are less than
three people using it.
.pg
Another oddity about the net device is that the file structure 
(\fBfile.h\fR) for
an open device contains pointers to two inodes, one for each of
the read and write sockets. So \fBnetread\fR, and \fBnetwrite\fR
get called with a
pointer to the address part of the read socket and the write socket,
repectively. This is why the net device could not be put into 
\fBbdevswitch\fR or \fBcdevswitch\fR.
.es
.es
.bs "The General Structure of the Daemon"
.pg
The daemon's \fBmain()\fR is a loop which reads instructions
from the kernel, and processes them by sending further commands
to the kernel, and by updating its
internal data structures.
There are three places where the kernel
gets instructions to send the daemon, and not surprisingly the
daemon has a module to deal with each type of message.
Instructions from the user, the IMP, and the foreign host are
handled by \fBkr_decode.c\fR, \fBir_proc.c\fR and \fBhr_proc.c\fR
respectively. The other modules update the two main
data structures; namely the socket table which holds the
state of each socket and the file table which
points to the sockets associated with each file.
.pg
It is important to realize that the daemon is not involved with
most of the traffic between the user and the foreign host. The
only user commands that reach the daemon are opens and closes.
The only network messages that the daemon sees are the
host to host protocol messages, and ICP messages
(including the IMP to host messages like \fBALL\fR, and \fBRFNM\fR which
are involved in the ICP). All other
messages are routed directly to the user (including regular \fBRFNM\fR's,
and \fBINS\fR's).
.pg
The daemon was designed in a very data driven fashion.
Almost all decisions are made by looking up procedure names
in tables. This causes the program to be divided up in
such a way that all
the procedures in one table appear in one file.
Thus, each file tends to handle all the outcomes of a
particular decision. Typical decisions include deciding 
what to do with a command, and deciding how to move a 
socket from its current state to its desired state.
.bs How the Daemon Parses Commands
.pg
When a message is received from the kernel the table
\fBkr_proc\fR is used to decode the message's opcode. This
table filters out open, close, timeout, and reset commands,
and hands them to the appropriate routine in \fBkr_dcode.c\fR 
(which is where \fBkr_proc[]\fR is defined).
If the opcode is receive, then the
message is coming from the IMP, and it is given to the \fBir_proc.c\fR
module. IMP messages include the type field from the IMP's
leader, so \fBir_proc.c\fR can filter out reports like "IMP going down"
or "host dead". Reports like "request for next message" are 
forwarded to the \fBfiles.c\fR module, and regular data messages are
passed to \fBhr_proc.c\fR. Most data messages contain the host to host
protocol which comes over link zero, so \fBhr_dcode\fR is called
to interpret it, but some data is part of the initial connection
protocol (i.e., a socket number),
and doesn't come over link zero. In this case, \fBhr_data\fR
is called.
.es
.bs How the Daemon Reacts to Commands
.pg
Most commands eventually get translated into instructions to
be performed on individual sockets. For example, an open 
command on a nonexistent file
will cause a socket to be put in listen state. Later, when an
\fBRTS\fR arrives, it causes an \fBRFC\fR operation to be performed
on the socket, which returns the \fBRFC\fR, and changes the state
of the socket to open.
Given the multitude of instruction/state
combinations, the only way to decide what procedure to execute
is to use a table that maps the instruction and
state into a procedure name.
.pg
Actually, there are two tables, one for existing sockets
called \fBskt_oper\fR (defined in \fRskt_oper.c\fR), and one for
non-existant (or un-matched) sockets called \fBso_unm\fR (defined
in \fBso_unm.c\fR). The first is a nine by nine table, and
the second is vector of length four (since the socket
doesn't have a state if its un-matched). All the states
and instructions are defined in \fBsocket.h\fR.
.pg
To make accessing these tables easier there is a procedure
called \fBskt_get\fR in \fBskt_util.c\fR, which will scan through the
socket structures looking for matching local socket numbers.
If found, it will use \fBskt_oper[]\fR to call the right procedure.
If not found, it will use \fBso_unm[]\fR.
As an global argument, it takes a \fBskt_req\fR
structure, which has the same form as
the regular socket structure except that its state field
contains the instruction opcode.
.pg
Some routines already know the location of the
right socket, so they use \fBskt_oper[]\fR directly. Namely, these are:
\fBkr_timo\fR, \fBkr_close\fR, \fBh_dead\fR (in \fBir_proc.c\fR) and
\fBkill\fR (in \fBskt_util.c\fR).
Others, namely, \fBhr_cls\fR, \fBhr_rfc\fR, \fBkr_ouicp\fR, \fBkr_osicp\fR
and \fBsm_splx\fR (\fBskt_util.c\fR)
use \fBget_skt\fR.
.es
.es
.bs Sample Operations of the NCP
.bs The Birth of the NCP Daemon and Kernel
.pg
The NCP Kernel and Daemon are started-up by running the "smalldaemon", which
opens \fB/dev/ncpkernel\fR, and executes the main daemon (\fB1main.c\fR).
Thus, the big daemon inheirits the file descriptor which lets it talk to the
kernel. The system
responds to the open by calling \fBncpopen\fR which sets a flag
(\fBncpopenstate\fR) stating that NCP is alive and well. Next it
calls \fBimpopen\fR, and then it
returns. Now here's the clever part; the first thing \fBimpopen\fR does
is fork. The parent returns, thus completing the \fBopen\fR call, and the
child preceeds to cut itself loose from its parent (by decrementing the
ref-counts on all of the parents files), and goes on living as a seperate
process in the kernel! All it does is
sleep on the \fBimp\fR data structure, and call \fBimp_input\fR whenever it
wakes up. Actually, it makes sure \fBncpopenstate\fR is non-zero,
before calling \fBimp_input\fR.
.es
.bs Death of the NCP Daemon and Kernel
.pg
When the daemon comes to life, it tells the system to execute 
\fBdaemon_dwn\fR if it is sent the kill signal. \fBdaemon_dwn\fR tells the
IMP that its host is dying, sends \fBRESET\fR messages to all hosts,
and notifies everyone who had a net file open. As the process
continues to die, the system will close all of its files. Magically,
file zero is the NCP kernel, so \fBncpclose\fR gets called. It empties
the queues, sets \fBncpopenstate\fR to zero, and wakes up the
input process. Of course, the first thing \fBimp_open\fR checks is the
state of \fBncpopenstate\fR, so it notices the bad news and calls
\fBimp_dwn\fR to die peacefully (i.e., clean out the buffers, and reset
the IMP interface). When that's done, \fBimp_open\fR will return to
\fBncpopen\fR who will return to the smalldaemon with an error message,
thus causing it to die, completing the whole process.
.es
.es
.bs The Use of Data Structures in the Kernel
.bs Kernel Buffers, Netbufs, and Net Messages
.pg
The NCP kernel needs smaller buffers that a regular kernel
buffer, so it turns one kernel buffer into
a bunch of "netbuf"s. These are blocks of 64 bytes which
include a link to the next "netbuf", a count of the number
of data bytes, a flag word, and 60 bytes of data.
.pg
They are usually linked together in circular
queues to form a "net message". Such messages are referenced
by a pointer to their last "netbuf", since that makes it easy
to add another "netbuf" to the end, or to locate the beginning
of the message. These messages are used to store all data
going to and coming from the IMP, so both the net device driver
and the NCP kernel driver deal with these.
.pg
The flag word serves two purposes. It contains a bit which flags
the last buffer in the message, and it contains a field which
tells which kernel buffer the "netbuf" came from. The latter is
only of interest to the routines that allocate and deallocate
"netbuf"s, and thus isn't important to an overall understanding
of the program. See \fBgetbuf\fR (in \fBkerbuf.c\fR) and \fBnetbuf.h\fR for
details.
.pg
The "end of message" flag is used in a lot of ways. The IMP output
driver uses it to decide when to set the \fBEND-OF-MESSAGE\fR bit in
the interface. It is also used by the routines that convert
messages to vectors of characters (or vice versa) for output
to the user or the daemon.
.pg
Messages are often linked together in a queue, by setting the last
"netbuf" of one message to point to the first "netbuf" of another.
For example, each read and write socket has such a queue, and
the commands to the daemon are put in the same type of queue (the
daemon queues are pointed to by \fBncprq\fR, and \fBncpirq\fR
(in \fBkerbuf.c\fR)).
When de-queueing a message the end of message flag is used to seperate
on message from the next.
.es
.bs Netfiles, Contabs, and Sockets
.pg
For each open net file, there is a user file structure (\fBfile.h\fR)
which points to the read and write sockets for that file.
The socket data structures are stored in the address part of
an inode, and are descibed in detail below. Net file headers
have a slightly different format than regular headers. The flag
and ref-count field are the same (\fBf_flag\fR = \fBFNET\fR), but there are
two inode pointers, and no offset pointer. The system \fBread\fR and
\fBwrite\fR calls are modified to pass \fBnetread\fR and \fBnetwrite\fR the
socket of the right flavor (i.e., the address part of the correct
inode).
.pg 
The \fBcontab\fR is used to map host and link numbers into sockets.
There are two such tables, one for read sockets, and one
for write sockets. It contains the local and foreign socket numbers,
which are used to validate protocol communications. It contains
a pointer to the socket structure associated with this connection.
.pg
The socket structures are defined in \fBnet.h\fR, and are used to hold
state information and things like connection byte size,
allocations, and a pointer to the parent file. The last pointer is
used translate references to sockets into file references that
are meaningful to the daemon. Lastly, sockets tell what process to
wake up when something unusual happens (like an \fBINS\fR). 
For ICP sockets, this is
the daemon (who is sleeping on a \fBread\fR). For regular sockets, this
is the process that is reading or writing on a net file. 
.pg
Read sockets also have a pointer to a message queue, which is used
to store all incoming messages to this socket. A similar queue is
present for write sockets, but it is only used to point to a tty
when the connection is a server socket. Regular write sockets
don't have a queue, since writes only happen one at a time.
.es
.bs Important IMP Variables
.pg
There are some variables related to the IMP that are used all
over the place, and it's hard to see where they get set and used;
this section should help in that area.
.pg
\fBimpiptr\fR is often renamed \fBsktp\fR, and points to the socket structure
for the data currently arriving from the IMP. It is set by 
\fBimp_input\fR (\fBimpi1.c\fR), and used by almost all the input routines
to queue messages. If it equals zero then the input is destined
for the daemon.
.pg
\fBimp\fR is the structure that leaders get read into. It is defined
in \fBimp.h\fR, and is used primarily as an address to sleep on.
.pg
\fBimpncp\fR is set by \fBimp_init\fR to point within the \fBimp\fR stucture.
\fBimp\fR was defined, so that it can be viewed as a leader or
as a command to the daemon. Thus imp-to-host messages are sent
to the daemon, by simply telling \fBto_ncp\fR to send some bytes
from \fBimpncp\fR.
.pg
\fBimpmsg\fR points to the net message that is currently being built.
It is initialized and updated by \fBihbget\fR which is called
whenever a new "netbuf" needs to be placed under the IMP's spigot.
.es
.es
.bs The Use of Data Structures in the Daemon
.bs Files, Sockets, and skt_reqs
.pg
The table \fBfiles\fR contains an entry for each open file. This
stucture is defined in \fBfiles.h\fR, and is mainly used to
keep track of the sockets associated with this file. Note
that this structure has nothing in common with the system
file table or file headers. A file entry can keep track
of sockets that have not been allocated. That is, an entry
can say that there are four sockets reserved, but only contain
a pointer to one of them.
.pg
The socket structure which is defined in \fBsockets.h\fR serves
as a holder for state information, and as a template for
socket requests. Thus, the \fBsockets\fR table can contain
entries for live sockets or for queued sockets as in the
case of an un-solicited \fBRFC\fR from a foreign host. A socket
has a timeout field, so \fBRFC\fR's won't jam the daemon forever, and
so the daemon can make sure all of its ICP's eventually terminate.
.es
.bs Probuf's
.pg
A "probuf" is used to queue protocol messages to another host.
The array \fBh_pb_q\fR has a pointer to a "probuf" for each host. They
are sent as soon as possible, but a queue is necessary since the
foreign host might not send an \fBRFNM\fR immediately. When he does,
a bit is set in bit map \fBrfnm_bm\fR. This allows another "probuf" to
be sent.
.es
.es
.he 1 '''
.bp
.ce
.ul
Table of Contents
.CO
.en
