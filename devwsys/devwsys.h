/* Temporary */
#define fatal(...) sysfatal("devwsys: fatal: " __VA_ARGS__)
#define debug(...) if(debuglevel) fprint(2, "devwsys: " __VA_ARGS__)

/* Datatypes: */
typedef struct Tagbuf Tagbuf;
typedef struct Window Window;

struct Tagbuf
{
	int t[32];
	void *r[32];
	int ri;
	int wi;
};

struct Window
{
	int id;
	int deleted;
	int mouseopen;
	char *label;
	Kbdbuf kbd;
	Memimage *img;
	Mousebuf mouse;
	Tagbuf kbdtags;
	Tagbuf mousetags;
	Rectangle r;
	void *x;
};

/* Global Vars */
int debuglevel;
int nwindow;
Window **window;

/* Global Funcs */
void deletewin(int);
Window* newwin(void);

/* Ixp Funcs */
void 9preply(Window*, Wsysmsg*);
int 9pserve(char*);

/* Xlib Funcs */
Memimage* xallocmemimage(void*);
void xclose(void);
void* xcreatewin(char*, char*, Rectangle);
int xfd(void);
int xinit(void);
Rectangle xmapwin(void*, int, Rectangle);
int xnextevent(void);
Rectangle xwinrectangle(char*, char*, int*);
