	"console",	/* fixed-location DL11 */
	-1,	60,	CHAR+INTR+KL,
	"\tklin; br4\n\tklou; br4\n",
	".globl\t_klrint\nklin:\tjsr\tr0,call; _klrint\n",
	".globl\t_klxint\nklou:\tjsr\tr0,call; _klxint\n",
	"",
	"\t&klopen,   &klclose,  &klread,   &klwrite,  &klsgtty,",

	"pc",	/* paper tape reader/punch */
	0,	70,	CHAR+INTR,
	"\tpcin; br4\n\tpcou; br4\n",
	".globl\t_pcrint\npcin:\tjsr\tr0,call; _pcrint\n",
	".globl\t_pcpint\npcou:\tjsr\tr0,call; _pcpint\n",
	"",
	"\t&pcopen,   &pcclose,  &pcread,   &pcwrite,  &nodev,",

	"clock",
	-2,	100,	INTR,
	"\tkwlp; br6\n",
	".globl\t_clock\n",
	"kwlp:\tjsr\tr0,call; _clock\n",
	"",
	"",

/*
 * 110 unused
 * 114 11/70 parity
 * 120 XY plotter
 * 124 DR11-B DMA interface
 * 130 AD01 A/D interface
 * 134 AFC11 analog subsystem
 * 140 AA11 display
 * 144 AA11 light pen
 * 150-164 unused
 * 170  ((one of the imp interfaces))
 */
	"si",	/* controller for multiple RPs */
	0,	170,	BLOCK+CHAR+INTR,
	"\tsiio; br5\n",
	".globl\t_siintr\n",
	"siio:\tjsr\tr0,call; _siintr\n",
	"\t&nulldev,\t&nulldev,\t&sistrategy, \t&sitab,",
	"\t&nulldev,  &nulldev,  &siread,   &siwrite,  &nodev,",


	"lp",	/* line printer */
	0,	200,	CHAR+INTR,
	"\tlpou; br4\n",
	"",
	".globl\t_lpint\nlpou:\tjsr\tr0,call; _lpint\n",
	"",
	"\t&lpopen,   &lpclose,  &nodev,    &lpwrite,  &nodev,",

	"rf",	/* fixed-head disk */
	0,	204,	BLOCK+CHAR+INTR,
	"\trfio; br5\n",
	".globl\t_rfintr\n",
	"rfio:\tjsr\tr0,call; _rfintr\n",
	"\t&nulldev,\t&nulldev,\t&rfstrategy, \t&rftab,",
	"\t&nulldev,  &nulldev,  &rfread,   &rfwrite,  &nodev,",

	"hs",	/* controller for multiple RFs */
	0,	204,	BLOCK+CHAR+INTR,
	"\thsio; br5\n",
	".globl\t_hsintr\n",
	"hsio:\tjsr\tr0,call; _hsintr\n",
	"\t&nulldev,\t&nulldev,\t&hsstrategy, \t&hstab,",
	"\t&nulldev,  &nulldev,  &hsread,   &hswrite,  &nodev,",

/*
 * 210 RC disk
 */

	"tc",	/* DECtape */
	0,	214,	BLOCK+INTR,
	"\ttcio; br6\n",
	".globl\t_tcintr\n",
	"tcio:\tjsr\tr0,call; _tcintr\n",
	"\t&nulldev,\t&tcclose,\t&tcstrategy, \t&tctab,",
	"",

	"rk",	/* cartridge disk */
	0,	220,	BLOCK+CHAR+INTR,
	"\trkio; br5\n",
	".globl\t_rkintr\n",
	"rkio:\tjsr\tr0,call; _rkintr\n",
	"\t&nulldev,\t&nulldev,\t&rkstrategy, \t&rktab,",
	"\t&nulldev,  &nulldev,  &rkread,   &rkwrite,  &nodev,",

	"tm",	/* 800-bpi 9-track tape */
	0,	224,	BLOCK+CHAR+INTR,
	"\ttmio; br5\n",
	".globl\t_tmintr\n",
	"tmio:\tjsr\tr0,call; _tmintr\n",
	"\t&tmopen,\t&tmclose,\t&tmstrategy, \t&tmtab,",
	"\t&tmopen,   &tmclose,  &tmread,   &tmwrite,  &nodev,",

	"ht",	/* 1600-bpi 9-track tape */
	0,	224,	BLOCK+CHAR+INTR,
	"\thtio; br5\n",
	".globl\t_htintr\n",
	"htio:\tjsr\tr0,call; _htintr\n",
	"\t&htopen,\t&htclose,\t&htstrategy, \t&httab,",
	"\t&htopen,   &htclose,  &htread,   &htwrite,  &nodev,",

	"cr",	/* card reader */
	0,	230,	CHAR+INTR,
	"\tcrin; br6\n",
	"",
	".globl\t_crint\ncrin:\tjsr\tr0,call; _crint\n",
	"",
	"\t&cropen,   &crclose,  &crread,   &nodev,    &nodev,",

/*
 * 234 UDC11 digital control subsystem
 * 240 programmed interrupt
 * 244 floating point
 * 250 segmentation violation
 */

	"rp",	/* disk pack */
	0,	254,	BLOCK+CHAR+INTR,
	"\trpio; br5\n",
	".globl\t_rpintr\n",
	"rpio:\tjsr\tr0,call; _rpintr\n",
	"\t&nulldev,\t&nulldev,\t&rpstrategy, \t&rptab,",
	"\t&nulldev,  &nulldev,  &rpread,   &rpwrite,  &nodev,",

	"hp",	/* controller for multiple RPs */
	0,	254,	BLOCK+CHAR+INTR,
	"\thpio; br5\n",
	".globl\t_hpintr\n",
	"hpio:\tjsr\tr0,call; _hpintr\n",
	"\t&hpopen,\t&nulldev,\t&hpstrategy, \t&hptab,",
	"\t&hpopen,   &nulldev,  &hpread,   &hpwrite,  &nodev,",

/*
 * 260 TA11 cassette tape
 */
	"rx",	/* RX11 floppy disk */
	0,	264,	BLOCK+CHAR+INTR,
	"\trxio; br5\n",
	".globl\t_rxintr\n",
	"rxio:\tjsr\tr0,call; _rxintr\n",
	"\t&nulldev,\t&nulldev,\t&rxstrategy, \t&rxtab,",
	"\t&nulldev,  &nulldev,  &rxread,   &rxwrite,  &nodev,",


	"vdh",	/* Very Distant Host (ARPAnet) interface */
	0,	270,	NET+CHAR+INTR,
	"\tvdhin; br7\n\tvdhou; br7\n",
	".globl\t_vdhiint\nvdhin:\tjsr\tr0,call; _vdhiint\n",
	".globl\t_vdhoint\nvdhou:\tjsr\tr0,call; _vdhoint\n",
	"",
	"\t&ncpopen,  &ncpclose, &ncpread,  &ncpwrite, &nodev,",

	"acc",	/* Associated Computer Consultants (ARPAnet) interface */
	0,	270,	NET+CHAR+INTR,
	"\taccin; br5\n\taccou; br5\n",
	".globl\t_acc_iint\naccin:\tjsr\tr0,call; _acc_iint\n",
	".globl\t_acc_oint\naccou:\tjsr\tr0,call; _acc_oint\n",
	"",
	"\t&ncpopen,  &ncpclose, &ncpread,  &ncpwrite, &nodev,",

/*
/*
 * logical devices -- devices simulated by software with no real existance
 */

	"mem",	/* memory devices, also null device */
	-1,	300,	CHAR,
	"",
	"",
	"",
	"",
	"\t&nulldev,  &nulldev,  &mmread,   &mmwrite,  &nodev,",

/*
 * tty -- /dev/tty device
 */

	"ptyc",	/* psuedo-tty controller side */
	0,	300,	CHAR,
	"",
	"",
	"",
	"",
	"\t&ptcopen,  &ptcclose, &ptcread,  &ptcwrite, &ptysgtty,",

	"ptys",	/* psuedo-tty slave side */
	0,	300,	CHAR,
	"",
	"",
	"",
	"",
	"\t&ptsopen,  &ptsclose, &ptsread,  &ptswrite, &ptysgtty,",

/*
/*
 * floating devices -- (address-300) is required boundry alignment
 */

	"dc",	/* programmable asynchronous serial line interface */
	0,	308,	CHAR+INTR,
	"\tdcin; br5+%d.\n\tdcou; br5+%d.\n",
	".globl\t_dcrint\ndcin:\tjsr\tr0,call; _dcrint\n",
	".globl\t_dcxint\ndcou:\tjsr\tr0,call; _dcxint\n",
	"",
	"\t&dcopen,   &dcclose,  &dcread,   &dcwrite,  &dcsgtty,",

	"kl",	/* asynchronous serial line interface */
	0,	308,	INTR+KL,
	"\tklin; br4+%d.\n\tklou; br4+%d.\n",
	"",
	"",
	"",
	"",

	"dp",	/* synchronous serial line interface */
	0,	308,	CHAR+INTR,
	"\tdpin; br6+%d.\n\tdpou; br6+%d.\n",
	".globl\t_dprint\ndpin:\tjsr\tr0,call; _dprint\n",
	".globl\t_dpxint\ndpou:\tjsr\tr0,call; _dpxint\n",
	"",
	"\t&dpopen,   &dpclose,  &dpread,   &dpwrite,  &nodev,",

/*
 * DM11-A  DM11A-compatible modem interface for DH11
 */

	"dn",	/* automatic calling unit */
	0,	304,	CHAR+INTR,
	"\tdnou; br5+%d.\n",
	"",
	".globl\t_dnint\ndnou:\tjsr\tr0,call; _dnint\n",
	"",
	"\t&dnopen,   &dnclose,  &nodev,    &dnwrite,  &nodev,",

	"dhdm",	/* DM11BB-compatible modem interface for DH11 */
	0,	304,	INTR,
	"\tdmin; br4+%d.\n",
	"",
	".globl\t_dmint\ndmin:\tjsr\tr0,call; _dmint\n",
	"",
	"",

/*
 * DR11-A+  DMA interface
 * DR11-C+  general device interface
 * PA611+   card reader
 * PA611+   card punch
 * DT11+    UNIBUS switch
 * DX11+    ???
 */

	"dl",	/* single asynchronous serial line interface */
	0,	308,	INTR+KL,
	"\tklin; br4+%d.\n\tklou; br4+%d.\n",
	"",
	"",
	"",
	"",

/*
 * DJ11  16-line asynchronous serial line multiplexor
 */

	"dh",	/* 16-line programmable asynchronous serial line multiplexor */
	0,	308,	CHAR+INTR+EVEN,
	"\tdhin; br5+%d.\n\tdhou; br5+%d.\n",
	".globl\t_dhrint\ndhin:\tjsr\tr0,call; _dhrint\n",
	".globl\t_dhxint\ndhou:\tjsr\tr0,call; _dhxint\n",
	"",
	"\t&dhopen,   &dhclose,  &dhread,   &dhwrite,  &dhsgtty,",

/*
 * GT40  graphics device
 * LPS+  lab peripheral system
 * DQ11  NPR synchronous line interface
 * DU11  synchronous line interface
 * VT20  video display
 */
