#include <lib9.h>
#include <kern.h>
#include <draw.h>
#include <memdraw.h>
#include <memlayer.h>
#include "dat.h"
#include "fns.h"

int nclient = 0;

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
	cl = malloc(sizeof(Client));
	if(cl == 0)
		return 0;
	memset(cl, 0, sizeof(Client));
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

int
initscreenimage(Memimage *m)
{
	// TODO: inferno-os/emu/port/devdraw.c:855,881
	return 1;
}
