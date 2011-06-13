/* Temporary */
#define fatal(...) ixp_eprint("ixpsrv: fatal: " __VA_ARGS__)
#define debug(...) if(debuglevel) fprint(2, "ixpsrv: " __VA_ARGS__)

/* Datatypes: */
typedef enum
{
	QNONE=-1,
	QROOT=0,
	QCONS,
	QCONSCTL,
	QLABEL,
	QMOUSE,
	QWINID,
	QMAX
} qpath;

typedef struct Fileinfo Fileinfo;
typedef struct Tagbuf Tagbuf;
typedef struct Window Window;

struct Fileinfo
{
	char *name;
	qpath parent;
	int type;
	int mode;
	unsigned int size;
};

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
extern Fileinfo file[];
int debuglevel;
int nwindow;
Window **window;

/* Global Funcs */
void deletewin(int);
Window* newwin(void);

/* Xlib functions */
Memimage* xallocmemimage(void*);
void xclose(void);
void* xcreatewin(char*, char*, Rectangle);
int xinit(void);
Rectangle xmapwin(void*, int, Rectangle);
int xnextevent(void);
Rectangle xwinrectangle(char*, char*, int*);
