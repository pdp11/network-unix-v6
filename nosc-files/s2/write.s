/
/ write4 -- write to another user
/
/ as write4.s;ld -n -s a.out -la;mv a.out /bin/write

/ This write command corrects many deficiencies of previous versions
/ Fixed by Greg CHesson, Jody Kravitz, Rick Balocca,
/ 	at the University of Illinois
/
/ fixes :
/ write is in raw mode
/ errors from write system calls are noted
/ signals are turned off in parent if child is forked (!escapes)
/ rubout or eot ends write
/
.globl	ttyn

	cmp	(sp)+,$2
	beq	1f
	bgt	2f
	mov	$1,r0
	sys	write; argm; eargm-argm
	sys	exit
2:
	movb	*4(sp),ltty
1:
	tst	(sp)+
	mov	(sp)+,r5
	sys	open; utmp; 0
	bec	1f
	mov	$1,r0
	sys	write; film; efilm-film
	sys	exit
1:
	mov	r0,ufil
1:
	mov	ufil,r0
	sys	read; ubuf; 16.
	tst	r0
	bne	2f
	jmp	8f
2:
	tstb	ltty
	beq	2f
	cmpb	ltty,8.+ubuf
	bne	1b
2:
	mov	$ubuf,r3
	mov	r5,r4
	mov	$9.,r2
2:
	dec	r2
	beq	2f
	cmpb	(r4)+,(r3)+
	beq	2b
	tstb	-1(r4)
	bne	1b
	cmpb	$' ,-1(r3)
	bne	1b
2:
	movb	8.+ubuf,ttyno
	sys	open; ttyx; 1
	bes	3f
	sys	stat; ttyx; statbuf
	bes	3f
	bit	$2,statbuf+4
	bne	2f
3:
	mov	$1,r0
	sys	write; dnymes; ednymes-dnymes
	sys	exit
2:
	mov	r0,ttyf
	clr	r0
	jsr	pc,ttyn
	mov	r0,r3
	mov	statbuf,r4
	mov	ufil,r0
	sys	seek; 0; 0
1:
	mov	ufil,r0
	sys	read; ubuf; 16.
	tst	r0
	jeq	unknown
	cmp	r3,ubuf+8.
	bne	1b
	mov	$ubuf,r0
	mov	$8.,r1
1:
	cmpb	$' ,(r0)+
	beq	1f
	dec	r1
	bne	1b
1:
	neg	r1
	add	$8,r1
	mov	r1,0f
6:
	mov	ttyf,r0
	sys	write; mesg; emesg-mesg
	jes	wfailed
	cmp	r0,$emesg-mesg
	jlt	wfailed
	mov	ttyf,r0
	sys	0; 5f
.data
5:
	sys	write; ubuf; 0:2
.text
	/ Note: the lack of check for error return
	mov	ttyf,r0
	sys	write; qnl; 4
	jes	wfailed
	cmp	r0,$4		/* number of chars we attempted to write
	jlt	wfailed
/sys	signal; 2; 9f
	clr	r0
	sys	gtty; vec
	mov	$vec,r3
	mov	$sav,r2
	mov	(r3)+,(r2)+
	mov	(r3)+,(r2)+
	mov	(r3),(r2)
	bis	$040,(r3)
	sys	stty; vec
	/ Note: no error checking
/
/ it sure would be nice if we output a bell here to let
/ the user know...... (jsk 4Jan77)
	mov	$1,r0
	sys	write; belmes;ebelmes-belmes
	jes	wfail2
	cmp	r0,$ebelmes-belmes
	jlt	wfail2
/
	inc	rawflg
7:
	clr	r0
	sys	read; ch; 1
	tst	r0
	beq	9f
	bic	$177600,ch
	cmp	ch,$04
	beq	9f
	cmp	ch,$0177
	beq	9f
	tst	nlflg
	beq	1f
	cmp	ch,$'!
	bne	1f
	sys	stty; sav
	sys	fork
		br mshproc
	sys	signal;2;1
	mov	r0,savsig2
	sys	signal;3;1
	mov	r0,savsig3
	sys	wait
	sys	0;ressig2
	sys	0;ressig3
.data
ressig2:	sys	signal;2;savsig2:	0
ressig3:	sys	signal;3;savsig3:	0
.text
	sys	signal;2;savsig3
	clr	r0
	sys	stty; vec
	mov	$1,r0
	sys	write; excl; 2
	br	7b
1:
	clr	nlflg
	cmp	ch,$'\n
	bne	1f
	inc	nlflg
1:
	mov	ttyf,r0
	sys	write; ch; 1
	jes	wfailed
	tst	r0
	jle	wfailed
	/* testing...
/	mov	$1,r0
/	sys	write; ok;eok-ok;
	br	7b
8:
	movb	(r5)+,ch
	beq	8f
	mov	$1,r0
	sys	write; ch; 1
	br	8b
8:
	tstb	ltty
	beq	8f
	mov	$1,r0
	sys	write; ltty-1; 2
8:
	mov	$1,r0
	sys	write; errmsg; eerrmsg-errmsg
	sys	exit
9:
	mov	ttyf,r0
	sys	write; endmsg; eendmsg-endmsg
	jes	wfailed
	cmp	r0,$eendmsg-endmsg
	jlt	wfailed
	tst	rawflg
	beq	1f
	clr	r0
wfail2:
	sys	stty; sav
1:
	sys	exit

unknown:
	mov	$"??,ubuf
	jbr	6b

mshproc:
	sys	exec; msh; mshp
	sys	exit

wfailed:
	mov	$1,r0
	sys	write; gone;egone-gone
	jbr	wfail2
.data
nlflg:
	1
rawflg:
	0
.text

mshp:
	msh
	minust
	0
msh:
	</bin/sh\0>
minust:
	<-t\0>
argm:
	<Arg count\n>
eargm:
film:
	<Cannot open utmp\n>
efilm:
.data
	< >		/ is ltty -1
ltty:
	.=.+1
.text
excl:
	<!\n>
qnl:
	<...\n>
.data
ttyx:
	</dev/ttyx\0>
ttyno	= .-2
.text
utmp:
	</etc/utmp\0>
endmsg:
	<EOT\n>
eendmsg:
errmsg:
	< not logged in.\n>
eerrmsg:
mesg:
	<\nMessage from >
emesg:
dnymes:
	<Permission denied.\n>
ednymes:
gone:
	<Other side closed\n>
egone:
belmes:
	<>
ebelmes:
/ok:
/	<I'm ok\n>
/eok:
	.even
	.bss

ttyf:	.=.+2
ubuf:	.=.+16.
statbuf:.=.+40.
ch:	.=.+2
ufil:	.=.+2
vec:	.=.+6
sav:	.=.+6
signal = 48.
