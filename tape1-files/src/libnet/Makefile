I = ../../include
U = /usr/include
L = ../../lib

INSTALL = mv -f
NAME = libnet

PUBLIC =\
	dialnum.o\
	gethost.o\
	getnet.o\
	getnhost.o\
	hashost.o\
	hostcap.o\
	hostfmt.o\
	hostname.o\
	islocal.o\
	lhostnum.o\
	netcap.o\
	netfmt.o\
	netname.o\
	thishost.o\
	thisnet.o\
	thisname.o

PRIVATE =\
	_ahostp.o\
	_atouv.o\
	_dialstr.o\
	_1parse.o\
	_2parse.o\
	_isnum.o\
	_loadmap.o\
	_namecmp.o\
	_nhosta.o\
	_nnetn.o\
	_nnetp.o\
	_shosta.o\
	_shostp.o\
	_snetn.o

$(NAME).a:	$(PUBLIC) $(PRIVATE)
	rm -f $(NAME).a
	ar rv $(NAME).a `lorder $(PUBLIC) $(PRIVATE) | tsort`

all.install:	$(NAME).install

cleanup:
	rm -f *.o

$(NAME).install: $(NAME).a $I/netlib.h
	ranlib $(NAME).a
	$(INSTALL) $(NAME).a $L

.c.o:
	$(CC) $(CF) -I$I -O -c $*.c

dialnum.o:	$I/netlib.h $I/netmap.h
gethost.o:	$I/netlib.h
getnet.o:	$I/netlib.h
getnhost.o:	$I/netlib.h
hashost.o:	$I/netlib.h
hostcap.o:	$I/netlib.h $I/netmap.h
hostfmt.o:	$I/netlib.h
hostname.o:	$I/netlib.h $I/netmap.h
islocal.o:	$I/netlib.h $I/netmap.h
lhostnum.o:	$I/netlib.h $U/sys/netparam.h
netcap.o:	$I/netlib.h $I/netmap.h
netfmt.o:	$I/netlib.h
netname.o:	$I/netlib.h $I/netmap.h
thishost.o:	$I/netlib.h $I/netmap.h
thisname.o:	$I/netlib.h $I/netfiles.h $U/stdio.h $U/ctype.h
thisnet.o:	$I/netlib.h $I/netmap.h

_ahostp.o:	$I/netlib.h $I/netmap.h
_dialstr.o:	$I/netlib.h $I/netmap.h
_2parse.o:	$I/netlib.h
_isnum.o:	$U/ctype.h
_loadmap.o:	$I/netlib.h $I/netmap.h $I/netfiles.h\
		$U/sys/types.h $U/sys/stat.h $U/stdio.h
_namecmp.o:	$I/netlib.h $I/netmap.h
_nhosta.o:	$I/netlib.h $I/netmap.h
_nnetn.o:	$I/netlib.h $I/netmap.h
_nnetp.o:	$I/netlib.h $I/netmap.h
_1parse.o:	$I/netlib.h $U/ctype.h
_shosta.o:	$I/netlib.h $I/netmap.h
_shostp.o:	$I/netlib.h $I/netmap.h
_snetn.o:	$I/netlib.h $I/netmap.h
