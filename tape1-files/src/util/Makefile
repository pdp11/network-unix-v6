U = /usr/include
N = /usr/src/netsys/bbnnet
B = ../../bin

CF = 
LF =

INSTALL = mv -f

all: mkgate netdebug netginit netreset netstat tcptest trpt

all.install: mkgate.install netdebug.install netginit.install netreset.install netstat.install tcptest.install trpt.install

cleanup:
	rm -f *.o

mkgate:		mkgate.o
	$(CC) $(LF) mkgate.o ../../lib/libnet.a -o mkgate

mkgate.install: mkgate
	$(INSTALL) mkgate $B

netdebug:	netdebug.o
	$(CC) $(LF) netdebug.o -o netdebug

netdebug.install: netdebug
	$(INSTALL) netdebug $B

netginit:	netginit.o
	$(CC) $(LF) netginit.o -o netginit

netginit.install: netginit
	$(INSTALL) netginit $B

netreset: netreset.o
	$(CC) $(LF) netreset.o -o netreset

netreset.install: netreset
	$(INSTALL) netreset $B

netstat: netstat.o
	$(CC) $(LF) netstat.o ../../lib/libnet.a -o netstat

netstat.install: netstat
	$(INSTALL) netstat $B

tcptest: tcptest.o
	$(CC) $(LF) tcptest.o ../../lib/libnet.a -o tcptest

tcptest.install: tcptest
	$(INSTALL) tcptest $B

trpt: trpt.o
	$(CC) $(LF) trpt.o -o trpt

trpt.install: trpt
	$(INSTALL) trpt $B

.c.o:
	$(CC) $(CF) -I$N -O -c $*.c

mkgate:		$U/netlib.h $N/net.h $N/ifcb.h

netdebug.o:	$U/netlib.h $N/net.h $N/con.h

netginit.o:	$U/netlib.h $N/net.h $N/con.h

netreset.o:	$U/netlib.h $N/net.h $N/con.h

netstat.o:	$N/net.h $N/con.h $N/ucb.h $N/tcp.h\
	$U/stdio.h $U/sys/param.h $U/sys/dir.h\
	$U/sys/user.h $U/sys/proc.h $U/nlist.h $U/netlib.h

tcptest.o:	$N/net.h $N/con.h\
	$U/sys/types.h $U/sys/timeb.h $U/signal.h $U/errno.h $U/netlib.h

trpt.o:	$N/net.h $N/fsm.h $N/fsmdef.h
