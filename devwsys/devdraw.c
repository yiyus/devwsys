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

DImage* drawlookup(Client*, int, int);

int nclient = 0;
char *error = nil;

Client*
drawnewclient(Window *w)
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
	cl->window = w;
	client[i] = cl;
	return cl;
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
		i = cl->window->screenimage;
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

static
int
drawcmp(char *a, char *b, int n)
{
	if(strlen(a) != n)
		return 1;
	return memcmp(a, b, n);
}

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

DImage*
drawlookup(Client *client, int id, int checkname)
{
	DImage *d;
	Draw *draw;

	d = client->dimage[id&HASHMASK];
	while(d){
		if(d->id == id){
			draw = &client->window->draw;
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
