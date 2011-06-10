/* Temporary */
#define fatal(...) ixp_eprint("ixpsrv: fatal: " __VA_ARGS__)
#define debug(...) if(debuglevel) fprint(2, "ixpsrv: " __VA_ARGS__)

/* Datatypes: */
typedef enum {QNONE=-1, QROOT=0, QIDENT, QLABEL, QMAX} qpath;

typedef struct Fileinfo Fileinfo;
struct Fileinfo
{
	char *name;
	qpath parent;
	int type;
	int mode;
	unsigned int size;
};

typedef struct Win Win;
struct Win
{
	int id;
	char *label;
	Memimage *img;
	Rectangle r;
	void *x;
};

/* Global Vars */
extern Fileinfo files[];
extern int debuglevel;

/* Global Funcs */
extern Win* newwin(void);

/* Xlib functions */
extern Memimage* xallocmemimage(void*);
extern void* xcreatewin(char*, char*, Rectangle);
extern int xinit(void);
extern Rectangle xmapwin(void*, int, Rectangle);
extern Rectangle xwinrectangle(char*, char*, int*);
