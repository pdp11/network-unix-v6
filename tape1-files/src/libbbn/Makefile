U = /usr/include
L = ../../lib

CF =
AF = rv

INSTALL = mv -f

all:	libbbn.a

all.install: libbbn.install

cleanup:
	rm -f *.o

libbbn.a:\
	abspath.o\
	atoiv.o\
	atolv.o\
	cmderr.o\
	dotpath.o\
	errmsg.o\
	tpass.o
	rm -f libbbn.a
	ar $(AF) libbbn.a `lorder $! | tsort`

libbbn.install: libbbn.a
	$(INSTALL) libbbn.a $L
	ranlib $L/libbbn.a

.c.o:
	$(CC) $(CF) -O -c $*.c

abspath.o: $U/sys/types.h $U/sys/stat.h

cmderr.o: $U/stdio.h
