#include <ctype.h>
#include <lib9.h>
#include <draw.h>
#include <cursor.h>
#include "dat.h"
#include "fns.h"

int nwindow = 0;

Window*
newwin(void)
{
	static int id = 0;
	Window *w;

	if(!(w = mallocz(sizeof(Window), 1)))
		return nil;
	if(!(window = realloc(window, ++nwindow*sizeof(Window*))))
		return nil;
	window[nwindow-1] = w;

	w->id = ++id;
	return w;
}

Window*
winlookup(int id)
{
	int i;
	Window *w;

	for(i=0; i<nwindow; i++){
		w = window[i];
		if(w && w->id == id)
			return w;
	}
	return nil;
}

void
deletewin(Window *w)
{
	int i;

	if(w->pid > 0 && killreply(w))
		return;
	drawdettach(w);
	xdeletewin(w);
	w->deleted++;
	for(i = 0; i < nwindow; i++){
		if(window[i] == w)
			break;
	}
	if(i == nwindow)
		return;
	--nwindow;
	memmove(window+i, window+i+1, (nwindow-i)*sizeof(Window*));
}

void
setlabel(Window *w, char *label)
{
	if(w->label)
		free(w->label);
	w->label = label;
	xupdatelabel(w);
}

