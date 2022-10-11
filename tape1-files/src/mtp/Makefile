L = ../../lib
B = ../../bin
E = ../../etc
I = ../../include
INSTALL = mv -f
CFLAGS = -O -DTCP
LIBS= $L/libnet.a
LFLAGS= -lnet
INCLUDE = $I

all: server_mtp mailer

all.install: server_mtp.install mailer.install

server_mtp: srvrmtp mtpsrv

mtpsrv: mtpsrv.o spawn.o $(LIBS)
	$(CC) mtpsrv.o spawn.o $(LFLAGS)

mtpsrv.o: mtpsrv.c $(INCLUDE)/con.h

srvrmtp: srvrmtp.o die.o spawn.o $(LIBS)
	$(CC) srvrmtp.o die.o spawn.o $(LFLAGS)

mailer: mailer.o $(LIBS)
	$(CC) mailer.o $(LFLAGS)

mailer.install: mailer
	$(INSTALL) mailer $E/net

server_mtp.install: mtpsrv.install srvrmtp.install

mtpsrv.install: mtpsrv
	$(INSTALL) mtpsrv $E/net

srvrmtp.install: srvrmtp
	$(INSTALL) srvrmtp $E/net
