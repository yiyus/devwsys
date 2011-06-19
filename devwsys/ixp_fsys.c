#include <time.h>
#include <lib9.h>
#include <draw.h>
#include <memdraw.h>
#include "dat.h"
#include "fns.h"

#include <ixp.h>
#define bool int
typedef union IxpFileIdU IxpFileIdU;
union IxpFileIdU {
	Window*	window;
	void*	ref;
};
#include <ixp_srvutil.h>

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

/* Error messages */
static char
	Edeleted[] = "window deleted",
	Einterrupted[] = "interrupted",
	Einuse[] = "file in use",
	Enoperm[] = "permission denied",
	Enofile[] = "file not found",
	Enomem[] = "out of memory";

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
			if(!file->tab.name) fatal("no mem");
		/* Dynamic dirs */
		if(dir->name[0] == '\0') {
			switch(parent->tab.type) {
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
	IxpFileId *f;
	Wsysmsg m;
	Window *w = nil;

	debug("fs_attach(%p)\n", r);

	f = ixp_srv_getfile();
	f->tab = dirtab[FsRoot][0];
	f->tab.name = strdup("/");
	if(!f->tab.name)
		ixp_respond(r, Enomem);
	r->fid->aux = f;
	r->fid->qid.type = f->tab.qtype;
	r->fid->qid.path = QID(f->tab.type, 0);
	r->ofcall.rattach.qid = r->fid->qid;
	m.type = Tinit;
	m.label = nil; /* pjw face */
	m.winsize = nil;
	m.v = r;
	runmsg(w, &m);
}

void
fs_open(Ixp9Req *r) {
	IxpFileId *f;
	Window *w;

	debug("fs_open(%p)\n", r);

	f = r->fid->aux;

	if(!ixp_srv_verifyfile(f, lookup_file)) {
		ixp_respond(r, Enofile);
		return;
	}

	switch(f->tab.type) {
	case FsFMouse:
		w = f->p.window;
		if(w->mouseopen){
			ixp_respond(r, Einuse);
			return;
		}
		w->mouseopen = 1;
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
	char buf[512];
	IxpFileId *f;
	Window *w;
	Wsysmsg m;

	debug("fs_read(%p)\n", r);

	f = r->fid->aux;
	if(!ixp_srv_verifyfile(f, lookup_file)) {
		ixp_respond(r, Enofile);
		return;
	}
	w = f->p.window;
	if(w->deleted){
		ixp_respond(r, Edeleted);
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
	else{
		if(f->pending) {
			ixp_pending_respond(r);
			return;
		}
		switch(f->tab.type) {
		case FsFCons:
			m.type = Trdkbd;
			m.v = r;
			runmsg(w, &m);
			return;
		case FsFLabel:
			if(w->label)
				ixp_srv_readbuf(r, w->label, strlen(w->label));
			ixp_respond(r, nil);
			return;
		case FsFMouse:
			m.type = Trdmouse;
			m.v = r;
			runmsg(w, &m);
			return;
		case FsFWinid:
			sprint(buf, "%11d ", w->id);
			ixp_srv_readbuf(r, buf, strlen(buf));
			ixp_respond(r, nil);
			return;
		}
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
	w = f->p.window;
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
	IxpFileId *f;
	Window *w;
	Wsysmsg m;

	debug("fs_write(%p)\n", r);

	f = r->fid->aux;
	if(!ixp_srv_verifyfile(f, lookup_file)) {
		ixp_respond(r, Enofile);
		return;
	}
	w = f->p.window;
	if(w->deleted){
		ixp_respond(r, Edeleted);
		return;
	}
	if(!f->tab.perm & 0200) {
		ixp_respond(r, Enoperm);
		return;
	}

	switch(f->tab.type) {
	case FsFLabel:
		m.type = Tlabel;
		r->ofcall.io.count = r->ifcall.io.count;
		if(!(m.label = malloc(r->ifcall.twrite.count + 1))) {
			r->ofcall.rwrite.count = 0;
			ixp_respond(r, Enomem);
			return;
		}
		memcpy(m.label, r->ifcall.twrite.data, r->ofcall.rwrite.count);
		m.label[r->ifcall.twrite.count] = 0;
		runmsg(w, &m);
		break;
	}
	ixp_respond(r, nil);
}

void
fs_clunk(Ixp9Req *r) {
	IxpFileId *f;
	Window *w;

	f = r->fid->aux;
	if(!ixp_srv_verifyfile(f, lookup_file)) {
		ixp_respond(r, nil);
		return;
	}
	w = f->p.window;

	switch(f->tab.type) {
	case FsFMouse:
		w->mouseopen = 0;
		w->mouse.wi = w->mouse.ri;
		w->mousetags.wi = w->mousetags.ri;
		break;
	}
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
ixpreply(Window *w, Wsysmsg *m)
{
	IxpFileId *f;
	int c;
	uchar buf[65536];
	Ixp9Req *r;
	Mouse ms;
	Rune rune;

	r = m->v;
	switch(m->type){
	case Rinit:
		if(w == nil) {
			ixp_respond(r, Enomem);
			return;
		}
		f = r->fid->aux;
		f->p.window = w;
		ixp_respond(r, NULL);
		return;

	case Rrdkbd:
		rune = m->rune;
		sprint(buf, "%C", rune);
		r->ifcall.rread.offset = 0;
		break;

	case Rrdmouse:
		ms = m->mouse;
		c = 'm';
		if(m->resized)
			c = 'r';
		sprint(buf, "%c%11d %11d %11d %11ld ", c, ms.xy.x, ms.xy.y, ms.buttons, ms.msec);
		r->ifcall.rread.offset %= strlen(buf);
		break;

	default:
		return;
	}
	ixp_srv_readbuf(r, buf, strlen(buf));
	ixp_respond(r, nil);
}
