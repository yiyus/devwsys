#include <time.h>
#include <lib9.h>
#include <draw.h>
#include <cursor.h>
#include "dat.h"
#include "fns.h"
#include "fsys.h"

#include <ixp.h>
#define bool int
typedef union IxpFileIdU IxpFileIdU;
union IxpFileIdU {
	Client*	client;
	Window*	window;
	void*	ref;
};
#include <ixp_srvutil.h>

/* Error messages */
const char
	*Ebadarg = "bad arg in system call",
	*Edeleted = "window deleted",
	*Einterrupted = "interrupted",
	*Einuse = "file in use",
	*Enodev = "no free devices",
	*Enofile = "file not found",
	*Enodrawimage = "unknown id for draw image",
	*Enomem = "out of memory",
	*Enoperm = "permission denied",
	*Eshortread =	"draw read too short",
	*Eshortwrite =	"draw write too short";

/* Macros */
#define debug9p(...) if(1) debug(__VA_ARGS__)
#define QID(t, i) (((vlong)((t)&0xFF)<<32)|((i)&0xFFFFFFFF))

/* Global Vars */

/* ad-hoc file tree. Empty names ("") indicate dynamic entries to be filled
 * in by lookup_file
 */
static IxpDirtab
	/* name		qtype	type			perm */
dirtab_root[] = {
	{".",			QTDIR,	FsRoot,		0500|DMDIR },
	{"draw",		QTDIR,	FsDDraw,		0500|DMDIR },
	{"wsys",		QTDIR,	FsDWsys,		0500|DMDIR },
	{"keyboard",	QTFILE,	FsFCons,		0600 },
	{"cons",		QTFILE,	FsFCons,		0600 },
	{"consctl",		QTFILE,	FsFConsctl,	0200 },
	{"cursor",		QTFILE,	FsFCursor,	0600 },
	{"pointer",		QTFILE,	FsFMouse,	0600 },
	{"mouse",		QTFILE,	FsFMouse,	0600 },
	{"snarf",		QTFILE,	FsFSnarf,		0600 },
	{"kill",		QTFILE,	FsFKill,		0400 },
	{"label",		QTFILE,	FsFLabel,		0600 },
	{"wctl",		QTFILE,	FsFWctl,		0600 },
	{"winid",		QTFILE,	FsFWinid,		0400 },
	{"winname",	QTFILE,	FsFWinname,	0400 },
	{nil}
},
dirtab_draw[] = {
	{".",			QTDIR,	FsDDraw,		0500|DMDIR },
	{"",			QTDIR,	FsDDrawn,	0500|DMDIR },
	{"new",		QTFILE,	FsFNew,		0600 },
	{nil}
},
dirtab_drawn[] = {
	{".",			QTDIR,	FsDDrawn,	0500|DMDIR },
	{"ctl",		QTFILE,	FsFCtl,		0600 },
	{"data",		QTFILE,	FsFData,		0600 },
	{"colormap",	QTFILE,	FsFColormap,	0400 },
	{"refresh",		QTFILE,	FsFRefresh,	0400 },
	{nil}
},
dirtab_wsys[] = {
	{".",			QTDIR,	FsDWsys,		0500|DMDIR },
	{"",			QTDIR,	FsDWsysn,	0500|DMDIR },
	{nil}
},
dirtab_wsysn[] = {
	{".",			QTDIR,	FsDWsysn,	0500|DMDIR },
	{"wsys",		QTDIR,	FsDWsys,		0500|DMDIR },
	{"keyboard",	QTFILE,	FsFCons,		0600 },
	{"cons",		QTFILE,	FsFCons,		0600 },
	{"consctl",		QTFILE,	FsFConsctl,	0200 },
	{"cursor",		QTFILE,	FsFCursor,	0600 },
	{"pointer",		QTFILE,	FsFMouse,	0600 },
	{"mouse",		QTFILE,	FsFMouse,	0600 },
	{"snarf",		QTFILE,	FsFSnarf,		0600 },
	{"kill",		QTFILE,	FsFKill,		0400 },
	{"label",		QTFILE,	FsFLabel,		0600 },
	{"wctl",		QTFILE,	FsFWctl,		0600 },
	{"winid",		QTFILE,	FsFWinid,		0400 },
	{"winname",	QTFILE,	FsFWinname,	0400 },
	{nil}
};

static IxpDirtab* dirtab[] = {
	[FsRoot] = dirtab_root,
	[FsDDraw] = dirtab_draw,
	[FsDDrawn] = dirtab_drawn,
	[FsDWsys] = dirtab_wsys,
	[FsDWsysn] = dirtab_wsysn,
};

typedef char* (*MsgFunc)(void*, IxpMsg*);
typedef char* (*BufFunc)(void*);

static char *snarfbuf;

static void
dostat(IxpStat *s, IxpFileId *f) {
	s->type = 0;
	s->dev = 0;
	s->qid.path = QID(f->tab.type, f->id);
	s->qid.version = 0;
	s->qid.type = f->tab.qtype;
	s->mode = f->tab.perm;
	s->mtime = time(nil);
	s->atime = s->mtime;
	s->length = 0;
	s->name = f->tab.name;
	s->uid = s->gid = s->muid = "";
}

/*
 * All lookups and directory organization should be performed through
 * lookup_file, mostly through the dirtab[] tree.
 */
static IxpFileId*
lookup_file(IxpFileId *parent, char *name)
{
	int i, id, clientid;
	IxpFileId *ret, *file, **last;
	IxpDirtab *dir;
	Client *cl;
	Window *w;

	if(!(parent->tab.perm & DMDIR))
		return nil;
	dir = dirtab[parent->tab.type];
	last = &ret;
	ret = nil;
	for(; dir->name; dir++) {
#               define push_file(nam, id_, vol) \
			file = ixp_srv_getfile(); \
			file->id = id_; \
			file->volatil = vol; \
			*last = file; \
			last = &file->next; \
			file->tab = *dir; \
			file->tab.name = nam; \
			assert(file->tab.name);
		/* Dynamic dirs */
		if(dir->name[0] == '\0') {
			switch(parent->tab.type) {
			case FsDDraw:
				id = 0;
				if(name) {
					id = (int)strtol(name, &name, 10);
					if(*name)
						continue;
				}
				for(i = 0; i < nclient; i++) {
					if(!(cl = client[i]))
						continue;
					clientid = drawclientid(cl);
					if(!name || clientid == id) {
						push_file(smprint("%d", clientid), clientid, 1);
						file->p.client = cl;
						file->index = id;
						if(name)
							goto LastItem;
					}
				}
				break;
			case FsDWsys:
				id = 0;
				if(name) {
					id = (int)strtol(name, &name, 10);
					if(*name)
						continue;
				}
				for(i = 0; i < nwindow; i++) {
					w = window[i];
					if(!name || w->id == id) {
						push_file(smprint("%d", w->id), w->id, 1);
						file->p.window = w;
						file->index = id;
						if(name)
							goto LastItem;
					}
				}
				break;
			}
		}else /* Static dirs */
		if(!name || name && !strcmp(name, dir->name)) {
			push_file(strdup(file->tab.name), 0, 0);
			file->p.ref = parent->p.ref;
			file->index = parent->index;
			if(name)
				goto LastItem;
		}
#		undef push_file
	}
LastItem:
	*last = nil;
	return ret;
}

/* Service Functions */
void
fs_attach(Ixp9Req *r) {
	char *label;
	IxpFileId *f;
	Window *w;

	debug9p("fs_attach(%p)\n", r);

	f = ixp_srv_getfile();
	f->tab = dirtab[FsRoot][0];
	f->tab.name = strdup("/");
	if(!f->tab.name)
		ixp_respond(r, Enomem);
	f->nref = 0;
	r->fid->aux = f;
	r->fid->qid.type = f->tab.qtype;
	r->fid->qid.path = QID(f->tab.type, 0);
	r->ofcall.rattach.qid = r->fid->qid;
	label = nil; /* pjw face */
	if(!(w = newwin(label)) || !drawattach(w, r->ifcall.tattach.aname)) {
		if(w){
			assert(w == window[nwindow-1]);
			free(w);
			nwindow--;
		}
		ixp_respond(r, Enomem);
		return;
	}
	f->p.window = w;
	ixp_respond(r, nil);
}

void
fs_open(Ixp9Req *r) {
	const char *err;
	IxpFileId *f;
	Window *w;
	Client *cl;

	debug9p("fs_open(%p)\n", r);

	f = r->fid->aux;
	if(!ixp_srv_verifyfile(f, lookup_file)) {
		ixp_respond(r, Enofile);
		return;
	}
	w = f->p.window;
	if(iswindow(f->tab.type)) {
		if(w->deleted) {
			ixp_respond(r, Edeleted);
			return;
		}
		if(f->tab.type == FsFNew){
			cl = drawnewclient(w->draw);
			if(cl == 0) {
				ixp_respond(r, Enodev);
				return;
			}
			f->p.client = cl;
			f->tab.type = FsFCtl;
		}
	}
	if(!iswindow(f->tab.type)){
		cl = f->p.client;
		err = drawopen(cl, f->tab.type);
		if(err != nil){
			ixp_respond(r, err);
			return;
		}
	}
	w = f->p.window;
	switch(f->tab.type) {
	case FsFNew:
		break;
	case FsFMouse:
		if(w->mouse.open){
			ixp_respond(r, Einuse);
			return;
		}
		w->mouse.open = 1;
		break;
	case FsFSnarf:
		if((r->ifcall.topen.mode&3) == ORDWR){
			/* one at a time please */
			ixp_respond(r, Enoperm);
			return;
		}
		break;
	}

	if((r->ifcall.topen.mode&3) == OEXEC
	|| (r->ifcall.topen.mode&3) != OREAD && !(f->tab.perm & 0200)
	|| (r->ifcall.topen.mode&3) != OWRITE && !(f->tab.perm & 0400)
	|| (r->ifcall.topen.mode & ~(3|OTRUNC)))
		ixp_respond(r, Enoperm);
	else
		ixp_respond(r, nil);
}

void
fs_walk(Ixp9Req *r) {

	debug9p("fs_walk(%p)\n", r);

	ixp_srv_walkandclone(r, lookup_file);
}

void
fs_read(Ixp9Req *r) {
	char buf[512], *s;
	IxpFileId *f;
	Client *cl;
	Window *w;
	IOResponse rread;

	debug9p("fs_read(%p)\n", r);

	f = r->fid->aux;
	if(!ixp_srv_verifyfile(f, lookup_file)) {
		ixp_respond(r, Enofile);
		return;
	}
	if(!f->tab.perm & 0400) {
		ixp_respond(r, Enoperm);
		return;
	}
	if(f->tab.perm & DMDIR) {
		ixp_srv_readdir(r, lookup_file, dostat);
		return;
	}
	cl = f->p.client;
	w = f->p.window;
	if(!iswindow(f->tab.type))
		w = drawwindow(cl);
	if(w->deleted && f->tab.type != FsFKill){
		ixp_respond(r, Edeleted);
		return;
	}
	if(!iswindow(f->tab.type)) {
		rread = drawread(cl, f->tab.type, buf, r->ifcall.io.count);
		if(rread.err != nil){
			ixp_respond(r, rread.err);
			return;
		}
		if(f->tab.type == FsFRefresh && rread.count == 0)
			return;
		if(f->tab.type == FsFCtl || f->tab.type == FsFData || f->tab.type == FsFRefresh)
			ixp_srv_readbuf(r, rread.data, rread.count);
		if(f->tab.type == FsFData)
			drawread(cl, FsFData, nil, -1); /* clear cl->readdata */
		ixp_respond(r, nil);
		return;
	}
	switch(f->tab.type) {
	case FsFCons:
		readkbd(w, r);
		return;
	case FsFKill:
		w->killr = r;
		if(w->deleted){
			sprint(buf, "%d", w->pid);
			ixp_srv_readbuf(r, buf, strlen(buf));
			ixp_respond(r, nil);
		}
		return;
	case FsFLabel:
		if(w->label)
			ixp_srv_readbuf(r, w->label, strlen(w->label));
		ixp_respond(r, nil);
		return;
	case FsFMouse:
		readmouse(w, r);
		return;
	case FsFSnarf:
		s= xgetsnarf(w);
		ixp_srv_readbuf(r, s, strlen(s));
		ixp_respond(r, nil);
		return;
	case FsFWctl:
		// XXX TODO: current
		s = w-> visible ? "visible" : "hidden";
		sprint(buf, "%11d %11d %11d %11d %s %s ", w->orig.x +  w->screenr.min.x, w->orig.y + w->screenr.min.y, w->screenr.max.x, w->screenr.max.y, s, "current");
		ixp_srv_readbuf(r, buf, strlen(buf));
		ixp_respond(r, nil);
		return;
	case FsFWinid:
		sprint(buf, "%11d ", w->id);
		ixp_srv_readbuf(r, buf, strlen(buf));
		ixp_respond(r, nil);
		return;
	case FsFWinname:
		ixp_srv_readbuf(r, w->name, strlen(w->name));
		ixp_respond(r, nil);
		return;
	}
	/* This should not be called if the file is not open for reading. */
	fatal("Read called on an unreadable file");
}

void
fs_stat(Ixp9Req *r) {
	IxpMsg m;
	IxpStat s;
	int size;
	char *buf;
	IxpFileId *f;
	Window *w;
	Client *cl;

	debug9p("fs_stat(%p)\n", r);

	f = r->fid->aux;
	if(!ixp_srv_verifyfile(f, lookup_file)) {
		ixp_respond(r, Enofile);
		return;
	}
	cl = f->p.client;
	w = f->p.window;
	if(!iswindow(f->tab.type))
		w = drawwindow(cl);
	if(w->deleted){
		ixp_respond(r, Edeleted);
		return;
	}

	dostat(&s, f);
	size = ixp_sizeof_stat(&s);
	r->ofcall.rstat.nstat = size;
	buf = malloc(size);
	if(!buf)
		ixp_respond(r, Enomem);

	m = ixp_message(buf, size, MsgPack);
	ixp_pstat(&m, &s);

	r->ofcall.rstat.stat = (uchar*)m.data;
	ixp_respond(r, nil);
}

void
fs_write(Ixp9Req *r) {
	char *label, *p;
	char err[256];
	IxpFileId *f;
	Window *w;
	Client *cl;
	IOResponse rwrite;
	Point pt;

	debug9p("fs_write(%p)\n", r);

	f = r->fid->aux;
	if(!ixp_srv_verifyfile(f, lookup_file)) {
		ixp_respond(r, Enofile);
		return;
	}
	if(!f->tab.perm & 0200) {
		ixp_respond(r, Enoperm);
		return;
	}
	cl = f->p.client;
	w = f->p.window;
	if(!iswindow(f->tab.type))
		w = drawwindow(cl);
	if(w->deleted){
		ixp_respond(r, Edeleted);
		return;
	}

	if(!iswindow(f->tab.type)) {
		if(f->tab.type == FsFRefresh)
			return;
		rwrite = drawwrite(cl, f->tab.type, r->ifcall.twrite.data, r->ifcall.io.count);
		r->ofcall.rwrite.count = rwrite.count;
		if(rwrite.err != nil){
			ixp_respond(r, rwrite.err);
			return;
		}
		ixp_respond(r, nil);
		return;
	}
	switch(f->tab.type) {
	case FsFCursor:
		r->ofcall.io.count = r->ifcall.io.count;
		if(cursorwrite(w, r->ifcall.twrite.data, r->ofcall.rwrite.count) < 0){
			r->ofcall.rwrite.count = 0;
			ixp_respond(r, "error"); // XXX TODO
			return;
		}
		break;
	case FsFLabel:
		r->ofcall.io.count = r->ifcall.io.count;
		if(!(label = malloc(r->ifcall.twrite.count + 1))) {
			r->ofcall.rwrite.count = 0;
			ixp_respond(r, Enomem);
			return;
		}
		memcpy(label, r->ifcall.twrite.data, r->ofcall.rwrite.count);
		label[r->ifcall.twrite.count] = 0;
		setlabel(w, label);
		break;
	case FsFMouse:
		r->ofcall.io.count = r->ifcall.io.count;
		if(r->ifcall.io.count == 0){
			ixp_respond(r, Eshortwrite);
			return;
		}
		p = &r->ifcall.twrite.data[1];
		pt.x = strtoul(p, &p, 0);
		if(p == 0){
			ixp_respond(r, Eshortwrite);
			return;
		}
		pt.y = strtoul(p, 0, 0);
		xsetmouse(w, pt);
		break;
	case FsFSnarf:
		if(r->ifcall.twrite.offset+r->ifcall.twrite.count >= SnarfSize){
			ixp_respond(r, "too much snarf");
			return;
		}
		if(r->ifcall.twrite.count == 0){
			r->ofcall.io.count = r->ifcall.io.count;
			break;
		}
		if(snarfbuf == nil)
			r->ofcall.io.count = 0;
		else
			r->ofcall.io.count = strlen(snarfbuf);
		if(r->ifcall.twrite.offset+r->ifcall.twrite.count > r->ofcall.io.count){
			r->ofcall.io.count = r->ifcall.twrite.offset+r->ifcall.twrite.count;
			p = malloc(r->ofcall.io.count+1);
			if(p == nil){
				ixp_respond(r, Enomem);
				return;
			}
			if(snarfbuf){
				strcpy(p, snarfbuf);
				free(snarfbuf);
			}
			snarfbuf = p;
		}
		memmove(snarfbuf+r->ifcall.twrite.offset, r->ifcall.twrite.data, r->ifcall.twrite.count);
		snarfbuf[r->ifcall.twrite.offset+r->ifcall.twrite.count] = '\0';
		break;
	case FsFWctl:
 		r->ofcall.io.count = r->ifcall.io.count;
		wctlmesg(w, r->ifcall.twrite.data, r->ofcall.rwrite.count, err);
		break;
	}
	ixp_respond(r, nil);
}

void
fs_clunk(Ixp9Req *r) {
	IxpFileId *f;
	Window *w;
	Client *cl;

	debug9p("fs_clunk(%p)\n", r);
	f = r->fid->aux;
	if(!ixp_srv_verifyfile(f, lookup_file)) {
		ixp_respond(r, nil);
		return;
	}

	cl = f->p.client;
	w = f->p.window;
	if(!iswindow(f->tab.type))
		w = drawwindow(cl);

	// TODO: ../../inferno-os/emu/port/devdraw.c:971
	switch(f->tab.type){
	case FsFCons:
		w->kbd.wi = w->kbd.ri;
		w->kbdreqs.wi = w->kbdreqs.ri;
		break;
	case FsFMouse:
		w->mouse.open = 0;
		w->mouse.wi = w->mouse.ri;
		w->mousereqs.wi = w->mousereqs.ri;
		break;
	case FsFSnarf:
		if(r->fid->omode == OWRITE){
			if(snarfbuf)
				xputsnarf(snarfbuf);
			else
				xputsnarf("");
		}
		free(snarfbuf);
		snarfbuf = nil;

	}
	if(!iswindow(f->tab.type))
		drawclose(cl, f->tab.type);
	ixp_respond(r, nil);
}

void
fs_flush(Ixp9Req *r) {
	Ixp9Req *or;
	IxpFileId *f;

	or = r->oldreq;
	f = or->fid->aux;
	ixp_respond(r->oldreq, Einterrupted);
	ixp_respond(r, nil);
}

void
fs_create(Ixp9Req *r) {
	debug9p("fs_create(%p)\n", r);
	ixp_respond(r, Enoperm);
}

void
fs_remove(Ixp9Req *r) {
	debug9p("fs_remove(%p)\n", r);
	ixp_respond(r, Enoperm);

}

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

/* Ixp Server */
Ixp9Srv p9srv = {
	.attach=	fs_attach,
	.open=	fs_open,
	.walk=	fs_walk,
	.read=	fs_read,
	.stat=	fs_stat,
	.write=	fs_write,
	.clunk=	fs_clunk,
	.flush=	fs_flush,
	.create=	fs_create,
	.remove=	fs_remove,
	.freefid=	fs_freefid
};

void
killrespond(Window *w)
{
	fs_read(w->killr);
}

/* Reply a read request */
void
readreply(void *v, char *buf)
{
	Ixp9Req *r;
	IxpFileId *f;
	Client *cl;
	Window *w;

	r = v;
	f = r->fid->aux;
	// print("XXX ixprread %s\n", f->tab.name);
	cl = f->p.client;
	w = f->p.window;
	if(!iswindow(f->tab.type))
		w = drawwindow(cl);
	if(w->deleted){
		ixp_respond(r, Edeleted);
		return;
	}
	r->ifcall.rread.offset = 0;
	ixp_srv_readbuf(r, buf, strlen(buf));
	ixp_respond(r, nil);
}

