#include <ctype.h>
#include <lib9.h>
#include <draw.h>
#include <memdraw.h>
#include "dat.h"
#include "fns.h"

static int id = 0;

Window*
newwin(void)
{
	Window *w;

	if(!(w = malloc(sizeof(Window))))
		return NULL;
	if(!(window = realloc(window, ++nwindow*sizeof(Window*))))
		return NULL;
	window[nwindow-1] = w;

	w->id = id++;
	w->deleted = 0;
	w->label = NULL;
	w->mouseopen = 0;

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
deletewin(int i)
{
	free(window[i]->x);
	window[i]->x = nil;
	window[i]->deleted++;
	--nwindow;
	memmove(window+i, window+i+1, (nwindow-i)*sizeof(Window*));
}
