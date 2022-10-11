#include "../h/param.h"
#include "../h/pte.h"
#include "../h/buf.h"
#include "../h/map.h"
#include "../h/mbavar.h"
#include "../h/vm.h"
#include "../h/ubavar.h"

#define C (caddr_t)

extern struct mba_driver hpdriver;
extern struct mba_driver hpdriver;
extern struct mba_driver htdriver;

struct mba_device mbdinit[] = {
	/* Device,  Unit, Mba, Drive, Dk */
	{ &hpdriver, 0,   '?',    0,    1 },
	{ &hpdriver, 1,   '?',    1,    1 },
	{ &htdriver, 0,   '?',  '?',    0 },
	0
};

struct mba_slave mbsinit [] = {
	/* Driver,  Ctlr, Unit, Slave */
	{ &htdriver,   0,   0,    0 },
	0
};

extern struct uba_driver dhdriver;
extern Xdhrint0(), Xdhxint0();
int	 (*dhint0[])() = { Xdhrint0, Xdhxint0, 0 } ;
extern struct uba_driver dzdriver;
extern Xdzrint0(), Xdzxint0();
int	 (*dzint0[])() = { Xdzrint0, Xdzxint0, 0 } ;
extern struct uba_driver dzdriver;
extern Xdzrint1(), Xdzxint1();
int	 (*dzint1[])() = { Xdzrint1, Xdzxint1, 0 } ;
extern struct uba_driver dzdriver;
extern Xdzrint2(), Xdzxint2();
int	 (*dzint2[])() = { Xdzrint2, Xdzxint2, 0 } ;
extern struct uba_driver accdriver;
extern Xacc_iint0(), Xacc_oint0();
int	 (*accint0[])() = { Xacc_iint0, Xacc_oint0, 0 } ;
extern struct uba_driver accdriver;
extern Xacc_iint1(), Xacc_oint1();
int	 (*accint1[])() = { Xacc_iint1, Xacc_oint1, 0 } ;

struct uba_ctlr ubminit[] = {
/*	 driver,	ctlr,	ubanum,	alive,	intr,	addr */
	0
};

struct uba_device ubdinit[] = {
	/* driver,  unit, ctlr,  ubanum, slave,   intr,    addr,    dk, flags*/
	{ &dhdriver,   0,    -1,    0,    -1,   dhint0, C 0160020,  0,  0xffff },
	{ &dzdriver,   0,    -1,    0,    -1,   dzint0, C 0160100,  0,  0xff },
	{ &dzdriver,   1,    -1,    0,    -1,   dzint1, C 0160110,  0,  0xff },
	{ &dzdriver,   2,    -1,    0,    -1,   dzint2, C 0160120,  0,  0xff },
	{ &accdriver,   0,    -1,    0,    -1,   accint0, C 0167600,  0,  0x0 },
	{ &accdriver,   1,    -1,    0,    -1,   accint1, C 0167700,  0,  0x0 },
	0
};
