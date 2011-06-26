#include <time.h>
#include <lib9.h>
#include <draw.h>
#include <memdraw.h>
#include <memlayer.h>
#include "dat.h"
#include "fns.h"

#include <ixp.h>
#define bool int
typedef union IxpFileIdU IxpFileIdU;
union IxpFileIdU {
	Client*	client;
	Window*	window;
	void*	ref;
};
#include <ixp_srvutil.h>

#define incref(r)	((*r) = ++(*r))
#define decref(r)	((*r) = --(*r))

/* Constants */
enum {
	/* Dirs */
	FsRoot,
	FsDDraw,
	FsDDrawn,
	FsDWsys,
	FsDWsysn,
	/* Files */
	FsFCons,
	FsFConsctl,
	FsFLabel,
	FsFMouse,
	FsFWinid,
	/* draw/ */
	FsFNew,
	/* draw/n/ */
	FsFCtl,
	FsFData,
	FsFColormap,
	FsFRefresh,
};

#define iswindow(t) ((t) == FsRoot || (t) > FsDDrawn && (t) < FsFCtl)

/* Error messages */
static char
	Ebadarg[] = "bad arg in system call",
	Edeleted[] = "window deleted",
	Einterrupted[] = "interrupted",
	Einuse[] = "file in use",
	Enodev[] = "no free devices",
	Enofile[] = "file not found",
	Enomem[] = "out of memory",
	Enoperm[] = "permission denied",
	Eshortread[] =	"draw read too short";

/* Macros */
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
	{"cons",		QTFILE,	FsFCons,		0600 },
	{"consctl",		QTFILE,	FsFConsctl,	0200 },
	{"label",		QTFILE,	FsFLabel,		0600 },
	{"mouse",		QTFILE,	FsFMouse,	0400 },
	{"winid",		QTFILE,	FsFWinid,		0400 },
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
	{"cons",		QTFILE,	FsFCons,		0600 },
	{"consctl",		QTFILE,	FsFConsctl,	0200 },
	{"label",		QTFILE,	FsFLabel,		0600 },
	{"mouse",		QTFILE,	FsFMouse,	0400 },
	{"winid",		QTFILE,	FsFWinid,		0400 },
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
	int i, id;
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
					if(!name || cl->clientid == id) {
						push_file(smprint("%d", cl->clientid), cl->clientid, 1);
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
	char *label, *winsize;
	IxpFileId *f;
	Window *w;

	debug("fs_attach(%p)\n", r);

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
	winsize = nil;
	if(!strncmp(r->ifcall.tattach.aname, "new ", 4))
		winsize = &r->ifcall.tattach.aname[4];
	if(!(w = newwin(label, winsize))) {
		ixp_respond(r, Enomem);
		return;
	}
	f->p.window = w;
	ixp_respond(r, nil);
}

void
fs_open(Ixp9Req *r) {
	IxpFileId *f;
	Window *w;
	Client *cl;
	Draw *d;

	debug("fs_open(%p)\n", r);

	f = r->fid->aux;

	if(!ixp_srv_verifyfile(f, lookup_file)) {
		ixp_respond(r, Enofile);
		return;
	}
	if(iswindow(f->tab.type)) {
		w = f->p.window;
		if(w->deleted) {
			ixp_respond(r, Edeleted);
			return;
		}
	}

	if(f->tab.type == FsFNew){
		w = f->p.window;
		cl = drawnewclient(&w->draw);
		if(cl == 0)
			ixp_respond(r, Enodev);
		f->p.client = cl;
		f->tab.type = FsFCtl;
	}
	switch(f->tab.type) {
	case FsFNew:
		break;
	case FsFCtl:
		cl = f->p.client;
		if(cl->busy){
			ixp_respond(r, Einuse);
			return;
		}
		cl->busy = 1;
		d = cl->draw;
		d->flushrect = Rect(10000, 10000, -10000, -10000);
		drawinstall(cl, 0, d->screenimage, 0);
		incref(&cl->r);
		break;
	case FsFColormap:
	case FsFData:
	case FsFRefresh:
		cl = f->p.client;
		incref(&cl->r);
		break;
	case FsFMouse:
		w = f->p.window;
		if(w->mouse.open){
			ixp_respond(r, Einuse);
			return;
		}
		w->mouse.open = 1;
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

	debug("fs_walk(%p)\n", r);

	ixp_srv_walkandclone(r, lookup_file);
}

void
fs_read(Ixp9Req *r) {
	char buf[512], *err;
	IxpFileId *f;
	Client *cl;
	Window *w;

	debug("fs_read(%p)\n", r);

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
	if(f->pending) {
		ixp_pending_respond(r);
		return;
	}
	cl = f->p.client;
	switch(f->tab.type) {
	case FsFCtl:
		if(r->ifcall.io.count < 12*12) {
			ixp_respond(r, Eshortread);
			return;
		}
		readdrawctl(buf, cl);
		err = drawerr();
		if(err != nil) {
			ixp_respond(r, err);
			return;
		}
		ixp_srv_readbuf(r, buf, strlen(buf));
		ixp_respond(r, nil);
		return;
	case FsFData:
		if(cl->readdata == nil) {
			ixp_respond(r, "no draw data");
			return;
		}
		if(r->ifcall.io.count < cl->nreaddata) {
			ixp_respond(r, Eshortread);
			return;
		}
		ixp_srv_readbuf(r, cl->readdata, cl->nreaddata);
		ixp_respond(r, nil);
		free(cl->readdata);
		cl->readdata = nil;
		return;
	case FsFColormap:
		r->ofcall.io.count = 0;
		ixp_respond(r, nil);
		return;
	case FsFRefresh:
		if(r->ifcall.io.count < 5*4) {
			ixp_respond(r, Ebadarg);
			return;
		}
		readrefresh(buf, r->ifcall.io.count, cl);
		ixp_respond(r, nil);
		return;
	}
	w = f->p.window;
	if(w->deleted){
		ixp_respond(r, Edeleted);
		return;
	}
	switch(f->tab.type) {
	case FsFCons:
		readkbd(w, r);
		return;
	case FsFLabel:
		if(w->label)
			ixp_srv_readbuf(r, w->label, strlen(w->label));
		ixp_respond(r, nil);
		return;
	case FsFMouse:
		readmouse(w, r);
		return;
	case FsFWinid:
		sprint(buf, "%11d ", w->id);
		ixp_srv_readbuf(r, buf, strlen(buf));
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

	debug("fs_stat(%p)\n", r);

	f = r->fid->aux;
	if(!ixp_srv_verifyfile(f, lookup_file)) {
		ixp_respond(r, Enofile);
		return;
	}
	if(f->tab.type > FsDDrawn && f->tab.type < FsFNew) {
		w = f->p.window;
		if(w->deleted){
			ixp_respond(r, Edeleted);
			return;
		}
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
	char *label;
	IxpFileId *f;
	Window *w;
	Client *cl;

	debug("fs_write(%p)\n", r);

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
	switch(f->tab.type) {
	case FsFData:
		r->ofcall.io.count = r->ifcall.io.count;
		drawmesg(cl, r->ifcall.twrite.data, r->ofcall.rwrite.count);
		// drawwakeall();
		ixp_respond(r, nil);
		return;
	case FsFCtl:
		r->ofcall.io.count = r->ifcall.io.count;
		if(r->ofcall.io.count != 4)
			ixp_respond(r, "unknown draw control request");
		cl->infoid = BGLONG((uchar*)r->ifcall.twrite.data);
		ixp_respond(r, nil);
		return;
	case FsFColormap:
		r->ofcall.io.count = 0;
		ixp_respond(r, nil);
		return;
	case FsFRefresh:
		return;
	}
	w = f->p.window;
	if(w->deleted){
		ixp_respond(r, Edeleted);
		return;
	}
	switch(f->tab.type) {
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
	}
	ixp_respond(r, nil);
}

void
fs_clunk(Ixp9Req *r) {
	IxpFileId *f;
	Window *w;
	Client *cl;

	debug("fs_clunk(%p)\n", r);
	f = r->fid->aux;
	if(!ixp_srv_verifyfile(f, lookup_file)) {
		ixp_respond(r, nil);
		return;
	}
	w = f->p.window;
	switch(f->tab.type) {
	case FsFCons:
		w->kbd.wi = w->kbd.ri;
		w->kbdreqs.wi = w->kbdreqs.ri;
		break;
	case FsFMouse:
		w->mouse.open = 0;
		w->mouse.wi = w->mouse.ri;
		w->mousereqs.wi = w->mousereqs.ri;
		break;
	}
	cl = f->p.client;
	if(f->tab.type == FsFCtl)
		cl->busy = 0;
	if(f->tab.type >= FsFNew && f->tab.type <= FsFColormap && (decref(&cl->r)==0))
		drawfree(cl);
	ixp_respond(r, nil);
}

void
fs_flush(Ixp9Req *r) {
	Ixp9Req *or;
	IxpFileId *f;

	or = r->oldreq;
	f = or->fid->aux;
	if(f->pending)
		ixp_pending_flush(r);
	/* else die() ? */
	ixp_respond(r->oldreq, Einterrupted);
	ixp_respond(r, nil);
}

void
fs_create(Ixp9Req *r) {
	debug("fs_create(%p)\n", r);
	ixp_respond(r, Enoperm);
}

void
fs_remove(Ixp9Req *r) {
	debug("fs_remove(%p)\n", r);
	ixp_respond(r, Enoperm);

}

void
fs_freefid(IxpFid *f) {
	IxpFileId *id, *tid;

	tid = f->aux;
	while((id = tid)) {
		if(iswindow(tid->tab.type) && tid->nref == 0)
			deletewin(tid->p.window);
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

/* Reply a read request */
void
ixprread(void *v, char *buf)
{
	Ixp9Req *r;

	r = v;
	r->ifcall.rread.offset = 0;
	ixp_srv_readbuf(r, buf, strlen(buf));
	ixp_respond(r, nil);
}
