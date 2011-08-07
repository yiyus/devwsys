/* 2011 JGL (yiyus) */
#include <lib9.h>
#include <ninep.h>
#include <ninepserver.h>
#include <draw.h>
#include <cursor.h>
#include "dat.h"
#include "fns.h"
#include "fsys.h"

void
fsloop(char *address, int xfd)
{
	Ninepserver s;

	server = &s;
	ninepinit(&s, &ops, address, 0555, 0);
	fsysinit(&s);
	for(;;) {
		nineplisten(&s, xfd);
		ninepwait(&s);
		if(ninepready(&s, xfd))
			xnextevent();
		ninepprocess(&s);
	}
	return;
}

