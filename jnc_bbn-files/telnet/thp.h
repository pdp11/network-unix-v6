/*
 * Defines for Terminal-to-Host Protocol
 */

#define THP true    /* For ifdef & ifndef */

/* Maximum number of network bytes a single user char can become. */

/* 7 because of padding via REPEAT records */
#define NETGROW 7

/* Maximum number of user chars a single network char can cause */

/* 255 because of REPEAT records */
#define USERGROW 255

/* THP Controls */

#define IAC            170
#define IP             254
#define AO             253
#define AYT            252
#define EC             251
#define EL             250
#define DO             249
#define WILL           248
#define DONT           247
#define WONT           246
#define XCONTROL       245
#define DATA           244
#define SUSPEND        243
#define SUSPENDED      233
#define CONTINUE       242
#define SET_MODE       241
#define REQUEST_MODE   240
#define DM             239
#define NOP            238
#define BREAK          237
#define STATUS_REQUEST 236
#define STATUS_REPLY   235
#define REPEAT         234

/* For SET_MODE and REQUEST_MODE */

#define RECORD         0
#define STREAM         1

/* THP Negotiated Options */

#define OPT_ECHO             0
#define OPT_BINARY           1
#define OPT_RCTE             2
#define OPT_INCLUDE_GA       3
#define OPT_XASCII           4
#define OPT_RECONNECT        5
#define OPT_RECORD_SIZE      6
#define OPT_LINEWIDTH        7
#define OPT_PAGESIZE         8
#define OPT_SET_VT           9
#define OPT_SET_HT          10
#define OPT_CR_DISP         11
#define OPT_LF_DISP         12
#define OPT_FF_DISP         13
#define OPT_HT_DISP         14
#define OPT_VT_DISP         15
#define OPT_EXOPT_LIST      16

/*
 * The following definitions are used in option negotiation.
 * The THP Specification does not assign values; they are arbitrary
 */

#define RECEIVER    1
#define SENDER      0
#define START       0
#define EXPECT      1

/* Structure of the reconnection record (after the RECONNECT byte) */

    struct Reconn
    {
        long RHost;
        int RPort;
        char RType; /* START or EXPECT */
    };

/* Structure of the formatting option records */

    struct Format
    {
        char SndRcv;    /* SENDER or RECEIVER */
        char Value;     /* Line width, page size, delay, etc. */
    };

/* XCONTROL records are not documented yet. Someday... */
