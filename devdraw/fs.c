/* 2011 JGL (yiyus) */
#include <lib9.h>
#include <draw.h>
#include <memdraw.h>

#include <ixp.h>

#include "devdraw.h"
#include "drawfcall.h"

/* Error messages */
static char
	Edeleted[] = "window deleted",
	Enoperm[] = "permission denied",
	Enofile[] = "file not found",
	Enomem[] = "out of memory";

/* name, parent, type, mode, size */
Fileinfo file[QMAX] = {
	{"", QNONE, P9_QTDIR, 0500|P9_DMDIR, 0},
	{"label", QROOT, P9_QTFILE, 0600, 0},
	{"winid", QROOT, P9_QTFILE, 0400, 0},
};

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
	if(!(w = newwin())) {
		return;
	}
	runmsg(w, &m);
	if(w == nil) {
		ixp_respond(r, Enomem);
		return;
	}
	r->fid->aux = w;
	ixp_respond(r, NULL);
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
	debug("fs_open(%p)\n", r);
	
	ixp_respond(r, NULL);
}

void
fs_clunk(Ixp9Req *r)
{
	debug("fs_clunk(%p)\n", r);
	
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
	Window *win;
	int n = 0;

	debug("fs_read(%p)\n", r);

	win = r->fid->aux;
	if(win->deleted){
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
		case QIDENT: {
			sprint(buf, "%11d ", win->id);
			if(r->ifcall.tread.offset <  11) {
				n = strlen(buf);
				if(r->ifcall.tread.offset + r->ifcall.tread.count >  11) {
					r->ofcall.rread.count =  11 - r->ifcall.tread.offset;
				} else {
					r->ofcall.rread.count = r->ifcall.tread.count;
				}
				if(!(r->ofcall.rread.data = malloc(r->ofcall.rread.count))) {
					r->ofcall.rread.count = 0;
					ixp_respond(r, Enomem);
					return;
				}
				memcpy(r->ofcall.rread.data, buf, r->ofcall.rread.count);
			}
			break;
		}
		case QLABEL: {
			if(win->label != NULL)
				n = strlen(win->label);
			if(n > 0 && r->ifcall.tread.offset < n) {
				if(r->ifcall.tread.offset + r->ifcall.tread.count > n) {
					r->ofcall.rread.count = n - r->ifcall.tread.offset;
				} else {
					r->ofcall.rread.count = r->ifcall.tread.count;
				}
				if(!(r->ofcall.rread.data = malloc(r->ofcall.rread.count))) {
					r->ofcall.rread.count = 0;
					ixp_respond(r, Enomem);
					return;
				}
				memcpy(r->ofcall.rread.data, win->label+r->ifcall.tread.offset, r->ofcall.rread.count);
			}
			break;
		}
	}

	ixp_respond(r, NULL);
}

void
fs_write(Ixp9Req *r)
{
	Window *win;

	debug("fs_write(%p)\n", r);

	win = r->fid->aux;
	if(win->deleted){
		ixp_respond(r, Edeleted);
		return;
	}

	switch(r->fid->qid.path){
		case QLABEL: {
			r->ofcall.rwrite.count = r->ifcall.twrite.count;
			if(!(win->label = realloc(win->label, r->ifcall.twrite.count + 1))) {
				r->ofcall.rwrite.count = 0;
				ixp_respond(r, Enomem);
				return;
			}
			memcpy(win->label, r->ifcall.twrite.data, r->ofcall.rwrite.count);
			win->label[r->ifcall.twrite.count] = 0;
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
