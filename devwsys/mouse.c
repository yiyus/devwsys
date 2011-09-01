#include <lib9.h>
#include <draw.h>
#include <cursor.h>
#include "dat.h"
#include "fns.h"

static void addmouse(Mousebuf*, Mouse, int);
static void matchmouse(Mousebuf*, Reqbuf*);
static void replymouse(Mousebuf*, void*);

void
readmouse(Window *w, void* r)
{
        Mousebuf *mouse;
        Reqbuf *mousereqs;

        mouse = &w->mouse;
        mousereqs = &w->mousereqs;

        addreq(mousereqs, r);
        // fprint(2, "mouse unstall\n");
        mouse->stall = 0;
        matchmouse(mouse, mousereqs);
}

void
writemouse(Window *w, Mouse m, int resized)
{
	Mousebuf *mouse;
	Reqbuf *mousereqs;

	mouse = &w->mouse;
	mousereqs = &w->mousereqs;

	addmouse(mouse, m, resized);
	matchmouse(mouse, mousereqs);
}

static
void
addmouse(Mousebuf *mouse, Mouse m, int resized)
{
	int p;

        if(!mouse->open || mouse->stall)
                return;

	mouse->m[mouse->wi] = m;
	if(resized){
		p = mouse->wi - 1;
		if(p < 0)
			p = nelem(mouse->m) - 1;
		mouse->m[mouse->wi] = mouse->m[p];
		mouse->resized = 1;
	}
	mouse->wi++;
	if(mouse->wi == nelem(mouse->m))
		mouse->wi = 0;
	if(mouse->wi == mouse->ri){
		mouse->stall = 1;
		mouse->ri = 0;
		mouse->wi = 1;
		mouse->m[0] = m;
		/* fprint(2, "mouse stall\n"); */
	}
}

static
void
replymouse(Mousebuf *mouse, void *r)
{
        int n;
        char buf[50], c;
        Mouse m;

        m = mouse->m[mouse->ri];
        c = 'm';
        if(mouse->resized)
                c = 'r';
        n = sprint(buf, "%c%11d %11d %11d %11ld ", c, m.xy.x, m.xy.y, m.buttons, m.msec);
        buf[n] = '\0';
        readreply(r, buf);
        mouse->resized = 0;
}

static
void
matchmouse(Mousebuf *mouse, Reqbuf *mousereqs)
{
        while(mouse->ri != mouse->wi && mousereqs->ri != mousereqs->wi){
                replymouse(mouse, nextreq(mousereqs));
                mouse->ri++;
                if(mouse->ri == nelem(mouse->m))
                        mouse->ri = 0;
        }
}

static int
revbyte(int b)
{
	int r;

	r = 0;
	r |= (b&0x01) << 7;
	r |= (b&0x02) << 5;
	r |= (b&0x04) << 3;
	r |= (b&0x08) << 1;
	r |= (b&0x10) >> 1;
	r |= (b&0x20) >> 3;
	r |= (b&0x40) >> 5;
	r |= (b&0x80) >> 7;
	return r;
}

static
void
drawwincursor(Window *win, Drawcursor* c)
{
	uchar *bs, *bc, *ps, *pm;
	int i, j, w, h, bpl;

	if(c->data == nil){
		if(win->cursor.h != 0)
			win->cursor.h = 0;
		xsetcursor(win);
		return;
	}

	h = (c->maxy-c->miny)/2;	/* image, then mask */
	if(h > CursorSize)
		h = CursorSize;
	bpl = bytesperline(Rect(c->minx, c->miny, c->maxx, c->maxy), 1);
	w = bpl;
	if(w > CursorSize/8)
		w = CursorSize/8;

	ps = win->cursor.src;
	pm = win->cursor.mask;
	bc = c->data;
	bs = c->data + h*bpl;
	for(i = 0; i < h; i++){
		for(j = 0; j < bpl && j < w; j++) {
			*ps++ = revbyte(bs[j]);
			*pm++ = revbyte(bs[j] | bc[j]);
		}
		bs += bpl;
		bc += bpl;
	}
	win->cursor.h = h;
	win->cursor.w = w*8;
	win->cursor.hotx = c->hotx;
	win->cursor.hoty = c->hoty;
	xsetcursor(win);
}

static
int
infernocursorwrite(Window *w, char *buf, int n)
{
	uchar *p;
	Drawcursor c;

	p = buf;
	/* TO DO: perhaps interpret data as an Image */
	/*
	 *  hotx[4] hoty[4] dx[4] dy[4] clr[dx/8 * dy/2] set[dx/8 * dy/2]
	 *  dx must be a multiple of 8; dy must be a multiple of 2.
	 */
	if(n < 8)
		return -1;
	c.hotx = BGLONG(p+0*4);
	c.hoty = BGLONG(p+1*4);
	c.minx = 0;
	c.miny = 0;
	c.maxx = BGLONG(p+2*4);
	c.maxy = BGLONG(p+3*4);
	if(c.maxx%8 != 0 || c.maxy%2 != 0 || n-4*4 != (c.maxx/8 * c.maxy))
		return -1;
	c.data = p + 4*4;
	drawwincursor(w, &c);
	return 0;
}

static
int
plan9cursorwrite(Window *w, char *buf, int n)
{
	uchar *p;
	Drawcursor c;

	p = buf;
	/*
	 *  offsetx[4] offsety[4] clr[2*16] set[2*16]
	 */
	if(n < 2*4+2*2*16)
		return -1;
	c.hotx = BGLONG(p+0*4);
	c.hoty = BGLONG(p+1*4);
	c.minx = 0;
	c.miny = 0;
	c.maxx = 16;
	c.maxy = 32;
	c.data = p + 2*4;
	drawwincursor(w, &c);
	return 0;
}

void
cursorwrite(Window *w, char *buf, int n)
{
	if(infernocursorwrite(w, buf, n) < 0 &&
	plan9cursorwrite(w, buf, n) < 0){
		w->cursor.h = 0;
		xsetcursor(w);
	}
}

