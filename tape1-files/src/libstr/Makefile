L = ../../lib

CF =
AF = rv

INSTALL = mv -f

all:	libstr.a

all.install: libstr.install

cleanup:
	rm -f *.o

libstr.a:\
	pcmp.o\
	safter.o\
	sall.o\
	salloc.o\
	sappend.o\
	sauto.o\
	sbcopy.o\
	sbefor.o\
	sbegin.o\
	sbfind.o\
	sbskip.o\
	scat.o\
	scatn.o\
	scmp.o\
	scopy.o\
	scopyn.o\
	send.o\
	seq.o\
	seqcas.o\
	serror.o\
	sfind.o\
	sindex.o\
	sinit.o\
	slen.o\
	smatch.o\
	smove.o\
	srev.o\
	ssearc.o\
	sskip.o\
	sslash.o\
	stokeq.o\
	string.o\
	strunc.o\
	substr.o\
	sunslash.o\
	sverif.o\
	vcmp.o\
	veq.o\
	veqcas.o\
	vmove.o
	rm -f libstr.a
	ar $(AF) libstr.a `lorder $! | tsort`

libstr.install: libstr.a
	$(INSTALL) libstr.a $L
	ranlib $L/libstr.a

.c.o:
	$(CC) $(CF) -O -c $*.c

sall.o\
salloc.o\
sauto.o\
sbefor.o\
sbegin.o\
scat.o\
scatn.o\
send.o\
sinit.o\
sslash.o\
string.o\
strunc.o\
substr.o\
sunslash.o:\
	string.h
