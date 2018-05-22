/*
Module Name:
	mpw -- make password

Installation:
	if $1e = finale goto finale
	cc mpw.c
	exit
: finale
	cc -O -s mpw.c
	if ! -r a.out exit
	su cp a.out /usr/bin/mpw
	rm -f a.out
	cd /usr/bin
	su chmod 755 mpw
	su chown bin mpw
	su chgrp bin mpw

Synopsis:
	mpw

Function:
	Generate a set of random passwords for user selection.

Diagnostics:

See Also:
	passwd(I)

Bugs:

Compile time parameters and effects:

Module History:
	Originally a Multics PL/I program, (C) 1972 by the Massachusettes
	    Institute of Technology and by Honeywell and reproduced by
	    permission of Honeywell, Inc.
	Transliterated into Unix C 27Feb79 by Greg Noel, with improvements
	    and simplifications.
*/

/*	Frequency of English digraphs (from D Edwards 1/27/66)		*/

int frequency[27][26] {

	/* Starting letter frequencies */

	1299,	425,	725,	271,	375,	470,	93,	223,	1009,
	24,	20,	355,	379,	319,	823,	618,	21,	317,
	962,	1991,	271,	104,	516,	6,	16,	14,

	/* transitions between letters */

    4, 20, 28, 52, 2, 11, 28, 4, 32, 4, 6, 62, 23, 167, 2, 14, 0, 83, 76, 127, 7, 25, 8, 1, 9, 1, /* aa - az */
     13, 0, 0, 0, 55, 0, 0, 0, 8, 2, 0, 22, 0, 0, 11, 0, 0, 15, 4, 2, 13, 0, 0, 0, 15, 0, /* ba - bz */
     32, 0, 7, 1, 69, 0, 0, 33, 17, 0, 10, 9, 1, 0, 50, 3, 0, 10, 0, 28, 11, 0, 0, 0, 3, 0, /* ca - cz */
     40, 16, 9, 5, 65, 18, 3, 9, 56, 0, 1, 4, 15, 6, 16, 4, 0, 21, 18, 53, 19, 5, 15, 0, 3, 0, /* da - dz */
     84, 20, 55, 125, 51, 40, 19, 16, 50, 1, 4, 55, 54, 146, 35, 37, 6, 191, 149, 65, 9, 26, 21, 12, 5, 0, /* ea - ez */
     19, 3, 5, 1, 19, 21, 1, 3, 30, 2, 0, 11, 1, 0, 51, 0, 0, 26, 8, 47, 6, 3, 3, 0, 2, 0, /* fa - fz */
     20, 4, 3, 2, 35, 1, 3, 15, 18, 0, 0, 5, 1, 4, 21, 1, 1, 20, 9, 21, 9, 0, 5, 0, 1, 0, /* ga - gz */
     101, 1, 3, 0, 270, 5, 1, 6, 57, 0, 0, 0, 3, 2, 44, 1, 0, 3, 10, 18, 6, 0, 5, 0, 3, 0, /* ha - hz */
     40, 7, 51, 23, 25, 9, 11, 3, 0, 0, 2, 38, 25, 202, 56, 12, 1, 46, 79, 117, 1, 22, 0, 4, 0, 3, /* ia - iz */
     3, 0, 0, 0, 5, 0, 0, 0, 1, 0, 0, 0, 0, 0, 4, 0, 0, 0, 0, 0, 3, 0, 0, 0, 0, 0, /* ja - jz */
     1, 0, 0, 0, 11, 0, 0, 0, 13, 0, 0, 0, 0, 2, 0, 0, 0, 0, 6, 2, 1, 0, 2, 0, 1, 0, /* ka - kz */
     44, 2, 5, 12, 62, 7, 5, 2, 42, 1, 1, 53, 2, 2, 25, 1, 1, 2, 16, 23, 9, 0, 1, 0, 33, 0, /* la - lz */
     52, 14, 1, 0, 64, 0, 0, 3, 37, 0, 0, 0, 7, 1, 17, 18, 1, 2, 12, 3, 8, 0, 1, 0, 2, 0, /* ma - mz */
     42, 10, 47, 122, 63, 19, 106, 12, 30, 1, 6, 6, 9, 7, 54, 7, 1, 7, 44, 124, 6, 1, 15, 0, 12, 0, /* na - nz */
     7, 12, 14, 17, 5, 95, 3, 5, 14, 0, 0, 19, 41, 134, 13, 23, 0, 91, 23, 42, 55, 16, 28, 0, 4, 1, /* oa - oz */
     19, 1, 0, 0, 37, 0, 0, 4, 8, 0, 0, 15, 1, 0, 27, 9, 0, 33, 14, 7, 6, 0, 0, 0, 0, 0, /* pa - pz */
     0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 17, 0, 0, 0, 0, 0, /* qa - qz */
     83, 8, 16, 23, 169, 4, 8, 8, 77, 1, 10, 5, 26, 16, 60, 4, 0, 24, 37, 55, 6, 11, 4, 0, 28, 0, /* ra - rz */
     65, 9, 17, 9, 73, 13, 1, 47, 75, 3, 0, 7, 11, 12, 56, 17, 6, 9, 48, 116, 35, 1, 28, 0, 4, 0, /* sa - sz */
     57, 22, 3, 1, 76, 5, 2, 330, 126, 1, 0, 14, 10, 6, 79, 7, 0, 49, 50, 56, 21, 2, 27, 0, 24, 0, /* ta - tz */
     11, 5, 9, 6, 9, 1, 6, 0, 9, 0, 1, 19, 5, 31, 1, 15, 0, 47, 39, 31, 0, 3, 0, 0, 0, 0, /* ua - uz */
     7, 0, 0, 0, 72, 0, 0, 0, 28, 0, 0, 0, 0, 0, 5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 3, 0, /* va - vz */
     36, 1, 1, 0, 38, 0, 0, 33, 36, 0, 0, 4, 1, 8, 15, 0, 0, 0, 4, 2, 0, 0, 1, 0, 0, 0, /* wa - wz */
     1, 0, 2, 0, 0, 1, 0, 0, 3, 0, 0, 0, 0, 0, 1, 5, 0, 0, 0, 3, 0, 0, 1, 0, 0, 0, /* xa - xz */
     14, 5, 4, 2, 7, 12, 12, 6, 10, 0, 0, 3, 7, 5, 17, 3, 0, 4, 16, 30, 0, 0, 5, 0, 0, 0, /* ya - yz */
     1, 0, 0, 0, 4, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}; /* za - zz */

char alphabet[26] "abcdefghijklmnopqrstuvwxyz";

main()
{
	register int ans;
	register int i, j;
	int tvec[2];
	extern fout;

	fout = dup(1);
	time(tvec);
	srand(tvec[1]);
	for(j = 0; j < 12; j++) {
		ans = 0;
		for(i = 0; i < 8; i++) {
			ans = freq(frequency[ans]);
			putchar(alphabet[ans-1]);
		}
		if(j == 5) putchar('\n');
		else { putchar(' '); putchar(' '); }
	}
	putchar('\n');
	flush();
}

freq(table)
int table[];
{
	register int i, rowsum, row;

/*	Calculate total frequency in the row of input character.	*/

	rowsum = 0;
	for(i = 0; i < 26; i++)
		rowsum = rowsum + table[i];

/*	Now find random position within the row.	*/

	rowsum = rand()%rowsum;
	for(row = 0, i = 0; rowsum >= row; i++)
		row = row + table[i];

	return i;
}
