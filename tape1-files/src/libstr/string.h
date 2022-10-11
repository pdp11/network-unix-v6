/*
 * This header file is for routines which manage and use the auto storage
 * feature of the string library. By default, storage will be obtained
 * by calls on alloc (III) of at least 512 bytes. The sinit()
 * routine permits the user to alter this behavior. The format
 * of an auto string area is as follows:
 *
 *		  (high addresses)
 *
 * c_top -----> ___________________
 *		|_             |_
 *		_		  _  available
 * c_sp ------> |__________________|_
 *		|_             |_
 *		_		  _  current frame
 *		|__________________|_
 * c_sfp -----> |_p_r_e_v__s_f_p__________|_
 *		|_             |_
 *		_		  _  previous frame(s)
 *		|__________________|_
 *		|_p_r_e_v__a_r_e_a_'_s__s_p____|_
 *		|_p_r_e_v__a_r_e_a_'_s__s_f_p___|_
 *		|_p_r_e_v__a_r_e_a_'_s__t_o_p___|_
 * prev sfp --> |________0__________|_
 *
 * When sbegin() is called, the value of c_sfp is pushed on the stack,
 * and c_sfp is made to point at it. When send() is called, c_sfp is
 * restored to the value it points to, and c_sp is made to point just above
 * it. If at any time in the course of filling up the current frame, storage
 * runs out, a new area is allocated to continue the frame in. The old area's
 * c_sp, c_sfp and c_top are saved in it, c_sfp is pointed at the marker
 * (which is NULL), and c_sp is pointed just after the base. When send() is
 * called later, it will see that the "previous c_sfp" is NULL and will free
 * the area, restore the previous c_sp and c_top, and continue as before,
 * freeing the portion of the current frame in the previous area as well.
 * Note that stack frames can straddle areas (but individual strings do not).
 */
#define NULL 0

/*
 * Machine-dependent alignment routines.
 * Note that the argument must be char*.
 */
#define ROUND 01
#define unaligned(x) (((x) & ROUND) != 0)
#define aligndown(x) ((x) & ~ROUND)
#define alignup(x) (((x) + ROUND) & ~ROUND)

#define INIT_ROOM    2	/* Space for user strings in initial area */
/* The value of 2 turns off this feature */
#define INIT_ALLOC   512   /* Initial area allocation size */

/*
 * Structure of area base
 */
struct sbase {
    char *b_marker;	/* NULL */
    char *b_sfp;	/* From previous area, or NULL */
    char *b_sp; 	/* From previous area */
    char *b_top;	/* From previous area */
};

/*
 * Structure of stack frame
 */
struct sframe {
    char *prev_sfp;
};

/*
 * String area control structure.
 */
struct sctrl {
    char *c_sp; 	/* Pointer to next avail byte */
    char *c_sfp;	/* Ptr to base of current stack frame */
    char *c_top;	/* Ptr to first unavailable byte */
    int   c_bsize;	/* Allocation size */
};

/* -------------------------- G E T (macro) ------------------------- */
/*
 * get (size, addr)
 * sets addr to the current stack pointer in the current frame.
 * If there isn't room for a thing size long, it calls _Sall to make sure.
 */
#define get(s,a) if(_ScB.c_sp + s >= _ScB.c_top) _Sall(s); a = _ScB.c_sp
