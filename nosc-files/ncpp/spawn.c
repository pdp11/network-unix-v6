/*	Forking is complicated by the fact that a process
 *	that exits becomes a "ZOMBIE".	It does not leave
 *      UNIX until it is waited for by its parent.  There-
 *      fore, the logger creates two processes,
 *	a child and a grandchild, so that the grandchild,
 *	which actually does the work, will be adopted by
 *	the init process when the child exits.
 */
spawn()
{	register int k, l;
	int pstat;

	while ((k = fork()) == -1) sleep(10);
	if (k)
	{	wait(&pstat);	/* Wait for k below to exit */
		return (1);	/* Returns non-zero in parent */
	}
	else
	{	while ((l = fork()) == -1) sleep(10);
		if (l) exit();	/* After creating l, exit. */
		return(0);
	}
}



