#include <lib9.h>
#include <kern.h>
#include <draw.h>
#include <memdraw.h>
#include <memlayer.h>
#include "dat.h"
#include "fns.h"

/* Error messages */
static char
	Enodrawimage[] = "unknown id for draw image",
	Eoldname[] = "named image no longer valid";

static DImage* drawlookup(Client*, int, int);
static void drawfreedimage(Draw*, DImage*);
static int drawgoodname(Draw*, DImage*);
static void drawflush(Draw*);

int nclient = 0;
char *error = nil;

Client*
drawnewclient(Draw *draw)
{
	static int clientid = 0;
	Client *cl, **cp;
	int i;

	for(i=0; i<nclient; i++){
		cl = client[i];
		if(cl == 0)
			break;
	}
	if(i == nclient){
		cp = malloc((nclient+1)*sizeof(Client*));
		if(cp == 0)
			return 0;
		memmove(cp, client, nclient*sizeof(Client*));
		free(client);
		client = cp;
		nclient++;
		cp[i] = 0;
	}
	cl = mallocz(sizeof(Client), 1);
	if(cl == 0)
		return 0;
	cl->slot = i;
	cl->clientid = ++clientid;
	cl->op = SoverD;
	cl->draw = draw;
	client[i] = cl;
	return cl;
}

void
drawreplacescreenimage(Client *cl, Memimage *m)
{
	/*
	 * Replace the screen image because the screen
	 * was resized.
	 * 
	 * In theory there should only be one reference
	 * to the current screen image, and that's through
	 * client0's image 0, installed a few lines above.
	 * Once the client drops the image, the underlying backing 
	 * store freed properly.  The client is being notified
	 * about the resize through external means, so all we
	 * need to do is this assignment.
	 */
	Memimage *om;
	Memimage *screenimage;

	screenimage = cl->draw->screenimage;
	// qlock(&sdraw.lk);
	om = screenimage;
	cl->draw->screenimage = m;
	// m->screenref = 1;
	// if(om && --om->screenref == 0){
	//	freememimage(om);
	// }
	// qunlock(&sdraw.lk);
}

static
void
drawrefreshscreen(DImage *l, Client *client)
{
	while(l != nil && l->dscreen == nil)
		l = l->fromname;
	if(l != nil && l->dscreen->owner != client)
		l->dscreen->owner->refreshme = 1;
}

static
void
drawrefresh(Memimage *m, Rectangle r, void *v)
{
	Refx *x;
	DImage *d;
	Client *c;
	Refresh *ref;

	USED(m);

	if(v == 0)
		return;
	x = v;
	c = x->client;
	d = x->dimage;
	for(ref=c->refresh; ref; ref=ref->next)
		if(ref->dimage == d){
			combinerect(&ref->r, r);
			return;
		}
	ref = mallocz(sizeof(Refresh), 1);
	if(ref){
		ref->dimage = d;
		ref->r = r;
		ref->next = c->refresh;
		c->refresh = ref;
	}
}

static
void
addflush(Draw *draw, Rectangle r)
{
	int abb, ar, anbb;
	Rectangle nbb;

	if(draw->softscreen==0 || !rectclip(&r, draw->screenimage->r))
		return;

	if(draw->flushrect.min.x >= draw->flushrect.max.x){
		draw->flushrect = r;
		draw->waste = 0;
		return;
	}
	nbb = draw->flushrect;
	combinerect(&nbb, r);
	ar = Dx(r)*Dy(r);
	abb = Dx(draw->flushrect)*Dy(draw->flushrect);
	anbb = Dx(nbb)*Dy(nbb);
	/*
	 * Area of new waste is area of new bb minus area of old bb,
	 * less the area of the new segment, which we assume is not waste.
	 * This could be negative, but that's OK.
	 */
	draw->waste += anbb-abb - ar;
	if(draw->waste < 0)
		draw->waste = 0;
	/*
	 * absorb if:
	 *	total area is small
	 *	waste is less than half total area
	 * 	rectangles touch
	 */
	if(anbb<=1024 || draw->waste*2<anbb || rectXrect(draw->flushrect, r)){
		draw->flushrect = nbb;
		return;
	}
	/* emit current state */
	if(draw->flushrect.min.x < draw->flushrect.max.x)
		xflushmemscreen(draw->window, draw->flushrect);
	draw->flushrect = r;
	draw->waste = 0;
}

Memscreen*
drawinstallscreen(Client *client, DScreen *d, int id, DImage *dimage, DImage *dfill, int public)
{
	Memscreen *s;
	CScreen *c;

	c = malloc(sizeof(CScreen));
	if(dimage && dimage->image && dimage->image->chan == 0)
		fatal("bad image %p in drawinstallscreen", dimage->image);

	if(c == 0)
		return 0;
	if(d == 0){
		d = malloc(sizeof(DScreen));
		if(d == 0){
			free(c);
			return 0;
		}
		s = malloc(sizeof(Memscreen));
		if(s == 0){
			free(c);
			free(d);
			return 0;
		}
		s->frontmost = 0;
		s->rearmost = 0;
		d->dimage = dimage;
		if(dimage){
			s->image = dimage->image;
			dimage->ref++;
		}
		d->dfill = dfill;
		if(dfill){
			s->fill = dfill->image;
			dfill->ref++;
		}
		d->ref = 0;
		d->id = id;
		d->screen = s;
		d->public = public;
		d->next = client->draw->dscreen;
		d->owner = client;
		client->draw->dscreen = d;
	}
	c->dscreen = d;
	d->ref++;
	c->next = client->cscreen;
	client->cscreen = c;
	return d->screen;
}

static
void
drawfreedscreen(Draw *draw, DScreen *this)
{
	DScreen *ds, *next;

	this->ref--;
	if(this->ref < 0)
		fprint(2, "negative ref in drawfreedscreen\n");
	if(this->ref > 0)
		return;
	ds = draw->dscreen;
	if(ds == this){
		draw->dscreen = this->next;
		goto Found;
	}
	while(next = ds->next){	/* assign = */
		if(next == this){
			ds->next = this->next;
			goto Found;
		}
		ds = next;
	}
	/*
	 * Should signal Enodrawimage, but too hard.
	 */
	return;

    Found:
	if(this->dimage)
		drawfreedimage(draw, this->dimage);
	if(this->dfill)
		drawfreedimage(draw, this->dfill);
	free(this->screen);
	free(this);
}

static
DScreen*
drawlookupdscreen(Draw *draw, int id)
{
	DScreen *s;

	s = draw->dscreen;
	while(s){
		if(s->id == id)
			return s;
		s = s->next;
	}
	return 0;
}

static
void
drawdelname(Draw *draw, DName *name)
{
	int i;

	i = name-draw->name;
	memmove(name, name+1, (draw->nname-(i+1))*sizeof(DName));
	draw->nname--;
}

static
void
drawfreedimage(Draw *draw, DImage *dimage)
{
	int i;
	Memimage *l;
	DScreen *ds;

	dimage->ref--;
	if(dimage->ref < 0)
		fprint(2, "negative ref in drawfreedimage\n");
	if(dimage->ref > 0)
		return;

	/* any names? */
	for(i=0; draw && i<draw->nname; )
		if(draw->name[i].dimage == dimage)
			drawdelname(draw, draw->name+i);
		else
			i++;
	if(dimage->fromname){	/* acquired by name; owned by someone else*/
		drawfreedimage(draw, dimage->fromname);
		goto Return;
	}
	ds = dimage->dscreen;
	l = dimage->image;
	dimage->dscreen = nil;	/* paranoia */
	dimage->image = nil;
	if(ds){
		if(l->data == draw->screenimage->data)
			addflush(draw, l->layer->screenr);
		if(l->layer->refreshfn == drawrefresh)	/* else true owner will clean up */
			free(l->layer->refreshptr);
		l->layer->refreshptr = nil;
		if(drawgoodname(draw, dimage))
			memldelete(l);
		else
			memlfree(l);
		drawfreedscreen(draw, ds);
	}else
		freememimage(dimage->image);
    Return:
	free(dimage->fchar);
	free(dimage);
}

static
void
drawuninstallscreen(Client *client, CScreen *this)
{
	CScreen *cs, *next;

	cs = client->cscreen;
	if(cs == this){
		client->cscreen = this->next;
		drawfreedscreen(client->draw, this->dscreen);
		free(this);
		return;
	}
	while(next = cs->next){	/* assign = */
		if(next == this){
			cs->next = this->next;
			drawfreedscreen(client->draw, this->dscreen);
			free(this);
			return;
		}
		cs = next;
	}
}

static
int
drawuninstall(Client *client, int id)
{
	DImage *d, **l;

	for(l=&client->dimage[id&HASHMASK]; (d=*l) != nil; l=&d->next){
		if(d->id == id){
			*l = d->next;
			drawfreedimage(client->draw, d);
			return 0;
		}
	}
	return -1;
}

Memimage*
drawinstall(Client *client, int id, Memimage *i, DScreen *dscreen)
{
	DImage *d;

	d = malloc(sizeof(DImage));
	if(d == 0)
		return 0;
	d->id = id;
	d->ref = 1;
	d->name = 0;
	d->vers = 0;
	d->image = i;
	d->nfchar = 0;
	d->fchar = 0;
	d->fromname = 0;
	d->dscreen = dscreen;
	d->next = client->dimage[id&HASHMASK];
	client->dimage[id&HASHMASK] = d;
	return i;
}

static
void
dstflush(Draw *draw, int dstid, Memimage *dst, Rectangle r)
{
	Memlayer *l;

	if(dstid == 0){
		combinerect(&draw->flushrect, r);
		return;
	}
	/* how can this happen? -rsc, dec 12 2002 */
	if(dst == 0){
		fprint(2, "nil dstflush\n");
		return;
	}
	l = dst->layer;
	if(l == nil)
		return;
	do{
		if(l->screen->image->data != draw->screenimage->data)
			return;
		r = rectaddpt(r, l->delta);
		l = l->screen->image->layer;
	}while(l);
	addflush(draw, r);
}

void
readdrawctl(char *buf, Client *cl)
{
	int n;
	DImage *di;
	Memimage *i;

	if(cl->infoid < 0)
		error = Enodrawimage;
	if(cl->infoid == 0){
		i = cl->draw->screenimage;
		if(i == nil)
			error = Enodrawimage;
	}else{
		di = drawlookup(cl, cl->infoid, 1);
		if(di == nil)
			error = Enodrawimage;
		i = di->image;
	}
	n = sprint(buf, "%11d %11d %11s %11d %11d %11d %11d %11d %11d %11d %11d %11d ",
		cl->clientid, cl->infoid, chantostr(buf, i->chan), (i->flags&Frepl)==Frepl,
		i->r.min.x, i->r.min.y, i->r.max.x, i->r.max.y,
		i->clipr.min.x, i->clipr.min.y, i->clipr.max.x, i->clipr.max.y);
	cl->infoid = -1;
}

void
readrefresh(char *buf, long n, Client *cl)
{
	Refresh *r;
	uchar *p;

/***
	for(;;){
		if(cl->refreshme || cl->refresh)
			break;
		qunlock(&sdraw.q);
		f(waserror()){
			qlock(&sdraw.q);	/* restore lock for waserror() above * /
			nexterror();
		}
		Sleep(&cl->refrend, drawrefactive, cl);
		poperror();
		qlock(&sdraw.q);
	}
***/
	p = buf;
	while(cl->refresh && n>=5*4){
		r = cl->refresh;
		BPLONG(p+0*4, r->dimage->id);
		BPLONG(p+1*4, r->r.min.x);
		BPLONG(p+2*4, r->r.min.y);
		BPLONG(p+3*4, r->r.max.x);
		BPLONG(p+4*4, r->r.max.y);
		cl->refresh = r->next;
		free(r);
		p += 5*4;
		n -= 5*4;
	}
	cl->refreshme = 0;
	n = p-(uchar*)buf;
}

void
drawfree(Client *cl)
{
	int i;
	Draw *draw;
	DImage *d, **dp;
	Refresh *r;

	draw = cl->draw;
	while(r = cl->refresh){	/* assign = */
		cl->refresh = r->next;
		free(r);
	}
	/* free names */
	for(i=0; draw && i<draw->nname; )
		if(draw->name[i].client == cl)
			drawdelname(draw, draw->name+i);
		else
			i++;
	while(cl->cscreen)
		drawuninstallscreen(cl, cl->cscreen);
	/* all screens are freed, so now we can free images */
	dp = cl->dimage;
	for(i=0; i<NHASH; i++){
		while((d = *dp) != nil){
			*dp = d->next;
			drawfreedimage(draw, d);
		}
		dp++;
	}
	client[cl->slot] = 0;
	drawflush(draw);	/* to erase visible, now dead windows */
	free(cl);
}

static
void
drawflush(Draw *draw)
{
	if(draw->flushrect.min.x < draw->flushrect.max.x)
		xflushmemscreen(draw->window, draw->flushrect);
	draw->flushrect = Rect(10000, 10000, -10000, -10000);
}

static
int
drawcmp(char *a, char *b, int n)
{
	if(strlen(a) != n)
		return 1;
	return memcmp(a, b, n);
}

static
DName*
drawlookupname(Draw *draw, int n, char *str)
{
	DName *name, *ename;

	name = draw->name;
	ename = &name[draw->nname];
	for(; name<ename; name++)
		if(drawcmp(name->name, str, n) == 0)
			return name;
	return 0;
}

static
int
drawgoodname(Draw *draw, DImage *d)
{
	DName *n;

	/* if window, validate the screen's own images */
	if(d->dscreen)
		if(drawgoodname(draw, d->dscreen->dimage) == 0
		|| drawgoodname(draw, d->dscreen->dfill) == 0)
			return 0;
	if(d->name == nil)
		return 1;
	n = drawlookupname(draw, strlen(d->name), d->name);
	if(n==nil || n->vers!=d->vers)
		return 0;
	return 1;
}

static
DImage*
drawlookup(Client *client, int id, int checkname)
{
	DImage *d;
	Draw *draw;

	d = client->dimage[id&HASHMASK];
	while(d){
		if(d->id == id){
			draw = client->draw;
			if(checkname && !drawgoodname(draw, d))
				error = Eoldname;
			return d;
		}
		d = d->next;
	}
	return 0;
}

char*
drawerr(void)
{
	char *err;

	err = error;
	error = nil;
	return err;
}

static
DScreen*
drawlookupscreen(Client *client, int id, CScreen **cs)
{
	CScreen *s;

	s = client->cscreen;
	while(s){
		if(s->dscreen->id == id){
			*cs = s;
			return s->dscreen;
		}
		s = s->next;
	}
	/* caller must check! */
	return 0;
}

static
int
drawaddname(Client *client, DImage *di, int n, char *str, char **err)
{
	DName *name, *ename, *new, *t;
	Draw *draw;
	char *ns;

	draw = client->draw;
	name = draw->name;
	ename = &name[draw->nname];
	for(; name<ename; name++)
		if(drawcmp(name->name, str, n) == 0){
			*err = "image name in use";
			return -1;
		}
	t = mallocz((draw->nname+1)*sizeof(DName), 1);
	ns = malloc(n+1);
	if(t == nil || ns == nil){
		free(t);
		free(ns);
		*err = "out of memory";
		return -1;
	}
	memmove(t, draw->name, draw->nname*sizeof(DName));
	free(draw->name);
	draw->name = t;
	new = &draw->name[draw->nname++];
	new->name = ns;
	memmove(new->name, str, n);
	new->name[n] = 0;
	new->dimage = di;
	new->client = client;
	// new->vers = ++draw->vers;
	return 0;
}

static int
drawclientop(Client *cl)
{
	int op;

	op = cl->op;
	cl->op = SoverD;
	return op;
}

static
Memimage*
drawimage(Client *client, uchar *a)
{
	DImage *d;

	d = drawlookup(client, BGLONG(a), 1);
	if(d == nil)
		return nil;	/* caller must check! */
	return d->image;
}

static
void
drawrectangle(Rectangle *r, uchar *a)
{
	r->min.x = BGLONG(a+0*4);
	r->min.y = BGLONG(a+1*4);
	r->max.x = BGLONG(a+2*4);
	r->max.y = BGLONG(a+3*4);
}

static
void
drawpoint(Point *p, uchar *a)
{
	p->x = BGLONG(a+0*4);
	p->y = BGLONG(a+1*4);
}

static
Point
drawchar(Memimage *dst, Point p, Memimage *src, Point *sp, DImage *font, int index, int op)
{
	FChar *fc;
	Rectangle r;
	Point sp1;

	fc = &font->fchar[index];
	r.min.x = p.x+fc->left;
	r.min.y = p.y-(font->ascent-fc->miny);
	r.max.x = r.min.x+(fc->maxx-fc->minx);
	r.max.y = r.min.y+(fc->maxy-fc->miny);
	sp1.x = sp->x+fc->left;
	sp1.y = sp->y+fc->miny;
	memdraw(dst, r, src, sp1, font->image, Pt(fc->minx, fc->miny), op);
	p.x += fc->width;
	sp->x += fc->width;
	return p;
}

static
uchar*
drawcoord(uchar *p, uchar *maxp, int oldx, int *newx)
{
	int b, x;

	if(p >= maxp)
		return nil;
	b = *p++;
	x = b & 0x7F;
	if(b & 0x80){
		if(p+1 >= maxp)
			return nil;
		x |= *p++ << 7;
		x |= *p++ << 15;
		if(x & (1<<22))
			x |= ~0<<23;
	}else{
		if(b & 0x40)
			x |= ~0<<7;
		x += oldx;
	}
	*newx = x;
	return p;
}

int
drawmesg(Client *client, void *av, int n)
{
	char cbuf[40], *err, ibuf[12*12+1], *s;
	int c, ci, doflush, dstid, e0, e1, esize, j, m;
	int ni, nw, oesize, oldn, op, ox, oy, repl, scrnid, y; 
	uchar *a, refresh, *u;
	u32int chan, value;
	CScreen *cs;
	DImage *di, *ddst, *dsrc, *font, *ll;
	DName *dn;
	Draw *draw;
	DScreen *dscrn;
	FChar *fc;
	Memimage *dst, *i, *l, **lp, *mask, *src;
	Memscreen *scrn;
	Point p, *pp, q, sp;
	Rectangle clipr, r;
	Refreshfn reffn;
	Refx *refx;
	Window *w;

	// qlock(&sdraw.lk);
	a = av;
	m = 0;
	oldn = n;
	draw = client->draw;
	w = draw->window;

	while((n-=m) > 0){
		a += m;
/*fprint(2, "XXX msgwrite %d(%d): %c\n", n, *a, *a); */
		switch(*a){
/*fprint(2, "bad command %d\n", *a); */
			err = "bad draw command";
			goto error;

		/* allocate: 'b' id[4] screenid[4] refresh[1] chan[4] repl[1]
			R[4*4] clipR[4*4] rrggbbaa[4]
		 */
		case 'b':
			m = 1+4+4+1+4+1+4*4+4*4+4;
			if(n < m)
				goto Eshortdraw;
			dstid = BGLONG(a+1);
			scrnid = BGSHORT(a+5);
			refresh = a[9];
			chan = BGLONG(a+10);
			repl = a[14];
			drawrectangle(&r, a+15);
			drawrectangle(&clipr, a+31);
			value = BGLONG(a+47);
			if(drawlookup(client, dstid, 0))
				goto Eimageexists;
			if(scrnid){
				dscrn = drawlookupscreen(client, scrnid, &cs);
				if(!dscrn)
					goto Enodrawscreen;
				scrn = dscrn->screen;
				if(repl || chan!=scrn->image->chan){
					err = "image parameters incompatibile with screen";
					goto error;
				}
				reffn = 0;
				switch(refresh){
				case Refbackup:
					break;
				case Refnone:
					reffn = memlnorefresh;
					break;
				case Refmesg:
					reffn = drawrefresh;
					break;
				default:
					err = "unknown refresh method";
					goto error;
				}
				l = memlalloc(scrn, r, reffn, 0, value);
				if(l == 0)
					goto Edrawmem;
				addflush(draw, l->layer->screenr);
				l->clipr = clipr;
				rectclip(&l->clipr, r);
				if(drawinstall(client, dstid, l, dscrn) == 0){
					memldelete(l);
					goto Edrawmem;
				}
				dscrn->ref++;
				if(reffn){
					refx = nil;
					if(reffn == drawrefresh){
						refx = mallocz(sizeof(Refx), 1);
						if(refx == 0){
							if(drawuninstall(client, dstid) < 0)
								goto Enodrawimage;
							goto Edrawmem;
						}
						refx->client = client;
						refx->dimage = drawlookup(client, dstid, 1);
					}
					memlsetrefresh(l, reffn, refx);
				}
				continue;
			}
			i = xallocmemimage(w, r, chan, PMundef);
			if(i == 0)
				goto Edrawmem;
			if(repl)
				i->flags |= Frepl;
			i->clipr = clipr;
			if(!repl)
				rectclip(&i->clipr, r);
			if(drawinstall(client, dstid, i, 0) == 0){
				freememimage(i);
				goto Edrawmem;
			}
			memfillcolor(i, value);
			continue;

		/* allocate screen: 'A' id[4] imageid[4] fillid[4] public[1] */
		case 'A':
			m = 1+4+4+4+1;
			if(n < m)
				goto Eshortdraw;
			dstid = BGLONG(a+1);
			if(dstid == 0)
				goto Ebadarg;
			if(drawlookupdscreen(draw, dstid))
				goto Escreenexists;
			ddst = drawlookup(client, BGLONG(a+5), 1);
			dsrc = drawlookup(client, BGLONG(a+9), 1);
			if(ddst==0 || dsrc==0)
				goto Enodrawimage;
			if(drawinstallscreen(client, 0, dstid, ddst, dsrc, a[13]) == 0)
				goto Edrawmem;
			continue;

		/* set repl and clip: 'c' dstid[4] repl[1] clipR[4*4] */
		case 'c':
			m = 1+4+1+4*4;
			if(n < m)
				goto Eshortdraw;
			ddst = drawlookup(client, BGLONG(a+1), 1);
			if(ddst == nil)
				goto Enodrawimage;
			if(ddst->name){
				err = "can't change repl/clipr of shared image";
				goto error;
			}
			dst = ddst->image;
			if(a[5])
				dst->flags |= Frepl;
			drawrectangle(&dst->clipr, a+6);
			continue;

		/* draw: 'd' dstid[4] srcid[4] maskid[4] R[4*4] P[2*4] P[2*4] */
		case 'd':
			m = 1+4+4+4+4*4+2*4+2*4;
			if(n < m)
				goto Eshortdraw;
			dst = drawimage(client, a+1);
			dstid = BGLONG(a+1);
			src = drawimage(client, a+5);
			mask = drawimage(client, a+9);
			if(!dst || !src || !mask)
				goto Enodrawimage;
			drawrectangle(&r, a+13);
			drawpoint(&p, a+29);
			drawpoint(&q, a+37);
			op = drawclientop(client);
			memdraw(dst, r, src, p, mask, q, op);
			dstflush(draw, dstid, dst, r);
			continue;

		/* toggle debugging: 'D' val[1] */
		case 'D':
			m = 1+1;
			if(n < m)
				goto Eshortdraw;
			drawdebug = a[1];
			continue;

		/* ellipse: 'e' dstid[4] srcid[4] center[2*4] a[4] b[4] thick[4] sp[2*4] alpha[4] phi[4]*/
		case 'e':
		case 'E':
			m = 1+4+4+2*4+4+4+4+2*4+2*4;
			if(n < m)
				goto Eshortdraw;
			dst = drawimage(client, a+1);
			dstid = BGLONG(a+1);
			src = drawimage(client, a+5);
			if(!dst || !src)
				goto Enodrawimage;
			drawpoint(&p, a+9);
			e0 = BGLONG(a+17);
			e1 = BGLONG(a+21);
			if(e0<0 || e1<0){
				err = "invalid ellipse semidiameter";
				goto error;
			}
			j = BGLONG(a+25);
			if(j < 0){
				err = "negative ellipse thickness";
				goto error;
			}
			
			drawpoint(&sp, a+29);
			c = j;
			if(*a == 'E')
				c = -1;
			ox = BGLONG(a+37);
			oy = BGLONG(a+41);
			op = drawclientop(client);
			/* high bit indicates arc angles are present */
			if(ox & ((ulong)1<<31)){
				if((ox & ((ulong)1<<30)) == 0)
					ox &= ~((ulong)1<<31);
				memarc(dst, p, e0, e1, c, src, sp, ox, oy, op);
			}else
				memellipse(dst, p, e0, e1, c, src, sp, op);
			dstflush(draw, dstid, dst, Rect(p.x-e0-j, p.y-e1-j, p.x+e0+j+1, p.y+e1+j+1));
			continue;

		/* free: 'f' id[4] */
		case 'f':
			m = 1+4;
			if(n < m)
				goto Eshortdraw;
			ll = drawlookup(client, BGLONG(a+1), 0);
			if(ll && ll->dscreen && ll->dscreen->owner != client)
				ll->dscreen->owner->refreshme = 1;
			if(drawuninstall(client, BGLONG(a+1)) < 0)
				goto Enodrawimage;
			continue;

		/* free screen: 'F' id[4] */
		case 'F':
			m = 1+4;
			if(n < m)
				goto Eshortdraw;
			if(!drawlookupscreen(client, BGLONG(a+1), &cs))
				goto Enodrawscreen;
			drawuninstallscreen(client, cs);
			continue;

		/* initialize font: 'i' fontid[4] nchars[4] ascent[1] */
		case 'i':
			m = 1+4+4+1;
			if(n < m)
				goto Eshortdraw;
			dstid = BGLONG(a+1);
			if(dstid == 0){
				err = "can't use display as font";
				goto error;
			}
			font = drawlookup(client, dstid, 1);
			if(font == 0)
				goto Enodrawimage;
			if(font->image->layer){
				err = "can't use window as font";
				goto error;
			}
			ni = BGLONG(a+5);
			if(ni<=0 || ni>4096){
				err = "bad font size (4096 chars max)";
				goto error;
			}
			free(font->fchar);	/* should we complain if non-zero? */
			font->fchar = mallocz(ni*sizeof(FChar), 1);
			if(font->fchar == 0){
				err = "no memory for font";
				goto error;
			}
			memset(font->fchar, 0, ni*sizeof(FChar));
			font->nfchar = ni;
			font->ascent = a[9];
			continue;

		/* set image 0 to screen image */
		case 'J':
			m = 1;
			if(n < m)
				goto Eshortdraw;
			if(drawlookup(client, 0, 0))
				goto Eimageexists;
			drawinstall(client, 0, draw->screenimage, 0);
			client->infoid = 0;
			continue;

		/* get image info: 'I' */
		case 'I':
			m = 1;
			if(n < m)
				goto Eshortdraw;
			if(client->infoid < 0)
				goto Enodrawimage;
			if(client->infoid == 0){
				i = draw->screenimage;
				if(i == nil)
					goto Enodrawimage;
			}else{
				di = drawlookup(client, client->infoid, 1);
				if(di == nil)
					goto Enodrawimage;
				i = di->image;
			}
			ni = sprint(ibuf, "%11d %11d %11s %11d %11d %11d %11d %11d"
					" %11d %11d %11d %11d ",
					client->clientid,
					client->infoid,	
					chantostr(cbuf, i->chan),
					(i->flags&Frepl)==Frepl,
					i->r.min.x, i->r.min.y, i->r.max.x, i->r.max.y,
					i->clipr.min.x, i->clipr.min.y, 
					i->clipr.max.x, i->clipr.max.y);
			free(client->readdata);
			client->readdata = malloc(ni);
			if(client->readdata == nil)
				goto Enomem;
			memmove(client->readdata, ibuf, ni);
			client->nreaddata = ni;
			client->infoid = -1;
			continue;	

		/* load character: 'l' fontid[4] srcid[4] index[2] R[4*4] P[2*4] left[1] width[1] */
		case 'l':
			m = 1+4+4+2+4*4+2*4+1+1;
			if(n < m)
				goto Eshortdraw;
			font = drawlookup(client, BGLONG(a+1), 1);
			if(font == 0)
				goto Enodrawimage;
			if(font->nfchar == 0)
				goto Enotfont;
			src = drawimage(client, a+5);
			if(!src)
				goto Enodrawimage;
			ci = BGSHORT(a+9);
			if(ci >= font->nfchar)
				goto Eindex;
			drawrectangle(&r, a+11);
			drawpoint(&p, a+27);
			memdraw(font->image, r, src, p, memopaque, p, S);
			fc = &font->fchar[ci];
			fc->minx = r.min.x;
			fc->maxx = r.max.x;
			fc->miny = r.min.y;
			fc->maxy = r.max.y;
			fc->left = a[35];
			fc->width = a[36];
			continue;

		/* draw line: 'L' dstid[4] p0[2*4] p1[2*4] end0[4] end1[4] radius[4] srcid[4] sp[2*4] */
		case 'L':
			m = 1+4+2*4+2*4+4+4+4+4+2*4;
			if(n < m)
				goto Eshortdraw;
			dst = drawimage(client, a+1);
			dstid = BGLONG(a+1);
			drawpoint(&p, a+5);
			drawpoint(&q, a+13);
			e0 = BGLONG(a+21);
			e1 = BGLONG(a+25);
			j = BGLONG(a+29);
			if(j < 0){
				err = "negative line width";
				goto error;
			}
			src = drawimage(client, a+33);
			if(!dst || !src)
				goto Enodrawimage;
			drawpoint(&sp, a+37);
			op = drawclientop(client);
			memline(dst, p, q, e0, e1, j, src, sp, op);
			/* avoid memlinebbox if possible */
			if(dstid==0 || dst->layer!=nil){
				/* BUG: this is terribly inefficient: update maximal containing rect*/
				r = memlinebbox(p, q, e0, e1, j);
				dstflush(draw, dstid, dst, insetrect(r, -(1+1+j)));
			}
			continue;

		/* create image mask: 'm' newid[4] id[4] */
/*
 *
		case 'm':
			m = 4+4;
			if(n < m)
				goto Eshortdraw;
			break;
 *
 */

		/* attach to a named image: 'n' dstid[4] j[1] name[j] */
		case 'n':
			m = 1+4+1;
			if(n < m)
				goto Eshortdraw;
			j = a[5];
			if(j == 0)	/* give me a non-empty name please */
				goto Eshortdraw;
			m += j;
			if(n < m)
				goto Eshortdraw;
			dstid = BGLONG(a+1);
			if(drawlookup(client, dstid, 0))
				goto Eimageexists;
			dn = drawlookupname(draw, j, (char*)a+6);
			if(dn == nil)
				goto Enoname;
			s = malloc(j+1);
			if(s == nil)
				goto Enomem;
			if(drawinstall(client, dstid, dn->dimage->image, 0) == 0)
				goto Edrawmem;
			di = drawlookup(client, dstid, 0);
			if(di == 0)
				goto Eoldname;
			di->vers = dn->vers;
			di->name = s;
			di->fromname = dn->dimage;
			di->fromname->ref++;
			memmove(di->name, a+6, j);
			di->name[j] = 0;
			client->infoid = dstid;
			continue;

		/* name an image: 'N' dstid[4] in[1] j[1] name[j] */
		case 'N':
			m = 1+4+1+1;
			if(n < m)
				goto Eshortdraw;
			c = a[5];
			j = a[6];
			if(j == 0)	/* give me a non-empty name please */
				goto Eshortdraw;
			m += j;
			if(n < m)
				goto Eshortdraw;
			di = drawlookup(client, BGLONG(a+1), 0);
			if(di == 0)
				goto Enodrawimage;
			if(di->name)
				goto Enamed;
			if(c)
				if(drawaddname(client, di, j, (char*)a+7, &err) < 0)
					goto error;
			else{
				dn = drawlookupname(draw, j, (char*)a+7);
				if(dn == nil)
					goto Enoname;
				if(dn->dimage != di)
					goto Ewrongname;
				drawdelname(draw, dn);
			}
			continue;

		/* position window: 'o' id[4] r.min [2*4] screenr.min [2*4] */
		case 'o':
			m = 1+4+2*4+2*4;
			if(n < m)
				goto Eshortdraw;
			dst = drawimage(client, a+1);
			if(!dst)
				goto Enodrawimage;
			if(dst->layer){
				drawpoint(&p, a+5);
				drawpoint(&q, a+13);
				r = dst->layer->screenr;
				ni = memlorigin(dst, p, q);
				if(ni < 0){
					err = "image origin failed";
					goto error;
				}
				if(ni > 0){
					addflush(draw, r);
					addflush(draw, dst->layer->screenr);
					ll = drawlookup(client, BGLONG(a+1), 1);
					drawrefreshscreen(ll, client);
				}
			}
			continue;

		/* set compositing operator for next draw operation: 'O' op */
		case 'O':
			m = 1+1;
			if(n < m)
				goto Eshortdraw;
			client->op = a[1];
			continue;

		/* filled polygon: 'P' dstid[4] n[2] wind[4] ignore[2*4] srcid[4] sp[2*4] p0[2*4] dp[2*2*n] */
		/* polygon: 'p' dstid[4] n[2] end0[4] end1[4] radius[4] srcid[4] sp[2*4] p0[2*4] dp[2*2*n] */
		case 'p':
		case 'P':
			m = 1+4+2+4+4+4+4+2*4;
			if(n < m)
				goto Eshortdraw;
			dstid = BGLONG(a+1);
			dst = drawimage(client, a+1);
			ni = BGSHORT(a+5);
			if(ni < 0){
				err = "negative cout in polygon";
				goto error;
			}
			e0 = BGLONG(a+7);
			e1 = BGLONG(a+11);
			j = 0;
			if(*a == 'p'){
				j = BGLONG(a+15);
				if(j < 0){
					err = "negative polygon line width";
					goto error;
				}
			}
			src = drawimage(client, a+19);
			if(!dst || !src)
				goto Enodrawimage;
			drawpoint(&sp, a+23);
			drawpoint(&p, a+31);
			ni++;
			pp = mallocz(ni*sizeof(Point), 1);
			if(pp == nil)
				goto Enomem;
			doflush = 0;
			if(dstid==0 || (dst->layer && dst->layer->screen->image->data == draw->screenimage->data))
				doflush = 1;	/* simplify test in loop */
			ox = oy = 0;
			esize = 0;
			u = a+m;
			for(y=0; y<ni; y++){
				q = p;
				oesize = esize;
				u = drawcoord(u, a+n, ox, &p.x);
				if(!u)
					goto Eshortdraw;
				u = drawcoord(u, a+n, oy, &p.y);
				if(!u)
					goto Eshortdraw;
				ox = p.x;
				oy = p.y;
				if(doflush){
					esize = j;
					if(*a == 'p'){
						if(y == 0){
							c = memlineendsize(e0);
							if(c > esize)
								esize = c;
						}
						if(y == ni-1){
							c = memlineendsize(e1);
							if(c > esize)
								esize = c;
						}
					}
					if(*a=='P' && e0!=1 && e0 !=~0)
						r = dst->clipr;
					else if(y > 0){
						r = Rect(q.x-oesize, q.y-oesize, q.x+oesize+1, q.y+oesize+1);
						combinerect(&r, Rect(p.x-esize, p.y-esize, p.x+esize+1, p.y+esize+1));
					}
					if(rectclip(&r, dst->clipr))		/* should perhaps be an arg to dstflush */
						dstflush(draw, dstid, dst, r);
				}
				pp[y] = p;
			}
			if(y == 1)
				dstflush(draw, dstid, dst, Rect(p.x-esize, p.y-esize, p.x+esize+1, p.y+esize+1));
			op = drawclientop(client);
			if(*a == 'p')
				mempoly(dst, pp, ni, e0, e1, j, src, sp, op);
			else
				memfillpoly(dst, pp, ni, e0, src, sp, op);
			free(pp);
			m = u-a;
			continue;

		/* read: 'r' id[4] R[4*4] */
		case 'r':
			m = 1+4+4*4;
			if(n < m)
				goto Eshortdraw;
			i = drawimage(client, a+1);
			if(!i)
				goto Enodrawimage;
			drawrectangle(&r, a+5);
			if(!rectinrect(r, i->r))
				goto Ereadoutside;
			c = bytesperline(r, i->depth);
			c *= Dy(r);
			free(client->readdata);
			client->readdata = mallocz(c, 0);
			if(client->readdata == nil){
				err = "readimage malloc failed";
				goto error;
			}
			client->nreaddata = memunload(i, r, client->readdata, c);
			if(client->nreaddata < 0){
				free(client->readdata);
				client->readdata = nil;
				err = "bad readimage call";
				goto error;
			}
			continue;

		/* string: 's' dstid[4] srcid[4] fontid[4] P[2*4] clipr[4*4] sp[2*4] ni[2] ni*(index[2]) */
		/* stringbg: 'x' dstid[4] srcid[4] fontid[4] P[2*4] clipr[4*4] sp[2*4] ni[2] bgid[4] bgpt[2*4] ni*(index[2]) */
		case 's':
		case 'x':
			m = 1+4+4+4+2*4+4*4+2*4+2;
			if(*a == 'x')
				m += 4+2*4;
			if(n < m)
				goto Eshortdraw;

			dst = drawimage(client, a+1);
			dstid = BGLONG(a+1);
			src = drawimage(client, a+5);
			if(!dst || !src)
				goto Enodrawimage;
			font = drawlookup(client, BGLONG(a+9), 1);
			if(font == 0)
				goto Enodrawimage;
			if(font->nfchar == 0)
				goto Enotfont;
			drawpoint(&p, a+13);
			drawrectangle(&r, a+21);
			drawpoint(&sp, a+37);
			ni = BGSHORT(a+45);
			u = a+m;
			m += ni*2;
			if(n < m)
				goto Eshortdraw;
			clipr = dst->clipr;
			dst->clipr = r;
			op = drawclientop(client);
			if(*a == 'x'){
				/* paint background */
				l = drawimage(client, a+47);
				if(!l)
					goto Enodrawimage;
				drawpoint(&q, a+51);
				r.min.x = p.x;
				r.min.y = p.y-font->ascent;
				r.max.x = p.x;
				r.max.y = r.min.y+Dy(font->image->r);
				j = ni;
				while(--j >= 0){
					ci = BGSHORT(u);
					if(ci<0 || ci>=font->nfchar){
						dst->clipr = clipr;
						goto Eindex;
					}
					r.max.x += font->fchar[ci].width;
					u += 2;
				}
				memdraw(dst, r, l, q, memopaque, ZP, op);
				u -= 2*ni;
			}
			q = p;
			while(--ni >= 0){
				ci = BGSHORT(u);
				if(ci<0 || ci>=font->nfchar){
					dst->clipr = clipr;
					goto Eindex;
				}
				q = drawchar(dst, q, src, &sp, font, ci, op);
				u += 2;
			}
			dst->clipr = clipr;
			p.y -= font->ascent;
			dstflush(draw, dstid, dst, Rect(p.x, p.y, q.x, p.y+Dy(font->image->r)));
			continue;

		/* use public screen: 'S' id[4] chan[4] */
		case 'S':
			m = 1+4+4;
			if(n < m)
				goto Eshortdraw;
			dstid = BGLONG(a+1);
			if(dstid == 0)
				goto Ebadarg;
			dscrn = drawlookupdscreen(draw, dstid);
			if(dscrn==0 || (dscrn->public==0 && dscrn->owner!=client))
				goto Enodrawscreen;
			if(dscrn->screen->image->chan != BGLONG(a+5)){
				err = "inconsistent chan";
				goto error;
			}
			if(drawinstallscreen(client, dscrn, 0, 0, 0, 0) == 0)
				goto Edrawmem;
			continue;

		/* top or bottom windows: 't' top[1] nw[2] n*id[4] */
		case 't':
			m = 1+1+2;
			if(n < m)
				goto Eshortdraw;
			nw = BGSHORT(a+2);
			if(nw < 0)
				goto Ebadarg;
			if(nw == 0)
				continue;
			m += nw*4;
			if(n < m)
				goto Eshortdraw;
			lp = mallocz(nw*sizeof(Memimage*), 1);
			if(lp == 0)
				goto Enomem;
			for(j=0; j<nw; j++){
				lp[j] = drawimage(client, a+1+1+2+j*4);
				if(lp[j] == nil){
					free(lp);
					goto Enodrawimage;
				}
			}
			if(lp[0]->layer == 0){
				err = "images are not windows";
				free(lp);
				goto error;
			}
			for(j=1; j<nw; j++)
				if(lp[j]->layer->screen != lp[0]->layer->screen){
					err = "images not on same screen";
					free(lp);
					goto error;
				}
			if(a[1])
				memltofrontn(lp, nw);
			else
				memltorearn(lp, nw);
			if(lp[0]->layer->screen->image->data == draw->screenimage->data)
				for(j=0; j<nw; j++)
					addflush(draw, lp[j]->layer->screenr);
			free(lp);
			ll = drawlookup(client, BGLONG(a+1+1+2), 1);
			drawrefreshscreen(ll, client);
			continue;

		/* visible: 'v' */
		case 'v':
			m = 1;
			drawflush(draw);
			continue;

		/* write: 'y' id[4] R[4*4] data[x*1] */
		/* write from compressed data: 'Y' id[4] R[4*4] data[x*1] */
		case 'y':
		case 'Y':
			m = 1+4+4*4;
			if(n < m)
				goto Eshortdraw;
			dstid = BGLONG(a+1);
			dst = drawimage(client, a+1);
			if(!dst)
				goto Enodrawimage;
			drawrectangle(&r, a+5);
			if(!rectinrect(r, dst->r))
				goto Ewriteoutside;
			y = memload(dst, r, a+m, n-m, *a=='Y');
			if(y < 0){
				err = "bad writeimage call";
				goto error;
			}
			dstflush(draw, dstid, dst, r);
			m += y;
			continue;
		}
	}
	// qunlock(&sdraw.lk);
	return oldn - n;

Enodrawimage:
	err = "unknown id for draw image";
	goto error;
Enodrawscreen:
	err = "unknown id for draw screen";
	goto error;
Eshortdraw:
	err = "short draw message";
	goto error;
/*
Eshortread:
	err = "draw read too short";
	goto error;
*/
Eimageexists:
	err = "image id in use";
	goto error;
Escreenexists:
	err = "screen id in use";
	goto error;
Edrawmem:
	err = "image memory allocation failed";
	goto error;
Ereadoutside:
	err = "readimage outside image";
	goto error;
Ewriteoutside:
	err = "writeimage outside image";
	goto error;
Enotfont:
	err = "image not a font";
	goto error;
Eindex:
	err = "character index out of range";
	goto error;
/*
Enoclient:
	err = "no such draw client";
	goto error;
Edepth:
	err = "image has bad depth";
	goto error;
Enameused:
	err = "image name in use";
	goto error;
*/
Enoname:
	err = "no image with that name";
	goto error;
Eoldname:
	err = "named image no longer valid";
	goto error;
Enamed:
	err = "image already has name";
	goto error;
Ewrongname:
	err = "wrong name for image";
	goto error;
Enomem:
	err = "out of memory";
	goto error;
Ebadarg:
	err = "bad argument in draw message";
	goto error;

error:
	werrstr("%s", err);
	// qunlock(&sdraw.lk);
	return -1;
}
