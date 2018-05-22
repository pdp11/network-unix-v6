/*
 * for use by any systems program that must know the location
 * of the REAL password file vice the "password file"
 */
#define PASSWD	"/etc/passwd"	/* The password file used locally */
#define PASSWORD "/etc/passwd"
/*
 *      used by login and by ksys to know about any active ksys
 *      which will shut system down
 */
#define KSYST "/etc/ksystime"   /* data about pending ksys */
#define SHUTSW "/shutsw"        /* within 5 minutes of shutdown */
/*
 *      struct of the data file KSYST
 *      used by ksys to record information for posterity
 *      or until another ksys happens along
 */
struct ksys{
	long dtime;   /* time of shut down (the trumpet sounds then) */
	int kpid;     /* process id of the ksys who will do the deed */
	};
