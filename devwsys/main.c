/* 2011 JGL (yiyus) */
#include <err.h>
#include <errno.h>
#include <sys/stat.h>
#include <lib9.h>
#include <draw.h>
#include <memdraw.h>

#include <ixp.h>

#include "keyboard.h"
#include "mouse.h"
#include "devwsys.h"
#include "x11/inc.h"
#include "x11/x.h"

#define GETARG() (cp-*argv == strlen(*argv)-1) ? *++argv : cp+1

int debuglevel = 0;
int nwindow;
Window **window;

extern Ixp9Srv p9srv; /* fs.c:/p9srv */

void endconnection(IxpConn*);
void eventfdready(IxpConn*);

int
main(int argc, char **argv)
{
	IxpServer srv = {0};
	char *address, *cp, buf[512];
	char *argv0;
	int fd, i;

	address = getenv("IXP_ADDRESS");
	/* XXX TODO: use ARGBEGIN, ... from <ixp_local.h> */
	argv0 = argv[0];
	while(*++argv) {
		if(strcmp(*argv, "--") == 0 || !(**argv == '-')) {
			break;
		}
		for(cp=*argv+1; cp<*argv+strlen(*argv); ++cp) {
			if(*cp == 'd') {
				debuglevel++;
				break;
			} else if(*cp == 'a') {
				address = GETARG();
				break;
			} else {
				errx(1, "usage: %s [-d] [ -a ADDRESS ]", argv0);
			}
		}
	}
	if(!address) {
		char *nsdir = ixp_namespace();
		if(mkdir(nsdir, 0700) == -1 && errno != EEXIST) {
			err(1, "mkdir: %s", nsdir);
		}
		snprint(buf, sizeof(buf), "unix!%s/wsys", nsdir);
		address = buf;
	}

	/* Connect to X */
	if(xinit() != 0)
		errx(1, "unable to connect to X server");

	// /* TODO: we'll need this for drawing. */
	//memimageinit();

	fd = ixp_announce(address);
	if(fd < 0) {
		err(1, "ixp_announce");
	}

	ixp_listen(&srv, fd, &p9srv, ixp_serve9conn, NULL);
	ixp_listen(&srv, xconn.fd, nil, eventfdready, endconnection);

	i = ixp_serverloop(&srv);

	return i;
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
	int closedwin;

	USED(c);
	closedwin = xnextevent();
	if(closedwin >= 0)
		deletewin(closedwin);
}
