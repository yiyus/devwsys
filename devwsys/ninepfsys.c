#include <time.h>
#include <lib9.h>
#include <draw.h>
#include <cursor.h>
#include <ninep.h>
#include <ninepserver.h>
#include "dat.h"
#include "fns.h"
#include "fsys.h"

/* Error messages */
char
	Edeleted[] = "window deleted",
	Einterrupted[] = "interrupted",
	Einuse[] = "file in use",
	Enofile[] = "file not found",
	Enodrawimage[] = "unknown id for draw image",
	Enoperm[] = "permission denied",
	Eshortread[] =	"draw read too short",
	Eshortwrite[] =	"draw write too short";

/* Global Vars */
static char *snarfbuf;
static Pending *killp;

void
fsinit(Ninepserver *s)
{
	ninepaddfile(s, Qroot, Qkill, "kill", 0444, eve);
	ninepadddir(s, Qroot, Qdraw, "draw", 0555, eve);
	ninepadddir(s, Qroot, Qwsys, "wsys", 0555, eve);
}

static
void
drawaddfiles(Ninepserver *s, DClient *c)
{
	int i;
	char name[12];
	Path p, cp;

	i = drawpath(c);
	p = PATH(i, 0);
	cp = p|Qdrawn;
	i = sprint(name, "%d", i);
	name[i] = 0;
	ninepadddir(s, Qdraw, cp, name, 0555, eve);
	ninepaddfile(s, cp, p|Qctl, "ctl", 0666, eve);
	ninepaddfile(s, cp, p|Qdata, "data", 0666, eve);
	ninepaddfile(s, cp, p|Qcolormap, "colormap", 0444, eve);
	ninepaddfile(s, cp, p|Qrefresh, "refresh", 0444, eve);
}

static
Path
wsysaddfiles(Ninepserver *s, Window *w)
{
	int i;
	char name[12];
	Path p;

	i = sprint(name, "%d", w->id);
	name[i] = 0;
	p = PATH(w->id, 0);
	ninepadddir(s, Qwsys, p, name, 0555, eve);
	ninepaddfile(s, p, p|Qcons, "cons", 0666, eve);
	ninepaddfile(s, p, p|Qconsctl, "consctl", 0222, eve);
	ninepaddfile(s, p, p|Qkeyboard, "keyboard", 0666, eve);
	ninepaddfile(s, p, p|Qcursor, "cursor", 0666, eve);
	ninepaddfile(s, p, p|Qmouse, "mouse", 0666, eve);
	ninepaddfile(s, p, p|Qpointer, "pointer", 0666, eve);
	ninepaddfile(s, p, p|Qsnarf, "snarf", 0666, eve);
	ninepaddfile(s, p, p|Qlabel, "label", 0666, eve);
	ninepaddfile(s, p, p|Qwctl, "wctl", 0666, eve);
	ninepaddfile(s, p, p|Qwinid, "winid", 0666, eve);
	ninepaddfile(s, p, p|Qwinname, "winname", 0666, eve);
	ninepadddir(s, p, p|Qwsys, "wsys", 0555, eve);
	ninepadddir(s, p, p|Qdraw, "draw", 0555, eve);
	ninepaddfile(s, p|Qdraw, p|Qnew, "new", 0666, eve);
	ninepbind(s, p|Qwsys, Qwsys);
	ninepbind(s, p|Qdraw, Qdraw);
	return p;
}

/* Service Functions */
char*
wsysattach(Qid *qid, char *uname, char *aname)
{
	Window *w;

	if(strcmp(aname, "/") == 0)
		return nil;
	w = newwin();
	if(w == nil)
		return Enomem;
	qid->path = wsysaddfiles(server, w);
	if(!drawattach(w,aname)) {
		if(w){
			assert(w == window[nwindow-1]);
			free(w);
			nwindow--;
		}
		return Enomem;
	}
	return nil;
}	

char*
wsysopen(Qid *qid, int mode)
{
	int type;
	Window *w;
	DClient *cl;

	w = qwindow(qid);
	if(w != nil && w->deleted)
		return Edeleted;
	type = QTYPE(qid->path);
	if(type == Qnew){
		cl = drawnewclient(w->draw);
		if(cl == 0)
			return Enodev;
		drawaddfiles(server, cl);
		type = Qctl;
		qid->path = PATH(drawpath(cl), type);
	}
	switch(type){
	case Qmouse:
	case Qpointer:
		if(w->mouse.open)
			return Einuse;
		w->mouse.open = 1;
		return nil;
	case Qsnarf:
		/* one at a time please */
		if((mode&3) == ORDWR)
			return Enoperm;
		return nil;

	/* Draw client */
	case Qctl:
	case Qdata:
	case Qcolormap:
	case Qrefresh:
		return drawopen(qid, type);
	}
	return nil;
}

char*
wsysread(Qid qid, char *buf, ulong *n, vlong offset)
{
	int type;
	char *s;
	Window *w;
	Pending *p;

	w = qwindow(&qid);
	type = QTYPE(qid.path);
	if(w != nil && w->deleted)
		return Edeleted;
	switch(type){
	case Qkill:
		killp = ninepreplylater(server);
		return nil;

	/* Window */
	case Qcons:
	case Qkeyboard:
		p = ninepreplylater(server);
		readkbd(w, p);
		return nil;
	case Qlabel:
		if(w->label)
			*n = ninepreadstr(offset, buf, strlen(w->label), w->label);
		return nil;
	case Qmouse:
	case Qpointer:
		p = ninepreplylater(server);
		readmouse(w, p);
		return nil;
	case Qsnarf:
		s = xgetsnarf(w);
		*n = ninepreadstr(offset, buf, strlen(s), s);
		return nil;
	case Qwctl:
		// XXX TODO: current
		s = w-> visible ? "visible" : "hidden";
		*n = sprint(buf, "%11d %11d %11d %11d %s %s ", w->orig.x +  w->screenr.min.x, w->orig.y + w->screenr.min.y, w->screenr.max.x, w->screenr.max.y, s, "current");
		return nil;
	case Qwinid:
		*n = sprint(buf, "%11d ", w->id);
		buf = &buf[offset];
		*n -= offset;
		return nil;
	case Qwinname:
		*n = ninepreadstr(offset, buf, strlen(w->name), w->name);
		return nil;

	/* Draw client */
	case Qctl:
	case Qdata:
	case Qcolormap:
	case Qrefresh:
		return drawread(qid, buf, n, offset);
	}
	// unreachable
	return "Read called on an unreadable file";
}

char*
wsyswrite(Qid qid, char *buf, ulong *n, vlong offset)
{
	int type;
	ulong count;
	char *label, *p;
	Window *w;
	Point pt;

	w = qwindow(&qid);
	if(w == nil || w->deleted)
		return Edeleted;
	type = QTYPE(qid.path);
	count = *n;
	switch(type){
	case Qconsctl:
		return nil;
	case Qcursor:
		if(cursorwrite(w, buf, *n) < 0){
			*n = 0;
			return "error"; // XXX TODO
		}
		return nil;
	case Qlabel:
		if((label = malloc(count + 1)) == nil) {
			*n = 0;
			return Enomem;
		}
		memcpy(label, buf, *n);
		label[count] = 0;
		setlabel(w, label);
		return nil;
	case Qmouse:
	case Qpointer:
		if(count == 0)
			return Eshortwrite;
		p = &buf[1];
		pt.x = strtoul(p, &p, 0);
		if(p == 0)
			return Eshortwrite;
		pt.y = strtoul(p, 0, 0);
		xsetmouse(w, pt);
		return nil;
	case Qsnarf:
		if(offset+count >= SnarfSize)
			return "too much snarf";
		if(count == 0)
			break;
		if(snarfbuf == nil)
			*n = 0;
		else
			*n = strlen(snarfbuf);
		if(offset+count > *n){
			*n = offset+count;
			if((p = malloc(*n+1)) == nil)
			if(p == nil)
				return Enomem;
			if(snarfbuf){
				strcpy(p, snarfbuf);
				free(snarfbuf);
			}
			snarfbuf = p;
		}
		memmove(snarfbuf+offset, buf, count);
		snarfbuf[offset+count] = '\0';
		return nil;
	case Qwctl:
		return wctlmesg(w, buf, count);

	/* Draw client */
	case Qctl:
	case Qdata:
	case Qcolormap:
	case Qrefresh:
		return drawwrite(qid, buf, n, offset);
	}
	// unreachable
	return "Write called on an unwritable file";
}

char*
wsysclose(Qid qid, int mode) {
	int type;
	Window *w;

	w = qwindow(&qid);
	type = QTYPE(qid.path);
	switch(type){
	case Qcons:
	case Qkeyboard:
		w->kbd.wi = w->kbd.ri;
		w->kbdreqs.wi = w->kbdreqs.ri;
		return nil;
	case Qmouse:
	case Qpointer:
		w->mouse.open = 0;
		w->mouse.wi = w->mouse.ri;
		w->mousereqs.wi = w->mousereqs.ri;
		return nil;
	case Qsnarf:
		if(mode == OWRITE){
			if(snarfbuf)
				xputsnarf(snarfbuf);
			else
				xputsnarf("");
		}
		free(snarfbuf);
		snarfbuf = nil;
		return nil;

	/*
	 * If it is a draw file, drawclose will be called
	 * by updateref. If it were called now it could
	 * free the client and qwindow (called from
	 * updateref) would not find a window.
	 */;
	}
	return nil;
}

Window*
qwindow(Qid *qid)
{
	int slot, type;

	slot = QSLOT(qid->path);
	type = QTYPE(qid->path);
	if(type <= Qnew)
		return winlookup(slot);
	else if(type > Qnew)
		return drawwindow(slot);
	return nil;
}

int
killreply(Window *w)
{
	Client *c;
	Fcall *f;

	if(killp == nil)
		return 0;
	c = killp->c;
	f = &killp->fcall;
	f->count = sprint(c->data, "%d\n", w->pid);
	f->data = c->data;
	ninepcompleted(killp);
	killp = nil;
	w->pid = 0;
	return 1;
}

void
readreply(void *v, char *buf)
{
	Pending *p;

	p = v;
	p->fcall.data = buf;
	p->fcall.count = strlen(buf);
	ninepcompleted(p);
}

Ninepops ops = {
	nil,			/* newclient */
	nil,			/* freeclient */

	wsysattach,	/* attach */
	nil,			/* walk */
	wsysopen,	/* open */
	nil,			/* create */
	wsysread,		/* read */
	wsyswrite,	/* write */
	wsysclose,	/* close */
	nil,			/* remove */
	nil,			/* stat */
	nil,			/* wstat */
};

