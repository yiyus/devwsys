/* 2011 JGL (yiyus) */
#include <err.h>
#include <errno.h>
#include <sys/stat.h>
#include <lib9.h>
#include <draw.h>
#include <cursor.h>
#include "dat.h"
#include "fns.h"
#include <ixp.h>

extern Ixp9Srv p9srv; /* ixp_fsys.c:/p9srv */

void endconnection(IxpConn*);
void eventfdready(IxpConn*);

int
fsloop(char *address)
{
	char buf[512];
	int fd, xfd;
	IxpServer srv = {0};

	if(!address)
		address = getenv("IXP_ADDRESS");
	if(!address) {
		char *nsdir = ixp_namespace();
		if(mkdir(nsdir, 0700) == -1 && errno != EEXIST) {
			err(1, "mkdir: %s", nsdir);
		}
		snprint(buf, sizeof(buf), "unix!%s/wsys", nsdir);
		address = buf;
	}

	fd = ixp_announce(address);
	if(fd < 0) {
		err(1, "ixp_announce");
	}
	xfd = xinit();
	if(xfd < 0) {
		fatal("unable to connect to X server");
	}

	ixp_listen(&srv, fd, &p9srv, ixp_serve9conn, NULL);
	ixp_listen(&srv, xfd, nil, eventfdready, endconnection);

	return ixp_serverloop(&srv);
}

void
endconnection(IxpConn *c)
{
	USED(c);
	xclose();
}

void
eventfdready(IxpConn *c)
{
	USED(c);
	xnextevent();
}
