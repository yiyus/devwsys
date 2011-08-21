
#define Qroot		0
#define MSGMAX	((((8192+128)*2)+3) & ~3)

extern char Enomem[];	/* out of memory */
extern char Eperm[];		/* permission denied */
extern char Enodev[];	/* no free devices */
extern char Ehungup[];	/* i/o on hungup channel */
extern char Eexist[];		/* file exists */
extern char Enonexist[];	/* file does not exist */
extern char Ebadcmd[];	/* bad command */
extern char Ebadarg[];	/* bad arguments */

typedef uvlong	Path;
typedef struct Ninepserver Ninepserver;
typedef struct Ninepops Ninepops;
typedef struct Ninepfile Ninepfile;
typedef struct Client Client;
typedef struct Pending Pending;
typedef struct Fid Fid;

struct Ninepserver
{
	Ninepops *ops;
	Path qidgen;
	int connfd;
	int needfile;
	Client *clients;
	Client *curc;
	Fcall fcall;
	Ninepfile *root;
	Ninepfile **ftab;
	void	*priv;	/* private */
};

struct Client
{
	Ninepserver *server;
	Client *next;
	int		fd;
	char	msg[MSGMAX];
	uint		nread;		/* valid bytes in msg (including nc)*/
	int		nc;			/* bytes consumed from front of msg by convM2S */
	char	data[MSGMAX];	/* Tread/Rread data */
	int		state;
	Fid		*fids;
	Pending **pending;
	char		*uname;	/* uid */
	char		*aname;	/* attach name */
	void		*u;
};

struct Pending
{
	Client *c;
	Fcall fcall;
	int flushed;
	Pending *next;
};

struct Ninepops
{
	char *(*newclient)(Client *c);
	char *(*freeclient)(Client *c);

	char *(*attach)(Qid *qid, char *uname, char *aname);
	char *(*walk)(Qid *qid, char *name);
	char *(*open)(Qid *qid, int mode);
	char *(*create)(Qid *qid, char *name, int perm, int mode);
	char *(*read)(Qid qid, char *buf, ulong *n, vlong offset);
	char *(*write)(Qid qid, char *buf, ulong *n, vlong offset);
	char *(*close)(Qid qid, int mode);
	char *(*remove)(Qid qid);
	char *(*stat)(Qid qid, Dir *d);
	char *(*wstat)(Qid qid, Dir *d);
};

struct Ninepfile
{
	Dir	d;
	Ninepfile *parent;
	Ninepfile *child;
	Ninepfile *sibling;
	Ninepfile *next;
	Ninepfile *bind;
	Ninepfile *nf;
	int ref;
	int open;
	void	*u;
};

char* ninepsetowner(char* user);
char *ninepnamespace(void);
char *ninepinit(Ninepserver *server, Ninepops *ops, char *address, int perm, int needfile);
char *ninepend(Ninepserver *server);

char *ninepwait(Ninepserver *server);

void nineplisten(Ninepserver *server, int fd);
int ninepready(Ninepserver *server, int fd);

void ninepdefault(Ninepserver *server);
void nineperror(Ninepserver *server, char *err);
void ninepreply(Ninepserver *server);

Pending *ninepreplylater(Ninepserver *server);
void ninepcompleted(Pending *pend);

Ninepfile *ninepaddfile(Ninepserver *server, Path pqid, Path qid, char *name, int mode, char *owner);
Ninepfile *ninepadddir(Ninepserver *server, Path pqid, Path qid, char *name, int mode, char *owner);
Ninepfile *ninepbind(Ninepserver *server, Path pqid, Path qid);
int nineprmfile(Ninepserver *server, Path qid);
Ninepfile *ninepfindfile(Ninepserver *server, Path qid);

int	ninepperm(Ninepfile *file, char *uid, int mode);
long ninepreadstr(ulong off, char *buf, ulong n, char *str);
Qid ninepqid(int path, int isdir);
void *ninepmalloc(int n);
void ninepfree(void *p);
void ninepdebug(void);
