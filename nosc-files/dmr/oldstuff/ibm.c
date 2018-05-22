#
/*
 * Character mapping tables for IBM 2741
 */
#define	nnn	0
#define	nnnn	0300
#define	PFX	01	/* prefix */
#define	IDL	02	/* idle */
#define CRC	074	/* circle-C */
#define	CRD	064	/* circle-D */
#define	TAB	0357	/* tab */
#define	RUB	0177	/* rubout */
#define	BK	0335	/* ibm backspace */
#define	LF	0356	/* ibm linefeed */
#define	NL	0355	/* ibm newline */
#define	UC	034	/* ibm shift-to-upper-case */
#define	LC	037	/* ibm shift-to-lower-case */

/*
 * ibm Courier-72 correspondence code - to - ASCII
 * illegal codes converted to nnn
 *
 * lower case table
 */
char lc[] {
/*	 	 0   1   2   3   4   5   6   7		*/
/* 00 */	040,'!','t','j','4','o','l','/',
/* 10 */	'5',047,'e','p',nnn,nnn,nnn,nnn,
/* 20 */	'2','.','n','=','z',nnn,nnn,nnn,
/* 30 */	'6','i','k','q', UC,010,nnn,nnn,
/* 40 */	'1','m','x','g','0','s','h','y',
/* 50 */	'7','r','d',';',nnn,012,012,011,
/* 60 */	'3','v','u','f','9','w','b','-',
/* 70 */	'8','a','c',',',CRC,IDL,PFX,RUB
};
/*
 * upper case table
 */
char uc[] {
/*		 0   1   2   3   4   5   6   7		*/
/* 00 */	040,']','T','J','$','O','L','?',
/* 10 */	'%','"','E','P',nnn,nnn,nnn,nnn,
/* 20 */	'@','.','N','+','Z',nnn,nnn,nnn,
/* 30 */	'[','I','K','Q',nnn,010,nnn, LC,
/* 40 */	'\\','M','X','G',')','S','H','Y',
/* 50 */	'&','R','D',':',nnn,012,015,011,
/* 60 */	'#','V','U','F','(','W','B','_',
/* 70 */	'*','A','C',',',CRC,IDL,PFX,RUB
};

/*
 * ASCII - to - Courier-72 correspondence code
 * 0300 means either upper or lower case
 * 0200 means must be in upper case
 * 0100 means must be in lower case
 *
 * non-printing ASCII control characters are converted
 * to nnnn.  Printing ASCII characters for which there
 * is no correspondence code equivalent are escaped
 * to a two-character sequence.  The first char is
 * the 2741 equivalent of '\\', the unix escape char.
 * The second char is found in the table mapibm[].
 * Characters to be mapped have no case bits.
 * The low order bits are used to access mapibm[].
 */
char xascii[] {
/*		  0    1    2    3    4    5    6    7  */
/* 000 */	nnnn,nnnn,nnnn,nnnn,nnnn,nnnn,nnnn,nnnn,
/* 010 */	  BK, TAB,  NL,nnnn,nnnn,nnnn,nnnn,nnnn,
/* 020 */	nnnn,nnnn,nnnn,nnnn,nnnn,nnnn,nnnn,nnnn,
/* 030 */	nnnn,nnnn,nnnn,nnnn,nnnn,nnnn,nnnn,nnnn,

	     /*  sp    !    "    #    $    %    &    '	*/
/* 040 */	0300,0101,0211,0260,0204,0210,0250,0111,

	     /*   (    )    *    +    ,    '    .    /	*/
/* 050 */	0264,0244,0270,0223,0373,0167,0321,0107,

	     /*   0    1    2    3    4    5    6    7	*/
/* 060 */	0144,0140,0120,0160,0104,0110,0130,0150,

	     /*   8    9    :    ;    <    =    >    ?	*/
/* 070 */	0170,0164,0253,0153,0003,0123,0004,0207,

	     /*   @    A    B    C    D    E    F    G	*/
/* 100 */	0220,0271,0266,0272,0252,0212,0263,0243,

	     /*   H    I    J    K    L    M    N    O	*/
/* 110 */	0246,0231,0203,0232,0206,0241,0222,0205,

	     /*   P    Q    R    S    T    U    V    W	*/
/* 120 */	0213,0233,0251,0245,0202,0262,0261,0265,

	     /*   X    Y    Z    [    \    ]    ^    _	*/
/* 130 */	0242,0247,0224,0230,0240,0201,0006,0267,

	     /*   `    a    b    c    d    e    f    g	*/
/* 140 */	0007,0171,0166,0172,0152,0112,0163,0143,

	     /*   h    i    j    k    l    m    n    o	*/
/* 150 */	0146,0131,0103,0132,0106,0141,0122,0105,

	     /*   p    q    r    s    t    u    v    w	*/
/* 160 */	0113,0133,0151,0145,0102,0162,0161,0165,

	     /*   x    y    z    {    |    }    ~   rub	*/
/* 170 */	0142,0147,0124,0000,0001,0002,0005,nnnn
};
char mapibm[]	{'(','!',')','L','G','T','U',047};
