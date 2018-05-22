copyip(ipfrom, ipto)
int *ipfrom, *ipto;
{
	register int *ipf, *ipt;

	ipf=ipfrom;  ipt=ipto;
	while((*ipt = *ipf)  && *ipf++ != -1)
		ipt++;
	*ipt = 0;
	return(ipt);
}

