#include <lib9.h>
#include <draw.h>
#include <memdraw.h>
#include <memlayer.h>
#include <cursor.h>
#include "dat.h"
#include "fns.h"
#include "inc.h"
#include "x.h"

/* alloc.c */

Memimage*
allocmemimage(Rectangle r, ulong chan)
{
	/*
	 * TODO: will only return _memimage(),
	 * should know the current window
	 */
	return xallocmemimage(nil, r, chan, PMundef);
}

void
freememimage(Memimage *m)
{
	xfreememimage(m);
}

/* cload.c */

int
cloadmemimage(Memimage *i, Rectangle r, uchar *data, int ndata)
{
	return xcloadmemimage(i, r, data, ndata);
}

/* draw.c */

void
memimagedraw(Memimage *dst, Rectangle r, Memimage *src, Point sp, Memimage *mask, Point mp, int op)
{
	xmemimagedraw(dst, r, src, sp, mask, mp, op);
}

void
memfillcolor(Memimage *m, ulong val)
{
	xmemfillcolor(m, val);
}

ulong
pixelbits(Memimage *m, Point p)
{
	return xpixelbits(m, p);
}

/* load.c */

int
loadmemimage(Memimage *i, Rectangle r, uchar *data, int ndata)
{
	return xloadmemimage(i, r, data, ndata);
}

/* unload.c */

int
unloadmemimage(Memimage *i, Rectangle r, uchar *data, int ndata)
{
	return xunloadmemimage(i, r, data, ndata);
}

