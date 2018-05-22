#
/*  H O S T S	-	decodes /host_status, a file updated by the
			 ncpdaemon from type 6 imp-to-host messages
*/



int stat_tab[256];


char *host_tab[]
{
	"ucla-nmc",
	"sri-arc",
	"ucsb-mod75",
	"utah-10",
	"bbn-11x",
	"rand-rcc",
	"sdc-lab",
	"harv-10",
	"ll-67",
	"su-ai",
	"ill-cac",
	"case-10",
	"cmu-10b",
	"i4-tenex",
	"ames-67",
	"radc",
	"nbs-icst",
	"etac",
	"lll-risos",
	"isi-speech11",
	"usc-44",
	"sdac-44",
	"belvoir",
	"arpa-dms",
	"brl",
	"cca-tenex",
	"parc-maxc",
	"fnwc",
	"lbl",
	"ucsd-cc",
	"hawaii-aloha",
	"bbn-ncc",
	"london",
	"office-1",
	"mit-multics",
	"sci",
	"ucla-ccn",
	"sri-ai",
	"scrl-elf",
	"bbn-tenex",
	"mit-dms",
	"sdc-cc",
	"harv-1",
	"ll-tx-2",
	"ill-nts",
	"cmu-10a",
	"i4-tenex",
	"usc-isi",
	"ll-ants",
	"parc-vts",
	"ucb",
	"hawaii-500",
	"ucla-ccbs",
	"chii",
	"utah-tip",
	"bbn-tenexb",
	"mit-ai",
	"ll-tsp",
	"univac",
	"cmu-11",
	"ames-tip",
	"mitre-tip",
	"radc-tip",
	"nbs-tip",
	"etac-tip",
	"isi-devtenex",
	"usc-tip",
	"gwc-tip",
	"docb-tip",
	"sdac-tip",
	"arpa-tip",
	"bbn-testip",
	"cca-tip",
	"parc-11",
	"fnwc-tip",
	"aloha-tip",
	"rml-tip",
	"ncc-tip",
	"nosar-tip",
	"london-tip",
	"rutgers-tip",
	"wpafb-tip",
	"afwl-tip",
	"su-dsl",
	"haskins",
	"mit-ml",
	"cmu-cc",
	"ames-11",
	"london-vdh",
	0
};

int host_num[]
{

	1,2,3,4,5,7,8,9,10,11,12,13,14,15,16,18,19,20,21,22,23,26,27,28,29,
	31,32,33,34,35,36,40,42,43,44,45,65,66,67,241,70,72,73,74,76,78,79,
	86,95,96,98,100,129,131,132,133,134,138,140,142,144,145,146,147,148,150,
	151,152,153,154,156,158,159,160,161,164,165,168,169,170,174,175,176,
	194,197,198,206,208,234,0
};

char *reasons[]
{
	"Unknown",
	"Unknown",
	"Unknown",
	"Unknown",
	"Unknown",
	"Scheduled PM",
	"Scheduled Hardware",
	"Scheduled Software",
	"Emergency Restart",
	"Power Outage",
	"Software Breakpoint",
	"Hardware Failure",
	"Not scheduled up",
	"Unknown",
	"Unknown",
	"Unknown"
};


char *days[]
{
	"Monday",
	"Tuesday",
	"Wednesday",
	"Thursday",
	"Friday",
	"Saturday",
	"Sunday",
	"Monday"
};

int fid;	/* file id of host_status file */
int hst_entry;	/* relative entry number of host name in host_tab */

/**/
main( argc,argv )
char *argv[];
{

	/* open and read in host stati */
	if( (fid = open( "/host_status",0 )) < 0 )
	{
		printf(" Cant open host status file \n");
		exit();
	}

	read( fid,stat_tab,512 );
	close( fid );

	/* does he want to see all alive hosts? */
	if( argc < 2 )
		prt_all();
	else
		/* while there are hosts to look for */
		while( --argc )
			/* if the string exits in host_tab get its entry num */
			if( (hst_entry = find_host( argv[argc] )) >= 0 )
				decode_status( hst_entry );
			else
				printf(" Unknown host %s\n",argv[argc]);
}
/**/
find_host( hstname )
char *hstname;
{

	register entry_num;
	register char *hname;

	for( entry_num = 0; (hname = host_tab[entry_num]) != 0; entry_num++ )
		if( compar( hname,hstname ) == 0 )
			return( entry_num );
	return( -1 );
}
/**/
prt_all()
{

	register entry_num;
	register j;

	printf("Alive Hosts are:\n");

	j = 0;
	for( entry_num = 1; host_tab[entry_num] ; entry_num++ )
	{
		if( stat_tab[ host_num[ entry_num ] ] == 0 )
		{
			printf("%-15s",host_tab[entry_num]);
			if( ++j == 4 )
			{
				printf("\n");
				j = 0;
			}
		}
	}
	printf("\n");
}
/**/
decode_status( entry_num )
{

	register status;
	register when;

	if( (status = stat_tab[ host_num[ entry_num ] ]) == 0 )
		printf("Host %s is alive\n",host_tab[ entry_num ] );
	else
	{
		when = status & 07777;
		if( when == 0777 )
			printf("Host %s is Dead - reason unknown\n",host_tab[entry_num]);
		else
			printf("Host %s is Dead - %s - will be alive: %s %d:%d\n",
			host_tab[ entry_num ],reasons[ (status>>12)&017 ],
			days[ when&07 ], (when>>3)&037, (when>>8)&017*5 );
	}
}

/**/
compar( s1,s2 )
char *s1;
char *s2;
{

	register char c1,c2;

	while( (c1 = *s1++) == (c2 = *s2++) && c1 != 0 );
	return( c1 );
}
