#include <lib9.h>
#include <kern.h>
#include <draw.h>
#include <memdraw.h>
#include "dat.h"
#include "fns.h"


int nclient = 0;

Client*
drawnewclient(void)
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
	client[i] = cl;
	return cl;
}
