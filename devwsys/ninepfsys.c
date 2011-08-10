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
	ninepadddir(s, Qroot, Qdraw, "draw", 0500, "inferno");
	ninepaddfile(s, Qdraw, Qnew, "new", 0600, "inferno");
	ninepadddir(s, Qroot, Qwsys, "wsys", 0500, "inferno");
}

static
void
drawaddfiles(Ninepserver *s, DClient *c)
{
	int id;
	char n[12];
	Path p;

	id = drawid(c);
	p = PATH(id, Qdraw);
	n[sprint(n, "%d", id)] = 0;
	ninepadddir(s, Qdraw, p, n, 0500, "inferno");
	ninepaddfile(s, p, p|Qctl, "ctl", 0600, "inferno");
	ninepaddfile(s, p, p|Qdata, "data", 0600, "inferno");
	ninepaddfile(s, p, p|Qcolormap, "colormap", 0400, "inferno");
	ninepaddfile(s, p, p|Qrefresh, "refresh", 0400, "inferno");
}

static
void
wsysaddfiles(Ninepserver *s, Window *w)
{
	char n[12];
	Path p;

	p = PATH(w->id, 0);
	n[sprint(n, "%d", w->id)] = 0;
	ninepadddir(s, Qwsys, p, n, 0500, "inferno");
	ninepaddfile(s, p, p|Qcons, "cons", 0600, "inferno");
	ninepaddfile(s, p, p|Qcons, "keyboard", 0600, "inferno");
	ninepaddfile(s, p, p|Qconsctl, "consctl", 0200, "inferno");
	ninepaddfile(s, p, p|Qcursor, "cursor", 0600, "inferno");
	ninepaddfile(s, p, p|Qmouse, "mouse", 0600, "inferno");
	ninepaddfile(s, p, p|Qmouse, "pointer", 0600, "inferno");
	ninepaddfile(s, p, p|Qsnarf, "snarf", 0600, "inferno");
	ninepaddfile(s, p, p|Qkill, "kill", 0600, "inferno");
	ninepaddfile(s, p, p|Qlabel, "label", 0600, "inferno");
	ninepaddfile(s, p, p|Qwctl, "wctl", 0600, "inferno");
	ninepaddfile(s, p, p|Qwinid, "winid", 0600, "inferno");
	ninepaddfile(s, p, p|Qwinname, "winname", 0600, "inferno");
	ninepadddir(s, p, Qwsys, "wsys", 0500, "inferno");
}

/* Service Functions */
char*
wsysnewclient(Client *c)
{
	Window *w;

	w = newwin();
	if(w == nil)
		return Enomem;
	c->u = w;
	wsysaddfiles(server, w);
	return nil;
}

char*
wsysfreeclient(Client *c)
{
	Window *w;

	w = c->u;
	drawdettach(w);
	w->draw = nil;
	deletewin(w);
	return nil;
}

char*
wsysattach(char *uname, char *aname)
{
	Window *w;

	w = curwindow;
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

	w = curwindow;
	if(w->deleted)
		return Edeleted;
	type = QTYPE(qid->path);
	if(type == Qnew){
		cl = drawnewclient(w->draw);
		if(cl == 0)
			return Enodev;
		drawaddfiles(server, cl);
		type = Qctl;
	}
	switch(type){
	case Qmouse:
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

	w = curwindow;
	type = QTYPE(qid.path);
	if(w->deleted && type != Qkill)
		return Edeleted;
	switch(type){
	case Qcons:
		p = ninepreplylater(server);
		readkbd(w, p);
		return nil;
	case Qkill:
		w->killpend = ninepreplylater(server);
		return nil;
	case Qlabel:
		if(w->label)
			ninepreadstr(0, buf, strlen(w->label), w->label);
		return nil;
	case Qmouse:
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

	w = curwindow;
	if(w->deleted)
		return Edeleted;
	type = QTYPE(qid.path);
	count = *n;
	switch(type){
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
	return "Read called on an unreadable file";
}

char*
wsysclose(Qid qid, int mode) {
	int type;
	Window *w;

	w = curwindow;
	type = QTYPE(qid.path);
	switch(type){
	case Qcons:
		w->kbd.wi = w->kbd.ri;
		w->kbdreqs.wi = w->kbdreqs.ri;
		return nil;
	case Qmouse:
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

/* Reply a read request */
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
	wsysnewclient,	/* newclient */
	wsysfreeclient,	/* freeclient */

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

