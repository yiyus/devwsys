/* 2011 JGL (yiyus) */
#include <lib9.h>
#include <ninep.h>
#include <ninepserver.h>
#include <draw.h>
#include <cursor.h>
#include "dat.h"
#include "fns.h"
#include "fsys.h"

char*
fsloop(char *address, int xfd)
{
	char *ns, *err;
	Ninepserver s;

	server = &s;
	if(address == nil){
		ns = ninepnamespace();
		if(ns == nil)
			address = "tcp!*!564";
		else
			address = smprint("%s/wsys", ns);
	}
	if(debuglevel > 0)
		ninepdebug();
	err = ninepinit(&s, &ops, address, 0555, 0);
	if(err != nil)
		return err;
	fsinit(&s);
	for(;;) {
		nineplisten(&s, xfd);
		err = ninepwait(&s);
		if(err != nil)
			fprint(2, "%s\n", err);
		if(ninepready(&s, xfd))
			xnextevent();
		ninepdefault(&s);
	}
	ninepend(&s);
	if(ns != nil){
		ninepfree(address);
		ninepfree(ns);
	}
	return nil;
}

