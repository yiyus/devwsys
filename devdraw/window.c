#include <lib9.h>
#include <draw.h>
#include <memdraw.h>
#include "devdraw.h"

int isdigit(uchar);

static int id = 0;

Win*
newwin(void)
{
	Win *win;

	if(!(win = malloc(sizeof(Win))))
		return NULL;
	win->id = id++;
	win->label = NULL;

	// TODO: add win to []wins and inc nwins

	return win;
}

int
parsewinsize(char *s, Rectangle *r, int *havemin)
{
	char c, *os;
	int i, j, k, l;

	os = s;
	*havemin = 0;
	*r = Rect(0,0,0,0);
	if(!isdigit((uchar)*s))
		goto oops;
	i = strtol(s, &s, 0);
	if(*s == 'x'){
		s++;
		if(!isdigit((uchar)*s))
			goto oops;
		j = strtol(s, &s, 0);
		r->max.x = i;
		r->max.y = j;
		if(*s == 0)
			return 0;
		if(*s != '@')
			goto oops;

		s++;
		if(!isdigit((uchar)*s))
			goto oops;
		i = strtol(s, &s, 0);
		if(*s != ',' && *s != ' ')
			goto oops;
		s++;
		if(!isdigit((uchar)*s))
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
	if(!isdigit((uchar)*s))
		goto oops;
	j = strtol(s, &s, 0);
	if(*s != c)
		goto oops;
	s++;
	if(!isdigit((uchar)*s))
		goto oops;
	k = strtol(s, &s, 0);
	if(*s != c)
		goto oops;
	s++;
	if(!isdigit((uchar)*s))
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

int
isdigit(uchar c){
	return '0'<=c && c<='9';
}
