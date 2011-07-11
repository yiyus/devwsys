#include <lib9.h>
#include <draw.h>
#include <memdraw.h>
#include <memlayer.h>
#include <cursor.h>
#include "dat.h"
#include "fns.h"

int kbdputc(Kbdbuf*, int);
long latin1(uchar*, int);

void
writekbd(Window *w, int ch)
{
	char buf[5];
	Rune r;
	Kbdbuf *kbd;

	kbd = &w->kbd;
	if((ch = kbdputc(kbd, ch)) == -1)
		return;
	r = ch;
	sprint(buf, "%C", r);
	ixppwrite(w->kbdp, buf);
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

