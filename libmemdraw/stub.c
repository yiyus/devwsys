#include "lib9.h"
#include "draw.h"
#include "memdraw.h"
#include "pool.h"

/* alloc.c */

Memimage*
allocmemimage(Rectangle r, ulong chan)
{
	return _allocmemimage(r, chan);
}

void
freememimage(Memimage *m)
{
	_freememimage(m);
}

/* cload.c */

int
cloadmemimage(Memimage *i, Rectangle r, uchar *data, int ndata)
{
	return _cloadmemimage(i, r, data, ndata);
}

/* draw.c */

void
memimagedraw(Memimage *dst, Rectangle r, Memimage *src, Point sp, Memimage *mask, Point mp, int op)
{
	_memimagedraw(dst, r, src, sp, mask, mp, op);
}

void
memfillcolor(Memimage *m, ulong val)
{
	_memfillcolor(m, val);
}

u32int
pixelbits(Memimage *m, Point p)
{
	return _pixelbits(m, p);
}

/* load.c */

int
loadmemimage(Memimage *i, Rectangle r, uchar *data, int ndata)
{
	_loadmemimage(i, r, data, ndata);
}

/* unload.c */

int
unloadmemimage(Memimage *i, Rectangle r, uchar *data, int ndata)
{
	_unloadmemimage(i, r, data, ndata);
}

