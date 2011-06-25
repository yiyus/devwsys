#include <lib9.h>
#include <draw.h>
#include <memdraw.h>
#include <memlayer.h>
#include "dat.h"
#include "fns.h"

void addkbd(Kbdbuf*, int);
void matchkbd(Kbdbuf*, Reqbuf*);
int kbdputc(Kbdbuf*, int);
long latin1(uchar*, int);
void replykbd(Kbdbuf*, void*);

void
writekbd(Window *w, int ch)
{
	Kbdbuf *kbd;
	Reqbuf *kbdreqs;

	kbd = &w->kbd;
	kbdreqs = &w->kbdreqs;

	addkbd(kbd, ch);
	matchkbd(kbd, kbdreqs);
}

void
readkbd(Window *w, void* r)
{
	Kbdbuf *kbd;
	Reqbuf *kbdreqs;

	kbd = &w->kbd;
	kbdreqs = &w->kbdreqs;

	addreq(kbdreqs, r);
	// fprint(2, "kbd unstall\n");
	kbd->stall = 0;
	matchkbd(kbd, kbdreqs);
}

void
addkbd(Kbdbuf *kbd, int ch)
{
	Rune r;

	ch = kbdputc(kbd, ch);
	if(kbd->stall || ch == -1)
		return;
	r = ch;

	kbd->r[kbd->wi++] = r;
	if(kbd->wi == nelem(kbd->r))
		kbd->wi = 0;
	if(kbd->ri == kbd->wi){
		kbd->stall = 1;
		/* fprint(2, "kbd stall\n"); */
	}
}

void
matchkbd(Kbdbuf *kbd, Reqbuf *kbdreqs)
{
	while(kbd->ri != kbd->wi && kbdreqs->ri != kbdreqs->wi){
		replykbd(kbd, nextreq(kbdreqs));
		kbd->ri++;
		if(kbd->ri == nelem(kbd->r))
			kbd->ri = 0;
	}
}

void
replykbd(Kbdbuf *kbd, void *r)
{
	char buf[5];
	Rune rune;

	rune = kbd->r[kbd->ri];
	sprint(buf, "%C", rune);
	ixprread(r, buf);
}

int
kbdputc(Kbdbuf *kbd, int ch)
{
	int n;
	Rune r;

	if(ch < 0) {
		kbd->alting = 0;
		kbd->nk = 0;
		return -1;
	}
	r = ch;
	if(kbd->alting){
		/*
		 * Kludge for Mac's X11 3-button emulation.
		 * It treats Command+Button as button 3, but also
		 * ends up sending XK_Meta_L twice.
		 */
		if(ch == Kalt){
			kbd->alting = 0;
			return -1;
		}
		kbd->nk += runetochar((char*)&kbd->k[kbd->nk], &r);
		n = latin1(kbd->k, kbd->nk);
		if(n > 0){
			kbd->alting = 0;
			r = (Rune)n;
			return r;
		}
		if(n == -1){
			kbd->alting = 0;
			kbd->k[kbd->nk] = 0;
			r = (Rune)0;
			return r;
		}
		/* n < -1, need more input */
		return -1;
	}else if(ch == Kalt){
		kbd->alting = 1;
		kbd->nk = 0;
		return -1;
	}
	return r;
}

