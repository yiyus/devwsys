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

typedef struct Window Window;
struct Window
{
	int id;
	int deleted;
	char *label;
	Memimage *img;
	Rectangle r;
	void *x;
};

/* Global Vars */
extern Fileinfo file[];
int debuglevel;
int nwindow;
Window **window;

/* Global Funcs */
Window* newwin(void);
void deletewin(int);

/* Xlib functions */
Memimage* xallocmemimage(void*);
void xclose(void);
void* xcreatewin(char*, char*, Rectangle);
int xinit(void);
Rectangle xmapwin(void*, int, Rectangle);
int xnextevent(void);
Rectangle xwinrectangle(char*, char*, int*);
