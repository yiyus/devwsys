#define fatal(...) sysfatal("devwsys: fatal: " __VA_ARGS__)
#define debug(...) if(debuglevel) fprint(2, "devwsys: " __VA_ARGS__)

/* Devdraw */
Client* drawnewclient(Window*);
Memimage* drawinstall(Client*, int, Memimage*, DScreen*);
int initscreenimage(Memimage*);

/* Keyboard */
void addkbd(Window*, int);
void matchkbd(Window*);
void readkbd(Window*, void*);

/* Mouse */
void addmouse(Window*, Mouse, int);
void matchmouse(Window*);
void readmouse(Window*, void*);

/* Window */
void deletewin(Window*);
Window* newwin(char*);
int parsewinsize(char*, Rectangle*, int*);
void setlabel(Window*, char*);

/* Ixp */
void ixpread(void*, char*);
int ixpserve(char*);

/* Xlib */
void xattach(Window*, char*);
void xclose(void);
void xdeletewin(Window*);
int xfd(void);
int xinit(void);
void xnextevent(void);
int xsetlabel(Window*);
