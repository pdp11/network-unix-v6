char *mypath;

m_maildir(folder)
{
	register char *fold, *path, *cp;
	static char mailfold[128];

	m_getdefs();
	if(!(fold = folder))
		fold = m_getfolder();
	if(*fold == '/' || *fold == '.')
		return(fold);
	cp = mailfold;
	if((path = m_find("path")) != -1 && *path) {
		if(*path != '/')
			cp = copy("/", copy(mypath, cp));
		cp = copy(path, cp);
		if(cp[-1] != '/')
			*cp++ = '/';
	}
	copy(fold, cp);
	return(mailfold);
}
