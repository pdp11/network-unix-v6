I = ../../include
U = /usr/include
L = ../../lib
B = ../../bin
E = ../../etc

CF =
LF = -n
LL = $L/libnet.a $L/libbbn.a $L/libstr.a

INSTALL = mv -f

all:	ftp ftpsrv srvrftp

all.install: ftp.install ftpsrv.install srvrftp.install

cleanup:
	rm -f *.o

ftp:	ftp.o die.o tcp_usr.o tcp_fns.o ftp_lib.o $(LL)
	$(CC) $(LF) ftp.o die.o tcp_usr.o tcp_fns.o ftp_lib.o $(LL) -o ftp

ftp.install: ftp
	$(INSTALL) ftp $B

ftpsrv: ftpsrv.o tcp_fns.o spawn.o die.o $(LL)
	$(CC) $(LF) ftpsrv.o tcp_fns.o spawn.o die.o $(LL) -o ftpsrv

ftpsrv.install: ftpsrv
	$(INSTALL) ftpsrv $E/net

srvrftp: srvrftp.o tcp_fns.o spawn.o die.o tcp_srv.o ftp_lib.o $(LL)
	$(CC) $(LF) srvrftp.o tcp_fns.o spawn.o die.o tcp_srv.o ftp_lib.o $(LL) -o srvrftp

srvrftp.install: srvrftp
	$(INSTALL) srvrftp $E/net

.c.o:
	$(CC) $(CF) -I$I -DTCP -O -c $*.c

ftp.o:	ftp.h usr.h ftptelnet.h ftp_lib.h\
	$U/ctype.h $U/sgtty.h $U/setjmp.h\
	$U/sys/types.h $U/sys/stat.h $U/sys/timeb.h

ftpsrv.o:	ftp.h

srvrftp.o:	srv.h

tcp_srv.o:	ftp.h

tcp_fns.o:	ftp.h

tcp_usr.o:	ftp.h usr.h

ftp_lib.o: ftp_lib.h ftp.h\
	$U/stdio.h $U/errno.h $U/sys/types.h $U/sys/timeb.h

spawn.o:	$U/errno.h

die.o:	$U/stdio.h $U/signal.h

ftp.h:	$I/netlib.h $I/con.h\
	$U/stdio.h $U/ctype.h $U/signal.h

srv.h:	ftp.h ftp_lib.h\
	$U/sys/types.h $U/sys/stat.h $U/errno.h

usr.h:	$U/signal.h $U/errno.h
