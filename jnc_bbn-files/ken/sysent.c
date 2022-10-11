#
/*
 */

/*
 * This table is the switch used to transfer
 * to the appropriate routine for processing a system call.
 * Each row contains the number of arguments expected
 * and a pointer to the routine.
 */
#include "../h/param.h"

int	sysent[]
{
	0, &nullsys,			/*  0 = indir */
	0, &rexit,			/*  1 = exit */
	0, &fork,			/*  2 = fork */
	2, &read,			/*  3 = read */
	2, &write,			/*  4 = write */
	2, &open,			/*  5 = open */
	0, &close,			/*  6 = close */
	0, &wait,			/*  7 = wait */
	2, &creat,			/*  8 = creat */
	2, &link,			/*  9 = link */
	1, &unlink,			/* 10 = unlink */
	2, &exec,			/* 11 = exec */
	1, &chdir,			/* 12 = chdir */
	0, &gtime,			/* 13 = time */
	3, &mknod,			/* 14 = mknod */
	2, &chmod,			/* 15 = chmod */
	2, &chown,			/* 16 = chown */
	1, &sbreak,			/* 17 = break */
	2, &stat,			/* 18 = stat */
	2, &seek,			/* 19 = seek */
	0, &getpid,			/* 20 = getpid */
	3, &smount,			/* 21 = mount */
	1, &sumount,			/* 22 = umount */
	0, &setuid,			/* 23 = setuid */
	0, &getuid,			/* 24 = getuid */
	0, &stime,			/* 25 = stime */
	3, &ptrace,			/* 26 = ptrace */
	0, &alarm,			/* 27 = alarm */
	1, &fstat,			/* 28 = fstat */
	0, &pause,			/* 29 = pause */
#ifdef SMDATE
	1, &smdate,			/* 30 = smdate */
#endif
#ifndef SMDATE
	0, &nosys,                      /* 30 = x */
#endif
	1, &stty,			/* 31 = stty */
	1, &gtty,			/* 32 = gtty */
	2, &saccess,			/* 33 = access */
	0, &nice,			/* 34 = nice */
	0, &sslep,			/* 35 = sleep */
	0, &sync,			/* 36 = sync */
	1, &kill,			/* 37 = kill */
	0, &getswit,			/* 38 = switch */
	0, &nosys,                      /* 39 = x */
	0, &nosys,                      /* 40 != nport ( BBN:mek 5/3/79) */
	0, &dup,			/* 41 = dup */
	0, &pipe,			/* 42 = pipe */
	1, &times,			/* 43 = times */
#ifdef SMALL
	0, &nosys,                      /* 44 = x */
#endif
#ifndef SMALL
	4, &profil,			/* 44 = prof */
#endif
	0, &nosys,			/* 45 = tiu */
	0, &setgid,			/* 46 = setgid */
	0, &getgid,			/* 47 = getgid */
	2, &ssig,			/* 48 = sig */
	0, &reboot,			/* 49 = reboot */
#ifdef NCP                              /* jsq bbn 10/19/78 */
	0, &sendins,			/* 50 = send ins */
	0, &net_reset,                  /* 51 = net_reset */
#endif NCP
#ifndef NCP                             /* jsq bbn 10/19/78 */
	0, &nosys,                      /* 50 = x */
	0, &nosys,                      /* 51 = x */
#endif NCP
	2, &nullsys,                    /* 52 = acctput */
	0, &itime,                      /* 53 = itime (BBN:JFH 5/11/78) */
#ifdef	LCBA
	1, &map_page,                   /* 54 = map_page */
#endif LCBA
#ifndef	LCBA
	0, &nosys,                      /* 54 = x */
#endif LCBA
	3, &await,			/* 55 = await (BBN:JFH 4/7/78  */
        0, &nosys,                      /* 56 != old await anymore (4/12/79) */
	0, &ztime,			/* 57 = ztime (rand:greep) */
	1, &waitr,			/* 58 = waitr (rand:greep) */
	2, &port,			/* 59 = port (rand:jsz) */
	0, &eofpipe,			/* 60 = eofp (rand:bobg) */
	0, &gprocs,			/* 61 = getproc (rand:bobg) */
	0, &sfork,			/* 62 = sfork (rand:bobg) */
        0, &nosys,                      /* 63 != empty any more */
	0, &nosys,                      /* 64 = x */
	0, &nosys,                      /* 65 = x */
	0, &nosys,                      /* 66 = x */
	0, &qtime,			/* 67 = qtime (BBN:mek  1/11/78) */
#ifdef RMI
	0, &rawstat,                    /* 68 = rawstat (BBN: JSQ 10-4-78)
#endif RMI
#ifndef RMI
	0, &nosys,                      /* 68 = x */
#endif RMI
#ifdef NETWORK
	0, &sgnet,                      /* 69 = sgnet (BBN: jsq 2-9-79) */
#endif NETWORK
#ifndef NETWORK
	0, &nosys,                      /* 69 = x */
#endif NETWORK
	1, &table,                      /* 70 = table (BBN: jsq 2-9-79) */
	0, &nosys,                      /* 71 = x  reserved for kahn */
	0, &cutloose,                   /* 72 = cutloose - Kahn March 15 79 */
	0, &nosys,                      /* 73 = modtty (BBN:dan) */
	0, &nosys,                      /* 74 = x */
	0, &nosys,                      /* 75 = x */
#ifdef SMALL
	0, &nosys                       /* 76: last entry should not
					   be used. Kept for out of bounds
					   system calls - adjust MAXSYSC in
					   param.h accordingly */
#endif
#ifndef SMALL
	0, &nosys,                      /* 76 = x */
	0, &nosys,                      /* 77 = x */
	0, &nosys,                      /* 78 = x */
	0, &nosys,                      /* 79 = x */
	0, &nosys,                      /* 80 = x */
	0, &nosys,                      /* 81 = x */
	0, &nosys,                      /* 82 = x */
	0, &nosys,                      /* 83 = x */
	0, &nosys,                      /* 84 = x */
	0, &nosys,                      /* 85 = x */
	0, &nosys,                      /* 86 = x */
	0, &nosys,                      /* 87 = x */
	0, &nosys,                      /* 88 = x */
	0, &nosys,                      /* 89 = x */
	0, &nosys,                      /* 90 = x */
	0, &nosys,                      /* 91 = x */
	0, &nosys,                      /* 92 = x */
	0, &nosys,                      /* 93 = x */
	0, &nosys,                      /* 94 = x */
	0, &nosys,                      /* 95 = x */
	0, &nosys,                      /* 96 = x */
	0, &nosys,                      /* 97 = x */
	0, &nosys,                      /* 98 = x */
	0, &nosys,                      /* 99 = x */
	0, &nosys,                      /* 100 = x */
	0, &nosys,                      /* 101 = x */
	0, &nosys,                      /* 102 = x */
	0, &nosys,                      /* 103 = x */
	0, &nosys,                      /* 104 = x */
	0, &nosys,                      /* 105 = x */
	0, &nosys,                      /* 106 = x */
	0, &nosys,                      /* 107 = x */
	0, &nosys,                      /* 108 = x */
	0, &nosys,                      /* 109 = x */
	0, &nosys,                      /* 110 = x */
	0, &nosys,                      /* 111 = x */
	0, &nosys,                      /* 112 = x */
	0, &nosys,                      /* 113 = x */
	0, &nosys,                      /* 114 = x */
	0, &nosys,                      /* 115 = x */
	0, &nosys,                      /* 116 = x */
	0, &nosys,                      /* 117 = x */
	0, &nosys,                      /* 118 = x */
	0, &nosys,                      /* 119 = x */
	0, &nosys,                      /* 120 = x */
	0, &nosys,                      /* 121 = x */
	0, &nosys,                      /* 122 = x */
	0, &nosys,                      /* 123 = x */
	0, &nosys,                      /* 124 = x */
	0, &nosys,                      /* 125 = x */
	0, &nosys,                      /* 126 = x */
	0, &nosys                       /* 127: last entry should not
					   be used. Kept for out of bounds
					   system calls
					 */
#endif
};
