#ifdef NCP                      /* jsq bbn 10/19/78 */
/*	op codes for communication to the ncp daemon     */

#define ncp_open	0	/* user wants to open a connection */
#define ncp_rcv		1	/* data came in off net for daemon */
#define ncp_close	2 	/* user wants to close a connection */
#define ncp_timo        3       /* timer has gone off */
#define ncp_rreset      4       /* reset a host */
#define ncp_rlog        5       /* log_to_ncp */

/*	op codes coming from the ncp daemon to kernel      */

#define ncp_send	0	/* send attached data to net  */
#define ncp_setup	1	/* create inode, fill in status, put in proper file */
#define ncp_mod		2	/* change state of a net inode */
#define ncp_rdy		3	/* do a wakeup on the inode */
#define ncp_clean	4	/* say ncp finished with inode */
#define ncp_reset	5	/* host or group of hosts reset themselves */
#define ncp_stimo       6       /* set timer interval */

#endif                          /* on NCP */
