char	foldprot[];

makedir(dir)
{
	register int i, j;
	int status;

	if((i = fork()) == 0) {
		execl("/bin/mkdir", "mkdir", dir, 0);
		printf("Can't exec mkdir!!?\n");
		flush();
		return(0);
	}
	if(i == -1) {
		printf("Can't fork\n");
		flush();
		return(0);
	}
	while((j = waita(&status)) != i && j != -1) ;
	if(status) {
		printf("Bad exit status (%o) from mkdir.\n", status);
		flush();
		return(0);
	}
	chmod(dir, ((i = m_find("folder-protect")) != -1)?
		   atooi(i) : atooi(foldprot));
	return(1);
}
