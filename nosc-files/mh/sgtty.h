/*
 *	struct for stty/gtty system call
 *
 *
 *		sg_ispd = input speed
 *		sg_ospd = output speed
 *
 *              sg_erase = character erase
 *              sg_kill = line kill
 *
.*              sg_flag = flags
 */

struct	sgtty	{
	char	sg_ispd, sg_ospd;
	char    sg_erase, sg_kill;
	int	sg_flag;
};

#define	NODELAY	01
#define	XTABS	02
#define	LCASE	04
#define	ECHO	010
#define	CRMOD	020
#define	RAW	040
#define	ALTESC	0400
#define	NTDELAY	010000
#define	PPL	020000
#define	TECO	040000
#define	NOSLASH	0100000

#define	CQUIT	002		/* control - b */
#define	CINTR	003		/* control - c */
#define	CEOT	004		/* control - d */
#define	CKILL	025		/* control - u */
#define	CERASE	0177		/* rubout */
#define	CRETYPE	022		/* control - r */
#define	CALTNEW	033		/* new style alt mode */
