/* 2011 JGL (yiyus) */
#include <lib9.h>
#include <ninep.h>
#include <ninepserver.h>
#include <draw.h>
#include <cursor.h>
#include "dat.h"
#include "fns.h"
#include "fsys.h"

static
void
updateref(void)
{
	int t;
	Fcall *f;
	Qid *qid;
	Window *w;

	if(server->curc == nil)
		return;
	f = &server->fcall;
	t = f->type;
	if(t == Rwalk && f->nwqid > 0)
		qid = &f->wqid[f->nwqid-1];
	else
		qid = &f->qid;
	if((w = qwindow(qid)) == nil)
		return;
	if(t == Rattach || t == Rwalk)
		w->ref++;
	else if(t == Rclunk){
		w->ref--;
		if(QTYPE(qid->path) > Qdrawn && w->draw)
			drawclose(*qid);
		if(w->ref == 0){
			/* ignore kill file, delete now */
			w->pid = 0;
			deletewin(w);
		}
	}
}

char*
fsloop(char *address, int xfd)
{
	char *ns, *err;
	Ninepserver s;
	Window *w;

	server = &s;
	if(address == nil){
		ns = ninepnamespace();
		if(ns == nil)
			address = "tcp!*!564";
		else
			address = smprint("%s/wsys", ns);
	}
	if(debug&Debug9p)
		ninepdebug();
	eve = ninepsetowner(nil);
	err = ninepinit(&s, &ops, address, 0555, 0);
	if(err != nil)
		return err;
	fsinit(&s);
	nineplisten(&s, xfd);
	for(;;) {
		err = ninepwait(&s);
		if(err != nil && debug&Debug9p){
			fprint(2, "%s\n", err);
			w = server->curc->u;
			if(w != nil)
				deletewin(w);
			continue;
		}
		if(ninepready(&s, xfd))
			xnextevent();
		if(s.fcall.type == Tauth)
			nineperror(&s, "devwsys: authentication not required");
		else
			ninepdefault(&s);
		updateref();
		ninepreply(&s);
	}
	ninepend(&s);
	if(ns != nil){
		ninepfree(address);
		ninepfree(ns);
	}
	return nil;
}

