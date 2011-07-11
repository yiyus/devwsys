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
newwin(char *label, char *winsize)
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
	if(!drawattach(w, winsize)) {
		// TODO: cleanup window
		return nil;
	}
	return w;
}

int
parsewinsize(char *s, Rectangle *r, int *havemin)
{
	char c, *os;
	int i, j, k, l;

	os = s;
	*havemin = 0;
	*r = Rect(0,0,0,0);
	if(!isdigit((int)*s))
		goto oops;
	i = strtol(s, &s, 0);
	if(*s == 'x'){
		s++;
		if(!isdigit((int)*s))
			goto oops;
		j = strtol(s, &s, 0);
		r->max.x = i;
		r->max.y = j;
		if(*s == 0)
			return 0;
		if(*s != '@')
			goto oops;

		s++;
		if(!isdigit((int)*s))
			goto oops;
		i = strtol(s, &s, 0);
		if(*s != ',' && *s != ' ')
			goto oops;
		s++;
		if(!isdigit((int)*s))
			goto oops;
		j = strtol(s, &s, 0);
		if(*s != 0)
			goto oops;
		*r = rectaddpt(*r, Pt(i,j));
		*havemin = 1;
		return 0;
	}

	c = *s;
	if(c != ' ' && c != ',')
		goto oops;
	s++;
	if(!isdigit((int)*s))
		goto oops;
	j = strtol(s, &s, 0);
	if(*s != c)
		goto oops;
	s++;
	if(!isdigit((int)*s))
		goto oops;
	k = strtol(s, &s, 0);
	if(*s != c)
		goto oops;
	s++;
	if(!isdigit((int)*s))
		goto oops;
	l = strtol(s, &s, 0);
	if(*s != 0)
		goto oops;
	*r = Rect(i,j,k,l);
	*havemin = 1;
	return 0;

oops:
	werrstr("bad syntax in window size '%s'", os);
	return -1;
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
	w->x = nil;
	w->deleted++;
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

