I = ../../include
U = /usr/include
L = ../../lib
B = ../../bin
E = ../../etc

CF =
LF =
LL = $L/libnet.a $L/libbbn.a $L/libstr.a

INSTALL = mv -f

all:	mkhost prhost host dumpmap loadmap

all.install:	mkhost.install prhost.install host.install

cleanup:
	rm -f *.o

mkhost:	mkhost.o $(LL)
	$(CC) $(LF) mkhost.o $(LL) -o mkhost

mkhost.install:	mkhost
	$(INSTALL) mkhost $E/net

prhost:	prhost.o $(LL)
	$(CC) $(LF) prhost.o $(LL) -o prhost

prhost.install:	prhost
	$(INSTALL) prhost $E/net

host:	host.o $(LL)
	$(CC) $(LF) host.o $(LL) -o host

host.install:	host
	$(INSTALL) host $B
	rm -f $B/net
	ln $B/host $B/net

dumpmap: dumpmap.o
	$(CC) $(LF) dumpmap.o -o dumpmap

loadmap: loadmap.o $(LL)
	$(CC) $(LF) loadmap.o $(LL) -o loadmap

.c.o:
	$(CC) $(CF) -I$I -O -c $*.c

mkhost.o:\
	$U/stdio.h $U/ctype.h\
	$I/netlib.h $I/netmap.h $I/netfiles.h

prhost.o:\
	$U/stdio.h $U/sys/types.h $U/sys/stat.h\
	$I/netlib.h $I/netmap.h

host.o:\
	$U/stdio.h $I/netlib.h

dumpmap.o:\
	$U/stdio.h\
	$I/netlib.h $I/netmap.h $I/netfiles.h
