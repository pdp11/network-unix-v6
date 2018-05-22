char msgprot[];

m_gmprot()
{
	register int prot;

	if((prot = m_find("msg-protect")) != -1)
		prot = atooi(prot);
	else
		prot = atooi(msgprot);
	return(prot);
}
