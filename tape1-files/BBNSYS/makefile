IDENT=-DBBNSYS -DVAX780 -DNOBADSECT -DBBNNET
PARAM=-DHZ=60 -DTIMEZONE=300 -DDST=1 -DMAXUSERS=48
#
# Makefile for Berkeley Virtual Vax/Unix, February 1981
#
# This makefile is constructed from a machine description:
#	config machine
# Most changes should be made in the machine description
#	/usr/sys/conf/``machineid''
# after which you should do
#	 config machineid
# Generic makefile changes should be made in
#	/usr/sys/conf/makefile
# after which config should be rerun for all machines.
#
# -DINSECURE	old style non-vanishing suid bits
# -DUNFAST	don't compile in inline expansions and speedups
# -DPGINPROF	gather statistics about pagein/pageout activity
# -DDISKMON	monitor disk cacheing activity (bio.c)
# -DINTRLVE	compile in code for file systems interleaved across drives
# -DTRACE	compile in kernel tracing hooks
#
C2=	/lib/c2

COPTS=	${IDENT} -DKERNEL -DVMUNIX
CFLAGS=	-O ${COPTS}

AHEADS=	../h/pcb.m

OBJS=acct.o alloc.o calloc.o clock.o fio.o iget.o ioctl.o machdep.o \
	main.o nami.o partab.o pipe.o prf.o prim.o rdwri.o rmap.o sig.o \
	slp.o subr.o sys.o sys1.o sys2.o sys3.o sys4.o sysent.o text.o \
	trap.o vmdrum.o vmmem.o vmmon.o vmpage.o vmproc.o vmpt.o \
	vmsched.o vmsubr.o vmswap.o vmsys.o udiv.o urem.o acc.o \
	autoconf.o bio.o bk.o conf.o cons.o dh.o dkbad.o dkleave.o \
	dsort.o dz.o flp.o hp.o ht.o mba.o mem.o mx1.o mx2.o pty.o sw.o \
	tty.o ttynew.o ttyold.o uba.o vaxcpu.o host.o icmp.o imp_io.o \
	ip_input.o ip_output.o mbuf.o netconf.o netuser.o netmain.o \
	raw_input.o tcp_input.o tcp_prim.o tcp_procs.o tcp_states.o 

CFILES=../sys/acct.c ../sys/alloc.c ../sys/calloc.c ../sys/clock.c \
	../sys/fio.c ../sys/iget.c ../sys/ioctl.c ../sys/machdep.c \
	../sys/main.c ../sys/nami.c ../sys/partab.c ../sys/pipe.c \
	../sys/prf.c ../sys/prim.c ../sys/rdwri.c ../sys/rmap.c \
	../sys/sig.c ../sys/slp.c ../sys/subr.c ../sys/sys.c \
	../sys/sys1.c ../sys/sys2.c ../sys/sys3.c ../sys/sys4.c \
	../sys/sysent.c ../sys/text.c ../sys/trap.c ../sys/vmdrum.c \
	../sys/vmmem.c ../sys/vmmon.c ../sys/vmpage.c ../sys/vmproc.c \
	../sys/vmpt.c ../sys/vmsched.c ../sys/vmsubr.c ../sys/vmswap.c \
	../sys/vmsys.c ../dev/acc.c ../dev/autoconf.c ../dev/bio.c \
	../dev/bk.c ../dev/conf.c ../dev/cons.c ../dev/dh.c \
	../dev/dkbad.c ../dev/dkleave.c ../dev/dsort.c ../dev/dz.c \
	../dev/flp.c ../dev/hp.c ../dev/ht.c ../dev/mba.c ../dev/mem.c \
	../dev/mx1.c ../dev/mx2.c ../dev/pty.c ../dev/sw.c ../dev/tty.c \
	../dev/ttynew.c ../dev/ttyold.c ../dev/uba.c ../dev/vaxcpu.c \
	../bbnnet/host.c ../bbnnet/icmp.c ../bbnnet/imp_io.c \
	../bbnnet/ip_input.c ../bbnnet/ip_output.c ../bbnnet/mbuf.c \
	../bbnnet/netconf.c ../bbnnet/netuser.c ../bbnnet/netmain.c \
	../bbnnet/raw_input.c ../bbnnet/tcp_input.c ../bbnnet/tcp_prim.c \
	../bbnnet/tcp_procs.c ../bbnnet/tcp_states.c 

vmunix: makefile locore.o ${OBJS} ioconf.o param.o swaphphp.o
	@echo loading vmunix
	@rm -f vmunix
	@sh ../conf/newvers.sh
	@cc $(CFLAGS) -c vers.c
	@ld -n -o vmunix -e start -x -T 80000000 locore.o ${OBJS} vers.o ioconf.o param.o swaphphp.o
	@echo rearranging symbols
	@-symorder ../sys/symbols.sort vmunix
	@size vmunix
	@chmod 755 vmunix

swaphphp.o: ../dev/swaphphp.c
	${CC} -I. -c -S ${COPTS} ../dev/swaphphp.c
	${C2} swaphphp.s | sed -f ../sys/asm.sed | ${AS} -o swaphphp.o
	rm -f swaphphp.s

all: vmunix

clean:
	rm -f eddep *vmunix* tags *.o locore.i [a-tv-z]*.s touch errs linterrs

lint: /tmp
	@lint -hbxn -I. -DGENERIC ${COPTS} ../sys/Locore.c \
	  ${CFILES} ../dev/swapgeneric.c ioconf.c | \
	    grep -v 'struct/union .* never defined' | \
	    grep -v 'possible pointer alignment problem'

../sys/symbols.sort: ../sys/symbols.raw
	grep -v '^#' ../sys/symbols.raw \
	    | sed 's/^	//' | sort | uniq > ../sys/symbols.sort

locore.o: assym.s ${AHEADS} ../sys/scb.s ../sys/locore.s ubglue.s \
    ../h/mtpr.h ../h/trap.h ../h/psl.h ../h/pte.h ../h/cpu.h ../h/mtpr.h mba.h
	cat assym.s ../sys/scb.s ../sys/locore.s ubglue.s > locore.c
	cc -E -DLOCORE ${COPTS} locore.c > locore.i
	@echo 'as -o locore.o $${AHEADS} locore.i'
	@as -o locore.o ${AHEADS} locore.i
	@rm locore.i

assym.s: ../h/param.h ../h/pte.h ../h/buf.h ../h/vmparam.h ../h/vmmeter.h \
    ../h/dir.h ../h/user.h ../h/cmap.h ../h/map.h ../h/ubavar.h ../h/proc.h
	cc ${IDENT} ../conf/genassym.c; ./genassym >assym.s; rm -f genassym

../h/param.h: /usr/include/signal.h touch
	./touch ../h/param.h
../h/tty.h: /usr/include/sgtty.h ../h/ioctl.h touch
	./touch ../h/tty.h
../h/user.h: ../h/pcb.h ../h/dmap.h ../h/vtimes.h ../h/types.h \
    /usr/include/errno.h assym.s touch
	./touch ../h/user.h
../h/vm.h: ../h/vmmac.h ../h/vmmeter.h ../h/vmparam.h ../h/vmsystm.h touch
	./touch ../h/vm.h
../sys/sysent.c: ../sys/vmsysent.c touch
	./touch ../sys/sysent.c

touch: ../conf/touch.c
	cc ../conf/touch.c -o touch

depend:
	grep '^#include' ${CFILES} | grep -v '<' | \
	      sed 's/:[^"]*"\([^"]*\)".*/: \1/' | \
	      sed 's/\.c/.o/' | sed 's,../[a-z]*/,,' | \
	awk ' { if ($$1 != prev) { print rec; rec = $$0; prev = $$1; } \
		else { if (length(rec $$2) > 78) { print rec; rec = $$0; } \
		       else rec = rec " " $$2 } } \
	      END { print rec } ' > makedep
	echo '$$r makedep' >>eddep
	echo '/^# DO NOT DELETE THIS LINE/+1,$$d' >eddep
	echo '$$r makedep' >>eddep
	echo 'w' >>eddep
	cp makefile makefile.bak
	ed - makefile < eddep
	rm eddep makedep

tags:
	/usr/ucb/ctags ${CFILES}

print:
	@pr -f makefile ../sys/TODO linterrs
	@/usr/ucb/ctags -x ${CFILES} | pr -f -h XREF
	@(size vmunix *.o) | pr -f -h sizes
	@ls -ls | pr -f
	@cd ../h; ls -ls | pr -f ; pr -f *.m *.h
	@cd ../sys; pr -f asm.sed Locore.c scb.s locore.s
	@pr -f ${CFILES}

ioconf.o: ioconf.c ../h/param.h ../h/pte.h ../h/buf.h ../h/map.h \
    ../h/mbavar.h ../h/vm.h ../h/ubavar.h
	${CC} -I. -c -S ${COPTS} ioconf.c
	${C2} ioconf.s | sed -f ../sys/asm.sed | ${AS} -o ioconf.o
	rm -f ioconf.s

param.c: ../sys/param.c
	cp ../sys/param.c .

param.o: param.c makefile
	${CC} -c -O ${IDENT} ${PARAM} param.c

acct.o: ../sys/acct.c
	${CC} -I. -c -S ${COPTS} ../sys/acct.c
	${C2} acct.s | sed -f ../sys/asm.sed | ${AS} -o acct.o
	rm -f acct.s

alloc.o: ../sys/alloc.c
	${CC} -I. -c -S ${COPTS} ../sys/alloc.c
	${C2} alloc.s | sed -f ../sys/asm.sed | ${AS} -o alloc.o
	rm -f alloc.s

calloc.o: ../sys/calloc.c
	${CC} -I. -c -S ${COPTS} ../sys/calloc.c
	${C2} calloc.s | sed -f ../sys/asm.sed | ${AS} -o calloc.o
	rm -f calloc.s

clock.o: ../sys/clock.c
	${CC} -I. -c -S ${COPTS} ../sys/clock.c
	${C2} clock.s | sed -f ../sys/asm.sed | ${AS} -o clock.o
	rm -f clock.s

fio.o: ../sys/fio.c
	${CC} -I. -c -S ${COPTS} ../sys/fio.c
	${C2} fio.s | sed -f ../sys/asm.sed | ${AS} -o fio.o
	rm -f fio.s

iget.o: ../sys/iget.c
	${CC} -I. -c -S ${COPTS} ../sys/iget.c
	${C2} iget.s | sed -f ../sys/asm.sed | ${AS} -o iget.o
	rm -f iget.s

ioctl.o: ../sys/ioctl.c
	${CC} -I. -c -S ${COPTS} ../sys/ioctl.c
	${C2} ioctl.s | sed -f ../sys/asm.sed | ${AS} -o ioctl.o
	rm -f ioctl.s

machdep.o: ../sys/machdep.c
	${CC} -I. -c -S ${COPTS} ../sys/machdep.c
	${C2} machdep.s | sed -f ../sys/asm.sed | ${AS} -o machdep.o
	rm -f machdep.s

main.o: ../sys/main.c
	${CC} -I. -c -S ${COPTS} ../sys/main.c
	${C2} main.s | sed -f ../sys/asm.sed | ${AS} -o main.o
	rm -f main.s

nami.o: ../sys/nami.c
	${CC} -I. -c -S ${COPTS} ../sys/nami.c
	${C2} nami.s | sed -f ../sys/asm.sed | ${AS} -o nami.o
	rm -f nami.s

partab.o: ../sys/partab.c
	${CC} -I. -c -S ${COPTS} ../sys/partab.c
	${C2} partab.s | sed -f ../sys/asm.sed | ${AS} -o partab.o
	rm -f partab.s

pipe.o: ../sys/pipe.c
	${CC} -I. -c -S ${COPTS} ../sys/pipe.c
	${C2} pipe.s | sed -f ../sys/asm.sed | ${AS} -o pipe.o
	rm -f pipe.s

prf.o: ../sys/prf.c
	${CC} -I. -c -S ${COPTS} ../sys/prf.c
	${C2} prf.s | sed -f ../sys/asm.sed | ${AS} -o prf.o
	rm -f prf.s

prim.o: ../sys/prim.c
	${CC} -I. -c -S ${COPTS} ../sys/prim.c
	${C2} prim.s | sed -f ../sys/asm.sed | ${AS} -o prim.o
	rm -f prim.s

rdwri.o: ../sys/rdwri.c
	${CC} -I. -c -S ${COPTS} ../sys/rdwri.c
	${C2} rdwri.s | sed -f ../sys/asm.sed | ${AS} -o rdwri.o
	rm -f rdwri.s

rmap.o: ../sys/rmap.c
	${CC} -I. -c -S ${COPTS} ../sys/rmap.c
	${C2} rmap.s | sed -f ../sys/asm.sed | ${AS} -o rmap.o
	rm -f rmap.s

sig.o: ../sys/sig.c
	${CC} -I. -c -S ${COPTS} ../sys/sig.c
	${C2} sig.s | sed -f ../sys/asm.sed | ${AS} -o sig.o
	rm -f sig.s

slp.o: ../sys/slp.c
	${CC} -I. -c -S ${COPTS} ../sys/slp.c
	${C2} slp.s | sed -f ../sys/asm.sed | ${AS} -o slp.o
	rm -f slp.s

subr.o: ../sys/subr.c
	${CC} -I. -c -S ${COPTS} ../sys/subr.c
	${C2} subr.s | sed -f ../sys/asm.sed | ${AS} -o subr.o
	rm -f subr.s

sys.o: ../sys/sys.c
	${CC} -I. -c -S ${COPTS} ../sys/sys.c
	${C2} sys.s | sed -f ../sys/asm.sed | ${AS} -o sys.o
	rm -f sys.s

sys1.o: ../sys/sys1.c
	${CC} -I. -c -S ${COPTS} ../sys/sys1.c
	${C2} sys1.s | sed -f ../sys/asm.sed | ${AS} -o sys1.o
	rm -f sys1.s

sys2.o: ../sys/sys2.c
	${CC} -I. -c -S ${COPTS} ../sys/sys2.c
	${C2} sys2.s | sed -f ../sys/asm.sed | ${AS} -o sys2.o
	rm -f sys2.s

sys3.o: ../sys/sys3.c
	${CC} -I. -c -S ${COPTS} ../sys/sys3.c
	${C2} sys3.s | sed -f ../sys/asm.sed | ${AS} -o sys3.o
	rm -f sys3.s

sys4.o: ../sys/sys4.c
	${CC} -I. -c -S ${COPTS} ../sys/sys4.c
	${C2} sys4.s | sed -f ../sys/asm.sed | ${AS} -o sys4.o
	rm -f sys4.s

sysent.o: ../sys/sysent.c
	${CC} -I. -c -S ${COPTS} ../sys/sysent.c
	${C2} sysent.s | sed -f ../sys/asm.sed | ${AS} -o sysent.o
	rm -f sysent.s

text.o: ../sys/text.c
	${CC} -I. -c -S ${COPTS} ../sys/text.c
	${C2} text.s | sed -f ../sys/asm.sed | ${AS} -o text.o
	rm -f text.s

trap.o: ../sys/trap.c
	${CC} -I. -c -S ${COPTS} ../sys/trap.c
	${C2} trap.s | sed -f ../sys/asm.sed | ${AS} -o trap.o
	rm -f trap.s

vmdrum.o: ../sys/vmdrum.c
	${CC} -I. -c -S ${COPTS} ../sys/vmdrum.c
	${C2} vmdrum.s | sed -f ../sys/asm.sed | ${AS} -o vmdrum.o
	rm -f vmdrum.s

vmmem.o: ../sys/vmmem.c
	${CC} -I. -c -S ${COPTS} ../sys/vmmem.c
	${C2} vmmem.s | sed -f ../sys/asm.sed | ${AS} -o vmmem.o
	rm -f vmmem.s

vmmon.o: ../sys/vmmon.c
	${CC} -I. -c -S ${COPTS} ../sys/vmmon.c
	${C2} vmmon.s | sed -f ../sys/asm.sed | ${AS} -o vmmon.o
	rm -f vmmon.s

vmpage.o: ../sys/vmpage.c
	${CC} -I. -c -S ${COPTS} ../sys/vmpage.c
	${C2} vmpage.s | sed -f ../sys/asm.sed | ${AS} -o vmpage.o
	rm -f vmpage.s

vmproc.o: ../sys/vmproc.c
	${CC} -I. -c -S ${COPTS} ../sys/vmproc.c
	${C2} vmproc.s | sed -f ../sys/asm.sed | ${AS} -o vmproc.o
	rm -f vmproc.s

vmpt.o: ../sys/vmpt.c
	${CC} -I. -c -S ${COPTS} ../sys/vmpt.c
	${C2} vmpt.s | sed -f ../sys/asm.sed | ${AS} -o vmpt.o
	rm -f vmpt.s

vmsched.o: ../sys/vmsched.c
	${CC} -I. -c -S ${COPTS} ../sys/vmsched.c
	${C2} vmsched.s | sed -f ../sys/asm.sed | ${AS} -o vmsched.o
	rm -f vmsched.s

vmsubr.o: ../sys/vmsubr.c
	${CC} -I. -c -S ${COPTS} ../sys/vmsubr.c
	${C2} vmsubr.s | sed -f ../sys/asm.sed | ${AS} -o vmsubr.o
	rm -f vmsubr.s

vmswap.o: ../sys/vmswap.c
	${CC} -I. -c -S ${COPTS} ../sys/vmswap.c
	${C2} vmswap.s | sed -f ../sys/asm.sed | ${AS} -o vmswap.o
	rm -f vmswap.s

vmsys.o: ../sys/vmsys.c
	${CC} -I. -c -S ${COPTS} ../sys/vmsys.c
	${C2} vmsys.s | sed -f ../sys/asm.sed | ${AS} -o vmsys.o
	rm -f vmsys.s

udiv.o: ../sys/udiv.s
	${AS} -o udiv.o ../sys/udiv.s

urem.o: ../sys/urem.s
	${AS} -o urem.o ../sys/urem.s

acc.o: ../dev/acc.c
	${CC} -I. -c -S ${COPTS} ../dev/acc.c
	${C2} -i acc.s | sed -f ../sys/asm.sed | ${AS} -o acc.o
	rm -f acc.s

autoconf.o: ../dev/autoconf.c
	${CC} -I. -c -S ${COPTS} ../dev/autoconf.c
	${C2} -i autoconf.s | sed -f ../sys/asm.sed | ${AS} -o autoconf.o
	rm -f autoconf.s

bio.o: ../dev/bio.c
	${CC} -I. -c -S ${COPTS} ../dev/bio.c
	${C2} bio.s | sed -f ../sys/asm.sed | ${AS} -o bio.o
	rm -f bio.s

bk.o: ../dev/bk.c
	${CC} -I. -c -S ${COPTS} ../dev/bk.c
	${C2} -i bk.s | sed -f ../sys/asm.sed | ${AS} -o bk.o
	rm -f bk.s

conf.o: ../dev/conf.c
	${CC} -I. -c -S ${COPTS} ../dev/conf.c
	${C2} conf.s | sed -f ../sys/asm.sed | ${AS} -o conf.o
	rm -f conf.s

cons.o: ../dev/cons.c
	${CC} -I. -c -S ${COPTS} ../dev/cons.c
	${C2} cons.s | sed -f ../sys/asm.sed | ${AS} -o cons.o
	rm -f cons.s

dh.o: ../dev/dh.c
	${CC} -I. -c -S ${COPTS} ../dev/dh.c
	${C2} -i dh.s | sed -f ../sys/asm.sed | ${AS} -o dh.o
	rm -f dh.s

dkbad.o: ../dev/dkbad.c
	${CC} -I. -c -S ${COPTS} ../dev/dkbad.c
	${C2} dkbad.s | sed -f ../sys/asm.sed | ${AS} -o dkbad.o
	rm -f dkbad.s

dkleave.o: ../dev/dkleave.c
	${CC} -I. -c -S ${COPTS} ../dev/dkleave.c
	${C2} dkleave.s | sed -f ../sys/asm.sed | ${AS} -o dkleave.o
	rm -f dkleave.s

dsort.o: ../dev/dsort.c
	${CC} -I. -c -S ${COPTS} ../dev/dsort.c
	${C2} dsort.s | sed -f ../sys/asm.sed | ${AS} -o dsort.o
	rm -f dsort.s

dz.o: ../dev/dz.c
	${CC} -I. -c -S ${COPTS} ../dev/dz.c
	${C2} -i dz.s | sed -f ../sys/asm.sed | ${AS} -o dz.o
	rm -f dz.s

flp.o: ../dev/flp.c
	${CC} -I. -c -S ${COPTS} ../dev/flp.c
	${C2} flp.s | sed -f ../sys/asm.sed | ${AS} -o flp.o
	rm -f flp.s

hp.o: ../dev/hp.c
	${CC} -I. -c -S ${COPTS} ../dev/hp.c
	${C2} -i hp.s | sed -f ../sys/asm.sed | ${AS} -o hp.o
	rm -f hp.s

ht.o: ../dev/ht.c
	${CC} -I. -c -S ${COPTS} ../dev/ht.c
	${C2} -i ht.s | sed -f ../sys/asm.sed | ${AS} -o ht.o
	rm -f ht.s

mba.o: ../dev/mba.c
	${CC} -I. -c -S ${COPTS} ../dev/mba.c
	${C2} -i mba.s | sed -f ../sys/asm.sed | ${AS} -o mba.o
	rm -f mba.s

mem.o: ../dev/mem.c
	${CC} -I. -c -S ${COPTS} ../dev/mem.c
	${C2} mem.s | sed -f ../sys/asm.sed | ${AS} -o mem.o
	rm -f mem.s

mx1.o: ../dev/mx1.c
	${CC} -I. -c -S ${COPTS} ../dev/mx1.c
	${C2} mx1.s | sed -f ../sys/asm.sed | ${AS} -o mx1.o
	rm -f mx1.s

mx2.o: ../dev/mx2.c
	${CC} -I. -c -S ${COPTS} ../dev/mx2.c
	${C2} mx2.s | sed -f ../sys/asm.sed | ${AS} -o mx2.o
	rm -f mx2.s

pty.o: ../dev/pty.c
	${CC} -I. -c -S ${COPTS} ../dev/pty.c
	${C2} pty.s | sed -f ../sys/asm.sed | ${AS} -o pty.o
	rm -f pty.s

sw.o: ../dev/sw.c
	${CC} -I. -c -S ${COPTS} ../dev/sw.c
	${C2} sw.s | sed -f ../sys/asm.sed | ${AS} -o sw.o
	rm -f sw.s

tty.o: ../dev/tty.c
	${CC} -I. -c -S ${COPTS} ../dev/tty.c
	${C2} tty.s | sed -f ../sys/asm.sed | ${AS} -o tty.o
	rm -f tty.s

ttynew.o: ../dev/ttynew.c
	${CC} -I. -c -S ${COPTS} ../dev/ttynew.c
	${C2} ttynew.s | sed -f ../sys/asm.sed | ${AS} -o ttynew.o
	rm -f ttynew.s

ttyold.o: ../dev/ttyold.c
	${CC} -I. -c -S ${COPTS} ../dev/ttyold.c
	${C2} ttyold.s | sed -f ../sys/asm.sed | ${AS} -o ttyold.o
	rm -f ttyold.s

uba.o: ../dev/uba.c
	${CC} -I. -c -S ${COPTS} ../dev/uba.c
	${C2} -i uba.s | sed -f ../sys/asm.sed | ${AS} -o uba.o
	rm -f uba.s

vaxcpu.o: ../dev/vaxcpu.c
	${CC} -I. -c -S ${COPTS} ../dev/vaxcpu.c
	${C2} vaxcpu.s | sed -f ../sys/asm.sed | ${AS} -o vaxcpu.o
	rm -f vaxcpu.s

host.o: ../bbnnet/host.c
	${CC} -I. -c -S ${COPTS} ../bbnnet/host.c
	${C2} host.s | sed -f ../sys/asm.sed | ${AS} -o host.o
	rm -f host.s

icmp.o: ../bbnnet/icmp.c
	${CC} -I. -c -S ${COPTS} ../bbnnet/icmp.c
	${C2} icmp.s | sed -f ../sys/asm.sed | ${AS} -o icmp.o
	rm -f icmp.s

imp_io.o: ../bbnnet/imp_io.c
	${CC} -I. -c -S ${COPTS} ../bbnnet/imp_io.c
	${C2} imp_io.s | sed -f ../sys/asm.sed | ${AS} -o imp_io.o
	rm -f imp_io.s

ip_input.o: ../bbnnet/ip_input.c
	${CC} -I. -c -S ${COPTS} ../bbnnet/ip_input.c
	${C2} ip_input.s | sed -f ../sys/asm.sed | ${AS} -o ip_input.o
	rm -f ip_input.s

ip_output.o: ../bbnnet/ip_output.c
	${CC} -I. -c -S ${COPTS} ../bbnnet/ip_output.c
	${C2} ip_output.s | sed -f ../sys/asm.sed | ${AS} -o ip_output.o
	rm -f ip_output.s

mbuf.o: ../bbnnet/mbuf.c
	${CC} -I. -c -S ${COPTS} ../bbnnet/mbuf.c
	${C2} mbuf.s | sed -f ../sys/asm.sed | ${AS} -o mbuf.o
	rm -f mbuf.s

netconf.o: ../bbnnet/netconf.c
	${CC} -I. -c -S ${COPTS} ../bbnnet/netconf.c
	${C2} netconf.s | sed -f ../sys/asm.sed | ${AS} -o netconf.o
	rm -f netconf.s

netuser.o: ../bbnnet/netuser.c
	${CC} -I. -c -S ${COPTS} ../bbnnet/netuser.c
	${C2} netuser.s | sed -f ../sys/asm.sed | ${AS} -o netuser.o
	rm -f netuser.s

netmain.o: ../bbnnet/netmain.c
	${CC} -I. -c -S ${COPTS} ../bbnnet/netmain.c
	${C2} netmain.s | sed -f ../sys/asm.sed | ${AS} -o netmain.o
	rm -f netmain.s

raw_input.o: ../bbnnet/raw_input.c
	${CC} -I. -c -S ${COPTS} ../bbnnet/raw_input.c
	${C2} raw_input.s | sed -f ../sys/asm.sed | ${AS} -o raw_input.o
	rm -f raw_input.s

tcp_input.o: ../bbnnet/tcp_input.c
	${CC} -I. -c -S ${COPTS} ../bbnnet/tcp_input.c
	${C2} tcp_input.s | sed -f ../sys/asm.sed | ${AS} -o tcp_input.o
	rm -f tcp_input.s

tcp_prim.o: ../bbnnet/tcp_prim.c
	${CC} -I. -c -S ${COPTS} ../bbnnet/tcp_prim.c
	${C2} tcp_prim.s | sed -f ../sys/asm.sed | ${AS} -o tcp_prim.o
	rm -f tcp_prim.s

tcp_procs.o: ../bbnnet/tcp_procs.c
	${CC} -I. -c -S ${COPTS} ../bbnnet/tcp_procs.c
	${C2} tcp_procs.s | sed -f ../sys/asm.sed | ${AS} -o tcp_procs.o
	rm -f tcp_procs.s

tcp_states.o: ../bbnnet/tcp_states.c
	${CC} -I. -c -S ${COPTS} ../bbnnet/tcp_states.c
	${C2} tcp_states.s | sed -f ../sys/asm.sed | ${AS} -o tcp_states.o
	rm -f tcp_states.s


# DO NOT DELETE THIS LINE -- make depend uses it

acct.o: ../h/param.h ../h/systm.h ../h/acct.h ../h/dir.h ../h/user.h
acct.o: ../h/inode.h ../h/proc.h ../h/seg.h
alloc.o: ../h/param.h ../h/systm.h ../h/mount.h ../h/filsys.h ../h/fblk.h
alloc.o: ../h/conf.h ../h/buf.h ../h/inode.h ../h/ino.h ../h/dir.h ../h/user.h
calloc.o: ../h/param.h ../h/dir.h ../h/user.h ../h/proc.h ../h/vm.h ../h/pte.h
calloc.o: ../h/cmap.h
clock.o: ../h/param.h ../h/systm.h ../h/dk.h ../h/callout.h ../h/seg.h
clock.o: ../h/dir.h ../h/user.h ../h/proc.h ../h/reg.h ../h/psl.h ../h/vm.h
clock.o: ../h/buf.h ../h/text.h ../h/vlimit.h ../h/mtpr.h ../h/clock.h
clock.o: ../h/cpu.h bk.h dh.h dz.h
fio.o: ../h/param.h ../h/systm.h ../h/dir.h ../h/user.h ../h/filsys.h
fio.o: ../h/file.h ../h/conf.h ../h/inode.h ../h/reg.h ../h/acct.h ../h/mount.h
iget.o: ../h/param.h ../h/systm.h ../h/mount.h ../h/dir.h ../h/user.h
iget.o: ../h/inode.h ../h/ino.h ../h/filsys.h ../h/conf.h ../h/buf.h
iget.o: ../h/inline.h
ioctl.o: ../h/param.h ../h/systm.h ../h/dir.h ../h/user.h ../h/tty.h
ioctl.o: ../h/proc.h ../h/inode.h ../h/file.h ../h/conf.h
machdep.o: ../h/param.h ../h/systm.h ../bbnnet/mbuf.h ../bbnnet/net.h
machdep.o: ../bbnnet/ifcb.h ../bbnnet/ucb.h ../h/dir.h ../h/user.h ../h/map.h
machdep.o: ../h/reg.h ../h/mtpr.h ../h/clock.h ../h/pte.h ../h/vm.h ../h/proc.h
machdep.o: ../h/psl.h ../h/buf.h ../h/ubavar.h ../h/ubareg.h ../h/cons.h
machdep.o: ../h/reboot.h ../h/conf.h ../h/mem.h ../h/cpu.h ../h/inode.h
machdep.o: ../h/file.h ../h/text.h ../h/clist.h ../h/callout.h ../h/cmap.h
machdep.o: ../h/rpb.h
main.o: ../h/param.h ../h/systm.h ../h/dir.h ../h/user.h ../h/filsys.h
main.o: ../h/mount.h ../h/map.h ../h/proc.h ../h/inode.h ../h/seg.h ../h/conf.h
main.o: ../h/buf.h ../h/mtpr.h ../h/pte.h ../h/clock.h ../h/vm.h ../h/cmap.h
main.o: ../h/text.h ../h/vlimit.h ../h/clist.h
nami.o: ../h/param.h ../h/systm.h ../h/inode.h ../h/mount.h ../h/dir.h
nami.o: ../h/user.h ../h/buf.h ../h/conf.h
pipe.o: ../h/param.h ../h/systm.h ../h/dir.h ../h/user.h ../h/inode.h
pipe.o: ../h/file.h ../h/reg.h ../h/inline.h ../h/proc.h
prf.o: ../h/param.h ../h/systm.h ../h/seg.h ../h/buf.h ../h/conf.h ../h/mtpr.h
prf.o: ../h/reboot.h ../h/vm.h ../h/msgbuf.h ../h/dir.h ../h/user.h ../h/tty.h
prim.o: ../h/param.h ../h/systm.h ../h/conf.h ../h/buf.h ../h/tty.h
prim.o: ../h/clist.h
rdwri.o: ../h/param.h ../h/systm.h ../h/inode.h ../h/dir.h ../h/user.h
rdwri.o: ../h/buf.h ../h/conf.h ../h/cmap.h ../h/vlimit.h ../h/proc.h
rmap.o: ../h/param.h ../h/systm.h ../h/map.h ../h/dir.h ../h/user.h ../h/proc.h
rmap.o: ../h/mtpr.h ../h/text.h
sig.o: ../h/param.h ../h/systm.h ../h/dir.h ../h/user.h ../h/proc.h
sig.o: ../h/inode.h ../h/reg.h ../h/text.h ../h/seg.h ../h/mtpr.h ../h/pte.h
sig.o: ../h/psl.h ../h/vm.h ../h/buf.h ../h/vlimit.h
slp.o: ../h/param.h ../h/systm.h ../h/dir.h ../h/user.h ../h/proc.h ../h/file.h
slp.o: ../h/inode.h ../h/vm.h ../h/pte.h ../h/inline.h ../h/mtpr.h
subr.o: ../h/param.h ../h/systm.h ../h/conf.h ../h/inode.h ../h/dir.h
subr.o: ../h/user.h ../h/buf.h ../h/proc.h ct.h
sys.o: ../h/param.h ../h/systm.h ../h/conf.h ../h/dir.h ../h/user.h ../h/tty.h
sys.o: ../h/proc.h
sys1.o: ../h/param.h ../h/systm.h ../h/map.h ../h/mtpr.h ../h/dir.h ../h/user.h
sys1.o: ../h/proc.h ../h/buf.h ../h/reg.h ../h/inode.h ../h/seg.h ../h/acct.h
sys1.o: /usr/include/wait.h ../h/pte.h ../h/vm.h ../h/text.h ../h/psl.h
sys1.o: ../h/vlimit.h ../h/file.h
sys2.o: ../h/param.h ../h/systm.h ../h/dir.h ../h/user.h ../h/reg.h ../h/file.h
sys2.o: ../h/inode.h ../h/pte.h ../h/vm.h ../h/buf.h ../h/mtpr.h ../h/proc.h
sys2.o: ../h/inline.h
sys3.o: ../h/param.h ../h/systm.h ../h/mount.h ../h/ino.h ../h/reg.h ../h/buf.h
sys3.o: ../h/filsys.h ../h/dir.h ../h/user.h ../h/inode.h ../h/file.h
sys3.o: ../h/conf.h ../h/stat.h ../h/inline.h
sys4.o: ../h/param.h ../h/systm.h ../h/dir.h ../h/user.h ../h/reg.h
sys4.o: ../h/inode.h ../h/proc.h ../h/clock.h ../h/mtpr.h ../h/timeb.h
sys4.o: ../h/times.h ../h/reboot.h
sysent.o: ../h/param.h ../h/systm.h ../h/vmsysent.h ../sys/vmsysent.c
text.o: ../h/param.h ../h/systm.h ../h/map.h ../h/dir.h ../h/user.h ../h/proc.h
text.o: ../h/text.h ../h/inode.h ../h/buf.h ../h/seg.h ../h/pte.h ../h/mtpr.h
text.o: ../h/vm.h ../h/cmap.h
trap.o: ../h/param.h ../h/systm.h ../h/dir.h ../h/user.h ../h/proc.h ../h/reg.h
trap.o: ../h/seg.h ../h/trap.h ../h/psl.h ../h/pte.h ../h/inline.h ../h/mtpr.h
vmdrum.o: ../h/param.h ../h/systm.h ../h/dir.h ../h/user.h ../h/proc.h
vmdrum.o: ../h/buf.h ../h/text.h ../h/map.h ../h/pte.h ../h/vm.h ../h/cmap.h
vmmem.o: ../h/param.h ../h/systm.h ../h/pte.h ../h/cmap.h ../h/dir.h
vmmem.o: ../h/user.h ../h/proc.h ../h/mtpr.h ../h/text.h ../h/vm.h ../h/file.h
vmmem.o: ../h/inode.h ../h/buf.h ../h/mount.h ../h/trace.h ../h/map.h
vmmon.o: ../h/param.h ../h/systm.h ../h/dir.h ../h/user.h ../h/vmmeter.h
vmmon.o: ../h/trace.h ../h/mtpr.h
vmpage.o: ../h/param.h ../h/systm.h ../h/inode.h ../h/dir.h ../h/user.h
vmpage.o: ../h/proc.h ../h/reg.h ../h/pte.h ../h/buf.h ../h/text.h ../h/mtpr.h
vmpage.o: ../h/cmap.h ../h/vm.h ../h/file.h ../h/trace.h
vmproc.o: ../h/param.h ../h/systm.h ../h/dir.h ../h/user.h ../h/proc.h
vmproc.o: ../h/mtpr.h ../h/pte.h ../h/map.h ../h/cmap.h ../h/text.h ../h/vm.h
vmpt.o: ../h/param.h ../h/systm.h ../h/dir.h ../h/user.h ../h/proc.h ../h/map.h
vmpt.o: ../h/mtpr.h ../h/pte.h ../h/cmap.h ../h/vm.h ../h/buf.h ../h/text.h
vmpt.o: ../h/mount.h ../h/inode.h
vmsched.o: ../h/param.h ../h/systm.h ../h/seg.h ../h/dir.h ../h/user.h
vmsched.o: ../h/proc.h ../h/text.h ../h/vm.h ../h/cmap.h
vmsubr.o: ../h/param.h ../h/systm.h ../h/dir.h ../h/user.h ../h/vm.h
vmsubr.o: ../h/proc.h ../h/mtpr.h ../h/pte.h ../h/cmap.h ../h/inode.h
vmsubr.o: ../h/buf.h ../h/text.h
vmswap.o: ../h/param.h ../h/systm.h ../h/dir.h ../h/user.h ../h/proc.h
vmswap.o: ../h/text.h ../h/map.h ../h/buf.h ../h/pte.h ../h/mtpr.h ../h/cmap.h
vmswap.o: ../h/vm.h
vmsys.o: ../h/param.h ../h/systm.h ../h/dir.h ../h/user.h ../h/proc.h
vmsys.o: ../h/reg.h ../h/file.h ../h/inode.h ../h/vm.h ../h/buf.h ../h/pte.h
vmsys.o: ../h/cmap.h ../h/tty.h ../h/mtpr.h ../h/vlimit.h ../h/trace.h
acc.o: acc.h ../h/param.h ../h/systm.h ../bbnnet/mbuf.h ../bbnnet/net.h
acc.o: ../bbnnet/imp.h ../bbnnet/ifcb.h ../h/map.h ../h/pte.h ../h/buf.h
acc.o: ../h/ubareg.h ../h/ubavar.h ../h/conf.h ../h/dir.h ../h/user.h
acc.o: ../h/accreg.h
autoconf.o: mba.h ../h/param.h ../h/systm.h ../h/map.h ../h/nexus.h ../h/pte.h
autoconf.o: ../h/buf.h ../h/mbareg.h ../h/mbavar.h ../h/dk.h ../h/vm.h
autoconf.o: ../h/ubareg.h ../h/ubavar.h ../h/mtpr.h ../h/cpu.h ../h/scb.h
autoconf.o: ../h/mem.h
bio.o: ../h/param.h ../h/systm.h ../h/dir.h ../h/user.h ../h/buf.h ../h/conf.h
bio.o: ../h/proc.h ../h/seg.h ../h/pte.h ../h/vm.h ../h/trace.h
bk.o: bk.h ../h/param.h ../h/systm.h ../h/dir.h ../h/user.h ../h/tty.h
bk.o: ../h/proc.h ../h/mx.h ../h/inode.h ../h/file.h ../h/conf.h ../h/buf.h
conf.o: ../h/param.h ../h/systm.h ../h/buf.h ../h/tty.h ../h/conf.h ../h/text.h
conf.o: ../h/dir.h ../h/user.h ../h/proc.h ../h/file.h ../h/inode.h ../h/acct.h
conf.o: ../h/pte.h hp.h tu.h rk.h te.h ts.h up.h acc.h ct.h dh.h dz.h lp.h va.h
conf.o: vp.h pty.h bk.h
cons.o: ../h/param.h ../h/conf.h ../h/dir.h ../h/user.h ../h/tty.h ../h/systm.h
cons.o: ../h/cons.h ../h/mtpr.h ../h/mx.h ../h/cpu.h
dh.o: dh.h bk.h ../h/param.h ../h/conf.h ../h/dir.h ../h/user.h ../h/tty.h
dh.o: ../h/map.h ../h/pte.h ../h/buf.h ../h/vm.h ../h/ubareg.h ../h/ubavar.h
dh.o: ../h/bk.h ../h/clist.h ../h/mx.h ../h/file.h
dkbad.o: ../h/param.h ../h/buf.h ../h/dkbad.h
dkleave.o: ../h/param.h ../h/buf.h
dsort.o: ../h/param.h ../h/systm.h ../h/buf.h
dz.o: dz.h bk.h ../h/param.h ../h/systm.h ../h/tty.h ../h/dir.h ../h/user.h
dz.o: ../h/map.h ../h/pte.h ../h/buf.h ../h/vm.h ../h/ubavar.h ../h/conf.h
dz.o: ../h/pdma.h ../h/bk.h ../h/file.h ../h/mx.h
flp.o: ../h/flp.h ../h/param.h ../h/systm.h ../h/conf.h ../h/dir.h ../h/user.h
flp.o: ../h/mtpr.h ../h/buf.h ../h/cons.h ../h/cpu.h
hp.o: hp.h ../h/param.h ../h/systm.h ../h/dk.h ../h/buf.h ../h/conf.h
hp.o: ../h/dir.h ../h/user.h ../h/map.h ../h/pte.h ../h/mbareg.h ../h/mbavar.h
hp.o: ../h/mtpr.h ../h/vm.h ../h/cmap.h ../h/dkbad.h ../h/hpreg.h
ht.o: tu.h ../h/param.h ../h/systm.h ../h/buf.h ../h/conf.h ../h/dir.h
ht.o: ../h/file.h ../h/user.h ../h/map.h ../h/pte.h ../h/mbareg.h ../h/mbavar.h
ht.o: ../h/mtio.h ../h/ioctl.h ../h/cmap.h ../h/cpu.h ../h/htreg.h
mba.o: mba.h ../h/param.h ../h/systm.h ../h/dk.h ../h/buf.h ../h/conf.h
mba.o: ../h/dir.h ../h/user.h ../h/proc.h ../h/map.h ../h/pte.h ../h/mbareg.h
mba.o: ../h/mbavar.h ../h/mtpr.h ../h/vm.h
mem.o: ../h/param.h ../h/dir.h ../h/user.h ../h/conf.h ../h/buf.h ../h/systm.h
mem.o: ../h/pte.h ../h/mtpr.h ../h/vm.h ../h/cmap.h
mx1.o: ../h/param.h ../h/systm.h ../h/dir.h ../h/user.h ../h/reg.h ../h/proc.h
mx1.o: ../h/tty.h ../h/inode.h ../h/mx.h ../h/file.h ../h/conf.h
mx2.o: ../h/param.h ../h/systm.h ../h/dir.h ../h/user.h ../h/proc.h ../h/tty.h
mx2.o: ../h/inode.h ../h/mx.h ../h/file.h ../h/conf.h ../h/buf.h
pty.o: pty.h ../h/param.h ../h/systm.h ../h/tty.h ../h/dir.h ../h/user.h
pty.o: ../h/conf.h ../h/buf.h ../h/file.h
sw.o: ../h/param.h ../h/systm.h ../h/buf.h ../h/conf.h ../h/dir.h ../h/user.h
sw.o: ../h/inode.h ../h/map.h
tty.o: ../h/param.h ../h/systm.h ../h/dir.h ../h/user.h ../h/tty.h ../h/proc.h
tty.o: ../h/mx.h ../h/inode.h ../h/file.h ../h/reg.h ../h/conf.h ../h/buf.h
tty.o: ../h/dk.h
ttynew.o: ../h/param.h ../h/systm.h ../h/dir.h ../h/user.h ../h/tty.h
ttynew.o: ../h/proc.h ../h/mx.h ../h/inode.h ../h/file.h ../h/reg.h ../h/conf.h
ttynew.o: ../h/buf.h ../h/dk.h
ttyold.o: ../h/param.h ../h/systm.h ../h/dir.h ../h/user.h ../h/tty.h
ttyold.o: ../h/proc.h ../h/mx.h ../h/inode.h ../h/file.h ../h/reg.h ../h/conf.h
ttyold.o: ../h/buf.h ../h/dk.h
uba.o: ../h/param.h ../h/systm.h ../h/cpu.h ../h/map.h ../h/pte.h ../h/buf.h
uba.o: ../h/vm.h ../h/ubareg.h ../h/ubavar.h ../h/dir.h ../h/user.h ../h/proc.h
uba.o: ../h/conf.h ../h/mtpr.h ../h/nexus.h ../h/dk.h
vaxcpu.o: ../h/param.h ../h/pte.h ../h/ubareg.h ../h/nexus.h ../h/cpu.h
host.o: ../h/param.h ../bbnnet/mbuf.h ../bbnnet/net.h
icmp.o: ../h/param.h ../h/systm.h ../bbnnet/mbuf.h ../bbnnet/net.h
icmp.o: ../bbnnet/ifcb.h ../bbnnet/tcp.h ../bbnnet/ip.h ../bbnnet/icmp.h
icmp.o: ../bbnnet/ucb.h
imp_io.o: ../h/param.h ../h/dir.h ../h/user.h ../bbnnet/mbuf.h ../bbnnet/net.h
imp_io.o: ../bbnnet/ifcb.h ../bbnnet/ip.h ../bbnnet/imp.h ../bbnnet/ucb.h
imp_io.o: ../bbnnet/fsm.h
ip_input.o: ../h/param.h ../bbnnet/mbuf.h ../bbnnet/net.h ../bbnnet/ifcb.h
ip_input.o: ../bbnnet/tcp.h ../bbnnet/ip.h ../bbnnet/icmp.h ../bbnnet/ucb.h
ip_output.o: ../h/param.h ../h/dir.h ../h/user.h ../bbnnet/mbuf.h
ip_output.o: ../bbnnet/net.h ../bbnnet/tcp.h ../bbnnet/ip.h ../bbnnet/ifcb.h
ip_output.o: ../bbnnet/ucb.h
mbuf.o: ../h/param.h ../h/pte.h ../h/cmap.h ../h/map.h ../bbnnet/mbuf.h
mbuf.o: ../bbnnet/net.h ../bbnnet/tcp.h ../bbnnet/ip.h ../h/vm.h
netconf.o: ../h/param.h ../h/accreg.h ../bbnnet/mbuf.h ../bbnnet/net.h
netconf.o: ../bbnnet/ifcb.h
netuser.o: ../h/param.h ../h/systm.h ../h/buf.h ../h/dir.h ../h/file.h
netuser.o: ../h/inode.h ../h/user.h ../h/proc.h ../bbnnet/mbuf.h
netuser.o: ../bbnnet/net.h ../bbnnet/imp.h ../bbnnet/ifcb.h ../bbnnet/tcp.h
netuser.o: ../bbnnet/ucb.h ../bbnnet/fsm.h ../bbnnet/tcp_pred.h
netmain.o: ../h/param.h ../h/inode.h ../bbnnet/mbuf.h ../bbnnet/net.h
netmain.o: ../bbnnet/ifcb.h ../h/buf.h ../h/dir.h ../h/user.h ../h/proc.h
raw_input.o: ../h/param.h ../bbnnet/mbuf.h ../bbnnet/net.h ../bbnnet/ifcb.h
raw_input.o: ../bbnnet/tcp.h ../bbnnet/ip.h ../bbnnet/ucb.h
tcp_input.o: ../h/param.h ../h/systm.h ../bbnnet/mbuf.h ../bbnnet/net.h
tcp_input.o: ../bbnnet/ifcb.h ../bbnnet/imp.h ../bbnnet/ucb.h ../bbnnet/tcp.h
tcp_input.o: ../bbnnet/ip.h ../h/dir.h ../h/user.h ../h/inode.h ../bbnnet/fsm.h
tcp_input.o: ../bbnnet/fsmdef.h
tcp_prim.o: ../h/param.h ../bbnnet/mbuf.h ../bbnnet/net.h ../bbnnet/ifcb.h
tcp_prim.o: ../bbnnet/tcp.h ../bbnnet/ip.h ../bbnnet/imp.h ../bbnnet/ucb.h
tcp_prim.o: ../bbnnet/fsm.h
tcp_procs.o: ../h/param.h ../bbnnet/mbuf.h ../bbnnet/net.h ../bbnnet/tcp.h
tcp_procs.o: ../bbnnet/ip.h ../bbnnet/imp.h ../bbnnet/ucb.h ../bbnnet/fsm.h
tcp_procs.o: ../bbnnet/tcp_pred.h
tcp_states.o: ../h/param.h ../bbnnet/mbuf.h ../bbnnet/net.h ../bbnnet/ifcb.h
tcp_states.o: ../bbnnet/tcp.h ../bbnnet/ip.h ../bbnnet/imp.h ../bbnnet/ucb.h
tcp_states.o: ../bbnnet/fsm.h ../bbnnet/tcp_pred.h
