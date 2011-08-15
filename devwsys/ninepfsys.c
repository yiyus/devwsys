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

void
fsinit(Ninepserver *s)
{
	ninepadddir(s, Qroot, Qwsys, "wsys", 0555, "inferno");
	ninepadddir(s, Qroot, Qdraw, "draw", 0555, "inferno");
}

static
void
drawaddfiles(Ninepserver *s, DClient *c)
{
	int i;
	char n[12];
	Path p, cp;

	i = drawpath(c);
	p = PATH(i, 0);
	cp = p|Qdrawn;
	n[sprint(n, "%d", i)] = 0;
	ninepadddir(s, Qdraw, cp, n, 0555, "inferno");
	ninepaddfile(s, cp, p|Qctl, "ctl", 0666, "inferno");
	ninepaddfile(s, cp, p|Qdata, "data", 0666, "inferno");
	ninepaddfile(s, cp, p|Qcolormap, "colormap", 0444, "inferno");
	ninepaddfile(s, cp, p|Qrefresh, "refresh", 0444, "inferno");
}

static
Path
wsysaddfiles(Ninepserver *s, Window *w)
{
	int n;
	char name[12];
	Path p;

	p = PATH(w->id, 0);
	n = sprint(name, "%d", w->id);
	name[n] = 0;
	ninepadddir(s, Qwsys, p, name, 0555, "inferno");
	ninepaddfile(s, p, p|Qcons, "cons", 0666, "inferno");
	ninepaddfile(s, p, p|Qconsctl, "consctl", 0222, "inferno");
	ninepaddfile(s, p, p|Qkeyboard, "keyboard", 0666, "inferno");
	ninepaddfile(s, p, p|Qcursor, "cursor", 0666, "inferno");
	ninepaddfile(s, p, p|Qmouse, "mouse", 0666, "inferno");
	ninepaddfile(s, p, p|Qpointer, "pointer", 0666, "inferno");
	ninepaddfile(s, p, p|Qsnarf, "snarf", 0666, "inferno");
	ninepaddfile(s, p, p|Qkill, "kill", 0666, "inferno");
	ninepaddfile(s, p, p|Qlabel, "label", 0666, "inferno");
	ninepaddfile(s, p, p|Qwctl, "wctl", 0666, "inferno");
	ninepaddfile(s, p, p|Qwinid, "winid", 0666, "inferno");
	ninepaddfile(s, p, p|Qwinname, "winname", 0666, "inferno");
	ninepadddir(s, p, p|Qwsys, "wsys", 0555, "inferno");
	ninepbind(s, p|Qwsys, Qwsys);
	ninepadddir(s, p, p|Qdraw, "draw", 0555, "inferno");
	ninepaddfile(s, p|Qdraw, p|Qnew, "new", 0666, "inferno");
	ninepbind(s, p|Qdraw, Qdraw);
	return p;
}

/* Service Functions */
char*
wsysattach(Qid *qid, char *uname, char *aname)
{
	Window *w;

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
	w->ref = 1;
	return nil;
}	

char*
wsyswalk(Qid *qid, char *name)
{
	Window *w;

	w = qwindow(qid);
	if(w != nil)
		incref(&w->ref);
	return "continue";
}

char*
wsysopen(Qid *qid, int mode)
{
	int type;
	Window *w;
	DClient *cl;

	w = qwindow(qid);
	if(w && w->deleted)
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
		cl = client[QSLOT(qid->path)];
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
	if(w && w->deleted && type != Qkill)
		return Edeleted;
	switch(type){
	case Qcons:
	case Qkeyboard:
		p = ninepreplylater(server);
		readkbd(w, p);
		return nil;
	case Qkill:
		if(w->killpend == nil)
			w->killpend = ninepreplylater(server);
		*n = 0;
		return nil;
	case Qlabel:
		if(w->label)
			ninepreadstr(0, buf, strlen(w->label), w->label);
		return nil;
	case Qmouse:
	case Qpointer:
		p = ninepreplylater(server);
		readmouse(w, p);
		return nil;
	case Qsnarf:
		s = xgetsnarf(w);
		*n = ninepreadstr(0, buf, strlen(s), s);
		return nil;
	case Qwctl:
		// XXX TODO: current
		s = w-> visible ? "visible" : "hidden";
		*n = sprint(buf, "%11d %11d %11d %11d %s %s ", w->orig.x +  w->screenr.min.x, w->orig.y + w->screenr.min.y, w->screenr.max.x, w->screenr.max.y, s, "current");
		return nil;
	case Qwinid:
		*n = sprint(buf, "%11d ", w->id);
		return nil;
	case Qwinname:
		*n = ninepreadstr(0, buf, strlen(w->name), w->name);
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
	if(w && w->deleted)
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
wsysclunk(Qid qid, int mode) {
	int type;
	Window *w;

	w = qwindow(&qid);
	if(w != nil)
		decref(&w->ref);
	type = QTYPE(qid.path);
	switch(type){
	case Qroot:
		if(w && w->ref == 0){
			drawdettach(w);
			w->draw = nil;
			deletewin(w);
		}
		return nil;
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

	/* Draw client */
	case Qctl:
	case Qdata:
	case Qcolormap:
	case Qrefresh:
		return drawclose(qid, mode);
	}
	return nil;
}

/*
void
fs_freefid(IxpFid *f) {
	IxpFileId *id, *tid;
	Window *w;

	tid = f->aux;
	while((id = tid)) {
		if(iswindow(tid->tab.type) && tid->nref == 0){
			w = tid->p.window;
			deletewin(w);
			drawdettach(w);
			w->draw = nil;
		}
		tid = id->next;
		ixp_srv_freefile(id);
	}
}
*/

void
killrespond(Window *w)
{
	Client *c;
	Fcall *f;
	Pending *p;

	p = w->killpend;
	c = p->c;
	f = &p->fcall;
	f->count = sprint(c->data, "%d", w->pid);
	f->data = c->data;
	ninepcompleted(p);
}

Window*
qwindow(Qid *qid)
{
	int slot, type;

	slot = QSLOT(qid->path);
	type = QTYPE(qid->path);
	if(type <= Qnew)
		return winlookup(slot);
	if(type >= Qdrawn)
		return drawwindow(client[slot-1]);
	return nil;
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
	wsyswalk,	/* walk */
	wsysopen,	/* open */
	nil,			/* create */
	wsysread,		/* read */
	wsyswrite,	/* write */
	wsysclunk,	/* clunk */
	nil,			/* remove */
	nil,			/* stat */
	nil,			/* wstat */
};

