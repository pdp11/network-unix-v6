#define HOSTSPEED B4800		/* speed of special dh lines to host */
char hostname[] "UNIVAC 1110";	/* The type of host computer */
#define SETUP write(fi, "UC", 2);	/* Whatever needs to be done */
					/* to set up the host for    */
					/* communication	     */
