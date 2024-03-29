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

%OBJS

%CFILES

%LOAD

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

%RULES

# DO NOT DELETE THIS LINE -- make depend uses it

