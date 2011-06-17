/* 2011 JGL (yiyus) */
#include <lib9.h>
#include <draw.h>
#include <memdraw.h>

#include <ixp.h>

#include "keyboard.h"
#include "mouse.h"
#include "devwsys.h"
#include "drawfcall.h"

/* Error messages */
static char
	Edeleted[] = "window deleted",
	Einuse[] = "file in use",
	Enoperm[] = "permission denied",
	Enofile[] = "file not found",
	Enomem[] = "out of memory";

/* name, parent, type, mode, size */
Fileinfo file[QMAX] = {
	{"", QNONE, P9_QTDIR, 0500|P9_DMDIR, 0},
	{"cons", QROOT, P9_QTFILE, 0600, 0},
	{"consctl", QROOT, P9_QTFILE, 0600, 0},
	{"label", QROOT, P9_QTFILE, 0600, 0},
	{"mouse", QROOT, P9_QTFILE, 0400, 0},
	{"winid", QROOT, P9_QTFILE, 0400, 0},
};

const char* rread(Ixp9Req*, char*);

void
fs_attach(Ixp9Req *r)
{
	Wsysmsg m;
	Window *w = nil;

	debug("fs_attach(%p)\n", r);

	r->fid->qid.type = file[QROOT].type;
	r->fid->qid.path = QROOT;
	r->ofcall.rattach.qid = r->fid->qid;
	m.type = Tinit;
	m.label = nil; // pjw face
	m.winsize = nil; // full screen
	m.v = r;
	if(!(w = newwin())) {
		return;
	}
	runmsg(w, &m);
}

void
fs_walk(Ixp9Req *r)
{
	qpath cwd;
	int i, j;
	
	debug("fs_walk(%p)\n", r);

	cwd = r->fid->qid.path;
	r->ofcall.rwalk.nwqid = 0;
	for(i = 0; i < r->ifcall.twalk.nwname; ++i){
		for(j = 0; j < QMAX; ++j){
			if(file[j].parent == cwd && strcmp(file[j].name, r->ifcall.twalk.wname[i]) == 0)
				break;
		}
		if(j >= QMAX){
			ixp_respond(r, Enofile);
			return;
		}
		r->ofcall.rwalk.wqid[r->ofcall.rwalk.nwqid].type = file[j].type;
		r->ofcall.rwalk.wqid[r->ofcall.rwalk.nwqid].path = j;
		r->ofcall.rwalk.wqid[r->ofcall.rwalk.nwqid].version = 0;
		++r->ofcall.rwalk.nwqid;
	}
	r->newfid->aux = r->fid->aux;
	ixp_respond(r, NULL);
}

void
fs_open(Ixp9Req *r)
{
	Window *w;

	debug("fs_open(%p)\n", r);

	switch(r->fid->qid.path){
		case QMOUSE: {
			w = r->fid->aux;
			if(w->mouseopen){
				ixp_respond(r, Einuse);
				return;
			}
			w->mouseopen = 1;
			break;
		}
	}

	ixp_respond(r, NULL);
}

void
fs_clunk(Ixp9Req *r)
{
	Window *w;

	debug("fs_clunk(%p)\n", r);
	
	switch(r->fid->qid.path){
		case QMOUSE: {
			w = r->fid->aux;
			w->mouseopen = 0;
			w->mouse.wi = w->mouse.ri;
			w->mousetags.wi = w->mousetags.ri;
			break;
		}
	}
	ixp_respond(r, NULL);
}

void
dostat(IxpStat *st, int path)
{
	st->type = file[path].type;
	st->qid.type = file[path].type;
	st->qid.path = path;
	st->mode = file[path].mode;
	st->name = file[path].name;
	st->length = 0;
	st->uid = st->gid = st->muid = "";
}

void
fs_stat(Ixp9Req *r)
{
	IxpStat st = {0};
	IxpMsg m;
	char buf[512];

	debug("fs_stat(%p)\n", r);

	dostat(&st, r->fid->qid.path);
	st.length = 0;
	m = ixp_message(buf, sizeof(buf), MsgPack);
	ixp_pstat(&m, &st);
	r->ofcall.rstat.nstat = ixp_sizeof_stat(&st);
	if(!(r->ofcall.rstat.stat = malloc(r->ofcall.rstat.nstat))) {
		r->ofcall.rstat.nstat = 0;
		ixp_respond(r, Enomem);
		return;
	}
	memcpy(r->ofcall.rstat.stat, m.data, r->ofcall.rstat.nstat);
	ixp_respond(r, NULL);
}

void
fs_read(Ixp9Req *r)
{
	char buf[512];
	Window *w;
	Wsysmsg m;

	debug("fs_read(%p)\n", r);

	w = r->fid->aux;
	if(w->deleted){
		ixp_respond(r, Edeleted);
		return;
	}

	if(file[r->fid->qid.path].type & P9_QTDIR){
		IxpStat st = {0};
		IxpMsg m;
		int i;

		m = ixp_message(buf, sizeof(buf), MsgPack);

		r->ofcall.rread.count = 0;
		if(r->ifcall.tread.offset > 0) {
			/* hack! assuming the whole directory fits in a single Rread */
			ixp_respond(r, NULL);
			return;
		}
		for(i = 0; i < QMAX; ++i){
			if(file[i].parent == r->fid->qid.path){
				dostat(&st, i);
				ixp_pstat(&m, &st);
				r->ofcall.rread.count += ixp_sizeof_stat(&st);
			}
		}
		if(!(r->ofcall.rread.data = malloc(r->ofcall.rread.count))) {
			r->ofcall.rread.count = 0;
			ixp_respond(r, Enomem);
			return;
		}
		memcpy(r->ofcall.rread.data, m.data, r->ofcall.rread.count);
		ixp_respond(r, NULL);
		return;
	}

	switch(r->fid->qid.path){
		case QCONS: {
			m.type = Trdkbd;
			m.v = r;
			runmsg(w, &m);
			return;
		}
		case QLABEL: {
			ixp_respond(r, rread(r, w->label));
			return;
		}
		case QMOUSE: {
			m.type = Trdmouse;
			m.v = r;
			runmsg(w, &m);
			return;
		}
		case QWINID: {
			sprint(buf, "%11d ", w->id);
			ixp_respond(r, rread(r, buf));
			return;
		}
	}
	ixp_respond(r, nil);
}

void
fs_write(Ixp9Req *r)
{
	Window *w;

	debug("fs_write(%p)\n", r);

	w = r->fid->aux;
	if(w->deleted){
		ixp_respond(r, Edeleted);
		return;
	}

	switch(r->fid->qid.path){
		case QLABEL: {
			r->ofcall.rwrite.count = r->ifcall.twrite.count;
			if(!(w->label = realloc(w->label, r->ifcall.twrite.count + 1))) {
				r->ofcall.rwrite.count = 0;
				ixp_respond(r, Enomem);
				return;
			}
			memcpy(w->label, r->ifcall.twrite.data, r->ofcall.rwrite.count);
			w->label[r->ifcall.twrite.count] = 0;
			break;
		}
	}
	ixp_respond(r, NULL);
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
fs_wstat(Ixp9Req *r)
{
	debug("fs_wstat(%p)\n", r);
	ixp_respond(r, Enoperm);
}

Ixp9Srv p9srv = {
	.attach=fs_attach,
	.open=fs_open,
	.clunk=fs_clunk,
	.walk=fs_walk,
	.read=fs_read,
	.stat=fs_stat,
	.write=fs_write,
	.create=fs_create,
	.remove=fs_remove,
	.wstat=fs_wstat,
};

void
fs_reply(Window *w, Wsysmsg *m)
{
	const char *error;
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
		r->fid->aux = w;
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
		// w->resized = 0;
		r->ifcall.rread.offset %= strlen(buf);
		break;

	default:
		return;
	}
	if(strlen(buf) == 0)
		return;
	error = rread(r, buf);
	ixp_respond(r, error);
}

const char*
rread(Ixp9Req *r, char *s)
{
	int n = 0;

	if(s != nil)
		n = strlen(s);
	if(n > 0 && r->ifcall.tread.offset < n) {
		if(r->ifcall.tread.offset + r->ifcall.tread.count > n) {
			r->ofcall.rread.count = n - r->ifcall.tread.offset;
		} else {
			r->ofcall.rread.count = r->ifcall.tread.count;
		}
		if(!(r->ofcall.rread.data = malloc(r->ofcall.rread.count))) {
			r->ofcall.rread.count = 0;
			return Enomem;
		}
		memcpy(r->ofcall.rread.data, s+r->ifcall.tread.offset, r->ofcall.rread.count);
	}
	return nil;
}
