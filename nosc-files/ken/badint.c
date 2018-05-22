extern int badvec;
/*
 *    rountine to handle unexpected interrupts
 *      stolen from Univ. of Ill.
 */

badint()
{
	printf("\n\r\007unexpected interrupt %o\n",badvec);
}
