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
	for(i=0; i<draw->nname; )
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
drawmesg(Client *client, void *av, int n)
{
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
	for(i=0; i<draw->nname; )
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
