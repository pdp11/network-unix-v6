#define NDEFINESIZE	20
#define NSTRINGS	256	/* number of characters that can be mapped */
#define NULL		0	/* null character */
#define LF		012	/* line feed character */
#define CR		015	/* carriage return */
#define RAW		040	/* raw tty mode */
#define ECHO		010	/* echo mode bit */
#define CRMOD		020	/* cr map bit */

/*





			THIS IS THE REGION THAT GETS WRITTEN
			AND READ IN DURING SAVE AND LOAD CHAR
			SET OPERATIONS.  TO ADD SOMETHING SET
			'SVESIZE' TO THE CORRECT NUMBER OF BYTES
			AND ADD IT TO THE END OF THE AREA.  NOTE
			'MAPTAB' MUST BE FIRST AND THE NEW ADD-
			ITION MUST BE INITIALIZED TO BE ALLOC-
			ATED CONTIGIOUSLY.



*/

#define SVESIZE		436
/* 	maptab	--  map table for all characters read in 	*/

char maptab[]
{
	000,001,002,003,004,005,006,007,010,
	011,012,013,014,0200,016,017,020,
	021,022,023,024,025,026,027,030,
	031,032,033,034,035,036,037,040,
	041,042,043,044,045,046,047,050,
	051,052,053,054,055,056,057,060,
	061,062,063,064,065,066,067,070,
	071,072,073,074,075,076,077,0100,
	0101,0102,0103,0104,0105,0106,0107,0110,
	0111,0112,0113,0114,0115,0116,0117,0120,
	0121,0122,0123,0124,0125,0126,0127,0130,
	0131,0132,0133,0134,0135,0136,0137,0140,
	0141,0142,0143,0144,0145,0146,0147,0150,
	0151,0152,0153,0154,0155,0156,0157,0160,
	0161,0162,0163,0164,0165,0166,0167,0170,
	0171,0172,0173,0174,0175,0176,0177
};

char definetab[ NDEFINESIZE ]	/* holds char *s from map tab into string tab */
{
	0
};
char stringtab[ NSTRINGS ]	/* holds the resultant character map strings */
{
	'\r','\n',0		/* put the fixed character map in */
};
char echomask[]			/* bit map if bit on char is echoed off not echoed */
{
	0200,0045,0000,0000,0377,0377,0377,0377,
	0377,0377,0377,0377,0377,0377,0377,0377
};
char linedel	0100;		/* make line delete '@' default */
char backsp	010;		/* make ^h default back space */
char endline	LF;		/* current endline character */
char flagchar	'^';		/* current flag character */
int  nxtdefentry  1;		/* global word to decide on next free entry in deftab */
int  curstrp	3;		/* index of next free area in stringtab */

int  echomode 0;		/* are we doing the echoing or is frn host */
int  charmode 1;		/* buffer full line or sent what is typed */

/*



			THIS THE END OF THE SAVE/LOAD ALLOCATION
			AREA.

*/
/* Struct used by 'getc' and 'map&echo' for buffering data from the terminal */
struct
{
	int cnt;		/* current number of chars in ' data ' */
	char *nextc;		/* pointer to next char to be gotten */
	char *def_expan_str;	/* points in stringtab if define being expaned */
	char data[80];		/* place to put the data */
} input
{
	0,
	"",
	""
};
