/* Temporary */
#define fatal(...) ixp_eprint("ixpsrv: fatal: " __VA_ARGS__)
#define debug(...) if(debuglevel) fprintf(stderr, "ixpsrv: " __VA_ARGS__)

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
};

/* Global Vars */
extern Ixp9Srv p9srv;
extern Fileinfo files[];
extern int debuglevel;
