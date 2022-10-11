/*
 * A clist structure is the head
 * of a linked list queue of characters.
 * The characters are stored in 4-word
 * blocks containing a link and 6 characters.
 * The routines getc and putc (m45.s or m40.s)
 * manipulate these structures.
 */
struct clist
{
    int c_cc;           /* character count */
    int c_cf;           /* pointer to first block */
    int c_cl;           /* pointer to last block */
};

/*
 * Cursor position structure.
 * This is one of the two structures currently
 * accessible via modtty.
 */
struct cursor
{
    char t_col;         /* Current column, for delay, tab-simul, etc. */
    char t_line;        /* Current line (after last read) */
};

/*
 * Terminal characteristics.
 * This is the other structure currently accessible via modtty.
 */
struct modes
{
    char t_width;       /* Screen width -- 0 means infinite */
    char t_len;         /* Screen length -- 0 means infinite */
    char t_pagelen;     /* Stop output when cursor.t_line >= pagelen */
    char t_term;        /* Terminal type -- see TY defines */

/* Physical properties */

    char t_speeds;      /* One speed in each nybble */
    char t_pflags;      /* Physical properties -- see T defines */

/* Input properties */

    char t_erase;
    char t_kill;
    char t_intr;
    char t_quit;
    char t_esc;
    char t_eot;
    char t_replay;
    int  t_breaks;  /* Break character classes */
    int  t_iflags;  /* Input flags */

    int  t_oflags;  /* Output properties */
};

/*
 * A tty structure is needed for
 * each UNIX character device that
 * is used for normal terminal IO.
 * The routines in tty.c handle the
 * common code associated with
 * these structures.
 * The definition and device dependent
 * code is in each driver. (kl.c dc.c dh.c)
 */

struct tty
{
    struct clist t_inq;     /* Input queue */
    struct clist t_outq;    /* Output queue */
    int  *t_addr;           /* Device address */
    int  t_pgrp;            /* Process group */
    char *t_itp;            /* Await pointer */
    int  t_state;           /* Internal state bits */
    char t_delct;           /* Count of delimiters in t_inq */
    char t_char;            /* Character temporary */
    struct cursor t_pos;    /* Cursor position */
    struct modes  t_modes;  /* Terminal modes */
};

/* Internal state bits */

#define TS_TIMEOUT      00001   /* Delay timeout in progress */
#define TS_WOPEN        00002   /* Waiting for open to complete */
#define TS_ISOPEN       00004   /* Device is open */
#define TS_SSTART       00010   /* Has special start routine at t_addr */
#define TS_CARR_ON      00020   /* Software copy of carrier-present */
#define TS_BUSY         00040   /* Output in progress */
#define TS_ASLEEP       00100   /* Wakeup when output done */
#define TSO_XOFFHNG     00200   /* For output ctrl: driver received XOFF */
#define TSI_XOFFHNG     00400   /* For input ctrl: driver sent XOFF */

/* Terminal types */

#define TY_UNKNOWN 0
#define TY_PRINT   1     /* Printing terminal */
#define TY_CRT     2     /* Glass TTY */
#define TY_VT52    3
#define TY_VT100   4
#define TY_ANN_ARBOR 5

/* Speeds */

#define T_INPUT_SPEED   0017
#define T_OUTPUT_SPEED  0360

#define L_SPEED   4
#define B_9600   13

/* Physical properties */

#define T_PARITY  001
#  define T_EVEN  000
#  define T_ODD   001

#define T_PENABLE 002
#define T_SB      004
#define T_HUPCL   010

/* Break character class definitions */

#define TB_UPPER        0000001
#define TB_LOWER        0000002
#define TB_DIGIT        0000004
#define TB_FORMAT       0000010
#define TB_LF           0000020
#define TB_SPACE        0000040
#define TB_ESC          0000100
#define TB_DEL          0000200
#define TB_CTRL         0000400
#define TB_TERM         0001000
#define TB_BAL          0002000
#define TB_OTHER        0004000
#define TB_UNDER        0010000
#define TB_CR           0020000
#define TB_NON_ASCII    0100000

/* Input flags */

#define TI_CLR_MSB      000001
#define TI_CRMOD        000002
#define TI_XONXOFF      000004
#define TI_BRK_ECHO     000010
#define TI_NONBRK_ECHO  000020
#define TI_TAB_ECHO     000040
#define TI_CR_ECHO      000100
#define TI_DEFER_ECHO   000200
#define TI_ONECASE      000400
#define TI_CRT          001000
#define TI_LOCAL_ECHO   002000

/* Output properties */

#define TO_PAD          000001
#define TO_AUTONL       000002
#define TO_XONXOFF      000004
#define TO_XTABS        000010

#define TO_TAB_DELAY    000060
#  define TO_TAB_37     000020
#  define TO_TAB_SPARE  000040
#  define TO_TAB_EXTRA  000060

#define TO_CR_DELAY     000300
#  define TO_CR_TN300   000100
#  define TO_CR_TI700   000200
#  define TO_CR_SPARE   000300

#define TO_VT_DELAY     000400
#define TO_CRMOD        001000
#define TO_ONECASE      002000

#define TO_NL_DELAY     014000
#  define TO_NL_ANN_ARBOR 0000
#  define TO_NL_37      004000
#  define TO_NL_VT05    010000
#  define TO_NL_ECD     014000

#define T_DELIM 0376    /* Put after each break char */
#define T_USR_DELIM -2  /* If user input char == T_DELIM, mapped to this */

/* default special characters */

#define CERASE  '\b'            /* Backspace */
#define CEOT    '\004'          /* Ctrl-D */
#define CKILL   '@'
#define CQUIT   '\034'          /* FS, cntl shift L */
#define CINTR   '\177'          /* DEL */
#define CXON    '\021'          /* cntl Q */
#define CXOFF   '\023'          /* cntl S */

/* ASCII table: parity, character type */

char partab[];

#define P_PARITY      0200
#define P_CTYPE       0077
#  define P_PRINTING     0
#  define P_CTRL         1
#  define P_BS           2
#  define P_LF           3
#  define P_HT           4
#  define P_VTFF         5
#  define P_CR           6

/* defines for use with modtty system call */

#define MOD_GET     000000
#define MOD_SET     000001

#define MOD_MODES   000000
#define MOD_CURSOR  000002

/* Priorities */

#define TTIPRI  10
#define TTOPRI  20

/* limits */

#define TTHIWAT 150
#define TTLOWAT 30
#define TTYHOG  256
#define TTYWARN 64

/*
 * If TI_XONXOFF set, XOFF sent when t_inq char count reaches TTYHOG - TTYWARN;
 * XON sent when output level gets down to TTYWARN.
 */

/* Hardware CSR bits */

#define DONE    0200
#define IENABLE 0100

/* -------------------------- E X P E R ---------------------- */

#define cinit xcinit
#define flushtty flushxty
#define gtty gxty
#define sgtty sgxty
#define stty sxty
#define ttread xtread
#define ttrstrt xtrstrt
#define ttstart xtstart
#define ttwrite xtwrite
#define ttycap xtycap
#define ttyinput xtyinput
#define ttyopen xtyopen
#define ttyoutput xtyoutput
#define ttystty xtystty
#define wflushtty wflushxty
#define maptab xmaptab
