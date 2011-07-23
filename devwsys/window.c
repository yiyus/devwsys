#include <ctype.h>
#include <lib9.h>
#include <draw.h>
#include <memdraw.h>
#include <memlayer.h>
#include <cursor.h>
#include "dat.h"
#include "fns.h"

int nwindow = 0;

Window*
newwin(char *label)
{
	static int id = 0;
	Window *w;

	if(!(w = mallocz(sizeof(Window), 1)))
		return nil;
	if(!(window = realloc(window, ++nwindow*sizeof(Window*))))
		return nil;
	window[nwindow-1] = w;

	w->id = ++id;
	w->label = label;
	return w;
}

void
deletewin(Window *w)
{
	int i;

	for(i = 0; i < nwindow; i++){
		if(window[i] == w)
			break;
	}
	if(i == nwindow)
		return;
	xdeletewin(w);
	drawdettach(w->draw);
	/*
	 * TODO
	 * Do not free w->draw (see devdraw.c:1920)
	 */
	// w->draw = nil;
	w->x = nil;
	w->deleted++;
	--nwindow;
	memmove(window+i, window+i+1, (nwindow-i)*sizeof(Window*));
	/*
	 * Write pid of the process to kill to the kill file.
	 */
	if(w->pid != 0)
		ixppwrite(w->killp, smprint("%d\n", w->pid));
}

void
setlabel(Window *w, char *label)
{
	if(w->label)
		free(w->label);
	w->label = label;
	xupdatelabel(w);
}

