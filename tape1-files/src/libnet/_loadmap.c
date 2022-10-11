#include "sys/types.h"
#include "sys/stat.h"
#define	LIBN
#include "netlib.h"
#include "netmap.h"
#include "netfiles.h"

char		*_mapname;	/* file name override */
static	int	loaded;		/* counter to allow nested load/free calls */
static	int	locked;		/* indicates map locked in core */
static	char	map_bin[] = MAP_BIN;	/* default map */
static	char	map_obin[] = MAP_OBIN;	/* backup map */
static	char	*map_base;	/* base of allocated area */
static	int	map_size;	/* size of allocated area */
static	time_t	map_time;	/* mode time of map */
/*
 * pointers for table access
 */
map_head	*_netmap;	/* -> network map header */
net_ent		*_nettab;	/* -> network table */
host_ent	*_hosttab;	/* -> host table */
char		*_namebuf;	/* -> name buffer */
char		*_dialbuf;	/* -> dial digit buffer */
unsigned	_lhostcnt;	/* # local hosts */
netaddr		_lhosttab[_LHOSTSIZ];	/* local host addresses */
unsigned	_dialcnt;	/* dial digit count */
char		_dialext[_DIALSIZ];	/* dial digit extension buffer */

/*
 * load the network map into core and initialize it
 *	may be called successively
 *	each call must be balanced by a call to _freemap
 *
 *	if map is locked, it will be reread if it changes
 */
_loadmap()
{
	register int fd;	/* map file descriptor */
	register char *mp;	/* map base ptr */
	struct stat sb;		/* stat buffer */
	extern char *malloc();

	if (loaded++ > 0)
		return 1;
	if (locked) {
		/*
		 * if using old map, switch to regular map
		 */
		if (!_mapname || _mapname == map_obin)
			_mapname = map_bin;
		/*
		 * if map inaccessible or unchanged just return
		 */
		if (stat(_mapname, &sb) != 0 || map_time == sb.st_mtime)
			return 1;
	}
	if (_mapname)
		fd = open(_mapname, 0);
	else if ((fd = open(_mapname = map_bin, 0)) < 0)
		fd = open(_mapname = map_obin, 0);
	if (fd < 0)
		return 0;
	/*
	 * save size and mod time
	 */
	if (fstat(fd, &sb) != 0) {
		close(fd);
		return 0;
	}
	map_size = sb.st_size;
	map_time = sb.st_mtime;
	/*
	 * allocate space and read in map
	 */
	if (!(map_base = malloc(map_size))) {
		close(fd);
		return 0;
	}
	if (read(fd, map_base, map_size) != map_size) {
		close(fd);
		return 0;
	}
	close(fd);
	/*
	 * setup access pointers
	 */
	_netmap = (map_head *)(mp = map_base);
	_nettab = (net_ent *)(mp += sizeof(map_head));
	_hosttab = (host_ent *)(mp += _netmap->map_nnet * sizeof(net_ent));
	_namebuf = (char *)(mp += _netmap->map_nhost * sizeof(host_ent));
	_dialbuf = (char *)(mp += _netmap->map_nchar);
	/*
	 * set local network chain
	 */
	localnet();
	return 1;
}

/*
 * lock map into core
 *	used by programs which access map continuously
 */
loadnetmap()
{
	if (locked)
		return 1;
	if (!_loadmap())
		return 0;
	++locked;
	return 1;
}

freenetmap()
{
	if (locked) {
		locked = 0;
		_freemap();
	}
}

/*
 * unload the map, free storage
 */
_freemap()
{
	if (--loaded > 0)
		return;
	loaded = 0;
	if (!locked) {
		_dialcnt = 0;
		if (map_base) {
			free(map_base);
			map_base = 0;
		}
	}
}

/*
 * When the network map is initially built, the network entries
 * are chained into a forward list of which _netmap[0] is the head.
 * localnet() reads a file containing the various network addresses
 * by which the host is known; it reorders the chain so that networks
 * local to the host are searched first.
 */
static
localnet()
{
#ifdef	NETWORKS
#include "stdio.h"

	FILE	*fp;
	register net_ent *np;
	register map_index *pp, *hp;
	netnumb	net;
	char	nname[NETNAMSIZ], hname[NETNAMSIZ];
	char	buf[BUFSIZ];

	if ((fp = fopen(NETWORKS, "r")) == NULL)
		return;
	/*
	 * setup pointer to index at head of list
	 * read file for network addresses
	 */
	hp = &_nettab[0].net_next;
	while (fgets(buf, sizeof buf, fp)) {
		switch (*_2parse(buf, 1, hname, nname)) {
		case '\n':
		case '\0':
			break;
		default:
			continue;
		}
		/*
		 * get net part of address
		 */
		net = _isnum(nname)? _nnetn(nname) : _snetn(nname);
		if (isbadnet(net))
			continue;
		/*
		 * look forward from the head to find ancestor net
		 * move net to head of list and make that net the new head
		 */
		for (pp = hp; *pp != 0; pp = &_nettab[*pp].net_next) {
			if (*pp == net) {
				np = &_nettab[net];
				if (hp != pp) {
					*pp = np->net_next;
					np->net_next = *hp;
					*hp = net;
				}
				hp = &np->net_next;
				break;
			}
		}
		/*
		 * install host address in table
		 */
		if (_lhostcnt < _LHOSTSIZ) {
			if (_isnum(hname))
				_lhosttab[_lhostcnt] = _nhosta(hname, net); else
				_lhosttab[_lhostcnt] = _shosta(hname, net);
			if (!isbadhost(_lhosttab[_lhostcnt]))
				++_lhostcnt;
		}
	}
	fclose(fp);
#endif	NETWORKS
}
