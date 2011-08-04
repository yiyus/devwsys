/* 2011 JGL (yiyus) */
#include <lib9.h>
#include <styxserver.h>
#include <draw.h>
#include <cursor.h>
#include "dat.h"
#include "fns.h"
#include "fsys.h"

/* From libstyx */
int styxnewmsg(Styxserver*, int);
void styxnewclient(Styxserver*, int);
void styxfreeclient(Styxserver*, int);

int
fsloop(char *address)
{
	int xfd;
	Styxserver s;

	server = &s;
	styxinit(&s, &ops, address, 0555, 0);
	fsysinit(&s);
	/* Connect to X */
	xfd = xinit();
	if(!xfd)
		fatal("unable to connect to X server");
	for(;;) {
		styxnewclient(&s, xfd);
		styxwait(&s);
		if(styxnewmsg(&s, xfd)){
			xnextevent();
			styxfreeclient(&s, xfd);
		}
		styxprocess(&s);
	}
	xclose();
	return 0;
}
