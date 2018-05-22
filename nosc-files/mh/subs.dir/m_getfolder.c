char    defalt[];

m_getfolder()
{
	register char *folder;

	m_getdefs();
	if((folder = m_find("folder")) == -1 || *folder == 0)
		folder = defalt;
	return(folder);
}
