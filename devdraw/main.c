/* 2011 JGL (yiyus) */
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <err.h>
#include <errno.h>
#include <sys/stat.h>

#include <ixp.h>

#include "devdraw.h"

int debuglevel = 0;

#define GETARG() (cp-*argv == strlen(*argv)-1) ? *++argv : cp+1

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
		snprintf(buf, sizeof(buf), "unix!%s/win", nsdir);
		address = buf;
	}

	/* Connect to X */
	if(xinit() != 0)
		errx(1, "unable to connect to X server");

	fd = ixp_announce(address);
	if(fd < 0) {
		err(1, "ixp_announce");
	}

	ixp_listen(&srv, fd, &p9srv, ixp_serve9conn, NULL);

	i = ixp_serverloop(&srv);

	return i;
}
