#define fatal(...) sysfatal("devwsys: fatal: " __VA_ARGS__)
#define debug(...) if(debuglevel) fprint(2, "devwsys: " __VA_ARGS__)

/* Devdraw */
Client* drawnewclient(void);

/* Drawfcall messages */
void replymsg(Window*, Wsysmsg*);
void runmsg(Window*, Wsysmsg*);

/* Keyboard */
void addkbd(Window*, Rune);
void matchkbd(Window*);
int kbdputc(Kbdbuf*, int);
long latin1(uchar*, int);

/* Mouse */
void addmouse(Window*, Mouse, int);
void matchmouse(Window*);

/* Window */
void deletewin(Window*);
Window* newwin(void);

/* Ixp */
void ixpreply(Window*, Wsysmsg*);
int ixpserve(char*);

/* Xlib */
Memimage* xallocmemimage(void*);
void xclose(void);
void* xcreatewin(char*, char*, Rectangle);
void xdeletewin(Window*);
int xfd(void);
int xinit(void);
Rectangle xmapwin(void*, int, Rectangle);
void xnextevent(void);
int xsetlabel(Window*);
Rectangle xwinrectangle(char*, char*, int*);
