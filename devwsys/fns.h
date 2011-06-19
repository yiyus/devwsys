#define fatal(...) sysfatal("devwsys: fatal: " __VA_ARGS__)
#define debug(...) if(debuglevel) fprint(2, "devwsys: " __VA_ARGS__)

/* Drawfcall messages */
void replymsg(Window*, Wsysmsg*);
void runmsg(Window*, Wsysmsg*);

/* Keyboard */
void addkbd(int, Rune);
void matchkbd(int);
int kbdputc(int);
long latin1(uchar*, int);

/* Mouse */
void addmouse(int, Mouse, int);
void matchmouse(int);

/* Window */
void deletewin(int);
int lookupwin(Window*);
Window* newwin(void);

/* Ixp */
void ixpreply(Window*, Wsysmsg*);
int ixpserve(char*);

/* Xlib */
Memimage* xallocmemimage(void*);
void xclose(void);
void* xcreatewin(char*, char*, Rectangle);
int xfd(void);
int xinit(void);
Rectangle xmapwin(void*, int, Rectangle);
int xnextevent(void);
int xsetlabel(Window*);
Rectangle xwinrectangle(char*, char*, int*);
