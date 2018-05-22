#define ACCVLEN		18
#define	ACCPSWD		06237
#define	AC_FORK		1
#define	AC_EXEC		2
#define	AC_ZOMB		3
#define	AC_DIAB		4
#define	AC_LPR		5
#define	AC_LOG		6
#define	AC_FILE		7
#define	AC_PAD		8
#define	AC_NET		9	/* network connection */
#define	AC_AFTIME	10

#define	L_REBOOT	'~'
#define	L_OLDTIME	'|'
#define	L_NEWTIME	'}'

#define AC_DRATE	0.04	/* 4 cents per block month */

/*
			 Map
Maj Min  disk 	Maj Min	 to
--- ---	 ----	--- ---	 ---
 0   0    RK	-2  0	Root
 0   1    RK	-2  1	Adm
 1   0    RP	-2  2	Mnt
 1   1    RP	-2  3	Lpd
 1   2    RP	-2  4	Cerl
 1   5    RP	-2  2	Mnt
 1   6    RP	-2  3	Lpd
 5   4    AED	-2  5	Sys
 5   5    AED	-2  0	Root
 5   6    AED	-2  4	Cerl
 5   7    AED	-2  5	Temp

*/



/*

	Structure of the fork accounting file

*/

struct	acc_fork
{
	char	af_type;
	char	af_1spare;
	char	af_uid;
	char	af_rgid;
	char	af_gid;
	char	af_ruid;
	int	af_par_proc_id;
	int	af_child_proc_id;
	int	af_date[ 2 ];
	int	af_2spare[ 2 ];
};

/*

	Structure of the execv accounting file

*/

struct	acc_exec
{
	char	ae_type;
	char	ae_pid;
	char	ae_uid;
	char	ae_gid;
	char	ae_name[ 14 ];
};

/*

	Structure of the deceased-fork accounting file

*/

struct	acc_zomb
{
	char	az_type;
	char	az_uid;
	int	az_date[ 2 ];
	char	az_hsyscalls;
	char	az_hstime;
	int	az_stime;
	int	az_utime[ 2 ];
	int	az_proc_id;
	int	az_syscalls;
};

/*

	Structure of the wtmp (login accounting) file

*/

struct	acc_login
{
	char	al_type;
	char	al_pid;
	char	al_tty;
	char	al_chg_type;
	char	al_uid;
	char	al_gid;
	char	al_name[ 8 ];
	int	al_date[ 2 ];
};

/*

	Structure of the lpd accounting file

*/

struct	acc_lpd
{
	char	ap_type;	/* 5 for lpr, 4 for diablo */
	char	ap_hcnt;	/* hi part of character count */
	int	ap_cnt;		/* low part of character count */
	int	ap_lcnt;	/* line count */
	int	ap_pgcnt;	/* page count */
	char	ap_uid;
	char	ap_gid;
	int	ap_bdate[ 2 ];	/* begin time */
	int	ap_edate[ 2 ];	/* ending time */
};

/*

	Structure of the disk-space accounting file

*/

struct ad_ent
{
	int	ad_blocks;
	char	ad_minor;
	char	ad_major;
	int	ad_uid;
	int	ad_files;
};

struct	acc_file
{
	char	ad_type;
	char	ad_1spare;
	struct ad_ent ad_user_entry [2];
};

#define	AC_SBIF	0	/* Begin input file */
#define	AC_SBOF	1	/* Begin output file */
#define	AC_SEIF	2	/* End input file */
#define	AC_SEOF	3	/* End output file */

struct acc_aftim	/* accounting file time */
{	char	at_type;
	char	at_subtype;
	char	at_fill [12];
	int	at_date [2];
};
