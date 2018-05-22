main() {
	register i,j;
	register char c;

	for(;;) {
		for(i = 1;i <=5;i++) {
			j = 0;
			while((c = getchar()) != '\n') {
				if(c == '\0') {
					putchar('\n');
					exit();
				}
				putchar(c);
				j++;
			}
			if(i != 5) {
				if(! (j/8)) { putchar('\t'); putchar('\t'); }
				else putchar('\t');
			}
			else putchar('\n');
		}
	}
}
