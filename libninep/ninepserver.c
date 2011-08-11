#include <lib9.h>
#include "ninep.h"
#include "ninepserver.h"
#include "ninepaux.h"

#define MAXSTAT	512
#define EMSGLEN			256		/* %r */

#define HASHSZ	32	/* power of 2 */

static	unsigned long		boottime;
static	char*	eve = "inferno";
static	int		Debug = 0;

char Enomem[] =	"out of memory";
char Eperm[] =		"permission denied";
char Enodev[] =		"no free devices";
char Ehungup[] =	"write to hungup channel";
char	Eexist[] =		"file exists";
char Enonexist[] =	"file does not exist";
char Ebadcmd[] =	"bad command";
char Ebadarg[] =	"bad arg in system call";
char Enofid[] =		"no such fid";
char Enotdir[] =		"not a directory";
char	Eopen[] =		"already open";
char	Ebadfid[] =	"bad fid";

#define OP(x, arg...) (ops && ops->x && (f->ename = ops->x(arg)) == nil)

enum{
	CDISC = 01,
	CNREAD = 02,
	CRECV = 04,
};

typedef struct Walkqid Walkqid;

struct Fid
{
	Client 	*client;
	Fid *next;
	short	fid;
	ushort	open;
	ushort	mode;	/* read/write */
	ulong	offset;	/* in file */
	int		dri;		/* dirread index */
	Qid		qid;
};

struct Walkqid
{
	Fid	*clone;
	int	nqid;
	Qid	qid[1];
};

#define ASSERT(A,B) ninepassert((int)A,B)

static int hash(int);
static void deletefids(Client*);
static Pending* findpending(Client*, ushort);

static
void
ninepfatal(char *fmt, ...)
{
	char buf[1024], *out;
	va_list arg;
	out = seprint(buf, buf+sizeof(buf), "Fatal error: ");
	va_start(arg, fmt);
	out = vseprint(out, buf+sizeof(buf), fmt, arg);
	va_end(arg);
	write(2, buf, out-buf);
	ninepexit(1);
}

static
void
ninepassert(int true, char *reason)
{
	if(!true)
		ninepfatal("assertion failed: %s\n", reason);
}

void *
ninepmalloc(int bytes)
{
	char *m = malloc(bytes);
	if(m == nil)
		ninepfatal(Enomem);
	memset(m, 0, bytes);
	return m;
}

void
ninepfree(void *p)
{
	free(p);
}

void
ninepdebug()
{
	Debug = 1;
}

static
Client *
newclient(Ninepserver *server, int fd)
{
	Client *c = (Client *)ninepmalloc(sizeof(Client));

	if(Debug)
		fprint(2, "New client at fd %d\n", fd);
	c->server = server;
	c->fd = fd;
	c->nread = 0;
	c->nc = 0;
	c->state = 0;
	c->fids = nil;
	c->pending = (Pending**)ninepmalloc(HASHSZ*sizeof(Pending*));
	c->uname = strdup(eve);
	c->aname = strdup("");
	c->next = server->clients;
	server->clients = c;
	if(server->ops->newclient)
		server->ops->newclient(c);
	return c;
}

static
void
freeclient(Client *c)
{
	Client **p;
	Ninepserver *server;

	if(Debug)
		fprint(2, "Freeing client at fd %d\n", c->fd);
	server = c->server;
	if(server->ops->freeclient)
		server->ops->freeclient(c);
	for(p = &server->clients; *p; p = &(*p)->next)
		if(*p == c){
			ninepclosesocket(c->fd);
			*p = c->next;
			deletefids(c);
			free(c->uname);
			free(c->aname);
			ninepfree(c);
			return;
		}
}

static
int
nbread(Client *c, int nr)
{
	int nb;

	if(c->state&CDISC)
		return -1;
	nb = nineprecv(c->server, c->fd, c->msg + c->nread, nr, 0);
	if(nb <= 0){
		c->nread = 0;
		c->state |= CDISC;
		return -1;
	}
	c->nread += nb;
	return 0;
}

static
int
rd(Client *c, Fcall *r)
{
	if(c->nc > 0){	/* last convM2S consumed nc bytes */
		c->nread -= c->nc;
		if((int)c->nread < 0){
			r->ename = "negative size in rd";
			return -1;
		}
		memmove(c->msg, c->msg+c->nc, c->nread);
		c->nc = 0;
	}
	if(c->state&CRECV){
		if(nbread(c, MSGMAX - c->nread) != 0){
			r->ename = "unexpected EOF";
			return -1;
		}
		c->state &= ~CRECV;
	}
	c->nc = convM2S((uchar*)(c->msg), c->nread, r);
	if(c->nc < 0){
		r->ename = "bad message format";
		return -1;
	}
	if(c->nc == 0 && c->nread > 0){
		c->nread = 0;
		c->state &= ~CNREAD;
		return 0;
	}
	if(c->nread > c->nc)
		c->state |= CNREAD;
	else
		c->state &= ~CNREAD;
	if(c->nc == 0)
		return 0;
	/* fprint(2, "rd: %F\n", r); */
	return 1;
}

static
int
wr(Client *c, Fcall *r)
{
	int n;
	char buf[MSGMAX];

	n = convS2M(r, (uchar*)buf, sizeof(buf));
	if(n < 0){
		r->ename = "bad message type in wr";
		return -1;
	}
	/* fprint(2, "wr: %F\n", r); */
	return ninepsend(c->server, c->fd, buf, n, 0);
}

static
void
sremove(Ninepserver *server, Ninepfile *f)
{
	Ninepfile *s, *next, **p;

	if(f == nil)
		return;
	if(f->d.qid.type&QTDIR)
		for(s = f->child; s != nil; s = next){
			next = s->sibling;
			sremove(server, s);
	}
	for(p = &server->ftab[hash(f->d.qid.path)]; *p; p = &(*p)->next)
		if(*p == f){
			*p = f->next;
			break;
		}
	for(p = &f->parent->child; *p; p = &(*p)->sibling)
		if(*p == f){
			*p = f->sibling;
			break;
		}
	ninepfree(f->d.name);
	ninepfree(f->d.uid);
	ninepfree(f->d.gid);
	ninepfree(f);
}

int
nineprmfile(Ninepserver *server, Path qid)
{
	Ninepfile *f;

	f = ninepfindfile(server, qid);
	if(f != nil){
		if(f->parent == nil)
			return -1;
		sremove(server, f);
		return 0;
	}
	return -1;
}

static
void
incref(Ninepfile *f)
{
	if(f != nil)
		f->ref++;
}

static
void
decref(Ninepfile *f)
{
	if(f != nil)
		--f->ref;
}

static
void
increff(Fid *f)
{
	incref(ninepfindfile(f->client->server, f->qid.path));
}

static
void
decreff(Fid *f)
{
	decref(ninepfindfile(f->client->server, f->qid.path));
}

static
void
incopen(Fid *f)
{
	Ninepfile *file;

	if(f->open && (file = ninepfindfile(f->client->server, f->qid.path)) != nil)
		file->open++;
}

static
void
decopen(Fid *f)
{
	Ninepfile *file;

	if(f->open && (file = ninepfindfile(f->client->server, f->qid.path)) != nil)
		file->open--;
}

int
ninepperm(Ninepfile *f, char *uid, int mode)
{
	int m, p;

	p = 0;
	switch(mode&3){
	case OREAD:	p = AREAD;	break;
	case OWRITE:	p = AWRITE;	break;
	case ORDWR:	p = AREAD+AWRITE;	break;
	case OEXEC:	p = AEXEC;	break;
	}
	if(mode&OTRUNC)
		p |= AWRITE;
	m = f->d.mode&7;
	if((p&m) == p)
		return 1;
	if(strcmp(f->d.uid, uid) == 0){
		m |= (f->d.mode>>6)&7;
		if((p&m) == p)
			return 1;
	}
	if(strcmp(f->d.gid, uid) == 0){
		m |= (f->d.mode>>3)&7;
		if((p&m) == p)
			return 1;
	}
	return 0;
}

static
int
hash(int n)
{
	return n&(HASHSZ-1);
}

Ninepfile *
ninepfindfile(Ninepserver *server, Path path)
{
	Ninepfile *f;

	for(f = server->ftab[hash(path)]; f != nil; f = f->next){
		if(f->d.qid.path == path)
			return f;
	}
	return nil;
}

static
Fid *
findfid(Client *c, short fid)
{
	Fid *f;
	for(f = c->fids; f && f->fid != fid; f = f->next)
		;
	return f;
}

static
void
deletefid(Client *c, Fid *d)
{
	/* TODO: end any outstanding reads on this fid */
	Fid **f;

	for(f = &c->fids; *f; f = &(*f)->next)
		if(*f == d){
			decreff(d);
			decopen(d);
			*f = d->next;
			ninepfree(d);
			return;
		}
}

static
void
deletefids(Client *c)
{
	Fid *f, *g;

	for(f = c->fids; f; f = g){
		decreff(f);
		decopen(f);
		g = f->next;
		ninepfree(f);
	}
}

Fid *
newfid(Client *c, short fid, Qid qid){
	Fid *f;

	f = ninepmalloc(sizeof(Fid));
	ASSERT(f, "newfid");
	f->client = c;
	f->fid = fid;
	f->open = 0;
	f->dri = 0;
	f->qid = qid;
	f->next = c->fids;
	c->fids = f;
	increff(f);
	return f;
}

int
eqqid(Qid a, Qid b)
{
	return a.path == b.path && a.vers == b.vers;
}

static
Fid *
fidclone(Fid *old, short fid)
{
	Fid *new;

	new = newfid(old->client, fid, old->qid);
	return new;
}

static
Walkqid*
devwalk(Client *c, Ninepfile *file, Fid *fp, Fid *nfp, char **name, int nname, char **err)
{
	Ninepserver *server;
	long j;
	Walkqid *wq;
	char *n;
	Ninepfile *p, *f;
	Ninepops *ops;
	Qid qid;

	*err = nil;
	server = c->server;
	ops = server->ops;

	wq = ninepmalloc(sizeof(Walkqid)+(nname-1)*sizeof(Qid));
	wq->nqid = 0;

	p = file;
	qid = p != nil ? p->d.qid : fp->qid;
	for(j = 0; j < nname; j++){
		if(!(qid.type&QTDIR)){
			if(j == 0)
				ninepfatal("devwalk error");
			*err = Enotdir;
			goto Done;
		}
		if(p != nil && !ninepperm(p, c->uname, OEXEC)){
			*err = Eperm;
			goto Done;
		}
		n = name[j];
		if(strcmp(n, ".") == 0){
    Accept:
			wq->qid[wq->nqid++] = nfp->qid;
			continue;
		}
		if(p != nil && strcmp(n, "..") == 0 && p->parent){
			decref(p);
			nfp->qid.path = p->parent->d.qid.path;
			nfp->qid.type = p->parent->d.qid.type;
			nfp->qid.vers = 0;
			incref(p->parent);
			p = p->parent;
			qid = p->d.qid;
			goto Accept;
		}
		
		if(ops->walk != nil){
			char *e;

			e = ops->walk(&qid, n);
			if(e == nil){
				decreff(nfp);
				nfp->qid = qid;
				increff(nfp);
				p = ninepfindfile(server, qid.path);
				if(server->needfile && p == nil)
					goto Done;
				qid = p != nil ? p->d.qid : nfp->qid;
				goto Accept;
			}
		}

		if(p != nil)
		for(f = p->child; f != nil; f = f->sibling){
			if(strcmp(n, f->d.name) == 0){
				decref(p);
				nfp->qid.path = f->d.qid.path;
				nfp->qid.type = f->d.qid.type;
				nfp->qid.vers = 0;
				incref(f);
				p = f;
				qid = p->d.qid;
				goto Accept;
			}
		}
		if(j == 0 && *err == nil)
			*err = Enonexist;
		goto Done;
	}
Done:
	if(*err != nil){
		ninepfree(wq);
		return nil;
	}
	return wq;
}

static
long
devdirread(Fid *fp, Ninepfile *file, char *d, long n)
{
	long dsz, m;
	Ninepfile *f;
	int i;

	struct{
		Dir d;
		char slop[100];	/* TO DO */
	}dir;

	f = file->child;
	for(i = 0; i < fp->dri; i++)
		if(f == 0)
			return 0;
		else
			f = f->sibling;
	for(m = 0; m < n; fp->dri++){
		if(f == nil)
			break;
		dir.d = f->d;
		dsz = convD2M(&dir.d, (uchar*)d, n-m);
		m += dsz;
		d += dsz;
		f = f->sibling;
	}

	return m;
}

static
char*
nilconv(char *s)
{
	if(s != nil && s[0] == '\0')
		return nil;
	return s;
}

static
Ninepfile *
newfile(Ninepserver *server, Ninepfile *parent, int isdir, Path qid, char *name, int mode, char *owner)
{
	Ninepfile *file;
	Dir d;
	int h;

	if(qid == -1)
		qid = server->qidgen++;
	file = ninepfindfile(server, qid);
	if(file != nil)
		return nil;
	if(parent != nil){
		for(file = parent->child; file != nil; file = file->sibling)
			if(strcmp(name, file->d.name) == 0)
				return nil;
	}
	file = (Ninepfile *)ninepmalloc(sizeof(Ninepfile));
	file->parent = parent;
	file->child = nil;
	h = hash(qid);
	file->next = server->ftab[h];
	server->ftab[h] = file;
	if(parent){
		file->sibling = parent->child;
		parent->child = file;
	}else
		file->sibling = nil;

	d.type = 'X';
	d.dev = 'x';
	d.qid.path = qid;
	d.qid.type = 0;
	d.qid.vers = 0;
	d.mode = mode;
	d.atime = time(0);
	d.mtime = boottime;
	d.length = 0;
	d.name = strdup(name);
	d.uid = strdup(owner);
	d.gid = strdup(eve);
	d.muid = "";

	if(isdir){
		d.qid.type |= QTDIR;
		d.mode |= DMDIR;
	}
	else{
		d.qid.type &= ~QTDIR;
		d.mode &= ~DMDIR;
	}

	file->d = d;
	file->ref = 0;
	file->open = 0;
	return file;
}

char *
ninepinit(Ninepserver *server, Ninepops *ops, char *address, int perm, int needfile)
{
	fmtinstall('F', fcallfmt);
	if(perm == -1)
		perm = 0555;
	server->ops = ops;
	server->clients = nil;
	server->root = nil;
	server->ftab = (Ninepfile**)ninepmalloc(HASHSZ*sizeof(Ninepfile*));
	server->qidgen = Qroot+1;
	if(ninepinitsocket() < 0)
		return "ninepinitsocket failed";
	server->connfd = ninepannounce(server, address);
	if(server->connfd < 0)
		return "can't announce on network port";
	ninepinitwait(server);
	server->root = newfile(server, nil, 1, Qroot, "/", perm|DMDIR, eve);
	server->needfile = needfile;
	return nil;
}

char*
ninepend(Ninepserver *server)
{
	USED(server);
	ninependsocket();
	return nil;
}

void
nineplisten(Ninepserver *server, int fd)
{
	ninepnewclient(server, fd);
}

int
ninepready(Ninepserver *server, int fd)
{
	return ninepnewmsg(server, fd);
}

static
char *
nineprequest(Ninepserver *server)
{
	Client *c;
	static Client *next;
	Fcall *f;
	int s;

	f = &server->fcall;
	if(ninepnewcall(server)){
		s = ninepaccept(server);
		if(s >= 0){
			newclient(server, s);
			ninepnewclient(server, s);
		}
	}
	if(next == nil)
		next = server->clients;
	server->curc = nil;
	for(c = next; c != nil; c = next){
		next = c->next;
		if(c->fd >= 0 && ninepnewmsg(server, c->fd))
			c->state |= CRECV;
		if(c->state&(CNREAD|CRECV)){
			if(c->state&CDISC){
				ninepfreeclient(server, c->fd);
				freeclient(c);
				continue;
			}
			if(rd(c, f) <= 0)
				return "error reading 9p request";
			server->curc = c;
			if(Debug)
				fprint(2, "<-%d- %F\n", c->fd, f);
			return nil;
		}
	}
	return nil;
}

char *
ninepwait(Ninepserver *server)
{
	Client *c;
	char *err;

	c = server->curc;
	if(c == nil || !c->state&CNREAD)
		if((err = ninepwaitmsg(server)) != nil)
			return err;
	if((err = nineprequest(server)) != nil)
		return err;
	return nil;
}

static
void
reply(Client *c, Fcall *f, char *err)
{
	if(err){
		f->type = Rerror;
		f->ename = err;
	}
	else if(f->type % 2 == 0)
		f->type++;
	if(Debug)
		fprint(2, "-%d-> %F\n", c->fd, f);
	wr(c, f);
}

void
ninepreply(Ninepserver *server, char *err)
{
	reply(server->curc, &server->fcall, err);
	server->curc = nil;
}

Pending*
ninepreplylater(Ninepserver *server)
{
	int h;
	Client *c;
	Pending *p;

	c = server->curc;
	p = ninepmalloc(sizeof(Pending));
	h = hash(server->fcall.tag);
	p->c = c;
	p->fcall = server->fcall;
	p->flushed = 0;
	p->flushtag = NOTAG;
	p->next = c->pending[h];
	c->pending[h] = p;
	server->curc = nil;
	return p;
}

static
Pending*
findpending(Client *c, ushort tag)
{
	int h;
	Pending *p;

	h = hash(tag);
	for(p = c->pending[h]; p != nil; p = p->next)
		if(p->fcall.tag == tag)
			return p;
	return nil;
}

static
void
deletepending(Pending *pend)
{
	int h;
	Pending *p;
	Client *c;

	h = hash(pend->fcall.tag);
	c = pend->c;
	p = c->pending[h];
	if(p == pend){
		c->pending[h] = pend->next;
		return;
	}
	for(; p != nil; p = p->next)
		if(p->next == pend){
			p->next = pend->next;
			break;
		}
	ninepfree(pend);
}

void
ninepcompleted(Pending *pend)
{
	Client *c;
	Fcall *f;

	if(!pend->flushed){
		c = pend->c;
		f = &pend->fcall;
		reply(c, f, nil);
		pend->flushed = 1;
		if(pend->flushtag != NOTAG){
			f->type = Rflush;
			f->tag = pend->flushtag;
			reply(c, f, nil);
		}
	}
	deletepending(pend);
}

void
ninepdefault(Ninepserver *server)
{
	Fid *fp, *nfp;
	int i, open, mode;
	char ebuf[EMSGLEN];
	Walkqid *wq;
	Ninepfile *file;
	Dir dir;
	Qid qid;
	char strs[128];
	Client *c;
	Fcall *f;
	Ninepops *ops;
	Pending *p;

	p = nil;
	c = server->curc;
	if(c == nil)
		return;
	f = &server->fcall;
	ops = server->ops;
	ebuf[0] = 0;
	if(f->type == Tflush){
		if(f->oldtag == f->tag || (p = findpending(c, f->oldtag)) == nil || p->flushed)
			reply(c, f, nil);
		p->flushtag = f->tag;		
		return;
	}
	file = nil;
	fp = findfid(c, f->fid);
	if(f->type != Tversion && f->type != Tauth && f->type != Tattach){
		if(fp == nil){
			ninepreply(server, Enofid);
			return;
		}
		else{
			file = ninepfindfile(c->server, fp->qid.path);
			if(c->server->needfile && file == nil){
				ninepreply(server, Enonexist);
				return;
			}
		}
	}
	/* if(fp == nil) fprint(2, "fid not found for %d\n", f->fid); */
	switch(f->type){
	case	Twalk:
		nfp = findfid(c, f->newfid);
		f->type = Rerror;
		if(nfp){
			deletefid(c, nfp);
			nfp = nil;
		}
		if(nfp){
			f->ename = "fid in use";
			break;
		}else if(fp->open){
			f->ename = "can't clone";
			break;
		}
		if(f->newfid != f->fid)
			nfp = fidclone(fp, f->newfid);
		else
			nfp = fp;
		if((wq = devwalk(c, file, fp, nfp, f->wname, f->nwname, &f->ename)) == nil){
			if(nfp != fp)
				deletefid(c, nfp);
			f->type = Rerror;
		}else{
			if(nfp != fp){
				if(wq->nqid != f->nwname)
					deletefid(c, nfp);
			}
			f->type = Rwalk;
			f->nwqid = wq->nqid;
			for(i = 0; i < wq->nqid; i++)
				f->wqid[i] = wq->qid[i];
			ninepfree(wq);
		}
		break;
	case	Topen:
		f->ename = nil;
		if(fp->open)
			f->ename = Eopen;
		else if((fp->qid.type&QTDIR) && (f->mode&(OWRITE|OTRUNC|ORCLOSE)))
			f->ename = Eperm;
		else if(file != nil && !ninepperm(file, c->uname, f->mode))
			f->ename = Eperm;
		else if((f->mode&ORCLOSE) && file != nil && file->parent != nil && !ninepperm(file->parent, c->uname, OWRITE))
			f->ename = Eperm;
		if(f->ename != nil){
			f->type = Rerror;
			break;
		}
		f->ename = Enonexist;
		decreff(fp);
		if(ops == nil || ops->open == nil || (f->ename = ops->open(&fp->qid, f->mode)) == nil){
			f->type = Ropen;
			f->qid = fp->qid;
			fp->mode = f->mode;
			fp->open = 1;
			fp->offset = 0;
			incopen(fp);
		}
		else
			f->type = Rerror;
		increff(fp);
		break;
	case	Tcreate:
		f->ename = nil;
		if(fp->open)
			f->ename = Eopen;
		else if(!(fp->qid.type&QTDIR))
			f->ename = Enotdir;
		else if((f->perm&DMDIR) && (f->mode&(OWRITE|OTRUNC|ORCLOSE)))
			f->ename = Eperm;
		else if(file != nil && !ninepperm(file, c->uname, OWRITE))
			f->ename = Eperm;
		if(f->ename != nil){
			f->type = Rerror;
			break;
		}
		f->ename = Eperm;
		decreff(fp);
		if(file != nil){
			if(f->perm&DMDIR)
				f->perm = (f->perm&~0777) | (file->d.mode&f->perm&0777) | DMDIR;
			else
				f->perm = (f->perm&(~0777|0111)) | (file->d.mode&f->perm&0666);
		}
		if(OP(create, &fp->qid, f->name, f->perm, f->mode)){
			f->type = Rcreate;
			f->qid = fp->qid;
			fp->mode = f->mode;
			fp->open = 1;
			fp->offset = 0;
			incopen(fp);
		}
		else
			f->type = Rerror;
		increff(fp);
		break;
	case	Tread:
		if(!fp->open){
			f->type = Rerror;
			f->ename = Ebadfid;
			break;
		}
		if(fp->qid.type&QTDIR || (file != nil && file->d.qid.type&QTDIR)){
			f->type = Rread;
			if(file == nil){
				f->ename = Eperm;
				if(OP(read, fp->qid, c->data, (ulong*)(&f->count), fp->dri)){
					f->data = c->data;
				}
				else
					f->type = Rerror;
			}
			else{
				f->count = devdirread(fp, file, c->data, f->count);
				f->data = c->data;
			}		
		}else{
			f->ename = Eperm;
			f->type = Rerror;
			if(OP(read, fp->qid, c->data, (ulong*)(&f->count), f->offset)){
				f->type = Rread;
				f->data = c->data;			
			}
		}
		break;
	case	Twrite:
		if(!fp->open){
			f->type = Rerror;
			f->ename = Ebadfid;
			break;
		}
		f->ename = Eperm;
		f->type = Rerror;
		if(OP(write,fp->qid, f->data, (ulong*)(&f->count), f->offset)){
			f->type = Rwrite;
		}
		break;
	case	Tclunk:
		open = fp->open;
		mode = fp->mode;
		qid = fp->qid;
		deletefid(c, fp);
		f->type = Rclunk;
		if(open && OP(close,qid, mode))
			break;
		f->type = Rerror;
		break;
	case	Tremove:
		if(file != nil && file->parent != nil && !ninepperm(file->parent, c->uname, OWRITE)){
			f->type = Rerror;
			f->ename = Eperm;
			deletefid(c, fp);
			break;
		}
		f->ename = Eperm;
		if(OP(remove, fp->qid))
			f->type = Rremove;
		else
			f->type = Rerror;
		deletefid(c, fp);
		break;
	case	Tstat:
		f->stat = ninepmalloc(MAXSTAT);
		f->ename = "stat error";
		if(ops->stat == nil && file != nil){
			f->type = Rstat;
			f->nstat = convD2M(&file->d, f->stat, MAXSTAT);
		}
		else if(OP(stat, fp->qid, &dir)){
			f->type = Rstat;
			f->nstat = convD2M(&dir, f->stat, MAXSTAT);
		}
		else
			f->type = Rerror;
		ninepfree(f->stat);
		break;
	case	Twstat:
		f->ename = Eperm;
		convM2D(f->stat, f->nstat, &dir, strs);
		dir.name = nilconv(dir.name);
		dir.uid = nilconv(dir.uid);
		dir.gid = nilconv(dir.gid);
		dir.muid = nilconv(dir.muid);
		if(OP(wstat, fp->qid, &dir))
			f->type = Rwstat;
		else
			f->type = Rerror;
		break;
	case	Tversion:
		f->type = Rversion;
		f->tag = NOTAG;
		break;
	case Tauth:
		f->type = Rauth;
		break;
	case	Tattach:
		if(fp){
			f->type = Rerror;
			f->ename = "fid in use";
		}else{
			Qid q;

			if(f->uname[0]){
				free(c->uname);
				c->uname = strdup(f->uname);
			}
			if(f->aname[0]){
				free(c->aname);
				c->aname = strdup(f->aname);
			}
			q.path = Qroot;
			q.type = QTDIR;
			q.vers = 0;
			fp = newfid(c, f->fid, q);
			f->type = Rattach;
			f->fid = fp->fid;
			f->qid = q;
			if(OP(attach, c->uname, c->aname))
				break;
			f->type = Rerror;
		}
		break;
	}
	if(c->server->curc == c)
		ninepreply(server, nil);
}

Ninepfile*
ninepaddfile(Ninepserver *server, Path pqid, Path qid, char *name, int mode, char *owner)
{
	Ninepfile *f, *parent;

	parent = ninepfindfile(server, pqid);
	if(parent == nil || (parent->d.qid.type&QTDIR) == 0)
		return nil;
	f = newfile(server, parent, 0, qid, name, mode, owner);
	return f;
}

Ninepfile*
ninepadddir(Ninepserver *server, Path pqid, Path qid, char *name, int mode, char *owner)
{
	Ninepfile *f, *parent;

	parent = ninepfindfile(server, pqid);
	if(parent == nil || (parent->d.qid.type&QTDIR) == 0)
		return nil;
	f = newfile(server, parent, 1, qid, name, mode|DMDIR, owner);
	return f;
}

long
ninepreadstr(ulong off, char *buf, ulong n, char *str)
{
	int size;

	size = strlen(str);
	if(off >= size)
		return 0;
	if(off+n > size)
		n = size-off;
	memmove(buf, str+off, n);
	return n;
}

Qid
ninepqid(int path, int isdir)
{
	Qid q;

	q.path = path;
	q.vers = 0;
	if(isdir)
		q.type = QTDIR;
	else
		q.type = 0;
	return q;
}

void
ninepsetowner(char *name)
{
	eve = name;
}
