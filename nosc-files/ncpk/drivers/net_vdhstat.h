/* VDH statistics */
# define vs_size 15		/* Number of VDH statistics */

struct vdhstatistics
{ /* This reduces number of globals */
  int looped;	/* Number of looped packets (wrong htoi bit) */
  int wpaks;	/* Number of packets recv'd waiting for line to come up */
  int chwrong;	/* Number of packets with wrong channel */
  int idups;	/* Number of duplicated packets on input */
  int ifull;	/* Number of times packet received before previous packet had
		   been processed */
  int igood;	/* Number of good packets received */
  int nobuf;	/* Number of times not enough buffering was available input */
  int inulls;	/* Number of null packets sent */
  int oidle;	/* Number of time output side went idle */
  int opaks;	/* Number of packets output (excluding repeats) */
  int nolink;	/* Regular message received with link field we don't know
		   about */
  int badld;	/* Number of bad leaders seen */
  int olinewait; /* Number of time waiting for IMP line (output) */
  int harderr;	/* Number of hard errors from VDH */
};
