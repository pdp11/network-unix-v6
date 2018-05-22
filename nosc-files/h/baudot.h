/* Baudot input mapping table -- the value of "FIGS" is OR'd as 6th bit
 * into the 5-bit character value to get a 6-bit index
 * into this table.
 */
#define FIGSCHR 0333
#define LETSCHR 0337
	/* This table uses "Communication" standard FIGS. */
char	bditab[] {
	0177,'e',012,'a',040,'s','i','u',
	015,'d','r','j','n','f','c','k',
	't','z','l','w','h','y','p','q',
	'o','b','g',FIGSCHR,'m','x','v',LETSCHR,
	0177,'3',012,'-',040,007,'8','7',
	015,'$','4',053,',','!',':','(',
	'5','"',')','2','#','6','0','1',
	'9','?','&',FIGSCHR,'.','/',';',LETSCHR,
};

/* Baudot output mapping table -- some simple transforms
 * are done to turn an ASCII char into a 6-bit index into
 * this table.  If the 040 bit is set in the resulting byte,
 * it means FIGS mode must be used to output it.
 * If the 200 bit is set, the byte can be output in either mode.
 * Note that entries for % and / are identical, as a crock.
 */
#define NOCH 0300
char	bdotab[] {
	0204,053,061,064,051,075,072,045,	/*  !"#$%&' */
	057,062,NOCH,NOCH,054,043,074,075,	/* ()*+,-./ */
	066,067,063,041,052,060,065,047,	/* 01234567 */
	046,070,056,076,NOCH,NOCH,NOCH,071,	/* 89:;<=>? */
	NOCH,03,031,016,011,01,015,032,		/* @ABCDEFG */
	024,006,013,017,022,034,014,030,	/* HIJKLMNO */
	026,027,012,005,020,007,036,023,	/* PQRSTUVW */
	035,025,021,NOCH,NOCH,NOCH,NOCH,NOCH,	/* XYZ[\]^_ */
};


