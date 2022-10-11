I = ../../include
U = /usr/include
L = ../../lib
B = ../../bin
E = ../../etc

CF =
LF =
LL = $L/libnet.a $L/libbbn.a $L/libstr.a

INSTALL = mv -f


all:	telnet netser

all.install: telnet.install netser.install

cleanup:
	rm -f *.o

TELNET =\
	user_2p.o\
	process.o\
	cmds.o\
	options.o\
	globals.o\
	args.o\
	ttyctl.o\
	libtn.a\
	$(LL)

telnet:	$(TELNET)
	$(CC) $(LF) $(TELNET) -ljobs -o telnet

telnet.install: telnet
	$(INSTALL) telnet $B

libtn.a: tel.o netc.o
	ar rv libtn.a tel.o netc.o
	ranlib libtn.a

netser: netser.o $(LL)
	$(CC) $(LF) netser.o $(LL) -o netser

netser.install: netser
	$(INSTALL) netser $E/net

.c.o:
	$(CC) $(CF) -I$I -DTCP -O -c $*.c

cmds.o: \
	$U/stdio.h $U/signal.h\
	$I/con.h\
	tnio.h telnet.h globdefs.h ttyctl.h

globals.o: globdefs.h tnio.h ttyctl.h

netc.o: $U/stdio.h tnio.h telnet.h

netser.o:\
	$U/signal.h $U/sgtty.h $U/stdio.h\
	$I/con.h\
	telnet.h

options.o: $U/stdio.h globdefs.h tnio.h telnet.h ttyctl.h

process.o: $U/stdio.h tnio.h globdefs.h ttyctl.h

tel.o:\
	$U/signal.h $U/errno.h\
	$I/con.h $I/netlib.h\
	tnio.h telnet.h

ttyctl.o: $U/stdio.h ttyctl.h

user_2p.o: $U/stdio.h $U/signal.h globdefs.h tnio.h telnet.h ttyctl.h

ttyctl.h:	$U/sgtty.h
