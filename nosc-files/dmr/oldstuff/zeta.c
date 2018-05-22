#
#include "../../h/param.h"
#include "../../h/conf.h"
#include "../../h/user.h"
#include "../../h/zeta_text.h"

#define Fetchbyte		cpass()
#define Hctefbyte		passc
#define Q_top			12
#define Words_Qslot		4
#define Plateau_size		8
#define True                    1
#define False        		0

#define Await_firstcomm		50000
#define Await_penconditioning	17750
#define Await_input		5000

#define Major_mvs		CommQ[0].mvs_major
#define Minor_mvs		CommQ[0].mvs_minor
#define Command			CommQ[0].command
#define Output_State		CommQ[0].outstate
#define Moves_Togo		CommQ[0].outstate

#define Zeta_readybit		0200
#define Run			0101

#define ZT_RITE			0172556->byte =
#define Zeta_Buf		0172556->byte
#define Zeta_Csr		0172554->byte
#define Clock_ctr		0172544->word
#define Clock_Buf		0172542->word
#define Clock_Csr		ZT.or =| 0172540->word

#define Zeta_Penconditioning	060

#define IS_Available		1
#define OS_Decodemove		-2
#define OS_Decodepenchange	-3
#define OS_Generalcommand	-4
#define OS_Closefile		-5



#define	SPEEDO		((sizeof ZT_throttle)/2 -1)
#define XSPEEDO		(sizeof ZT_extended_throttle)/2
#define Msec			/10 - 5

struct  {char chr[4];};
struct  {int  word;};
struct  {char byte;};

struct Q
{
	int  command;
	int  outstate;
	int  mvs_major;
	int  mvs_minor;
} CommQ [Q_top+2];

struct Z {
	int
	 	/*
		 * holds a copy of the Csr when a 'read' is performed
		 */
	 csr_copy
		/*
		 * notes the speed-index of the current delay for pen-steps
		 */
	,currspeed
		/*
		 * holds max-subscr that can index speed table
		 */
	,speedlimit
		/*
		 * diagnostic: counts the number of times the ZETA was
		 * NOT found ready
		 */
	,unready
		/*
		 * indicates the next CommQ bfr which is available for
		 * inserting data
		 */
	,nextbfr
	,choicept
	,accel
		/*
		 * diagnostic: count of the times clock interrupt
		 * found a move incomplete
		 */
	,pendg_pm
		/*
		 * or a non-penmove pending
		 */
	,pendg_npm
	,pendg_z

		/*
		 * 'or' of all clock-csr deposits
		 */
	,or
	;
}  ZT;


	/*
	 * retains 'state info on the 'write' (input) routine
	 */
int	ZT_input_state IS_Available;
	/*
	 * command to get	 * commands
	 */
char	ZT_comm[]	{014, 034, 054, 074, /* select pen #0->#3 */
			 020, 040};          /* pen-down->up    */
	/*
	 * command delays
	 */
int	ZT_static_delays[]	{17750, 17750, 17750, 17750, /* select delays */
			 04400, 06000};/* Emperically derived down/up delays */

int	ZT_throttle[]	
       {     /* who says the zeta can't be run slow? */
        3340 Msec, 2880 Msec, 2520 Msec, 2260 Msec, 2040 Msec,
	1860 Msec, 1720 Msec, 1600 Msec, 1480 Msec, 1380 Msec,
	1300 Msec, 1240 Msec, 1180 Msec, 1120 Msec, 1060 Msec,
	1020 Msec,  980 Msec,  940 Msec,  900 Msec,  860 Msec,
	 840 Msec,  820 Msec,  800 Msec,  780 Msec,  760 Msec,
	 740 Msec,  720 Msec,  700 Msec,  680 Msec,  660 Msec,
	 640 Msec,  620 Msec,  600 Msec,  580 Msec,  560 Msec
	};

int	ZT_extended_throttle[] /* Ho ho ho... now how much of THIS will run?*/
	{
	540 Msec, 520 Msec, 500 Msec, 480 Msec, 460 Msec,
	450 Msec, 440 Msec, 430 Msec, 420 Msec, 410 Msec,
	400 Msec, 390 Msec, 380 Msec, 370 Msec, 360 Msec,
	350 Msec, 340 Msec, 330 Msec, 320 Msec, 310 Msec,
	300 Msec, 290 Msec, 280 Msec, 270 Msec, 260 Msec,
	250 Msec
	};

/*
 * pen-moves:     octant  0    1    2    3    4    5    6    7
 */
char	ZT_maj_comm []	{001, 004, 004, 002, 002, 010, 010, 001};
char	ZT_min_comm []	{005, 005, 006, 006, 012, 012, 011, 011};
/*
 *		The octants:      +Y
 *		               \ 2 | 1 /
 *			       3 \ | / 0
 *			  -X ------------- +X
 *			       4 / | \ 7
 *			       / 5 | 6 \
 *				  -Y
 */

/*
 * zt_popQ: The purpose of this routine is to 'pop' the command queue
 * one slot
 */

zt_popQ()
{
	register int *here, *there;
	for (	here = Words_Qslot + (there = &CommQ);
		here < &CommQ[Q_top+2];
		*there++ = *here++);

	if ( --ZT.nextbfr == (Q_top >> 1) ) /* low water mark is 50% */
		wakeup(&CommQ);
}

/*
 * zt_recalc: This routine returns the number of contiguous pen_steps
 * w/in this octant ('foreseeablemoves')
 */

zt_recalc()
{
	register int foreseeablemoves, goal_throttle;
	register struct Q *C;
	int	delta_speed;

	foreseeablemoves = Moves_Togo;
	if (ZT.nextbfr <= 1)
		goto exile;
	for (C = &CommQ[1]; C < &CommQ[ZT.nextbfr]; C++)
	{
		if (C->command != Command)
			break;
		foreseeablemoves =+ C->mvs_major;
	}
 exile:

	/*
	 * Knowing the number ('foreseeablemoves') of pen-steps (queued
	 * already) which are contiguously within the current octant, one
	 * knows how many steps one can take to achieve maximum speed
	 * and then decelerate back to zero.
	 *
	 * One can anticipate ahead of time the point at which one will
	 * have to make a change in acceleration or read this moment,
	 * which is marked by (Moves_Togo == ZT.choicept), prompts
	 * a recalculation of the acceleration (+/0/-) over the next
	 * interval, and calculates the next 'ZT.choicept'.
	 */

	goal_throttle= (foreseeablemoves + ZT.currspeed - Plateau_size) >> 1;
	if (ZT.speedlimit < goal_throttle)
	{
		goal_throttle=ZT.speedlimit;
	}
	else
	{
		if (goal_throttle < 0)
		{
			goal_throttle = 0;
		}
	}

	/*
	 * 'goal_throttle', which was just calculated, is the ZETA
	 * speed which needs to be attained by the next 'choicept' and
	 * therefore is compared to the current speed to identify
	 * the acceleration necessary
	 */

	if ( (delta_speed = goal_throttle-ZT.currspeed) == 0)
	{
		ZT.accel = 0;
		ZT.choicept   =+ goal_throttle - foreseeablemoves;
	}
	else
	{
		if (delta_speed > 0)
		{
			ZT.accel = 1;
			ZT.choicept   =- delta_speed;
		}
		else
		{
			/*
			 * ZT.choicept == -1 means
			 * 'decelerate to zero-speed'
			 */
			ZT.accel = ZT.choicept = -1;
		}
	}
}

/*
 * ZT_RITE: This routine performs the loading of the ZETA Bfr and
 * the tracking of the pen-position
 */
/* temporarily ?? out of service
 *
 */
#ifdef	OUTOFSERV

ZT_RITE(bite)  char bite;
{
	register int chomp, ax, ay;

	if (!ZT.inch--)
	{
		ZT.inch= 400;
		ZT.inches+;
	}
	chomp = bite;
	ax = ZT.abs_xloc;
	ay = ZT.abs_yloc;
	if (chomp&01)
		ax++;
	if (chomp&02)
		ax--;
	if (chomp&04)
		ay++;
	if (chomp&010)
		ay--;
	ZT.abs_xloc = ax;
	ZT.abs_yloc = ay;
	Zeta_Buf = chomp;
}

zt_trouble()
{
	/*
	 * If the ZETA is not ready, let's try to do something about
	 * it... it should be noted, however, that things may have
	 * crumbled apart by this point... the ZETA will run the risk
	 * of having slithered its pens around if stopped at full
	 * speed... but what the hell, ya can only give it a try!
	 * Recovery will necessitate restarting the pen from speed zero.
	 */

	ZT.unread++;	/* inc diagnostic counter */
	printf("\nzeta:not ready\n");	/* alert the operator once */
	while ( !(Zeta_Csr & Zeta_readybit) )
	{
		printf("\007");		/* ding the bell */
		if (CommQ[ZT.nextbfr-1].outstate == OS_Closefile)
			goto closet;
		/*
		 * run for 10 seconds (at line freq speed)
		 */
		Clock_Buf = 10;
		Clock_Csr = 0105;
	}
	ZT.choicept = Moves_Togo-2;	/* restart the 'speed' from zero */
	ZT.currspeed = 0;
}
#endif	OUTSERV

/*
 * ztopen: If the ZETA is ready and not in use, initialize the world
 *  and give this user the lock on the ZETA
 */

ztopen(dev,arg)     int dev, arg;     
{
	register int  *zap;

	if (
/*
 * the zeta_ready test should not be performed until the interface
 * permits
 *		( !(Zeta_Csr & Zeta_readybit ))
 *	||
 */
		ZT_input_state != IS_Available
	) {
		u.u_error = EBUSY;
		return;
	}
	ZT.speedlimit = SPEEDO / 3;
	ZT.pendg_pm	= 
	ZT.pendg_npm	=
	ZT.pendg_z	=
	ZT.currspeed	=
	ZT_input_state	=
	ZT.nextbfr	=
	ZT.or		=
			0;

	for (zap= &CommQ; zap<&CommQ[Q_top+2]; *zap++=0);

	Clock_Buf = (-2);
	Clock_Csr = Run;
}

/*
 * ztclose: purge the input stream, wait for the command queue to empty
 * itself, and mark the ZETA 'available' once again
 */

ztclose(dev,arg)     int dev, arg;
{
	while (Fetchbyte != -1);   /* purge the input queue */
	spl5();
	CommQ[ZT.nextbfr++].outstate = OS_Closefile;
	spl0();
}

/*
 * ztread: Permit the user to 'read' the ZETA Csr and counters
 */

ztread(dev,arg)     int dev, arg;
{
	ZT.csr_copy = Zeta_Csr;
	/*
	 * this really should be done with routines other than
	 * from the network
	 */
	bcopyout (	 &ZT
			,u.u_base
			,min (u.u_count, sizeof ZT)
			);
}

/*
 * ztintr: The interrupt processor which drives the ZETA 
 */

ztintr(dev,argh)     int dev, argh;
{
	static int	slopac;
	static char	majcomm, mincomm;
	register	slop_reg, comm_reg, maj_reg;

stateofthenation:

	if (Output_State > 0)

	{
		/*
		 * VECTOR EMISSION
		 *
		 * If Output_State > 0. the interrupt program is in the
		 * midst of processing a pen-move command.  The current
		 * pen-move has <Output_State> more increments to be
		 * made before it is completed; hence, when
		 * Output_State == 0 the pen-move command will be
		 * just-completed.
		 */

		Clock_Buf = ZT_throttle [ZT.currspeed];
		/*
		 * start clock immediately to minimize timing
		 * uncertainty
		 */
		Clock_Csr = Run;
		/*
		 * The following algorithm for determining which
		 * octal-axis (max/min) to use is obscure, but works
		 */
		comm_reg = majcomm;
		if ( ( slop_reg = slopac + Minor_mvs) > (maj_reg = Major_mvs) )
		{
			slop_reg =- maj_reg;
			comm_reg = mincomm;
		}
		slopac = slop_reg;
		ZT_RITE (comm_reg);

		/*
		 * 'choicept's are points where a decision must be made
		 * as to whether to: accelerate/decelerate/level-off
		 * in speed
		 */

		if (--Moves_Togo == ZT.choicept)
			zt_recalc();
		if ( (ZT.currspeed =+ ZT.accel) < 0)
			ZT.currspeed = 0;
		/*
		 * falling through here takes one to interrupt exit
		 */
	}	/* end if Output_state > 0 */
	/*
	 * COMMAND PROCESSING 
	 */

	else
	{
		switch (Output_State)
		{
		case 0:
			/*
			 * Entered when the last command was a pen move,
			 * and it has just been completed.  It is now time
			 * to clean up the debris of that set of pen-step
			 * instructions and to proceed in interpreting
			 * the next command in the queue
			 */

			if (ZT.nextbfr)
			{
				/*
				 * if there are more commands in the queue,
				 * popq
				 *
				 *
				 * popQ shifts everything 'down' one buffer
				 * in the queue.
				 *
				 * a side effect is that the 'state' of the
				 * Command buffer is changed, so it's time
				 * to process that new state
				 */
				zt_popQ();
				goto stateofthenation;
			}
			/*
			 * if there are no commands, awaken user from any
			 * PEN_PAUSE command, and then sleep a while
			 */
			wakeup (&ZT);
			Clock_Buf = (-2);	/* Rest for .6 seconds */
			goto run_and_exit;	/* Rest for .6 seconds */

		case OS_Decodemove:
			/*
			 * The queue has been found to contain a pen-move
			 * command and the necessary initializations must
			 * be made before emitting the first ZETA instruction
			 */

			mincomm = ZT_min_comm [Command];
			majcomm = ZT_maj_comm [Command];
			ZT.choicept = (Moves_Togo = Major_mvs) - 1;
			slopac= Major_mvs >> 1;
			goto stateofthenation;

		case OS_Decodepenchange:
			/*
			 * There must be a conditioning command and a long
			 * pause before the actual pen-change command can
			 * be emitted. This segment emits the command and
			 * sets up the next 'general command' to effect the
			 * pen-change
			 */

			Clock_Buf = Await_penconditioning;
			ZT_RITE (Zeta_Penconditioning);
			Output_State = OS_Generalcommand;
			goto run_and_exit;

		case OS_Closefile:
	closet:

			/*
			 * OS_Closefile: When a 'close' is effected, the
			 * ztclose routine places an OS_Closefile command
			 * in the queue, and this segment encounters
			 * that command
			 */

			/*
			 * put our baby to sleep for the duration
			 */
			Clock_Csr = 0;
			Clock_Buf = 0;
			ZT_input_state = IS_Available;
			ZT_RITE ( ZT_comm[5] );	/* raise pen from paper */
			return;

		case OS_Generalcommand:
			/*
			 * OS_Generalcommand: This is entered to effect:
			 * pen-up/down or pen-selection (after conditioning)
			 */

			/*
			 * the next two lines require proper initialization
			 * of the two arrays, care must be exercised !
			 */
			Clock_Buf = ZT_static_delays [Command-PEN_SELECT];
			ZT_RITE (ZT_comm [Command-PEN_SELECT]);
			zt_popQ ();

     run_and_exit:
			Clock_Csr = Run;
		}	/* end switch */
	}		/* end else part of Output_state > 0 */
	/*
	 * now fall back to sleep until the next clock interrupt hits
	 */
}

char  ZT_xlate_to_octant[] { 0,3,7,4,1,2,6,5 };

char  ZT_Speed[]
{
	2,
	(SPEEDO)/6,
	(SPEEDO)/3,
	2*(SPEEDO)/3,
	(SPEEDO),
	(SPEEDO)+(XSPEEDO)/3,
	(SPEEDO)+2*(XSPEEDO)/3,
	(SPEEDO)+(XSPEEDO)
};

ztwrite(dev,argh)     int dev, argh;
{
	register  *buff;
  	static int	lastbyte, firstbyte;
	static int	vals[2], temp, state;
	static char	delta_format;

	for (;;)
	{
		if ( ( lastbyte = Fetchbyte ) == -1)
			return (0);
		if ( !ZT_input_state )
		{
			/*
			 * the following handles command-bytes
			 *
			 * (these are expected when ZT_input_state == 0
			 */
			spl5();
			while (ZT.nextbfr > Q_top)
				sleep (&CommQ, 2);	/* priority is arbitrary */

			switch ( CommQ [ZT.nextbfr].command = lastbyte )
			{
			case PEN_PAUSE:
				if (Output_State | ZT.nextbfr)
					sleep (&ZT,0);
				goto  release_interrupts;
			case PEN_REGISTER: /* unimplemented */
			case PEN_NULL:
				goto  release_interrupts;
			case PEN_MOVE_XY:
				delta_format = True;
			case PEN_MOVE+0:
			case PEN_MOVE+1:
			case PEN_MOVE+2:
			case PEN_MOVE+3:
			case PEN_MOVE+4:
			case PEN_MOVE+5:
			case PEN_MOVE+6:
			case PEN_MOVE+7:
				/*
				 * accumulate 4 bytes of operands
				 */
				ZT_input_state = -4;
				goto  release_interrupts;

			case PEN_SELECT+0:  /* pen #1 */
			case PEN_SELECT+1:  /* pen #2 */
			case PEN_SELECT+2:  /* pen #3 */
			case PEN_SELECT+3:  /* pen #4 */
				state = OS_Decodepenchange;
				goto  activate_new_command;
			case PEN_UP:
			case PEN_DOWN:
				state = OS_Generalcommand;
				goto  activate_new_command;
			case PEN_SPEED+0:
			case PEN_SPEED+1:
			case PEN_SPEED+2:
			case PEN_SPEED+3:
			case PEN_SPEED+4:
			case PEN_SPEED+5:
			case PEN_SPEED+6:
			case PEN_SPEED+7:
				ZT.speedlimit = ZT_Speed [lastbyte - PEN_SPEED];
				goto release_interrupts;

			default:
				/* abort the stupid mess --
				 * bad "command" given.
				 */
		abort:
				spl0();
				while ( Fetchbyte != -1 );
				u.u_error = EINVAL;
				return (-1);
			}	/* end switch */
		}		/* end if ZT.inputstate */
		else
		{
			/*
			 * PROCESS COMMAND OPERANDS
			 *
			 * these are expected when
			 * ZT_input_state < 0
			 */
			vals->chr[4+ZT_input_state++] = lastbyte;
			if ( !ZT_input_state )
			{
				/*
				 * ZT_input_state == 0 as the last
				 * nec. byte arrives.
				 */
				if ( !delta_format )
				{
					if ( vals[0] < vals[1]
					|| vals[1] < 0 )
						goto  abort;
				}
				else
				{
					state = delta_format = 0;
					if ( vals[0] < 0 )
					{
						state = 1;
						vals[0] = -vals[0];
					}
					if ( vals[1] < 0 )
					{
						state =+ 2;
						vals[1] = -vals[1];
					}
					if ( vals[0] < vals[1] )
					{
						state =+ 4; 
						temp = vals[0];
						vals[0] = vals[1];
						vals[1] = temp;
					}
					spl5();
					CommQ [ZT.nextbfr].command =
						ZT_xlate_to_octant [state];
				}
				spl5();
				CommQ [ZT.nextbfr].mvs_major = vals [0];
				CommQ [ZT.nextbfr].mvs_minor = vals [1];

				state = OS_Decodemove;
				goto  activate_new_command;
			}	/* end if !ZT_input_state */
			goto  release_interrupts;
		}	/* end else part of ZT.inputstate */
	activate_new_command:
		CommQ [ZT.nextbfr++].outstate = state;
	release_interrupts:
		spl0();
	}	/* end of for-ever */
}
