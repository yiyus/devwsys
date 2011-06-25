typedef struct Xconn Xconn;
typedef struct Xmem Xmem;
typedef struct Xwin Xwin;

struct Xconn {
	ulong		chan;
	XColormap	cmap;
	XDisplay		*display;
	int			fd;		/* of display */
	int			depth;	/* of screen */
	XColor		map[256];
	XColor		map7[128];
	uchar		map7to8[128][2];
	int			toplan9[256];
	int			tox11[256];
	int			usetable;
	Rectangle		screenrect;
	XScreen		*screen;
	XVisual		*vis;
	XWindow		root;
};

struct Xmem
{
	int		pixmap;	/* pixmap id */
	XImage	*xi;		/* local image */
	int		dirty;	/* is the X server ahead of us?  */
	Rectangle	dirtyr;	/* which pixels? */
	Rectangle	r;		/* size of image */
};

struct Xwin {
	Atom 		wmdelmsg;
	XDrawable	drawable;
	XDrawable	screenpm;
	XDrawable	nextscreenpm;
};

extern Xconn xconn;

int xtoplan9kbd(XEvent*);
int xtoplan9mouse(XEvent*, Mouse*);
