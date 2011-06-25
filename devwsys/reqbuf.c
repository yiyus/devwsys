#include <lib9.h>
#include <draw.h>
#include <memdraw.h>
#include <memlayer.h>
#include "dat.h"
#include "fns.h"

void
addreq(Reqbuf *reqs, void* r)
{
	reqs->r[reqs->wi] = r;
	reqs->wi++;
	if(reqs->wi == nelem(reqs->r))
		reqs->wi = 0;
	if(reqs->wi == reqs->ri)
		sysfatal("too many queued kbd reads");
}

void*
nextreq(Reqbuf *reqs)
{
	void *r;

	r = reqs->r[reqs->ri];
	reqs->ri++;
	if(reqs->ri == nelem(reqs->r))
		reqs->ri = 0;
	return r;
}
